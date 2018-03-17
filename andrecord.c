#include "bufqueue.h"
#include "jhelpers.h"
#include "sles.h"
#include "utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <pthread.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/window.h>

#define BUFFER_COUNT 4
#define KICKSTART_COUNT 3

struct Instance {
  int sample_rate;
  int buffer_size;
  jobject multicast_lock;
  ANativeActivity* activity;
  atomic_flag running;
  pthread_t thread;
  struct BufferQueue* queue_impl;
};

static int ScanClients(int fd, struct sockaddr* addr) {
  socklen_t addr_len = sizeof(*addr);
  int result = recvfrom(fd, NULL, 0, MSG_DONTWAIT, addr, &addr_len);
  if (!result) {
    struct in_addr in = ((struct sockaddr_in*)addr)->sin_addr;
    uint16_t port = ((struct sockaddr_in*)addr)->sin_port;
    LOG(INFO, "Client discovered at %s:%u", inet_ntoa(in), ntohs(port));
  }
  return !result || errno == EAGAIN;
}

static void ThreadLoop(struct Instance* instance, SLRecordItf recorder,
                       SLAndroidSimpleBufferQueueItf queue, int fd) {
  struct BufferQueue queue_impl[3];
  void* queue_storage[LENGTH(queue_impl)][BUFFER_COUNT];
  for (unsigned i = 0; i < LENGTH(queue_impl); ++i) {
    InitBufferQueue(&queue_impl[i], LENGTH(queue_storage[i]), queue_storage[i]);
  }
  instance->queue_impl = queue_impl;
  uint8_t buffers[BUFFER_COUNT][instance->buffer_size];
  for (int i = 0; i < BUFFER_COUNT; ++i) {
    if (i < KICKSTART_COUNT) {
      SLresult result =
          (*queue)->Enqueue(queue, buffers[i], instance->buffer_size);
      if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to enqueue kickstart buffer (%s)",
            SlResultString(result));
        goto shortcut;
      }
    }
    struct BufferQueue* target = i < KICKSTART_COUNT ? &instance->queue_impl[1]
                                                     : &instance->queue_impl[0];
    BufferQueuePush(target, buffers[i]);
  }
  SLresult result =
      (*recorder)->SetRecordState(recorder, SL_RECORDSTATE_RECORDING);
  if (result != SL_RESULT_SUCCESS) {
    LOG(ERROR, "Failed to start recording (%s)", SlResultString(result));
    goto shortcut;
  }
  struct sockaddr addr;
  memset(&addr, 0, sizeof(addr));
  while (atomic_flag_test_and_set(&instance->running)) {
    void* buffer = BufferQueuePop(&instance->queue_impl[2], 1);
    if (!ScanClients(fd, &addr)) {
      LOG(ERROR, "Failed to scan clients (%s)", strerror(errno));
      break;
    }
    if (addr.sa_family) {
      ssize_t sent =
          sendto(fd, buffer, instance->buffer_size, 0, &addr, sizeof(addr));
      if (sent != instance->buffer_size) {
        LOG(ERROR, "Failed to send data (%s)", strerror(errno));
        break;
      }
    }
    BufferQueuePush(&instance->queue_impl[0], buffer);
  }
shortcut:
  result = (*recorder)->SetRecordState(recorder, SL_RECORDSTATE_STOPPED);
  if (result != SL_RESULT_SUCCESS) {
    LOG(ERROR, "Failed to stop recording (%s)", SlResultString(result));
  }
  result = (*queue)->Clear(queue);
  if (result != SL_RESULT_SUCCESS) {
    LOG(ERROR, "Failed to clear queue (%s)", SlResultString(result));
  }
}

static void QueueCallback(SLAndroidSimpleBufferQueueItf queue, void* data) {
#ifdef ENABLE_CALLBACK_LOGGING
  LOG(DEBUG, "Entering %s(%p)", __func__, data);
#endif  // ENABLE_CALLBACK_LOGGING
  struct Instance* instance = (struct Instance*)data;
  void* output = BufferQueuePop(&instance->queue_impl[1], 1);
  BufferQueuePush(&instance->queue_impl[2], output);
  void* input = BufferQueuePop(&instance->queue_impl[0], 1);
  for (; input; input = BufferQueuePop(&instance->queue_impl[0], 0)) {
    SLresult result = (*queue)->Enqueue(queue, input, instance->buffer_size);
    if (result != SL_RESULT_SUCCESS) {
      LOG(ERROR, "Failed to enqueue buffer (%s)", SlResultString(result));
      BufferQueuePush(&instance->queue_impl[0], input);
      return;
    }
    BufferQueuePush(&instance->queue_impl[1], input);
  }
  // TODO(mburakov): In sample code there's a logic to put device to sleep if
  // shadow buffer is empty. This makes no sense here as we always enqueue.
}

static void* ThreadProc(void* arg) {
  LOG(DEBUG, "Entering %s(%p)", __func__, arg);
  struct Instance* instance = (struct Instance*)arg;
  SLObjectItf engine_obj = NULL, recorder_obj = NULL;
  int fd = -1;
  do {
    SLEngineItf engine_iface = CreateAudioEngine(&engine_obj);
    if (!engine_iface) {
      LOG(ERROR, "Failed to create audio engine");
      break;
    }
    SLRecordItf recorder_iface = CreateAudioRecorder(
        engine_iface, instance->sample_rate, BUFFER_COUNT, &recorder_obj);
    if (!recorder_iface) {
      LOG(ERROR, "Failed to create audio recorder");
      break;
    }
    SLAndroidSimpleBufferQueueItf queue_iface =
        CreateAudioQueue(recorder_obj, QueueCallback, arg);
    if (!queue_iface) {
      LOG(ERROR, "Failed to create audio queue");
      break;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
      LOG(ERROR, "Failed to create socket (%s)", strerror(errno));
      break;
    }
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(12345)};
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      LOG(ERROR, "Failed to bind socket (%s)", strerror(errno));
      break;
    }
    ThreadLoop(arg, recorder_iface, queue_iface, fd);
  } while (0);
  if (recorder_obj) {
    (*recorder_obj)->Destroy(recorder_obj);
  }
  if (engine_obj) {
    (*engine_obj)->Destroy(engine_obj);
  }
  if (fd != -1) {
    close(fd);
  }
  LOG(DEBUG, "Leaving %s(%p)", __func__, arg);
  return NULL;
}

static void OnActivityResume(ANativeActivity* activity) {
  LOG(DEBUG, "Entering %s(%p)", __func__, (void*)activity);
  ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
  struct Instance* instance = (struct Instance*)malloc(sizeof(struct Instance));
  do {
    if (!instance) {
      LOG(ERROR, "Failed to allocate instance data (%s)", strerror(errno));
      break;
    }
    int frames_per_buffer;
    if (!GetBufferConfig(activity->env, activity->clazz, &instance->sample_rate,
                         &frames_per_buffer)) {
      LOG(ERROR, "Failed to get buffer configuration");
      break;
    }
    LOG(INFO, "Audio configuration sample_rate=%d, frames_per_buffer=%d",
        instance->sample_rate, frames_per_buffer);
    int sample_size = SL_PCMSAMPLEFORMAT_FIXED_16 >> 3;
    instance->buffer_size = frames_per_buffer * sample_size;
    instance->multicast_lock = NULL;
    if (!AcquireMulticastLock(activity->env, activity->clazz, "andrecord",
                              &instance->multicast_lock)) {
      LOG(ERROR, "Failed to acquire multicast lock");
      break;
    }
    instance->activity = activity;
    atomic_flag_test_and_set(&instance->running);
    if (pthread_create(&instance->thread, NULL, ThreadProc, instance)) {
      LOG(ERROR, "Failed to create thread (%s)", strerror(errno));
      break;
    }
    activity->instance = instance;
    return;
  } while (0);
  if (instance) {
    if (instance->multicast_lock) {
      ReleaseMulticastLock(activity->env, instance->multicast_lock);
    }
    free(instance);
  }
}

static void OnActivityPause(ANativeActivity* activity) {
  LOG(DEBUG, "Entering %s(%p)", __func__, (void*)activity);
  if (!activity->instance) {
    return;
  }
  struct Instance* instance = activity->instance;
  activity->instance = NULL;
  atomic_flag_clear(&instance->running);
  if (pthread_join(instance->thread, NULL)) {
    LOG(ERROR, "Failed to join thread (%s)", strerror(errno));
  }
  ReleaseMulticastLock(activity->env, instance->multicast_lock);
  free(instance);
}

__attribute__((visibility("default"))) void ANativeActivity_onCreate(
    ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  (void)savedState;
  (void)savedStateSize;
  LOG(DEBUG, "Entering %s(%p)", __func__, (void*)activity);
  activity->callbacks->onResume = OnActivityResume;
  activity->callbacks->onPause = OnActivityPause;
}

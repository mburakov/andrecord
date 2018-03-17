#include "jhelpers.h"
#include "utils.h"

#include <stdlib.h>

#include <android/log.h>

#define UNEXCEPT(op, env, ...)                   \
  (*env)->op(env, __VA_ARGS__);                  \
  if ((*env)->ExceptionCheck(env) == JNI_TRUE) { \
    (*env)->ExceptionDescribe(env);              \
    (*env)->ExceptionClear(env);                 \
  }

#define FIND_OR_BREAK(out, what, env, clazz, name, sig) \
  UNEXCEPT(Get##what##ID, env, clazz, name, sig);       \
  if (!out) {                                           \
    LOG(ERROR, "Failed to find " #what " %s", name);    \
    break;                                              \
  }

static jobject GetSystemService(JNIEnv* env, jobject activity,
                                jclass activity_class, const char* name) {
  jobject result = NULL;
  jobject service_name = NULL;
  do {
    jfieldID service_name_field =
        FIND_OR_BREAK(service_name_field, StaticField, env, activity_class,
                      name, "Ljava/lang/String;");
    service_name =
        UNEXCEPT(GetStaticObjectField, env, activity_class, service_name_field);
    if (!service_name) {
      LOG(ERROR, "Failed to get name of system service %s", name);
      break;
    }
    jmethodID get_system_service = FIND_OR_BREAK(
        get_system_service, Method, env, activity_class, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    result = UNEXCEPT(CallObjectMethod, env, activity, get_system_service,
                      service_name);
    if (!result) {
      LOG(ERROR, "Failed to get system service %s", name);
      break;
    }
  } while (0);
  if (service_name) {
    UNEXCEPT(DeleteLocalRef, env, service_name);
  }
  return result;
}

int AcquireMulticastLock(JNIEnv* env, jobject activity, jobject* lock) {
  LOG(DEBUG, "Entering %s()", __func__);
  return 1;
}

int ReleaseMulticastLock(JNIEnv* env, jobject lock) {
  LOG(DEBUG, "Entering %s()", __func__);
  return 1;
}

int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out) {
  LOG(DEBUG, "Entering %s()", __func__);
  void* values[][2] = {
      {"PROPERTY_OUTPUT_SAMPLE_RATE", sample_rate_out},
      {"PROPERTY_OUTPUT_FRAMES_PER_BUFFER", frames_per_buffer_out}};
  int result = 0;
  jclass activity_class = NULL;
  jobject audio_manager = NULL;
  jclass manager_class = NULL;
  do {
    activity_class = UNEXCEPT(GetObjectClass, env, activity);
    if (!activity_class) {
      LOG(ERROR, "Failed to get activity class");
      break;
    }
    audio_manager =
        GetSystemService(env, activity, activity_class, "AUDIO_SERVICE");
    if (!audio_manager) {
      LOG(ERROR, "Failed to get audio manager service");
      break;
    }
    manager_class = UNEXCEPT(GetObjectClass, env, audio_manager);
    if (!manager_class) {
      LOG(ERROR, "Failed to get audio manager class");
      break;
    }
    jmethodID get_property =
        FIND_OR_BREAK(get_property, Method, env, manager_class, "getProperty",
                      "(Ljava/lang/String;)Ljava/lang/String;");
    FOR_EACH(void*(*it)[], values) {
      jfieldID property_name_field =
          FIND_OR_BREAK(property_name_field, StaticField, env, manager_class,
                        (*it)[0], "Ljava/lang/String;");
    }
  } while (result = 1, 0);
  if (manager_class) {
    UNEXCEPT(DeleteLocalRef, env, manager_class);
  }
  if (audio_manager) {
    UNEXCEPT(DeleteLocalRef, env, audio_manager);
  }
  if (activity_class) {
    UNEXCEPT(DeleteLocalRef, env, activity_class);
  }
  return result;
}

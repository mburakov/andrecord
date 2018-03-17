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
                                const char* name) {
  jobject result = 0;
  jclass activity_class = NULL;
  do {
    activity_class = UNEXCEPT(GetObjectClass, env, activity);
    if (!activity_class) {
      LOG(ERROR, "Failed to get activity class");
      break;
    }
    jfieldID service_name_field =
        FIND_OR_BREAK(service_name_field, StaticField, env, activity_class,
                      name, "Ljava/lang/String;");
    jmethodID get_system_service = FIND_OR_BREAK(
        get_system_service, Method, env, activity_class, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject service_name =
        UNEXCEPT(GetStaticObjectField, env, activity_class, service_name_field);
    if (!service_name) {
      LOG(ERROR, "Failed to get name of system service %s", name);
      break;
    }
    result = UNEXCEPT(CallObjectMethod, env, activity, get_system_service,
                      service_name);
    UNEXCEPT(DeleteLocalRef, env, service_name);
    if (!result) {
      LOG(ERROR, "Failed to get system service %s", name);
      break;
    }
  } while (0);
  if (activity_class) {
    UNEXCEPT(DeleteLocalRef, env, activity_class);
  }
  return result;
}

static jobject CreateMulticastLock(JNIEnv* env, jobject activity,
                                   const char* tag_value) {
  jobject result = NULL;
  jobject wifi_manager = NULL;
  jclass manager_class = NULL;
  do {
    wifi_manager = GetSystemService(env, activity, "WIFI_SERVICE");
    if (!wifi_manager) {
      LOG(ERROR, "Failed to get wifi manager service");
      break;
    }
    manager_class = UNEXCEPT(GetObjectClass, env, wifi_manager);
    if (!manager_class) {
      LOG(ERROR, "Failed to get wifi manager class");
      break;
    }
    jmethodID create_multicast_lock = FIND_OR_BREAK(
        create_multicast_lock, Method, env, manager_class,
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;");
    jobject tag = UNEXCEPT(NewStringUTF, env, tag_value);
    if (!tag) {
      LOG(ERROR, "Failed to create tag string");
      break;
    }
    result = UNEXCEPT(CallObjectMethod, env, wifi_manager,
                      create_multicast_lock, tag);
    UNEXCEPT(DeleteLocalRef, env, tag);
  } while (0);
  if (manager_class) {
    UNEXCEPT(DeleteLocalRef, env, manager_class);
  }
  if (wifi_manager) {
    UNEXCEPT(DeleteLocalRef, env, wifi_manager);
  }
  return result;
}

static int StringToInt(JNIEnv* env, jobject str) {
  const char* value = UNEXCEPT(GetStringUTFChars, env, str, NULL);
  if (!value) {
    LOG(ERROR, "Failed to get string value");
    return 0;
  }
  int result = atoi(value);
  UNEXCEPT(ReleaseStringUTFChars, env, str, value);
  return result;
}

int AcquireMulticastLock(JNIEnv* env, jobject activity, const char* tag,
                         jobject* multicast_lock_out) {
  LOG(DEBUG, "Entering %s()", __func__);
  int result = 0;
  jobject multicast_lock = NULL;
  jobject lock_class = NULL;
  do {
    multicast_lock = CreateMulticastLock(env, activity, tag);
    if (!multicast_lock) {
      LOG(ERROR, "Failed to create multicast lock");
      break;
    }
    lock_class = UNEXCEPT(GetObjectClass, env, multicast_lock);
    if (!lock_class) {
      LOG(ERROR, "Failed to get class of multicast lock");
      break;
    }
    jmethodID acquire =
        FIND_OR_BREAK(acquire, Method, env, lock_class, "acquire", "()V");
    jmethodID is_held =
        FIND_OR_BREAK(is_held, Method, env, lock_class, "isHeld", "()Z");
    UNEXCEPT(CallVoidMethod, env, multicast_lock, acquire);
    jboolean held = UNEXCEPT(CallBooleanMethod, env, multicast_lock, is_held);
    if (held != JNI_TRUE) {
      LOG(ERROR, "Failed to acquire multicast lock");
      break;
    }
    jobject multicast_lock_global = UNEXCEPT(NewGlobalRef, env, multicast_lock);
    if (!multicast_lock_global) {
      LOG(ERROR, "Failed to create global multicast lock reference");
      break;
    }
    *multicast_lock_out = multicast_lock_global;
  } while (result = 1, 0);
  if (lock_class) {
    UNEXCEPT(DeleteLocalRef, env, lock_class);
  }
  if (multicast_lock) {
    UNEXCEPT(DeleteLocalRef, env, multicast_lock);
  }
  return result;
}

int ReleaseMulticastLock(JNIEnv* env, jobject multicast_lock) {
  LOG(DEBUG, "Entering %s()", __func__);
  int result = 0;
  jclass lock_class = NULL;
  do {
    lock_class = UNEXCEPT(GetObjectClass, env, multicast_lock);
    if (!lock_class) {
      LOG(ERROR, "Failed to get class of multicast lock");
      break;
    }
    jmethodID release =
        FIND_OR_BREAK(release, Method, env, lock_class, "release", "()V");
    jmethodID is_held =
        FIND_OR_BREAK(is_held, Method, env, lock_class, "isHeld", "()Z");
    UNEXCEPT(CallVoidMethod, env, multicast_lock, release);
    jboolean held = UNEXCEPT(CallBooleanMethod, env, multicast_lock, is_held);
    if (held) {
      LOG(ERROR, "Failed to release multicast lock");
      break;
    }
    UNEXCEPT(DeleteGlobalRef, env, multicast_lock);
  } while (result = 1, 0);
  if (lock_class) {
    UNEXCEPT(DeleteLocalRef, env, lock_class);
  }
  return result;
}

int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out) {
  LOG(DEBUG, "Entering %s()", __func__);
  struct Pair {
    const char* const key;
    int value;
  } values_map[] = {{.key = "PROPERTY_OUTPUT_SAMPLE_RATE"},
                    {.key = "PROPERTY_OUTPUT_FRAMES_PER_BUFFER"}};
  int result = 0;
  jobject audio_manager = NULL;
  jclass manager_class = NULL;
  do {
    audio_manager = GetSystemService(env, activity, "AUDIO_SERVICE");
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
    FOR_EACH(struct Pair * it, values_map) {
      jfieldID property_name_field =
          FIND_OR_BREAK(property_name_field, StaticField, env, manager_class,
                        it->key, "Ljava/lang/String;");
      jobject property_name = UNEXCEPT(GetStaticObjectField, env, manager_class,
                                       property_name_field);
      if (!property_name) {
        LOG(ERROR, "Failed to get name of audio property %s", it->key);
        break;
      }
      jobject value = UNEXCEPT(CallObjectMethod, env, audio_manager,
                               get_property, property_name);
      UNEXCEPT(DeleteLocalRef, env, property_name);
      if (!value) {
        LOG(ERROR, "Failed to get audio property %s", it->key);
        break;
      }
      it->value = StringToInt(env, value);
      UNEXCEPT(DeleteLocalRef, env, value);
      if (!it->value) {
        LOG(ERROR, "Invalid value of audio property %s", it->key);
        break;
      }
    }
    if (values_map[0].value && values_map[1].value) {
      *sample_rate_out = values_map[0].value;
      *frames_per_buffer_out = values_map[1].value;
    }
  } while (result = 1, 0);
  if (manager_class) {
    UNEXCEPT(DeleteLocalRef, env, manager_class);
  }
  if (audio_manager) {
    UNEXCEPT(DeleteLocalRef, env, audio_manager);
  }
  return result;
}

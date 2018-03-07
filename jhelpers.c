#include "jhelpers.h"
#include "utils.h"

#include <stdlib.h>

#include <arpa/inet.h>

#include <android/log.h>

#define UNEXCEPT(op, env, ...)                     \
    (*env)->op(env, __VA_ARGS__);                  \
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) { \
        (*env)->ExceptionDescribe(env);            \
        (*env)->ExceptionClear(env);               \
    }

static jobject CallJavaStaticMethod(JNIEnv* env, const char* class_name,
                                    const char* name, const char* signature,
                                    ...) {
    jclass clazz = UNEXCEPT(FindClass, env, class_name);
    if (!clazz) {
        LOG(ERROR, "Failed to find class %s in JVM", class_name);
        return NULL;
    }
    jmethodID method = UNEXCEPT(GetStaticMethodID, env, clazz, name, signature);
    if (!method) {
        LOG(ERROR, "Failed to find method %s.%s%s in JVM", class_name, name,
            signature);
        UNEXCEPT(DeleteLocalRef, env, clazz);
        return NULL;
    }
    va_list args;
    va_start(args, signature);
    jobject result =
            UNEXCEPT(CallStaticObjectMethodV, env, clazz, method, args);
    va_end(args);
    UNEXCEPT(DeleteLocalRef, env, clazz);
    return result;
}

static jobject CallJavaMethod(JNIEnv* env, jobject object, const char* name,
                              const char* signature, ...) {
    // TODO(mburakov): Does this really never return NULL?
    jclass clazz = UNEXCEPT(GetObjectClass, env, object);
    jmethodID method = UNEXCEPT(GetMethodID, env, clazz, name, signature);
    UNEXCEPT(DeleteLocalRef, env, clazz);
    if (!method) {
        LOG(ERROR, "Failed to find method %s%s in JVM", name, signature);
        return NULL;
    }
    va_list args;
    va_start(args, signature);
    jobject result = UNEXCEPT(CallObjectMethodV, env, object, method, args);
    va_end(args);
    return result;
}

static jint CallJavaMethodInt(JNIEnv* env, jobject object, const char* name,
                              const char* signature, ...) {
    // TODO(mburakov): Does this really never return NULL?
    jclass clazz = UNEXCEPT(GetObjectClass, env, object);
    jmethodID method = UNEXCEPT(GetMethodID, env, clazz, name, signature);
    UNEXCEPT(DeleteLocalRef, env, clazz);
    if (!method) {
        LOG(ERROR, "Failed to find method %s%s in JVM", name, signature);
        return 0;
    }
    va_list args;
    va_start(args, signature);
    jint result = UNEXCEPT(CallIntMethodV, env, object, method, args);
    va_end(args);
    return result;
}

static jobject GetJavaStaticField(JNIEnv* env, const char* class_name,
                                  const char* name, const char* signature) {
    jclass clazz = UNEXCEPT(FindClass, env, class_name);
    if (!clazz) {
        LOG(ERROR, "Failed to find class %s in JVM", class_name);
        return NULL;
    }
    jfieldID field = UNEXCEPT(GetStaticFieldID, env, clazz, name, signature);
    if (!field) {
        LOG(ERROR, "Failed to find static field %s.%s%s in JVM", class_name,
            name, signature);
        UNEXCEPT(DeleteLocalRef, env, clazz);
        return NULL;
    }
    jobject result = UNEXCEPT(GetStaticObjectField, env, clazz, field);
    UNEXCEPT(DeleteLocalRef, env, clazz);
    return result;
}

static uint32_t ConvertJavaString(JNIEnv* env, jobject string,
                                  uint32_t (*conv)(const char*)) {
    const char* value = UNEXCEPT(GetStringUTFChars, env, string, NULL);
    if (!value) {
        LOG(ERROR, "Failed to get string value");
        return 0;
    }
    LOG(DEBUG, "Converting string \"%s\"", value);
    uint32_t result = conv(value);
    UNEXCEPT(ReleaseStringUTFChars, env, string, value);
    return result;
}

static uint32_t atou(const char* string) {
    return strtoul(string, NULL, 10);
}

int GetBroadcastAddr(JNIEnv* env, jobject activity, const char* iface,
                     uint32_t* addr_out) {
    LOG(DEBUG, "Entering %s(%s)", __func__, iface);
    jobject name = UNEXCEPT(NewStringUTF, env, iface);
    if (!name) {
        LOG(ERROR, "Failed to allocate string object");
        return 0;
    }
    jobject interface = CallJavaStaticMethod(
            env, "java/net/NetworkInterface", "getByName",
            "(Ljava/lang/String;)Ljava/net/NetworkInterface;", name);
    UNEXCEPT(DeleteLocalRef, env, name);
    if (!interface) {
        LOG(ERROR, "Failed to get interface");
        return 0;
    }
    jobject address_list = CallJavaMethod(
            env, interface, "getInterfaceAddresses", "()Ljava/util/List;");
    UNEXCEPT(DeleteLocalRef, env, interface);
    if (!address_list) {
        LOG(ERROR, "Failed to get interface addresses");
        return 0;
    }
    jint list_size = CallJavaMethodInt(env, address_list, "size", "()I");
    LOG(DEBUG, "Found %d addresses on interface %s", list_size, iface);
    jobject broadcast = NULL;
    for (int i = 0; i < list_size && !broadcast; ++i) {
        jobject address = CallJavaMethod(env, address_list, "get",
                                         "(I)Ljava/lang/Object;", i);
        if (!address) {
            LOG(ERROR, "Failed to get address %d", i);
            continue;
        }
        broadcast = CallJavaMethod(env, address, "getBroadcast",
                                   "()Ljava/net/InetAddress;");
        UNEXCEPT(DeleteLocalRef, env, address);
        if (!broadcast) {
            LOG(DEBUG, "Broadcast is not set for address %d", i);
            continue;
        }
    }
    UNEXCEPT(DeleteLocalRef, env, address_list);
    if (!broadcast) {
        LOG(ERROR, "No broadcast addresses found");
        return 0;
    }
    jobject address = CallJavaMethod(env, broadcast, "getHostAddress",
                                     "()Ljava/lang/String;");
    UNEXCEPT(DeleteLocalRef, env, broadcast);
    if (!address) {
        LOG(ERROR, "Failed to get broadcast string");
        return 0;
    }
    uint32_t address_int = ConvertJavaString(env, address, inet_addr);
    UNEXCEPT(DeleteLocalRef, env, address);
    if (!address_int) {
        LOG(ERROR, "Failed to convert broadcast string");
        return 0;
    }
    *addr_out = address_int;
    LOG(DEBUG, "Selected broadcast address %d.%d.%d.%d", *addr_out & 0xff,
        *addr_out >> 8 & 0xff, *addr_out >> 16 & 0xff, *addr_out >> 24 & 0xff);
    return 1;
}

int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out) {
    LOG(DEBUG, "Entering %s()", __func__);
    jobject service_name =
            GetJavaStaticField(env, "android/content/Context", "AUDIO_SERVICE",
                               "Ljava/lang/String;");
    if (!service_name) {
        LOG(ERROR, "Failed to get audio service name");
        return 0;
    }
    jobject service = CallJavaMethod(env, activity, "getSystemService",
                                     "(Ljava/lang/String;)Ljava/lang/Object;",
                                     service_name);
    UNEXCEPT(DeleteLocalRef, env, service_name);
    if (!service) {
        LOG(ERROR, "Failed to get audio service");
        return 0;
    }
    jobject sample_rate = NULL;
    jobject frames_per_buffer = NULL;
    do {
        jobject sample_rate_name = GetJavaStaticField(
                env, "android/media/AudioManager",
                "PROPERTY_OUTPUT_SAMPLE_RATE", "Ljava/lang/String;");
        if (!sample_rate_name) {
            LOG(ERROR, "Failed to get sample rate property name");
            break;
        }
        sample_rate = CallJavaMethod(
                env, service, "getProperty",
                "(Ljava/lang/String;)Ljava/lang/String;", sample_rate_name);
        UNEXCEPT(DeleteLocalRef, env, sample_rate_name);
        jobject frames_per_buffer_name = GetJavaStaticField(
                env, "android/media/AudioManager",
                "PROPERTY_OUTPUT_FRAMES_PER_BUFFER", "Ljava/lang/String;");
        if (!frames_per_buffer_name) {
            LOG(ERROR, "Failed to get frames per buffer property name");
            break;
        }
        frames_per_buffer =
                CallJavaMethod(env, service, "getProperty",
                               "(Ljava/lang/String;)Ljava/lang/String;",
                               frames_per_buffer_name);
        UNEXCEPT(DeleteLocalRef, env, frames_per_buffer_name);
    } while (0);
    UNEXCEPT(DeleteLocalRef, env, service);
    uint32_t sample_rate_int = 0;
    uint32_t frames_per_buffer_int = 0;
    if (!sample_rate) {
        LOG(ERROR, "Failed to get sample rate");
    } else {
        sample_rate_int = ConvertJavaString(env, sample_rate, atou);
        UNEXCEPT(DeleteLocalRef, env, sample_rate);
    }
    if (!frames_per_buffer) {
        LOG(ERROR, "Failed to get frames per buffer");
    } else {
        frames_per_buffer_int = ConvertJavaString(env, frames_per_buffer, atou);
        UNEXCEPT(DeleteLocalRef, env, frames_per_buffer);
    }
    if (!sample_rate_int || !frames_per_buffer_int) {
        LOG(ERROR, "Failed to get buffer configuration parameter");
        return 0;
    }
    *sample_rate_out = sample_rate_int;
    *frames_per_buffer_out = frames_per_buffer_int;
     LOG(DEBUG, "Selected configuration sample_rate=%d, frames_per_buffer=%d",
         *sample_rate_out, *frames_per_buffer_out);
    return 1;
}

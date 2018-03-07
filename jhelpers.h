#include <jni.h>

int GetBroadcastAddr(JNIEnv* env, jobject activity, const char* iface,
                     uint32_t* addr_out);
int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out);

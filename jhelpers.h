#include <jni.h>

int AcquireMulticastLock(JNIEnv* env, jobject activity, const char* tag,
                         jobject* multicast_lock_out);
int ReleaseMulticastLock(JNIEnv* env, jobject multicast_lock);
int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out);

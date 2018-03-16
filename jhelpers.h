#include <jni.h>

int AcquireMulticastLock(JNIEnv* env, jobject activity, jobject* lock);
int ReleaseMulticastLock(JNIEnv* env, jobject lock);
int GetBufferConfig(JNIEnv* env, jobject activity, int* sample_rate_out,
                    int* frames_per_buffer_out);

#include <SLES/OpenSLES_Android.h>

const char* SlResultString(SLresult result);
SLEngineItf CreateAudioEngine(SLObjectItf* object);
SLRecordItf CreateAudioRecorder(SLEngineItf engine, SLuint32 sample_rate,
                                SLuint32 queue_length, SLObjectItf* object);
SLAndroidSimpleBufferQueueItf CreateAudioQueue(
    SLObjectItf recorder, slAndroidSimpleBufferQueueCallback callback,
    void* callback_arg);

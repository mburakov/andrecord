#include "sles.h"
#include "utils.h"

#include <string.h>

#include <android/log.h>

const char* SlResultString(SLresult result) {
    BEGIN_MAP(result);
    MAP_STR(SL_RESULT_SUCCESS);
    MAP_STR(SL_RESULT_PRECONDITIONS_VIOLATED);
    MAP_STR(SL_RESULT_PARAMETER_INVALID);
    MAP_STR(SL_RESULT_MEMORY_FAILURE);
    MAP_STR(SL_RESULT_RESOURCE_ERROR);
    MAP_STR(SL_RESULT_RESOURCE_LOST);
    MAP_STR(SL_RESULT_IO_ERROR);
    MAP_STR(SL_RESULT_BUFFER_INSUFFICIENT);
    MAP_STR(SL_RESULT_CONTENT_CORRUPTED);
    MAP_STR(SL_RESULT_CONTENT_UNSUPPORTED);
    MAP_STR(SL_RESULT_CONTENT_NOT_FOUND);
    MAP_STR(SL_RESULT_PERMISSION_DENIED);
    MAP_STR(SL_RESULT_FEATURE_UNSUPPORTED);
    MAP_STR(SL_RESULT_INTERNAL_ERROR);
    MAP_STR(SL_RESULT_UNKNOWN_ERROR);
    MAP_STR(SL_RESULT_OPERATION_ABORTED);
    MAP_STR(SL_RESULT_CONTROL_LOST);
    END_MAP("Unknown OpenSL ES error");
}

SLEngineItf CreateAudioEngine(SLObjectItf* object) {
    SLresult result = slCreateEngine(object, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to create audio engine (%s)",
            SlResultString(result));
        return NULL;
    }
    result = (**object)->Realize(*object, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to realize audio engine (%s)",
            SlResultString(result));
        return NULL;
    }
    SLEngineItf iface;
    result = (**object)->GetInterface(*object, SL_IID_ENGINE, &iface);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to get audio engine interface (%s)",
            SlResultString(result));
        return NULL;
    }
    return iface;
}

SLRecordItf CreateAudioRecorder(SLEngineItf engine, SLuint32 sample_rate,
                                SLuint32 queue_length, SLObjectItf* object) {
    SLAndroidDataFormat_PCM_EX format_pcm;
    memset(&format_pcm, 0, sizeof(format_pcm));
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = 1;
    format_pcm.sampleRate = sample_rate * 1000;
    format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audio_source = {&loc_dev, NULL};
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, queue_length};
    SLDataSink audio_sink = {&loc_bq, &format_pcm};
    const SLInterfaceID id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                SL_IID_ANDROIDCONFIGURATION};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    SLresult result = (*engine)->CreateAudioRecorder(
            engine, object, &audio_source, &audio_sink, LENGTH(id), id, req);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to create audio recorder (%s)",
            SlResultString(result));
        return NULL;
    }
    SLAndroidConfigurationItf config_iface;
    result = (**object)->GetInterface(*object, SL_IID_ANDROIDCONFIGURATION,
                                      &config_iface);
    if (result != SL_RESULT_SUCCESS) {
        LOG(WARN, "Failed to get audio configuraiton interface (%s)",
            SlResultString(result));
    } else {
        SLuint32 preset = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
        (*config_iface)
                ->SetConfiguration(config_iface,
                                   SL_ANDROID_KEY_RECORDING_PRESET, &preset,
                                   sizeof(preset));
    }
    result = (**object)->Realize(*object, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to realize audio recorder (%s)",
            SlResultString(result));
        return NULL;
    }
    SLRecordItf iface;
    result = (**object)->GetInterface(*object, SL_IID_RECORD, &iface);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to get audio recorder interface (%s)",
            SlResultString(result));
        return NULL;
    }
    return iface;
}

SLAndroidSimpleBufferQueueItf CreateAudioQueue(
        SLObjectItf recorder, slAndroidSimpleBufferQueueCallback callback,
        void* callback_arg) {
    SLAndroidSimpleBufferQueueItf iface;
    SLresult result = (*recorder)->GetInterface(
            recorder, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &iface);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to create audio queue (%s)", SlResultString(result));
        return NULL;
    }
    result = (*iface)->RegisterCallback(iface, callback, callback_arg);
    if (result != SL_RESULT_SUCCESS) {
        LOG(ERROR, "Failed to register queue callback (%s)",
            SlResultString(result));
        return NULL;
    }
    return iface;
}

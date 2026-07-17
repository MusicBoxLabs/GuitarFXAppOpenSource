#include <jni.h>
#include "AudioEngine.h"

static AudioEngine engine;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeStart(JNIEnv *, jobject) {
    return engine.start() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeStop(JNIEnv *, jobject) {
    engine.stop();
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetInputGain(JNIEnv *, jobject, jfloat v) {
    engine.inputGain.store(v);
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetVolume(JNIEnv *, jobject, jfloat v) {
    engine.volume.store(v);
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetDrive(JNIEnv *, jobject, jfloat v) {
    engine.drive.store(v);
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetTone(JNIEnv *, jobject, jfloat v) {
    engine.tone.store(v);
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetFx(JNIEnv *, jobject, jfloat v) {
    engine.fx.store(v);
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeSetMode(JNIEnv *, jobject, jint m) {
    engine.mode.store(m);
}
JNIEXPORT jfloat JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeGetPeak(JNIEnv *, jobject) {
    return engine.inputPeak.load();
}
JNIEXPORT jdouble JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeGetLatencyMs(JNIEnv *, jobject) {
    return engine.getLatencyMs();
}
JNIEXPORT jboolean JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeStartRecording(JNIEnv *env, jobject, jstring path) {
    const char *p = env->GetStringUTFChars(path, nullptr);
    bool ok = engine.startRecording(p);
    env->ReleaseStringUTFChars(path, p);
    return ok ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL
Java_com_edbataille_guitarfx_MainActivity_nativeStopRecording(JNIEnv *, jobject) {
    engine.stopRecording();
}

}

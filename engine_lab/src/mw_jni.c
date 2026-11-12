#include "mw_engine.h"
#include <jni.h>
#include <stdint.h>
#define PTR(v) ((MW_Engine *)(intptr_t)(v))
JNIEXPORT jlong JNICALL Java_MatryoshkaEngine_create(JNIEnv *env, jclass cls, jint w, jint h) {
    (void)env; (void)cls; return (jlong)(intptr_t)mw_create(w, h);
}
JNIEXPORT void JNICALL Java_MatryoshkaEngine_scene(JNIEnv *env, jclass cls, jlong h, jint frame) {
    (void)env; (void)cls; mw_scene(PTR(h), (uint32_t)frame);
}
JNIEXPORT jint JNICALL Java_MatryoshkaEngine_checksum(JNIEnv *env, jclass cls, jlong h) {
    (void)env; (void)cls; return (jint)mw_checksum(PTR(h));
}
JNIEXPORT jboolean JNICALL Java_MatryoshkaEngine_save(JNIEnv *env, jclass cls, jlong h, jstring path) {
    (void)cls;
    if (!path) return JNI_FALSE;
    const char *chars = (*env)->GetStringUTFChars(env, path, NULL);
    if (!chars) return JNI_FALSE;
    int result = mw_save_ppm(PTR(h), chars);
    (*env)->ReleaseStringUTFChars(env, path, chars);
    return result ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_MatryoshkaEngine_destroy(JNIEnv *env, jclass cls, jlong h) {
    (void)env; (void)cls; mw_destroy(PTR(h));
}

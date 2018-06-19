/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <jni.h>
#include <android/log.h>
#include <assert.h>


// Android log function wrappers
static const char* kTAG = "hideosksuggestions";


#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))


// processing callback to handler class
typedef struct _context {
    JavaVM  *javaVM;

    jclass   jniHelperClz;
    jobject  jniHelperObj;

    jclass   mainActivityClz;
    jobject  mainActivityObj;
} Context;
Context g_ctx;


/*
 * This will initialize the context members
 */
void initJNI(JNIEnv* env, jobject thiz) {
    jclass clz = env->GetObjectClass(thiz);
    g_ctx.mainActivityClz = static_cast<jclass>(env->NewGlobalRef(clz));
    g_ctx.mainActivityObj = env->NewGlobalRef(thiz);
}

/* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   hideosksuggestions/app/src/main/java/com/company/hideosksuggestions/MainActivity.java
 */
jstring stringFromJNI( JNIEnv* env, jobject thiz )
{
    return env->NewStringUTF( "This is a text-edit with suggestions!");
}

/*
 *  A helper function to show how to call
 *     java static functions JniHelper::getBuildVersion()
 *     java non-static function JniHelper::getRuntimeMemorySize()
 *  The trivial implementation for these functions are inside file
 *     JniHelper.java
 */
void queryRuntimeInfo(JNIEnv *env, jobject thiz)
{
    // Find out which OS we are running on. It does not matter for this app
    // just to demo how to call static functions.
    // Our java JniHelper class id and thiz are initialized when this
    // shared lib got loaded, we just directly use them
    //    static function does not need thiz, so we just need to feed
    //    class and method id to JNI
    jmethodID versionFunc = env->GetStaticMethodID(
            g_ctx.jniHelperClz,
            "getBuildVersion", "()Ljava/lang/String;");
    if (!versionFunc) {
        LOGE("Failed to retrieve getBuildVersion() methodID @ line %d",
             __LINE__);
        return;
    }
    jstring buildVersion = static_cast<jstring>(env->CallStaticObjectMethod(g_ctx.jniHelperClz, versionFunc));
    const char *version = env->GetStringUTFChars( buildVersion, NULL);
    if (!version) {
        LOGE("Unable to get version string @ line %d", __LINE__);
        return;
    }
    LOGI("Android Version - %s", version);
    env->ReleaseStringUTFChars( buildVersion, version);

    // we are called from JNI_OnLoad, so got to release LocalRef to avoid leaking
    env->DeleteLocalRef( buildVersion);

    // Query available memory size from a non-static public function
    // we need use an thiz of JniHelper class to call JNI
    jmethodID memFunc = env->GetMethodID( g_ctx.jniHelperClz,
                                            "getRuntimeMemorySize", "()J");
    if (!memFunc) {
        LOGE("Failed to retrieve getRuntimeMemorySize() methodID @ line %d",
             __LINE__);
        return;
    }
    jlong result = env->CallLongMethod( thiz, memFunc);
    LOGI("Runtime free memory size: %" PRId64, result);
    (void)result;  // silence the compiler warning
}

/*
 * processing one time initialization:
 *     Cache the javaVM into our context
 *     Find class ID for JniHelper
 *     Create an thiz of JniHelper
 *     Make global reference since we are using them from a native thread
 * Note:
 *     All resources allocated here are never released by application
 *     we rely on system to free all global refs when it goes away;
 *     the pairing function JNI_OnUnload() never gets called at all.
 */
jint onLoad(JavaVM **vm)
{
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = *vm;
    if((*vm)->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass  clz = env->FindClass("com/company/hideosksuggestions/JniHandler");
    g_ctx.jniHelperClz = static_cast<jclass>(env->NewGlobalRef(clz));

    jmethodID  jniHelperCtor = env->GetMethodID( g_ctx.jniHelperClz,
                                                   "<init>", "()V");
    jobject    handler = env->NewObject( g_ctx.jniHelperClz,
                                           jniHelperCtor);
    g_ctx.jniHelperObj = env->NewGlobalRef( handler);
    queryRuntimeInfo(env, g_ctx.jniHelperObj);

     g_ctx.mainActivityObj = NULL;
    return  JNI_VERSION_1_6;
}

/*
 * Interface to Java side to start s, caller is from onResume()
 */
void mainActivityStarts(JNIEnv* env, jobject thiz)
{
    jclass clz = env->GetObjectClass( thiz);
    g_ctx.mainActivityClz = static_cast<jclass>(env->NewGlobalRef(clz));
    g_ctx.mainActivityObj = env->NewGlobalRef( thiz);
}

/*
 */
void mainActivityStops(JNIEnv* env) {
    // release object we allocated from Starts() function
    env->DeleteGlobalRef( g_ctx.mainActivityClz);
    env->DeleteGlobalRef( g_ctx.mainActivityObj);
    g_ctx.mainActivityObj = NULL;
    g_ctx.mainActivityClz = NULL;
}

class AndroidFrameGuard
{
public:
    AndroidFrameGuard(JNIEnv* env, int n) :
            m_env(env)
    {
        env->PushLocalFrame(n);
    }

    ~AndroidFrameGuard()
    {
        m_env->PopLocalFrame(0);
    }

private:
    JNIEnv* m_env;
};


jboolean isAndroidOskShown(JNIEnv* env)
{
    //AndroidFrameGuard guard(env,40);

    jclass contextClass = env->FindClass("android/content/Context");
    jfieldID inputMethodServiceField = env->GetStaticFieldID(
            contextClass,
            "INPUT_METHOD_SERVICE",
            "Ljava/lang/String;");

    jobject inputMethodServiceString = env->GetStaticObjectField(
            contextClass,
            inputMethodServiceField);

    jmethodID activityGetSystemService = env->GetMethodID(
            g_ctx.mainActivityClz,
            "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;");

    jobject inputMethodService = env->CallObjectMethod(
            g_ctx.mainActivityObj,
            activityGetSystemService,
            inputMethodServiceString);

    jclass inputMethodServiceClass = env->GetObjectClass(inputMethodService);

    jmethodID inputMethodServiceIsAcceptingTextMethod = env->GetMethodID(
            inputMethodServiceClass,
            "isAcceptingText",
            "()Z");

    jboolean shown = env->CallBooleanMethod( inputMethodService, inputMethodServiceIsAcceptingTextMethod);

    return shown;
}

void toggleAndroidOSK(JNIEnv* env, jobject thiz)
{

    jmethodID setInputTypeMethod = env->GetMethodID( g_ctx.mainActivityClz, "toggleTextEditInputType", "()V");
    jstring text = env->NewStringUTF( "Hello world");
    env->CallVoidMethod( g_ctx.mainActivityObj, setInputTypeMethod, text);

    if(!isAndroidOskShown(env)) {
        jclass contextClass = env->FindClass("android/content/Context");
        jfieldID inputMethodServiceField = env->GetStaticFieldID(
                contextClass,
                "INPUT_METHOD_SERVICE",
                "Ljava/lang/String;");

        jobject inputMethodServiceString = env->GetStaticObjectField(
                contextClass,
                inputMethodServiceField);

        jmethodID activityGetSystemService = env->GetMethodID(
                g_ctx.mainActivityClz,
                "getSystemService",
                "(Ljava/lang/String;)Ljava/lang/Object;");

        jobject inputMethodService = env->CallObjectMethod(
                g_ctx.mainActivityObj,
                activityGetSystemService,
                inputMethodServiceString);

        jclass inputMethodServiceClass = env->GetObjectClass(inputMethodService);

        jmethodID inputMethodServiceToggleSoftInputMethod = env->GetMethodID(
                inputMethodServiceClass,
                "toggleSoftInput",
                "(II)V");

        env->CallVoidMethod(inputMethodService, inputMethodServiceToggleSoftInputMethod,
                            2 /*SHOW_FORCED*/, 0);
    }
}


/** JNI C interface **/
extern "C" {
    JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_toggleAndroidOskJNI(JNIEnv *env, jobject thiz);
    JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_initJNI(JNIEnv *env, jobject thiz);
    JNIEXPORT jstring JNICALL Java_com_company_hideosksuggestions_MainActivity_stringFromJNI( JNIEnv* env, jobject thiz );
    JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_starts(JNIEnv *env, jobject thiz);
}

JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_toggleAndroidOskJNI(JNIEnv *env, jobject thiz) {
    toggleAndroidOSK(env,thiz);
}

JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_initJNI(JNIEnv *env, jobject thiz) {
    initJNI(env, thiz);
}

JNIEXPORT jstring JNICALL Java_com_company_hideosksuggestions_MainActivity_stringFromJNI( JNIEnv* env, jobject thiz ) {
    return stringFromJNI(env,thiz);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM **vm, void* reserved)
{
    return onLoad(vm);
}

JNIEXPORT void JNICALL Java_com_company_hideosksuggestions_MainActivity_starts(JNIEnv *env, jobject thiz)
{
    mainActivityStarts(env,thiz);
}

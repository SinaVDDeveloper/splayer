#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include "player.h"
#include "player_jni_entry.h"
#include "player_log.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#ifndef NELEM
# define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif


#ifdef __cplusplus
extern "C" {
#endif

class AndroidNativeOpenGl2Channel;


/*
 * Register native JNI-callable methods.
 *
 * "className" looks like "java/lang/String".
 */
int jniRegisterNativeMethods(JNIEnv* env, const char* className,
    const JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    STrace::Log(SPLAYER_TRACE_DEBUG,"Registering %s natives\n", className);
    clazz = env->FindClass( className);
    if (clazz == NULL) {
        STrace::Log(SPLAYER_TRACE_ERROR,"Native registration unable to find class '%s'\n", className);
        return -1;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        STrace::Log(SPLAYER_TRACE_ERROR,"RegisterNatives failed for '%s'\n", className);
        return -1;
    }
    return 0;
}


/*
 * Throw an exception with the specified class and an optional message.
 */
int jniThrowException(JNIEnv* env, const char* className, const char* msg)
{
    jclass exceptionClass;

    exceptionClass = env->FindClass( className);
    if (exceptionClass == NULL) {
        STrace::Log(SPLAYER_TRACE_ERROR,"Unable to find exception class %s\n", className);
        assert(0);      /* fatal during dev; should always be fatal? */
        return -1;
    }

    if (env->ThrowNew( exceptionClass, msg) != JNI_OK) {
        STrace::Log(SPLAYER_TRACE_ERROR,"Failed throwing '%s' '%s'\n", className, msg);
        assert(!"failed to throw");
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

static const char* const kMPClassPathName 					= 	"com/sina/sinavideo/coreplayer/splayer/MediaPlayer";
static const char* const kMPViewClassPathName 				= 	"com/sina/sinavideo/coreplayer/splayer/VideoView";
static const char* const kMPMediaCodecWrapClassPathName 	= 	"com/sina/sinavideo/coreplayer/splayer/SMedia";
static LYH_Mutex* g_mp_lock = NULL; 	//static Mutex sLock;
static STrace* gStrace = NULL;


struct fields_t {
    jfieldID    context;
    //jfieldID    surface_texture;
    jmethodID   post_event;
};


static fields_t fields;
static JavaVM* g_java_vm = NULL;


// ----------------------------------------------------------------------------

JNIMediaPlayerListener::JNIMediaPlayerListener(JavaVM* jvm_, jobject thiz, jobject weak_thiz)
{
    // Hold onto the MediaPlayer class for use in calling the static method
    // that posts events to the application thread.
    jvm = jvm_;

	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}

	 
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        STrace::Log(SPLAYER_TRACE_ERROR,"Can't find MediaPlayer class");
        jniThrowException(env, "java/lang/Exception", NULL);
        return;
    }
    mClass = (jclass)env->NewGlobalRef(clazz);

    // We use a weak reference so the MediaPlayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    mObject  = env->NewGlobalRef(weak_thiz);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}

JNIMediaPlayerListener::~JNIMediaPlayerListener()
{
    // remove global references
	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
	 
    env->DeleteGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}

void JNIMediaPlayerListener::notify(int msg, int ext1, int ext2, void *obj)
{
    bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
    
	env->CallStaticVoidMethod(mClass, fields.post_event, mObject, msg, ext1, ext2, NULL);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}


// ----------------------------------------------------------------------------

JNIAndroidOpenGl2RendererListener::JNIAndroidOpenGl2RendererListener(JavaVM* jvm_, jobject weak_thiz)
{
   
    jvm = jvm_;

	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}

	 // get the ViEAndroidGLES20 class
	jclass javaRenderClassLocal = env->FindClass(kMPViewClassPathName);
	if (!javaRenderClassLocal) {
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not find ViEAndroidGLES20", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
		return ;
	}

	// create a global reference to the class (to tell JNI that
	// we are referencing it after this function has returned)
	mClass = reinterpret_cast<jclass> (env->NewGlobalRef(javaRenderClassLocal));
	if (!mClass) {
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not create Java ViEAndroidGLES20 class reference", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
		return ;
	}

	// Delete local class ref, we only use the global ref
	env->DeleteLocalRef(javaRenderClassLocal);
    
    // We use a weak reference so the MediaPlayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    mObject  = env->NewGlobalRef(weak_thiz);

	 // get the method ID for the ReDraw function
  	_redrawCid = env->GetMethodID(mClass, "ReDraw", "()V");
  	if (_redrawCid == NULL) {
    	STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not get ReDraw ID", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
    	return ;
  	}

  	_registerNativeCID = env->GetMethodID(mClass,"RegisterNativeObject", "(J)V");
  	if (_registerNativeCID == NULL) {
    	STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not get RegisterNativeObject ID", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
    	return ;
  	}

  	_deRegisterNativeCID = env->GetMethodID(mClass,"DeRegisterNativeObject", "(J)V");
  	if (_deRegisterNativeCID == NULL) {
    	STrace::Log(SPLAYER_TRACE_ERROR, "%s: could not get DeRegisterNativeObject ID", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
    	return ;
  	}

///
   _getWidthCid = env->GetMethodID(mClass, "GetWidth", "()I");
   if (_getWidthCid == NULL) {
	   STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not get GetWidth ID", __FUNCTION__);
	   jniThrowException(env, "java/lang/Exception", NULL);
	   return ;
   }

     _getHeightCid = env->GetMethodID(mClass, "GetHeight", "()I");
   if (_getHeightCid == NULL) {
	   STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not get GetHeight ID", __FUNCTION__);
	   jniThrowException(env, "java/lang/Exception", NULL);
	   return ;
   }

///
  	JNINativeMethod nativeFunctions[2] = {
    	{ "DrawNative","(J)V",(void*) &AndroidNativeOpenGl2Channel::DrawNativeStatic, },
    	{ "CreateOpenGLNative","(JII)I",(void*) &AndroidNativeOpenGl2Channel::CreateOpenGLNativeStatic },
  	};
  	if (env->RegisterNatives(mClass, nativeFunctions, 2) == 0) {
    	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: Registered native functions", __FUNCTION__);
  	}
  	else {
    	STrace::Log(SPLAYER_TRACE_ERROR,"%s: Failed to register native functions", __FUNCTION__);
		jniThrowException(env, "java/lang/Exception", NULL);
    	return ;
  	}

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}

JNIAndroidOpenGl2RendererListener::~JNIAndroidOpenGl2RendererListener()
{
    // remove global references
	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
	 
    env->DeleteGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}




void JNIAndroidOpenGl2RendererListener::unRegisterObject(long context)
{
    bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
  	
  	env->CallVoidMethod(mObject, _deRegisterNativeCID,(jlong) context);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}


void JNIAndroidOpenGl2RendererListener::redraw()
{
	//STrace::Log(SPLAYER_TRACE_ERROR,"%s: into",__FUNCTION__);	   

    bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
	
  	env->CallVoidMethod(mObject, _redrawCid);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}

void JNIAndroidOpenGl2RendererListener::registerObject(long context)
{
    bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
  	
  	env->CallVoidMethod(mObject, _registerNativeCID, (jlong) context);

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
}


int JNIAndroidOpenGl2RendererListener::getSize(int &width, int &height)
{
	bool isAttached = false;  
   JNIEnv* env = NULL;	
   if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
	   jint res = jvm->AttachCurrentThread(&env, NULL); 	
	   if ((res < 0) || !env) { 	 
		   STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);	  
		   return -1;    
	   }	
	   isAttached = true;  
   }
   
   width = env->CallIntMethod(mObject, _getWidthCid);
   height = env->CallIntMethod(mObject, _getHeightCid);

   if (isAttached) {
	   if (jvm->DetachCurrentThread() < 0) {
		   STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
	   }
   }
   return 0;
}


// ----------------------------------------------------------------------------

JNIAndroidSurfaceRendererListener::JNIAndroidSurfaceRendererListener(JavaVM* jvm_, jobject thiz)
{
   
    jvm = jvm_;
	mObject = 0;
	mMediacodecClass = 0;

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);	   

	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}
	
    mObject  = env->NewGlobalRef(thiz);

	///add mediacodec_class for hard decode
	jclass g_mediacodec_class_local = env->FindClass(kMPMediaCodecWrapClassPathName); 
	if (!g_mediacodec_class_local) {
	   STrace::Log(SPLAYER_TRACE_ERROR,"%s: fuck !!! could not find %s class", __FUNCTION__,kMPMediaCodecWrapClassPathName);
	   //jniThrowException(env, "java/lang/RuntimeException", "could not find SMedia class");
	   //return ;
	}else{
		mMediacodecClass = reinterpret_cast<jclass> (env->NewGlobalRef(g_mediacodec_class_local));
		if (!mMediacodecClass) {
	   		STrace::Log(SPLAYER_TRACE_ERROR,"%s: could not create mMediacodecClass reference", __FUNCTION__);
	   		//jniThrowException(env, "java/lang/Exception", NULL);
	   		//return ;
		}
		env->DeleteLocalRef(g_mediacodec_class_local);
	}

	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: out",__FUNCTION__);	
}

JNIAndroidSurfaceRendererListener::~JNIAndroidSurfaceRendererListener()
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);	

    // remove global references
	bool isAttached = false;  
	JNIEnv* env = NULL;  
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {    
		jint res = jvm->AttachCurrentThread(&env, NULL);     
		if ((res < 0) || !env) {      
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not attach thread to JVM (%d, %p)",__FUNCTION__,res, env);      
			return ;    
		}    
		isAttached = true;  
	}

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: releasing surface object",__FUNCTION__);
    if(mObject){
		env->DeleteGlobalRef(mObject);
		mObject = NULL;
    }

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: releasing meidacodec class",__FUNCTION__);
	if(mMediacodecClass){
		env->DeleteGlobalRef(mMediacodecClass);
		mMediacodecClass = NULL;
	}

	
	if (isAttached) {
		if (jvm->DetachCurrentThread() < 0) {
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: Could not detach thread from JVM",__FUNCTION__);
		}
	}
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: out",__FUNCTION__);	
}

	
jobject JNIAndroidSurfaceRendererListener::getSurface()
{
	return mObject;
}


jclass JNIAndroidSurfaceRendererListener::getMediaCodecClass()
{
	return mMediacodecClass;
}

// ----------------------------------------------------------------------------

static MediaPlayer* getMediaPlayer(JNIEnv* env, jobject thiz)
{
    //Mutex::Autolock l(sLock);
    if(g_mp_lock)LYH_LockMutex(g_mp_lock);
    MediaPlayer* p = (MediaPlayer*)env->GetIntField(thiz, fields.context);
	if(g_mp_lock)LYH_UnlockMutex(g_mp_lock);
    return p;
}

static MediaPlayer* setMediaPlayer(JNIEnv* env, jobject thiz, MediaPlayer* context)
{
    //Mutex::Autolock l(sLock);
    if(g_mp_lock)LYH_LockMutex(g_mp_lock);
    MediaPlayer* old = (MediaPlayer*)env->GetIntField(thiz, fields.context);
    env->SetIntField(thiz, fields.context, (int)context);
	if(g_mp_lock)LYH_UnlockMutex(g_mp_lock);
    return old;
}

// If exception is NULL and opStatus is not OK, this method sends an error
// event to the client application; otherwise, if exception is not NULL and
// opStatus is not OK, this method throws the given exception to the client
// application.
static void process_media_player_call(MediaPlayer* mp,JNIEnv *env, jobject thiz, int opStatus)
{
	if ( opStatus == SPLAYER_ERROR_SPALYER_STATE ) {
		jniThrowException(env, "java/lang/IllegalStateException", NULL);
	} else if ( opStatus == SPLAYER_ERROR_NO_MEMORY ) {
		jniThrowException(env, "java/lang/RuntimeException", NULL);
	} else if ( opStatus == SPLAYER_ERROR_PARAMETER ) {
	    jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
	} else if ( opStatus == SPLAYER_ERROR_NETWORK ) {
	    //MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_IO,0);
		}
	} else if ( opStatus == SPLAYER_ERROR_UNSUPPORTED ) {
	    //MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_UNSUPPORTED,0);
		}
	} else if ( opStatus == SPLAYER_ERROR_NORMAL ) {
		//MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_UNSUPPORTED,0);
		}
	} else if ( opStatus == SPLAYER_ERROR_INTERNAL ) {
		//MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_MALFORMED,0);
		}
	} else if ( opStatus == SPLAYER_ERROR_TIMEDOUT ) {
		//MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED,MEDIA_ERROR_TIMED_OUT,0);
		}
	} else if ( opStatus != SPLAYER_SUCCESS ){
		//MediaPlayer* mp = getMediaPlayer(env, thiz);
        if (mp != 0) {
			mp->notify(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, 0,0);
		}
	}   
}


static void android_media_MediaPlayer_native_init(JNIEnv *env,jobject thiz,jstring path, jint filter, jint console, jint file)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);

    jclass clazz;

    clazz = env->FindClass(kMPClassPathName);
    if (clazz == NULL) {
        return;
    }

    fields.context = env->GetFieldID(clazz, "mNativeContext", "I");
    if (fields.context == NULL) {
        return;
    }

    fields.post_event = env->GetStaticMethodID(clazz, "postEventFromNative",
                                               "(Ljava/lang/Object;IIILjava/lang/Object;)V");
    if (fields.post_event == NULL) {
        return;
    }

	if(NULL==g_mp_lock){
		g_mp_lock = LYH_CreateMutex();
	}

	const char *tmp = env->GetStringUTFChars(path, NULL);
    if (tmp == NULL) {  // Out of memory
		jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }

	STrace::SetFilter(filter);
	STrace::EnableConsole( (console==1)?true:false);
	STrace::EnableFile( (file==1)?true:false);
	STrace::SetFilePath(tmp);
	
	env->ReleaseStringUTFChars(path, tmp);
    tmp = NULL;

	//STrace::Log(SPLAYER_TRACE_ERROR,"%s init OK",__FUNCTION__);

}


static void android_media_MediaPlayer_native_setup(JNIEnv *env, jobject thiz, jobject weak_this, jint hard)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into,hard=%d", __FUNCTION__,hard);
	int ret;
	
    MediaPlayer* mp; 
	
	mp = MediaPlayer::Create(hard);
	
	if(NULL==mp){
		jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
	}
	ret = mp->setup(g_java_vm,thiz, weak_this);
	
	// Stow our new C++ MediaPlayer in an opaque field in the Java object.
    if(SPLAYER_SUCCESS==ret){
		setMediaPlayer(env, thiz, mp);
    }
	
	process_media_player_call(mp, env, thiz, ret);
   
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
}

static void android_media_MediaPlayer_setDataSource(
        JNIEnv *env, jobject thiz, jstring path,
        jobjectArray keys, jobjectArray values) {

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

    if (path == NULL) {
        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return;
    }

    const char *tmp = env->GetStringUTFChars(path, NULL);
    if (tmp == NULL) {  // Out of memory
		jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: setDataSource: path %s", __FUNCTION__,tmp);

	ret = mp->set_datasource(tmp);
	
    env->ReleaseStringUTFChars(path, tmp);
    tmp = NULL;
	
    process_media_player_call(mp,env, thiz, ret);
}

static void android_media_MediaPlayer_setSurfaceView(JNIEnv *env, jobject thiz, jobject jsurfaceview)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

	if(mp->using_hard_decode()){ //unsupport surfaceview in hard mode
		ret = SPLAYER_ERROR_SPALYER_STATE;
		STrace::Log(SPLAYER_TRACE_ERROR,"%s unsupport surfaceview in hard mode", __FUNCTION__);
		process_media_player_call(mp,env, thiz, ret);
		return;
	}

	//JNIAndroidOpenGl2RendererListener* listener = new JNIAndroidOpenGl2RendererListener(g_java_vm, jsurfaceview);
	//if (listener == NULL) {
    //    jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
    //    return;
    //}
    //mp->opengl2_render_listener = listener;

	ret = mp->set_surfaceview(g_java_vm,jsurfaceview);
	
    process_media_player_call(mp, env, thiz, ret);
}

static void android_media_MediaPlayer_setSurface(JNIEnv *env, jobject thiz, jobject jsurface)
{
	//STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int ret;

    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL) {
		STrace::Log(SPLAYER_TRACE_ERROR,"%s mp NULL", __FUNCTION__);
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	
	if(!mp->using_hard_decode()){ //unsupport surface in soft mode
		ret = SPLAYER_ERROR_SPALYER_STATE;
		STrace::Log(SPLAYER_TRACE_ERROR,"%s unsupport surface in soft mode", __FUNCTION__);
		process_media_player_call(mp,env, thiz, ret);
		return;
	}

	ret = mp->set_surface(g_java_vm, jsurface); 
	
	//STrace::Log(SPLAYER_TRACE_DEBUG,"%s 5 ,ret=%d", __FUNCTION__,ret);
    process_media_player_call(mp,env, thiz, ret);
}

static void android_media_MediaPlayer_prepare(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;

    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }


	//if(NULL==g_java_vm){
	//	STrace::Log(SPLAYER_TRACE_ERROR,"%s: jvm NULL",__FUNCTION__);
	//	jniThrowException(env, "java/lang/IllegalStateException", NULL);
    //    return;
	//}
	//mp->jvm = g_java_vm;

	ret = mp->prepare();
	
    process_media_player_call( mp, env, thiz,ret );
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: out",__FUNCTION__);
}

static void android_media_MediaPlayer_prepareAsync(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret ;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

 
	//if(NULL==g_java_vm){
	//	STrace::Log(SPLAYER_TRACE_ERROR,"%s: jvm NULL",__FUNCTION__);
	//	jniThrowException(env, "java/lang/IllegalStateException", NULL);
    //    return;
	//}
	//mp->jvm = g_java_vm;

	ret = mp->prepareAsync();
	
    process_media_player_call( mp, env, thiz,ret );
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: out",__FUNCTION__);
}

static void android_media_MediaPlayer_start(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	
	ret = mp->start();

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s start result=%d",__FUNCTION__,ret);
	
    process_media_player_call( mp, env, thiz, ret );
}

static void android_media_MediaPlayer_stop(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	
	ret = mp->stop();
	
    process_media_player_call( mp, env, thiz, ret );
}

static void android_media_MediaPlayer_pause(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

	ret = mp->pause();
	
    process_media_player_call( mp, env, thiz, ret );
}

static jboolean android_media_MediaPlayer_isPlaying(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
	
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return false;
    }
    jboolean is_playing;

	is_playing = mp->is_playing();

    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: isPlaying: %d", __FUNCTION__, is_playing);
    return is_playing;
}

static void android_media_MediaPlayer_seekTo(JNIEnv *env, jobject thiz, int msec)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into,%d(msec)",__FUNCTION__,msec);
	int ret;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	
	ret = mp->seekto(msec);
	
    process_media_player_call( mp, env, thiz, ret );
}

static int android_media_MediaPlayer_getVideoWidth(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int w = mp->get_video_width();
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: getVideoWidth: %d",__FUNCTION__, w);
    return w;
}

static int android_media_MediaPlayer_getVideoHeight(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int h = mp->get_video_height();
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: getVideoHeight: %d",__FUNCTION__, h);
    return h;
}


static float android_media_MediaPlayer_getVideoAspectRatio(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    float v = mp->get_video_aspect_ratio();
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: getVideoAspectRatio: %f",__FUNCTION__, v);
    return v;
}


static int android_media_MediaPlayer_getCurrentPosition(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
	
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
		///应该去掉抛出异常?还是上面捕捉?
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec = mp->get_current_position();
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: getCurrentPosition: %d (msec)",__FUNCTION__ ,msec);
    return msec;
}

static int android_media_MediaPlayer_getDuration(JNIEnv *env, jobject thiz)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
	int ret;
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec =	mp->get_duration();
	
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s: getDuration: %d (msec)",__FUNCTION__, msec);
    return msec;
}

static void android_media_MediaPlayer_reset(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int ret;
		
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

	ret = mp->reset();
	
    process_media_player_call(mp, env, thiz, ret );
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
}

static void android_media_MediaPlayer_setLooping(JNIEnv *env, jobject thiz, jboolean looping)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into,looping=%d", __FUNCTION__,looping);
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	int ret = mp->set_looping(looping);
	
    process_media_player_call(mp,  env, thiz, ret );
}

static jboolean android_media_MediaPlayer_isLooping(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return false;
    }
	jboolean islooping = mp->is_looping();
	
    return islooping;
}

static void android_media_MediaPlayer_release(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
	int ret ;
    
    MediaPlayer* mp = setMediaPlayer(env, thiz, 0);
    if (mp != NULL) {
		ret = mp->release();

		MediaPlayer::Destroy(mp);
		//process_media_player_call( mp, env, thiz, ret );
    }else{
		 STrace::Log(SPLAYER_TRACE_WARNING,"%s MediaPlayer field NULL", __FUNCTION__);
	}
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
}

static void android_media_MediaPlayer_native_finalize(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp != NULL) {
        STrace::Log(SPLAYER_TRACE_DEBUG,"%s: MediaPlayer finalized without being released", __FUNCTION__);
    }
    android_media_MediaPlayer_release(env, thiz);
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out", __FUNCTION__);
}


static void android_media_MediaPlayer_config(JNIEnv * env,jobject thiz,jstring cfg,jint val)
{	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into",__FUNCTION__);	

    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	const char* cfg_ = env->GetStringUTFChars( cfg , NULL);	
	if(cfg_==NULL){		
		STrace::Log(SPLAYER_TRACE_ERROR,"%s get file error!",__FUNCTION__);		
		return ;	
	}	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s config=%s,value=%d",__FUNCTION__,cfg_,val);	
	
	int ret = mp->config(cfg_,val); 

	env->ReleaseStringUTFChars(cfg, cfg_);			

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out!",__FUNCTION__);	
	
}

static jboolean android_media_MediaPlayer_isBuffering(JNIEnv *env, jobject thiz)
{
    STrace::Log(SPLAYER_TRACE_DEBUG,"%s into", __FUNCTION__);
    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return false;
    }
	jboolean isbuffering = mp->is_buffering();
	
    return isbuffering;
}

static void android_media_MediaPlayer_setBufferSize(JNIEnv *env, jobject thiz, jint buf_size)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into",__FUNCTION__);	

    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s buffer size=%d",__FUNCTION__,buf_size);	

	if(buf_size>=0){
		mp->config("buffer",buf_size);	
	}

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out!",__FUNCTION__);
}


static void android_media_MediaPlayer_setVolume(JNIEnv *env, jobject thiz, jfloat left, jfloat right)
{
	STrace::Log(SPLAYER_TRACE_DEBUG,"%s into,left=%f,right=%f",__FUNCTION__,left,right);	

    MediaPlayer* mp = getMediaPlayer(env, thiz);
    if (mp == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

	mp->setVolume(left, right); 

	STrace::Log(SPLAYER_TRACE_DEBUG,"%s out!",__FUNCTION__);
}


// ----------------------------------------------------------------------------

static JNINativeMethod gMethods[] = {
	{"native_init",         "(Ljava/lang/String;III)V",         (void *)android_media_MediaPlayer_native_init},
	{"native_setup",        "(Ljava/lang/Object;I)V",           (void *)android_media_MediaPlayer_native_setup},
	{"native_finalize",     "()V",                              (void *)android_media_MediaPlayer_native_finalize},
    {"setDataSource", 		"(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",(void *)android_media_MediaPlayer_setDataSource},
    {"setDisplay",    		"(Landroid/view/SurfaceView;)V",    (void *)android_media_MediaPlayer_setSurfaceView},
    {"setSurface",    		"(Landroid/view/Surface;)V",    	(void *)android_media_MediaPlayer_setSurface},
    {"prepare",             "()V",                              (void *)android_media_MediaPlayer_prepare},
    {"prepareAsync",        "()V",                              (void *)android_media_MediaPlayer_prepareAsync},
    {"native_start",        "()V",                              (void *)android_media_MediaPlayer_start},
    {"native_stop",         "()V",                              (void *)android_media_MediaPlayer_stop},
    {"getVideoWidth",       "()I",                              (void *)android_media_MediaPlayer_getVideoWidth},
    {"getVideoHeight",      "()I",                              (void *)android_media_MediaPlayer_getVideoHeight},
    {"getVideoAspectRatio", "()F",                              (void *)android_media_MediaPlayer_getVideoAspectRatio},
    {"seekTo",              "(I)V",                             (void *)android_media_MediaPlayer_seekTo},
    {"native_pause",        "()V",                              (void *)android_media_MediaPlayer_pause},
    {"isPlaying",           "()Z",                              (void *)android_media_MediaPlayer_isPlaying},
    {"getCurrentPosition",  "()I",                              (void *)android_media_MediaPlayer_getCurrentPosition},
    {"getDuration",         "()I",                              (void *)android_media_MediaPlayer_getDuration},
    {"native_release",      "()V",                              (void *)android_media_MediaPlayer_release},
    {"native_reset",        "()V",                              (void *)android_media_MediaPlayer_reset},
    {"setLooping",          "(Z)V",                             (void *)android_media_MediaPlayer_setLooping},
    {"isLooping",           "()Z",                              (void *)android_media_MediaPlayer_isLooping},
    {"isBuffering",         "()Z",                              (void *)android_media_MediaPlayer_isBuffering},
    {"setBufferSize",       "(I)V",                             (void *)android_media_MediaPlayer_setBufferSize},
    {"setVolume",       	"(FF)V",                            (void *)android_media_MediaPlayer_setVolume},
    {"config",           	"(Ljava/lang/String;I)V",           (void *)android_media_MediaPlayer_config},
};


// This function only registers the native methods
static int register_android_media_MediaPlayer(JNIEnv *env)
{
    return jniRegisterNativeMethods(env,kMPClassPathName, gMethods, NELEM(gMethods));
}
/*
static void signal_handle(int sig, siginfo_t *siginfo, void *context)
{
	char buf[100];
	memset(buf,0,sizeof(buf));
	sprintf(buf,"sig=%d,si_addr=%x",sig,siginfo->si_addr);
	
	int fd = open("/sdcard/serror",O_CREAT | O_RDWR | O_TRUNC);
	if (fd<0) 
		exit(-1);

	write(fd, buf, strlen(buf));

	close(fd);
	
	exit(-1);
}
*/
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

	if(gStrace==NULL){
		gStrace = STrace::Create();
		if(gStrace){
			STrace::Log(SPLAYER_TRACE_ERROR,"%s: into",__FUNCTION__);
		}
	}

	if(vm==NULL){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: vm NULL\n",__FUNCTION__);
		return result;
	}
	g_java_vm = vm;
	
    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        STrace::Log(SPLAYER_TRACE_ERROR,"%s: ERROR: GetEnv failed\n",__FUNCTION__);
        goto bail;
    }
    assert(env != NULL);

    if (register_android_media_MediaPlayer(env) < 0) {
        STrace::Log(SPLAYER_TRACE_ERROR,"%s: ERROR: MediaPlayer native registration failed\n",__FUNCTION__);
        goto bail;
    }

	/*
	//install SIGSEGV signal handle
	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	// Use the sa_sigaction field because the handles has two additional parameters
	act.sa_sigaction = &signal_handle;
	//The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler.
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &act, NULL) < 0) {
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: install SIGSEGV signal handle failed\n",__FUNCTION__);
		result = -1;
		goto bail;
	}else{
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: install SIGSEGV signal handle success\n",__FUNCTION__);
	}
	*/
	
    // success -- return valid version number
    STrace::Log(SPLAYER_TRACE_ERROR,"%s:  JNI_OnLoad() success\n",__FUNCTION__);
    result = JNI_VERSION_1_4;

bail:
    return result;
}


void JNI_OnUnload(JavaVM* vm, void* reserved)
{
	if(g_mp_lock!=NULL){
		LYH_DestroyMutex(g_mp_lock);
		g_mp_lock = NULL;
	}

 	if(gStrace){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s into",__FUNCTION__);
		gStrace->Destroy();
		gStrace = NULL;
	}
}


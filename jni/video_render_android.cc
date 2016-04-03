#include "video_render_android.h"
#include "player_log.h"
#include <sys/stat.h>
#include <fcntl.h> 
#include <errno.h>


/*************************AndroidNativeOpenGl2Renderer********************/

AndroidNativeOpenGl2Renderer::AndroidNativeOpenGl2Renderer(
	//JavaVM* jvm,
    //void* window
    JNIAndroidOpenGl2RendererListener* opengl2_render_listener
	) :
    //_javaRenderObj(NULL),
    //_javaRenderClass(NULL),
	_openGLChannel(NULL),
	_opengl2_render_listener(opengl2_render_listener)
	//_ptrWindow((jobject)window),
	//g_jvm(jvm)	
{
	
}



AndroidNativeOpenGl2Renderer::~AndroidNativeOpenGl2Renderer() {
	STrace::Log(SPLAYER_TRACE_WARNING, "~AndroidNativeOpenGl2Renderer into");
			   
	if(_openGLChannel){
		STrace::Log(SPLAYER_TRACE_WARNING, "~AndroidNativeOpenGl2Renderer() delete _openGLChannel");
		delete _openGLChannel;
		_openGLChannel = NULL;
	}
	/*
	if (g_jvm) {
		bool isAttached = false;
		JNIEnv* env = NULL;
		if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		  jint res = g_jvm->AttachCurrentThread(&env, NULL);
		  if ((res < 0) || !env) {
			STrace::Log("%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, res, env);
			env = NULL;
		  }else {
			isAttached = true;
		  }
		}

		env->DeleteGlobalRef(_javaRenderObj);
		env->DeleteGlobalRef(_javaRenderClass);

		if (isAttached) {
		  if (g_jvm->DetachCurrentThread() < 0) {
			STrace::Log("%s: Could not detach thread from JVM", __FUNCTION__);
		  }
		}
	}*/
	STrace::Log(SPLAYER_TRACE_DEBUG, "~AndroidNativeOpenGl2Renderer out");
}

int AndroidNativeOpenGl2Renderer::Init() {
	STrace::Log(SPLAYER_TRACE_DEBUG, "AndroidNativeOpenGl2Renderer::%s into", __FUNCTION__);
	/*
	if (!g_jvm) {
		STrace::Log("(%s): Not a valid Java VM pointer.", __FUNCTION__);
		return -1;
	}
	if (!_ptrWindow) { 
		STrace::Log("(%s): No window have been provided.", __FUNCTION__);
		return -1;
	}

	// get the JNI env for this thread
	bool isAttached = false;
	JNIEnv* env = NULL;
	if (g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		jint res = g_jvm->AttachCurrentThread(&env, NULL);
		if ((res < 0) || !env) {
		  STrace::Log("%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, res, env);
		  return -1;
		}
		isAttached = true;
	}

	// get the ViEAndroidGLES20 class
	jclass javaRenderClassLocal = env->FindClass("cn/com/sina/player/ViEAndroidGLES20");
	if (!javaRenderClassLocal) {
		STrace::Log("%s: could not find ViEAndroidGLES20", __FUNCTION__);
		return -1;
	}

	// create a global reference to the class (to tell JNI that
	// we are referencing it after this function has returned)
	_javaRenderClass = reinterpret_cast<jclass> (env->NewGlobalRef(javaRenderClassLocal));
	if (!_javaRenderClass) {
		STrace::Log("%s: could not create Java SurfaceHolder class reference", __FUNCTION__);
		return -1;
	}

	// Delete local class ref, we only use the global ref
	env->DeleteLocalRef(javaRenderClassLocal);

	// create a reference to the object (to tell JNI that we are referencing it
	// after this function has returned)
	_javaRenderObj = _ptrWindow;
	//_javaRenderObj = env->NewGlobalRef(_ptrWindow);
	//if (!_javaRenderObj) {
	//	STrace::Log("%s: could not create Java SurfaceRender object reference",__FUNCTION__);
	//	return -1;
	//}

	// Detach this thread if it was attached
	if (isAttached) {
		if (g_jvm->DetachCurrentThread() < 0) {
		  STrace::Log("%s: Could not detach thread from JVM", __FUNCTION__);
		}
	}
	*/
	if(!_openGLChannel){
		_openGLChannel = CreateAndroidRenderChannel(-1, 0, 0.0f, 0.0f, 1.0f, 1.0f);
	}
	
	if(NULL==_openGLChannel){
		STrace::Log(SPLAYER_TRACE_ERROR,"%s: CreateAndroidRenderChannel(-1, 0, 0.0f, 0.0f, 1.0f, 1.0f) fail", __FUNCTION__);
		return -1;
	}
	 
	STrace::Log(SPLAYER_TRACE_DEBUG, "AndroidNativeOpenGl2Renderer::%s done",__FUNCTION__);
			   
	return 0;

}

//AndroidStream*
AndroidNativeOpenGl2Channel*
AndroidNativeOpenGl2Renderer::CreateAndroidRenderChannel(
    int streamId,
    int zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom 
	) {
	
  STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into",__FUNCTION__);
  AndroidNativeOpenGl2Channel* stream = new AndroidNativeOpenGl2Channel(_opengl2_render_listener);
  if (stream && stream->Init(zOrder, left, top, right, bottom) == 0){
    return stream;
  }
  else {
    delete stream;
  }
  return NULL;
}


int AndroidNativeOpenGl2Renderer::RenderFrame(VideoFrame* videoFrame) ///int AndroidNativeOpenGl2Renderer::RenderFrame(VideoFrame& videoFrame)
{
	if(_openGLChannel){
		_openGLChannel->RenderFrame(0,videoFrame);
	}
	return 0;
}

int AndroidNativeOpenGl2Renderer::ReRender(){
	if(_openGLChannel){
		_openGLChannel->ReRender();
	}
	return 0;
}

void AndroidNativeOpenGl2Renderer::GetWidthHeight(int& width, int& height)
{
	if(_openGLChannel){
		_openGLChannel->GetWidthHeight(width, height);
	}
}

/**********************AndroidNativeOpenGl2Channel**********************/

AndroidNativeOpenGl2Channel::AndroidNativeOpenGl2Channel(
    JNIAndroidOpenGl2RendererListener* opengl2_render_listener):
    //_id(streamId),
	_renderCritSect( LYH_CreateMutex() ),
	//_jvm(jvm),
	//_javaRenderObj(javaRenderObj),
    //_registerNativeCID(NULL), _deRegisterNativeCID(NULL),
    _opengl2_render_listener(opengl2_render_listener),
    _openGLRenderer(opengl2_render_listener),
	_width(-1), 
	_height(-1)	{

}
AndroidNativeOpenGl2Channel::~AndroidNativeOpenGl2Channel() {
	STrace::Log(SPLAYER_TRACE_WARNING,"~AndroidNativeOpenGl2Channel() into");

	LYH_LockMutex( _renderCritSect );
	if(_opengl2_render_listener){
		STrace::Log(SPLAYER_TRACE_WARNING,"~AndroidNativeOpenGl2Channel() _opengl2_render_listener->unRegisterObject");
		_opengl2_render_listener->unRegisterObject((long)this);
	}
	LYH_UnlockMutex( _renderCritSect );
	
	LYH_DestroyMutex( _renderCritSect );
	/*
	if (_jvm) {
		bool isAttached = false;
		JNIEnv* env = NULL;
		if (_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		  jint res = _jvm->AttachCurrentThread(&env, NULL);
		  if ((res < 0) || !env) {
			STrace::Log("%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, res, env);
			env = NULL;
		  } else {
			isAttached = true;
		  }
		}
		if (env && _deRegisterNativeCID) {
		  env->CallVoidMethod(_javaRenderObj, _deRegisterNativeCID);
		}

		if (isAttached) {
		  if (_jvm->DetachCurrentThread() < 0) {
			STrace::Log("%s: Could not detach thread from JVM", __FUNCTION__);
		  }
		}
	}*/
	
	STrace::Log(SPLAYER_TRACE_DEBUG,"~AndroidNativeOpenGl2Channel out");
}

int AndroidNativeOpenGl2Channel::Init(	int zOrder,
                                        const float left,
                                        const float top,
                                        const float right,
                                        const float bottom)
{
  STrace::Log(SPLAYER_TRACE_DEBUG,"AndroidNativeOpenGl2Channel::%s: into", __FUNCTION__);

  /*
  if (!_jvm) {
    STrace::Log("%s: Not a valid Java VM pointer", __FUNCTION__);
    return -1;
  }

  // get the JNI env for this thread
  bool isAttached = false;
  JNIEnv* env = NULL;
  if (_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    jint res = _jvm->AttachCurrentThread(&env, NULL);
    if ((res < 0) || !env) {
      STrace::Log("%s: Could not attach thread to JVM (%d, %p)", __FUNCTION__, res, env);
      return -1;
    }
    isAttached = true;
  }

  jclass javaRenderClass = env->FindClass("cn/com/sina/player/ViEAndroidGLES20");
  if (!javaRenderClass) {
    STrace::Log("%s: could not find ViESurfaceRenderer", __FUNCTION__);
    return -1;
  }

  // get the method ID for the ReDraw function
  _redrawCid = env->GetMethodID(javaRenderClass, "ReDraw", "()V");
  if (_redrawCid == NULL) {
    STrace::Log("%s: could not get ReDraw ID", __FUNCTION__);
    return -1;
  }

  _registerNativeCID = env->GetMethodID(javaRenderClass,"RegisterNativeObject", "(J)V");
  if (_registerNativeCID == NULL) {
    STrace::Log("%s: could not get RegisterNativeObject ID", __FUNCTION__);
    return -1;
  }

  _deRegisterNativeCID = env->GetMethodID(javaRenderClass,"DeRegisterNativeObject", "()V");
  if (_deRegisterNativeCID == NULL) {
    STrace::Log( "%s: could not get DeRegisterNativeObject ID", __FUNCTION__);
    return -1;
  }

  JNINativeMethod nativeFunctions[2] = {
    { "DrawNative",
      "(J)V",
      (void*) &AndroidNativeOpenGl2Channel::DrawNativeStatic, },
    { "CreateOpenGLNative",
      "(JII)I",
      (void*) &AndroidNativeOpenGl2Channel::CreateOpenGLNativeStatic },
  };
  if (env->RegisterNatives(javaRenderClass, nativeFunctions, 2) == 0) {
    STrace::Log("%s: Registered native functions", __FUNCTION__);
  }
  else {
    STrace::Log("%s: Failed to register native functions", __FUNCTION__);
    return -1;
  }

  STrace::Log("%s: _javaRenderObj=%0x", __FUNCTION__,_javaRenderObj);
  env->CallVoidMethod(_javaRenderObj, _registerNativeCID, (jlong) this);
  STrace::Log("%s: out", __FUNCTION__);

  // Detach this thread if it was attached
  if (isAttached) {
    if (_jvm->DetachCurrentThread() < 0) {
      STrace::Log( "%s: Could not detach thread from JVM", __FUNCTION__);
    }
  }
	*/
  LYH_LockMutex( _renderCritSect );
  if(_opengl2_render_listener){
	_opengl2_render_listener->registerObject((long) this);
  }
  LYH_UnlockMutex( _renderCritSect );

  if (_openGLRenderer.SetCoordinates(zOrder, left, top, right, bottom) != 0) {
    	return -1;
  }
  STrace::Log(SPLAYER_TRACE_DEBUG,"%s: AndroidNativeOpenGl2Channel done", __FUNCTION__);
  return 0;
}

int AndroidNativeOpenGl2Channel::RenderFrame(
    const int streamId,
    VideoFrame* videoFrame) { ///VideoFrame& videoFrame) {
  	//   STrace::Log(kTraceInfo, kTraceVideoRenderer,_id, "%s:" ,__FUNCTION__);
  	//LYH_LockMutex( _renderCritSect );
  	_bufferToRender.CopyFrame(videoFrame);  ///_bufferToRender.SwapFrame(videoFrame);
  	//LYH_UnlockMutex( _renderCritSect );

  	////////////dumo picture//////////
  	if(0){
	  static int fileindex = 1;
	  char filename[50];
	  sprintf(filename,"/sdcard/%d.yuv",fileindex);
	  int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC); 
	  if(fd>0){
	  	write(fd,_bufferToRender.Buffer(),_bufferToRender.Length());
	  }else{
		STrace::Log(SPLAYER_TRACE_ERROR, "%s: cannot open %s, errno=%d" ,__FUNCTION__,filename,errno);
	  }
	  fileindex++;
	  close(fd);
  	}
	/////////////////////////////////
  
  	DeliverFrame();
  
  	return 0;
}

int AndroidNativeOpenGl2Channel::ReRender(){
	STrace::Log(SPLAYER_TRACE_ERROR, "%s: into" ,__FUNCTION__);
	if(_bufferToRender.Buffer()){
		DeliverFrame();
	}
}


/*Implements AndroidStream
 * Calls the Java object and render the buffer in _bufferToRender
 */
void AndroidNativeOpenGl2Channel::DeliverFrame() {
  /*
  if (_jvm) {
		// get the JNI env for this thread
		bool isAttached = false;
		JNIEnv* env = NULL;
		if (_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		  // try to attach the thread and get the env
		  // Attach this thread to JVM
		  jint res = _jvm->AttachCurrentThread(&env, NULL);

		  // Get the JNI env for this thread
		  if ((res < 0) || !env) {
			STrace::Log(
						 "%s: Could not attach thread to JVM (%d, %p)",
						 __FUNCTION__, res, env);
			env = NULL;
		  } else {
			isAttached = true;
		  }
		}
		if (env) {
		  env->CallVoidMethod(_javaRenderObj, _redrawCid);
		}

		if (isAttached) {
		  if (_jvm->DetachCurrentThread() < 0) {
			STrace::Log(
						 "%s: Could not detach thread from JVM",
						 __FUNCTION__);
		  }
		}
	}*/
	//STrace::Log(SPLAYER_TRACE_ERROR,"%s: into",__FUNCTION__);	
	LYH_LockMutex( _renderCritSect );
	if(_opengl2_render_listener){
		_opengl2_render_listener->redraw();
  	}
	LYH_UnlockMutex( _renderCritSect );
}

/*
 * JNI callback from Java class. Called when the render
 * want to render a frame. Called from the GLRenderThread
 * Method:    DrawNative
 * Signature: (J)V
 */
void JNICALL AndroidNativeOpenGl2Channel::DrawNativeStatic(
    JNIEnv * env, jobject, jlong context) {
  AndroidNativeOpenGl2Channel* renderChannel =
      reinterpret_cast<AndroidNativeOpenGl2Channel*>(context);
  renderChannel->DrawNative();
}

void AndroidNativeOpenGl2Channel::DrawNative() {
	//¼ÓËø
	LYH_LockMutex( _renderCritSect );
 	_openGLRenderer.Render(_bufferToRender);
  	LYH_UnlockMutex( _renderCritSect );
  
}

/*
 * JNI callback from Java class. Called when the GLSurfaceview
 * have created a surface. Called from the GLRenderThread
 * Method:    CreateOpenGLNativeStatic
 * Signature: (JII)I
 */
jint JNICALL AndroidNativeOpenGl2Channel::CreateOpenGLNativeStatic(
    JNIEnv * env,
    jobject,
    jlong context,
    jint width,
    jint height) {

  AndroidNativeOpenGl2Channel* renderChannel = reinterpret_cast<AndroidNativeOpenGl2Channel*> (context);

  STrace::Log(SPLAYER_TRACE_DEBUG,"%s: into.", __FUNCTION__);
  
  return renderChannel->CreateOpenGLNative(width, height);
}

jint AndroidNativeOpenGl2Channel::CreateOpenGLNative(
    int width, int height) {
	int res = _openGLRenderer.Setup(width, height);
	if(-1!=res){
		_width = width;
		_height = height;
	}
  return res; 
}


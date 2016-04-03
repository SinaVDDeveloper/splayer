package com.sina.sinavideo.coreplayer.splayer;

import java.io.IOException;
import java.util.concurrent.locks.ReentrantLock;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.PixelFormat;

import com.sina.sinavideo.coreplayer.splayer.GLSurfaceView;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;

public class VideoView extends GLSurfaceView implements GLSurfaceView.Renderer,
											IPlayControl,
											MediaPlayer.OnPreparedListener,
											MediaPlayer.OnErrorListener,
											MediaPlayer.OnBufferingUpdateListener,
											MediaPlayer.OnCompletionListener,
											MediaPlayer.OnInfoListener,
											MediaPlayer.OnSeekCompleteListener,
											MediaPlayer.OnVideoSizeChangedListener{
    private static String TAG = "VideoView";
    private static final boolean DEBUG = true;
    // True if onSurfaceCreated has been called.
    private boolean surfaceCreated = false;
    private boolean openGLCreated = false;
    // True if NativeFunctionsRegistered has been called.
    private boolean nativeFunctionsRegisted = false;
    private ReentrantLock nativeFunctionLock = new ReentrantLock();
    // Address of Native object that will do the drawing.
    private long nativeObject = 0;
    private int viewWidth = 0;
    private int viewHeight = 0;
    
    //touch event
    private Handler mHandler = null;
    /////////////////////////////////////////
    private Context mContext;
	private String mPlayURL = null;
	private MediaPlayer player = null;
	private SurfaceHolder mSurfaceHolder = null;
	//////////////////////////
	
    public static boolean UseOpenGL2(Object renderWindow) {
        return VideoView.class.isInstance(renderWindow);
    }

    public VideoView(Context context,Handler hand) {
        super(context);
        
    	Log.d(TAG, "into");
        
        init(false, 0, 0);
        
        mHandler = hand;
        mContext = context;
        getHolder().addCallback(mSHCallback);
    }

    public VideoView(Context context,Handler hand,boolean translucent,
            int depth, int stencil) {
        super(context);
        init(translucent, depth, stencil);
        
        mHandler = hand;
        mContext = context;
        getHolder().addCallback(this);
    }

    private void init(boolean translucent, int depth, int stencil) {

        // By default, GLSurfaceView() creates a RGB_565 opaque surface.
        // If we want a translucent one, we should change the surface's
        // format here, using PixelFormat.TRANSLUCENT for GL Surfaces
        // is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
        if (translucent) {
            this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        }

        // Setup the context factory for 2.0 rendering.
        // See ContextFactory class definition below
        setEGLContextFactory(new ContextFactory());

        // We need to choose an EGLConfig that matches the format of
        // our surface exactly. This is going to be done in our
        // custom config chooser. See ConfigChooser class definition
        // below.
        setEGLConfigChooser( translucent ?
                             new ConfigChooser(8, 8, 8, 8, depth, stencil) :
                             new ConfigChooser(5, 6, 5, 0, depth, stencil) );

        // Set the renderer responsible for frame rendering
        this.setRenderer(this);
        this.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }

    private static class ContextFactory implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            Log.w(TAG, "creating OpenGL ES 2.0 context");
            checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
            EGLContext context = egl.eglCreateContext(display, eglConfig,
                    EGL10.EGL_NO_CONTEXT, attrib_list);
            checkEglError("After eglCreateContext", egl);
            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
            egl.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
        }

        // This EGL config specification is used to specify 2.0 rendering.
        // We use a minimum size of 4 bits for red/green/blue, but will
        // perform actual matching in chooseConfig() below.
        private static int EGL_OPENGL_ES2_BIT = 4;
        private static int[] s_configAttribs2 =
        {
            EGL10.EGL_RED_SIZE, 4,
            EGL10.EGL_GREEN_SIZE, 4,
            EGL10.EGL_BLUE_SIZE, 4,
            EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL10.EGL_NONE
        };

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

            // Get the number of minimally matching EGL configurations
            int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            // Allocate then read the array of minimally matching EGL configs
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs, num_config);

            if (DEBUG) {
                //printConfigs(egl, display, configs);
            }
            // Now return the "best" one
            return chooseConfig(egl, display, configs);
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display,
                EGLConfig[] configs) {
            for(EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (d < mDepthSize || s < mStencilSize)
                    continue;

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config,
                            EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config,
                            EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config,
                        EGL10.EGL_ALPHA_SIZE, 0);

                if (r == mRedSize && g == mGreenSize && b == mBlueSize && a == mAlphaSize)
                    return config;
            }
            return null;
        }

        private int findConfigAttrib(EGL10 egl, EGLDisplay display,
                EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private void printConfigs(EGL10 egl, EGLDisplay display,
            EGLConfig[] configs) {
            int numConfigs = configs.length;
            Log.w(TAG, String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                Log.w(TAG, String.format("Configuration %d:\n", i));
                printConfig(egl, display, configs[i]);
            }
        }

        private void printConfig(EGL10 egl, EGLDisplay display,
                EGLConfig config) {
            int[] attributes = {
                    EGL10.EGL_BUFFER_SIZE,
                    EGL10.EGL_ALPHA_SIZE,
                    EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE,
                    EGL10.EGL_RED_SIZE,
                    EGL10.EGL_DEPTH_SIZE,
                    EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT,
                    EGL10.EGL_CONFIG_ID,
                    EGL10.EGL_LEVEL,
                    EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS,
                    EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES,
                    EGL10.EGL_SAMPLE_BUFFERS,
                    EGL10.EGL_SURFACE_TYPE,
                    EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE,
                    EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE,
                    EGL10.EGL_ALPHA_MASK_SIZE,
                    EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE,
                    0x3042 // EGL10.EGL_CONFORMANT
            };
            String[] names = {
                    "EGL_BUFFER_SIZE",
                    "EGL_ALPHA_SIZE",
                    "EGL_BLUE_SIZE",
                    "EGL_GREEN_SIZE",
                    "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE",
                    "EGL_STENCIL_SIZE",
                    "EGL_CONFIG_CAVEAT",
                    "EGL_CONFIG_ID",
                    "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT",
                    "EGL_MAX_PBUFFER_PIXELS",
                    "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE",
                    "EGL_NATIVE_VISUAL_ID",
                    "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES",
                    "EGL_SAMPLES",
                    "EGL_SAMPLE_BUFFERS",
                    "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE",
                    "EGL_TRANSPARENT_RED_VALUE",
                    "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE",
                    "EGL_BIND_TO_TEXTURE_RGB",
                    "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL",
                    "EGL_MAX_SWAP_INTERVAL",
                    "EGL_LUMINANCE_SIZE",
                    "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE",
                    "EGL_RENDERABLE_TYPE",
                    "EGL_CONFORMANT"
            };
            int[] value = new int[1];
            for (int i = 0; i < attributes.length; i++) {
                int attribute = attributes[i];
                String name = names[i];
                if (egl.eglGetConfigAttrib(display, config, attribute, value)) {
                    Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
                } else {
                    // Log.w(TAG, String.format("  %s: failed\n", name));
                    while (egl.eglGetError() != EGL10.EGL_SUCCESS);
                }
            }
        }

        // Subclasses can adjust these values:
        protected int mRedSize;
        protected int mGreenSize;
        protected int mBlueSize;
        protected int mAlphaSize;
        protected int mDepthSize;
        protected int mStencilSize;
        private int[] mValue = new int[1];
    }

    // IsSupported
    // Return true if this device support Open GL ES 2.0 rendering.
    public static boolean IsSupported(Context context) {
    	Log.d(TAG, " IsSupported()");
    	
        ActivityManager am =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo info = am.getDeviceConfigurationInfo();
        if(info.reqGlEsVersion >= 0x20000) {
            // Open GL ES 2.0 is supported.
            return true;
        }
        return false;
    }

    ///implements GLSurfaceView.Renderer
    public void onDrawFrame(GL10 gl) { 
    	//log("onDrawFrame() into");
    	
        nativeFunctionLock.lock();
        if(!nativeFunctionsRegisted || !surfaceCreated) {
        	Log.d(TAG, " !nativeFunctionsRegisted || !surfaceCreated");
            nativeFunctionLock.unlock();
            return;
        }

        if(!openGLCreated) {
        	Log.d(TAG, " CreateOpenGLNative into");
            if(0 != CreateOpenGLNative(nativeObject, viewWidth, viewHeight)) {
            	Log.d(TAG, " CreateOpenGLNative fail");
                return; // Failed to create OpenGL
            }
            openGLCreated = true; // Created OpenGL successfully
        }
        //log("DrawNative() begin");
        DrawNative(nativeObject); // Draw the new frame
        //log("DrawNative() end");
        nativeFunctionLock.unlock();
        //log("onDrawFrame() out");
    }

    ///implements GLSurfaceView.Renderer
    public void onSurfaceChanged(GL10 gl, int width, int height) { 
    	log("onSurfaceChanged() into,width=" + width + ",height="+height);
    	
    	surfaceCreated = true;
        viewWidth = width;
        viewHeight = height;

        nativeFunctionLock.lock();
        if(nativeFunctionsRegisted) {
            if(CreateOpenGLNative(nativeObject,width,height) == 0)
                openGLCreated = true;
        }
        nativeFunctionLock.unlock();
        log("onSurfaceChanged() out");
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) { ///implements GLSurfaceView.Renderer
    	log("onSurfaceCreated() into");
    }

    public void RegisterNativeObject(long nativeObject) {
    	log("RegisterNativeObject() into");
        nativeFunctionLock.lock();
        this.nativeObject = nativeObject;
        nativeFunctionsRegisted = true;
        nativeFunctionLock.unlock();
        log("RegisterNativeObject() out");
    }

    public void DeRegisterNativeObject(long obj) {
    	log( "DeRegisterNativeObject() into");
        nativeFunctionLock.lock();
        nativeFunctionsRegisted = false;
        openGLCreated = false;
        this.nativeObject = 0;
        nativeFunctionLock.unlock();
        log( "DeRegisterNativeObject() out");
    }

    public void ReDraw() {
    	//log( "ReDraw() into" );
        if(surfaceCreated) {
            // Request the renderer to redraw using the render thread context.
            this.requestRender();
        }else{
        	log("suface not created yet");
        }
    }

    ///call from native
    public int GetWidth(){
    	return viewWidth;
    }
    
    ///call from native
    public int GetHeight(){
    	return viewHeight;
    }
    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // MotionEvent reports input details from the touch screen
        // and other input controls. In this case, you are only
        // interested in events where the touch position changed.
    	log("onTouchEvent() into");
        float x = e.getX();
        float y = e.getY();

        switch (e.getAction()) {
            case MotionEvent.ACTION_UP:
            	log("onTouchEvent() MotionEvent.ACTION_UP");
            	if(null!=mHandler){
            		Message message = new Message();
					message.what = 0;
					mHandler.sendMessage(message);
            	}
            	break;
            case MotionEvent.ACTION_DOWN:
            	log("onTouchEvent() MotionEvent.ACTION_DOWN");
            	break;
        }
        return true;
    }
	
  
    private native int CreateOpenGLNative(long nativeObject,int width, int height);
    private native void DrawNative(long nativeObject);
    
    
    /////////////////////////
    @Override
	public void setPlayURL(String path){
		mPlayURL = path;
	}
	@Override
	public void start(){
		if(null!=player){
			try{
				player.start();
			}catch(Exception ex){
				log("start() fail");
			}
		}
	}
	
	@Override
	public void pause(){
		if(null!=player){
			try{
				player.pause();
			}catch(Exception ex){
				log("pause() fail");
			}
		}
	}
	
	@Override
	public void release(){
		if(null!=player){
			try{
				player.release();
				player = null;
			}catch(Exception ex){
				log("release() fail");
			}
		}
	}
	
	@Override
	public boolean isPlaying(){
		if(null!=player){
			return player.isPlaying();
		}
		return false;
	}
	
	@Override
	public int getCurrentPosition(){
		if(null!=player){
			return player.getCurrentPosition();
		}
		return -1;
	}
	
	@Override
	public void seekTo(int msec){
		if(null!=player){
			player.seekTo(msec);
		}
	}
	
	
	@Override
	public int getDuration(){
		if(null!=player){
			return player.getDuration();
		}
		return -1;
	}
	
	
	private int fast = 0;
	private int drop = 0;
	private int lowres = 0;
	private int skip = 0;
	private int idct = 0;
	private int filter = 0;
	private int debug =  MediaPlayer.SPLAYER_TRACE_ERROR;
	private int buffer =  5000;
	private int auto = 0;
	@Override
	public void config(String name, int val){
		if(name.equals("fast")){
			fast = val;
		}else if(name.equals("drop")){
			drop = val;
		}else if(name.equals("lowres")){
			lowres = val;
		}else if(name.equals("skip")){
			skip = val;
		}else if(name.equals("idct")){
			idct = val;
		}else if(name.equals("filter")){
			filter = val;
		}else if(name.equals("buffer")){
			buffer = val;
		}else if(name.equals("debug")){
			debug = val;
		}else if(name.equals("auto")){
			auto = val;
		}
	}
	
	private void Play(String fileName){
		log("play into");
		
		player = new MediaPlayer(mContext,false);	
		player.setOnBufferingUpdateListener(this);
		player.setOnCompletionListener(this);
		player.setOnErrorListener(this);
		player.setOnInfoListener(this);
		player.setOnPreparedListener(this);
		player.setOnSeekCompleteListener(this);
		player.setOnVideoSizeChangedListener(this);
	
		player.config("fast", fast);
		player.config("drop", drop);
		player.config("lowres", lowres);
		player.config("skip", skip);
		player.config("idct", idct);
		player.config("filter", filter);
		player.config("debug", debug);
		player.config("buffer", (int)buffer);
		player.config("auto", 0); //drop auto 
		
		player.setWakeMode(mContext, PowerManager.SCREEN_BRIGHT_WAKE_LOCK);
	
		try{
			player.setDataSource(fileName);
			
			player.setDisplay(this);

//			player.setSurface(mSurfaceHolder.getSurface());
			
			
			player.prepareAsync(); //player.prepare();
			
			////should start at prepard notify , not here
			//player.start();
		
		}catch(IOException ex){
			log("catch IOException");
		}catch(IllegalArgumentException ex){
			log("catch IllegalArgumentException");
		}catch(SecurityException ex){
			log("catch SecurityException");
		}catch(IllegalStateException ex){
			log("catch IllegalStateException");
		}	
		log("play out");
	}
	
	SurfaceHolder.Callback mSHCallback = new SurfaceHolder.Callback() {
	public void surfaceCreated(SurfaceHolder holder){
		log("surfaceCreated() into");
		
		if(mSurfaceHolder==null){
			mSurfaceHolder = holder;
			if(mPlayURL!=null){
				Play(mPlayURL);
			}
		}else{
			log("surfaceCreated() mSurfaceHolder not null");
		}
		log("surfaceCreated() out");
	}
	
	 public void surfaceChanged(SurfaceHolder holder, int format, int w,int h) {
		 log("surfaceChanged() into");
	 }

	 public void surfaceDestroyed(SurfaceHolder holder) {
		 log("surfaceDestroyed() into");
		 
		 if(player!=null){
			 player.release();
			 player = null;
		 }
		 
		 mSurfaceHolder = null;
		 
		 log("surfaceDestroyed() out");
	 }
	};
    ////////////////////////
	 
	 
	// ////////////////MediaPlayer event notifylistener////////////////
	public void onPrepared(MediaPlayer mp) {
		log("recv prepared  notify");
		Message message = new Message();
		message.what = 1;
		mHandler.sendMessage(message);
	}

	public void onCompletion(MediaPlayer mp) {
		log("recv playback completed  notify");
		Message message = new Message();
		message.what = 6;
		mHandler.sendMessage(message);
	}

	public void onBufferingUpdate(MediaPlayer mp, int percent) {
		//log("recv buffer update notify");
		Message message = new Message();
		Bundle bundle = new Bundle();
		message.what = 2;
		bundle.putLong("percent", percent);
		message.setData(bundle);
		mHandler.sendMessage(message);
	}

	public void onSeekComplete(MediaPlayer mp) {
		log("recv seek completed  notify");
		Message message = new Message();
		message.what = 5;
		mHandler.sendMessage(message);
	}

	public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
		log("recv video size changed  notify");
	}

	public boolean onError(MediaPlayer mp, int what, int extra) {
		log("recv error notify");

		Message message = new Message();
		message.what = 7;
		mHandler.sendMessage(message);

		return true;
	}

	public boolean onInfo(MediaPlayer mp, int what, int extra) {
		log("recv info notify");
		if (what == MediaPlayer.MEDIA_INFO_BUFFERING_START) {
			log("recv info notify MEDIA_INFO_BUFFERING_START");
			Message message = new Message();
			message.what = 3;
			mHandler.sendMessage(message);
		} else if (what == MediaPlayer.MEDIA_INFO_BUFFERING_END) {
			log("recv info notify MEDIA_INFO_BUFFERING_END");
			Message message = new Message();
			message.what = 4;
			mHandler.sendMessage(message);
		}

		return true;
	}

	// /////////////////////////listener end///////////////////////////

	  private void log(String str){
	    	if(DEBUG){
	    		Log.d(TAG, "VideoView: " + str);
	    	}
	    }

	@Override
	public void setVolume(float leftVolume) {
		// TODO Auto-generated method stub
		if(player!=null){
			player.setVolume(leftVolume, leftVolume);
		}
	}
}

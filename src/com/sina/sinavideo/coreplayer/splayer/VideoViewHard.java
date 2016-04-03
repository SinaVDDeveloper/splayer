package com.sina.sinavideo.coreplayer.splayer;


import java.io.IOException;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Toast;

import com.sina.sinavideo.coreplayer.splayer.MediaPlayer;

public class VideoViewHard extends SurfaceView implements IPlayControl,
													MediaPlayer.OnPreparedListener,
													MediaPlayer.OnErrorListener,
													MediaPlayer.OnBufferingUpdateListener,
													MediaPlayer.OnCompletionListener,
													MediaPlayer.OnInfoListener,
													MediaPlayer.OnSeekCompleteListener,
													MediaPlayer.OnVideoSizeChangedListener{
	private Context context;
	//private MediaPlayer mp = null;
	private Handler mHandler = null;
	private static String TAG = "VideoViewHard";
	private String mPlayURL = null;
	private MediaPlayer player = null;
	private SurfaceHolder mSurfaceHolder = null;
	private  final boolean DEBUG = false;
	/////
	private int fast = 0;
	private int drop = 0;
	private int lowres = 0;
	private int skip = 0;
	private int idct = 0;
	private int filter = 0;
	private int debug = MediaPlayer.SPLAYER_TRACE_ERROR;
	private int buffer = 5000;
	private int auto = 0;
	////
	
	public VideoViewHard(Context context,Handler handler){
		super(context);
		
		Log.d(TAG, "into");
		
		this.context = context;
		//this.mp = mp;
		this.mHandler = handler;
		getHolder().addCallback(mSHCallback);
	
	}
	
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
	
	
	@Override
	public void setVolume(float leftVolume) {
		// TODO Auto-generated method stub
		if(player!=null){
			player.setVolume(leftVolume, leftVolume);
		}
	}
	
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
		log("play into, fileName=" + fileName);
		
		try{
			player = new MediaPlayer(context,true);	
			player.setOnBufferingUpdateListener(this);
			player.setOnCompletionListener(this);
			player.setOnErrorListener(this);
			player.setOnInfoListener(this);
			player.setOnPreparedListener(this);
			player.setOnSeekCompleteListener(this);
			player.setOnVideoSizeChangedListener(this);
		
			//player.config("fast", fast);
			//player.config("drop", drop);
			//player.config("lowres", lowres);
			//player.config("skip", skip);
			//player.config("idct", idct);
			//player.config("filter", filter);
			player.config("debug", debug);
			player.config("buffer", (int)buffer);
			Log.d(TAG,"set buffer=" + buffer);
			//player.config("auto", 0); //drop auto 
			
			player.setWakeMode(context, PowerManager.SCREEN_BRIGHT_WAKE_LOCK);
	
			player.setDataSource(fileName);
			
//			player.setDisplay(playRender);
			
			player.setSurface(mSurfaceHolder.getSurface());
			
			
			player.prepareAsync(); //player.prepare();
			
			////should start at prepard notify , not here
			//player.start();
		
		}catch(IOException ex){
			log("MediaPlayer ERROR: catch IOException");
		}catch(IllegalArgumentException ex){
			log("MediaPlayer ERROR: catch IllegalArgumentException");
		}catch(SecurityException ex){
			log("MediaPlayer ERROR: catch SecurityException");
		}catch(IllegalStateException ex){
			log("MediaPlayer ERROR: catch IllegalStateException");
		}	
		log("play out");
	}
	
	SurfaceHolder.Callback mSHCallback = new SurfaceHolder.Callback() {
	public void surfaceCreated(SurfaceHolder holder){
		log("surfaceCreated() into,holder.getSurface()="+holder.getSurface());
		
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
            	//log("onTouchEvent() MotionEvent.ACTION_DOWN");
            	break;
        }
        return true;
    }
	 
	//////////////////MediaPlayer event notifylistener////////////////
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
		log("recv buffer update notify");
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
		log("recv error notify ， what=" + what +", extra=" + extra);
		if(extra==MediaPlayer.MEDIA_ERROR_DECODER_FAIL){
			Message message = new Message();
			message.what = 8;
			mHandler.sendMessage(message);
		}else{
			Message message = new Message();
			message.what = 7;
			mHandler.sendMessage(message);
		}
		
		
		return true;
	}

	public boolean onInfo(MediaPlayer mp, int what, int extra) {
		log("recv info notify");
		if(what==MediaPlayer.MEDIA_INFO_BUFFERING_START){
			log("recv info notify MEDIA_INFO_BUFFERING_START");
			Message message = new Message();
			message.what = 3;
			mHandler.sendMessage(message);
		}else if(what==MediaPlayer.MEDIA_INFO_BUFFERING_END){
			log("recv info notify MEDIA_INFO_BUFFERING_END");
			Message message = new Message();
			message.what = 4;
			mHandler.sendMessage(message);
		}

		return true;
	}
	///////////////////////////listener end///////////////////////////
	
	 private void log(String str){
		 if(DEBUG)Log.d(TAG, str);
	 }
}

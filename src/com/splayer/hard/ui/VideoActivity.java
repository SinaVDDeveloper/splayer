package com.splayer.hard.ui;


import java.io.IOException;

import com.sina.sinavideo.coreplayer.splayer.IPlayControl;
import com.sina.sinavideo.coreplayer.splayer.MediaPlayer;
import com.sina.sinavideo.coreplayer.splayer.VideoView;
import com.sina.sinavideo.coreplayer.splayer.VideoViewHard;
import com.splayer.hard.R;
import com.splayer.hard.R.drawable;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;


public class VideoActivity extends Activity implements View.OnClickListener,
													SeekBar.OnSeekBarChangeListener
													{

	private RelativeLayout parentHolder;
	private LinearLayout playRenderHolder;
	private LinearLayout playRenderHolderSmall;
	//public static SurfaceView videoView;
	private SurfaceView playRenderSoft = null;
	private SurfaceView playRenderHard = null;
	private IPlayControl mIPlayControl;
	private String TAG = "*liang*java*";
	private ImageView btnPlay;
	private String fileName =  "http://192.168.9.100/2.mp4";
	//private  boolean playing = false;
	private LinearLayout controlBar; 
	private RelativeLayout titleBar;
	private Handler mHandler = new EventHandler();
	private TextView curTimeView;
	//private String totalTimeView = "00:00";
	private SeekBar seekVideoBar;
	private SeekBar seekVolumeBar;
	private final int MP_MAX_VOLUME =  1; 
	private ImageView btnBack;
	private ImageView btnList;
	private ImageView btnScreen;
	private ImageView btnVolume;
    ///power lock
	//private WakeLock wakeLock = null;
	///config
	private int fast = 0;
	private int drop = 0;
	private int lowres = 0;
	private int skip = 0;
	private int filter = 0;
	private int idct = 0;
	private int debug = MediaPlayer.SPLAYER_TRACE_ERROR;
	private int soft = 0; //hard or soft
	private long buffer =  5000; //5 seconds
	private TextView bufferUpdateView;
	private boolean seeking = false;
	private int mCurPosition = -1;
	
	//switch full
	private boolean fullScreen = true;
	private boolean switching = false;
	private boolean mDestroyPlay = false;
	

	private boolean mHardMode = true;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Log.d(TAG, "VideoActivity onCreate() into");
		
		 //requestWindowFeature(Window.FEATURE_NO_TITLE);
	     //getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
	     
		// Set screen orientation
	     //setRequestedOrientation ( ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		 //PowerManager pm = (PowerManager)this.getSystemService(Context.POWER_SERVICE);
		 //wakeLock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, TAG);
		 //wakeLock.setReferenceCounted(false);
	        
		setContentView(R.layout.video);
		
		parentHolder = (RelativeLayout)findViewById(R.id.parentHolder); 
		
		playRenderHolder = (LinearLayout) findViewById(R.id.playRenderHolder); //(LinearLayout) findViewById(R.id.playRenderHolder); 
		
		playRenderHolderSmall =  (LinearLayout) findViewById(R.id.playRenderHolderSmall);
		//videoView = new SurfaceView(this);
		//playRenderHolder2.addView(videoView);
		
	
		
		controlBar = (LinearLayout) findViewById(R.id.controlBar);
		titleBar = (RelativeLayout) findViewById(R.id.titleBar);
		
		btnPlay =  (ImageView) findViewById(R.id.btnPlay); 
		btnPlay.setOnClickListener(this);
		
		btnBack =  (ImageView) findViewById(R.id.btnBack); 
		btnBack.setOnClickListener(this);
		
		btnList =  (ImageView) findViewById(R.id.btnList); 
		btnList.setOnClickListener(this);
		
		btnScreen =  (ImageView) findViewById(R.id.btnScreen); 
		btnScreen.setOnClickListener(this);
		
		btnVolume =  (ImageView) findViewById(R.id.btnVolume); 
		btnVolume.setOnClickListener(this);
		
		
		curTimeView =  (TextView) findViewById(R.id.curTimeView);
		//totalTimeView =  (TextView) findViewById(R.id.totalTimeView);
		seekVideoBar = (SeekBar) findViewById(R.id.seekPlay);
		seekVideoBar.setOnSeekBarChangeListener(this);
		
		seekVolumeBar = (SeekBar) findViewById(R.id.seekVolume);
		seekVolumeBar.setOnSeekBarChangeListener(this);
	
		bufferUpdateView =  (TextView) findViewById(R.id.bufferUpdateView);
		
		Intent intent= this.getIntent();    
        fileName = intent.getStringExtra("fileName");  
        fast = intent.getIntExtra("fast",0);
        drop = intent.getIntExtra("drop",0);
        lowres = intent.getIntExtra("lowres",0);
        skip = intent.getIntExtra("skip",0);
        idct = intent.getIntExtra("idct",0);
        filter = intent.getIntExtra("filter",0);
        debug = intent.getIntExtra("debug",0);
        if(debug==1){
        	debug = 49; //16; // 49 DEBUG 24 WARNNING 16 ERROR
        }else{
        	debug = 16;
        }
        soft = intent.getIntExtra("soft",0);
        if(soft == 1){
        	mHardMode = false;
        }
//        buffer = intent.getIntExtra("buffer",-1);
//        if(-1!=buffer){
//        	buffer = buffer * (1*1024*1024);
//        }
        
        //if(!playing){
        Log.d(TAG, "VideoActivity will play: [" + fileName + "], mHardMode=" + mHardMode);
        
       
//        if(mHardMode==true){
//        	playRender = new HardSurfaceView(this,mHandler);
//        	playRender.getHolder().addCallback(this);
//        	playRenderHolder.addView(playRender);
//        }else{
//        	Play(fileName);
//        }
        
      if(mHardMode==true){
    	  playRenderHard = new VideoViewHard(this,mHandler);
    	  mIPlayControl = (IPlayControl)playRenderHard;
    	  mIPlayControl.setPlayURL(fileName);
//          mIPlayControl.config("fast", fast);
//          mIPlayControl.config("drop", drop);
//          mIPlayControl.config("lowres", lowres);
//          mIPlayControl.config("skip", skip);
//          mIPlayControl.config("idct", idct);
//          mIPlayControl.config("filter", filter);
          mIPlayControl.config("debug", debug);
//          mIPlayControl.config("soft", soft);
          mIPlayControl.config("buffer", (int)buffer);
          playRenderHolder.addView(playRenderHard);
          
      }else{
    	  playRenderSoft = new VideoView(this,mHandler);
    	  mIPlayControl = (IPlayControl)playRenderSoft;
    	  mIPlayControl.setPlayURL(fileName);
          mIPlayControl.config("fast", fast);
          mIPlayControl.config("drop", drop);
          mIPlayControl.config("lowres", lowres);
          mIPlayControl.config("skip", skip);
          mIPlayControl.config("idct", idct);
          mIPlayControl.config("filter", filter);
          mIPlayControl.config("debug", debug);
          mIPlayControl.config("soft", soft);
          mIPlayControl.config("buffer", (int)buffer);
          playRenderHolder.addView(playRenderSoft);
      }
          	
	  Log.d(TAG, "VideoActivity onCreate() out");
	}

	  @Override
	 public void onConfigurationChanged(Configuration newConfig) {
		  super.onConfigurationChanged(newConfig);
		  //Log.d(TAG, "VideoActivity onConfigurationChanged() into");
		  
		  if(newConfig.orientation== Configuration.ORIENTATION_LANDSCAPE) {
			  //Log.d(TAG, "VideoActivity onConfigurationChanged()  Configuration.ORIENTATION_LANDSCAPE");
		  }
		  else if(newConfig.orientation== Configuration.ORIENTATION_PORTRAIT){
			  //Log.d(TAG, "VideoActivity onConfigurationChanged()  Configuration.ORIENTATION_PORTRAIT");
		  }else{
			  //Log.d(TAG, "VideoActivity onConfigurationChanged()  newConfig.orientation="+newConfig.orientation);
		  }
		  //setContentView(R.layout.video);
	  }
	
	
	
	private void switchFull(){
		if(switching){
			return;
		}
		switching = true;
		if(fullScreen){
			playRenderHolder.removeAllViews();
			playRenderHolder.setVisibility(View.INVISIBLE);
			playRenderHolderSmall.setVisibility(View.VISIBLE);
			 if(mHardMode==true){
				 playRenderHolderSmall.addView(playRenderHard);
			 }else{
				 playRenderHolderSmall.addView(playRenderSoft);
			 }
		}else{
			playRenderHolderSmall.removeAllViews();
			playRenderHolderSmall.setVisibility(View.INVISIBLE);
			playRenderHolder.setVisibility(View.VISIBLE);
			 if(mHardMode==true){
				 playRenderHolder.addView(playRenderHard);
			 }else{
				 playRenderHolder.addView(playRenderSoft);
			 }
		}
		fullScreen = !fullScreen;
		switching = false;
	}
	
	public void hardSoftSwitch(){
		if(switching){
			return;
		}
		switching = true;
		
		 try{
			 mCurPosition = mIPlayControl.getCurrentPosition();
			 Log.d(TAG, "hardSoftSwitch() mCurPosition=" + mCurPosition);
		 }catch(IllegalStateException ex){
		 }
		
		 playRenderHolder.removeAllViews();
		 mHardMode = !mHardMode;
		 
		
		 
		 
		 if(mHardMode==true){
	    	  playRenderHard = new VideoViewHard(this,mHandler);
	    	  mIPlayControl = (IPlayControl)playRenderHard;
	    	  mIPlayControl.setPlayURL(fileName);
	          //mIPlayControl.config("fast", fast);
	          //mIPlayControl.config("drop", drop);
	          //mIPlayControl.config("lowres", lowres);
	          //mIPlayControl.config("skip", skip);
	          //mIPlayControl.config("idct", idct);
	          //mIPlayControl.config("filter", filter);
	          mIPlayControl.config("debug", debug);
	          //mIPlayControl.config("soft", soft);
	          mIPlayControl.config("buffer", (int)buffer);
	          
	          playRenderHolder.addView(playRenderHard);
	          
	      }else{
	    	  playRenderSoft = new VideoView(this,mHandler);
	    	  mIPlayControl = (IPlayControl)playRenderSoft;
	    	  mIPlayControl.setPlayURL(fileName);
	          mIPlayControl.config("fast", fast);
	          mIPlayControl.config("drop", drop);
	          mIPlayControl.config("lowres", lowres);
	          mIPlayControl.config("skip", skip);
	          mIPlayControl.config("idct", idct);
	          mIPlayControl.config("filter", filter);
	          mIPlayControl.config("debug", debug);
	          mIPlayControl.config("soft", soft);
	          mIPlayControl.config("buffer", (int)buffer);
	          
	          playRenderHolder.addView(playRenderSoft);
	      }
		 
		 switching = false;
	}
	
	private void showVolume(){
		if(seekVolumeBar.getVisibility()!=ImageView.VISIBLE){
			seekVolumeBar.setVisibility(ImageView.VISIBLE);
		}else{
			seekVolumeBar.setVisibility(ImageView.INVISIBLE);
		}
	}
	 public void onClick(View arg0) {
		 	//Log.d(TAG, "VideoActivity onClick() into");
	        switch (arg0.getId()) {
	        case R.id.btnPlay:
	        	boolean playing = false;
	        	if(mIPlayControl!=null)
	        		playing = mIPlayControl.isPlaying();
	        		        	
	        	if(playing){
	        		if(mIPlayControl!=null)
	        			mIPlayControl.pause();
	        		btnPlay.setImageDrawable(getResources().getDrawable(R.drawable.play));
	        	}else{
	        		if(mIPlayControl!=null)
	        			mIPlayControl.start();
	        		btnPlay.setImageDrawable(getResources().getDrawable(R.drawable.stop));
	        	}
	        	break;
	        case R.id.btnBack:
	        	mDestroyPlay = true;
	        	mHandler.removeCallbacks(showTimeRun);
	        	if(mIPlayControl!=null)
	        		mIPlayControl.release();
	        	//wakeLock.release(); // screen stay on during the call
		        finish();
		        System.exit(0);
	        	break;
	        case R.id.btnList:
	        	hardSoftSwitch();
	        	break;
	        case R.id.btnScreen:
	        	switchFull();
	        	break;
	        case R.id.btnVolume:
	        	showVolume();
	        	break;
	        }
	 }
	 
	
	 @Override
	 public boolean onKeyDown(int keyCode, KeyEvent event) {
		  //Log.d(TAG, "VideoActivity onKeyDown() into");
	      if (keyCode == KeyEvent.KEYCODE_BACK) {
	    	    mDestroyPlay = true;
	    	    mHandler.removeCallbacks(showTimeRun);
	    	    if(mIPlayControl!=null){
	    	    	mIPlayControl.release();
	    	    }
	    	    //wakeLock.release(); // screen stay on during the call
	            finish();
	            //System.exit(0);
	            return true;
	      }
	      return super.onKeyDown(keyCode, event);
	 }
	 
	private boolean showControlBarState = true;
	private void ShowControlBar(){
		log("ShowControlBar() into,showControlBarState="+showControlBarState);
      	//controlBar.setVisibility((showState==true)?View.INVISIBLE:View.VISIBLE);
      	//titleBar.setVisibility((showState==true)?View.INVISIBLE:View.VISIBLE);
      	//btnPlay.setVisibility((showState==true)?View.INVISIBLE:View.VISIBLE);	
		if(showControlBarState==true){
			parentHolder.removeView(controlBar);
			parentHolder.removeView(titleBar);
		}else{
			parentHolder.addView(controlBar);
			parentHolder.addView(titleBar);
		}
		
		showControlBarState = !showControlBarState;
	}
	
	
	private void ShowBuffer(long updateFrame){		
		String bfs = String.format("%d", updateFrame) + "%";
		bufferUpdateView.setText(bfs);
	}
	
	private void showBufferView(boolean show){
		if(show){
			parentHolder.removeView(bufferUpdateView);
			parentHolder.addView(bufferUpdateView);
		}else{
			parentHolder.removeView(bufferUpdateView);
		}
	}
	
	private void freshTime(){
		log("freshTime() into");
		seeking = false;
		long hours, mins, secs;
		int updateTime = 0;
		try{
			updateTime = mIPlayControl.getCurrentPosition();
		}catch(IllegalStateException ex){
			return;
		}
		updateTime = updateTime /1000;
 		String strTime;
 		secs = updateTime;
     	mins = secs / 60;
     	secs %= 60;
     	hours = mins / 60;
     	mins %= 60;
     	if(hours>0){
     		strTime = String.format("%02d:%02d:%02d", hours,mins,secs); 
     	}else{
     		strTime = String.format("%02d:%02d",mins,secs); 
     	}
     	curTimeView.setText(strTime + "/" + totalTimeStr);
     	seekVideoBar.setProgress((int)updateTime);
	}
	private String totalTimeStr = null;
	private Runnable showTimeRun = new Runnable() { 
		
        public void run() {   
        	long hours, mins, secs;
             
            if(totalTimeStr==null){
            	 int totalTime = mIPlayControl.getDuration();
            	 log("getDuration="+totalTime);
            	 //totalTime = totalTime + 5000;
			     secs = totalTime / 1000;
			     mins = secs / 60;
			     secs %= 60;
			     hours = mins / 60;
			     mins %= 60;
			     if(hours>0){
			    	 totalTimeStr = String.format("%02d:%02d:%02d", hours,mins,secs); 
				 }else{
					 totalTimeStr = String.format("%02d:%02d",mins,secs); 
				 }
		         seekVideoBar.setMax((int)(totalTime / 1000) - 1);
		         seekVideoBar.setProgress(0);
		         
		     	 seekVolumeBar.setMax(MP_MAX_VOLUME * 1000);
				 seekVolumeBar.setProgress(MP_MAX_VOLUME *1000);
				 if(mIPlayControl!=null){
					 mIPlayControl.setVolume(1);
				 }
				
            }
            
   		 	if(seeking==false){
   		 	    int updateTime = 0;
	   		 	try{
	   				updateTime = mIPlayControl.getCurrentPosition();
	   			}catch(IllegalStateException ex){
	   				return;
	   			}
   		 	    updateTime = updateTime /1000;
   		 		String strTime;
   		 		secs = updateTime;
   		     	mins = secs / 60;
   		     	secs %= 60;
   		     	hours = mins / 60;
   		     	mins %= 60;
   		     	if(hours>0){
   		     		strTime = String.format("%02d:%02d:%02d", hours,mins,secs); 
   		     	}else{
   		     		strTime = String.format("%02d:%02d",mins,secs); 
   		     	}
   		     	curTimeView.setText(strTime + "/" + totalTimeStr);
   		     	seekVideoBar.setProgress((int)updateTime);
   		 	}   
   		    mHandler.postDelayed(this, 1000);
        }  
    };  
    
	private class EventHandler extends Handler {
		@Override
		public void handleMessage(Message msg) {
			switch (msg.what) {
			case 0:
				//log("handleMessage() recv notify");
				ShowControlBar();
				break;
			case 1:		
				Log.d(TAG,"before seekto " +mCurPosition );
				if(mCurPosition>0){
					Log.d(TAG,"seekto " +mCurPosition );
					mIPlayControl.seekTo(mCurPosition);
					mCurPosition = -1;
				}
				mIPlayControl.start();
				mHandler.post(showTimeRun);  
				//showTime((int)msg.getData().getLong("time")) ;
				break;
			case 2:
				//log("handleMessage() recv notify");
				ShowBuffer(msg.getData().getLong("percent"));
				break;
			case 3:
				showBufferView(true);
				break;
			case 4:
				showBufferView(false);
//				 if(mHardMode==true){
//					 Log.d(TAG, "SurfaceView.invalidate()");
//			    	  playRenderHard.invalidate();
//				 }
//				 else{
//					 Log.d(TAG, "SurfaceView.invalidate()");
//					 playRenderSoft.invalidate();
//				 }
				break;
			case 5:
				freshTime();
				break;
			case 6:
				boolean playing = mIPlayControl.isPlaying();
	        	btnPlay.setImageDrawable(getResources().getDrawable(playing==true? R.drawable.stop:R.drawable.play));	        	
				break;
			case 7:
				Toast.makeText(VideoActivity.this, "播放视频失败", Toast.LENGTH_SHORT).show(); 
				mHandler.removeCallbacks(showTimeRun);
		    	if(mIPlayControl!=null){
		    		mIPlayControl.release();
		    	}
				break;
			case 8:
				Toast.makeText(VideoActivity.this, "硬解码失败", Toast.LENGTH_SHORT).show(); 
				mHandler.removeCallbacks(showTimeRun);
		    	if(mIPlayControl!=null){
		    		mIPlayControl.release();
		    	}
				break;
			default:
				break;
			}

		}
	}
	
	///////////////////////////seek bar///////////////////////////
	public void	 onProgressChanged(SeekBar seekBar, int progress, boolean fromUser){
	}

	public void	 onStartTrackingTouch(SeekBar seekBar){
		seeking = true;
		
		if( seekBar.getId() == R.id.seekPlay ){
			int pos = seekVideoBar.getProgress();
			log("video seek, seekbar start, pos="+pos);
		}else if( seekBar.getId() == R.id.seekVolume ){
			int pos = seekVolumeBar.getProgress();
			log("volume seek seekbar start, pos="+pos);
		}
	}
	
	public void	 onStopTrackingTouch(SeekBar seekBar){
		if( seekBar.getId() == R.id.seekPlay ){
			int pos = seekVideoBar.getProgress();
			log("video seek, seekbar stop, pos="+pos);
			mIPlayControl.seekTo(pos*1000);
			seekVideoBar.setProgress(pos);
			this.showBufferView(true);
			this.ShowBuffer(0);
		}else if( seekBar.getId() == R.id.seekVolume ){
			int pos = seekVolumeBar.getProgress();
			log("volume seek, seekbar stop, pos="+pos);
			float vol = (float)pos / 1000 ;
			mIPlayControl.setVolume( vol );
			seekVolumeBar.setProgress(pos);
		}
		
	}
	///////////////////////////seek bar end///////////////////////////
	

	 
	private boolean DEBUG_ENABLE = false;
	private void log(String str){
		if(DEBUG_ENABLE){
			Log.d(TAG, "VideoActivity "+ str);
		}
	}
}

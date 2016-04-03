package  com.splayer.hard.ui;


import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import com.sina.sinavideo.coreplayer.splayer.IPlayControl;
import com.sina.sinavideo.coreplayer.splayer.MediaPlayer;
import com.sina.sinavideo.coreplayer.splayer.VideoViewHard;
import com.splayer.hard.R;
import com.splayer.hard.SinaApplication;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;

import android.os.Bundle;

import android.os.Message;

import android.os.Handler;
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
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Toast;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.ToggleButton;



public class MultiActivity extends Activity implements View.OnClickListener{

	private String TAG = "*liang*java*";

	private Button playBtn;
	private Button playBtn2;
	private Button playBtn3;
	private Button playBtn4;
	private Button playBtn5;
	private LinearLayout playRenderHolder;//private LinearLayout playRenderHolder;
	//private MediaPlayer player = null;
	private SurfaceView playRender = null;
	private Handler mHandler = new EventHandler();
	private IPlayControl mIPlayControl = null;
	///power lock
	//private WakeLock wakeLock = null;

	private String httpFile =  "http://119.188.27.139/data5/video07/2014/02/21/1712321-102-0728.mp4";//"/sdcard/2.mp4";
	private String httpFile2 = "http://111.161.7.201/data3/video08/2014/05/04/1879271-102-008-1717.mp4";//"/sdcard/1.mp4";
	private String httpFile3 = "http://111.161.7.200/video08/2014/04/30/1871982-102-024-0918.mp4";//"/sdcard/3.mp4";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Log.d(TAG, "MultiActivity onCreate() into");
		
		 //requestWindowFeature(Window.FEATURE_NO_TITLE);
	     //getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
	     // Set screen orientation
	     //setRequestedOrientation (/*ActivityInfo.SCREEN_ORIENTATION_PORTRAIT*/ ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

		setContentView(R.layout.multi);
		
		//local

		playBtn =  (Button) findViewById(R.id.playBtn); 
		playBtn.setOnClickListener(this);
		
		playBtn2 =  (Button) findViewById(R.id.playBtn2); 
		playBtn2.setOnClickListener(this);
		
		playBtn3 =  (Button) findViewById(R.id.playBtn3); 
		playBtn3.setOnClickListener(this);
		
		playBtn4 =  (Button) findViewById(R.id.playBtn4); 
		playBtn4.setOnClickListener(this);
		
		playBtn5 =  (Button) findViewById(R.id.playBtn5); 
		playBtn5.setOnClickListener(this);
		
		playRenderHolder = (LinearLayout) findViewById(R.id.playRenderHolder);
		
		 //PowerManager pm = (PowerManager)this.getSystemService(Context.POWER_SERVICE);
		 //wakeLock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, TAG);
		 //wakeLock.setReferenceCounted(false);
		 
		 //wakeLock.acquire(); // screen stay on during the call
		
		Log.d(TAG, "MultiActivity onCreate() out");
	}

	@Override
	protected  void onDestroy(){
		Log.d(TAG, "MultiActivity onDestroy() into");
		super.onDestroy();
		
	}
	
	@Override
	protected  void onPause(){
		Log.d(TAG, "MultiActivity onPause() into");
		super.onPause();
		
	}
	
	
	@Override
	protected  void onResume(){
		Log.d(TAG, "MultiActivity onResume() into");
		super.onResume();
		
	}
	
//	public void onPrepared(MediaPlayer mp) {
//		log("recv prepared  notify");
//		player.start();
//		
//	}

	private void Play(String path){
		try{
//			if(mIPlayControl!=null){
//				mIPlayControl.release();
//			}
			if(playRender!=null)playRender.setVisibility(SurfaceView.GONE);
			playRenderHolder.removeAllViews();
			
			playRender = new VideoViewHard(this,mHandler);
			playRender.setVisibility(SurfaceView.VISIBLE);
			mIPlayControl = (IPlayControl)playRender;
		
			mIPlayControl.setPlayURL(path);
       
			//playRenderHolder.removeAllViews();
			playRenderHolder.addView(playRender);
	
		}catch(IllegalArgumentException ex){
			log("catch IllegalArgumentException");
		}catch(SecurityException ex){
			log("catch SecurityException");
		}catch(IllegalStateException ex){
			log("catch IllegalStateException");
		}catch(Exception ex){
			log("catch IOException");
		}	
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
	 public void onClick(View arg0) {
	        switch (arg0.getId()) {
	            case R.id.playBtn:
	            	httpFile =  SinaApplication.getDataDir(this) +"video/"  + "t1.mp4";
	            	Play(httpFile);
	            	break;
	            case R.id.playBtn2:
	            	httpFile2 =  SinaApplication.getDataDir(this) +"video/"  + "t3.mp4";
	             	Play(httpFile2); 	
	            	break;
	            case R.id.playBtn3:
	            	httpFile3 =  SinaApplication.getDataDir(this) +"video/"  + "t10.mp4";
	             	Play(httpFile3); 	
	            	break;
	            case R.id.playBtn4:
	            	String httpFile4 =  SinaApplication.getDataDir(this) +"video/"  + "t2.mp4";
	             	Play(httpFile4); 	
	            	break;
	            case R.id.playBtn5:
	            	String httpFile5 =  SinaApplication.getDataDir(this) +"video/"  + "t4.mp4";
	             	Play(httpFile5); 	
	            	break;
	        }
	 }
	 
	  @Override
	    public boolean onKeyDown(int keyCode, KeyEvent event) {
	        if (keyCode == KeyEvent.KEYCODE_BACK) {
	        	//wakeLock.release(); 
	        	if(mIPlayControl!=null)
	        		mIPlayControl.release();
	            finish();
	     
	            System.exit(0);
	            return true;
	        }
	        return super.onKeyDown(keyCode, event);
	    }
	  
	  
	  private class EventHandler extends Handler {
			@Override
			public void handleMessage(Message msg) {
				switch (msg.what) {
				case 0:
					//log("handleMessage() recv notify");
					//ShowControlBar();
					break;
				case 1:		
					log("handleMessage() recv prepared notify");
					//mIPlayControl.seekTo(4000);
					mIPlayControl.start();
					//mHandler.post(showTimeRun);  
					//showTime((int)msg.getData().getLong("time")) ;
					break;
				case 2:
					//log("handleMessage() recv notify");
					//ShowBuffer(msg.getData().getLong("percent"));
					break;
				case 3:
					//showBufferView(true);
					break;
				case 4:
					//showBufferView(false);
//					 if(mHardMode==true){
//						 Log.d(TAG, "SurfaceView.invalidate()");
//				    	  playRenderHard.invalidate();
//					 }
//					 else{
//						 Log.d(TAG, "SurfaceView.invalidate()");
//						 playRenderSoft.invalidate();
//					 }
					break;
				case 5:
					//freshTime();
					break;
				case 6:
					//boolean playing = mIPlayControl.isPlaying();
		        	//btnPlay.setImageDrawable(getResources().getDrawable(playing==true? R.drawable.stop:R.drawable.play));	        	
					break;
				case 7:
					Toast.makeText(MultiActivity.this, "播放器解码失败", Toast.LENGTH_SHORT).show(); 
					//mHandler.removeCallbacks(showTimeRun);
			    	if(mIPlayControl!=null){
			    		mIPlayControl.release();
			    	}
					break;
				default:
					break;
				}

			}
		}
	  
	  private boolean DEBUG_ENABLE = false;
		private void log(String str){
			if(DEBUG_ENABLE){
				Log.d(TAG, "VideoActivity "+ str);
			}
		}
}

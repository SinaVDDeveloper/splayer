package com.splayer.hard.ui;


import com.sina.sinavideo.coreplayer.splayer.IPlayControl;
import com.sina.sinavideo.coreplayer.splayer.VideoViewHard;
import com.splayer.hard.R;
import com.splayer.hard.SinaApplication;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;


import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.util.Log;
import android.view.KeyEvent;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Toast;
import android.widget.RelativeLayout.LayoutParams;

public class CloudTestActivity extends Activity implements View.OnClickListener {

	private RelativeLayout parentHolder;
	private LinearLayout playRenderHolder;
	private SurfaceView playRenderHard = null;
	private String TAG = "CloudTestActivity";
	private String fileName = "http://192.168.9.100/2.mp4";
	private Handler mHandler = new EventHandler();
	// /power lock
	// private WakeLock wakeLock = null;
	// /config
	private int fast = 0;
	private int drop = 0;
	private int lowres = 0;
	private int skip = 0;
	private int filter = 0;
	private int idct = 0;
	private int debug = 16;
	private int soft = 0; // hard or soft
	private long buffer = 100;

	// switch full
	private boolean mHardMode = true;

	private IPlayControl mIPlayControl = null;
	private boolean DEBUG_ENABLE = true;
	private Button btnPlay = null;
	private Button btnFull = null;
	private boolean playing = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		Log.d(TAG, "onCreate() into");

		// requestWindowFeature(Window.FEATURE_NO_TITLE);
		// getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
		// WindowManager.LayoutParams.FLAG_FULLSCREEN);
		// Set screen orientation
		// setRequestedOrientation ( ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		// PowerManager pm =
		// (PowerManager)this.getSystemService(Context.POWER_SERVICE);
		// wakeLock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, TAG);
		// wakeLock.setReferenceCounted(false);

		setContentView(R.layout.cloud);

		parentHolder = (RelativeLayout) findViewById(R.id.parentHolder);

		playRenderHolder = (LinearLayout) findViewById(R.id.playRenderHolder);
																				
		btnPlay = (Button) findViewById(R.id.btnPlay);
		btnPlay.setOnClickListener(this);
		
		btnFull = (Button) findViewById(R.id.btnFull);
		btnFull.setOnClickListener(this);

		Log.d(TAG, "onCreate() out");
	}

	private void play(){
		if(playing==true){
			return;
		}
		playing = true;
		
		fileName =  SinaApplication.getDataDir(this) +"video/"  + "t1.mp4";
		Log.d(TAG, "play() into," + fileName);
		if (mHardMode == true) {
			playRenderHard = new VideoViewHard(this, mHandler);
			mIPlayControl = (IPlayControl) playRenderHard;
			mIPlayControl.setPlayURL(fileName);
			mIPlayControl.config("fast", fast);
			mIPlayControl.config("drop", drop);
			mIPlayControl.config("lowres", lowres);
			mIPlayControl.config("skip", skip);
			mIPlayControl.config("idct", idct);
			mIPlayControl.config("filter", filter);
			mIPlayControl.config("debug", debug);
			mIPlayControl.config("soft", soft);
			mIPlayControl.config("buffer", (int) buffer);
			playRenderHolder.addView(playRenderHard);
		}
		
	}
	
	private void full(){
		if( this.getRequestedOrientation() == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE ){
			Log.d(TAG, "full() make a menualPortraitClick");
			menualPortraitClick();
		}else{
			Log.d(TAG, "full() make a menualLandscapeClick");
			menualLandscapeClick();
			
		}
	}
	
	private LayoutParams oldLP = null;
	public void menualLandscapeClick() {
		Log.d(TAG, "menualLandscapeClick() into");
		if (android.os.Build.VERSION.SDK_INT >= 9){
			this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
		
		} else {
			this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
			
		}
		LayoutParams lp = new LayoutParams(LayoutParams.FILL_PARENT,LayoutParams.FILL_PARENT);
		oldLP = (LayoutParams)playRenderHolder.getLayoutParams();
		playRenderHolder.setLayoutParams(lp); 
		Log.d(TAG, "menualLandscapeClick() out");
	}

	public void menualPortraitClick() {
		Log.d(TAG, "menualPortraitClick() into");
		if (android.os.Build.VERSION.SDK_INT >= 9){
			this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT);
		} else {
			this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		}
		if(oldLP!=null){
			playRenderHolder.setLayoutParams(oldLP);
		}else{
			LayoutParams lp = new LayoutParams(LayoutParams.FILL_PARENT,LayoutParams.FILL_PARENT);
			oldLP = (LayoutParams)playRenderHolder.getLayoutParams();
			playRenderHolder.setLayoutParams(lp); 
		}
		 
		Log.d(TAG, "menualPortraitClick() out");
	}
	    
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
	  super.onConfigurationChanged(newConfig);
	  Log.d(TAG, "onConfigurationChanged() into");
  
	  if(newConfig.orientation== Configuration.ORIENTATION_LANDSCAPE) {
		  Log.d(TAG, "onConfigurationChanged()  Configuration.ORIENTATION_LANDSCAPE");
	  }
	  else if(newConfig.orientation== Configuration.ORIENTATION_PORTRAIT){
		  Log.d(TAG, "onConfigurationChanged()  Configuration.ORIENTATION_PORTRAIT");
	
	  }else{
		  Log.d(TAG, "onConfigurationChanged()  newConfig.orientation="+newConfig.orientation);
	  }
	}  

	public void onClick(View arg0) {
		// Log.d(TAG, "VideoActivity onClick() into");
		switch (arg0.getId()) {
		case R.id.btnPlay:
			play();
			break;
		case R.id.btnFull:
			full();
			break;
		}
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		Log.d(TAG, "onKeyDown() into, keyCode=" + keyCode);
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			/*
			if (mIPlayControl != null) {
				mIPlayControl.release();
			}
			finish();
			// System.exit(0);
			 * 
			 */
			return true;
		}
		return super.onKeyDown(keyCode, event);
	}

	private class EventHandler extends Handler {
		@Override
		public void handleMessage(Message msg) {
			switch (msg.what) {
			case 0:
				// log("handleMessage() recv notify");
				// ShowControlBar();
				break;
			case 1:
				// mIPlayControl.seekTo(5000);
				if(mIPlayControl!=null)
					mIPlayControl.start();
				// mHandler.post(showTimeRun);
				// showTime((int)msg.getData().getLong("time")) ;
				break;
			case 2:
				// log("handleMessage() recv notify");
				// ShowBuffer(msg.getData().getLong("percent"));
				break;
			case 3:
				// showBufferView(true);
				break;
			case 4:
				// showBufferView(false);
				// if(mHardMode==true){
				// Log.d(TAG, "SurfaceView.invalidate()");
				// playRenderHard.invalidate();
				// }
				// else{
				// Log.d(TAG, "SurfaceView.invalidate()");
				// playRenderSoft.invalidate();
				// }
				break;
			case 5:
				// freshTime();
				break;
			case 6:
				boolean playing = mIPlayControl.isPlaying();
				// btnPlay.setImageDrawable(getResources().getDrawable(playing==true?
				// R.drawable.stop:R.drawable.play));
				if(playing==true){
					
				}else{
					Toast.makeText(CloudTestActivity.this, "播放结束",
							Toast.LENGTH_SHORT).show();
					Log.d(TAG, "播放结束");
				}
				break;
			case 7:
				Toast.makeText(CloudTestActivity.this, "播放器解码失败",
						Toast.LENGTH_SHORT).show();
				Log.e(TAG, "播放器解码失败");
				// mHandler.removeCallbacks(showTimeRun);
				if (mIPlayControl != null) {
					mIPlayControl.release();
				}
				break;
			default:
				break;
			}

		}
	}

	private void log(String str) {
		if (DEBUG_ENABLE) {
			Log.d(TAG, "" + str);
		}
	}
}

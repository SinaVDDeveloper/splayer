package  com.splayer.hard.ui;


import com.splayer.hard.R;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;

import android.net.Uri;
import android.os.Bundle;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.Menu;
import android.view.Window;
import android.view.WindowManager;
import android.widget.MediaController;
import android.widget.VideoView;

public class PlayerActivity extends Activity {

	private VideoView vvPlay;
	
	private String videoAddr =  "http://124.192.140.140/mtv.v.iask.com/vod/20141200131_hd.m3u8";
	//private String videoAddr =  "http://10.0.0.9/20140102-164729-348-6074.mp4"; //"http://10.0.0.9/VIDEO0011.mp4"; 
	
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);
		// Set screen orientation
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE  /*  SCREEN_ORIENTATION_PORTRAIT*/);
		
		setContentView(R.layout.m3u8);
		
		vvPlay = (VideoView)findViewById(R.id.vvPlay);
		
		Uri uri = Uri.parse(videoAddr);
		vvPlay.setMediaController(new MediaController(this));
		vvPlay.setVideoURI(uri);
		vvPlay.start();
		vvPlay.requestFocus();
	}
}

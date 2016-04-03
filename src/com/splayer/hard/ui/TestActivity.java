package  com.splayer.hard.ui;

import com.splayer.hard.R;
import com.splayer.hard.R.layout;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

public class TestActivity extends Activity{
	private String TAG = "*liang*java*";
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		Log.d(TAG, "TestActivity onCreate() into");
		
		setContentView(R.layout.test);
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		
		Log.d(TAG, "TestActivity onDestroy() into");
		
	
	}
	
	@Override
	protected void onPause() {
		super.onPause();
		
		Log.d(TAG, "TestActivity onPause() into");
		
	
	}
	
	@Override
	protected void onResume() {
		super.onResume();
		
		Log.d(TAG, "TestActivity onResume() into");
		
	
	}
	
	 @Override
	    public boolean onKeyDown(int keyCode, KeyEvent event) {
		 Log.d(TAG, "TestActivity onKeyDown() into");
		 
	        if (keyCode == KeyEvent.KEYCODE_BACK) {
	            finish();
	            System.exit(0);
	            return true;
	        }
	        return super.onKeyDown(keyCode, event);
	    }
	 
	 @Override
	  public void onConfigurationChanged(Configuration newConfig) {
		  super.onConfigurationChanged(newConfig);
		  Log.d(TAG, "TestActivity onConfigurationChanged() into");
		  
		  if(newConfig.orientation== Configuration.ORIENTATION_LANDSCAPE) {
			  Log.d(TAG, "TestActivity onConfigurationChanged()  Configuration.ORIENTATION_LANDSCAPE");
		  }
		  else if(newConfig.orientation== Configuration.ORIENTATION_PORTRAIT){
			  Log.d(TAG, "TestActivity onConfigurationChanged()  Configuration.ORIENTATION_PORTRAIT");
		  }else{
			  Log.d(TAG, "TestActivity onConfigurationChanged()  newConfig.orientation="+newConfig.orientation);
		  }
		  
	  }
}

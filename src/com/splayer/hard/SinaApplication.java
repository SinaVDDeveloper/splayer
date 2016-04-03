package com.splayer.hard;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.util.Log;


public class SinaApplication extends Application {

	private final static String TAG = "SinaApplication";
	
	@Override
	public void onCreate() {
		super.onCreate();
		
		Thread.setDefaultUncaughtExceptionHandler(new SPlayerException());

		extract(this.getApplicationContext());
	}
	
	public static String getDataDir(Context ctx) {
		ApplicationInfo ai = ctx.getApplicationInfo();
		return "/data/data/" + ai.packageName + "/";
	}
	
	private void extract(Context ctx){
		Log.d(TAG,"extract() begin");
		File file = new File( getDataDir(ctx) + "video" );
		if(!file.exists()){
			file.mkdir();
		}
		String[] fs = {"t3.mp4","t4.mp4"};  // {"t1.mp4","t2.mp4","t3.mp4","t4.mp4","t10.mp4"}; 
		for(String f : fs){
			copyAssetsFile( ctx, f,  getDataDir(ctx) + "video/" + f);
		}
		Log.d(TAG,"extract() end");
	}
	
	private static String copyAssetsFile(Context ctx, String srcFname, String destName) {
	    byte[] buffer = new byte[1024];
	    InputStream is = null;
	    BufferedInputStream bis = null;
	    FileOutputStream fos = null;

	    try {
	      try {
	        File f = new File(destName);
	        if (f.exists())
	          return destName;
	      
	         f.createNewFile();
	      } catch (Exception fe) {
	        Log.e(TAG,"loadLib error, "+fe.toString());
	      }

	      is = ctx.getClass().getResourceAsStream("/assets/" + srcFname);//ctx.getResources().openRawResource(rawID);
	      bis = new BufferedInputStream(is);
	      fos = new FileOutputStream(destName);
	      while (bis.read(buffer) != -1) {
	        fos.write(buffer);
	      }
	    } catch (Exception e) {
	      Log.e(TAG,"loadLib error, "+e.toString());
	      return null;
	    } finally {
	    	try {
	    		 if(null!=fos)fos.close();
	    		 if(null!=bis)bis.close();
			     if(null!=is)is.close();
			} catch (Throwable t) {
				Log.w(TAG, "fail to close", t);
			}
	    }

	    return destName;
	  }
}

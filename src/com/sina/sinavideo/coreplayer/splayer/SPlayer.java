package com.sina.sinavideo.coreplayer.splayer;

import java.io.File;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.util.Log;

public class SPlayer {
	private static Context mCtx = null;
	public static void initContext(Context ctx){
		mCtx = ctx;
	}
	public static String getLogDir(){
		if(mCtx == null){
			String defaulePath = "/sdcard/";
			return defaulePath;
		}
		return getLogDir(mCtx);
	}
	public static String getLogDir(Context ctx){
		String logPath = getDataDir(ctx) + "log/";
		try{
			File f = new File(logPath);
			if(!f.exists()){
				f.mkdir();
			}else{
				if(!f.isDirectory() && f.isFile()){
					Log.e("SPlayer", "log exit but is file");
					f.delete();
					f.mkdir();
				}
			}
		}catch(Exception ex){
			Log.e("SPlayer", "create log dir fail");
			ex.printStackTrace();
		}
		return logPath;
	}
	
	public static String getDataDir(Context ctx) {
		ApplicationInfo ai = ctx.getApplicationInfo();
		if (ai.dataDir != null)
			return fixLastSlash(ai.dataDir);
		else
			return "/data/data/" + ai.packageName + "/";
	}

	public static String fixLastSlash(String str) {
		String res = str == null ? "/" : str.trim() + "/";
		if (res.length() > 2 && res.charAt(res.length() - 2) == '/')
			res = res.substring(0, res.length() - 1);
		return res;
	}
}

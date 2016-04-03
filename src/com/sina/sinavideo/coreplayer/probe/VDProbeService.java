package com.sina.sinavideo.coreplayer.probe;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class VDProbeService extends Service {
	private static final String TAG = "com.sina.sinavideo.coreplayer.probe.VDProbeService";
	// 数据部分
	public VDProbeServiceData mProbeData = new VDProbeServiceData();
	private final IProbeRemoteService.Stub mBinder = new IProbeRemoteService.Stub() {
	};

	@Override
	public IBinder onBind(Intent arg0) {
		// TODO Auto-generated method stub
		return mBinder;
	}

	@Override
	public void onCreate() {
		// TODO Auto-generated method stub
		super.onCreate();
	}

	@Override
	public void onDestroy() {
		// TODO Auto-generated method stub
		super.onDestroy();
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		// TODO Auto-generated method stub
		return super.onStartCommand(intent, flags, startId);
	}

	@Override
	public boolean onUnbind(Intent intent) {
		// TODO Auto-generated method stub
		return super.onUnbind(intent);
	}

}

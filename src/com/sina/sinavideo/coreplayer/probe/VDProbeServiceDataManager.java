package com.sina.sinavideo.coreplayer.probe;

import java.io.File;
import java.io.IOException;
import java.io.ObjectOutputStream;

import android.content.Context;

public class VDProbeServiceDataManager {
	public final String mFilePath = "probe.data";

	public VDProbeServiceDataManager() {
		super();
	}

	private boolean checkPath(Context context) {
		String path = context.getCacheDir().getAbsolutePath() + "/" + mFilePath;
		File fp = new File(path);
		if (!fp.exists()) {
			try {
				fp.createNewFile();
			} catch (IOException ex) {
				return false;
			}
		}
		return true;
	}

	public void sync(VDProbeServiceData probeData, Context context) {
		if (!checkPath(context)) {
			return;
		}
	}
}

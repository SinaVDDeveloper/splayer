package cn.com.sina.util.whitelist;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Point;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

public class VDWhitelistSysInfo {
	private final static String TAG = "VDWhitelistSysInfo";

	public VDWhitelistSysInfo() {
		super();
	}

	@SuppressLint("NewApi")
	public void transSysInfo(Context context) {

		String brand = VDWhitelistUtil.getSystemProperty("ro.product.brand");
		String model = VDWhitelistUtil.getSystemProperty("ro.product.model");
		String cpu = VDWhitelistUtil.getSystemProperty("ro.product.cpu.abi");
		String sdkVersion = VDWhitelistUtil
				.getSystemProperty("ro.build.version.sdk");
		String sdkRelease = VDWhitelistUtil
				.getSystemProperty("ro.build.version.release");
		String os = VDWhitelistUtil.filterOS();

		WindowManager wm = (WindowManager) context
				.getSystemService(Context.WINDOW_SERVICE);
		Display display = wm.getDefaultDisplay();
		int width = 0;
		int height = 0;
		if (android.os.Build.VERSION.SDK_INT >= 13) {
			Point point = new Point();
			display.getSize(point);
			width = point.x;
			height = point.y;
		} else {
			width = display.getWidth();
			height = display.getHeight();
		}

	}
}

package cn.com.sina.util.whitelist;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.graphics.Point;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.net.Uri;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

public class VDWhitelistUtil {
	private final static String TAG = "VDWhitelistUtil";
	public final static String mSystemProperty = getSystemProperty();

	public static class WhitelistData implements Serializable {
		/**
		 * 
		 */
		private static final long serialVersionUID = 6394952115204899337L;
		public String name = "";
		public String ro_product_brand = "";
		public String ro_product_model = "";
		public String ro_product_cpu_abi = "";
		public String ro_build_version_sdk = "";
		public String ro_build_version_release = "";
		public String ro_board_platform = "";
		public String ro_sf_lcd_density = "";
		public String os = "";
		public int width = 0;
		public int height = 0;
		public long profile0 = 0;
		public long profile1 = 0;
		public long profile2 = 0;
		public long profile4 = 0;
		public long profile8 = 0;
		public long profile16 = 0;
		public long profile32 = 0;
		public long profile64 = 0;
	}

	public static String getDBTmpPath(Context context) {
		String path = context.getFilesDir().getAbsolutePath() + "/whitelist";
		File file = new File(path);
		if (!file.exists()) {
			file.mkdir();
		}
		path += "/whitelist.db.tmp";
		if (!file.exists()) {
			try {
				file.createNewFile();
			} catch (IOException ex) {

			}
		}
		return path;
	}

	public static String getDBPath(Context context) {
		String path = context.getFilesDir().getAbsolutePath() + "/whitelist";
		File file = new File(path);
		if (!file.exists()) {
			file.mkdir();
		}
		path += "/whitelist.db";
		if (!file.exists()) {
			try {
				file.createNewFile();
			} catch (IOException ex) {

			}
		}
		return path;
	}

	public static String getDBTimeStampPath(Context context) {
		String path = context.getCacheDir().getAbsolutePath() + "/whitelist";
		File file = new File(path);
		if (!file.exists()) {
			file.mkdir();
		}
		path += "/whitelist.datetime";

		return path;
	}

	@SuppressLint("NewApi")
	public static void transSysInfo(Context context) {
		WhitelistData data = new WhitelistData();
		data.ro_product_brand = VDWhitelistUtil
				.getSystemProperty("ro.product.brand");
		data.ro_product_model = VDWhitelistUtil
				.getSystemProperty("ro.product.model");
		data.ro_product_cpu_abi = VDWhitelistUtil
				.getSystemProperty("ro.product.cpu.abi");
		data.ro_build_version_sdk = VDWhitelistUtil
				.getSystemProperty("ro.build.version.sdk");
		data.ro_build_version_release = VDWhitelistUtil
				.getSystemProperty("ro.build.version.release");
		data.os = VDWhitelistUtil.filterOS();

		WindowManager wm = (WindowManager) context
				.getSystemService(Context.WINDOW_SERVICE);
		Display display = wm.getDefaultDisplay();
		if (android.os.Build.VERSION.SDK_INT >= 13) {
			Point point = new Point();
			display.getSize(point);
			data.width = point.x;
			data.height = point.y;
		} else {
			data.width = display.getWidth();
			data.height = display.getHeight();
		}

		WhitelistData codecData = listCodec();
		data.profile0 = codecData.profile0;
		data.profile1 = codecData.profile1;
		data.profile2 = codecData.profile2;
		data.profile4 = codecData.profile4;
		data.profile8 = codecData.profile8;
		data.profile16 = codecData.profile16;
		data.profile32 = codecData.profile32;
		data.profile64 = codecData.profile64;

		String path = syncFile(data, context);
		if (!path.equals("")) {
			sendMail(path, context);
		}
	}

	private static void sendMail(String path, Context context) {
		Intent intent = new Intent(android.content.Intent.ACTION_SEND);
		String[] recipients = new String[] { "sunxiao@staff.sina.com.cn", "", };
		intent.putExtra(android.content.Intent.EXTRA_EMAIL, recipients);
		intent.putExtra(android.content.Intent.EXTRA_SUBJECT, "白名单列表");
		intent.putExtra(android.content.Intent.EXTRA_TEXT, "");
		intent.putExtra(android.content.Intent.EXTRA_STREAM,
				Uri.fromFile(new File(path)));
		intent.setType("application/octet-stream");

		context.startActivity(intent);
	}

	private static String syncFile(WhitelistData data, Context context) {
		String path = context.getFilesDir().getAbsolutePath() + "/whitelist";
		File file = new File(path);
		if (!file.exists()) {
			file.mkdir();
		}
		path += "/whitelist.new.data";
		ObjectOutputStream fos = null;
		try {
			fos = new ObjectOutputStream(new FileOutputStream(new File(path)));
			fos.writeObject(data);
		} catch (Exception ex) {
			if (fos != null) {
				try {
					fos.close();
				} catch (Exception ex2) {
				}
				return "";
			}
		}
		return path;
	}

	@SuppressLint("NewApi")
	public static WhitelistData listCodec() {
		WhitelistData ret = new WhitelistData();
		String mimeType = "video/avc";
		if (android.os.Build.VERSION.SDK_INT < 16) {
			return ret;
		}
		int numCodecs = MediaCodecList.getCodecCount();
		for (int i = 0; i < numCodecs; i++) {
			MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);

			if (codecInfo.isEncoder()) {
				continue;
			}
			String[] types = codecInfo.getSupportedTypes();
			for (int j = 0; j < types.length; j++) {
				if (types[j].equalsIgnoreCase(mimeType)) {
					CodecCapabilities ccs = codecInfo
							.getCapabilitiesForType(mimeType);
					for (CodecProfileLevel cpl : ccs.profileLevels) {
						Log.d(TAG, "CodecProfileLevel level-> " + cpl.level
								+ ",profile->" + cpl.profile);

						switch (cpl.level) {
						case 0:
							ret.profile0 |= cpl.profile;
							break;
						case 1:
							ret.profile1 |= cpl.profile;
							break;
						case 2:
							ret.profile2 |= cpl.profile;
							break;
						case 4:
							ret.profile4 |= cpl.profile;
							break;
						case 8:
							ret.profile8 |= cpl.profile;
							break;
						case 16:
							ret.profile16 |= cpl.profile;
							break;
						case 32:
							ret.profile32 |= cpl.profile;
							break;
						case 64:
							ret.profile64 |= cpl.profile;
							break;
						default:
							break;
						}
					}
				}
			}

		}
		return ret;
	}

	public static String filterOS() {
		String prop = mSystemProperty;
		if (prop.contains("miui")) {
			return "MIUI";
		} else if (prop.contains("EmotionUI")) {
			return "EmotionUI";
		} else if (prop.contains("flyme")) {
			return "Flyme";
		} else if (prop.contains("[ro.build.user]: [nubia]")) {
			return "Nubia UI";
		} else if (prop.contains("Nokia_X")) {
			return "Nokia_X";
		} else if (prop.contains("[ro.build.soft.version]: [A.")) {
			return "ColorOS";
		} else if (prop.contains("ro.htc.")) {
			return "HTC";
		} else if (prop.contains("[ro.build.user]: [zte")) {
			return "ZTE";
		} else if (prop.contains("[ro.product.brand]: [vivo")) {
			return "Funtouch OS";
		}
		return "Android";
	}

	private static String getSystemProperty() {
		String line = "";
		BufferedReader input = null;
		try {
			Process p = Runtime.getRuntime().exec("getprop");
			input = new BufferedReader(
					new InputStreamReader(p.getInputStream()), 2048);
			String ret = input.readLine();
			while (ret != null) {
				line += ret + "\n";
				ret = input.readLine();
			}
			input.close();
		} catch (IOException ex) {
			Log.e(TAG, "Unable to read sysprop", ex);
			return null;
		} finally {
			if (input != null) {
				try {
					input.close();
				} catch (IOException e) {
					Log.e(TAG, "Exception while closing InputStream", e);
				}
			}
		}
		return line;
	}

	public static String getSystemProperty(String propName) {
		String line = "";
		BufferedReader input = null;
		try {
			Process p = Runtime.getRuntime().exec("getprop " + propName);
			input = new BufferedReader(
					new InputStreamReader(p.getInputStream()), 1024);
			line = input.readLine();
			input.close();
		} catch (IOException ex) {
			Log.e(TAG, "Unable to read sysprop " + propName, ex);
			return null;
		} finally {
			if (input != null) {
				try {
					input.close();
				} catch (IOException e) {
					Log.e(TAG, "Exception while closing InputStream", e);
				}
			}
		}
		return line;
	}
}

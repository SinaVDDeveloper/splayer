package cn.com.sina.util.whitelist;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import com.splayer.hard.R;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.provider.ContactsContract.Directory;
import android.util.Log;

public class VDWhitelist {
	private static boolean mIsInWhiteList = false;
	private static boolean mIsChecked = false;
	private final static String TAG = "VDWhitelist";
	private Context mContext = null;

	public VDWhitelist() {
		super();
	}

	public void setContext(Context ctx) {
		mContext = ctx;
	}

	public boolean getIsInWhiteList(Context context) {
		if (!mIsChecked) {
			mIsInWhiteList = isWhiteList(context);
			mIsChecked = true;
		}
		return mIsInWhiteList;
	}

	/**
	 * 同时肩负重装app以及网络下载app检查更新的双重任务
	 * 
	 * @param context
	 * @return
	 */
	private String copyWhitelistDB(Context context) {
		File tarDBPath = new File(VDWhitelistUtil.getDBTmpPath(context));
		File tarRealDBPath = new File(VDWhitelistUtil.getDBPath(context));
		OutputStream os = null;
		InputStream is = null;
		try {
			os = new FileOutputStream(tarDBPath);
			is = context.getResources().openRawResource(R.raw.whitelist);
			byte[] buffer = new byte[1024];
			int len = is.read(buffer);
			while (len > 0) {
				os.write(buffer, 0, len);
				len = is.read(buffer);
			}
			os.flush();
			is.close();
			os.close();
		} catch (Exception ex) {
			if (os != null) {
				try {
					os.close();
				} catch (Exception ex2) {

				}
			}
			if (is != null) {
				try {
					is.close();
				} catch (Exception ex2) {

				}
			}
			return null;
		}
		if (tarRealDBPath.exists()) {
			// 如果原来就存在白名单，那么比较日期，保留较新的
			SQLiteDatabase newDB = null;
			SQLiteDatabase oldDB = null;
			try {
				newDB = SQLiteDatabase.openDatabase(
						tarRealDBPath.getAbsolutePath(), null,
						SQLiteDatabase.OPEN_READONLY);
				oldDB = SQLiteDatabase.openDatabase(
						tarDBPath.getAbsolutePath(), null,
						SQLiteDatabase.OPEN_READONLY);
				if (newDB != null && oldDB != null) {
					String sql = "select stamp from datestamp";
					Cursor newCursor = newDB.rawQuery(sql, null);
					Cursor oldCursor = oldDB.rawQuery(sql, null);
					if (newCursor.moveToNext() && oldCursor.moveToNext()) {
						int newDate = Integer.valueOf(newCursor.getString(0));
						int oldDate = Integer.valueOf(oldCursor.getString(0));
						if (oldDate > newDate) {
							// 原来的更新，保留原来的
							tarDBPath.delete();
						} else {
							tarRealDBPath.delete();
							tarDBPath.renameTo(tarRealDBPath);
						}
					}
				} else {
					tarDBPath.renameTo(tarRealDBPath);
				}
			} catch (Exception ex) {
				Log.e(TAG, "error in open db.reason:" + ex.getMessage());
			}

		} else {
			tarDBPath.renameTo(tarRealDBPath);
		}
		return VDWhitelistUtil.getDBPath(context);
	}

	protected boolean isWhiteList(Context context) {
		String brand = VDWhitelistUtil.getSystemProperty("ro.product.brand");
		String model = VDWhitelistUtil.getSystemProperty("ro.product.model");
		String cpu = VDWhitelistUtil.getSystemProperty("ro.product.cpu.abi");
		String sdkVersion = VDWhitelistUtil
				.getSystemProperty("ro.build.version.sdk");
		String sdkRelease = VDWhitelistUtil
				.getSystemProperty("ro.build.version.release");
		String os = VDWhitelistUtil.filterOS();

		String path = copyWhitelistDB(context);

		SQLiteDatabase db = null;
		try {
			db = SQLiteDatabase.openDatabase(path, null,
					SQLiteDatabase.OPEN_READONLY);
		} catch (Exception ex) {
			Log.e(TAG, "error in open db.reason:" + ex.getMessage());
		}
		if (db == null) {
			return false;
		}
		String sql = "select ro_build_version_sdk,ro_build_version_release,os from whitelist where ro_product_brand='"
				+ brand
				+ "' and ro_product_model='"
				+ model
				+ "' and ro_product_cpu_abi='" + cpu + "'";
		Cursor cursor = db.rawQuery(sql, null);
		String sqlBuildSDK = null;
		String sqlBuildRelease = null;
		String sqlOs = null;
		while (cursor.moveToNext()) {
			sqlBuildSDK = cursor.getString(0);
			sqlBuildRelease = cursor.getString(1);
			sqlOs = cursor.getString(2);
		}
		db.close();

		if (sqlBuildSDK == null || sqlBuildRelease == null || os == null) {
			return false;
		}

		if (sqlBuildSDK.equals(sdkVersion)
				&& sqlBuildRelease.equals(sdkRelease)) {
			String myOS = VDWhitelistUtil.filterOS();
			if (sqlOs.equals(myOS)) {
				return true;
			}
		}

		return false;
	}

}

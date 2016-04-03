package cn.com.sina.util.whitelist;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.Date;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.method.DateTimeKeyListener;
import android.util.Log;

/**
 * 按照一下逻辑更新 1、一天值下载一次 2、下载后检查相应的md5传，如果相等则不更新 3、在第一次安装的时候，要检查原来的目录中，是否有文件
 * 4、检查timestamp表中的stamp字段的日期
 * 
 * @author sunxiao
 * 
 */
public class VDWhitelistRefresh {
	private final String TAG = "VDWhitelistRefresh";
	private final String mNonRefreshKey = "NonRefreshKey";
	private final String mRefreshKey = "RefreshKey";
	private final String mUrlString = "";
	private Context mContext = null;
	private final Handler mHandler = new Handler() {
		public void handleMessage(android.os.Message msg) {
			if (msg.getData().containsKey(mNonRefreshKey)) {
				// 无刷新回调
			} else if (msg.getData().containsKey(mRefreshKey)) {
				// 有刷新回调
			}
		};
	};

	public VDWhitelistRefresh() {
		super();
	}

	/**
	 * 不要用activity的context
	 * 
	 * @param context
	 */
	public void refresh(Context context) {
		mContext = context;
		Thread thread = new Thread(new DownloadWhitelist());
		thread.start();
	}

	private long byteArrayToLong(byte[] buffer, int offset) {
		long value = 0;
		for (int i = 0; i < 8; i++) {
			int shift = (4 - 1 - i) * 8;
			value += (buffer[i + offset] & 0x000000FF) << shift;// 往高位游
		}
		return value;
	}

	/**
	 * 按照一下逻辑更新白名单 一天只检查一次
	 * 
	 * @return
	 */
	private boolean isCanRefresh() {
		boolean ret = false;
		if (mContext == null) {
			return false;
		}
		String dateFile = VDWhitelistUtil.getDBTimeStampPath(mContext);
		if (new File(dateFile).exists()) {
			// 已经有此文件，直接比较
			Date date = new Date();
			long curTimeStamp = date.getTime();
			FileInputStream fis = null;
			try {
				fis = new FileInputStream(new File(dateFile));
				byte[] buffer = new byte[256];
				fis.read(buffer);
				long oldTimeStamp = byteArrayToLong(buffer, 0);
				if ((curTimeStamp - oldTimeStamp) > 60 * 60 * 24) {
					ret = true;
				}
			} catch (Exception ex) {
				if (fis != null) {
					try {
						fis.close();
					} catch (Exception ex2) {

					}
				}
			}
		} else {
			ret = true;
		}
		return ret;
	}

	private class DownloadWhitelist implements Runnable {
		@Override
		public void run() {
			// TODO Auto-generated method stub
			if (isCanRefresh()) {
				Bundle bundle = new Bundle();
				bundle.putString(mNonRefreshKey, "");
				Message msg = new Message();
				msg.setData(bundle);
				mHandler.sendMessage(msg);
				return;
			}
			InputStream is = null;
			BufferedOutputStream bos = null;
			String path = VDWhitelistUtil.getDBTmpPath(mContext);
			try {
				URL url = new URL(VDWhitelistRefresh.this.mUrlString);
				URLConnection conn = url.openConnection();
				conn.setConnectTimeout(5000);
				is = conn.getInputStream();
				bos = new BufferedOutputStream(new FileOutputStream(new File(
						path)));
				byte[] buffer = new byte[2048];
				int offset = 0;
				int len = 0;
				while ((len = is.read(buffer, offset, buffer.length)) != -1) {
					bos.write(buffer);
					offset += len;
				}

			} catch (MalformedURLException ex) {
				Log.e(TAG, ex.getMessage());
			} catch (IOException ex) {
				Log.e(TAG, ex.getMessage());
			} catch (Exception ex) {
				Log.e(TAG, ex.getMessage());
			} finally {
				if (bos != null) {
					try {
						bos.close();
					} catch (Exception ex) {

					}
				}
				if (is != null) {
					try {
						is.close();
					} catch (Exception ex) {

					}
				}
			}

			Bundle bundle = new Bundle();
			bundle.putString(mRefreshKey, path);
			Message msg = new Message();
			msg.setData(bundle);
			mHandler.sendMessage(msg);
		}
	}
}

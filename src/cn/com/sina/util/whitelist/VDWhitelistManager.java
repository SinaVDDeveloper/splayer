package cn.com.sina.util.whitelist;

import android.content.Context;
import android.os.HandlerThread;

public class VDWhitelistManager {

	private VDWhitelist mWhitelist = null;

	public VDWhitelistManager() {
		mWhitelist = new VDWhitelist();
	}

	private static class VDWhitelistManagerInstance {
		public static VDWhitelistManager INSTANCE = new VDWhitelistManager();
	};

	public static VDWhitelistManager getInstance() {
		return VDWhitelistManagerInstance.INSTANCE;
	}

	public boolean isInWhitelist(Context context) {
		return mWhitelist.getIsInWhiteList(context);
	}
}

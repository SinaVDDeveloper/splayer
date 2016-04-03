package  com.splayer.hard.ui;

import cn.com.sina.util.whitelist.VDWhitelistManager;
import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import com.splayer.hard.R;
import com.splayer.hard.SinaApplication;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;

public class VDWhitelistActivity extends Activity {
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		super.onCreate(savedInstanceState);

		setContentView(R.layout.whitelist);

		TextView tv1 = (TextView) findViewById(R.id.tv1);
		if (VDWhitelistManager.getInstance().isInWhitelist(this)) {
			tv1.setText("白名单机型");
		} else {
			tv1.setText("非名单机型");
		}
	}
}


package com.splayer.hard.ui;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import com.splayer.hard.R;
import com.splayer.hard.R.id;
import com.splayer.hard.R.layout;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.ToggleButton;

public class SingleActivity extends Activity implements View.OnClickListener {

    private String TAG = "*liang*java*";
    private Button play;
    private EditText addr;
    private Button play2;
    private EditText addr2;
    private Button playHttp;
    private EditText addrHttp;
    private Button playHttp2;
    private Button playHttp3;
    private EditText addrHttp2;
    private Button playLive;
    private EditText addrLive;
    private Button playLive2;
    private EditText addrLive2;
    private Button playLive3;
    private ToggleButton fastEnable;
    private int fast = 0;
    private ToggleButton dropEnable;
    private int drop = -1;
    private ToggleButton debugEnable;
    private int debug = 0;
    private ToggleButton softEnable;
    private int soft = 0;
    private Button btnTest;
    private Button btnVideoView;
    private Spinner lowresSpin;
    private int lowres = 0;
    private String[] lowresVal = {
            "0", "1", "2", "3"
    };
    private Spinner skipframeSpin;
    private int skipframe = 0;
    private String[] skipVal = {
            "-1", "0", "1", "2", "3", "4"
    };
    private Spinner skipIdctSpin;
    private int skipIdct = 0;
    private Spinner skipLoopfilterSpin;
    private int skipLoopfilter = 0;
    private Spinner bufferFrameSpin;
    private int bufferFrame = 0;
    private String[] bufferVal = {
            "-1", "1", "2", "3", "4"
    };

    private String localFile = "http://202.106.169.170/player/ovs1_idx_chid_1048087_br_400_fn_3_pn_weitv_sig_md5.m3u8";
    private String localFile2 = "http://111.161.7.204/data10/video09/2014/10/29/2571209-102-2330.mp4";
    // private String localFile2 =
    // "http://111.161.7.204/data10/video09/2014/10/29/2571209-102-2330.mp4";

    private String httpFile = "http://202.106.169.170/mtv.v.iask.com/manifest/2014091629_400.m3u8";
    private String httpFile2 = "http://202.106.169.170/mtv.v.iask.com/manifest/2014091629_800.m3u8";
    private String httpFile3 = "http://124.192.140.140/mtv.v.iask.com/vod/20141200131_hd.m3u8";

    // private String liveFile =
    // "http://202.106.169.170/mtv.v.iask.com/manifest/201408113_200.m3u8";
    // private String liveFile2 =
    // "http://202.106.169.170/mtv.v.iask.com/manifest/201408113_400.m3u8";
    // private String liveFile3 = "";
    // private String liveFile =
    // "http://202.106.169.170/mtv.v.iask.com/manifest/20141224162_400.m3u8";
    // private String liveFile2 =
    // "http://202.106.169.170/mtv.v.iask.com/manifest/20141224162_800.m3u8";
    // private String liveFile3 =
    // "http://202.106.169.170/mtv.v.iask.com/manifest/20141224162_1200.m3u8";
    private String liveFile = "http://202.106.169.170/mtv.v.iask.com/manifest/20141221146_400.m3u8";
    private String liveFile2 = "http://202.106.169.170/mtv.v.iask.com/manifest/20141221146_800.m3u8";
    private String liveFile3 = "http://202.106.169.170/mtv.v.iask.com/manifest/20141221146_1200.m3u8";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(TAG, "SingleActivity onCreate() into");

        // requestWindowFeature(Window.FEATURE_NO_TITLE);
        // getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
        // WindowManager.LayoutParams.FLAG_FULLSCREEN);
        // Set screen orientation
        // setRequestedOrientation (/*ActivityInfo.SCREEN_ORIENTATION_PORTRAIT*/
        // ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        setContentView(R.layout.single);

        // local
        addr = (EditText) findViewById(R.id.btnAddr);
        addr.setText(localFile);
        play = (Button) findViewById(R.id.btnPlay);
        play.setOnClickListener(this);

        addr2 = (EditText) findViewById(R.id.btnAddr2);
        addr2.setText(localFile2);
        play2 = (Button) findViewById(R.id.btnPlay2);
        play2.setOnClickListener(this);

        // http
        playHttp = (Button) findViewById(R.id.btnPlayHttp);
        playHttp.setOnClickListener(this);

        playHttp2 = (Button) findViewById(R.id.btnPlayHttp2);
        playHttp2.setOnClickListener(this);

        playHttp3 = (Button) findViewById(R.id.btnPlayHttp3);
        playHttp3.setOnClickListener(this);

        // live
        playLive = (Button) findViewById(R.id.btnPlayLive);
        playLive.setOnClickListener(this);

        playLive2 = (Button) findViewById(R.id.btnPlayLive2);
        playLive2.setOnClickListener(this);

        playLive3 = (Button) findViewById(R.id.btnPlayLive3);
        playLive3.setOnClickListener(this);

        // for test
        btnTest = (Button) findViewById(R.id.btnTest);
        btnTest.setOnClickListener(this);

        btnVideoView = (Button) findViewById(R.id.btnVideoView);
        btnVideoView.setOnClickListener(this);

        // for config
        dropEnable = (ToggleButton) findViewById(R.id.dropEnable);
        dropEnable.setOnClickListener(this);
        dropEnable.setChecked(false);
        fastEnable = (ToggleButton) findViewById(R.id.fastEnable);
        fastEnable.setOnClickListener(this);
        fastEnable.setChecked(false);
        // debug
        debugEnable = (ToggleButton) findViewById(R.id.debugEnable);
        debugEnable.setOnClickListener(this);
        debugEnable.setChecked(false);
        // soft
        softEnable = (ToggleButton) findViewById(R.id.softEnable);
        softEnable.setOnClickListener(this);
        softEnable.setChecked(false);

        lowresSpin = (Spinner) findViewById(R.id.lowres);
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, lowresVal);
        lowresSpin.setAdapter(adapter);

        skipframeSpin = (Spinner) findViewById(R.id.skipFrame);
        ArrayAdapter<String> adapter2 = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, skipVal);
        skipframeSpin.setAdapter(adapter2);

        skipIdctSpin = (Spinner) findViewById(R.id.skipIdct);
        ArrayAdapter<String> adapter3 = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, skipVal);
        skipIdctSpin.setAdapter(adapter3);

        skipLoopfilterSpin = (Spinner) findViewById(R.id.skipLoopfilter);
        ArrayAdapter<String> adapter4 = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, skipVal);
        skipLoopfilterSpin.setAdapter(adapter4);

        bufferFrameSpin = (Spinner) findViewById(R.id.bufferFrame);
        ArrayAdapter<String> adapter5 = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, bufferVal);
        bufferFrameSpin.setAdapter(adapter5);

        Log.d(TAG, "SingleActivity onCreate() out");
    }

    private void Play(int id) {
        String pf = "";
        if (id == R.id.btnPlay) {
            pf = addr.getText().toString();
        }
        if (id == R.id.btnPlay2) {
            pf = addr2.getText().toString();
        } else if (id == R.id.btnPlayHttp) {
            pf = httpFile; // addrHttp.getText().toString();
        } else if (id == R.id.btnPlayHttp2) {
            pf = httpFile2; // addrHttp2.getText().toString();
        } else if (id == R.id.btnPlayHttp3) {
            pf = httpFile3; // addrHttp2.getText().toString();
        } else if (id == R.id.btnPlayLive) {
            pf = liveFile; // addrLive.getText().toString();
        } else if (id == R.id.btnPlayLive2) {
            pf = liveFile2; // addrLive2.getText().toString();
        } else if (id == R.id.btnPlayLive3) {
            pf = liveFile3; // addrLive2.getText().toString();
        }
        if (pf.length() > 0) {
            Intent intent = new Intent(this, VideoActivity.class);
            intent.putExtra("fileName", pf);
            // fast decode
            if (fastEnable.isChecked()) {
                fast = 1;
            } else {
                fast = 0;
            }
            intent.putExtra("fast", fast);
            // drop late frame
            if (dropEnable.isChecked()) {
                drop = 1;
            } else {
                drop = -1;
            }
            intent.putExtra("drop", drop);
            // debug
            if (debugEnable.isChecked()) {
                debug = 1;
            } else {
                debug = 0;
            }
            intent.putExtra("debug", debug);
            if (softEnable.isChecked()) {
                soft = 1;
            } else {
                soft = 0;
            }
            intent.putExtra("soft", soft);
            // lowres
            lowres = Integer.parseInt(lowresVal[lowresSpin.getSelectedItemPosition()]);
            intent.putExtra("lowres", lowres);
            // skip frame
            skipframe = Integer.parseInt(skipVal[skipframeSpin.getSelectedItemPosition()]);
            intent.putExtra("skip", skipframe);
            // skip idct
            skipIdct = Integer.parseInt(skipVal[skipIdctSpin.getSelectedItemPosition()]);
            intent.putExtra("idct", skipIdct);
            // skip loopfilter
            skipLoopfilter = Integer.parseInt(skipVal[skipLoopfilterSpin.getSelectedItemPosition()]);
            intent.putExtra("filter", skipLoopfilter);
            // buffer frame
            bufferFrame = Integer.parseInt(bufferVal[bufferFrameSpin.getSelectedItemPosition()]);
            intent.putExtra("buffer", bufferFrame);
            startActivity(intent);
            Log.d(TAG, "SingleActivity drop=" + drop + ",fast=" + fast + ",lowres=" + lowres + ",skipframe=" + skipframe
                    + ",skipIdct=" + skipIdct + "skipLoopfilter=" + skipLoopfilter + ",buffer" + bufferFrame);
        }
    }

    private void test() {
        Intent intent = new Intent(this, TestActivity.class);
        startActivity(intent);
    }

    private void testVideoView() {
        Intent intent = new Intent(this, PlayerActivity.class);
        startActivity(intent);
    }

    public void onClick(View arg0) {
        switch (arg0.getId()) {
            case R.id.btnPlay:
            case R.id.btnPlay2:
            case R.id.btnPlayHttp:
            case R.id.btnPlayLive:
            case R.id.btnPlayHttp2:
            case R.id.btnPlayLive2:
            case R.id.btnPlayHttp3:
            case R.id.btnPlayLive3:
                Play(arg0.getId());
                break;
            case R.id.btnTest:
                test();
            case R.id.btnVideoView:
                testVideoView();
            case R.id.fastEnable:
                if (fastEnable.isChecked()) {
                    fast = 1;
                } else {
                    fast = 0;
                }
                break;
            case R.id.dropEnable:
                if (dropEnable.isChecked()) {
                    drop = 1;
                } else {
                    drop = -1;
                }
                break;
            case R.id.debugEnable:
                if (fastEnable.isChecked()) {
                    debug = 1;
                } else {
                    debug = 0;
                }
                break;
        }
    }

    class SpinnerSelectedListener implements OnItemSelectedListener {

        public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2, long arg3) {
            Log.d(TAG, "SingleActivity SpinnerSelectedListener.onItemSelected() into");
            if (arg0.getId() == R.id.lowres) {
                SingleActivity.this.lowres = Integer.parseInt(SingleActivity.this.lowresVal[arg2]);
            } else if (arg0.getId() == R.id.skipFrame) {
                SingleActivity.this.skipframe = Integer.parseInt(SingleActivity.this.lowresVal[arg2]);
            } else if (arg0.getId() == R.id.skipIdct) {
                SingleActivity.this.skipIdct = Integer.parseInt(SingleActivity.this.lowresVal[arg2]);
            } else if (arg0.getId() == R.id.skipLoopfilter) {
                SingleActivity.this.skipLoopfilter = Integer.parseInt(SingleActivity.this.lowresVal[arg2]);
            } else if (arg0.getId() == R.id.bufferFrame) {
                SingleActivity.this.bufferFrame = Integer.parseInt(SingleActivity.this.bufferVal[arg2]);
            } else {
                Log.d(TAG, "SingleActivity SpinnerSelectedListener.onItemSelected() unknow spiner");
            }
        }

        public void onNothingSelected(AdapterView<?> arg0) {
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            finish();
            // System.exit(0);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
}

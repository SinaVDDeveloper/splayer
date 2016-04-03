
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

public class MainActivity extends Activity implements View.OnClickListener {

    private String TAG = "*liang*java*";
    private Button btnSingle;
    private Button btnMulti;
    private Button btnTow;
    private Button btnOverlap;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(TAG, "MainActivity onCreate() into");

        // requestWindowFeature(Window.FEATURE_NO_TITLE);
        // getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
        // WindowManager.LayoutParams.FLAG_FULLSCREEN);
        // Set screen orientation
        // setRequestedOrientation (/*ActivityInfo.SCREEN_ORIENTATION_PORTRAIT*/
        // ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        setContentView(R.layout.main);

        btnSingle = (Button) findViewById(R.id.btnSingle);
        btnSingle.setOnClickListener(this);

        btnMulti = (Button) findViewById(R.id.btnMulti);
        btnMulti.setOnClickListener(this);

        btnTow = (Button) findViewById(R.id.btnTow);
        btnTow.setOnClickListener(this);

        btnOverlap = (Button) findViewById(R.id.btnOverlap);
        btnOverlap.setOnClickListener(this);

        Log.d(TAG, "MainActivity onCreate() out");
    }

    private void single() {
        Intent intent = new Intent(this, SingleActivity.class);
        startActivity(intent);
    }

    private void multi() {
        Intent intent = new Intent(this, MultiActivity.class);
        startActivity(intent);
    }

    private void tow() {
        // Intent intent = new Intent(this,TowPLayerActivity.class);
        // startActivity(intent);
    }

    private void overlap() {
        // Intent intent = new Intent(this,OverlapActivity.class);
        // startActivity(intent);
    }

    public void onClick(View arg0) {
        switch (arg0.getId()) {
            case R.id.btnSingle:
                single();
                break;
            case R.id.btnMulti:
                multi();
                break;
            case R.id.btnTow:
                tow();
                break;
            case R.id.btnOverlap:
                overlap();
                break;
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            finish();
            System.exit(0);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
}

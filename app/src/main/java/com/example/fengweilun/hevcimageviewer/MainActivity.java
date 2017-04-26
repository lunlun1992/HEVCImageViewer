package com.example.fengweilun.hevcimageviewer;

import android.app.Activity;
import android.graphics.Color;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.support.annotation.Nullable;
import android.text.Layout;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.util.Timer;
import java.util.TimerTask;

/**
 * Created by fengweilun on 13/04/2017.
 */

public class MainActivity extends Activity
{
    MyGLSurfaceView view1;
    MyGLSurfaceView view2;
    TextView tv1;
    TextView tv2;
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //setContentView(R.layout.activity_main);
        LinearLayout out = new LinearLayout(this);
        out.setOrientation(LinearLayout.VERTICAL);
        out.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        //first linearlayout
        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.HORIZONTAL);
        ll.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        view1 = new MyGLSurfaceView(this, 0);
        view1.setLayoutParams(new ViewGroup.LayoutParams(720, 960));
        tv1 = new TextView(this);
        tv1.setTextColor(Color.BLACK);
        tv1.setText("dasdasdas");
        ll.addView(view1);
        ll.addView(tv1);


//        LinearLayout lw = new LinearLayout(this);
//        lw.setOrientation(LinearLayout.HORIZONTAL);
//        lw.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
//        view2 = new MyGLSurfaceView(this, 1);
//        view2.setLayoutParams(new ViewGroup.LayoutParams(720, 960));
//        tv2 = new TextView(this);
//        tv2.setTextColor(Color.BLACK);
//        tv2.setText("dasdasdas");
//        lw.addView(view2);
//        lw.addView(tv2);


        out.addView(ll);
        //out.addView(lw);
        setContentView(out);

    }
}



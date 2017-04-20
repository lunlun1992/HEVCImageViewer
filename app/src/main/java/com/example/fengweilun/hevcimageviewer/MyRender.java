package com.example.fengweilun.hevcimageviewer;

import android.graphics.Canvas;
import android.opengl.GLSurfaceView;
import android.support.annotation.IntegerRes;
import android.widget.TextView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by fengweilun on 14/04/2017.
 */

public class MyRender implements GLSurfaceView.Renderer
{
    private class MyData
    {
        public long frames;
    }
    private int decoder_tag;
    private MyData data = new MyData();
    static{
        System.loadLibrary("imageshow");
    }
    public MyRender()
    {
        super();
    }

    public long getFrames()
    {
        long frames = 0;
        synchronized (data) {
            frames = data.frames;
        }
        return frames;
    }

    public MyRender(int tag)
    {
        super();
        this.decoder_tag = tag;
    }

    private native long nativeDrawFrame(int decoder_tag);
    private native int nativeInit(int decoder_tag);
    //private native int nativeSetup(int w, int h);
    @Override
    public void onDrawFrame(GL10 gl)
    {
        long ret = nativeDrawFrame(decoder_tag);

        synchronized (data) {
            data.frames = ret;
        }

    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height)
    {
        //nativeSetup(width, height);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config)
    {
        nativeInit(decoder_tag);
    }

}

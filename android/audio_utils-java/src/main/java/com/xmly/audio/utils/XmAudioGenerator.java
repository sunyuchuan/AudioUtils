package com.xmly.audio.utils;

import android.os.Build;
import android.util.Log;

/**
 * 为录制的人声pcm数据添加特效并混音,编码输出到m4a文件
 *
 * Created by sunyc on 19-11-19.
 */

public class XmAudioGenerator {
    private static final String TAG = "XmAudioGenerator";
    private static final int DefaultSampleRate = 44100;
    private static final int DefaultChannelNumber = 1;
    // 编码器类型:软件编码
    public static final int ENCODER_FFMPEG = 0;
    // 编码器类型:硬件编码
    public static final int ENCODER_MEDIA_CODEC = 1;
    //是否加载过so
    private static boolean mIsLibLoaded = false;
    //本地XmAudioGenerator对象
    private long mNativeXmAudioGenerator = 0;
    //start接口的返回值类型 与本地的enum GeneratorStatus一一对应
    public static final int GS_ERROR = -1;
    public static final int GS_COMPLETED = 0;
    public static final int GS_STOPPED = 1;

    private static final LibLoader sLocalLibLoader = new LibLoader() {
        @Override
        public void loadLibrary(String libName) throws UnsatisfiedLinkError, SecurityException {
            String ABI = Build.CPU_ABI;
            Log.i(TAG, "ABI " + ABI + " libName " +libName);
            System.loadLibrary(libName + "-" + ABI);
        }
    };

    private static void loadLibrariesOnce(LibLoader libLoader) {
        synchronized (XmAudioGenerator.class) {
            if (!mIsLibLoaded) {
                if (libLoader == null)
                    libLoader = sLocalLibLoader;

                libLoader.loadLibrary("ijkffmpeg");
                libLoader.loadLibrary("xmaudio_utils");
                mIsLibLoaded = true;
            }
        }
    }

    private void init() {
        setLogModeAndLevel(XmAudioUtils.LOG_MODE_ANDROID, XmAudioUtils.LOG_LEVEL_DEBUG, null);
        try {
            native_setup();
        } catch (OutOfMemoryError e) {
            e.printStackTrace();
        }
    }

    public XmAudioGenerator()
    {
        loadLibrariesOnce(sLocalLibLoader);
        init();
    }

    public XmAudioGenerator(LibLoader libLoader)
    {
        loadLibrariesOnce(libLoader);
        init();
    }

    /**
     * native层代码的日志控制
     * @param logMode 日志打印模式:
     *                LOG_MODE_FILE 输出日志到文件
     *                LOG_MODE_ANDROID 输出到android的日志系统logcat
     *                LOG_MODE_SCREEN ubuntu等电脑操作系统打印在终端窗口上显示
     * @param logLevel 日志级别 LOG_LEVEL_DEBUG等
     * @param outLogPath 如果输出到文件,需要设置文件路径
     */
    public void setLogModeAndLevel(int logMode, int logLevel, String outLogPath) {
        if (logMode == XmAudioUtils.LOG_MODE_FILE && outLogPath == null) {
            Log.e(TAG, "Input Params is inValid, exit");
            return;
        }

        native_set_log(logMode, logLevel, outLogPath);
    }

    /**
     * 添加特效并混音/编码输出
     * @param inConfigFilePath json配置文件,包含特效参数和混音参数等所有参数
     * @param outM4aPath 输出m4a文件路径
     * @param encoderType 编码器类型:
     *                    ENCODER_FFMPEG 软件编码
     *                    ENCODER_MEDIA_CODEC 硬件编码
     * @return GS_ERROR GS_COMPLETED GS_STOPPED
     */
    public int start(String inConfigFilePath, String outM4aPath, int encoderType) {
        int ret = -1;
        if (inConfigFilePath == null || outM4aPath == null) {
            return ret;
        }

        try {
            ret = native_start(inConfigFilePath, outM4aPath, encoderType);
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        return ret;
    }

    /**
     * 获取合成进度
     * @return 进度值 范围是0到100
     */
    public int getProgress() {
        int ret = -1;

        try {
            ret = native_get_progress();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }

        return ret;
    }

    /**
     * 主动停止合成
     */
    public void stop() {
        try {
            native_stop();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    /**
     * 释放native内存
     */
    public void release() {
        try {
            native_release();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        }
    }

    private native void native_release();
    private native void native_stop();
    private native int native_get_progress();
    private native int native_start(String inConfigFilePath, String outM4aPath, int encoderType);
    private native void native_set_log(int logMode, int logLevel, String outLogPath);
    private native void native_close_log_file();
    private native void native_setup();

    @Override
    protected void finalize() throws Throwable {
        Log.i(TAG, "finalize");
        try {
            native_release();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        } finally {
            super.finalize();
        }
    }
}
package com.xmly.audio.effect.activity;

import android.content.Context;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;

import com.xmly.audio.effect.JsonUtils;
import com.xmly.audio.effect.R;
import com.xmly.audio.effect.Utils;
import com.xmly.audio.effect.audio.AudioPlayer;
import com.xmly.audio.utils.XmAudioGenerator;
import com.xmly.audio.utils.XmAudioUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Created by sunyc on 19-12-23.
 */

public class MixActivity extends AppCompatActivity implements View.OnClickListener {
    private final static String TAG = MixActivity.class.getName();
    private static final String config = "/sdcard/audio_effect_test/effect_config.txt";
    private static final String output = "/sdcard/audio_effect_test/mixed.pcm";
    private static final String encoderOutput = "/sdcard/audio_effect_test/final.m4a";

    private static final String decodeRawAudio = "/sdcard/audio_effect_test/side_chain_music_test.wav";
    private static final String decodePcm = "/sdcard/audio_effect_test/decode.pcm";

    private AudioPlayer mPlayer;
    private XmAudioUtils mAudioUtils;
    private XmAudioGenerator mAudioGenerator;

    private Button mBtnMix;
    private Button mBtnAudition;
    private Button mBtnFade;

    private Handler mHandler = null;
    private static final int MSG_PROGRESS = 1;
    private static final int MSG_COMPLETED = 2;
    private static final int MSG_MIX_COMPLETED = 3;
    private static final int MSG_FADE_COMPLETED = 4;
    private volatile boolean abort = false;
    private volatile boolean abortProgress = false;
    private volatile boolean abortFade = false;
    private ProgressBar mProgressBar;

    public static Intent newIntent(Context context) {
        Intent intent = new Intent(context, MixActivity.class);
        return intent;
    }

    public static void intentTo(Context context) {
        context.startActivity(newIntent(context));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_mix);

        mAudioUtils = new XmAudioUtils();
        mAudioGenerator = new XmAudioGenerator();
        mPlayer = new AudioPlayer();

        mBtnAudition = findViewById(R.id.btn_mix_audition);
        mBtnAudition.setOnClickListener(this);

        mBtnMix = findViewById(R.id.btn_mix);
        mBtnMix.setOnClickListener(this);

        mBtnFade = findViewById(R.id.btn_fade);
        mBtnFade.setOnClickListener(this);

        mProgressBar = findViewById(R.id.progress_mix);
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mHandler == null) {
            mHandler = new Handler() {
                @Override
                public void handleMessage(Message msg) {
                    switch (msg.what) {
                        case MSG_PROGRESS:
                            mProgressBar.setProgress(msg.arg1);
                            break;
                        case MSG_COMPLETED:
                            mBtnAudition.setText("试听");
                            stopAudition();
                            break;
                        case MSG_MIX_COMPLETED:
                            mBtnMix.setText("合成");
                            mProgressBar.setProgress(0);
                            abortProgress = true;
                            break;
                        case MSG_FADE_COMPLETED:
                            mBtnFade.setText("试听");
                            stopFade();
                            break;
                        default:
                            break;
                    }
                }
            };
        }
    }

    @Override
    protected void onStop() {
        stopAudition();
        stopMix();
        stopFade();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        mAudioUtils.release();
        mAudioGenerator.release();
        super.onDestroy();
    }

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.btn_mix_audition) {
            if (mBtnAudition.getText().toString().contentEquals("试听")) {
                stopFade();
                startAudition();
            } else if (mBtnAudition.getText().toString().contentEquals("停止试听")) {
                stopAudition();
            }
        } else if (id == R.id.btn_mix) {
            if (mBtnMix.getText().toString().contentEquals("合成")) {
                startMix();
            } else if (mBtnMix.getText().toString().contentEquals("停止合成")) {
                stopMix();
            }
        } else if (id == R.id.btn_fade) {
            if (mBtnFade.getText().toString().contentEquals("试听")) {
                stopAudition();
                startFade();
            } else if (mBtnFade.getText().toString().contentEquals("停止试听")) {
                stopFade();
            }
        }
    }

    private void startAudition() {
        mBtnAudition.setText("停止试听");
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                long startTime = System.currentTimeMillis();
                if (!mPlayer.isPlayerStarted()) {
                    mPlayer.startPlayer(AudioManager.STREAM_MUSIC, 44100, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
                }
                JsonUtils.createOutputFile(output);

                int bufferSize = 1024;
                short[] buffer = new short[bufferSize];
                File outMixedPcm = new File(output);
                if (outMixedPcm.exists()) outMixedPcm.delete();

                FileOutputStream osMixedPcm = null;
                try {
                    osMixedPcm = new FileOutputStream(outMixedPcm);
                } catch (FileNotFoundException e) {
                    e.printStackTrace();
                }

                int ret = mAudioUtils.mixer_init(config);
                if (ret < 0) {
                    Log.e(TAG, "mixer_init failed");
                    return;
                }

                ret = mAudioUtils.mixer_seekTo(11000);
                if (ret < 0) {
                    Log.e(TAG, "mixer_seekTo failed");
                    return;
                }

                long cur_size = 0;
                abort = false;
                while (!abort) {
                    ret = mAudioUtils.get_mixed_frame(buffer, bufferSize);
                    if (ret <= 0) {
                        mPlayer.stopPlayer();
                        break;
                    }

                    try {
                        if (mPlayer.isPlayerStarted()) {
                            mPlayer.play(buffer, 0, ret);
                        }
                        byte[] data = Utils.getByteArrayInLittleOrder(buffer);
                        osMixedPcm.write(data, 0, 2*ret);
                        osMixedPcm.flush();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }

                    cur_size += ret;
                    if (1000 * (cur_size / (float)44100 / 2) > 33000) {
                        break;
                    }
                }

                ret = mAudioUtils.mixer_seekTo(96226);
                if (ret < 0) {
                    Log.e(TAG, "mixer_seekTo failed");
                    return;
                }

                while (!abort) {
                    ret = mAudioUtils.get_mixed_frame(buffer, bufferSize);
                    if (ret <= 0) {
                        mPlayer.stopPlayer();
                        break;
                    }

                    try {
                        if (mPlayer.isPlayerStarted()) {
                            mPlayer.play(buffer, 0, ret);
                        }
                        byte[] data = Utils.getByteArrayInLittleOrder(buffer);
                        osMixedPcm.write(data, 0, 2*ret);
                        osMixedPcm.flush();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
                long endTime = System.currentTimeMillis();
                Log.i(TAG, "mix audition cost time "+(float)(endTime - startTime)/(float)1000);
                mPlayer.stopPlayer();
                mHandler.sendMessage(mHandler.obtainMessage(MSG_COMPLETED));
            }
        };
        new Thread(runnable).start();
    }

    private void stopAudition() {
        //mPlayer.stopPlayer();
        abort = true;
    }

    private void startMix() {
        mBtnMix.setText("停止合成");
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                JsonUtils.createOutputFile(encoderOutput);
                long startTime = System.currentTimeMillis();
                int ret = mAudioGenerator.start(config, encoderOutput, XmAudioUtils.ENCODER_FFMPEG);
                if (ret == XmAudioGenerator.GS_ERROR) {
                    Log.e(TAG, "mix error");
                } else if (ret == XmAudioGenerator.GS_STOPPED) {
                    Log.w(TAG, "mix stopped");
                }
                long endTime = System.currentTimeMillis();
                Log.i(TAG, "mix cost time "+(float)(endTime - startTime)/(float)1000);
                mHandler.sendMessage(mHandler.obtainMessage(MSG_MIX_COMPLETED));
            }
        };
        new Thread(runnable).start();

        Runnable progressRunnable = new Runnable() {
            @Override
            public void run() {
                int progress = 0;
                abortProgress = false;
                while (progress < 99 && !abortProgress) {
                    try {
                        Thread.sleep(100L);
                        progress = mAudioGenerator.getProgress();
                        mHandler.sendMessage(mHandler.obtainMessage(MSG_PROGRESS, progress, 0));
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                mHandler.sendMessage(mHandler.obtainMessage(MSG_MIX_COMPLETED));
            }
        };
        new Thread(progressRunnable).start();
        mProgressBar.setProgress(0);
    }

    private void stopMix() {
        mAudioGenerator.stop();
        abortProgress = true;
    }

    private void startFade() {
        mBtnFade.setText("停止试听");
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                long startTime = System.currentTimeMillis();
                if (!mPlayer.isPlayerStarted()) {
                    mPlayer.startPlayer(AudioManager.STREAM_MUSIC, 44100, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
                }
                JsonUtils.createOutputFile(decodePcm);

                int bufferSize = 1024;
                short[] buffer = new short[bufferSize];
                File outDecode = new File(decodePcm);
                if (outDecode.exists()) outDecode.delete();
                FileOutputStream osDecode = null;
                try {
                    osDecode = new FileOutputStream(outDecode);
                } catch (FileNotFoundException e) {
                    e.printStackTrace();
                }

                int cropStartTimeMs = 0;
                int cropEndTimeMs = XmAudioUtils.getAudioFileDurationMs(decodeRawAudio, false, 0, 0, 0) / 2;
                mAudioUtils.decoder_create(decodeRawAudio, cropStartTimeMs, cropEndTimeMs, 44100, 2, 100);
                mAudioUtils.decoder_seekTo(1000);
                mAudioUtils.fadeInit(44100, 2, 0, cropEndTimeMs, 3000, 3000);
                long curSize = 0;
                abortFade = false;
                while (!abortFade) {
                    int ret = mAudioUtils.get_decoded_frame(buffer, bufferSize, false);
                    if (ret <= 0) {
                        mPlayer.stopPlayer();
                        break;
                    }

                    int bufferStartTime = (int) ((double)(1000 * curSize) / 2 / 44100);
                    curSize += ret;
                    mAudioUtils.fade(buffer, ret, bufferStartTime);

                    try {
                        if (mPlayer.isPlayerStarted()) {
                            mPlayer.play(buffer, 0, ret);
                        }
                        byte[] data = Utils.getByteArrayInLittleOrder(buffer);
                        osDecode.write(data, 0, 2*ret);
                        osDecode.flush();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }

                try {
                    osDecode.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                long endTime = System.currentTimeMillis();
                Log.i(TAG, "decode cost time "+(float)(endTime - startTime)/(float)1000);
                mPlayer.stopPlayer();
                mHandler.sendMessage(mHandler.obtainMessage(MSG_FADE_COMPLETED));
            }
        };
        new Thread(runnable).start();
    }

    private void stopFade() {
        abortFade = true;
    }
}

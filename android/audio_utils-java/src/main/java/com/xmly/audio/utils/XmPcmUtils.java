package com.xmly.audio.utils;

import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;

public class XmPcmUtils {
    private static final String TAG = "XmPcmUtils";
    private static final int SAMPLE_RATE = 44100;
    private static final int NB_CHANNELS = 2;
    private static final int BYTES_PER_SAMPLE = 2;
    private String mInPcmPath = null;
    private int mSrcSampleRate = 0;
    private int mSrcNbChannels = 0;
    private int mDstSampleRate = 0;
    private int mDstNbChannels = 0;
    File mInFile = null;
    BufferedInputStream mBis = null;
    private long mCurSamplePosition = 0;

    private short[] getShortArrayInLittleOrder(byte[] b) {
        if (b == null || b.length % 2 != 0)
            throw new IllegalArgumentException("无法转换数组，输入参数错误：b == null or b.length % 2 != 0");

        short[] s = new short[b.length >> 1];
        ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(s);
        return s;
    }

    public XmPcmUtils() {}

    /**
     * init XmPcmUtils
     * @param inPcmPath pcm文件路径
     * @param srcSampleRate pcm原始采样率
     * @param srcNbChannels pcm声道数
     * @param dstSampleRate 目标采样率
     * @param dstNbChannels 目标声道数
     * @return false|true false:失败 true:成功
     */
    public synchronized boolean init(String inPcmPath, int srcSampleRate, int srcNbChannels,
                                     int dstSampleRate, int dstNbChannels) {
        if (null == inPcmPath || (srcNbChannels != 1 && srcNbChannels != 2)) return false;
        if (dstSampleRate <= 0 || (dstNbChannels != 1 && dstNbChannels != 2)) return false;

        mInPcmPath = inPcmPath;
        mSrcSampleRate = srcSampleRate;
        mSrcNbChannels = srcNbChannels;
        mDstSampleRate = dstSampleRate;
        mDstNbChannels = dstNbChannels;
        mCurSamplePosition = 0;
        try {
            mInFile = new File(mInPcmPath);
            if (mBis != null) {
                mBis.close();
                mBis = null;
            }
            mBis = new BufferedInputStream(new FileInputStream(mInFile));
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    /**
     * 得到采样后的数据
     * @param buffer 目标buffer
     * @return 获取到的有效pcm数据长度
     */
    public synchronized int resample(short[] buffer) {
        if (null == buffer || null == mBis) return -1;

        int interval = (int) Math.ceil(mSrcSampleRate / (double) mDstSampleRate);
        int curNbSample = 0;
        int dstNbSample = buffer.length / mDstNbChannels;

        try {
            int blockSize = BYTES_PER_SAMPLE * mSrcNbChannels;
            int nbSamples = 512;
            byte[] temp = new byte[nbSamples * blockSize];
            int length = 0;
            while (interval * (dstNbSample - curNbSample) >= nbSamples) {
                if (mBis == null || (length = mBis.read(temp)) <= 0) {
                    if (mBis != null) {
                        mBis.close();
                        mBis = null;
                    }
                    break;
                }
                int lengthInShort = length >> 1;
                short[] tempShort = getShortArrayInLittleOrder(temp);

                for (int i = 0; i < lengthInShort; i += mSrcNbChannels) {
                    if (((mCurSamplePosition + (i / mSrcNbChannels)) % interval) == 0) {
                        if (mDstNbChannels == 1) {
                            if (mSrcNbChannels == 1) {
                                buffer[curNbSample] = tempShort[i];
                            } else {
                                buffer[curNbSample] = (short) ((tempShort[i] + tempShort[i + 1]) / 2);
                            }
                        } else {
                            if (mSrcNbChannels == 1) {
                                buffer[2 * curNbSample] = tempShort[i];
                                buffer[2 * curNbSample + 1] = tempShort[i];
                            } else {
                                buffer[2 * curNbSample] = tempShort[i];
                                buffer[2 * curNbSample + 1] = tempShort[i + 1];
                            }
                        }
                        curNbSample ++;
                    }
                }

                mCurSamplePosition += lengthInShort / mSrcNbChannels;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
        return curNbSample * mDstNbChannels;
    }

    /**
     * 获取pcm文件的时长
     * @param inPcmPath pcm文件的绝对路径
     * @param sampleRate pcm的采样率
     * @param nbChannels pcm的声道数
     * @param bytesPerSample pcm的采样深度,单位是字节,8位是一个字节，16位是2个字节
     * @return pcm播放时长,单位是毫秒, 小于0是获取失败
     */
    public static long getPcmDurationMs(String inPcmPath, int sampleRate, int nbChannels, int bytesPerSample)
    {
        long ret = -1;
        if (null == inPcmPath || sampleRate <= 0
                || nbChannels <= 0 || bytesPerSample <= 0) return ret;

        File f= new File(inPcmPath);
        if (f.exists() && f.isFile()) {
            ret = (long) ((f.length() / (float)sampleRate / (float)nbChannels / (float)bytesPerSample) * 1000);
        }else {
            Log.e(TAG, "file doesn't exist or is not a file");
            ret = -1;
        }

        return ret;
    }

    private static long align(long x, int align) {
        return ((( x ) + (align) - 1) / (align) * (align));
    }

    /**
     * 对pcm文件裁剪
     * @param inPcmPath 输入pcm文件路径
     * @param sampleRate pcm采样率
     * @param nbChannels pcm声道数
     * @param bytesPerSample pcm的采样深度,单位是字节,8位是一个字节，16位是2个字节
     * @param startTimeMs 裁剪开始位置，单位是毫秒
     * @param endTimeMs 裁剪结束位置，单位是毫秒
     * @param outPcmPath 输出pcm文件路径
     * @return true|false，false: 失败 true: 成功
     */
    public static boolean pcmFileCrop(String inPcmPath, int sampleRate, int nbChannels, int bytesPerSample,
                                      long startTimeMs, long endTimeMs, String outPcmPath)
    {
        if (null == inPcmPath || sampleRate <= 0
                || nbChannels <= 0|| bytesPerSample <= 0 || outPcmPath == null) return false;

        long duration = XmPcmUtils.getPcmDurationMs(inPcmPath, sampleRate, nbChannels, bytesPerSample);
        if (duration <= 0) return false;

        startTimeMs = startTimeMs < 0 ? 0 : (startTimeMs > duration ? duration : startTimeMs);
        endTimeMs = endTimeMs < 0 ? 0 : (endTimeMs > duration ? duration : endTimeMs);

        int blockAlign = nbChannels * bytesPerSample;
        long startPos = align((long) ((startTimeMs /(float) 1000) * sampleRate * nbChannels * bytesPerSample),
                blockAlign);
        long endPos = align((long) ((endTimeMs /(float) 1000) * sampleRate * nbChannels * bytesPerSample),
                blockAlign);
        long curPos = 0;

        File inFile = new File(inPcmPath);
        BufferedInputStream bis = null;
        File outFile = new File(outPcmPath);
        BufferedOutputStream bos = null;
        try {
            if (!outFile.getParentFile().exists()) {
                if (!outFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "pcmFileCrop : mkdir " + outFile + " create failed");
                    return false;
                }
            }
            if (!outFile.exists()) {
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "pcmFileCrop : " + outFile + " create failed");
                    return false;
                }
            } else {
                outFile.delete();
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "pcmFileCrop : " + outFile + " create failed");
                    return false;
                }
            }
            outFile.setReadable(true, false);
            outFile.setWritable(true, false);

            bis = new BufferedInputStream(new FileInputStream(inFile));
            bos = new BufferedOutputStream(new FileOutputStream(outFile, false));
            byte[] temp = new byte[1024];
            int length = 0;
            while (curPos < endPos) {
                if ((length = bis.read(temp)) <= 0) break;

                curPos += length;
                if (curPos >= startPos) {
                    if (curPos <= endPos) {
                        int offset = ((curPos - length) - startPos) >= 0 ? 0 : (int) (startPos - (curPos - length));
                        bos.write(temp, offset, length - offset);
                    } else {
                        bos.write(temp, 0, (int) (length - (curPos - endPos)));
                    }
                }
            }

            bis.close();
            bis = null;
            bos.flush();
            bos.close();
            bos = null;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        } finally {
            try {
                if (bis != null)
                    bis.close();
                bis = null;
                if (bos != null) {
                    bos.flush();
                    bos.close();
                }
                bos = null;
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }
        }

        return true;
    }

    /**
     * 对pcm文件拼接
     * @param inPcmPathList 需要拼接的pcm文件路径List
     * @param sampleRate pcm采样率
     * @param nbChannels pcm声道数
     * @param bytesPerSample pcm的采样深度,单位是字节,8位是一个字节，16位是2个字节
     * @param outPcmPath 输出pcm文件路径
     * @return true|false，false: 失败, true: 成功
     */
    public static boolean pcmFileConcat(ArrayList<String> inPcmPathList, int sampleRate,
                                        int nbChannels, int bytesPerSample, String outPcmPath) {
        if (null == inPcmPathList || sampleRate <= 0
                || nbChannels <= 0 || bytesPerSample <= 0 || outPcmPath == null) return false;

        if (inPcmPathList.size() <= 0) return false;

        File outFile = new File(outPcmPath);
        BufferedOutputStream bos = null;
        File inFile = null;
        BufferedInputStream bis = null;
        try {
            if (!outFile.getParentFile().exists()) {
                if (!outFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "pcmFileConcat : mkdir " + outFile + " create failed");
                    return false;
                }
            }
            if (!outFile.exists()) {
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "pcmFileConcat : " + outFile + " create failed");
                    return false;
                }
            } else {
                outFile.delete();
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "pcmFileConcat : " + outFile + " create failed");
                    return false;
                }
            }
            outFile.setReadable(true, false);
            outFile.setWritable(true, false);

            bos = new BufferedOutputStream(new FileOutputStream(outFile, false));

            byte[] temp = new byte[1024];
            int length = 0;
            for (int i = 0; i < inPcmPathList.size(); i++) {
                inFile = new File(inPcmPathList.get(i));
                bis = new BufferedInputStream(new FileInputStream(inFile));
                while (true) {
                    if ((length = bis.read(temp)) <= 0) break;
                    bos.write(temp, 0, length);
                }
                bis.close();
                bis = null;
            }

            bos.flush();
            bos.close();
            bos = null;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        } finally {
            try {
                if (bis != null)
                    bis.close();
                bis = null;
                if (bos != null) {
                    bos.flush();
                    bos.close();
                }
                bos = null;
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }
        }

        return true;
    }
}
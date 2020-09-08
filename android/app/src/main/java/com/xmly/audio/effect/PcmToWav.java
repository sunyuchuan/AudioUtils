package com.xmly.audio.effect;

import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

/**
 * Created by HXL on 16/8/11.
 * 将pcm文件转化为wav文件
 */
public class PcmToWav {
    private static final String TAG = "PcmToWav";
    private String mOutWavFilePath = null;
    private int mPcmSampleRate = 0;
    private int mPcmNbChannels = 0;
    private WaveHeader mWavHeader = null;
    private int mDataSize = 0;

    private File mOutFile = null;
    private BufferedOutputStream mBos = null;

    public PcmToWav() {}

    /**
     * init
     * @param pcmSampleRate input pcm data of sample rate
     * @param pcmNbChannels input pcm data of channels number
     * @param outWavFilePath output wav file path
     */
    public boolean init(int pcmSampleRate, int pcmNbChannels, String outWavFilePath) {
        if (outWavFilePath == null) {
            Log.e(TAG, "init : out Wav File Path is null");
            return false;
        }

        release();
        mOutWavFilePath = outWavFilePath;
        mPcmSampleRate = pcmSampleRate;
        mPcmNbChannels = pcmNbChannels;
        mWavHeader = new WaveHeader();
        mOutFile = new File(mOutWavFilePath);
        mDataSize = 0;

        // 填入参数，比特率\声道数等等.
        // 长度字段 = 内容的大小（TOTAL_SIZE) +
        // 头部字段的大小(不包括前面4字节的标识符RIFF以及fileLength本身的4字节)
        mWavHeader.fileLength = mDataSize + (44 - 8);
        mWavHeader.FmtHdrLeth = 16;
        mWavHeader.FormatTag = 0x0001;
        mWavHeader.Channels = (short) mPcmNbChannels;
        mWavHeader.SamplesPerSec = mPcmSampleRate;
        mWavHeader.BitsPerSample = 16;
        mWavHeader.BlockAlign = (short) (mWavHeader.Channels * mWavHeader.BitsPerSample / 8);
        mWavHeader.AvgBytesPerSec = mWavHeader.BlockAlign * mWavHeader.SamplesPerSec;
        mWavHeader.DataHdrLeth = mDataSize;

        try {
            byte[] wavHeader = mWavHeader.getHeader();
            if (wavHeader.length != 44) { // WAV标准，头部应该是44字节,如果不是44个字节则不进行转换文件
                Log.e(TAG, "init : wav header length is not 44 byte");
                return false;
            }

            if (!mOutFile.getParentFile().exists()) {
                if (!mOutFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "init : mkdir " + mOutFile + " create failed");
                    return false;
                }
            }
            if (!mOutFile.exists()) {
                if(!mOutFile.createNewFile()) {
                    Log.e(TAG, "init : " + mOutFile + " create failed");
                    return false;
                }
            } else {
                mOutFile.delete();
                if(!mOutFile.createNewFile()) {
                    Log.e(TAG, "init : " + mOutFile + " create failed");
                    return false;
                }
            }
            mOutFile.setReadable(true, false);
            mOutFile.setWritable(true, false);

            mBos = new BufferedOutputStream(new FileOutputStream(mOutFile, false));
            mBos.write(wavHeader, 0, wavHeader.length);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }

        return true;
    }

    /**
     * write pcm data
     * @param buffer data buffer
     * @param buffer_len data buffer of length
     * @return true|false
     */
    public boolean writePcmData(byte[] buffer, int buffer_len) {
        if (buffer == null || buffer_len <= 0) {
            Log.e(TAG, "writePcmData : buffer is null or buffer_len is 0");
            return false;
        }

        try {
            mBos.write(buffer, 0, buffer_len);
            mDataSize += buffer_len;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    /**
     * Rewrite wav header after writing pcm data
     * @return true|false
     */
    public boolean writeWavHeader() {
        // wav中的数据部分size已经改变，需要重新填写wav头中的data size.
        mWavHeader.fileLength = mDataSize + (44 - 8);
        mWavHeader.DataHdrLeth = mDataSize;
        byte[] wavHeader = null;
        try {
            if (mBos != null) {
                mBos.flush();
                mBos.close();
                mBos = null;
            }

            wavHeader = mWavHeader.getHeader();
            RandomAccessFile wavfile = new RandomAccessFile(mOutFile, "rw");
            wavfile.seek(0);
            wavfile.write(wavHeader, 0, wavHeader.length);
            wavfile.close();
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "writeWavHeader : write wav header failed");
            return false;
        }

        return true;
    }

    public void release() {
        try {
            if (mBos != null) {
                mBos.flush();
                mBos.close();
            }
            mBos = null;
            mOutFile = null;
            mWavHeader = null;
            mOutWavFilePath = null;
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void finalize() throws Throwable {
        Log.i(TAG, "finalize");
        try {
            release();
        } finally {
            super.finalize();
        }
    }

    /**
     * 将pcm文件转为一个wav文件
     *
     * @param pcmFilePath pcm文件路径集合
     * @param destPath 目标wav文件路径
     * @return true|false
     */
    public static boolean PCMFilesToWAVFile(String pcmFilePath, int pcmSampleRate,
                                            int pcmNbChannels, String destPath) {
        if (pcmFilePath == null || destPath == null) {
            Log.e(TAG, "PCMFilesToWAVFile : pcmFilePath or destPath is null");
            return false;
        }

        File inFile = new File(pcmFilePath);
        BufferedInputStream bis = null;
        File outFile = new File(destPath);
        BufferedOutputStream bos = null;
        int DATA_SIZE = 0;

        // 填入参数，比特率等等。这里用的是16位单声道 441000 hz
        WaveHeader header = new WaveHeader();
        // 长度字段 = 内容的大小（TOTAL_SIZE) +
        // 头部字段的大小(不包括前面4字节的标识符RIFF以及fileLength本身的4字节)
        header.fileLength = DATA_SIZE + (44 - 8);
        header.FmtHdrLeth = 16;
        header.FormatTag = 0x0001;
        header.Channels = (short) pcmNbChannels;
        header.SamplesPerSec = pcmSampleRate;
        header.BitsPerSample = 16;
        header.BlockAlign = (short) (header.Channels * header.BitsPerSample / 8);
        header.AvgBytesPerSec = header.BlockAlign * header.SamplesPerSec;
        header.DataHdrLeth = DATA_SIZE;

        try {
            byte[] wavHeader = header.getHeader();
            if (wavHeader.length != 44) { // WAV标准，头部应该是44字节,如果不是44个字节则不进行转换文件
                Log.e(TAG, "PCMFilesToWAVFile : wav header length is not 44 byte");
                return false;
            }

            if (!outFile.getParentFile().exists()) {
                if (!outFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "PCMFilesToWAVFile : mkdir " + outFile + " create failed");
                    return false;
                }
            }
            if (!outFile.exists()) {
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "PCMFilesToWAVFile : " + outFile + " create failed");
                    return false;
                }
            } else {
                outFile.delete();
                if(!outFile.createNewFile()) {
                    Log.e(TAG, "PCMFilesToWAVFile : " + outFile + " create failed");
                    return false;
                }
            }
            outFile.setReadable(true, false);
            outFile.setWritable(true, false);

            bis = new BufferedInputStream(new FileInputStream(inFile));
            bos = new BufferedOutputStream(new FileOutputStream(outFile, false));
            byte[] temp = new byte[1024];
            int length = 0;
            bos.write(wavHeader, 0, wavHeader.length);
            while ((length = bis.read(temp)) > 0) {
                bos.write(temp, 0, length);
                DATA_SIZE += length;
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

        try {
            // wav中的数据部分size已经改变，需要重新填写wav头中的data size.
            header.fileLength = DATA_SIZE + (44 - 8);
            header.DataHdrLeth = DATA_SIZE;
            byte[] wavHeader = header.getHeader();
            RandomAccessFile wavfile = new RandomAccessFile(outFile, "rw");
            wavfile.seek(0);
            wavfile.write(wavHeader, 0, wavHeader.length);
            wavfile.close();
        } catch (IOException e) {
            e.printStackTrace();
            Log.e(TAG, "PCMFilesToWAVFile : write wav header failed");
            return false;
        }

        Log.i(TAG, "PCMFilesToWAVFile  success!");
        return true;
    }
}

package com.xmly.audio.utils;

import android.util.Log;

/**
 * Created by sunyc on 20-6-2.
 */

public class FadeInPcm {
    private static final String TAG = "FadeInPcm";
    private int mNbChannels = 0;
    private int mBitsPerSample = 0;
    private long mFadeInNbSamples = 0;
    private long mFadeInSamplesCount = 0;
    private boolean mFadingIn = false;
    private int mRate = 0;

    /**
     * init params of fade in
     * @param sampleRate sampling rate of input pcm data
     * @param nbChannels number of channels of input pcm data
     * @param bitsPerSample sampling depth of input pcm data
     * @param fadeInTimeMs fade in time that unit is milliseconds
     */
    public void init(int sampleRate, int nbChannels,
                     int bitsPerSample, int fadeInTimeMs) {
        this.mNbChannels = nbChannels;
        this.mBitsPerSample = bitsPerSample;
        this.mFadeInNbSamples = (long)fadeInTimeMs * (long)sampleRate / 1000;
        this.mFadeInSamplesCount = 0;
        this.mFadingIn = true;
        this.mRate = 0;
    }

    /**
     * fade in,bits per sample is 16
     * @param buffer the buffer containing pcm data
     * @param bufferSizeInShort length of pcm data
     * @return false : end of fade in
     * @throws IllegalArgumentException
     */
    public boolean fadeIn16Bits(short[] buffer, int bufferSizeInShort) throws IllegalArgumentException {
        if (buffer == null || bufferSizeInShort <= 0) {
            Log.e(TAG, "buffer is null or bufferSizeInShort <= 0, exit");
            throw new IllegalArgumentException();
        }

        if (mBitsPerSample != 16) {
            Log.e(TAG, "bits per sample is not 16, exit");
            throw new IllegalArgumentException();
        }

        if (mNbChannels != 1 && mNbChannels != 2) {
            Log.e(TAG, "number of channels is invalid, exit");
            throw new IllegalArgumentException();
        }

        if (mFadingIn) {
            if (mFadeInSamplesCount >= mFadeInNbSamples) {
                mRate = 32767;
                mFadingIn = false;
                return mFadingIn;
            }

            for (int i = 0; i < bufferSizeInShort; i++) {
                buffer[i] = (short) (buffer[i] * mRate >> 15);
                if (mNbChannels == 2 && (i + 1) < bufferSizeInShort) {
                    buffer[i + 1] = (short) (buffer[i + 1] * mRate >> 15);
                    i++;
                }

                mFadeInSamplesCount ++;

                if (mFadeInSamplesCount >= mFadeInNbSamples) {
                    mRate = 32767;
                    mFadingIn = false;
                    return mFadingIn;
                } else {
                    mRate = (int) (32767 * (double) mFadeInSamplesCount / (double)mFadeInNbSamples);
                    mFadingIn = true;
                }
            }
        }

        return mFadingIn;
    }

//    /**
//     * fade in,bits per sample is 8
//     * @param buffer the buffer containing pcm data
//     * @param bufferSizeInByte length of pcm data
//     * @return false : end of fade in
//     * @throws IllegalArgumentException
//     */
//    public boolean fadeIn8Bits(byte[] buffer, int bufferSizeInByte) throws IllegalArgumentException {
//        if (buffer == null || bufferSizeInByte <= 0) {
//            Log.e(TAG, "buffer is null or bufferSizeInShort <= 0, exit");
//            throw new IllegalArgumentException();
//        }
//
//        if (mBitsPerSample != 8) {
//            Log.e(TAG, "bits per sample is not 8, exit");
//            throw new IllegalArgumentException();
//        }
//
//        if (mNbChannels != 1 && mNbChannels != 2) {
//            Log.e(TAG, "number of channels is invalid, exit");
//            throw new IllegalArgumentException();
//        }
//
//        if (mFadingIn) {
//            if (mFadeInSamplesCount >= mFadeInNbSamples) {
//                mRate = 127;
//                mFadingIn = false;
//                return mFadingIn;
//            }
//
//            for (int i = 0; i < bufferSizeInByte; i++) {
//                buffer[i] = (byte) (buffer[i] * mRate >> 7);
//                if (mNbChannels == 2 && (i + 1) < bufferSizeInByte) {
//                    buffer[i + 1] = (byte) (buffer[i + 1] * mRate >> 7);
//                    i++;
//                }
//
//                mFadeInSamplesCount ++;
//
//                if (mFadeInSamplesCount >= mFadeInNbSamples) {
//                    mRate = 127;
//                    mFadingIn = false;
//                    return mFadingIn;
//                } else {
//                    mRate = (int) (127 * (double) mFadeInSamplesCount / (double)mFadeInNbSamples);
//                    mFadingIn = true;
//                }
//            }
//        }
//
//        return mFadingIn;
//    }

}

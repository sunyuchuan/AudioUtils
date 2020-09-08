package com.xmly.audio.effect;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Utils {
    public static short[] toShortArray(byte[] src) {

        int count = src.length >> 1;
        short[] dest = new short[count];
        for (int i = 0; i < count; i++) {
            dest[i] = (short) (src[i * 2] << 8 | src[2 * i + 1] & 0xff);
        }
        return dest;
    }

    public static byte[] toByteArray(short[] src) {

        int count = src.length;
        byte[] dest = new byte[count << 1];
        for (int i = 0; i < count; i++) {
            dest[i * 2] = (byte) (src[i] >> 8);
            dest[i * 2 + 1] = (byte) (src[i] >> 0);
        }

        return dest;
    }

    public static short[] toShortArrayInBigEnd(byte[] b) {
        if (b == null || b.length % 2 != 0)
            throw new IllegalArgumentException("无法转换数组，输入参数错误：b == null or b.length % 2 != 0");

        short[] s = new short[b.length >> 1];
        ByteBuffer.wrap(b).order(ByteOrder.BIG_ENDIAN).asShortBuffer().get(s);
        return s;
    }

    public static short[] getShortArrayInLittleOrder(byte[] b, int offset, int length) {
        if (b == null || (length-offset) % 2 != 0)
            throw new IllegalArgumentException("无法转换数组，输入参数错误：b == null or b.length % 2 != 0");

        short[] s = new short[b.length >> 1];
        ByteBuffer.wrap(b, offset, length).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(s);
        return s;
    }

    public static short[] getShortArrayInLittleOrder(byte[] b) {
        if (b == null || b.length % 2 != 0)
            throw new IllegalArgumentException("无法转换数组，输入参数错误：b == null or b.length % 2 != 0");

        short[] s = new short[b.length >> 1];
        ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(s);
        return s;
    }

    public static byte[] getByteArrayInLittleOrder(short[] shorts) {
        if(shorts==null){
            return null;
        }
        byte[] bytes = new byte[shorts.length * 2];
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().put(shorts);
        return bytes;
    }
}

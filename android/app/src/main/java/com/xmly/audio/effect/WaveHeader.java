package com.xmly.audio.effect;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

/**
 * Created by HXL on 16/3/9.
 * wav文件头
 */

public class WaveHeader {
    public final char fileID[] = {'R', 'I', 'F', 'F'};
    public int fileLength;
    public final char wavTag[] = {'W', 'A', 'V', 'E'};

    public final char FmtHdrID[] = {'f', 'm', 't', ' '};
    public int FmtHdrLeth;
    public short FormatTag;
    public short Channels;
    public int SamplesPerSec;
    public int AvgBytesPerSec;
    public short BlockAlign;
    public short BitsPerSample;
    public final char DataHdrID[] = {'d', 'a', 't', 'a'};
    public int DataHdrLeth;

    public byte[] getHeader() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        WriteChar(bos, fileID);
        WriteInt(bos, fileLength);
        WriteChar(bos, wavTag);
        WriteChar(bos, FmtHdrID);
        WriteInt(bos, FmtHdrLeth);
        WriteShort(bos, FormatTag);
        WriteShort(bos, Channels);
        WriteInt(bos, SamplesPerSec);
        WriteInt(bos, AvgBytesPerSec);
        WriteShort(bos, BlockAlign);
        WriteShort(bos, BitsPerSample);
        WriteChar(bos, DataHdrID);
        WriteInt(bos, DataHdrLeth);
        bos.flush();
        byte[] r = bos.toByteArray();
        bos.close();
        return r;
    }

    private void WriteShort(ByteArrayOutputStream bos, short value) throws IOException {
        bos.write((byte) value);
        bos.write((byte) (value >>> 8));
    }

    private void WriteInt(ByteArrayOutputStream bos, int value) throws IOException {
        bos.write((byte) value);
        bos.write((byte) (value >>> 8));
        bos.write((byte) (value >>> 16));
        bos.write((byte) (value >>> 24));
    }

    private void WriteChar(ByteArrayOutputStream bos, char[] id) {
        for (int i = 0; i < id.length; i++) {
            char c = id[i];
            bos.write(c);
        }
    }
}
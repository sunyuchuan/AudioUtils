package com.xmly.audio.utils;

/**
 * Created by sunyc on 19-10-10.
 */

public interface LibLoader {
    void loadLibrary(String libName) throws UnsatisfiedLinkError,
            SecurityException;
}

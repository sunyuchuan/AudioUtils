package com.xmly.audio.effect;

import android.util.JsonReader;
import android.util.JsonWriter;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Created by sunyc on 19-10-8.
 */

public class JsonUtils {
    private static final String TAG = "JsonUtils";
    private static final String EFFECT_NAME = "name";
    private static final String EFFECT_INFO = "info";
    private static final String raw = "/sdcard/audio_effect_test/side_chain_test.wav";
    //private static final String raw = "/sdcard/audio_effect_test/pcm_mono_44kHz_0035.pcm";
    private static final String bgm1 = "/sdcard/audio_effect_test/bgm1.mp3";
    private static final String bgm2 = "/sdcard/audio_effect_test/bgm2.mp4";
    private static final String music1 = "/sdcard/audio_effect_test/audio_effects_1.mp3";
    private static final String music2 = "/sdcard/audio_effect_test/audio_effects_2.mp3";

    public static void generateJsonFile(String jsonPath, HashMap<String, String> effectsMap) {
        if (effectsMap == null || !createOutputFile(jsonPath)) {
            Log.e(TAG, "generateJsonFile: createOutputFile failed");
            return;
        }

        JSONArray jArray = new JSONArray();
        Iterator<HashMap.Entry<String, String>> iterator = effectsMap.entrySet().iterator();
        while (iterator.hasNext()) {
            HashMap.Entry<String, String> entry = iterator.next();
            jArray.put(generateJsonObject(entry.getKey(), entry.getValue()));
        }

        JSONArray recordArray = new JSONArray();
        JSONObject json = new JSONObject();
        try {
            json.put("file_path", raw);
            json.put("isPcm", "false");
            json.put("volume", 80);
            json.put("cropStartTimeMs", 10000);
            json.put("cropEndTimeMs", 100000);
            json.put("sampleRate", 44100);
            json.put("nbChannels", 1);
            json.put("startTimeMs", 10000);
            json.put("endTimeMs", 180000);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        recordArray.put(json);

        try {
            FileOutputStream fileOutputStream = new FileOutputStream(jsonPath);
            JsonWriter jsonWriter = new JsonWriter(new OutputStreamWriter(fileOutputStream, "UTF-8"));
            jsonWriter.beginObject();
            jsonWriter.name("effects").value(jArray.toString());
            jsonWriter.name("record").value(recordArray.toString());
            jsonWriter.endObject();
            jsonWriter.close();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {

        }
    }

    public static void readJsonFile(String jsonPath) {
        try {
            FileInputStream fileInputStream = new FileInputStream(jsonPath);
            JsonReader read = new JsonReader(new InputStreamReader(fileInputStream));
            read.beginObject();
            while (read.hasNext()) {
                String key = read.nextName();
                String value = read.nextString();
                Log.i(TAG, "key : " + key + " ,value : " + value);
                if (key.equals("effects")) {
                    JSONArray jarray = new JSONArray(value);
                    for (int i = 0; i < jarray.length(); i++) {
                        Log.i(TAG, "effects["+i+"] : " + jarray.getJSONObject(i).toString());
                    }
                }
            }
            read.endObject();
        } catch (IOException e) {
            e.printStackTrace();
        } catch (JSONException e) {
            e.printStackTrace();
        } finally {

        }
    }

    private static JSONObject generateJsonObject(String name, String info) {
        if (name == null || info == null) {
            return null;
        }

        JSONObject json = new JSONObject();
        try {
            json.put(EFFECT_NAME, name);
            json.put(EFFECT_INFO, info);
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        }

        return json;
    }

    public static boolean createOutputFile(String jsonPath) {
        File outFile = new File(jsonPath);
        try {
            if (!outFile.getParentFile().exists()) {
                if (!outFile.getParentFile().mkdirs()) {
                    Log.e(TAG, "createJsonFile : mkdir " + outFile + " create failed");
                    return false;
                }
            }
            if (!outFile.exists()) {
                if (!outFile.createNewFile()) {
                    Log.e(TAG, "createJsonFile : " + outFile + " create failed");
                    return false;
                }
            } else {
                outFile.delete();
                if (!outFile.createNewFile()) {
                    Log.e(TAG, "createJsonFile : " + outFile + " create failed");
                    return false;
                }
            }
            outFile.setReadable(true, false);
            outFile.setWritable(true, false);
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        } finally {

        }

        return true;
    }
}

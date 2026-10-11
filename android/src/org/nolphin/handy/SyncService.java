package org.nolphin.handy;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.ClipData;
import android.content.ContentValues;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.IBinder;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Hält die Verbindung zu Nolphin am PC: wartet, bis der PC eine Datei in
 * "Zum Handy" legt, lädt sie nach Downloads/Nolphin und sendet auf Wunsch
 * Dateien vom Handy an den PC.
 */
public class SyncService extends Service {
    static final String ACTION_STOP = "org.nolphin.handy.STOP";
    static final String ACTION_UPLOAD = "org.nolphin.handy.UPLOAD";
    static final String ACTION_ALL = "org.nolphin.handy.ALL";

    private static final String CH_STATE = "state";
    private static final String CH_FILES = "files";
    private static final int ID_STATE = 1;

    /** Zustand für die Anzeige in MainActivity. */
    static volatile String status = "Nicht verbunden";
    private static final ArrayDeque<String> events = new ArrayDeque<>();

    static synchronized void event(String text) {
        events.addFirst(text);
        while (events.size() > 30) {
            events.removeLast();
        }
    }

    static synchronized String eventsText() {
        StringBuilder b = new StringBuilder();
        for (String e : events) {
            b.append(e).append('\n');
        }
        return b.toString();
    }

    private volatile boolean stop;
    private volatile boolean forceAll;
    private Thread worker;
    private int nextNotification = 100;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        createChannels();
        String action = intent != null ? intent.getAction() : null;
        if (ACTION_STOP.equals(action)) {
            stop = true;
            status = "Nicht verbunden";
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
            return START_NOT_STICKY;
        }
        startForeground(ID_STATE, stateNotification(status), ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        if (ACTION_ALL.equals(action)) {
            forceAll = true;
        } else if (ACTION_UPLOAD.equals(action) && intent != null) {
            final List<Uri> uris = urisOf(intent);
            new Thread(new Runnable() {
                @Override
                public void run() {
                    upload(uris);
                }
            }, "nolphin-upload").start();
        }
        if (worker == null || !worker.isAlive()) {
            stop = false;
            worker = new Thread(new Runnable() {
                @Override
                public void run() {
                    loop();
                }
            }, "nolphin-sync");
            worker.start();
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        stop = true;
        super.onDestroy();
    }

    // ---- Benachrichtigungen -------------------------------------------------------------

    private void createChannels() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel(CH_STATE, "Verbindung", NotificationManager.IMPORTANCE_LOW));
        nm.createNotificationChannel(new NotificationChannel(CH_FILES, "Dateien", NotificationManager.IMPORTANCE_DEFAULT));
    }

    private Notification stateNotification(String text) {
        return new Notification.Builder(this, CH_STATE)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentTitle("Nolphin Handy")
                .setContentText(text)
                .setOngoing(true)
                .build();
    }

    private void setStatus(String text) {
        if (!text.equals(status)) {
            status = text;
            getSystemService(NotificationManager.class).notify(ID_STATE, stateNotification(text));
        }
    }

    private void notifyFile(String title, String text) {
        Notification n = new Notification.Builder(this, CH_FILES)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentTitle(title)
                .setContentText(text)
                .setAutoCancel(true)
                .build();
        getSystemService(NotificationManager.class).notify(nextNotification++, n);
        event(title + ": " + text);
    }

    // ---- PC -> Handy -----------------------------------------------------------------------

    private void loop() {
        String version = "";
        while (!stop) {
            String host = Conn.host(this);
            String token = Conn.token(this);
            if (host == null || token == null) {
                setStatus("Nicht verbunden");
                return;
            }
            try {
                URL url = new URL("http://" + host + "/wait?t=" + Uri.encode(token) + "&v=" + Uri.encode(version));
                HttpURLConnection c = (HttpURLConnection) url.openConnection();
                c.setConnectTimeout(8000);
                c.setReadTimeout(40000);
                int code = c.getResponseCode();
                if (code == 403) {
                    c.disconnect();
                    setStatus("Schlüssel abgelaufen: QR-Code am PC neu scannen");
                    sleep(5000);
                    continue;
                }
                if (code != 200) {
                    c.disconnect();
                    setStatus("PC antwortet mit Fehler " + code);
                    sleep(5000);
                    continue;
                }
                JSONObject o = new JSONObject(readAll(c.getInputStream()));
                c.disconnect();
                version = o.getString("v");
                setStatus("Verbunden mit " + host);
                handleFiles(host, token, o.getJSONArray("files"));
            } catch (Exception e) {
                setStatus("PC nicht erreichbar, neuer Versuch …");
                version = "";
                sleep(5000);
            }
        }
    }

    private void handleFiles(String host, String token, JSONArray files) throws Exception {
        Set<String> seen = Conn.seen(this);
        boolean first = !Conn.seeded(this);
        boolean all = forceAll;
        forceAll = false;
        for (int i = 0; i < files.length() && !stop; i++) {
            JSONObject f = files.getJSONObject(i);
            String name = f.getString("n");
            long size = f.getLong("s");
            String key = name + "|" + size;
            if (first) {
                seen.add(key);   // beim ersten Verbinden nichts Altes laden
                continue;
            }
            if (!all && seen.contains(key)) {
                continue;
            }
            if (download(host, token, name)) {
                seen.add(key);
                Conn.saveSeen(this, seen);
                notifyFile("Vom PC empfangen", name);
            } else {
                notifyFile("Laden fehlgeschlagen", name);
            }
        }
        if (first) {
            Conn.setSeeded(this);
        }
        Conn.saveSeen(this, seen);
    }

    private boolean download(String host, String token, String name) {
        Uri target = null;
        try {
            URL url = new URL("http://" + host + "/dl?t=" + Uri.encode(token) + "&n=" + Uri.encode(name));
            HttpURLConnection c = (HttpURLConnection) url.openConnection();
            c.setConnectTimeout(8000);
            c.setReadTimeout(30000);
            if (c.getResponseCode() != 200) {
                c.disconnect();
                return false;
            }
            ContentValues cv = new ContentValues();
            cv.put(MediaStore.Downloads.DISPLAY_NAME, name);
            cv.put(MediaStore.Downloads.MIME_TYPE, mimeOf(name));
            cv.put(MediaStore.Downloads.RELATIVE_PATH, "Download/Nolphin");
            cv.put(MediaStore.Downloads.IS_PENDING, 1);
            target = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
            if (target == null) {
                c.disconnect();
                return false;
            }
            try (InputStream in = c.getInputStream(); OutputStream out = getContentResolver().openOutputStream(target)) {
                copy(in, out);
            }
            c.disconnect();
            cv.clear();
            cv.put(MediaStore.Downloads.IS_PENDING, 0);
            getContentResolver().update(target, cv, null, null);
            return true;
        } catch (Exception e) {
            if (target != null) {
                getContentResolver().delete(target, null, null);
            }
            return false;
        }
    }

    // ---- Handy -> PC -----------------------------------------------------------------------

    private List<Uri> urisOf(Intent intent) {
        List<Uri> list = new ArrayList<>();
        ClipData clip = intent.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u != null) {
                    list.add(u);
                }
            }
        }
        return list;
    }

    private void upload(List<Uri> uris) {
        String host = Conn.host(this);
        String token = Conn.token(this);
        if (host == null || token == null) {
            notifyFile("Senden nicht möglich", "Erst den QR-Code vom PC scannen");
            return;
        }
        for (Uri u : uris) {
            String name = displayName(u);
            long size = sizeOf(u);
            try {
                URL url = new URL("http://" + host + "/up?t=" + Uri.encode(token) + "&n=" + Uri.encode(name));
                HttpURLConnection c = (HttpURLConnection) url.openConnection();
                c.setConnectTimeout(8000);
                c.setReadTimeout(60000);
                c.setRequestMethod("POST");
                c.setDoOutput(true);
                if (size < 0) {
                    throw new IOException("Größe unbekannt");
                }
                c.setFixedLengthStreamingMode(size);
                try (InputStream in = getContentResolver().openInputStream(u); OutputStream out = c.getOutputStream()) {
                    copy(in, out);
                }
                int code = c.getResponseCode();
                c.disconnect();
                if (code == 200) {
                    notifyFile("An den PC gesendet", name);
                } else {
                    notifyFile("Senden fehlgeschlagen (" + code + ")", name);
                }
            } catch (Exception e) {
                notifyFile("Senden fehlgeschlagen", name);
            }
        }
    }

    private String displayName(Uri u) {
        String name = null;
        try (Cursor cur = getContentResolver().query(u, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cur != null && cur.moveToFirst()) {
                name = cur.getString(0);
            }
        } catch (Exception ignored) {
        }
        if (name == null || name.isEmpty()) {
            name = u.getLastPathSegment();
        }
        return name != null ? name : "Datei";
    }

    private long sizeOf(Uri u) {
        try (Cursor cur = getContentResolver().query(u, new String[] { OpenableColumns.SIZE }, null, null, null)) {
            if (cur != null && cur.moveToFirst() && !cur.isNull(0)) {
                return cur.getLong(0);
            }
        } catch (Exception ignored) {
        }
        return -1;
    }

    // ---- Hilfen ----------------------------------------------------------------------------

    private static String mimeOf(String name) {
        int dot = name.lastIndexOf('.');
        if (dot >= 0 && dot < name.length() - 1) {
            String m = MimeTypeMap.getSingleton().getMimeTypeFromExtension(name.substring(dot + 1).toLowerCase(Locale.ROOT));
            if (m != null) {
                return m;
            }
        }
        return "application/octet-stream";
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }
        out.flush();
    }

    private static String readAll(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream b = new java.io.ByteArrayOutputStream();
        copy(in, b);
        return b.toString("UTF-8");
    }

    private void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ignored) {
        }
    }
}

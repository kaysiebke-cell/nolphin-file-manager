package org.nolphin.handy;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;

import java.util.HashSet;
import java.util.Set;

/** Gespeicherte Verbindung zum PC und die Liste der schon geladenen Dateien. */
final class Conn {
    private Conn() {}

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("nolphin", Context.MODE_PRIVATE);
    }

    /** "host:port" des PCs oder null. */
    static String host(Context c) {
        return prefs(c).getString("host", null);
    }

    static String token(Context c) {
        return prefs(c).getString("token", null);
    }

    static boolean connected(Context c) {
        return host(c) != null && token(c) != null;
    }

    static void clear(Context c) {
        prefs(c).edit().remove("host").remove("token").apply();
    }

    /**
     * Liest einen Verbindungslink: nolphin://connect?host=H&t=T oder den Link
     * aus dem QR-Code (http://H/?t=T). Gibt false zurück, wenn er nicht passt.
     */
    static boolean parse(Context c, String link) {
        if (link == null) {
            return false;
        }
        Uri u = Uri.parse(link.trim());
        String scheme = u.getScheme();
        String host = null;
        if ("nolphin".equals(scheme)) {
            host = u.getQueryParameter("host");
        } else if ("http".equals(scheme)) {
            host = u.getAuthority();
        }
        String token = u.getQueryParameter("t");
        if (host == null || host.isEmpty() || token == null || token.isEmpty()) {
            return false;
        }
        prefs(c).edit().putString("host", host).putString("token", token).apply();
        return true;
    }

    /** Dateien, die schon auf dem Handy sind (Name und Größe). */
    static Set<String> seen(Context c) {
        return new HashSet<>(prefs(c).getStringSet("seen", new HashSet<String>()));
    }

    static void saveSeen(Context c, Set<String> seen) {
        prefs(c).edit().putStringSet("seen", new HashSet<>(seen)).apply();
    }

    /** Beim allerersten Verbinden werden vorhandene Dateien nicht geladen. */
    static boolean seeded(Context c) {
        return prefs(c).getBoolean("seeded", false);
    }

    static void setSeeded(Context c) {
        prefs(c).edit().putBoolean("seeded", true).apply();
    }
}

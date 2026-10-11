package org.nolphin.handy;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/** Verbindungsstatus und die wenigen Bedienelemente der App. */
public class MainActivity extends Activity {
    private static final int PICK_FILES = 7;

    private TextView statusView;
    private TextView logView;
    private EditText linkField;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable refresh = new Runnable() {
        @Override
        public void run() {
            update();
            handler.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        buildUi();
        if (Build.VERSION.SDK_INT >= 33) {
            requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, 1);
        }
        handleIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (Conn.connected(this)) {
            startSync(null);   // nach einem Neustart des Handys wieder aufnehmen
        }
        handler.post(refresh);
    }

    @Override
    protected void onPause() {
        handler.removeCallbacks(refresh);
        super.onPause();
    }

    private void handleIntent(Intent intent) {
        Uri data = intent != null ? intent.getData() : null;
        if (data != null && "nolphin".equals(data.getScheme())) {
            if (Conn.parse(this, data.toString())) {
                Toast.makeText(this, "Mit dem PC verbunden", Toast.LENGTH_SHORT).show();
                startSync(null);
            } else {
                Toast.makeText(this, "Ungültiger Verbindungslink", Toast.LENGTH_LONG).show();
            }
        }
    }

    private void startSync(String action) {
        Intent i = new Intent(this, SyncService.class);
        if (action != null) {
            i.setAction(action);
        }
        startForegroundService(i);
    }

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density);
    }

    private Button button(String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setOnClickListener(l);
        return b;
    }

    private void buildUi() {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(20), dp(24), dp(20), dp(24));

        TextView title = new TextView(this);
        title.setText("Nolphin Handy");
        title.setTextSize(24);
        box.addView(title);

        statusView = new TextView(this);
        statusView.setTextSize(16);
        statusView.setPadding(0, dp(12), 0, dp(12));
        box.addView(statusView);

        box.addView(button("Dateien an den PC senden …", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!Conn.connected(MainActivity.this)) {
                    Toast.makeText(MainActivity.this, "Erst den QR-Code vom PC scannen", Toast.LENGTH_LONG).show();
                    return;
                }
                Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                pick.addCategory(Intent.CATEGORY_OPENABLE);
                pick.setType("*/*");
                pick.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                startActivityForResult(pick, PICK_FILES);
            }
        }));
        box.addView(button("Alle Dateien vom PC laden", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startSync(SyncService.ACTION_ALL);
            }
        }));
        box.addView(button("Verbindung trennen", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Conn.clear(MainActivity.this);
                startSync(SyncService.ACTION_STOP);
                update();
            }
        }));

        linkField = new EditText(this);
        linkField.setHint("Link vom PC (falls der QR-Code nicht geht)");
        linkField.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        box.addView(linkField);
        box.addView(button("Mit Link verbinden", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (Conn.parse(MainActivity.this, linkField.getText().toString())) {
                    linkField.setText("");
                    startSync(null);
                } else {
                    Toast.makeText(MainActivity.this, "Ungültiger Link", Toast.LENGTH_LONG).show();
                }
            }
        }));

        logView = new TextView(this);
        logView.setTextSize(13);
        logView.setPadding(0, dp(16), 0, 0);
        box.addView(logView);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(box);
        setContentView(scroll);
    }

    private void update() {
        String host = Conn.host(this);
        statusView.setText(host == null
                ? "Nicht verbunden.\nScanne am PC in Nolphin (Seitenleiste „Handy“) den QR-Code."
                : SyncService.status + "\nPC: " + host);
        logView.setText(SyncService.eventsText());
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_FILES || result != RESULT_OK || data == null) {
            return;
        }
        ClipData clip = data.getClipData();
        ClipData out = null;
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (out == null) {
                    out = ClipData.newUri(getContentResolver(), "Dateien", u);
                } else {
                    out.addItem(new ClipData.Item(u));
                }
            }
        } else if (data.getData() != null) {
            out = ClipData.newUri(getContentResolver(), "Datei", data.getData());
        }
        if (out == null) {
            return;
        }
        Intent i = new Intent(this, SyncService.class);
        i.setAction(SyncService.ACTION_UPLOAD);
        i.setClipData(out);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startForegroundService(i);
        Toast.makeText(this, "Wird an den PC gesendet", Toast.LENGTH_SHORT).show();
    }
}

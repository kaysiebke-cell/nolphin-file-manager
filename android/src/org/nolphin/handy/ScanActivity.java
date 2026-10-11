package org.nolphin.handy;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Bundle;
import android.view.Gravity;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Scannt den QR-Code von Nolphin mit der Kamera. Der Code wird mit dem eigenen
 * QR-Leser (QrDecoder) gelesen, ohne Bibliothek und ohne Google-Dienste.
 */
@SuppressWarnings("deprecation")
public class ScanActivity extends Activity implements TextureView.SurfaceTextureListener, Camera.PreviewCallback {
    private static final int REQUEST_CAMERA = 11;

    private TextureView texture;
    private TextView hint;
    private TextView diag;
    private int frames;
    private Camera camera;
    private Camera.Size previewSize;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final AtomicBoolean busy = new AtomicBoolean(false);
    private volatile boolean done;
    private String lastRejected = "";
    private long rejectedAt;
    private volatile boolean verifying;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(0xFF000000);

        texture = new TextureView(this);
        texture.setSurfaceTextureListener(this);
        root.addView(texture, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT, Gravity.CENTER));

        hint = new TextView(this);
        hint.setText("Richte die Kamera auf den QR-Code am PC\n(Nolphin, Seitenleiste „Handy“)");
        hint.setTextColor(0xFFFFFFFF);
        hint.setBackgroundColor(0x99000000);
        hint.setTextSize(16);
        hint.setGravity(Gravity.CENTER);
        hint.setPadding(24, 24, 24, 24);
        root.addView(hint, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP));

        diag = new TextView(this);
        diag.setTextColor(0xFFAAAAAA);
        diag.setTextSize(12);
        diag.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams dlp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        dlp.bottomMargin = 200;
        root.addView(diag, dlp);

        Button cancel = new Button(this);
        cancel.setText("Abbrechen");
        cancel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        });
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        lp.bottomMargin = 64;
        root.addView(cancel, lp);
        setContentView(root);

        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] { Manifest.permission.CAMERA }, REQUEST_CAMERA);
        }
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        if (request == REQUEST_CAMERA) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
                if (texture.isAvailable()) {
                    openCamera(texture.getSurfaceTexture());
                }
            } else {
                Toast.makeText(this, "Ohne Kamera kann der QR-Code nicht gescannt werden. Du kannst den Link auch einfügen.",
                        Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    // ---- Kamera ----------------------------------------------------------------------------

    private void openCamera(SurfaceTexture surface) {
        if (camera != null || checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        try {
            camera = Camera.open();
            Camera.Parameters p = camera.getParameters();
            previewSize = bestSize(p.getSupportedPreviewSizes());
            p.setPreviewSize(previewSize.width, previewSize.height);
            List<String> focus = p.getSupportedFocusModes();
            if (focus != null && focus.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
                p.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            }
            p.setPreviewFormat(android.graphics.ImageFormat.NV21);
            camera.setParameters(p);
            previewSize = camera.getParameters().getPreviewSize();   // was die Kamera wirklich liefert
            camera.setDisplayOrientation(displayOrientation());
            camera.setPreviewTexture(surface);
            camera.setPreviewCallback(this);
            fitPreview();
            camera.startPreview();
        } catch (IOException | RuntimeException e) {
            Toast.makeText(this, "Kamera nicht verfügbar", Toast.LENGTH_LONG).show();
            closeCamera();
            finish();
        }
    }

    /** Vorschaugröße nahe 1280x720: genug Auflösung für den Code, noch schnell genug zum Lesen. */
    private static Camera.Size bestSize(List<Camera.Size> sizes) {
        Camera.Size best = sizes.get(0);
        long bestScore = Long.MAX_VALUE;
        for (Camera.Size s : sizes) {
            long score = Math.abs((long) s.width * s.height - 1280L * 720L);
            if (score < bestScore) {
                bestScore = score;
                best = s;
            }
        }
        return best;
    }

    private int displayOrientation() {
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(0, info);
        int rotation = getWindowManager().getDefaultDisplay().getRotation();
        int degrees = rotation * 90;
        return (info.orientation - degrees + 360) % 360;
    }

    /** Das Bild nicht verzerren: Vorschau im Seitenverhältnis der Kamera in die Mitte setzen. */
    private void fitPreview() {
        final int pw = previewSize.width, ph = previewSize.height;
        texture.post(new Runnable() {
            @Override
            public void run() {
                View parent = (View) texture.getParent();
                int w = parent.getWidth(), h = parent.getHeight();
                boolean portrait = h >= w;
                double aspect = portrait ? (double) pw / ph : (double) ph / pw;   // Höhe / Breite im Bild
                int viewW = w;
                int viewH = (int) Math.round(w * (portrait ? aspect : 1 / aspect));
                if (viewH > h) {
                    viewH = h;
                    viewW = (int) Math.round(h / (portrait ? aspect : 1 / aspect));
                }
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(viewW, viewH, Gravity.CENTER);
                texture.setLayoutParams(lp);
            }
        });
    }

    private void closeCamera() {
        if (camera != null) {
            camera.setPreviewCallback(null);
            try {
                camera.stopPreview();
            } catch (RuntimeException ignored) {
            }
            camera.release();
            camera = null;
        }
    }

    // ---- Bilder lesen ----------------------------------------------------------------------

    @Override
    public void onPreviewFrame(byte[] data, Camera cam) {
        if (done || previewSize == null || !busy.compareAndSet(false, true)) {
            return;   // läuft noch ein Lesevorgang: dieses Bild überspringen
        }
        final int w = previewSize.width, h = previewSize.height;
        if (data.length < w * h) {
            busy.set(false);
            return;
        }
        // NV21: die ersten w*h Bytes sind die Helligkeit, also schon ein Graustufenbild
        final byte[] gray = Arrays.copyOf(data, w * h);
        worker.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    final long t0 = System.nanoTime();
                    final String text = QrDecoder.decode(gray, w, h);
                    final long ms = (System.nanoTime() - t0) / 1000000;
                    final int seen = QrDecoder.lastFinderCount;
                    frames++;
                    final int count = frames;
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            diag.setText("Kamerabild " + w + "×" + h + " · gelesen: " + count + " · Suchmuster: " + seen + " · " + ms + " ms");
                        }
                    });
                    if (text == null) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                showProgress(seen);
                            }
                        });
                    }
                    if (text != null) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                accept(text);
                            }
                        });
                    }
                } finally {
                    busy.set(false);
                }
            }
        });
    }

    /** Sagt dem Nutzer, was die Kamera sieht, damit er das Handy richtig halten kann. */
    private void showProgress(int finders) {
        if (done || verifying || !lastRejected.isEmpty()) {
            return;
        }
        if (finders >= 3) {
            hint.setText("QR-Code erkannt, bitte ruhig halten …\n(etwas näher oder weiter weg, bis er scharf ist)");
        } else {
            hint.setText("Richte die Kamera auf den QR-Code am PC\n(Nolphin, Seitenleiste „Handy“), etwa eine Handbreit entfernt");
        }
    }

    private void accept(final String text) {
        if (done || verifying) {
            return;
        }
        if (text.equals(lastRejected) && System.currentTimeMillis() - rejectedAt < 4000) {
            return;   // derselbe abgelehnte Code: nicht bei jedem Bild erneut prüfen
        }
        verifying = true;
        hint.setText("QR-Code gelesen. Prüfe die Verbindung zum PC …");
        Connector.connect(this, text, new Connector.Listener() {
            @Override
            public void onSuccess(Link link) {
                done = true;
                Toast.makeText(ScanActivity.this, "Mit dem PC verbunden", Toast.LENGTH_SHORT).show();
                startForegroundService(new Intent(ScanActivity.this, SyncService.class));
                finish();
            }

            @Override
            public void onFailure(String message) {
                verifying = false;
                lastRejected = text;
                rejectedAt = System.currentTimeMillis();
                hint.setText(message + "\n\nRichte die Kamera auf den aktuellen QR-Code am PC.");
            }
        });
    }

    // ---- Lebenszyklus -----------------------------------------------------------------------

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        openCamera(surface);
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        closeCamera();
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
    }

    @Override
    protected void onPause() {
        closeCamera();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (texture.isAvailable() && camera == null) {
            openCamera(texture.getSurfaceTexture());
        }
    }

    @Override
    protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }
}

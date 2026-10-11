package org.nolphin.handy;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.ContentValues;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.provider.MediaStore;
import android.hardware.Camera;
import android.os.Bundle;
import android.view.Gravity;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStream;
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
    private static final int REQUEST_PHOTO = 21;
    private static final int REQUEST_PICK = 22;
    private Uri photoUri;

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

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.CENTER);
        buttons.addView(button("Foto aufnehmen", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                takePhoto();
            }
        }));
        buttons.addView(button("Bild wählen", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                pick.addCategory(Intent.CATEGORY_OPENABLE);
                pick.setType("image/*");
                startActivityForResult(pick, REQUEST_PICK);
            }
        }));
        buttons.addView(button("Abbrechen", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        }));
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        lp.bottomMargin = 48;
        root.addView(buttons, lp);
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

    private Button button(String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setOnClickListener(l);
        return b;
    }

    // ---- Foto statt Live-Bild --------------------------------------------------------------

    /** Die Kamera-App des Handys macht ein scharfes Foto; so klappt es auch, wenn die Live-Vorschau nicht reicht. */
    private void takePhoto() {
        ContentValues cv = new ContentValues();
        cv.put(MediaStore.Images.Media.DISPLAY_NAME, "nolphin-scan.jpg");
        cv.put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg");
        photoUri = getContentResolver().insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, cv);
        if (photoUri == null) {
            Toast.makeText(this, "Foto nicht möglich", Toast.LENGTH_LONG).show();
            return;
        }
        Intent i = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        i.putExtra(MediaStore.EXTRA_OUTPUT, photoUri);
        try {
            startActivityForResult(i, REQUEST_PHOTO);
        } catch (RuntimeException e) {
            getContentResolver().delete(photoUri, null, null);
            photoUri = null;
            Toast.makeText(this, "Keine Kamera-App gefunden", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        final Uri source;
        final boolean temporary;
        if (request == REQUEST_PHOTO) {
            source = photoUri;
            temporary = true;
            photoUri = null;
            if (result != RESULT_OK && source != null) {
                getContentResolver().delete(source, null, null);
                return;
            }
        } else if (request == REQUEST_PICK && result == RESULT_OK && data != null) {
            source = data.getData();
            temporary = false;
        } else {
            return;
        }
        if (source == null) {
            return;
        }
        hint.setText("Foto wird gelesen …");
        new Thread(new Runnable() {
            @Override
            public void run() {
                final String text = readImage(source);
                if (temporary) {
                    getContentResolver().delete(source, null, null);   // das Foto bleibt nicht liegen
                }
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (text != null) {
                            accept(text);
                        } else {
                            hint.setText("Im Foto wurde kein QR-Code gefunden.\nDen ganzen Code scharf und möglichst gerade aufnehmen.");
                        }
                    }
                });
            }
        }, "nolphin-photo").start();
    }

    /** Liest ein Foto (auf höchstens etwa 2600 Punkte verkleinert) und sucht den QR-Code darin. */
    private String readImage(Uri uri) {
        try {
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inJustDecodeBounds = true;
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                BitmapFactory.decodeStream(in, null, o);
            }
            int max = Math.max(o.outWidth, o.outHeight);
            int sample = 1;
            while (max / sample > 2600) {
                sample *= 2;
            }
            o.inJustDecodeBounds = false;
            o.inSampleSize = sample;
            Bitmap b;
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                b = BitmapFactory.decodeStream(in, null, o);
            }
            if (b == null) {
                return null;
            }
            int w = b.getWidth(), h = b.getHeight();
            int[] px = new int[w * h];
            b.getPixels(px, 0, w, 0, 0, w, h);
            b.recycle();
            byte[] gray = new byte[w * h];
            for (int i = 0; i < gray.length; i++) {
                int p = px[i];
                gray[i] = (byte) ((((p >> 16) & 255) * 3 + ((p >> 8) & 255) * 6 + (p & 255)) / 10);
            }
            return QrDecoder.decodeMultiScale(gray, w, h);
        } catch (Exception | OutOfMemoryError e) {
            return null;
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

    /** Vorschaugröße nahe 1920x1080: genug Auflösung auch für einen kleinen Code in größerem Abstand. */
    private static Camera.Size bestSize(List<Camera.Size> sizes) {
        Camera.Size best = sizes.get(0);
        long bestScore = Long.MAX_VALUE;
        for (Camera.Size s : sizes) {
            long score = Math.abs((long) s.width * s.height - 1920L * 1080L);
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
                    final String text = QrDecoder.decodeMultiScale(gray, w, h);
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

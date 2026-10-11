#!/bin/bash
# Baut die Handy-App (APK) nur mit den Werkzeugen des Android-SDK, ohne Gradle und ohne Bibliotheken.
# Voraussetzungen: JDK (javac, keytool) und das Android-SDK (android.jar, aapt2, d8, zipalign, apksigner).
# Aufruf: android/build.sh   Ergebnis: android/build/nolphin-handy.apk
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
[ -d "$SDK" ] || { echo "Android-SDK nicht gefunden (ANDROID_HOME setzen): $SDK" >&2; exit 2; }

BT="$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -1)"
# Die jüngste Plattform mit ganzzahliger Version (keine Vorabversionen wie 37.0)
PLATFORM="$(ls -d "$SDK"/platforms/android-[0-9][0-9] 2>/dev/null | sort -V | tail -1)"
[ -n "$BT" ] && [ -n "$PLATFORM" ] || { echo "build-tools oder platforms fehlen im SDK" >&2; exit 2; }
for t in aapt2 d8 zipalign apksigner; do [ -x "$BT/$t" ] || { echo "$t fehlt in $BT" >&2; exit 2; }; done
command -v javac >/dev/null && command -v keytool >/dev/null || { echo "JDK (javac, keytool) fehlt" >&2; exit 2; }

OUT="$HERE/build"
rm -rf "$OUT" && mkdir -p "$OUT/classes"

# Eigener Signaturschlüssel, einmal erzeugt und außerhalb des Repositories abgelegt.
KEYDIR="${XDG_CONFIG_HOME:-$HOME/.config}/nolphin"
KS="$KEYDIR/handy.keystore"
PASSFILE="$KEYDIR/handy.keystore.pass"
if [ ! -f "$KS" ]; then
    mkdir -p "$KEYDIR" && chmod 700 "$KEYDIR"
    head -c 24 /dev/urandom | base64 | tr -d '=+/' > "$PASSFILE" && chmod 600 "$PASSFILE"
    keytool -genkeypair -keystore "$KS" -storepass "$(cat "$PASSFILE")" -keypass "$(cat "$PASSFILE")" \
        -alias nolphin-handy -keyalg RSA -keysize 3072 -validity 36500 -dname "CN=Nolphin Handy" >/dev/null
fi

javac --release 11 -Xlint:all -cp "$PLATFORM/android.jar" -d "$OUT/classes" $(find "$HERE/src" -name '*.java')
"$BT/d8" --lib "$PLATFORM/android.jar" --min-api 29 --output "$OUT" $(find "$OUT/classes" -name '*.class')
"$BT/aapt2" link --manifest "$HERE/AndroidManifest.xml" -I "$PLATFORM/android.jar" \
    --min-sdk-version 29 --target-sdk-version 34 -o "$OUT/base.apk"
(cd "$OUT" && zip -q -j base.apk classes.dex)
"$BT/zipalign" -f 4 "$OUT/base.apk" "$OUT/aligned.apk"
"$BT/apksigner" sign --ks "$KS" --ks-pass "pass:$(cat "$PASSFILE")" --key-pass "pass:$(cat "$PASSFILE")" \
    --out "$OUT/nolphin-handy.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify "$OUT/nolphin-handy.apk"
rm -f "$OUT/base.apk" "$OUT/aligned.apk" "$OUT/nolphin-handy.apk.idsig"
echo "Fertig: $OUT/nolphin-handy.apk"

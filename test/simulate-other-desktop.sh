#!/bin/bash
# Startet Nolphin aus dem Build-Ordner in einer nachgestellten Umgebung eines
# Desktops OHNE Cinnamon-/MATE-Schemas (z. B. XFCE) und meldet, ob es abstuerzt.
# Installiert nichts und aendert keine Einstellungen (eigene D-Bus-Sitzung,
# GSettings nur im Speicher). Ersetzt KEINEN echten Test auf XFCE/MATE.
#
# Aufruf: [BUILD=build-noxapp] [WAIT=Sekunden] test/simulate-other-desktop.sh [Desktopname, Standard: XFCE]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DESKTOP="${1:-XFCE}"
BUILD="${BUILD:-build}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/xdg/glib-2.0/schemas" "$WORK/dir"
for e in /usr/share/*; do b=$(basename "$e"); [ "$b" = glib-2.0 ] || ln -s "$e" "$WORK/xdg/$b"; done
for e in /usr/share/glib-2.0/*; do b=$(basename "$e"); [ "$b" = schemas ] || ln -s "$e" "$WORK/xdg/glib-2.0/$b"; done
for f in /usr/share/glib-2.0/schemas/*.xml; do
    case "$(basename "$f")" in *innamon*|*mate*|*MATE*) ;; *) cp "$f" "$WORK/xdg/glib-2.0/schemas/";; esac
done
glib-compile-schemas "$WORK/xdg/glib-2.0/schemas"

cat > "$WORK/run.sh" <<RUN
#!/bin/bash
cd "$ROOT"
export XDG_DATA_DIRS="$WORK/xdg" XDG_CURRENT_DESKTOP="$DESKTOP" XDG_SESSION_DESKTOP="${DESKTOP,,}" DESKTOP_SESSION="${DESKTOP,,}"
export GTK_USE_PORTAL=0 GIO_USE_PORTALS=0 NO_AT_BRIDGE=1 GSETTINGS_BACKEND=memory
export GSETTINGS_SCHEMA_DIR="$ROOT/$BUILD/libnolphin-private" LD_LIBRARY_PATH="$ROOT/$BUILD/libnolphin-extension"
exec ./$BUILD/src/nolphin "$WORK/dir"
RUN
chmod +x "$WORK/run.sh"

dbus-run-session -- "$WORK/run.sh" > "$WORK/log" 2>&1 &
sleep "${WAIT:-100}"   # der Start ist in Sandboxen langsam
if grep -E "GLib-GIO-ERROR|assertion|Segmentation" "$WORK/log" | grep -v xdg-desktop-portal; then
    echo "FEHLER: Nolphin ist unter '$DESKTOP' abgestuerzt"; RC=1
elif xwininfo -root -tree | grep -qE "\"$(basename "$WORK/dir")( |\")"; then
    echo "OK: Nolphin laeuft unter '$DESKTOP' (ohne Cinnamon-/MATE-Schemas), Fenster ist offen"; RC=0
else
    echo "UNKLAR: kein Absturz, aber (noch) kein Fenster - WAIT erhoehen"; RC=2
fi
pkill -f "dbus-run-session -- $WORK/run.sh"; pkill -f "$BUILD/src/nolphin $WORK/dir"
exit $RC

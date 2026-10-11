
install_man(
  'nolphin.1',
  'nolphin-desktop.1',
  'nolphin-connect-server.1',
)

if gtkdoc_enabled
  subdir('reference')
endif

# Just for Dist
extraMans = [
  'dnd.txt',
  'key_mouse_navigation.txt',
  'load-states.dia',
  'nolphin-io.txt',
  'style-guide.html',
]

# Eingebaute Hilfe (F1): Themen als Markdown plus die Bilder, die sie zeigen
install_subdir('hilfe', install_dir: join_paths(nolphinDataPath, 'help'))
install_subdir('bilder', install_dir: join_paths(nolphinDataPath, 'help'))

# Handy-App (APK): wird nur mitinstalliert, wenn sie vorher mit android/build.sh gebaut wurde.
# Nolphin bietet sie im Handy-Austausch zum Installieren an; ohne APK läuft alles im Browser des Handys.
phoneApk = join_paths(meson.project_source_root(), 'android', 'build', 'nolphin-handy.apk')
if run_command('test', '-f', phoneApk, check: false).returncode() == 0
  install_data(phoneApk, install_dir: join_paths(nolphinDataPath, 'phone'))
endif

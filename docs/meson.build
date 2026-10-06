
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

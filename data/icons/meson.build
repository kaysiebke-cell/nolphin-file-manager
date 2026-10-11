# Die Menü-Symbole menu-*.png (Sortierpfeile usw.) stecken in der Programm-Ressource und
# werden nicht ins System-Icon-Verzeichnis installiert: Sie kollidieren dort mit nemo-data.
publicIcons = [
  'hicolor/apps/16x16/nolphin.png',
  'hicolor/apps/22x22/nolphin.png',
  'hicolor/apps/24x24/nolphin.png',
  'hicolor/apps/32x32/nolphin.png',
  'hicolor/apps/48x48/nolphin.png',
  'hicolor/apps/256x256/nolphin.png',
  'hicolor/apps/scalable/nolphin.svg',
  'hicolor/actions/16x16/nolphin-eject.png',
  'hicolor/actions/32x32/nolphin-eject.png',
  'hicolor/actions/scalable/nolphin-location-symbolic.svg',
  'hicolor/actions/scalable/nolphin-location-symbolic-rtl.svg',
  'hicolor/actions/scalable/nolphin-sidebar-hide-symbolic.svg',
  'hicolor/actions/scalable/nolphin-sidebar-hide-symbolic-rtl.svg',
  'hicolor/actions/scalable/nolphin-sidebar-show-symbolic.svg',
  'hicolor/actions/scalable/nolphin-sidebar-show-symbolic-rtl.svg',
  'hicolor/actions/scalable/nolphin-sidebar-tree-symbolic.svg',
  'hicolor/actions/scalable/nolphin-sidebar-tree-symbolic-rtl.svg',
  'hicolor/actions/scalable/nolphin-sidebar-places-symbolic.svg',
  'hicolor/actions/scalable/nolphin-auto-arrange-symbolic.svg',
  'hicolor/actions/scalable/nolphin-desktop-scale-symbolic.svg',
  'hicolor/actions/scalable/nolphin-horizontal-layout-symbolic.svg',
  'hicolor/actions/scalable/nolphin-horizontal-layout-wide-symbolic.svg',
  'hicolor/actions/scalable/nolphin-vertical-layout-symbolic.svg',
  'hicolor/actions/scalable/nolphin-vertical-layout-wide-symbolic.svg',
  'hicolor/actions/scalable/nolphin-recursive-search-symbolic.svg',
  'hicolor/actions/scalable/nolphin-recursive-search-symbolic-rtl.svg',
  'hicolor/status/scalable/nolphin-progress-0-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-10-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-20-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-30-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-40-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-50-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-60-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-70-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-80-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-90-symbolic.svg',
  'hicolor/status/scalable/nolphin-progress-100-symbolic.svg',
]
privateIcons = [
  'hicolor/emblems/16x16/emblem-note.png',
  'hicolor/emblems/24x24/emblem-note.png',
  'hicolor/emblems/48x48/emblem-note.png',
]

foreach icon : publicIcons
  nameParts = icon.split('/')
  theme   = nameParts[0]
  context = nameParts[1]
  size    = nameParts[2]
  file    = nameParts[3]
  install_data(icon,
    install_dir: join_paths(get_option('datadir'), 'icons', theme, size, context)
  )
endforeach


foreach icon : privateIcons
  nameParts = icon.split('/')
  theme   = nameParts[0]
  context = nameParts[1]
  size    = nameParts[2]
  file    = nameParts[3]
  install_data(icon,
    install_dir: join_paths(get_option('datadir'), 'nolphin', 'icons', theme, size, context)
  )
endforeach


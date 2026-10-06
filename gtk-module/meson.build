# GTK-3-Modul: Vorschau in den Dateiauswahldialogen anderer Programme
# (aktivieren mit gtk-modules=nolphin-preview-module, siehe Hilfe)
shared_module('nolphin-preview-module',
  [ 'nolphin-preview-module.c', '../libnolphin-private/nolphin-file-chooser-preview.c' ],
  include_directories: [ rootInclude, include_directories('../libnolphin-private') ],
  dependencies: [ gtk ],
  name_prefix: 'lib',
  install: true,
  install_dir: join_paths(get_option('libdir'), 'gtk-3.0', 'modules'),
)

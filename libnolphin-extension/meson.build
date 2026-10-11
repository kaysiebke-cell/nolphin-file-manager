
nolphin_i18n_header = [
  'nolphin-extension-i18n.h'
]

nolphin_extension_sources = [
  gresources,
  'nolphin-column-provider.c',
  'nolphin-column.c',
  'nolphin-desktop-preferences.c',
  'nolphin-extension-private.h',
  'nolphin-file-info.c',
  'nolphin-info-provider.c',
  'nolphin-location-widget-provider.c',
  'nolphin-menu-item.c',
  'nolphin-menu-provider.c',
  'nolphin-name-and-desc-provider.c',
  'nolphin-property-page-provider.c',
  'nolphin-property-page.c',
  'nolphin-menu.c',
  'nolphin-simple-button.c',
]

nolphin_extension_headers = [
  'nolphin-column-provider.h',
  'nolphin-column.h',
  'nolphin-desktop-preferences.h',
  'nolphin-extension-types.h',
  'nolphin-file-info.h',
  'nolphin-info-provider.h',
  'nolphin-location-widget-provider.h',
  'nolphin-menu-item.h',
  'nolphin-menu-provider.h',
  'nolphin-name-and-desc-provider.h',
  'nolphin-property-page-provider.h',
  'nolphin-property-page.h',
  'nolphin-menu.h',
  'nolphin-simple-button.h',
]

nolphin_extension_incdir = include_directories('.')

nolphin_extension_deps = [ glib, gtk ]

nolphin_extension_lib = shared_library('nolphin-extension',
  nolphin_extension_sources + nolphin_i18n_header,
  dependencies: nolphin_extension_deps,
  include_directories: [ rootInclude, ],
  install: true,
  version: '1.4.0',
  link_args: [ '-Wl,-no-undefined' ],
)

install_headers(nolphin_extension_headers,
  subdir: join_paths('nolphin', 'libnolphin-extension')
)

nolphin_extension = declare_dependency(
  include_directories: include_directories('.'),
  link_with: [ nolphin_extension_lib ],
  dependencies: nolphin_extension_deps,
)

typelibdir = go_intr.get_variable(pkgconfig: 'typelibdir',
  pkgconfig_define: ['libdir', join_paths(get_option('prefix'), get_option('libdir'))]
)

gnome.generate_gir(nolphin_extension_lib,
  sources: nolphin_extension_sources + nolphin_extension_headers,
  nsversion: '3.0',
  namespace: 'Nolphin',
  export_packages: 'libnolphin-extension',
  includes: [ 'Gtk-3.0', 'Gio-2.0', 'GLib-2.0', ],
  include_directories: [ rootInclude, ],
  install_dir_typelib: typelibdir,
  install: true,
)

pkgconfig.generate(filebase: 'libnolphin-extension',
  name: 'libnolphin-extension',
  description: 'A library to create Nolphin view extensions',
  version: meson.project_version(),
  requires: [ 'gio-2.0', 'glib-2.0', 'gtk+-3.0' ],
  libraries: nolphin_extension_lib,
  subdirs: 'nolphin',
  variables: 'extensiondir=${libdir}/@0@/@1@'.format('nolphin', 'extensions-3.0'),
)


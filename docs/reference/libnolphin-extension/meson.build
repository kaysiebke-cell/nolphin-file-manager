
docConf = configuration_data()
docConf.set('NOLPHIN_EXTENSION_VERSION_INFO', NOLPHIN_EXTENSION_VERSION_INFO)

configure_file(
  input : 'version.xml.in',
  output: 'version.xml',
  configuration: docConf
)

gnome.gtkdoc('libnolphin-extension',
  main_xml: 'libnolphin-extension-docs.xml',
  src_dir: join_paths(meson.project_source_root(), 'libnolphin-extension'),
  dependencies: nolphin_extension,
  install: true,
  gobject_typesfile: 'libnolphin-extension.types',
  install_dir: 'libnolphin-extension',
  scan_args: [
    '--rebuild-types',
    '--ignore-headers="config.h"' ]
)

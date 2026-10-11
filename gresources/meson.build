gresources = gnome.compile_resources(
  'nolphin-resources', 'nolphin.gresource.xml',
  source_dir: '.',
  c_name: 'nolphin',
  install_header: false,
  install: false
)

layout_editor_gresources = gnome.compile_resources(
  'nolphin-action-layout-editor-resources', 'nolphin-action-layout-editor.gresource.xml',
  source_dir: '.',
  gresource_bundle: true,
  install: true,
  install_dir: nolphinDataPath
)

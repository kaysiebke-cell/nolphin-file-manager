conf = configuration_data()
conf.set_quoted('PKG_DATADIR', nolphinDataPath)
conf.set_quoted('LOCALE_DIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())

config_py = configure_file(
  input: 'leconfig.py.in',
  output: 'leconfig.py',
  configuration: conf,
  install: true,
  install_dir: nolphinDataPath / 'layout-editor',
)

bin_conf = configuration_data()
bin_conf.set('PKG_DATADIR', nolphinDataPath)

bin = configure_file(
  input: 'nolphin-action-layout-editor.in',
  output: 'nolphin-action-layout-editor',
  configuration: bin_conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x'
)

install_data(
    'nolphin_action_layout_editor.py',
    install_dir: nolphinDataPath / 'layout-editor',
    install_mode: 'rwxr-xr-x'
)

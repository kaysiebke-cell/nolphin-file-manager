
################################################################################

desktopFiles = [
  'nolphin-autorun-software.desktop',
  'nolphin-autostart.desktop',
  'org.nolphin.FileManager.desktop',
]

install_data(desktopFiles,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

dbusFiles = [
  configure_file(
    input : 'nolphin.service.in',
    output: 'nolphin.service',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nolphin.FileManager1.service.in',
    output: 'nolphin.FileManager1.service',
    configuration: dataConf,
  ),
]

install_data(dbusFiles,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services')
)

################################################################################

# nolphin.xml is generated from nolphin.xml.in by ../generate_additional_file
install_data('nolphin.xml',
  install_dir: join_paths(get_option('datadir'), 'mime', 'packages')
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

rootPolicyFile = configure_file(
  input : 'org.nolphin.root.policy.in',
  output: 'org.nolphin.root.policy',
  configuration: dataConf,
)

install_data(rootPolicyFile,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions')
)

foreach lang_file: ['nolphin_action.lang', 'nolphin_search_helper.lang']
  foreach sv_dir: ['gtksourceview-2.0', 'gtksourceview-3.0', 'gtksourceview-4']
    install_data(lang_file,
      install_dir: join_paths(get_option('datadir'), sv_dir, 'language-specs')
    )
  endforeach
endforeach

################################################################################

subdir('icons')

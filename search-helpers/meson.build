libgsf = dependency('libgsf-1', required: true)

mso_to_txt_sources = [
  'nolphin-mso-to-txt.c'
]

mso_to_txt = executable('nolphin-mso-to-txt',
  mso_to_txt_sources,
  dependencies: [libgsf, gio, glib],
  install: true
)

ppt_to_txt_sources = [
  'nolphin-ppt-to-txt.c'
]

ppt_to_txt = executable('nolphin-ppt-to-txt',
  ppt_to_txt_sources,
  dependencies: [gio, glib],
  install: true
)

install_data(
    ['nolphin-xls-to-txt', 'nolphin-odf-to-txt'],
    install_dir: join_paths(get_option('prefix'), get_option('bindir')),
    install_mode: 'rwxr-xr-x'
)

install_data(
    'epub2text.nolphin_search_helper',
    'mso.nolphin_search_helper',
    'mso-ppt.nolphin_search_helper',
    'mso-xls.nolphin_search_helper',
    'odf.nolphin_search_helper',
    install_dir: join_paths(nolphinDataPath, 'search-helpers')
)

install_data(
    'nolphin-epub2text',
    install_dir: get_option('bindir')
)

subdir('third-party')
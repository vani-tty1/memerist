add_global_arguments('-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_10', language: 'c')

blueprints = custom_target('blueprints',
  input: files(
    'meme-window.blp',
    'primary-menu.blp',
    'shortcuts-dialog.blp',
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

myapp_sources = [
  'main.c',
  'meme-application.c',
  'meme-window.c',
  'meme-core.c',
  'meme-window.c',
  'meme-history.c',
  'meme-canvas.c',
  'meme-fileio.c',
  'meme-renderer.c',
  'meme-gpu.c',
  'about.c',
  'keyboard-shortcuts.c'
]

myapp_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('cairo'),
  dependency('epoxy'),
  cc.find_library('m'),
]

resources = gnome.compile_resources(
    'memerist-resources',
    'memerist.gresource.xml',
    dependencies: blueprints,
    source_dir: [
        meson.current_build_dir(),
        meson.current_source_dir(),
        meson.project_source_root() / 'data',
        meson.project_build_root() / 'data'
    ],
    c_name: 'memerist',
)

executable('memerist', myapp_sources, resources,
  dependencies: myapp_deps,
       install: true,
)

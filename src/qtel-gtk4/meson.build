project('qtel-gtk4', 'cpp',
  version: '2.0.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.62.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
  ]
)

# Import modules
gnome = import('gnome')
i18n = import('i18n')

# Application ID
app_id = 'org.svxlink.Qtel'

# Directories
prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')
schemadir = datadir / 'glib-2.0' / 'schemas'

# Dependencies
gtk4_dep = dependency('gtk4', version: '>= 4.10')
adw_dep = dependency('libadwaita-1', version: '>= 1.4')
glib_dep = dependency('glib-2.0', version: '>= 2.74')
gio_dep = dependency('gio-2.0', version: '>= 2.74')
sigcpp_dep = dependency('sigc++-3.0', required: false)
if not sigcpp_dep.found()
  sigcpp_dep = dependency('sigc++-2.0')
endif
# GSM library often doesn't have pkg-config, try to find it manually
gsm_dep = dependency('gsm', required: false)
if not gsm_dep.found()
  cc = meson.get_compiler('cpp')
  gsm_dep = cc.find_library('gsm', required: true)
endif

# SvxLink internal libraries
# These will be found in the parent build or via pkg-config when installed
# For now, we'll build against installed svxlink libs or use cmake subproject
# In production, these would be proper subprojects or installed dependencies

# Check if we're building within svxlink tree or standalone
asynccore_dep = dependency('asynccore', required: false)
asyncglib_dep = dependency('asyncglib', required: false)
asyncaudio_dep = dependency('asyncaudio', required: false)
echolib_dep = dependency('echolib', required: false)

if not asynccore_dep.found()
  # Building within svxlink - use relative paths
  # This will be updated once svxlink cmake exports proper pkg-config files
  message('Building within svxlink source tree - using relative include paths')

  # Use relative paths from current source directory
  svxlink_inc = include_directories(
    '../async/core',
    '../async/glib',
    '../async/audio',
    '../echolib',
    '../misc',
    '../qtel',  # For multirate_filter_coeff.h
    '/usr/include/gsm',  # For gsm.h used by EchoLinkQso.h
  )

  # Link against the libraries built by CMake
  cc = meson.get_compiler('cpp')
  svxlink_build_lib = '../build/lib'

  echolib_lib = cc.find_library('echolib',
    dirs: [meson.current_source_dir() / svxlink_build_lib],
    required: true)
  asynccore_lib = cc.find_library('asynccore',
    dirs: [meson.current_source_dir() / svxlink_build_lib],
    required: true)
  asyncaudio_lib = cc.find_library('asyncaudio',
    dirs: [meson.current_source_dir() / svxlink_build_lib],
    required: true)
  asyncglib_lib = cc.find_library('asyncglib',
    dirs: [meson.current_source_dir() / svxlink_build_lib],
    required: true)

  svxlink_libs = [echolib_lib, asynccore_lib, asyncaudio_lib, asyncglib_lib]
else
  svxlink_inc = []
  svxlink_libs = [asynccore_dep, asyncglib_dep, asyncaudio_dep, echolib_dep]
endif

# Get git revision
git = find_program('git', required: false)
if git.found()
  git_rev = run_command(git, 'rev-parse', '--short', 'HEAD', check: false)
  if git_rev.returncode() == 0
    git_revision = git_rev.stdout().strip()
  else
    git_revision = 'unknown'
  endif
  git_dirty = run_command(git, 'diff', '--quiet', check: false)
  if git_dirty.returncode() != 0
    git_revision = git_revision + '-dirty'
  endif
else
  git_revision = 'unknown'
endif

# Configuration data
config_data = configuration_data()
config_data.set_quoted('APP_ID', app_id)
config_data.set_quoted('APP_VERSION', meson.project_version())
config_data.set_quoted('APP_GIT_REV', git_revision)
config_data.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config_data.set_quoted('LOCALEDIR', localedir)
config_data.set_quoted('DATADIR', datadir)

config_header = configure_file(
  output: 'qtel-config.h',
  configuration: config_data,
)

# Blueprint compiler (optional - will fall back to manual UI if not available)
blueprint_compiler = find_program('blueprint-compiler', required: false)

# Source files
qtel_sources = [
  'src/main.cpp',
  'src/qtel-application.cpp',
  'src/qtel-window.cpp',
  'src/qtel-call-dialog.cpp',
  'src/qtel-preferences.cpp',
  'src/settings.cpp',
  'src/station-object.cpp',
  'src/station-list-model.cpp',
  'src/vox.cpp',
  'src/MsgHandler.cpp',
]

# UI files - Blueprint or XML
if blueprint_compiler.found()
  message('Blueprint compiler found - will compile .blp files')

  blueprints = gnome.compile_blueprints(
    name: 'qtel-blueprints',
    source_dir: 'ui',
    resource_prefix: '/org/svxlink/qtel/ui',
    namespaced: false,
  )
else
  message('Blueprint compiler not found - using XML UI files')
  blueprints = []
endif

# Resources
qtel_resources = gnome.compile_resources(
  'qtel-resources',
  'resources/qtel.gresource.xml',
  source_dir: 'resources',
  c_name: 'qtel',
  dependencies: blueprints,
)

# GSettings schema
install_data(
  'data/org.svxlink.Qtel.gschema.xml',
  install_dir: schemadir,
)

# Compile schemas for development
gnome.compile_schemas(
  build_by_default: true,
  depend_files: 'data/org.svxlink.Qtel.gschema.xml',
)

# Desktop file (without translations for now - will be added in Phase 6)
desktop_file = configure_file(
  input: 'data/org.svxlink.Qtel.desktop.in',
  output: 'org.svxlink.Qtel.desktop',
  copy: true,
)
install_data(
  desktop_file,
  install_dir: datadir / 'applications',
)

# Validate desktop file
desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('validate-desktop-file',
    desktop_file_validate,
    args: [desktop_file],
  )
endif

# AppStream metadata (without translations for now - will be added in Phase 6)
appstream_file = configure_file(
  input: 'data/org.svxlink.Qtel.metainfo.xml.in',
  output: 'org.svxlink.Qtel.metainfo.xml',
  copy: true,
)
install_data(
  appstream_file,
  install_dir: datadir / 'metainfo',
)

# Validate AppStream file
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('validate-appstream',
    appstreamcli,
    args: ['validate', '--no-net', appstream_file],
  )
endif

# Install icons
iconsdir = datadir / 'icons' / 'hicolor'

install_data(
  'resources/icons/hicolor/scalable/apps/org.svxlink.Qtel.svg',
  install_dir: iconsdir / 'scalable' / 'apps',
)

install_data(
  'resources/icons/hicolor/symbolic/apps/org.svxlink.Qtel-symbolic.svg',
  install_dir: iconsdir / 'symbolic' / 'apps',
)

# Build executable
qtel_exe = executable('qtel-gtk4',
  qtel_sources,
  qtel_resources,
  config_header,
  include_directories: svxlink_inc,
  dependencies: [
    gtk4_dep,
    adw_dep,
    glib_dep,
    gio_dep,
    sigcpp_dep,
    gsm_dep,
  ] + svxlink_libs,
  install: true,
  install_dir: bindir,
)

# Translations
# subdir('po')

# Summary
summary({
  'prefix': prefix,
  'bindir': bindir,
  'datadir': datadir,
  'Blueprint compiler': blueprint_compiler.found(),
}, section: 'Directories')

summary({
  'GTK4': gtk4_dep.version(),
  'libadwaita': adw_dep.version(),
  'GLib': glib_dep.version(),
  'sigc++': sigcpp_dep.version(),
}, section: 'Dependencies')

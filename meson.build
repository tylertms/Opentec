project(
  'opentec',
  'c',
  meson_version: '>=1.12.0',
  default_options: [
    'build.c_std=c11',
    'c_std=c11',
    'warning_level=3',
    'werror=true',
  ],
)

if meson.is_cross_build() and host_machine.cpu_family() == 'arm'
  add_languages('c', native: true)
endif

common_include = include_directories('source')
wqr_frame_sources = files('source/common/wqr_frame.c')

subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')

project(
  'opentec',
  'c',
  meson_version: '>=1.12.0',
  default_options: [
    'buildtype=release',
    'build.c_std=c11',
    'c_std=c11',
    'optimization=2',
    'warning_level=3',
    'werror=true',
  ],
)

if meson.is_cross_build()
  add_languages('c', native: true)
endif

subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')
subdir('source/podium-dd1-motor')
subdir('source/podium-dd2-motor')

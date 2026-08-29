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

if (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m4'
)
  add_languages('c', native: true)
endif

subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')
subdir('source/podium-dd1-motor')
subdir('source/podium-dd2-motor')

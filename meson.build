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
  and (
    host_machine.cpu() == 'cortex-m4'
    or host_machine.cpu() == 'cortex-m0plus'
  )
)
  add_languages('c', native: true)
endif

if meson.is_cross_build() and host_machine.cpu_family() == 'dspic'
  dspic_device_pack = subproject('dspic33e-device-pack')
  dspic_device_pack_path = dspic_device_pack.get_variable('device_pack_path')
  dspic_device = '-mcpu=33EP512MU810'
  dspic_pack = '-mdfp=' + dspic_device_pack_path
  dspic_linker_script = dspic_device_pack_path / 'support/dsPIC33E/gld/p33EP512MU810.gld'

  add_project_arguments(
    dspic_device,
    dspic_pack,
    '-mlarge-code',
    '-mconst-in-data',
    '-DOPENTEC_SIM30_TEST',
    '-c',
    language: 'c',
  )
  add_project_link_arguments(
    dspic_device,
    dspic_pack,
    '-mlarge-code',
    '-Wl,--script=' + dspic_linker_script,
    language: 'c',
  )
endif

subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')
subdir('source/podium-dd1-motor')
subdir('source/podium-dd2-motor')

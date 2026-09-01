dspic33e_sim = subproject('dspic33e-sim', default_options: ['werror=false'])
kinetis_sim = subproject('kinetis-sim', default_options: ['werror=false'])

dspic33e_image = dspic33e_sim.get_variable('dspic33ep_mu_firmware_image_dependency')
kinetis_image = kinetis_sim.get_variable('kinetis_firmware_image_dependency')
windows_compiler = meson.get_compiler('c')
common_dialog = windows_compiler.find_library('comdlg32')

dd1_sim_sources = files('machine.c', 'wheel.c')

executable(
  'dd1-sim',
  'main.c',
  dd1_sim_sources,
  dependencies: [dspic33e_image, kinetis_image, common_dialog],
  install: false,
)

dd1_sim_test = executable(
  'dd1-sim-self-test',
  'test.c',
  'wheel.c',
  build_by_default: false,
)

test('dd1-sim-self-test', dd1_sim_test, suite: 'dd1-sim')

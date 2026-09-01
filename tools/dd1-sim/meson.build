dspic33e_sim = subproject('dspic33e-sim', default_options: ['werror=false'])
kinetis_sim = subproject('kinetis-sim', default_options: ['werror=false'])

dspic33e_core = dspic33e_sim.get_variable('dspic33ep_mu_simulator_dependency')
kinetis_core = kinetis_sim.get_variable('kinetis_simulator_dependency')
windows_compiler = meson.get_compiler('c')
common_dialog = windows_compiler.find_library('comdlg32')
windows_crypto = windows_compiler.find_library('advapi32')
direct2d = windows_compiler.find_library('d2d1')
directwrite = windows_compiler.find_library('dwrite')
ole32 = windows_compiler.find_library('ole32')
windows_codecs = windows_compiler.find_library('windowscodecs')

configure_file(input: 'assets/formula-v25.png', output: 'formula-v25.png', copy: true)

dd1_sim_sources = files('firmware.c', 'machine.c', 'wheel.c')

executable(
  'dd1-sim',
  'main.c',
  dd1_sim_sources,
  dependencies: [
    dspic33e_core,
    kinetis_core,
    common_dialog,
    windows_crypto,
    direct2d,
    directwrite,
    ole32,
    windows_codecs,
  ],
  install: false,
)

dd1_sim_test = executable(
  'dd1-sim-self-test',
  'test.c',
  'wheel.c',
  build_by_default: false,
)

test('dd1-sim-self-test', dd1_sim_test, suite: 'dd1-sim')

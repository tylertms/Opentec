protocol_test = executable(
  'wqr-protocol-test',
  'source/protocol.c',
  protocol_sources,
  build_by_default: false,
  include_directories: protocol_include,
  native: meson.is_cross_build(),
)

test('wqr-protocol', protocol_test, suite: ['wqr', 'protocol'])

if meson.is_cross_build()
  simulator = subproject('kinetis-sim', native: true)
  simulator_dependency = simulator.get_variable('kinetis_firmware_image_dependency')
  firmware_test = executable(
    'wqr-firmware-test',
    'source/firmware.c',
    protocol_sources,
    build_by_default: false,
    dependencies: simulator_dependency,
    include_directories: protocol_include,
    native: true,
  )

  test(
    'wqr-firmware',
    firmware_test,
    args: firmware.full_path(),
    depends: firmware,
    suite: ['wqr', 'firmware'],
  )
endif

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
    args: checksummed_binary.full_path(),
    depends: checksummed_binary,
    suite: ['wqr', 'firmware'],
  )

  test(
    'wqr-checksum',
    python,
    args: [
      meson.project_source_root() / 'tools/checksum.py',
      '--verify',
      checksummed_binary.full_path(),
    ],
    depends: checksummed_binary,
    suite: ['wqr', 'package'],
  )

  test(
    'wqr-encryption',
    python,
    args: [
      meson.project_source_root() / 'tools/crypto.py',
      'verify',
      firmware_hex.full_path(),
      firmware_package.full_path(),
      '--legacy-wide-secret',
    ],
    depends: firmware_package,
    suite: ['wqr', 'package'],
  )
endif

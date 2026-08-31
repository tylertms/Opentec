protocol_test = executable(
  'wqr-protocol-test',
  'source/protocol.c',
  wqr_protocol_sources,
  build_by_default: false,
  include_directories: wqr_include,
  native: meson.is_cross_build(),
  override_options: ['b_ndebug=false'],
)

test('wqr-protocol', protocol_test, suite: ['wqr', 'protocol'])

if meson.is_cross_build()
  simulator = subproject('kinetis-sim', native: true)
  simulator_dependency = simulator.get_variable('kinetis_firmware_image_dependency')
  firmware_test = executable(
    'wqr-firmware-test',
    'source/firmware.c',
    wqr_protocol_sources,
    build_by_default: false,
    dependencies: simulator_dependency,
    include_directories: wqr_include,
    native: true,
  )

  test(
    'wqr-firmware',
    firmware_test,
    args: [checksummed_binary.full_path(), firmware.full_path()],
    depends: [checksummed_binary, firmware],
    suite: ['wqr', 'firmware'],
  )

  test(
    'wqr-hex',
    python,
    args: [
      meson.current_source_dir() / 'source/package.py',
      hex.full_path(),
      checksummed_binary.full_path(),
    ],
    depends: [hex, checksummed_binary],
    suite: ['wqr', 'package'],
  )

  test(
    'wqr-checksum',
    python,
    args: [
      meson.current_source_dir() / '../tools/checksum.py',
      '--verify',
      checksummed_binary.full_path(),
    ],
    depends: checksummed_binary,
    suite: ['wqr', 'package'],
  )

  test(
    'wqr-encrypted-package',
    python,
    args: [
      meson.project_source_root() / 'tools/crypto.py',
      'verify',
      hex.full_path(),
      package.full_path(),
      '--legacy-wide-secret',
    ],
    depends: package,
    suite: ['wqr', 'package'],
  )
endif

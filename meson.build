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
if wqr_is_target
  package = custom_target(
    wqr_name,
    input: hex,
    output: wqr_name + '.hex',
    command: [
      python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--legacy-wide-secret',
      '--preserve-newlines',
    ],
    depend_files: wqr_package_dependencies,
    build_by_default: true,
    build_always_stale: true,
    build_subdir: 'firmware',
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

subdir('source/podium-dd1-dd2-base')
if base_is_dspic
  base_package = custom_target(
    'podium-dd1-dd2-base-hex',
    input: base_hex,
    output: 'podium-dd1-dd2-base.hex',
    command: [
      base_python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--header',
      base_package_header,
    ],
    build_by_default: true,
    build_subdir: 'firmware',
  )

  test(
    'base-encryption',
    base_python,
    args: [
      meson.project_source_root() / 'tools/crypto.py',
      'verify',
      base_hex.full_path(),
      base_package.full_path(),
      '--header',
      base_package_header,
    ],
    depends: base_package,
    suite: ['base', 'package'],
  )
endif

subdir('source/podium-dd1-motor')
if motor_is_target
  custom_target(
    motor_name,
    input: motor_hex,
    output: motor_name + '.hex',
    command: [
      motor_python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--legacy-wide-secret',
      '--preserve-newlines',
    ],
    depend_files: motor_package_dependencies,
    build_by_default: true,
    build_always_stale: true,
    build_subdir: 'firmware',
  )
endif

subdir('source/podium-dd2-motor')
if motor_is_target
  custom_target(
    motor_name,
    input: motor_hex,
    output: motor_name + '.hex',
    command: [
      motor_python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--legacy-wide-secret',
      '--preserve-newlines',
    ],
    depend_files: motor_package_dependencies,
    build_by_default: true,
    build_always_stale: true,
    build_subdir: 'firmware',
  )
endif

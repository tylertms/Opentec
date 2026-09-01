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

firmware_packages = []
subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')
subdir('source/podium-dd1-motor')
subdir('source/podium-dd2-motor')

crypto = meson.project_source_root() / 'tools/crypto.py'
foreach firmware_package : firmware_packages
  package = custom_target(
    firmware_package['target'],
    input: firmware_package['input'],
    output: firmware_package['output'],
    command: [
      firmware_package['python'],
      crypto,
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
    ] + firmware_package['encrypt_args'],
    depend_files: firmware_package['dependencies'],
    build_by_default: true,
    build_always_stale: firmware_package.get('always_stale', false),
  )

  if firmware_package.has_key('test')
    test(
      firmware_package['test'],
      firmware_package['python'],
      args: [
        crypto,
        'verify',
        firmware_package['input'].full_path(),
        package.full_path(),
      ] + firmware_package['verify_args'],
      depends: package,
      suite: firmware_package['suite'],
    )
  endif
endforeach

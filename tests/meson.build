if not meson.is_cross_build()
  python = import('python').find_installation()
  test(
    'wqr-checksum',
    python,
    args: files('wqr_checksum_test.py'),
    env: {'PYTHONPATH': meson.project_source_root()},
  )
endif

project('rtcesl', 'c')

rtcesl_root = meson.current_source_dir() / 'CM0_RTCESL_4.8.1_MCUX'

rtcesl_dependency = declare_dependency(
  include_directories: include_directories(
    'CM0_RTCESL_4.8.1_MCUX/AMCLIB/Include',
    'CM0_RTCESL_4.8.1_MCUX/GDFLIB/Include',
    'CM0_RTCESL_4.8.1_MCUX/GFLIB/Include',
    'CM0_RTCESL_4.8.1_MCUX/GMCLIB/Include',
    'CM0_RTCESL_4.8.1_MCUX/MLIB/Include',
    is_system: true,
  ),
  link_args: [
    rtcesl_root / 'AMCLIB/libAMCLIB.a',
    rtcesl_root / 'GFLIB/libGFLIB.a',
    rtcesl_root / 'GDFLIB/libGDFLIB.a',
    rtcesl_root / 'GMCLIB/libGMCLIB.a',
    rtcesl_root / 'MLIB/libMLIB.a',
  ],
)

meson.override_dependency('rtcesl', rtcesl_dependency)

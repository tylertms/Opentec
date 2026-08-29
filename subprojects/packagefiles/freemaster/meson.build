project('freemaster', 'c')

freemaster_sources = files(
  'src/common/freemaster_appcmd.c',
  'src/common/freemaster_can.c',
  'src/common/freemaster_net.c',
  'src/common/freemaster_pdbdm.c',
  'src/common/freemaster_pipes.c',
  'src/common/freemaster_protocol.c',
  'src/common/freemaster_rec.c',
  'src/common/freemaster_scope.c',
  'src/common/freemaster_serial.c',
  'src/common/freemaster_sha.c',
  'src/common/freemaster_tsa.c',
  'src/common/freemaster_ures.c',
  'src/common/freemaster_utils.c',
)

freemaster_includes = include_directories(
  'src/common',
  'src/platforms/gen32le',
  is_system: true,
)

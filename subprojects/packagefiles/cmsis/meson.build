project('cmsis', 'c')

cmsis_dependency = declare_dependency(
  include_directories: include_directories('Core/Include', is_system: true),
)

meson.override_dependency('cmsis', cmsis_dependency)

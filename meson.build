project('vkguide', 'c', default_options: ['c_std=c17', 'warning_level=3'])

cc = meson.get_compiler('c')
sdl3_dep = dependency('SDL3')
vulkan_dep = dependency('vulkan')

executable(
  'vkguide',
  ['src/main.c'],
  dependencies: [sdl3_dep, vulkan_dep]
)

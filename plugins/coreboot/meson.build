cargs = ['-DG_LOG_DOMAIN="FuPluginCoreboot"']

shared_module('fu_plugin_coreboot',
  sources : [
    'fu-plugin-coreboot.c',
    'fu-coreboot-common.c',
  ],
  include_directories : [
    include_directories('../..'),
    include_directories('../../src'),
    include_directories('../../libfwupd'),
  ],
  install : true,
  install_dir: plugin_dir,
  link_with : [
    libfwupdprivate,
  ],
  c_args : [
    cargs,
  ],
  dependencies : [
    plugin_deps,
  ],
)

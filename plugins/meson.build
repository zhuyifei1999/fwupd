subdir('ata')
subdir('dfu')
subdir('colorhug')
subdir('coreboot')
subdir('ebitdo')
subdir('fastboot')
subdir('jabra')
subdir('steelseries')
subdir('dell-dock')
subdir('logitech-hidpp')
subdir('nitrokey')
subdir('optionrom')
subdir('rts54hid')
subdir('rts54hub')
subdir('solokey')
subdir('synaptics-cxaudio')
subdir('synaptics-prometheus')
subdir('test')
subdir('thelio-io')
subdir('upower')
subdir('wacom-raw')
subdir('wacom-usb')
subdir('superio')
subdir('synaptics-rmi')
subdir('vli-usbhub')

# depends on dfu
subdir('csr')

if get_option('plugin_emmc')
subdir('emmc')
endif

if get_option('plugin_nvme')
subdir('nvme')
endif

if get_option('plugin_modem_manager')
subdir('modem-manager')
endif

if get_option('plugin_altos')
subdir('altos')
endif

if get_option('plugin_amt')
subdir('amt')
endif

if get_option('plugin_thunderbolt')
subdir('thunderbolt')
subdir('thunderbolt-power')
endif

if get_option('plugin_redfish')
subdir('redfish')
endif

if get_option('plugin_dell')
subdir('dell')
subdir('dell-esrt')
endif

if get_option('plugin_synaptics')
subdir('synapticsmst')
endif

if get_option('plugin_uefi')
subdir('uefi')
subdir('uefi-recovery')
endif

if get_option('plugin_flashrom')
subdir('flashrom')
endif

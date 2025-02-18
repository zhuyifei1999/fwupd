coreboot
========

Introduction
------------

Until now only the version detection has been implemented.

System identification
---------------------

coreboot can be detected the following ways:
1. by parsing SMBIOS type 0 vendor string. On coreboot enabled platforms
   it's always `coreboot`.
2. by parsing ACPI tables. An ACPI device with the _HID `BOOT0000` exists.
   (This HID has been reserved for coreboot enabled platforms)
3. by parsing the devicetree. A node under *firmware/coreboot* with the
   compatible id `coreboot` exists.

coreboot version string
-----------------------

The coreboot version string always starts with `CBET`.
After the prefix the *version*, *major*, *minor* string follows and finally
the *build string*, containing the exact commit and repository state, follows.

For example:
> CBET4000 4.10-989-gc8a4e4b9c5-dirty

GUID Generation
---------------

These device uses hardware ID values which are derived from SMBIOS.
The following HWIDs are added on coreboot enabled platforms:

* HardwareID-3
* HardwareID-4
* HardwareID-5
* HardwareID-6
* HardwareID-10

They do match the values provided by `fwupdtool hwids` or
the `ComputerHardwareIds.exe` Windows utility.

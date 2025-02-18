VIA USB Hub Support
===================

Introduction
------------

This plugin is used to update USB hubs from VIA.

Firmware Format
---------------

The daemon will decompress the cabinet archive and extract a firmware blob in
an undisclosed binary file format.

This plugin supports the following protocol ID:

 * com.vli.usbhub

GUID Generation
---------------

These devices use the standard USB DeviceInstanceId values, e.g.

 * `USB\VID_17EF&PID_3083&REV_0001`
 * `USB\VID_17EF&PID_3083`
 * `USB\VID_17EF`

These devices also use custom GUID values for the SPI flash configuration, e.g.

 * `VLI_USBHUB\SPI_37303840`
 * `VLI_USBHUB\SPI_3730`
 * `VLI_USBHUB\SPI_37`

Quirk Use
---------

This plugin uses the following plugin-specific quirks:

| Quirk                      | Description                      | Minimum fwupd version |
|----------------------------|----------------------------------|-----------------------|
| `SpiCmdChipErase`          | Flash command to erase chip      | 1.3.3                 |
| `SpiCmdChipErase`          | Flash command to erase sector    | 1.3.3                 |
| `SpiCmdReadId`             | Flash command to read the ID     | 1.3.3                 |
| `SpiCmdReadIdSz`           | Size of the ReadId response      | 1.3.3                 |

The `SpiCmdReadId` and `SpiCmdReadIdSz` quirks have to be assigned to the device
instance attribute, rather then the flash part as the ID is required to query
the other flash chip parameters. For example:

    [DeviceInstanceId=USB\VID_2109&PID_0210]
    Plugin = vli_usbhub
    SpiCmdReadId = 0xf8
    SpiCmdReadIdSz = 4

    # W3IRDFLASHxxx
    [Guid=VLI_USBHUB\\SPI_37303840]
    SpiCmdChipErase = 0xc7
    SpiCmdSectorErase = 0x20

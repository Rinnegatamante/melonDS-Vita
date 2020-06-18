# melonDS-Vita

Vita port of MelonDS emulator

The source code is provided under the GPLv3 license.

## How to Use

melonDS requires BIOS/firmware copies from a DS. Files required:
 * bios7.bin, 16KB: ARM7 BIOS
 * bios9.bin, 4KB: ARM9 BIOS
 * firmware.bin, 128/256/512KB: firmware
 
Firmware boot requires a firmware dump from an original DS or DS Lite.
DS firmwares dumped from a DSi or 3DS aren't bootable and only contain configuration data, thus they are only suitable when booting games directly.

### Possible Firmware Sizes

 * 128KB: DSi/3DS DS-mode firmware (reduced size due to lacking bootcode)
 * 256KB: regular DS firmware
 * 512KB: iQue DS firmware

DS BIOS dumps from a DSi or 3DS can be used with no compatibility issues. DSi BIOS dumps (in DSi mode) are not compatible. Or maybe they are. I don't know.

## How to Build

### Linux:

Requires VitaSDK to compile. Directions for installing VitaSDK are found at [VitaSDK/VDPM](https://github.com/vitasdk/vdpm)

```sh
mkdir -p build/sce_sys
make
```

## How to Install

Transfer melonDS.vpk to the Vita

Run the vpk from an application like VitaShell to install

Create the directory ux0:data/melonDS

Transfer the three bios binaries to the melonDS directory along with any roms



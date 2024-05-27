# CraftOS-Pi
A port of CraftOS-PC for bare metal Raspberry Pi. Based on [CraftOS-ESP](https://github.com/MCJack123/CraftOS-ESP) code.

## Features
- CC: Tweaked 1.109.2 emulation
  - Lua 5.2 runtime
- VGA terminal display with 53x22 resolution
  - 640x400 output mode (may have borders)
- USB port for keyboard input
  - Supports all keys
- Supports reading and writing files on the SD card
- Wi-Fi/HTTP connectivity
  - Wi-Fi configuration stored in `/wpa_supplicant.conf`
  - HTTP/HTTPS supported
- Audio out through speaker peripheral
- Modem communication over 802.11 Wi-Fi (WIP)

## Requirements
Any Raspberry Pi (except 5), plus an SD card, HDMI display, and USB keyboard.

## License
CraftOS-Pi is licensed under the GPLv3 license.  
craftos2-lua is licensed under the MIT License.  
craftos2-rom is licensed under the Mozilla Public License and the ComputerCraft Public License.  
CraftOS-Pi contains portions of code from Circle and circle-stdlib, which are licensed under the GPLv3 License.  

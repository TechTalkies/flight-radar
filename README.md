# 📡 Micro Radar
This project is a fork of [Anthony Sturdy's Micro Radar project](https://github.com/AnthonySturdy/micro-radar?tab=readme-ov-file) and adapts it for inexpensive and readily available ESP32 hardware, rather than complex devices.

---
![alt](Images/sample.gif)

### [Watch the full build here](https://www.youtube.com/watch?v=KuVckV0wF9w)

---
### What's Different?

**This fork adds:**

- ESP32-S3 Zero support
- Round GC9A01 display support
- Rotary encoder navigation
- Aircraft selection and details screen
- Enhanced aircraft icons
- Multi-color aircraft rendering
- Onboard RGB status LED
- Simplified hardware requirements
---
## Flashing the Firmware

### Option 1: Flash Prebuilt Firmware (Recommended)

No development environment required.

1. Connect the ESP32 to your computer using a USB cable.

2. Open the Tech Talkies Flasher:

   https://techtalkies.github.io/flash.html

3. Select the firmware.

4. Click **Connect** and choose your ESP32.

5. Click **Install** and wait for the process to complete.

---

### Option 2: Build and Flash from Source

If you'd like to modify the firmware, build it yourself using PlatformIO.

1. Clone or download this repository.
2. Install:

   * Visual Studio Code
   * PlatformIO extension
3. Open the project folder in VS Code.
4. Allow PlatformIO to download the required dependencies.
5. Build and upload the firmware to your ESP32 using the PlatformIO toolbar. And check if it works without modifications first.

After making your changes, rebuild and upload the firmware to your device.


---
### Credits

https://github.com/AnthonySturdy/micro-radar

Many thanks to Anthony Sturdy for creating and open-sourcing the original project that made this fork possible.

# ESP32 Flashing Guide using Flash Download Tool

## Overview

This guide provides step-by-step instructions on flashing ESP32 firmware using the Flash Download Tool. The Flash Download Tool is a utility that allows you to burn firmware onto ESP32 devices. Follow these instructions carefully to ensure a successful flashing process.

### Prerequisites

Before you begin, make sure you have the following:

1. **ESP32 Device:** Ensure that you have an ESP32 development board or module.
2. **USB Cable:** A USB cable to connect your ESP32 device to your computer.
3. **Flash Download Tool:** Download and install the Flash Download Tool on your computer. You can download the tool from the following link: [Flash Download Tool 3.9.5](https://www.espressif.com/sites/default/files/tools/flash_download_tool_3.9.5.zip)
4. **CP2102 Driver:** If your computer does not automatically recognize the ESP32 device, you may need to install the CP2102 USB-to-UART bridge driver. Download the driver from the following link: [CP210x Universal Windows Driver](https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip)
5. **Binary Files:** Place the binary files in the same directory as this README file:


   - `bootloader.bin` at address `0x1000`
   - `partition.bin` at address `0x8000`
   - `SNIFR.bin` at address `0x20000`

### Steps

#### 1. Connect ESP32 to your Computer

Connect your ESP32 device to your computer using a USB cable. Make sure the device is properly connected and recognized by your computer.

#### 2. Install USB Drivers

- If you haven't already, install the CP2102 driver. Download it from the following link: [CP210x Universal Windows Driver](https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip)

#### 3. Open Flash Download Tool

Before proceeding, make sure to run the Flash Download Tool as an administrator. Right-click on the tool and select "Run as Administrator" to avoid any permission issues.

#### 4. Configure Flash Download Tool Settings

Configure the Flash Download Tool settings as follows:

- **Chip Type:** Select the appropriate ESP32 chip type.
- **Flash Mode:** Choose the correct flash mode (e.g., DIO, DOUT, QIO, QOUT).
- **Flash Frequency:** Set the flash frequency based on your ESP32 module specifications.
- **COM Port:** Select the COM port to which your ESP32 device is connected.

#### 5. Load Firmware

Click the "ESP32 DownloadTool" button to load the firmware. Select the firmware files with the appropriate addresses:
   - `bootloader.bin` at address `0x1000`
   - `partition.bin` at address `0x8000`
   - `SNIFR.bin` at address `0x20000`



#### 6. Enter Download Mode(skip)

Press and hold the BOOT button on your ESP32 device while clicking the "Start" button in the Flash Download Tool. Keep the BOOT button pressed until the flashing process is complete.

#### 7. Monitor Progress

Monitor the flashing progress in the Flash Download Tool. The progress bar will indicate the status of the flashing process.

#### 8. Complete Flashing

Once the flashing process is complete, you will see a "Download Complete" message in the Flash Download Tool. You can now release the BOOT button on your ESP32 device.

## Conclusion

Congratulations! You have successfully flashed the firmware onto your ESP32 device using the Flash Download Tool. If you encounter any issues, double-check your connections and settings before retrying the flashing process.

For additional troubleshooting and support, refer to the official documentation provided by Espressif or the community forums.
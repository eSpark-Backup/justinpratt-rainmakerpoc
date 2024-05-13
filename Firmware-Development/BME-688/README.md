# BME688-Firmware port from Arduino IDE to ESP-IDF



## How to use
~~You should first setup the communication method for the firmware, either I2C or SPI.~~ Default is SPI.

Use "idf.py menuconfig".
Build and flash firmware to your Adafruit HUZZAH32.

Connect BME688 development board with Adafruit HUZZAH32.

If at any point during operation device encounters a problem builtin LED (GPIO 13) will start flashing.
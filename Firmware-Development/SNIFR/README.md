# BME688-Firmware port from Arduino IDE to ESP-IDF



## How to use


## Instructions

If architecture other than ESP32 is used, than change the precompiled binary path to the binary of targeted architecture in 

components > Bosch-BSEC2-Library-master > CMakeLists.txt

target_link_libraries(${COMPONENT_TARGET} "-L${CMAKE_CURRENT_LIST_DIR}/Precompiled_binaries/esp32")
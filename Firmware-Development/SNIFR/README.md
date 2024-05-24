# BME-688 on ESP RainMaker


## How to use

To reset WiFi credentials press Boot (GPIO 0) button for 3-4secs.
To reset the device press Boot (GPIO 0) button for >5secs.

## Instructions

If architecture other than ESP32 is used, than change the precompiled binary path to the binary of targeted architecture in 

components > Bosch-BSEC2-Library-master > CMakeLists.txt

target_link_libraries(${COMPONENT_TARGET} "-L${CMAKE_CURRENT_LIST_DIR}/Precompiled_binaries/esp32")

## NOTE

Default timer stack "configTIMER_TASK_STACK_DEPTH" was modified to accomodate the function "app_sensor_update" which is created using a timer in function "app_sensor_init"
Default stack size was insuffecient.

configTIMER_TASK_STACK_DEPTH is located in file "FreeRTOSConfig.h"
# BME-688 on ESP RainMaker

SNIFR Project file is located in "${project-Directory}/appMain/SNIFR/main".

To make any changes, open the project in VSCode and navigate to "${project-Directory}/appMain/SNIFR" folder mentioned above. RUN "idf.py build" in terminal to start building the project.

This project was built with ESP-IDF v5.1.2


/* Configure the Sensor Power mode, ULP or LP mode

    18v/33v = Voltage at Vdd. 1.8V or 3.3V

    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP

    4d/28d = Operating age of the sensor in days

    bme688_sel_18v_3s_4d

    bme688_sel_18v_3s_28d

    bme688_sel_18v_300s_4d

    bme688_sel_18v_300s_28d

    bme688_sel_33v_3s_4d

    bme688_sel_33v_3s_28d

    bme688_sel_33v_300s_4d

    bme688_sel_33v_300s_28d

*/

Use one of the configuration files depending on desired sensor operation mode.

bsec_config_iaq[] = {path to one of the files mentioned above};


## How to use

To reset WiFi credentials press Boot (GPIO 0) button for 3-4secs.

To reset the device press Boot (GPIO 0) button for >10secs.

## Instructions

If architecture other than ESP32 is used, than change the precompiled binary path to the binary of targeted architecture in 

components > Bosch-BSEC2-Library-master > CMakeLists.txt

target_link_libraries(${COMPONENT_TARGET} "-L${CMAKE_CURRENT_LIST_DIR}/Precompiled_binaries/esp32")

## NOTE

Default timer stack "configTIMER_TASK_STACK_DEPTH" was modified to accomodate the function "app_sensor_update" which is created using a timer in function "app_sensor_init"
Default stack size was insuffecient.

configTIMER_TASK_STACK_DEPTH is located in file "FreeRTOSConfig.h"

![image](https://github.com/eSpark-Consultants/justinpratt-rainmakerpoc/assets/152279596/40fbf70a-9d50-414d-8e7b-34ad108d5dc9)
![image](https://github.com/eSpark-Consultants/justinpratt-rainmakerpoc/assets/152279596/f8d35dc9-fa86-4aa4-a755-e3f51d8be81b)


MQTT BUDGETTING

Disable MQTT budgeting in menuconfig.
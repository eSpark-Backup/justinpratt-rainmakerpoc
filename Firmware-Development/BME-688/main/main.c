#include <stdio.h>
#include <bsec2.h>
#include "driver/gpio.h"
#include "esp_log.h"
// #include "strings.h"

/* Macros used */
#define PANIC_LED   GPIO_NUM_2
#define ERROR_DUR   1000

static const char *TAG = "[MAIN]";

/* Helper functions declarations */
/**
 * @brief : This function toggles the led when a fault was detected
 */
void errLeds(void)
{
    const char *func_TAG = "errLeds:";
    ESP_LOGE(TAG, "%s Error occured", func_TAG);

    while(1)
    {
        gpio_set_level(PANIC_LED, 1);
        vTaskDelay(ERROR_DUR);
        gpio_set_level(PANIC_LED, 0);
        vTaskDelay(ERROR_DUR);
    }
}

/**
 * @brief : This function checks the BSEC status, prints the respective error code. Halts in case of error
 * @param[in] bsec  : Bsec2 class object
 */
void checkBsecStatus(Bsec2 bsec)
{
    const char *func_TAG = "checkBsecStatus:";

    if (bsec.status < BSEC_OK)
    {
        // ESP_LOGE(TAG, "%s BSEC error code : " + String(bsec.status), func_TAG);
        ESP_LOGE(TAG, "%s BSEC error code : %s", bsec.status.str(), func_TAG);
        errLeds(); /* Halt in case of failure */
    }
    else if (bsec.status > BSEC_OK)
    {
        ESP_LOGW(TAG, "%s BSEC warning code : %s", bsec.status.str(), func_TAG);
    }

    if (bsec.sensor.status < BME68X_OK)
    {
        ESP_LOGE(TAG, "%s BME68X error code : %s", bsec.sensor.status.str(), func_TAG);
        errLeds(); /* Halt in case of failure */
    }
    else if (bsec.sensor.status > BME68X_OK)
    {
        ESP_LOGW(TAG, "%s BME68X warning code : %s", bsec.sensor.status.str(), func_TAG);
    }
}

/**
 * @brief : This function is called by the BSEC library when a new output is available
 * @param[in] input     : BME68X sensor data before processing
 * @param[in] outputs   : Processed BSEC BSEC output data
 * @param[in] bsec      : Instance of BSEC2 calling the callback
 */
void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
    const char *func_TAG = "newDataCallback:";

    if (!outputs.nOutputs)
    {
        return;
    }
    long tempVar = (outputs.output[0].time_stamp / INT64_C(1000000));
    ESP_LOGI("BSEC outputs:\n\ttimestamp = %s", tempVar.str());
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output  = outputs.output[i];
        switch (output.sensor_id)
        {
            case BSEC_OUTPUT_IAQ:
                ESP_LOGI(TAG, "\tiaq = " + String(output.signal));
                ESP_LOGI(TAG, "\tiaq accuracy = " + String((int) output.accuracy));
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                ESP_LOGI(TAG, "\ttemperature = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                ESP_LOGI(TAG, "\tpressure = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                ESP_LOGI(TAG, "\thumidity = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RAW_GAS:
                ESP_LOGI(TAG, "\tgas resistance = " + String(output.signal));
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                ESP_LOGI(TAG, "\tstabilization status = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                ESP_LOGI(TAG, "\trun in status = " + String(output.signal));
                break;
            default:
                break;
        }
    }
}

/* Create an object of the class Bsec2 */
Bsec2 envSensor;

void app_main(void)
{
    const char *func_TAG = "app_main:";

    gpio_set_direction();
    gpio_set_level();


    /* Desired subscription list of BSEC2 outputs */
    bsecSensor sensorList[] = {
            BSEC_OUTPUT_IAQ,
            BSEC_OUTPUT_RAW_TEMPERATURE,
            BSEC_OUTPUT_RAW_PRESSURE,
            BSEC_OUTPUT_RAW_HUMIDITY,
            BSEC_OUTPUT_RAW_GAS,
            BSEC_OUTPUT_STABILIZATION_STATUS,
            BSEC_OUTPUT_RUN_IN_STATUS
    };

    /* Initialize the communication interfaces */
    // Serial.begin(115200);
    // Wire.begin();
    gpio_set_level(PANIC_LED, GPIO_MODE_OUTPUT);

    /* Valid for boards with USB-COM. Wait until the port is open */
    // while(!Serial) vTaskDelay(10);

    /* Initialize the library and interfaces */
    if (!envSensor.begin(BME68X_I2C_ADDR_LOW, Wire))
    {
        checkBsecStatus(envSensor);
    }

    /* Subsribe to the desired BSEC2 outputs */
    if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP))
    {
        checkBsecStatus(envSensor);
    }

    /* Whenever new data is available call the newDataCallback function */
    envSensor.attachCallback(newDataCallback);

    ESP_LOGI(TAG, "BSEC library version " + \
            String(envSensor.version.major) + "." \
            + String(envSensor.version.minor) + "." \
            + String(envSensor.version.major_bugfix) + "." \
            + String(envSensor.version.minor_bugfix));
    
    while(1){
        /* Call the run function often so that the library can 
        * check if it is time to read new data from the sensor  
        * and process it.
        */
        if (!envSensor.run())
        {
            checkBsecStatus(envSensor);
        }
    }
}

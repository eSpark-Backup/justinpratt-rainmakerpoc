#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "strings.h"
#include <stdio.h>
#include <bsec2.hpp>

/*
 You have to set this config value with menuconfig
 CONFIG_INTERFACE
 for i2c
 CONFIG_MODEL
 CONFIG_SDA_GPIO
 CONFIG_SCL_GPIO
 CONFIG_RESET_GPIO
 for SPI
 CONFIG_CS_GPIO
 CONFIG_DC_GPIO
 CONFIG_RESET_GPIO
*/


/* Macros used */
#define PANIC_LED           GPIO_NUM_2
#define ERROR_DUR           1000
#define I2C_NUM             I2C_NUM_0                               //i2c port
#define I2C_MASTER_FREQ_HZ  400000                                  //i2c communication frequency
#define I2C_ADDRESS         BME68X_I2C_ADDR_LOW                     //i2c address of BME688

static const char *TAG = "[MAIN]";

/* Helper functions declarations */
/**
 * @brief : This function toggles the led when a fault was detected
 */
void errLeds(void)
{
    ESP_LOGE(TAG, "%s: Error occured", __func__);

    while(1)
    {
        gpio_set_level(PANIC_LED, 1);
        vTaskDelay(ERROR_DUR/portTICK_PERIOD_MS);
        gpio_set_level(PANIC_LED, 0);
        vTaskDelay(ERROR_DUR/portTICK_PERIOD_MS);
    }
}

/**
 * @brief : This function checks the BSEC status, prints the respective error code. Halts in case of error
 * @param[in] bsec  : Bsec2 class object
 */
void checkBsecStatus(Bsec2 bsec)
{
    if (bsec.status < BSEC_OK)
    {
        // ESP_LOGE(TAG, "%s BSEC error code : " + String(bsec.status), func_TAG);
        ESP_LOGE(TAG, "%s: BSEC error code : %d", __func__, bsec.status);
        errLeds(); /* Halt in case of failure */
    }
    else if (bsec.status > BSEC_OK)
    {
        ESP_LOGW(TAG, "%s: BSEC warning code : %d", __func__, bsec.status);
    }

    if (bsec.sensor.status < BME68X_OK)
    {
        ESP_LOGE(TAG, "%s: BME68X error code : %d", __func__, bsec.sensor.status);
        errLeds(); /* Halt in case of failure */
    }
    else if (bsec.sensor.status > BME68X_OK)
    {
        ESP_LOGW(TAG, "%s: BME68X warning code : %d", __func__, bsec.sensor.status);
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
    if (!outputs.nOutputs)
    {
        return;
    }
    long tempVar = (outputs.output[0].time_stamp / INT64_C(1000000));
    ESP_LOGI(TAG, "BSEC outputs:\n\ttimestamp = %ld", tempVar);
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output  = outputs.output[i];
        switch (output.sensor_id)
        {
            case BSEC_OUTPUT_IAQ:
                ESP_LOGI(TAG, "\tiaq = %f", output.signal);
                ESP_LOGI(TAG, "\tiaq accuracy = %d", (int) output.accuracy);
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                ESP_LOGI(TAG, "\ttemperature = %f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                ESP_LOGI(TAG, "\tpressure = %f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                ESP_LOGI(TAG, "\thumidity = %f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_GAS:
                ESP_LOGI(TAG, "\tgas resistance = %f", output.signal);
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                ESP_LOGI(TAG, "\tstabilization status = %f", output.signal);
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                ESP_LOGI(TAG, "\trun in status = %f", output.signal);
                break;
            default:
                break;
        }
    }
}

/* Create an object of the class Bsec2 */
Bsec2 envSensor;

#ifdef __cplusplus
extern "C" {
#endif

void app_main(void)
{
    // const char *func_TAG = "app_main:";

    // gpio_set_direction();
    // gpio_set_level();


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
    gpio_set_direction(PANIC_LED, GPIO_MODE_OUTPUT);

#if CONFIG_I2C_INTERFACE
    i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = CONFIG_SDA_GPIO,
		.scl_io_num = CONFIG_SCL_GPIO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master{
            .clk_speed = I2C_MASTER_FREQ_HZ,    },
		.clk_flags = 0
	};
	ESP_LOGI(TAG, "INTERFACE is i2c");
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM, &i2c_config));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0));
    
    /* Initialize the library and interfaces */             //I2C interface
    if (!envSensor.begin(BME68X_I2C_ADDR_LOW, I2C_NUM))
    {
        checkBsecStatus(envSensor);
    }
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_MOSI_GPIO,    // Set appropriate MOSI pin if needed
        .miso_io_num = CONFIG_MISO_GPIO,    // Set appropriate MISO pin if needed
        .sclk_io_num = CONFIG_SCLK_GPIO,    // Set appropriate SCLK pin if needed
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0,
        .isr_cpu_id = INTR_CPU_ID_AUTO,
        .intr_flags = 0
    };

    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,                            // SPI mode 0
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 1000000,            // 1 MHz clock speed
        .input_delay_ns = 0,
        .spics_io_num = CONFIG_CS_GPIO,
        .flags = 0,
        .queue_size = 1,
        .pre_cb = nullptr,
        .post_cb = nullptr,
    };
    spi_device_handle_t spi;
    spi_host_device_t spi_host = (spi_host_device_t)CONFIG_SPI_INTERFACE; //SPI2_HOST;
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &dev_cfg, &spi));
	ESP_LOGI(TAG, "INTERFACE is SPI");
	ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d",CONFIG_MISO_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(TAG, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
	ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
    
    /* Initialize the library and interfaces */             //SPI interface
    if (!envSensor.begin(CONFIG_CS_GPIO, spi))
    {
        checkBsecStatus(envSensor);
    }
#endif // CONFIG_SPI_INTERFACE

    /* Valid for boards with USB-COM. Wait until the port is open */
    // while(!Serial) vTaskDelay(10);


    ESP_LOGI(TAG, "BSEC library version %d.%d.%d.%d",
            envSensor.version.major,
            envSensor.version.minor,
            envSensor.version.major_bugfix,
            envSensor.version.minor_bugfix);

    /* Subsribe to the desired BSEC2 outputs */
    if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP))
    {
        checkBsecStatus(envSensor);
    }

    /* Whenever new data is available call the newDataCallback function */
    envSensor.attachCallback(newDataCallback);

    
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
#ifdef __cplusplus
}
#endif

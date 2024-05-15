#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "strings.h"
#include <stdio.h>
#include <bsec2.hpp>
#include <commMux.hpp>

/* Communication Interface's Defines */
#define I2C_NUM             I2C_NUM_0                               //i2c port
#define I2C_MASTER_FREQ_HZ  400000                                  //i2c communication frequency
// #define I2C_ADDRESS         BME68X_I2C_ADDR_LOW                     //i2c address of BME688
#define CONFIG_SDA_GPIO     GPIO_NUM_23
#define CONFIG_SCL_GPIO     GPIO_NUM_22
////-----------------------------------
#define SPI_COMM_SPEED      32000000
#define CONFIG_MOSI_GPIO    GPIO_NUM_18
#define CONFIG_MISO_GPIO    GPIO_NUM_19
#define CONFIG_SCLK_GPIO    GPIO_NUM_5
#define CONFIG_CS_GPIO      GPIO_NUM_33
#define SPI_INTERFACE       SPI2_HOST


/* Macros used */
#define PANIC_LED           GPIO_NUM_13                             //GPIO_NUM_2 on esp32, GPIO_NUM_13 on huzzah32
#define ERROR_DUR           1000
#define DEV_KIT_SENSORS     8
#define MEAS_DUR            140
// #define MOUNT_POINT         "/Dev_Kit"                              //SD Card
// #define LOG_FILE_NAME       "/BME688_Datalogger_Log.csv"            //SD Card
// #define MAX_CHAR_SIZE       64

static const char *TAG = "[MAIN]";


Bme68x bme[DEV_KIT_SENSORS];
commMux commSetup[DEV_KIT_SENSORS];
uint8_t lastMeasindex[DEV_KIT_SENSORS] = {0};
bme68xData sensorData[DEV_KIT_SENSORS] = {0};
String logHeader;
uint32_t lastLogged = 0;

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


#ifdef __cplusplus
extern "C" {
#endif

void app_main(void)
{
    /* Initialize the communication interfaces */
    gpio_set_direction(PANIC_LED, GPIO_MODE_OUTPUT);

    //I2C interface
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
	ESP_LOGI(TAG, "/* I2C INTERFACE */");
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM, &i2c_config));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM, i2c_config.mode, 0, 0, 0));

    //SPI interface
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_MOSI_GPIO,
        .miso_io_num = CONFIG_MISO_GPIO,
        .sclk_io_num = CONFIG_SCLK_GPIO,
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
        .clock_speed_hz = SPI_COMM_SPEED,
        .input_delay_ns = 0,
        .spics_io_num = CONFIG_CS_GPIO,
        .flags = 0,
        .queue_size = 1,
        .pre_cb = nullptr,
        .post_cb = nullptr,
    };
    spi_device_handle_t spi;
    spi_host_device_t spi_host = (spi_host_device_t)SPI_INTERFACE; //SPI2_HOST;
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &dev_cfg, &spi));
	ESP_LOGI(TAG, "/* SPI INTERFACE */");
	ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(TAG, "CONFIG_MISO_GPIO=%d",CONFIG_MISO_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(TAG, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);

    commMuxBegin(I2C_NUM, spi);
    vTaskDelay(100 /portTICK_PERIOD_MS);

    /* Communication interface set for all the 8 sensors in the development kit */
    for (uint8_t i = 0; i < DEV_KIT_SENSORS; i++) {
        commSetup[i] = commMuxSetConfig(I2C_NUM, spi, i, commSetup[i]);
        // setChipSelect(I2C_NUM, commSetup[i].select);
        bme[i].begin(BME68X_SPI_INTF, commMuxRead, commMuxWrite, commMuxDelay, &commSetup[i]);
            if(bme[i].checkStatus()) {
                ESP_LOGE(TAG, "Initializing sensor '%d' failed with error: %s", i, bme[i].statusString().c_str());
                errLeds();
            }
    }

    /* Setting the default heater profile configuration */
    for (uint8_t i = 0; i < DEV_KIT_SENSORS; i++) {
        ESP_LOGD(TAG, "Performing profile configuration on sensor '%d'.", i);
        
        bme[i].setTPH();

        /* Heater temperature in degree Celsius as per the suggested heater profile
            */
        uint16_t tempProf[10] = {320, 100, 100, 100, 200, 200, 200, 320, 320, 320};
        /* Multiplier to the shared heater duration */
        uint16_t mulProf[10] = {5, 2, 10, 30, 5, 5, 5, 5, 5, 5};
        /* Shared heating duration in milliseconds */
        uint16_t sharedHeatrDur = MEAS_DUR - bme[i].getMeasDur(BME68X_PARALLEL_MODE);

        bme[i].setHeaterProf(tempProf, mulProf, sharedHeatrDur, 10);

        /* Parallel mode of sensor operation */
        bme[i].setOpMode(BME68X_PARALLEL_MODE);
        ESP_LOGD(TAG, "Configuration succesfull.");
    }
    
    while(1){
        uint8_t nFieldsLeft = 0;
        int16_t indexDiff;
        bool newLogdata = false;
        /* Control loop for data acquisition - checks if the data is available */
        if ((millis() - lastLogged) >= MEAS_DUR) {

            lastLogged = millis();
            for (uint8_t i = 0; i < DEV_KIT_SENSORS; i++) {
                if (bme[i].fetchData()) {
                    do {
                        nFieldsLeft = bme[i].getData(sensorData[i]);
                        /* Check if new data is received */
                        if (sensorData[i].status & BME68X_NEW_DATA_MSK) {
                            /* Inspect miss of data index */
                            indexDiff = (int16_t)sensorData[i].meas_index - (int16_t)lastMeasindex[i];
                            if (indexDiff > 1) {

                                ESP_LOGE(TAG, "Skip I: %d , DIFF: %d, MI: %d, LMI: %d, S: %X",
                                         i, indexDiff, sensorData[i].meas_index, lastMeasindex[i], sensorData[i].status);
                                errLeds();
                            }
                            lastMeasindex[i] = sensorData[i].meas_index;

                            logHeader += millis();
                            logHeader += ",";
                            logHeader += i;
                            logHeader += ",";
                            logHeader += sensorData[i].temperature;
                            logHeader += ",";
                            logHeader += sensorData[i].pressure;
                            logHeader += ",";
                            logHeader += sensorData[i].humidity;
                            logHeader += ",";
                            logHeader += sensorData[i].gas_resistance;
                            logHeader += ",";
                            logHeader += sensorData[i].gas_index;
                            logHeader += ",";
                            logHeader += sensorData[i].meas_index;
                            logHeader += ",";
                            logHeader += sensorData[i].idac;
                            logHeader += ",";
                            // logHeader += String(sensorData[i].status, HEX);
                            // logHeader += ",";
                            logHeader += sensorData[i].status & BME68X_GASM_VALID_MSK;
                            logHeader += ",";
                            logHeader += sensorData[i].status & BME68X_HEAT_STAB_MSK;
                            logHeader += "\r\n";
                            newLogdata = true;
                        }
                    } while (nFieldsLeft);
                }
            }
        }

        if (newLogdata) {
            newLogdata = false;

            gpio_set_level(PANIC_LED, 1);

            ESP_LOGI(TAG, "Readings: %s", logHeader.c_str());
            logHeader = "";

            gpio_set_level(PANIC_LED, 0);
        }
    }
}
#ifdef __cplusplus
}
#endif

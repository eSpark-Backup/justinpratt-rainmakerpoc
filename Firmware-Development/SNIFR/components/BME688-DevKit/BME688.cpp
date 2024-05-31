#include "BME688.h"
#include "esp_log.h"
#include "esp_system.h"

/* Communication Interface's Defines */
#define I2C_NUM             I2C_NUM_0                               //i2c port
#define I2C_MASTER_FREQ_HZ  UINT32_C(400000)                        //i2c communication frequency
#define CONFIG_SDA_GPIO     GPIO_NUM_23
#define CONFIG_SCL_GPIO     GPIO_NUM_22
////-----------------------------------
#define SPI_COMM_SPEED      UINT32_C(8000000)                       //32000000
#define CONFIG_MOSI_GPIO    GPIO_NUM_18
#define CONFIG_MISO_GPIO    GPIO_NUM_19
#define CONFIG_SCLK_GPIO    GPIO_NUM_5
#define CONFIG_CS_GPIO      GPIO_NUM_3
#define SPI_INTERFACE       SPI2_HOST


/* Macros used */
#define PANIC_LED           GPIO_NUM_13                         //GPIO_NUM_2 on esp32, GPIO_NUM_13 on huzzah32
#define ERROR_DUR           1000
#define DEV_KIT_SENSORS     1                   //8             //using only one sensor for reporting to rainmaker
#define MEAS_DUR            140

static const char *TAG = "[BME688]";


Bsec2 envSensor[DEV_KIT_SENSORS];
commMux commSetup[DEV_KIT_SENSORS];
uint8_t bsecMemBlock[DEV_KIT_SENSORS][BSEC_INSTANCE_SIZE];
uint8_t sensor = 0;
spi_device_handle_t spi;

/* Helper functions declarations */
/**
 * @brief : This function toggles the led when a fault was detected
 */
void ledBlink(void *pvParameter)
{
    while(1)
    {
        // ESP_LOGW(TAG, "Free heap size %lu", esp_get_free_heap_size());
        gpio_set_level(PANIC_LED, 1);
        vTaskDelay((ERROR_DUR*3)/portTICK_PERIOD_MS);
        // ESP_LOGW(TAG, "Free heap size %lu", esp_get_free_heap_size());
        gpio_set_level(PANIC_LED, 0);
        vTaskDelay(ERROR_DUR/portTICK_PERIOD_MS);
    }
}


#ifdef __cplusplus
extern "C" {
#endif

void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs)
    {
        return;
    }

    ESP_LOGI(TAG, "BSEC outputs:\n\tsensor num = %d", sensor);
    ESP_LOGI(TAG, "\ttimestamp = %d", (int) (outputs.output[0].time_stamp / INT64_C(1000000)));
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output  = outputs.output[i];
        switch (output.sensor_id)
        {
            case BSEC_OUTPUT_IAQ:
                ESP_LOGI(TAG, "\tiaq = %0.3f", output.signal);
                ESP_LOGI(TAG, "\tiaq accuracy = %d", (int) output.accuracy);
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                ESP_LOGI(TAG, "\ttemperature = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                ESP_LOGI(TAG, "\tpressure = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                ESP_LOGI(TAG, "\thumidity = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_RAW_GAS:
                ESP_LOGI(TAG, "\tgas resistance = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                ESP_LOGI(TAG, "\tstabilization status = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                ESP_LOGI(TAG, "\trun in status = %0.3f", output.signal);
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                g_gas_ppm = output.signal;
                ESP_LOGI(TAG, "\tBSEC_OUTPUT_BREATH_VOC_EQUIVALENT in status = %0.3f", g_gas_ppm);
                break;
            default:
                break;
        }
    }
}

esp_err_t checkBsecStatus(Bsec2 bsec)
{
    if (bsec.status < BSEC_OK)
    {
        ESP_LOGE(TAG, "%s: BSEC error code : %d", __func__, bsec.status);
        return ESP_FAIL;
    }
    else if (bsec.status > BSEC_OK)
    {
        ESP_LOGW(TAG, "%s: BSEC warning code : %d", __func__, bsec.status);
    }

    if (bsec.sensor.status < BME68X_OK)
    {
        ESP_LOGE(TAG, "%s: BME68X error code : %d", __func__, bsec.sensor.status);
        return ESP_FAIL;
    }
    else if (bsec.sensor.status > BME68X_OK)
    {
        ESP_LOGW(TAG, "%s: BME68X warning code : %d", __func__, bsec.sensor.status);
    }

    ESP_LOGI(TAG, "%s: Status OK.", __func__);
    return ESP_OK;
}

esp_err_t BME688Init(void)
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
    spi_host_device_t spi_host = (spi_host_device_t)SPI_INTERFACE; //SPI2_HOST;
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &dev_cfg, &spi));
	ESP_LOGI(TAG, "/* SPI INTERFACE */");
	ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d",CONFIG_MISO_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(TAG, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);

    /* Desired subscription list of BSEC2 outputs */
    bsecSensor sensorList[] = {
            // BSEC_OUTPUT_IAQ,
            // BSEC_OUTPUT_RAW_TEMPERATURE,
            // BSEC_OUTPUT_RAW_PRESSURE,
            // BSEC_OUTPUT_RAW_HUMIDITY,
            // BSEC_OUTPUT_RAW_GAS,
            // BSEC_OUTPUT_STABILIZATION_STATUS,
            // BSEC_OUTPUT_RUN_IN_STATUS,
            BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
    };

    if(commMuxBegin(I2C_NUM, spi) != ESP_OK){
        return ESP_FAIL;
    }
    vTaskDelay(100 /portTICK_PERIOD_MS);

    /* Communication interface set for all the 8 sensors in the development kit */
    for (uint8_t i = 0; i < DEV_KIT_SENSORS; i++) {
        commSetup[i] = commMuxSetConfig(I2C_NUM, spi, i, commSetup[i]);
        envSensor[i].allocateMemory(bsecMemBlock[i]);
        if (!envSensor[i].begin(BME68X_SPI_INTF, commMuxRead, commMuxWrite, commMuxDelay, &commSetup[i]))
        {
            //run if errors were encountered while running envSensor[i].begin()
            if(checkBsecStatus (envSensor[i]) != ESP_OK){
                ESP_LOGW(TAG, "%s: Error encountered while setting up DevKit Sensor %d", __func__, i);
                return ESP_FAIL;
            }
        }

        if (!envSensor[i].updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP))
        {
            if(checkBsecStatus (envSensor[i]) != ESP_OK){
                ESP_LOGW(TAG, "%s: Error encountered while setting up DevKit Sensor %d's subscription.", __func__, i);
                return ESP_FAIL;
            }
        }

        envSensor[i].attachCallback(newDataCallback);
    }
    ESP_LOGI(TAG, "BSEC library version %d.%d.%d.%d"
            , envSensor[0].version.major
            , envSensor[0].version.minor
            , envSensor[0].version.major_bugfix
            , envSensor[0].version.minor_bugfix);
    
    return ESP_OK;
}

esp_err_t BME688Run(void){
        for (sensor = 0; sensor < DEV_KIT_SENSORS; sensor++)
        {
            if (!envSensor[sensor].run())
            {
                if(checkBsecStatus(envSensor[sensor]) != ESP_OK){
                    return ESP_FAIL;
                }
            }
            vTaskDelay(10 /portTICK_PERIOD_MS);
        }
        return ESP_OK;
}

#ifdef __cplusplus
}
#endif

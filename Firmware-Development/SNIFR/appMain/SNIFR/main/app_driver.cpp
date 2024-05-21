/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

#include <app_reset.h>
// #include <ws2812_led.h>
#include "app_priv.h"
#include "BME688.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  (button_active_t)0
/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO    CONFIG_EXAMPLE_OUTPUT_GPIO

/* These values correspoind to H,S,V = 120,100,10 */
#define DEFAULT_RED     0
#define DEFAULT_GREEN   25
#define DEFAULT_BLUE    0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static const char *TAG = "[app_driver]";

static bool g_power_state = DEFAULT_SWITCH_POWER;
// static float g_temperature = DEFAULT_TEMPERATURE;
// static int g_gas_ppm = DEFAULT_GAS_PPM;
static TimerHandle_t sensor_timer;

#ifdef __cplusplus
extern "C" {
#endif

static void app_sensor_update(TimerHandle_t handle)
{
    if(BME688Run()){
        esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(gas_sensor_device, ESP_RMAKER_PARAM_GAS),
                    esp_rmaker_float(g_gas_ppm));
    }
}

float app_get_current_gas_value()
{
    return g_gas_ppm;
}

esp_err_t app_sensor_init(void)
{
    // g_temperature = DEFAULT_TEMPERATURE;
    g_gas_ppm = DEFAULT_GAS_PPM;
    if(BME688Init() == ESP_OK){
        ESP_LOGI(TAG, "%s: Creating timer for BME688Run.", __func__);
        // sensor_timer = xTimerCreate("BME688Run", (REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
        //                         pdTRUE, NULL, BME688Run);
        sensor_timer = xTimerCreate("app_sensor_update", (REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
                                pdTRUE, NULL, app_sensor_update);
    }
    if (sensor_timer) {
        xTimerStart(sensor_timer, 0);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void push_btn_cb(void *arg)
{
    bool new_state = !g_power_state;
    app_driver_set_state(new_state);
    esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(switch_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(new_state));
}

static void set_power_state(bool target)
{
    gpio_set_level((gpio_num_t)OUTPUT_GPIO, target);
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create((gpio_num_t)BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* Configure power */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = (gpio_pullup_t)1
    };
    io_conf.pin_bit_mask = ((uint64_t)1 << (gpio_num_t)OUTPUT_GPIO);
    /* Configure the GPIO */
    gpio_config(&io_conf);
    if(app_sensor_init() != ESP_OK){
        ESP_LOGE(TAG, "%s: Sensor Initializing failed. Check connections.", __func__);
    }
}

// int IRAM_ATTR app_driver_set_state(bool state)
int app_driver_set_state(bool state)
{
    if(g_power_state != state) {
        g_power_state = state;
        set_power_state(g_power_state);
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_power_state;
}

#ifdef __cplusplus
}
#endif
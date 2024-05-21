#pragma once

#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "strings.h"
#include <stdio.h>
// #include "app_priv.h"
#include <bsec2.hpp>
#include <commMux.hpp>


static int g_gas_ppm = 0;//DEFAULT_GAS_PPM;

#ifdef __cplusplus
extern "C" {
#endif

void ledBlink(void *pvParameter);
esp_err_t BME688Init(void);
esp_err_t BME688Run(void);

#ifdef __cplusplus
}
#endif
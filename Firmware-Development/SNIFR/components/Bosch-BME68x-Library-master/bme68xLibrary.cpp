/**
 * Copyright (c) 2021 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file    bme68xLibrary.cpp
 * @date    8 Feb 2022
 * @version 1.1.40407
 *
 */

#include <string.h>
#include "esp_log.h"

#include "bme68xLibrary.h"

static const char *TAG = "[bme68xLibrary]";

#define ERROR_DUR           1000

/* Maximum transaction size. Field size 17 x 3 */
#define BME68X_MAX_READ_LENGTH 51

#ifdef ARDUINO_ARCH_ESP32
#define BME68X_I2C_BUFFER_SIZE I2C_BUFFER_LENGTH

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_MBED
/* Assuming all MBED implementations of Wire have 256 byte sized buffers */
#define BME68X_I2C_BUFFER_SIZE 256

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_ESP8266
#define BME68X_I2C_BUFFER_SIZE BUFFER_LENGTH

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_AVR
#define BME68X_I2C_BUFFER_SIZE BUFFER_LENGTH

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_NRF52
#define BME68X_I2C_BUFFER_SIZE SERIAL_BUFFER_SIZE

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_SAMD
/* Assuming all Arduino's and Adafruit's SAMD
 * implementations of Wire have 256 byte sized buffers */
#define BME68X_I2C_BUFFER_SIZE 256

#define BME68X_BURST_SPI_TRANSFER
#endif

#ifdef ARDUINO_ARCH_SAM
#define BME68X_I2C_BUFFER_SIZE BUFFER_LENGTH

#define BME68X_BURST_SPI_TRANSFER
#endif

/* Optimistically assume support for at least 64 byte reads */
#ifndef BME68X_I2C_BUFFER_SIZE
#define BME68X_I2C_BUFFER_SIZE 64
#endif

#if BME68X_MAX_READ_LENGTH > BME68X_I2C_BUFFER_SIZE
#warning "Wire read requires a larger buffer size. Use SPI"
#endif

// /////----------------------microsecondDelay-------------------------
#define NOP() asm volatile ("nop")

// unsigned long IRAM_ATTR micros()
// {
//     return (unsigned long) (esp_timer_get_time());
// }
// void IRAM_ATTR delayMicroseconds(uint32_t us)
// {
//     uint32_t m = micros();
//     if(us){
//         uint32_t e = (m + us);
//         if(m > e){ //overflow
//             while(micros() > e){
//                 NOP();
//             }
//         }
//         while(micros() < e){
//             NOP();
//         }
//     }
// }
// /////----------------------microsecondDelay-------------------------

Bme68x::Bme68x(void)
{
	comm.i2c.wireobj = (i2c_port_t) NULL;
	comm.i2c.i2cAddr = 0;
	comm.spi.spiobj = NULL;
	comm.spi.cs = 0;
	status = BME68X_OK;
	memset(&bme6, 0, sizeof(bme6));
	memset(&conf, 0, sizeof(conf));
	memset(&heatrConf, 0, sizeof(heatrConf));
	memset(sensorData, 0, sizeof(sensorData));
	bme6.amb_temp = 25; /* Typical room temperature in Celsius */
	nFields = 0;
	iFields = 0;
	lastOpMode = BME68X_SLEEP_MODE;
}

/**
 * @brief Function to initialize the sensor based on custom callbacks
*/
void Bme68x::begin(bme68xIntf intf, bme68x_read_fptr_t read, bme68x_write_fptr_t write,
		bme68x_delay_us_fptr_t idleTask, void *intfPtr)
{

	bme6.intf = intf;
	bme6.read = read;
	bme6.write = write;
	bme6.delay_us = idleTask;
	bme6.intf_ptr = intfPtr;
	bme6.amb_temp = 25;

	status = bme68x_init(&bme6);
}

/**
 * @brief Function to initialize the sensor based on ESP32 hardware i2c library
 */
void Bme68x::begin(uint8_t i2cAddr, i2c_port_t i2c, bme68x_delay_us_fptr_t idleTask)
{
	comm.i2c.i2cAddr = i2cAddr;
	comm.i2c.wireobj = i2c;
	bme6.intf = BME68X_I2C_INTF;
	bme6.read = bme68xI2cRead;
	bme6.write = bme68xI2cWrite;
	bme6.delay_us = idleTask;
	bme6.intf_ptr = &comm;
	bme6.amb_temp = 25;

	status = bme68x_init(&bme6);
}

/**
 * @brief Function to initialize the sensor based on the SPI library
 */
void Bme68x::begin(uint8_t chipSelect, spi_device_handle_t &spi, bme68x_delay_us_fptr_t idleTask)
{
	gpio_reset_pin((gpio_num_t) chipSelect);
	gpio_set_direction((gpio_num_t) chipSelect, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t) chipSelect, 1);
	vTaskDelay(1/portTICK_PERIOD_MS);
	gpio_set_level((gpio_num_t) chipSelect, 0); /* Switch to SPI with a dummy transaction */
	vTaskDelay(1/portTICK_PERIOD_MS);
	gpio_set_level((gpio_num_t) chipSelect, 1);
	comm.spi.cs = chipSelect;

	comm.spi.spiobj = spi;
	bme6.intf = BME68X_SPI_INTF;
	bme6.read = bme68xSpiRead;
	bme6.write = bme68xSpiWrite;
	bme6.delay_us = idleTask;
	bme6.intf_ptr = &comm;
	bme6.amb_temp = 25;

	status = bme68x_init(&bme6);
}

/**
 * @brief Function to read a register
 */
uint8_t Bme68x::readReg(uint8_t regAddr)
{
	uint8_t regData;
	readReg(regAddr, &regData, 1);
	return regData;
}

/**
 * @brief Function to read multiple registers
 */
void Bme68x::readReg(uint8_t regAddr, uint8_t *regData, uint32_t length)
{
	status = bme68x_get_regs(regAddr, regData, length, &bme6);
}

/**
 * @brief Function to write data to a register
 */
void Bme68x::writeReg(uint8_t regAddr, uint8_t regData)
{
	status = bme68x_set_regs(&regAddr, &regData, 1, &bme6);
}

/**
 * @brief Function to write multiple registers
 */
void Bme68x::writeReg(uint8_t *regAddr, const uint8_t *regData, uint32_t length)
{
	status = bme68x_set_regs(regAddr, regData, length, &bme6);
}

/**
 * @brief Function to trigger a soft reset
 */
void Bme68x::softReset(void)
{
	status = bme68x_soft_reset(&bme6);
}

/**
 * @brief Function to set the ambient temperature for better configuration
 */ 
void Bme68x::setAmbientTemp(int8_t temp)
{
	bme6.amb_temp = temp;
}

/**
 * @brief Function to get the measurement duration in microseconds
 */
uint32_t Bme68x::getMeasDur(uint8_t opMode)
{
	if (opMode == BME68X_SLEEP_MODE)
		opMode = lastOpMode;

	return bme68x_get_meas_dur(opMode, &conf, &bme6);
}

/**
 * @brief Function to set the operation mode
 */
void Bme68x::setOpMode(uint8_t opMode)
{
	status = bme68x_set_op_mode(opMode, &bme6);
	if ((status == BME68X_OK) && (opMode != BME68X_SLEEP_MODE))
		lastOpMode = opMode;
}

/**
 * @brief Function to get the operation mode
 */
uint8_t Bme68x::getOpMode(void)
{
	uint8_t opMode;
	status = bme68x_get_op_mode(&opMode, &bme6);
	return opMode;
}

/**
 * @brief Function to get the Temperature, Pressure and Humidity over-sampling
 */
void Bme68x::getTPH(uint8_t &osHum, uint8_t &osTemp, uint8_t &osPres)
{
	status = bme68x_get_conf(&conf, &bme6);

	if (status == BME68X_OK)
	{
		osHum = conf.os_hum;
		osTemp = conf.os_temp;
		osPres = conf.os_pres;
	}
}

/**
 * @brief Function to set the Temperature, Pressure and Humidity over-sampling
 */
void Bme68x::setTPH(uint8_t osTemp, uint8_t osPres, uint8_t osHum)
{
	ESP_LOGD(TAG, "%s: running bme68x_get_conf.", __func__);
	status = bme68x_get_conf(&conf, &bme6);
	ESP_LOGD(TAG, "%s: bme68x_get_conf succesfull.", __func__);

	if (status == BME68X_OK)
	{
		conf.os_hum = osHum;
		conf.os_temp = osTemp;
		conf.os_pres = osPres;

		ESP_LOGD(TAG, "%s: running bme68x_set_conf.", __func__);
		status = bme68x_set_conf(&conf, &bme6);
		ESP_LOGD(TAG, "%s: bme68x_set_conf succesfull.", __func__);
	}
}

/**
 * @brief Function to get the filter configuration
 */
uint8_t Bme68x::getFilter(void)
{
	status = bme68x_get_conf(&conf, &bme6);

	return conf.filter;
}

/**
 * @brief Function to set the filter configuration
 */
void Bme68x::setFilter(uint8_t filter)
{
	status = bme68x_get_conf(&conf, &bme6);

	if (status == BME68X_OK)
	{
		conf.filter = filter;

		status = bme68x_set_conf(&conf, &bme6);
	}
}

/**
 * @brief Function to get the sleep duration during Sequential mode
 */
uint8_t Bme68x::getSeqSleep(void)
{
	status = bme68x_get_conf(&conf, &bme6);

	return conf.odr;
}

/**
 * @brief Function to set the sleep duration during Sequential mode
 */
void Bme68x::setSeqSleep(uint8_t odr)
{
	status = bme68x_get_conf(&conf, &bme6);

	if (status == BME68X_OK)
	{
		conf.odr = odr;

		status = bme68x_set_conf(&conf, &bme6);
	}
}

/**
 * @brief Function to set the heater profile for Forced mode
 */
void Bme68x::setHeaterProf(uint16_t temp, uint16_t dur)
{
	heatrConf.enable = BME68X_ENABLE;
	heatrConf.heatr_temp = temp;
	heatrConf.heatr_dur = dur;

	status = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatrConf, &bme6);
}

/**
 * @brief Function to set the heater profile for Sequential mode
 */
void Bme68x::setHeaterProf(uint16_t *temp, uint16_t *dur, uint8_t profileLen)
{
	heatrConf.enable = BME68X_ENABLE;
	heatrConf.heatr_temp_prof = temp;
	heatrConf.heatr_dur_prof = dur;
	heatrConf.profile_len = profileLen;

	status = bme68x_set_heatr_conf(BME68X_SEQUENTIAL_MODE, &heatrConf, &bme6);
}

/**
 * @brief Function to set the heater profile for Parallel mode
 */
void Bme68x::setHeaterProf(uint16_t *temp, uint16_t *mul, uint16_t sharedHeatrDur, uint8_t profileLen)
{
	heatrConf.enable = BME68X_ENABLE;
	heatrConf.heatr_temp_prof = temp;
	heatrConf.heatr_dur_prof = mul;
	heatrConf.shared_heatr_dur = sharedHeatrDur;
	heatrConf.profile_len = profileLen;

	status = bme68x_set_heatr_conf(BME68X_PARALLEL_MODE, &heatrConf, &bme6);
}

/**
 * @brief Function to fetch data from the sensor into the local buffer
 */
uint8_t Bme68x::fetchData(void)
{
	nFields = 0;
	status = bme68x_get_data(lastOpMode, sensorData, &nFields, &bme6);
	iFields = 0;

	return nFields;
}

/**
 * @brief Function to get a single data field
 */
uint8_t Bme68x::getData(bme68xData &data)
{
	if (lastOpMode == BME68X_FORCED_MODE)
	{
		data = sensorData[0];
	} else
	{
		if (nFields)
		{
			/* iFields spans from 0-2 while nFields spans from
			 * 0-3, where 0 means that there is no new data
			 */
			data = sensorData[iFields];
			iFields++;

			/* Limit reading continuously to the last fields read */
			if (iFields >= nFields)
			{
				iFields = nFields - 1;
				return 0;
			}

			/* Indicate if there is something left to read */
			return nFields - iFields;
		}
	}

	return 0;
}

/**
 * @brief Function to get whole sensor data
 */
bme68xData* Bme68x::getAllData(void)
{
	return sensorData;
}

/**
 * @brief Function to get the BME68x heater configuration
 */
const bme68xHeatrConf& Bme68x::getHeaterConfiguration(void)
{
	return heatrConf;
}

/**
 * @brief Function to retrieve the sensor's unique ID
 */
uint32_t Bme68x::getUniqueId(void)
{
    uint8_t id_regs[4];
    uint32_t uid;
    readReg(BME68X_REG_UNIQUE_ID, id_regs, 4);

    uint32_t id1 = ((uint32_t) id_regs[3] + ((uint32_t) id_regs[2] << 8)) & 0x7fff;
    uid = (id1 << 16) + (((uint32_t) id_regs[1]) << 8) + (uint32_t) id_regs[0];

    return uid;
}

/**
 * @brief Function to get the error code of the interface functions
 */
BME68X_INTF_RET_TYPE Bme68x::intfError(void)
{
	return bme6.intf_rslt;
}

/**
 * @brief Function to check if an error / warning has occurred
 */
int8_t Bme68x::checkStatus(void)
{
	if (status < BME68X_OK)
	{
		return BME68X_ERROR;
	}
	else if(status > BME68X_OK)
	{
		return BME68X_WARNING;
	}
	else
	{
		return BME68X_OK;
	}
}

/**
 * @brief Function to get a brief text description of the error
 */
String Bme68x::statusString(void)
{
	String ret = "";
	switch (status)
	{
	case BME68X_OK:
		/* Don't return a text for OK */
		break;
	case BME68X_E_NULL_PTR:
		ret = "Null pointer";
		break;
	case BME68X_E_COM_FAIL:
		ret = "Communication failure";
		break;
	case BME68X_E_DEV_NOT_FOUND:
		ret = "Sensor not found";
		break;
	case BME68X_E_INVALID_LENGTH:
		ret = "Invalid length";
		break;
	case BME68X_W_DEFINE_OP_MODE:
		ret = "Set the operation mode";
		break;
	case BME68X_W_NO_NEW_DATA:
		ret = "No new data";
		break;
	case BME68X_W_DEFINE_SHD_HEATR_DUR:
		ret = "Set the shared heater duration";
		break;
	default:
		ret = "Undefined error code";
	}

	return ret;
}

/**
 * @brief Function that implements the default microsecond delay callback
 */
void bme68xDelayUs(uint32_t periodUs, void *intfPtr) {
    (void) intfPtr;

	uint32_t m = (unsigned long) (esp_timer_get_time());
    if(periodUs){
        uint32_t e = (m + periodUs);
        if(m > e){ //overflow
            while((unsigned long) (esp_timer_get_time()) > e){
                NOP();
            }
        }
        while((unsigned long) (esp_timer_get_time()) < e){
            NOP();
        }
    }
}

/**
 * @brief Function that implements the default SPI write transaction
 */
int8_t bme68xSpiWrite(uint8_t regAddr, const uint8_t *regData, uint32_t length, void *intfPtr) {
    bme68xScommT *comm = NULL;

    if (intfPtr) {
        comm = (bme68xScommT *)intfPtr;

        if (comm->spi.spiobj) {
			uint8_t *buffer = (uint8_t *)malloc(length + 1);
			if (buffer == NULL) {
				ESP_LOGE(TAG, "%s: Failed to allocate memory for SPI transaction", __func__);
				return 1;
			}
			buffer[0] = regAddr & 0x7F;  // Clear the MSB for write operation
			memcpy(&buffer[1], regData, length);

            spi_transaction_t t;
            memset(&t, 0, sizeof(t));
            t.length = 8 * (1 + length); // Command + Data
			t.tx_buffer = buffer;

            gpio_set_level((gpio_num_t)comm->spi.cs, 0);

			if(spi_device_transmit(comm->spi.spiobj, &t) != ESP_OK){
				ESP_LOGE(TAG,"%s: Communication failure. Could not write register.", __func__);
			}
			free(buffer);
			
            gpio_set_level((gpio_num_t)comm->spi.cs, 1);
        } else {
            return BME68X_E_NULL_PTR;
        }
    } else {
        return BME68X_E_NULL_PTR;
    }

    return BME68X_OK;
}

/**
 * @brief Function that implements the default SPI read transaction
 */
int8_t bme68xSpiRead(uint8_t regAddr, uint8_t *regData, uint32_t length, void *intfPtr) {
    bme68xScommT *comm = NULL;

    if (intfPtr) {
        comm = (bme68xScommT *)intfPtr;

        if (comm->spi.spiobj) {
			uint8_t *buffer = (uint8_t *)malloc(length + 1);
			if (buffer == NULL) {
				ESP_LOGE(TAG, "%s: Failed to allocate memory for SPI transaction", __func__);
				return 1;
			}
			buffer[0] = regAddr | 0x80;

            spi_transaction_t t;
            memset(&t, 0, sizeof(t));
            t.length = 8 * (1 + length); // Command + Data
            t.rxlength = 8 * length;
			t.tx_buffer = buffer;            // Pointer to the address buffer
			t.rx_buffer = buffer;            // Pointer to the data buffer

            gpio_set_level((gpio_num_t)comm->spi.cs, 0);

            if(spi_device_transmit(comm->spi.spiobj, &t) != ESP_OK){
				ESP_LOGE(TAG,"%s: Communication failure. Could not target register.", __func__);
			}

            gpio_set_level((gpio_num_t)comm->spi.cs, 1);
        } else {
            return BME68X_E_NULL_PTR;
        }
    } else {
        return BME68X_E_NULL_PTR;
    }

    return BME68X_OK;
}

/**
 * @brief Function that implements the default I2C write transaction
 */
int8_t bme68xI2cWrite(uint8_t regAddr, const uint8_t *regData,
        uint32_t length, void *intfPtr) {
    uint32_t i;
    int8_t rslt = BME68X_OK;
    bme68xScommT *comm = NULL;
	// uint8_t write_buf[2] = {regAddr, regData};

#ifdef BME68X_I2C_BUFFER_SIZE
    if (length + 1 > BME68X_I2C_BUFFER_SIZE)
        return BME68X_E_COM_FAIL;
#endif

    if (intfPtr) {
        comm = (bme68xScommT *) intfPtr;
        if (comm->i2c.wireobj) {
			i2c_cmd_handle_t cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (comm->i2c.i2cAddr << 1) | I2C_MASTER_WRITE, true);
			i2c_master_write_byte(cmd, regAddr, true);

            for (i = 0; i < length; i++) {
                i2c_master_write_byte(cmd, regData[i], true);
            }
			i2c_master_stop(cmd);
            if (i2c_master_cmd_begin(comm->i2c.wireobj, cmd, (ERROR_DUR /portTICK_PERIOD_MS))) {
                rslt = BME68X_E_COM_FAIL;
            }
			i2c_cmd_link_delete(cmd);
        }
		else {
            rslt = BME68X_E_NULL_PTR;
        }
    } else {
        rslt = BME68X_E_NULL_PTR;
    }

    return rslt;
}

/**
 * @brief Function that implements the default I2C read transaction
 */ 
int8_t bme68xI2cRead(uint8_t regAddr, uint8_t *regData, uint32_t length,
        void *intfPtr) {
    // uint32_t i;
    int8_t rslt = BME68X_OK;
    bme68xScommT *comm = NULL;

#ifdef BME68X_I2C_BUFFER_SIZE
    if (length > BME68X_I2C_BUFFER_SIZE)
        return BME68X_E_COM_FAIL;
#endif

    if (intfPtr) {
        comm = (bme68xScommT *) intfPtr;
        if (comm->i2c.wireobj) {
            if (i2c_master_write_read_device(comm->i2c.wireobj, comm->i2c.i2cAddr, 
												&regAddr, 1, regData, length, 
												(ERROR_DUR / portTICK_PERIOD_MS) )) 
			{
                return BME68X_E_COM_FAIL;
            }
        }
		 else {
            rslt = BME68X_E_NULL_PTR;
        }
    } else {
        rslt = BME68X_E_NULL_PTR;
    }

    return rslt;
}

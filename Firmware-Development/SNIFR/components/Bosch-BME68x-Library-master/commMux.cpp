/**
 Copyright (c) 2021 Bosch Sensortec GmbH. All rights reserved.

 BSD-3-Clause

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

 @file    commMux.cpp
 @date    8 Feb 2022
 @version 1.1.40407

 */

// #include "esp_err.h
#include "esp_log.h"
#include "commMux.hpp"

#define CLOCK_FREQUENCY 400000
#define COMM_SPEED 8000000

static const char *TAG = "[commMux]";

const uint8_t I2C_EXPANDER_ADDR = 0x20;
const uint8_t I2C_EXPANDER_OUTPUT_REG_ADDR = 0x01;
const uint8_t I2C_EXPANDER_OUTPUT_DESELECT = 0xFF;
const uint8_t I2C_EXPANDER_CONFIG_REG_ADDR = 0x03;
const uint8_t I2C_EXPANDER_CONFIG_REG_MASK = 0x00;

#ifdef __cplusplus
extern "C" {
#endif

/////----------------------microsecondDelay-------------------------
#define NOP() asm volatile ("nop")

unsigned long IRAM_ATTR micros()
{
    return (unsigned long) (esp_timer_get_time());
}
void IRAM_ATTR delayMicroseconds(uint32_t us)
{
    uint32_t m = micros();
    if(us){
        uint32_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                NOP();
            }
        }
        while(micros() < e){
            NOP();
        }
    }
}
/////----------------------microsecondDelay-------------------------

/**
 * @brief Function to configure the communication across sensors
 */
commMux commMuxSetConfig(i2c_port_t wireobj, spi_device_handle_t &spiobj, uint8_t idx, commMux &comm)
{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	comm.select = ((0x01 << idx) ^ 0xFF);
	comm.spiobj = &spiobj;
	comm.wireobj = wireobj;

	return comm;
}

/**
 * @brief Function to trigger the communication
 */
esp_err_t commMuxBegin(i2c_port_t wireobj, spi_device_handle_t &spiobj)
{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (I2C_EXPANDER_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_CONFIG_REG_ADDR, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_CONFIG_REG_MASK, true);

	i2c_master_stop(cmd);
	if(i2c_master_cmd_begin(wireobj, cmd, (100 /portTICK_PERIOD_MS)) != ESP_OK){
		ESP_LOGE(TAG, "%s: Initializing I2C Mux IC Failed.", __func__);
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}
	else{
		ESP_LOGI(TAG, "%s: Initializing I2C Mux IC Success.", __func__);
	}

	i2c_cmd_link_delete(cmd);
	return ESP_OK;
}

/** 
 * @brief Function to set the chip select pin of the SPI
 */
void setChipSelect(i2c_port_t wireobj, uint8_t mask)
	{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	if(mask == I2C_EXPANDER_OUTPUT_DESELECT){
		ESP_LOGD(TAG, "%s: Deselecting CS lines.", __func__);
	}
	else{
		ESP_LOGD(TAG, "%s: Selecting CS line.", __func__);
	}

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (I2C_EXPANDER_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_OUTPUT_REG_ADDR, true);
	i2c_master_write_byte(cmd, mask, true);

	i2c_master_stop(cmd);
	if(i2c_master_cmd_begin(wireobj, cmd, (100 /portTICK_PERIOD_MS)) != ESP_OK){
		ESP_LOGE(TAG, "%s: Selecting CS line Failed.", __func__);
	}
	else{
		ESP_LOGD(TAG, "%s: CS line operation complete.", __func__);
	}

	i2c_cmd_link_delete(cmd);
}

/**
 * @brief Function to write the sensor data to the register
 */
int8_t commMuxWrite(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	commMux *comm = (commMux*) intf_ptr;

	if (comm)
	{
		setChipSelect(comm->wireobj, comm->select);

		uint8_t *buffer = (uint8_t *)malloc(length + 1);
		if (buffer == NULL) {
			ESP_LOGE(TAG, "%s: Failed to allocate memory for SPI transaction", __func__);
			return 1;
		}
		buffer[0] = reg_addr & 0x7F;  // Clear the MSB for write operation
    	memcpy(&buffer[1], reg_data, length);

		spi_transaction_t t;
		memset(&t, 0, sizeof(t));
		t.length = 8 * (1 + length); // Command + Data
		t.tx_buffer = buffer;

		if(spi_device_transmit(*comm->spiobj, &t) != ESP_OK){
			ESP_LOGE(TAG,"%s: Communication failure. Could not write register.", __func__);
		}
		free(buffer);

		setChipSelect(comm->wireobj, I2C_EXPANDER_OUTPUT_DESELECT);

		return 0;
	}

	return 1;
}

/**
 * @brief Function to read the sensor data from the register
 */
int8_t commMuxRead(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	commMux *comm = (commMux*) intf_ptr;

	if (comm)
	{
		setChipSelect(comm->wireobj, comm->select);

		uint8_t *buffer = (uint8_t *)malloc(length + 1);
		buffer[0] = reg_addr | 0x80;

		spi_transaction_t t;
		memset(&t, 0, sizeof(t));        // Zero out the transaction
		t.length = 8 * (length + 1);     // Total data length in bits (address + data)
		t.rxlength = 8 * length;   		 // Total receive length in bits
		t.tx_buffer = buffer;            // Pointer to the address buffer
		t.rx_buffer = buffer;            // Pointer to the data buffer

		// Send the address and receive the data
		if(comm->spiobj == NULL){

			ESP_LOGE(TAG,"%s: comm->spiobj is NULL.", __func__);
		}
		else if(spi_device_transmit(*comm->spiobj, &t) != ESP_OK){
			ESP_LOGE(TAG,"%s: Communication failure. Could not target register.", __func__);
			goto fault;
		}

		memcpy(reg_data, &buffer[1], length);
		// buffer = NULL;
		free(buffer);

		setChipSelect(comm->wireobj, I2C_EXPANDER_OUTPUT_DESELECT);

		return 0;
	}

fault:
	setChipSelect(comm->wireobj, I2C_EXPANDER_OUTPUT_DESELECT);
	return 1;
}

/**
 * @brief Function to maintain a delay between communication
 */
void commMuxDelay(uint32_t period_us, void *intf_ptr)
{
    ESP_LOGD(TAG, "%s: was called by %p", __func__, __builtin_return_address(0));
    
	(void) intf_ptr;
	delayMicroseconds(period_us);
}

#ifdef __cplusplus
}
#endif
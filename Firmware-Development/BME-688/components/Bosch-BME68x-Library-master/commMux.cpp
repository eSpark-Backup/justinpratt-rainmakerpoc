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

#include "commMux.hpp"

#define CLOCK_FREQUENCY 400000
#define COMM_SPEED 8000000

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
	comm.select = ((0x01 << idx) ^ 0xFF);
	comm.spiobj = &spiobj;
	comm.wireobj = wireobj;

	return comm;
}

/**
 * @brief Function to trigger the communication
 */
void commMuxBegin(i2c_port_t wireobj, spi_device_handle_t &spiobj)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (I2C_EXPANDER_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_CONFIG_REG_ADDR, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_CONFIG_REG_MASK, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(wireobj, cmd, (1000 /portTICK_PERIOD_MS));

	i2c_cmd_link_delete(cmd);
}

/** 
 * @brief Function to set the ship select pin of the SPI
 */
static void setChipSelect(i2c_port_t wireobj, uint8_t mask)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (I2C_EXPANDER_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, I2C_EXPANDER_OUTPUT_REG_ADDR, true);
	i2c_master_write_byte(cmd, mask, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(wireobj, cmd, (1000 /portTICK_PERIOD_MS));

	i2c_cmd_link_delete(cmd);
}

/**
 * @brief Function to write the sensor data to the register
 */
int8_t commMuxWrite(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
	commMux *comm = (commMux*) intf_ptr;
	// uint32_t i;

	if (comm)
	{
		setChipSelect(comm->wireobj, comm->select);

		spi_transaction_t t;
		memset(&t, 0, sizeof(t));
		t.length = 8 * (1 + length); // Command + Data
		t.flags = SPI_TRANS_USE_TXDATA;

		t.tx_data[0] = reg_addr;
		memcpy(t.tx_data + 1, reg_data, length);

		spi_device_transmit(*comm->spiobj, &t);

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
	commMux *comm = (commMux*) intf_ptr;
	// uint32_t i;

	if (comm)
	{
		setChipSelect(comm->wireobj, comm->select);

		spi_transaction_t t;
		memset(&t, 0, sizeof(t));
		t.length = 8 * (1 + length); // Command + Data
		t.flags = SPI_TRANS_USE_TXDATA;
		t.tx_data[0] = reg_addr | 0x80; // Set read bit

		spi_device_transmit(*comm->spiobj, &t);

		memset(&t, 0, sizeof(t));
		// t.flags = 0;
		t.flags = SPI_TRANS_USE_RXDATA;
		t.length = 8 * length;
		t.rxlength = 8 * length;
		t.rx_buffer = reg_data;

		spi_device_transmit(*comm->spiobj, &t);

		setChipSelect(comm->wireobj, I2C_EXPANDER_OUTPUT_DESELECT);

		return 0;
	}

	return 1;
}

/**
 * @brief Function to maintain a delay between communication
 */
void commMuxDelay(uint32_t period_us, void *intf_ptr)
{
	(void) intf_ptr;
	delayMicroseconds(period_us);
}

#ifdef __cplusplus
}
#endif
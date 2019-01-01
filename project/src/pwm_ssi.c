/**
 * RTL8710 40MHz PWM emulated with SSI bitstream
 * Copyright (C) 2016  kissste
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "dlist.h"
#include <autoconf.h>
#include <spi_api.h>
#include <spi_ex_api.h>
#include <flash_api.h>
#include <platform_opts.h>
#include <platform_stdlib.h>
#include "device_lock.h"
#include "port_api.h"   // mbed
#include "PortNames.h"   // mbed
#include "pwm_ssi.h"

//Debug Port
unsigned int pin_mask;
port_t port0;
const uint8_t My_Port_Def[] = {
	PC_4, 0xFF    // must end with 0xFF
};	

SSI_t *SSI_cfg = NULL;

//Array with 32-bit values which have one bit more set to '1' in every consecutive array index value
const uint32_t fakePwm32[] = { 0x00000010, 0x00000410,
	0x00400410, 0x00400C10, 0x00500C10, 0x00D00C10, 0x20D00C10, 0x21D00C10,
	0x21D80C10, 0xA1D80C10, 0xA1D80D10, 0xA1D80D30, 0xA1DC0D30, 0xA1DC8D30,
	0xB1DC8D30, 0xB9DC8D30, 0xB9FC8D30, 0xBDFC8D30, 0xBDFE8D30, 0xBDFE8D32,
	0xBDFE8D33, 0xBDFECD33, 0xFDFECD33, 0xFDFECD73, 0xFDFEDD73, 0xFFFEDD73,
	0xFFFEDD7B, 0xFFFEFD7B, 0xFFFFFD7B, 0xFFFFFDFB, 0xFFFFFFFB, 0xFFFFFFFF};

const uint64_t fakePwm64[]={
	0x4000000000000000, 0x4000000080000000, 0x4000020000100000, 0x4000800080008000, 
	0x4004002001000800, 0x4010020080100200, 0x4020100804020100, 0x4080808080808080, 
	0x4081020408102040, 0x4104082081040820, 0x4208208210410410, 0x4210821082108210, 
	0x4421084210842108, 0x4422110884221108, 0x8444222211110888, 0x8888888888888888, 
	0x8888911122224444, 0x8891224488912244, 0x8912449122489224, 0x8924892489248924, 
	0x8924924924924924, 0x9249249292492492, 0x924A492925249492, 0x9292929292929292, 
	0x9294A4A529494A52, 0x94A5294A94A5294A, 0x94A952952A52A54A, 0x952A952A952A952A, 
	0x954AA9552AA554AA, 0x95552AAA95552AAA, 0x955555552AAAAAAA, 0xAAAAAAAAAAAAAAAA, 
	0xAAAAAAAAD5555555, 0xAAAAD555AAAAD555, 0xAAB556AAD55AAB55, 0xAAD5AAD5AAD5AAD5, 
	0xAB56AD6AD5AD5AB5, 0xAB5AD6B5AB5AD6B5, 0xAD6B5B5AD6B6B5AD, 0xADADADADADADADAD, 
	0xADB5B6D6DADB6B6D, 0xADB6DB6DADB6DB6D, 0xB6DB6DB6DB6DB6DB, 0xB6DBB6DBB6DBB6DB, 
	0xB6EDBB6EDDB76DDB, 0xB76EDDBBB76EDDBB, 0xB7776EEEDDDDBBBB, 0xBBBBBBBBBBBBBBBB, 
	0xBBBBDDDDEEEEF777, 0xBBDDEEF7BBDDEEF7, 0xBBDEF7BDEF7BDEF7, 0xBDEFBDEFBDEFBDEF, 
	0xBDF7DF7DEFBEFBEF, 0xBEFBF7DFBEFBF7DF, 0xBF7EFDFBF7EFDFBF, 0xBFBFBFBFBFBFBFBF, 
	0xBFDFEFF7FBFDFEFF, 0xBFEFFDFFBFEFFDFF, 0xBFFBFFDFFEFFF7FF, 0xBFFFBFFFBFFFBFFF, 
	0xBFFFFDFFFFEFFFFF, 0xBFFFFFFFBFFFFFFF, 0xBFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};


/* Quick and dirty, we use one big DMA buffer for the whole PWM length.
 * TODO: use smaller DMA buffer and fill in bit patterns on the fly */
uint32_t dma_buffer[SSI_MAX_LEN];

/* wake up waiting tasks when DMA transfer is complete */
static void master_tr_done_callback(void *pdata, SpiIrq event)
{
    SSI_t *cfg;
    cfg = (SSI_t *) pdata;

    if (event == SpiTxIrq) {
		port_write(&port0, 0xFF);
		//spi_master_write_stream_dma(&cfg->spi_master, &(cfg->dma_buff[0]), cfg->buff_len);	
		spi_master_write_stream_dma(&cfg->spi_master, &(cfg->dma_buff[0]), cfg->buff_len);	
		port_write(&port0, 0x00);
	}
}

uint16_t populate_buffer(uint16_t stream_len) {
	uint16_t j = 0;
	
#if 0	
	uint8_t k = 1;
	int8_t d = 1;
	for(uint16_t i=0;i<4096;i++) {
		dma_buffer[i] = 0x1<<k;
		dma_buffer[i] = dma_buffer[i]|0x80000001;
		k += d;
		if (k == 0) {
			d = 1; 
			k = 2;
		} else if (k == 31) {
			d = -1; 
			k = 29;	
		}
	}
#endif

#if 0
	memset(&dma_buffer[0], 0x0, stream_len*sizeof(uint32_t));	
	
	dma_buffer[0] = 0xFFFFFFFF;
	dma_buffer[1] = 0x0;
	dma_buffer[2] = 0xFFFFFFFF;
	dma_buffer[stream_len-1]= 0xAAAAAAAA;
#endif
	
	while(stream_len > j+(1+32)+(1+32)*2) {
		dma_buffer[j++] = 0x0;
		
		for(uint8_t i=0; i<32; i++) {
			dma_buffer[j++] = fakePwm32[i];
		}

		dma_buffer[j++] = 0x0;
		dma_buffer[j++] = 0x0;
		
		for(uint8_t i=0; i<64; i++) {
			uint64_t tmp = fakePwm64[i];
			dma_buffer[j++] = tmp>>16;
			dma_buffer[j++] = tmp&0x00000000FFFFFFFF;
		}
	}
	
	//clean it the rest
	//memset(&dma_buffer[j],0x0,(stream_len-j)*sizeof(uint32_t));
	return j*sizeof(uint32_t);
}

SSI_t *SSI_init(uint16_t stream_len)
{
    int result;
    BaseType_t status;
    uint32_t reset_off;
    SSI_t *cfg;

	//Debug port
	port_mode(&port0, PullNone);
    // Assign pins to this port 
    port0.pin_def = (uint8_t*)My_Port_Def;
    pin_mask = 0xFF;    // each bit map to 1 pin: 0: pin disable, 1: pin enable
    port_init(&port0, PortC, pin_mask, PIN_OUTPUT);	
	port_write(&port0, 0x0);
	
    result = 0;

    cfg = malloc(sizeof(*cfg));
    if(cfg == NULL){
        printf("[%s] malloc for cfg failed\n", __func__);
        result = -1;
        goto err_out;
    }

    memset(cfg, 0x0, sizeof(*cfg));

	spi_init(&(cfg->spi_master), SPI1A_MOSI, SPI1A_MISO, SPI1A_SCLK, SPI1A_CS);
	//spi_init(&(cfg->spi_master), SPI0C_MOSI, SPI0C_MISO, SPI0C_SCLK, SPI0C_CS);
    spi_format(&(cfg->spi_master), 32, 3, 0); //32 bits, mode 3, slave
    spi_frequency(&(cfg->spi_master), SCLK_FREQ);
    spi_irq_hook(&(cfg->spi_master), master_tr_done_callback, (uint32_t)cfg);

	cfg->dma_buff = dma_buffer;
	cfg->buff_len = populate_buffer(stream_len);
	
	printf("[%d] buff_len\n", cfg->buff_len);
	
	spi_master_write_stream_dma(&cfg->spi_master, &(cfg->dma_buff[0]), cfg->buff_len);

err_out:
    if(result != 0 && cfg != NULL){
        if(cfg->dma_buff != NULL){
            free(cfg->dma_buff);
        }

        free(cfg);
        cfg = NULL;
    }
    
    return cfg;
}

void run_PWM(void *pvParameters __attribute__((unused)))
{

	SSI_cfg = SSI_init(SSI_MAX_LEN);
    if(SSI_cfg == NULL){
        printf("[%s] SSI_init() failed\n", __func__);
        goto err_out;
    }

    while(1){
		vTaskDelay(1000);
    }

err_out:
    while(1){
        vTaskDelay(1000);
    }
}

#define STACKSIZE               512
TaskHandle_t PWM_task = NULL;

static int PWM_start(void)
{
    int result;
    BaseType_t status;

    printf("[%s] Called\n", __func__);
    
    result = 0;

    if(PWM_task == NULL){
        status = xTaskCreate(run_PWM, (const char * )"led_PWM", STACKSIZE,
                                 NULL, tskIDLE_PRIORITY + 1, &PWM_task);

        if(status != pdPASS){
            printf("[%s] Create PWM task failed!\n", __func__);
            result = -1;
            goto err_out;
        }
    }

err_out:
    return result;
}

void main(void)
{
    int result;

	//ConfigDebugErr  = 1;
	//ConfigDebugInfo = 1;
	//ConfigDebugWarn = 1;	

	HalCpuClkConfig(CPU_CLOCK_SEL_VALUE); // 0 - 166666666 Hz, 1 - 83333333 Hz, 2 - 41666666 Hz, 3 - 20833333 Hz, 4 - 10416666 Hz, 5 - 4000000 Hz
	HAL_LOG_UART_ADAPTER pUartAdapter;
	pUartAdapter.BaudRate = RUART_BAUD_RATE_38400;
	HalLogUartSetBaudRate(&pUartAdapter);
	
    console_init();
    
    //printf("[%s] g_user_ap_sta_num: %d\n", __func__, g_user_ap_sta_num);
	
	result = PWM_start();
    if(result == 0){
		wlan_network();
        vTaskStartScheduler();
    }
	
err_out:
    return result;	
}

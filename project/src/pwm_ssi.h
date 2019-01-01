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
#ifndef __PWM_SSI_H__
#define __PWM_SSI_H__
#include "main.h"
//#include <wifi/wifi_conf.h>

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <spi_api.h>
#include <spi_ex_api.h>
//#include "semphr.h"
//#include "event_groups.h"

#define SSI_MAX_LEN  4096

extern struct blinken_cfg *blinken_get_config(void);

typedef struct {
    spi_t               spi_master;
    volatile uint8_t   *dma_buff;
    volatile size_t     buff_len;
    uint16_t            stream_len;
} SSI_t;

#define CPU_CLOCK_SEL_VALUE 0

//#define SCLK_FREQ 		20388888 // 20.83MHz CPU@166.66MHz @SPI1 ssi_idx=0 C-port
#define SCLK_FREQ 		41666666 // 41.66MHz CPU@166.66MHz @SPI1 ssi_idx=1 A-port

// SPI0 (S0)
#define SPI0C_MOSI	PC_2
#define SPI0C_MISO	PC_3
#define SPI0C_SCLK	PC_1
#define SPI0C_CS	PC_0

// SPI1 (S1)
#define SPI1B_MOSI  PB_6
#define SPI1B_MISO  PB_7
#define SPI1B_SCLK  PB_5
#define SPI1B_CS    PB_4

// SPI2 (S2) for DEV 3V0
// Please note that PA_7 need pull high before using GPIOA group
#define SPI1A_MOSI  PA_1
#define SPI1A_MISO  PA_0
#define SPI1A_SCLK  PA_2
#define SPI1A_CS    PA_4

// SPI2 (S2)
#define SPI2D_MOSI  PD_2
#define SPI2D_MISO  PD_3
#define SPI2D_SCLK  PD_1
#define SPI2D_CS    PD_0

extern SSI_t *SSI_init(uint16_t stream_len);

#endif

//
//  setup.h
//  test
//
//  Created by Rene on 09/12/13.
//  Copyright (c) 2013 Rene Hopf. All rights reserved.
//

#ifndef test_setup_h
#define test_setup_h

#include "stm32f4_discovery.h"
#include "stm32f4xx_conf.h"
#include "misc.h"
#include "../../stm32f103/inc/common.h"
#include "defines_res.h"

#include "stm32_ub_usb_cdc.h"

//#define mag_res 5250
#define mag_res 8400

//sample times for F4: 3,15,28,56,84,112,144,480
#define RES_SampleTime ADC_SampleTime_28Cycles

#define  ADC_ANZ 8
#define  PID_WAVES 4

void setup();
void setup_res();
void setup_pwm();
void setup_usart();
void SysTick_Handler(void);

volatile unsigned int systime;

volatile uint32_t ADC_DMA_Buffer[ADC_ANZ*PID_WAVES];
volatile uint16_t UART_DMA_Buffer[sizeof(to_hv_t)+1];

to_hv_t to_hv;

TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
TIM_OCInitTypeDef TIM_OCInitStructure;
NVIC_InitTypeDef NVIC_InitStructure;
GPIO_InitTypeDef GPIO_InitStructure;
DAC_InitTypeDef DAC_InitStructure;
DMA_InitTypeDef DMA_InitStructure;
ADC_InitTypeDef ADC_InitStructure;
RCC_ClocksTypeDef RCC_Clocks;



#endif

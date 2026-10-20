#ifndef __ADC_H
#define __ADC_H

#include "stm32f10x.h"

extern uint16_t IBI, BPM;
void PulseSensor_Config(void);
u16 Get_LightVale(void);
void get_BMP(void);
uint16_t Get_Array_Max(uint16_t * array, uint32_t size);	
uint16_t Get_Array_Min(uint16_t * array, uint32_t size);


#endif

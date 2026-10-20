#ifndef _BUZZER_H_
#define _BUZZER_H_
#include "stm32f10x.h"

// 蜂鸣器初始化
void BUZZER_Config(void);

// 按键初始化 (虽然在buzzer.c里实现，但需要在这里声明)
void KEY_Config(void);

// 按键扫描
u8 KEY_Scan(void);

#endif
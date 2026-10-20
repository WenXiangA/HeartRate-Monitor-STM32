#include "adc.h"
#include "stdio.h"
#include "lcd.h"

// ================= 变量定义 =================
uint16_t readData, preReadData;
uint16_t timeCount = 0;     
uint16_t firstTimeCount = 0;    
uint16_t secondTimeCount = 0;   

typedef enum {FALSE, TRUE} BOOL;

uint16_t IBI = 600;       // 默认间隔
uint16_t BPM = 0;         // 初始化为0
uint16_t SIG;       

uint16_t data[50] = {0}; 
uint8_t index_pulse = 0; // 缓存数组下标
uint16_t max, min, mid = 2048;   // 初始化为ADC中点，避免启动阶段误检
uint16_t filter;                

BOOL PULSE = FALSE;             
BOOL PRE_PULSE = FALSE;         

uint8_t pulseCount = 0;         

// ================= 函数实现 =================

// 初始化 ADC 和 GPIO (PC3)
void PulseSensor_Config(void)
{
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitLed;
    GPIO_InitLed.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitLed.GPIO_Mode = GPIO_Mode_AIN; 
    GPIO_InitLed.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitLed);
    
    ADC_InitTypeDef ADC_InitTypeDefStruct;
    ADC_InitTypeDefStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_InitTypeDefStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitTypeDefStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitTypeDefStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitTypeDefStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitTypeDefStruct.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitTypeDefStruct);
    
    ADC_Cmd(ADC1, ENABLE);  
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

// 读取 ADC 值并滤波
u16 Get_LightVale(void)
{
    u8 i, j;
    u16 buff[10] = {0};
    u16 temp;
    
    ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_55Cycles5);
    
    for(i = 0; i < 10; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
        buff[i] = ADC_GetConversionValue(ADC1);
    }
    
    for(i = 0; i < 9; i++)
    {
        for(j = 0; j < 9 - i; j++)
        {
            if(buff[j] > buff[j+1])
            {
                temp = buff[j];
                buff[j] = buff[j+1];
                buff[j+1] = temp;
            }
        }
    }
    temp = 0;
    for(i = 1; i < 9; i++) temp += buff[i];
    return temp / 8;
}

uint16_t Get_Array_Max(uint16_t * array, uint32_t size)
{
    uint16_t max_val = array[0];
    uint32_t i;
    for (i = 1; i < size; i++) if (array[i] > max_val) max_val = array[i];
    return max_val;
}

uint16_t Get_Array_Min(uint16_t * array, uint32_t size)
{
    uint16_t min_val = array[0];
    uint32_t i;
    for (i = 1; i < size; i++) if (array[i] < min_val) min_val = array[i];
    return min_val;
}

// 核心计算函数 (10ms一次)
void get_BMP(void)
{
    preReadData = readData;       
    readData = Get_LightVale();   

    data[index_pulse++] = readData;

    if (index_pulse >= 50)
    {
        index_pulse = 0; 
        max = Get_Array_Max(data, 50);
        min = Get_Array_Min(data, 50);

        if ((max - min) < 20) 
        {
            mid = 4095; 
            BPM = 0;    
        }
        else
        {
            mid = (max + min) / 2;
            filter = (max - min) / 2; // 信号幅值，可用于后续质量过滤(当前未使用)
        }
    }

    if (index_pulse < 50) { timeCount++; return; } // 窗口未填满，跳过脉搏检测
    PRE_PULSE = PULSE; 
    PULSE = (readData > mid) ? TRUE : FALSE; 

    if (PRE_PULSE == FALSE && PULSE == TRUE) 
    {
        pulseCount++;
        pulseCount %= 2;

        if (pulseCount == 1) firstTimeCount = timeCount;
        if (pulseCount == 0) 
        {
            secondTimeCount = timeCount;
            timeCount = 0; 

            if (secondTimeCount > firstTimeCount)
            {
                IBI = (secondTimeCount - firstTimeCount) * 10; 
                if(IBI > 0) 
                {
                    uint16_t new_BPM = 60000 / IBI; 
                    if (new_BPM > 30 && new_BPM < 200) BPM = new_BPM;
                }
            }
        }
    }
    
    if(timeCount > 250) { BPM = 0; timeCount = 0; index_pulse = 0; }
    timeCount++; 
}
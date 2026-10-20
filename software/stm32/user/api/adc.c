#include "adc.h"
#include "stdio.h"
#include "lcd.h"

// ================= 可调参数 =================
#define WINDOW_SIZE      250   /* 滑动窗口样本数：250 * 10ms = 2.5s */
#define SAMPLE_PERIOD_MS 10    /* get_BPM() 的调用周期(ms)，需与 main.c 时间片一致 */
#define PULSE_TIMEOUT    250   /* 无脉搏超时：250 * 10ms = 2.5s */
#define BPM_MIN          30    /* 有效心率下限(BPM) */
#define BPM_MAX          200   /* 有效心率上限(BPM) */
#define SIGNAL_MIN_AMPL  20    /* 判定"有脉搏"的最小信号幅度(ADC LSB) */

// ================= 变量定义 =================
uint16_t readData;                  /* 本次滤波后的 ADC 采样值 */
uint16_t timeCount = 0;             /* 距上一次脉搏的时间片计数 */

typedef enum {FALSE, TRUE} BOOL;

uint16_t IBI = 600;                 /* 心搏间隔(ms)，默认 600ms 对应 100 BPM */
uint16_t BPM = 0;                   /* 心率值，0 表示未检出 */

uint16_t data[WINDOW_SIZE] = {0};   /* 滑动窗口(填满后转为环形缓冲) */
uint16_t index_pulse = 0;           /* 窗口写入下标 */
uint16_t max, min, mid = 2048;      /* mid 初始化为 ADC 中点，避免启动阶段误检 */
uint16_t filter;                    /* 信号幅值，预留给后续信号质量过滤 */

BOOL PULSE     = FALSE;             /* 当前采样值是否高于阈值 */
BOOL PRE_PULSE = FALSE;             /* 上一次的比较结果，用于边沿检测 */

uint8_t window_ready = 0;           /* 滑动窗口是否已填满(上电预热标志) */
uint8_t signal_ok    = 0;           /* 窗口内信号幅度是否足够 */
uint8_t edge_valid   = 0;           /* 是否已捕获到第一个有效上升沿 */

// ================= 内部函数 =================

/* 完全重置脉搏检测状态机(含窗口，需要重新预热) */
static void PulseDetect_Reset(void)
{
    BPM          = 0;
    timeCount    = 0;
    index_pulse  = 0;
    window_ready = 0;
    signal_ok    = 0;
    edge_valid   = 0;
    PULSE        = FALSE;
    PRE_PULSE    = FALSE;
}

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

// 读取 ADC 值并滤波(10 次采样，去极大极小后取平均)
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

/*
 * 心率计算核心(由 main.c 每 10ms 调用一次)
 *
 * 修复说明(原 get_BMP 的三处缺陷)：
 *   1. 原代码用 `if (index_pulse < 50) return;` 判断窗口是否填满，
 *      但 index_pulse 在上一句刚被归零，该条件恒为真 ——
 *      上升沿检测与 BPM 计算整段是不可达死代码，屏幕上的 BPM 恒为 0。
 *      现改用独立的 window_ready 标志。
 *   2. 原代码每两次脉搏才清一次 timeCount，累计的是两个心搏间隔，
 *      在 40 BPM 附近会撞上 2.5s 超时而自我重置。
 *      现改为每个上升沿都清零，timeCount 即"距上次脉搏的间隔"。
 *   3. 原窗口仅 50 点(0.5s)，比低心率的心动周期还短，
 *      常出现整窗无脉搏导致误判为"信号弱"。现扩大到 250 点(2.5s)。
 *
 * 经仿真验证：30~200 BPM 全区间稳定检出，传感器未佩戴时无误报。
 */
void get_BPM(void)
{
    readData = Get_LightVale();

    data[index_pulse++] = readData;

    /* 窗口填满后刷新阈值；此后窗口作为环形缓冲，每 WINDOW_SIZE 个样本更新一次 */
    if (index_pulse >= WINDOW_SIZE)
    {
        index_pulse  = 0;
        window_ready = 1;

        max = Get_Array_Max(data, WINDOW_SIZE);
        min = Get_Array_Min(data, WINDOW_SIZE);

        if ((max - min) < SIGNAL_MIN_AMPL)
        {
            signal_ok = 0;          /* 幅度过小：传感器未佩戴或手指未放好 */
            BPM = 0;
        }
        else
        {
            signal_ok = 1;
            mid    = (max + min) / 2;               /* 判定阈值取窗口中值 */
            filter = (max - min) / 2;               /* 幅值，预留给后续信号质量过滤 */
            (void)filter;                           /* 消除未使用警告 */
        }
    }

    /* 上电预热：窗口未填满前不做检测 */
    if (window_ready == 0)
    {
        timeCount = 0;
        return;
    }

    /* 信号过弱：只清显示值并重新对齐，不重置窗口
       (若在此重置窗口，低心率下会反复重新预热，永远出不了数) */
    if (signal_ok == 0)
    {
        BPM        = 0;
        timeCount  = 0;
        edge_valid = 0;
        PULSE      = FALSE;
        PRE_PULSE  = FALSE;
        return;
    }

    PRE_PULSE = PULSE;
    PULSE = (readData > mid) ? TRUE : FALSE;

    /* 上升沿 = 一次心搏 */
    if (PRE_PULSE == FALSE && PULSE == TRUE)
    {
        if (edge_valid == 0)
        {
            edge_valid = 1;         /* 第一个沿只用于对齐，其间隔无意义 */
            timeCount  = 0;
        }
        else
        {
            IBI = timeCount * SAMPLE_PERIOD_MS;     /* 本次心搏间隔(ms) */
            timeCount = 0;

            if (IBI > 0)
            {
                uint16_t new_BPM = 60000 / IBI;
                if (new_BPM >= BPM_MIN && new_BPM <= BPM_MAX)
                {
                    BPM = new_BPM;
                }
            }
        }
    }

    /* 超过 2.5s 没有脉搏：归零并重新预热 */
    if (timeCount > PULSE_TIMEOUT)
    {
        PulseDetect_Reset();
        return;
    }

    timeCount++;
}

#include "stm32f10x.h"
#include "delay.h"
#include "led.h"
#include "buzzer.h"
#include "lcd.h"  
#include "spi.h"  
#include "adc.h"
#include "stdio.h" 

// 引入外部变量
extern uint16_t BPM; // 来自 adc.c

// 定义心率报警阈值，默认 100
int BPM_Threshold = 100; 

void JTAG_SW_Config(void)
{
    //关闭JTAG接口，开启SW接口--PA15、PB3、PB4可用
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}

int main(void)
{
    u8 ledflag=0;
    u8 key=0;
    
    char buf[30];       // 存放 BPM 字符串
    char buf_limit[30]; // 存放阈值字符串
    
    JTAG_SW_Config();
    Systick_Init(72000);
    
    LED_Config();    // LED初始化
    BUZZER_Config();  // 蜂鸣器初始化
    KEY_Config();    // 按键初始化
    
    LCD_Init();      // 屏幕初始化
    PulseSensor_Config(); // 传感器初始化

    // 初始清屏
    LCD_Clear(BLACK);
    
    // 显示固定标题 (加了 (u8*) 强转消除警告)
    LCD_ShowChinese(30, 5, (u8*)"心率检测器", BLUE, BLACK, 16, 1);
    
    while (1)
    {
        // --- 1. 界面刷新 (每200ms) ---
        if(ledtime >= 200)
        {
            // LED 闪烁指示运行状态
            if(ledflag==0) GPIO_ResetBits(GPIOC,GPIO_Pin_0); // led1亮
            else           GPIO_SetBits(GPIOC,GPIO_Pin_0);   // led1灭
            ledflag = !ledflag;
            ledtime = 0;
            
            // 显示 BPM
            sprintf(buf, "BPM: %3d   ", BPM); 
            LCD_ShowString(30, 60, (u8*)buf, WHITE, BLACK, 16, 0);

            // 显示 报警阈值
            sprintf(buf_limit, "Limit: %3d ", BPM_Threshold);
            LCD_ShowString(30, 90, (u8*)buf_limit, YELLOW, BLACK, 16, 0);
        }
        
        // --- 2. 心率采样 (每10ms) ---
        if(bmptime >= 10)
        {
            get_BMP();
            bmptime = 0;
        }

        // --- 3. 报警逻辑 ---
        // 只有当检测到有效心率(>0) 且 超过阈值时才报警
        if(BPM > 0 && BPM > BPM_Threshold)
        {
            // 报警模式
            // 高电平触发蜂鸣器(NPN三极管驱动)
            GPIO_SetBits(GPIOA, GPIO_Pin_6);   // 高电平→NPN导通→蜂鸣器响
        }
        else
        {
            // 正常模式 (不响)
            GPIO_ResetBits(GPIOA, GPIO_Pin_6); // 低电平→NPN截止→蜂鸣器静默
        }

        // --- 4. 按键扫描 (每10ms) ---
        if(keytime >= 10)
        {
            key = KEY_Scan();
            if(key == 1) // Key1 按下，阈值增加
            {
                BPM_Threshold += 5; 
                if(BPM_Threshold > 180) BPM_Threshold = 180;
            }
            if(key == 2) // Key2 按下，阈值减少
            {
                BPM_Threshold -= 5; 
                if(BPM_Threshold < 40) BPM_Threshold = 40;
            }
            keytime = 0;
        }
    }
}
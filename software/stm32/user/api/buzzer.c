#include "buzzer.h"
//PA6
void BUZZER_Config(void)
{
	//使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	//初始化GPIO
	GPIO_InitTypeDef buzzer_gpio={0};
	buzzer_gpio.GPIO_Mode=GPIO_Mode_Out_PP;//通用推挽输出
	buzzer_gpio.GPIO_Pin=GPIO_Pin_6;
	buzzer_gpio.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&buzzer_gpio);
	//设置初始电平
	GPIO_ResetBits(GPIOA,GPIO_Pin_6);
}
//按键初始化
void KEY_Config(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	GPIO_InitTypeDef key_gpio={0};
	key_gpio.GPIO_Mode=GPIO_Mode_IN_FLOATING;//浮空输入
	//PA0
	key_gpio.GPIO_Pin=GPIO_Pin_0;
	GPIO_Init(GPIOA,&key_gpio);
	//PB8
	key_gpio.GPIO_Pin=GPIO_Pin_8;
	GPIO_Init(GPIOB,&key_gpio);
	//按键扫描
}
	// 修正后的按键扫描函数
// 修正后的按键扫描函数
u8 KEY_Scan(void)
{
    static u32 count1 = 0;
    static u32 count2 = 0;

    // --- 按键 1 检测 (PA0, 高电平有效) ---
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1) 
    {
        count1++; // 只要按着，计数器就增加
    }
    else // 松手了
    {
        // 【关键修改】把 1000 改成了 5
        // 因为主循环每 10ms 进来一次，5 * 10ms = 50ms，刚好符合人手按下的时间
        if(count1 >= 5) 
        {
            count1 = 0;
            return 1; // 返回键值 1 (增加)
        }
        count1 = 0; // 如果按下的时间太短（比如抖动），就清零忽略
    }

    // --- 按键 2 检测 (PB8, 低电平有效) ---
    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) == 0)
    {
        count2++;
    }
    else
    {
        // 【关键修改】把 1000 改成了 5
        if(count2 >= 5)
        {
            count2 = 0;
            return 2; // 返回键值 2 (减少)
        }
        count2 = 0;
    }

    return 0; // 没有有效按键
}
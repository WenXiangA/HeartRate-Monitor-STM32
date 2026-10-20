#include "led.h"

// 初始化 LED 相关的 GPIO 引脚 (PC0, PC1, PC2)
void LED_Config(void)
{
    // 1. 开启时钟：GPIOC 挂载在 APB2 总线上
    //如果不开启时钟，配置再对，引脚也没电，无法工作
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // 2. 配置结构体参数
    GPIO_InitTypeDef led_gpio = {0};
    
    led_gpio.GPIO_Mode = GPIO_Mode_Out_PP;      // 推挽输出 (Push-Pull)：驱动能力强，适合点亮 LED
    led_gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2; // 使用 '|' 符号同时选中三个引脚
    led_gpio.GPIO_Speed = GPIO_Speed_50MHz;     // 设置翻转速度为 50MHz
    
    GPIO_Init(GPIOC, &led_gpio);                // 将上述配置写入寄存器

    // 3. 设置初始电平：拉高
    // 对于“低电平点亮”的 LED 电路，初始化输出高电平 = 默认熄灭
    // 防止一上电灯就乱亮
    GPIO_SetBits(GPIOC, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2);
}
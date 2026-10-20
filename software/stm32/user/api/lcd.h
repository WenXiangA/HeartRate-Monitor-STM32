#ifndef __LCD_H
#define __LCD_H		

#include "stm32f10x.h"

//设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏
#define USE_HORIZONTAL 1  

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 128
#define LCD_H 160
#else
#define LCD_W 160
#define LCD_H 128
#endif

// LCD底层函数声明
void LCD_Writ_Bus(u8 dat);
void LCD_WR_DATA8(u8 dat);
void LCD_WR_DATA(u16 dat);
void LCD_WR_REG(u8 dat);
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);
void LCD_Init(void);

// 【关键修改点】必须加上这一行声明！
void LCD_Clear(u16 Color); 

// 图形绘制函数声明
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);
void LCD_DrawPoint(u16 x,u16 y,u16 color);
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color);

// 汉字与字符显示函数声明
void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese12x12(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);

void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[]);

// 画笔颜色定义
#define WHITE            0xFFFF
#define BLACK            0x0000    
#define BLUE             0x001F  
#define BRED             0XF81F
#define GRED             0XFFE0
#define GBLUE            0X07FF
#define RED              0xF800
#define MAGENTA          0xF81F
#define GREEN            0x07E0
#define CYAN             0x7FFF
#define YELLOW           0xFFE0
#define BROWN            0XBC40 
#define BRRED            0XFC07 
#define GRAY             0X8430 
#define DARKBLUE         0X01CF 
#define LIGHTBLUE        0X7D7C 
#define GRAYBLUE         0X5458 
#define LIGHTGREEN       0X841F 
#define LGRAY            0XC618 
#define LGRAYBLUE        0XA651 
#define LBBLUE           0X2B12 

#endif
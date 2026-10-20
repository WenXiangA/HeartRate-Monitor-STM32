# HeartRate-Monitor-STM32

基于 STM32F103RCT6 的心率监测仪设计与实现

## 项目简介

本项目使用 STM32 单片机采集 PulseSensor 心率传感器的模拟信号，通过 ADC 转换与软件算法实时计算心率值（BPM），并在 1.8 寸 LCD 屏幕上显示。支持按键调节心率阈值，超过阈值时蜂鸣器报警。硬件部分使用嘉立创EDA绘制了完整的原理图（含主控、电源、USB下载、LCD接口、传感器接口等电路）。

## 功能特性

- **实时心率采集**：PulseSensor 模拟信号 → STM32 ADC → BPM 计算
- **LCD 显示**：1.8 寸 ST7735S 屏幕，SPI 接口，128×160 RGB565
- **阈值调节**：两个按键调整报警阈值（步进 5，范围 40~180 BPM）
- **蜂鸣器报警**：心率超过阈值时自动报警
- **LED 指示**：心跳闪烁指示灯
- **非阻塞调度**：基于 SysTick 的时间片架构，无阻塞延时

## 硬件平台

| 模块       | 型号                    | 接口                 |
| ---------- | ----------------------- | -------------------- |
| 主控芯片   | STM32F103RCT6           | —                    |
| 心率传感器 | PulseSensor             | ADC (PC3, ADC1_CH13) |
| 显示屏     | 1.8 寸 LCD ST7735S      | SPI2                 |
| 蜂鸣器     | 有源蜂鸣器 + S8050 驱动 | GPIO PA6             |
| 按键 ×2    | 阈值加减                | PA0 / PB8            |
| LED ×3     | 状态指示                | PC0 / PC1 / PC2      |

## 项目结构

```
HeartRate-Monitor-STM32/
├── docs/                               # 算法与设计文档
│   └── ALGORITHM.md                    # 心率检测算法说明与验证结果
├── hardware/                           # 硬件设计文件
│   ├── schematic.pdf                   # 原理图 (嘉立创EDA导出)
│   ├── BOM.csv                         # 物料清单
│   └── 心率.eprj2                      # 嘉立创EDA 工程文件
├── software/                           # 固件源码
│   └── stm32/
│       ├── project/                    # Keil5 工程文件
│       ├── startup/                    # 启动文件
│       ├── STM32F10x_StdPeriph_Driver/ # ST 标准外设库 V3.5
│       └── user/                       # 用户代码
│           ├── main.c                  # 主程序
│           ├── stm32f10x_it.c          # 中断处理
│           └── api/                    # 驱动模块
│               ├── adc.c/h             # 心率采集 + BPM 算法
│               ├── lcd.c/h             # LCD 显示驱动 (ST7735S)
│               ├── spi.c/h             # SPI2 底层驱动
│               ├── buzzer.c/h          # 蜂鸣器 + 按键驱动
│               ├── led.c/h             # LED 驱动
│               ├── delay.c/h           # SysTick 定时 + 非阻塞时间片
│               ├── lcdfont.h           # 字库数据
│               └── pic1.c              # 开机图片数据
├── verify/                             # 算法仿真验证（Python）
│   └── bpm_fix_verify.py               # 修复前后对比，30~200 BPM 全区间扫描
├── .gitignore
└── README.md
```

## 核心算法

### 心率计算流程

1. **ADC 采样**：每 10ms 读取 PC3 模拟值，10 次采样去极值平均滤波
2. **滑动窗口**：维护 250 个样本（2.5s）的环形缓冲，动态计算 max/min/mid
3. **脉搏检测**：信号从低于 mid 跳变到高于 mid → 检测上升沿
4. **BPM 计算**：每个上升沿测量一次心搏间隔 IBI → `BPM = 60000 / IBI`
5. **有效性校验**：BPM 范围 30~200（闭区间），超时 2.5s 归零并重新预热

> 上电后需先填满 2.5s 滑动窗口才开始检测，因此首次出数约需 3~5 秒。
>
> 算法经 Python 仿真验证（`verify/bpm_fix_verify.py`）：30~200 BPM 全区间稳定检出，
> 稳态误差 < 1 BPM，传感器未佩戴时无误报。详见 [算法说明](docs/ALGORITHM.md)。

### 软件调度

基于 SysTick 1ms 中断提供时间基准，主循环采用非阻塞时间片调度：

| 任务     | 周期  | 说明                |
| -------- | ----- | ------------------- |
| LCD 刷新 | 200ms | 显示 BPM 值和阈值   |
| 心率采集 | 10ms  | ADC 采样 + 算法计算 |
| 按键扫描 | 10ms  | 50ms 消抖           |

## 引脚分配

| 引脚            | 功能               | 模式     |
| --------------- | ------------------ | -------- |
| PC3             | PulseSensor 信号   | 模拟输入 |
| PB13            | SPI2_SCK (LCD)     | 复用推挽 |
| PB15            | SPI2_MOSI (LCD)    | 复用推挽 |
| PB10            | LCD_CS             | 推挽输出 |
| PB11            | LCD_DC             | 推挽输出 |
| PB12            | LCD_RES            | 推挽输出 |
| PC6             | LCD_BL (背光)      | 推挽输出 |
| PA0             | KEY1 (阈值+)       | 浮空输入 |
| PB8             | KEY2 (阈值-)       | 浮空输入 |
| PA6             | 蜂鸣器             | 推挽输出 |
| PC0 / PC1 / PC2 | LED1 / LED2 / LED3 | 推挽输出 |

## 硬件接线

### PulseSensor → STM32

| 传感器引脚 | 开发板 | 说明         |
| ---------- | ------ | ------------ |
| S (信号)   | PC3    | ADC1 通道 13 |
| + (电源)   | 3V3    | 供电         |
| - (地)     | GND    | 共地         |

### LCD (ST7735S) → STM32

| LCD 引脚 | 开发板 | 说明      |
| -------- | ------ | --------- |
| SCL      | PB13   | SPI2 时钟 |
| SDA      | PB15   | SPI2 MOSI |
| CS       | PB10   | 片选      |
| DC       | PB11   | 数据/命令 |
| RES      | PB12   | 复位      |
| BL       | PC6    | 背光      |

## 编译与烧录

### 开发环境

- **IDE**：Keil MDK-ARM (uVision5)
- **库**：STM32F10x 标准外设库 V3.5
- **下载方式**：ST-Link (SWD) 或 CH340 串口下载

### 步骤

1. 用 Keil5 打开 `software/stm32/project/HeartRateMonitor.uvprojx`
2. 编译生成 HEX 文件
3. 通过 ST-Link 或串口 (FlyMCU) 烧录到 STM32

## 已修复的问题

- ~~`BUZZER_Config()` 在 main.c 中被注释~~ → 已取消注释，PA6 正确初始化为推挽输出
- ~~蜂鸣器电平逻辑与硬件电路矛盾~~ → 已修正为高电平触发（NPN 三极管驱动）
- ~~`mid` 初始值为 0，启动阶段可能误检~~ → 已初始化为 2048，并在窗口未填满时跳过脉搏检测
- ~~`adc.h` 中 `ADC_Data` / `BMP_show` 无效 extern 声明~~ → 已删除
- `filter` 变量计算后未使用 → 已标注注释，保留供后续信号质量过滤使用
- 心率算法重写：滑动窗口阈值检测改为独立状态机，并补充仿真验证（详见 [算法说明](docs/ALGORITHM.md)）

## 许可

本项目采用 [MIT License](LICENSE) 发布。

# STM32F407 LVGL + FreeRTOS 音乐播放器（上位机 / Host）

STM32F407ZGT6 作上位机，跑 LVGL 界面 + FreeRTOS，通过 USART1 与下位机（RCT6 + SD + VS1053）双机通信，控制音乐播放、歌单浏览、音量等。

## 硬件
- 主控：STM32F407ZGT6
- 屏幕：ST7789 240×240 SPI 屏
- 通信：USART1（115200/8N1）连下位机 RCT6
- 下位机仓库：`pluteed/Job-hunt-project`（RCT6 + FatFs + VS1053 音乐解码）

## 目录
- `Core/Src`、`Core/Inc`：用户应用与驱动（LVGL 移植、输入设备、App 框架、双机协议）
- `Middlewares/Third_Party/FreeRTOS`：FreeRTOS 内核（CMSIS_V2）
- `Middlewares/Third_Party/lvgl-9.5`：LVGL v9.5 源码（已裁掉 demo/examples/tests 等非编译目录）
- `Drivers`：HAL + CMSIS
- `MDK-ARM`：Keil 工程文件（编译产物已被 .gitignore 排除）

## 构建
1. 用 CubeMX 打开 `STM32F407ZGT6_LVGL_FREERTOS.ioc` 生成外设代码（若未生成）。
2. Keil MDK-ARM 打开 `MDK-ARM/STM32F407ZGT6_LVGL_FREERTOS.uvprojx`。
3. 编译器须用 **ARM Compiler 6（AC6）**（LVGL v9 需 C11），非 AC5。
4. 编译下载；LVGL 对象操作全部集中在 `gui_task`，通过互斥量保护线程安全。

## 双机协议（与下位机共用）
帧格式：`AA 55 | LEN_L LEN_H | CMD | SEQ | PAYLOAD | SUM`，`SUM = (CMD+SEQ+ΣPAYLOAD) & 0xFF`（累加和，非 CRC）。
主要命令：列表请求 `0x21`、播放指定索引 `0x22`、停止 `0x25`、下一首 `0x26`、设置音量 `0x28`、状态请求 `0x29`、心跳 `0x20`；下位机回 `0x01` ACK 与 `0x10/0x11/0x12` 列表帧、`0x13` 状态帧。

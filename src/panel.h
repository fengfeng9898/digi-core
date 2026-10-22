#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// 全局唯一的面板对象（main 里创建，ui 里绘制）
extern MatrixPanel_I2S_DMA* g_matrix;

// 画面方向：0=顺时针90  1=逆时针90
extern uint8_t g_rot_dir;

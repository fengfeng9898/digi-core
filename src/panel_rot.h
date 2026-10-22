#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// 这个库没实现 setRotation（面板类忽略它），所以在这里重写虚拟绘制函数，
// 把所有绘制做 90° 坐标映射。逻辑坐标仍是 0..63。
//   g_rot_dir = 0 -> 画面顺时针 90（(x,y) -> (63-y, x)）
//   g_rot_dir = 1 -> 画面逆时针 90（(x,y) -> (y, 63-x)）
extern uint8_t g_rot_dir;

class RotPanel : public MatrixPanel_I2S_DMA {
public:
    explicit RotPanel(const HUB75_I2S_CFG& cfg) : MatrixPanel_I2S_DMA(cfg) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        int16_t px, py;
        map_pt(x, y, px, py);
        MatrixPanel_I2S_DMA::drawPixel(px, py, color);
    }

    void fillScreen(uint16_t color) override {
        MatrixPanel_I2S_DMA::fillScreen(color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        int16_t px, py;
        if (g_rot_dir == 0) {
            map_pt(x, y, px, py);                 // 左上角映射
            MatrixPanel_I2S_DMA::fillRect((int16_t)(63 - y - h + 1), x, h, w, color);
            (void)px; (void)py;
        } else {
            MatrixPanel_I2S_DMA::fillRect(y, (int16_t)(63 - x - w + 1), h, w, color);
        }
    }

    // 横线旋转后变成竖线
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        if (g_rot_dir == 0) MatrixPanel_I2S_DMA::drawFastVLine((int16_t)(63 - y), x, w, color);
        else                MatrixPanel_I2S_DMA::drawFastVLine(y, (int16_t)(63 - x - w + 1), w, color);
    }

    // 竖线旋转后变成横线
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        if (g_rot_dir == 0) MatrixPanel_I2S_DMA::drawFastHLine((int16_t)(63 - y - h + 1), x, h, color);
        else                MatrixPanel_I2S_DMA::drawFastHLine(y, (int16_t)(63 - x), h, color);
    }

private:
    static inline void map_pt(int16_t x, int16_t y, int16_t& px, int16_t& py) {
        if (g_rot_dir == 0) { px = (int16_t)(63 - y); py = x; }
        else                { px = y;                py = (int16_t)(63 - x); }
    }
};

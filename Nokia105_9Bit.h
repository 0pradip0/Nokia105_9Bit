#ifndef NOKIA105_9BIT_H
#define NOKIA105_9BIT_H

#include <Arduino.h>
#include "driver/spi_master.h"

// 2560 pixels = 16 lines in landscape (5120 bytes data -> 5760 bytes packed DMA)
#define LCD_CHUNK_PIXELS  2560 
#define LCD_DMA_BUF_SIZE  ((LCD_CHUNK_PIXELS * 2 * 9) / 8)

class Nokia105_9Bit {
private:
    int8_t   _cs_pin;
    int8_t   _clk_pin;
    int8_t   _sda_pin;
    int8_t   _res_pin;

    uint16_t _init_width;
    uint16_t _init_height;
    uint16_t _width;
    uint16_t _height;
    uint8_t  _rotation;
    bool     _bgr;

    spi_device_handle_t _spi_lcd;
    spi_transaction_t   _spi_trans;

    // High-speed chunk buffer in DMA RAM (~5.7 KB)
    uint8_t*            _dma_chunk_buf;
    spi_transaction_t   _dma_chunk_trans;

    void initHardwareSPI(uint32_t speed_hz);
    inline void send9Bit(uint8_t dc, uint8_t data);
    void packChunk(const uint16_t *src, uint8_t *dest, uint32_t pixels);

public:
    Nokia105_9Bit(int8_t cs = 5, int8_t clk = 18, int8_t sda = 23, int8_t res = 4, 
                  uint16_t w = 128, uint16_t h = 160, bool bgr = false);

    void begin(uint32_t speed_hz = 26 * 1000 * 1000);
    void setRotation(uint8_t rotation);

    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    // High-speed chunked push (Takes LVGL 16-bit RGB565 buffers and streams at 60 FPS)
    void pushColors(const uint16_t *data, uint32_t len);

    // Primitives for standalone projects
    void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    void fillScreen(uint16_t color);

    uint16_t width()  const { return _width; }
    uint16_t height() const { return _height; }
};

#endif
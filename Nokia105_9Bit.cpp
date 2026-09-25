#include "Nokia105_9Bit.h"

Nokia105_9Bit::Nokia105_9Bit(int8_t cs, int8_t clk, int8_t sda, int8_t res, 
                             uint16_t w, uint16_t h, bool bgr)
    : _cs_pin(cs), _clk_pin(clk), _sda_pin(sda), _res_pin(res),
      _init_width(w), _init_height(h), _width(w), _height(h),
      _rotation(0), _bgr(bgr), _dma_chunk_buf(NULL) {}

void Nokia105_9Bit::initHardwareSPI(uint32_t speed_hz) {
    if (_res_pin >= 0) {
        pinMode(_res_pin, OUTPUT);
        digitalWrite(_res_pin, LOW);
        delay(50);
        digitalWrite(_res_pin, HIGH);
        delay(120);
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num     = _sda_pin;
    buscfg.sclk_io_num     = _clk_pin;
    buscfg.miso_io_num     = -1;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = LCD_DMA_BUF_SIZE + 64;

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = speed_hz;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = _cs_pin;
    devcfg.queue_size     = 2;
    devcfg.flags          = SPI_DEVICE_NO_DUMMY;

    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &_spi_lcd);

    _spi_trans.length = 9;
    _spi_trans.flags  = SPI_TRANS_USE_TXDATA;

    // Allocate small 5.7 KB DMA buffer in internal SRAM
    _dma_chunk_buf = (uint8_t *)heap_caps_malloc(LCD_DMA_BUF_SIZE, MALLOC_CAP_DMA);
}

inline void Nokia105_9Bit::send9Bit(uint8_t dc, uint8_t data) {
    _spi_trans.tx_data[0] = (dc ? 0x80 : 0x00) | (data >> 1);
    _spi_trans.tx_data[1] = (data & 0x01) << 7;
    spi_device_polling_transmit(_spi_lcd, &_spi_trans);
}

void Nokia105_9Bit::sendCommand(uint8_t cmd) { send9Bit(0, cmd); }
void Nokia105_9Bit::sendData(uint8_t data)   { send9Bit(1, data); }

void Nokia105_9Bit::begin(uint32_t speed_hz) {
    initHardwareSPI(speed_hz);

    sendCommand(0x01); delay(120);
    sendCommand(0x11); delay(120);
    sendCommand(0x3A); sendData(0x05); // RGB565

    setRotation(0);

    sendCommand(0x21); // Display Inversion ON
    sendCommand(0x29); // Display ON
    delay(50);
}

void Nokia105_9Bit::setRotation(uint8_t rotation) {
    _rotation = rotation % 4;
    uint8_t madctl = 0;

    switch (_rotation) {
        case 0: madctl = 0xC0; _width = _init_width;  _height = _init_height; break;
        case 1: madctl = 0x60; _width = _init_height; _height = _init_width;  break;
        case 2: madctl = 0x00; _width = _init_width;  _height = _init_height; break;
        case 3: madctl = 0xA0; _width = _init_height; _height = _init_width;  break;
    }

    if (_bgr) madctl |= 0x08;

    sendCommand(0x36);
    sendData(madctl);
}

void Nokia105_9Bit::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    sendCommand(0x2A);
    sendData(0x00); sendData(x0);
    sendData(0x00); sendData(x1);

    sendCommand(0x2B);
    sendData(0x00); sendData(y0);
    sendData(0x00); sendData(y1);

    sendCommand(0x2C);
}

// Interleaves D/C bits into 8-bit stream (4 pixels -> 9 bytes)
void Nokia105_9Bit::packChunk(const uint16_t *src, uint8_t *dest, uint32_t pixels) {
    uint32_t chunks = pixels / 4;
    uint32_t d = 0;

    for (uint32_t i = 0; i < chunks; i++) {
        uint16_t p0 = *src++; uint16_t p1 = *src++;
        uint16_t p2 = *src++; uint16_t p3 = *src++;

        uint8_t b0 = p0 >> 8, b1 = p0 & 0xFF;
        uint8_t b2 = p1 >> 8, b3 = p1 & 0xFF;
        uint8_t b4 = p2 >> 8, b5 = p2 & 0xFF;
        uint8_t b6 = p3 >> 8, b7 = p3 & 0xFF;

        dest[d++] = 0x80 | (b0 >> 1);
        dest[d++] = ((b0 & 0x01) << 7) | 0x40 | (b1 >> 2);
        dest[d++] = ((b1 & 0x03) << 6) | 0x20 | (b2 >> 3);
        dest[d++] = ((b2 & 0x07) << 5) | 0x10 | (b3 >> 4);
        dest[d++] = ((b3 & 0x0F) << 4) | 0x08 | (b4 >> 5);
        dest[d++] = ((b4 & 0x1F) << 3) | 0x04 | (b5 >> 6);
        dest[d++] = ((b5 & 0x3F) << 2) | 0x02 | (b6 >> 7);
        dest[d++] = ((b6 & 0x7F) << 1) | 0x01;
        dest[d++] = b7;
    }
}

// Slices any payload into 5.7 KB bursts — zero software overhead, 60+ FPS
void Nokia105_9Bit::pushColors(const uint16_t *data, uint32_t len) {
    if (!_dma_chunk_buf) return;

    while (len > 0) {
        uint32_t toSend = (len > LCD_CHUNK_PIXELS) ? LCD_CHUNK_PIXELS : len;
        uint32_t alignedSend = toSend & ~0x03; // Multiple of 4

        if (alignedSend > 0) {
            packChunk(data, _dma_chunk_buf, alignedSend);

            memset(&_dma_chunk_trans, 0, sizeof(spi_transaction_t));
            _dma_chunk_trans.length    = (alignedSend * 2 * 9);
            _dma_chunk_trans.tx_buffer = _dma_chunk_buf;

            spi_device_polling_transmit(_spi_lcd, &_dma_chunk_trans);

            data += alignedSend;
            len  -= alignedSend;
        }

        // Remainder fallback (1-3 trailing pixels)
        if (len < 4 && len > 0) {
            for (uint32_t i = 0; i < len; i++) {
                uint16_t c = *data++;
                sendData(c >> 8);
                sendData(c & 0xFF);
            }
            break;
        }
    }
}

void Nokia105_9Bit::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= _width || y >= _height) return;
    setAddrWindow(x, y, x, y);
    sendData(color >> 8);
    sendData(color & 0xFF);
}

void Nokia105_9Bit::fillScreen(uint16_t color) {
    setAddrWindow(0, 0, _width - 1, _height - 1);
    uint32_t total = (uint32_t)_width * _height;

    // Stack buffer for solid fill bursts
    uint16_t fillBuf[160];
    for (int i = 0; i < 160; i++) fillBuf[i] = color;

    while (total > 0) {
        uint32_t count = (total > 160) ? 160 : total;
        pushColors(fillBuf, count);
        total -= count;
    }
}
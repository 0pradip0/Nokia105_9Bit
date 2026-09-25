#include <Arduino.h>
#include <Nokia105_9Bit.h>

// CS, CLK, SDA, RES, Width, Height, BGR
Nokia105_9Bit lcd(5, 18, 23, 4, 128, 160, false);

// Common RGB565 Color Definitions
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

void drawColorBar(uint16_t y0, uint16_t y1, uint16_t color) {
    lcd.setAddrWindow(0, y0, lcd.width() - 1, y1);
    uint32_t count = (uint32_t)lcd.width() * (y1 - y0 + 1);
    for (uint32_t i = 0; i < count; i++) {
        lcd.sendData(color >> 8);
        lcd.sendData(color & 0xFF);
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize at 20 MHz
    lcd.begin(20 * 1000 * 1000);
    lcd.setRotation(0); // Portrait (128x160)

    // Clear display
    lcd.fillScreen(COLOR_BLACK);
    delay(500);

    // Draw horizontal color test bars
    uint16_t barHeight = lcd.height() / 7;
    drawColorBar(barHeight * 0, barHeight * 1 - 1, COLOR_RED);
    drawColorBar(barHeight * 1, barHeight * 2 - 1, COLOR_GREEN);
    drawColorBar(barHeight * 2, barHeight * 3 - 1, COLOR_BLUE);
    drawColorBar(barHeight * 3, barHeight * 4 - 1, COLOR_YELLOW);
    drawColorBar(barHeight * 4, barHeight * 5 - 1, COLOR_CYAN);
    drawColorBar(barHeight * 5, barHeight * 6 - 1, COLOR_MAGENTA);
    drawColorBar(barHeight * 6, lcd.height() - 1,  COLOR_WHITE);

    // Draw 4 corner marker pixels to verify bounds
    lcd.drawPixel(0, 0, COLOR_RED);
    lcd.drawPixel(lcd.width() - 1, 0, COLOR_GREEN);
    lcd.drawPixel(0, lcd.height() - 1, COLOR_BLUE);
    lcd.drawPixel(lcd.width() - 1, lcd.height() - 1, COLOR_YELLOW);
}

void loop() {
    // Idle
}
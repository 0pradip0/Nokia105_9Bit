#include <Arduino.h>
#include <Nokia105_9Bit.h>

Nokia105_9Bit lcd(5, 18, 23, 4, 128, 160, false);

void benchmarkFrames(uint16_t color, uint8_t frames) {
    uint32_t start = millis();
    for (uint8_t i = 0; i < frames; i++) {
        lcd.fillScreen(color);
    }
    uint32_t elapsed = millis() - start;
    float fps = (frames * 1000.0) / elapsed;

    Serial.print("Rotation: ");
    Serial.print(lcd.width());
    Serial.print("x");
    Serial.print(lcd.height());
    Serial.print(" | Total time: ");
    Serial.print(elapsed);
    Serial.print(" ms | FPS: ");
    Serial.println(fps);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Maximize SPI speed to 26 MHz
    lcd.begin(26 * 1000 * 1000);
}

void loop() {
    // Cycle through all 4 orientations
    for (uint8_t r = 0; r < 4; r++) {
        lcd.setRotation(r);

        // Flash solid color to show new orientation
        uint16_t testColor = (r % 2 == 0) ? 0x001F : 0xF800; // Blue for portrait, Red for landscape
        benchmarkFrames(testColor, 30); // Draw 30 full frames

        delay(1500);
    }
}
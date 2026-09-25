# Nokia105_9Bit

[![Framework](https://img.shields.io/badge/Framework-Arduino%20ESP32-blue.svg)](https://github.com/espressif/arduino-esp32)
[![License: Non-Commercial](https://img.shields.io/badge/License-Personal%20%2F%20Non--Commercial-red.svg)](LICENSE)
[![SPI Speed](https://img.shields.io/badge/SPI%20Speed-Up%20to%2026MHz-brightgreen.svg)]()
[![Frame Rate](https://img.shields.io/badge/Frame%20Rate-60%2B%20FPS-success.svg)]()

A lightweight, high-speed display driver for the **ESP32** designed to operate salvaged **9-bit serial Nokia 105 / 1616 LCD screens** (SPFD54124B / ST7735 controller). It utilizes the ESP32's native SPI peripheral and hardware bit-packing to bypass the throughput limitations of software bit-banging.

---

## Technical Overview

Nokia 105 screens require a non-standard 9-bit transmission format (1 command/data bit + 8 data bits). Standard microcontrollers often resort to manual software pin-toggling ("bit-banging") or single-byte transactions, introducing severe CPU overhead and dropping refresh rates below 5 FPS.

This library solves the protocol bottleneck by packing 9-bit frames into continuous 72-bit hardware SPI chunks (8 pixels = 9 SPI bytes). Pixel blocks are streamed in 5.7 KB hardware bursts at up to 26 MHz, delivering full-screen refresh rates above 60 FPS without monopolizing system memory.

---

## Pros & Cons

### Pros
* **High Refresh Rate:** Reaches 55–65+ FPS full-screen refreshes at 26 MHz bus speeds.
* **Low RAM Overhead (~5.7 KB):** Uses an internal chunk buffer rather than allocating a full off-screen framebuffer (~133 KB), leaving internal SRAM available for application memory.
* **Hardware-Driven:** Leverages the ESP32 native `spi_master` hardware peripheral rather than CPU-blocking GPIO delays.
* **LVGL Ready:** Directly interfaces with LVGL 8 and LVGL 9 display flush callbacks.
* **Self-Contained:** Zero external library dependencies required for standalone drawing.
* **Hardware Display Controls:** Includes software rotation (0–3), address windowing, display sleep/wake, and backlight PWM control.

### Cons
* **ESP32 Architecture Only:** Relies directly on the ESP-IDF SPI driver (`driver/spi_master.h`); will not compile on AVR (Arduino Uno/Nano), SAMD, or RP2040.
* **Display Specific:** Hardcoded for 9-bit serial controllers (SPFD54124B / ST7735 9-bit variant); incompatible with standard 4-wire 8-bit SPI panels (ILI9341, ST7789).
* **Write-Only Bus:** 3-wire serial operation does not provide a MISO line; display RAM readback is unsupported.
* **Physical Assembly Required:** Salvaged phone LCDs feature high-density flex ribbons that require breakout boards or fine-pitch soldering.

---

## Hardware Specifications & Pinout

### Display Pinout

| LCD Pin # | Label | Description | ESP32 Connection |
|:---:|:---|:---|:---|
| **1** | **TE** | Tearing Effect (V-Sync) | **Leave Disconnected** |
| **2** | **RES** | Reset (Active Low) | **GPIO 4** |
| **3** | **CS** | Chip Select (Active Low) | **GPIO 5** |
| **4** | **CLK** | Serial Clock (SCL / SCK) | **GPIO 18** (VSPI SCK) |
| **5** | **SDA** | Serial Data (MOSI / SDIN) | **GPIO 23** (VSPI MOSI) |
| **6** | **VDD** | Logic Supply (3.3V) | **3.3V** |
| **7** | **GND** | Ground | **GND** |
| **8** | **LEDA** | Backlight Anode (+) | **GPIO 15** (via 47Ω–100Ω resistor) or **3.3V** |
| **9** | **LEDK** | Backlight Cathode (-) | **GND** |

> **Warning:** Supply logic and backlight strictly with 3.3V. Applying 5V will permanently damage the display controller.

---

## Installation

1. Download the repository source.
2. Place the folder into your local Arduino libraries directory:
   * **Windows:** `Documents/Arduino/libraries/Nokia105_9Bit`
   * **Linux:** `~/Arduino/libraries/Nokia105_9Bit`
   * **macOS:** `~/Documents/Arduino/libraries/Nokia105_9Bit`
3. Restart the Arduino IDE.

---

## How to Use

### 1. Standalone Setup
The library can be used independently to draw shapes, text, or solid fills without any UI framework.

```cpp
#include <Arduino.h>
#include <Nokia105_9Bit.h>

// Constructor parameters: CS, CLK, SDA, RES, Backlight, Width, Height, BGR
Nokia105_9Bit lcd(5, 18, 23, 4, 15, 128, 160, false);

void setup() {
    // Initialize SPI bus at 26 MHz
    lcd.begin(26 * 1000 * 1000);

    // 0 = Portrait (128x160), 1 = Landscape (160x128)
    lcd.setRotation(1);

    // Set backlight brightness: 0 (Off) to 255 (Full)
    lcd.setBrightness(200);

    // Clear screen with RGB565 color
    lcd.fillScreen(0x001F); // Blue

    // Draw individual pixel: x, y, color
    lcd.drawPixel(80, 64, 0xFFFF); // Center white pixel
}

void loop() {
}

#include <Arduino.h>
#include <Nokia105_9Bit.h>
#include <math.h>

// CS=5, CLK=18, SDA=23, RES=4, Native Res=128x160, BGR=false
Nokia105_9Bit lcd(5, 18, 23, 4, 128, 160, false);

#define W 160
#define H 128

// Full 40KB Off-Screen Framebuffer (eliminates all screen flicker)
static uint16_t fb[W * H];

// RGB565 Neon Color Palette
#define BG_COLOR       0x0821 // Deep cosmic navy
#define COLOR_CYAN     0x07FF
#define COLOR_MAGENTA  0xF81F
#define COLOR_YELLOW   0xFFE0
#define COLOR_GREEN    0x07E0
#define COLOR_WHITE    0xFFFF
#define COLOR_GRAY     0x52AA

// =========================================================================
// MINIMAL 5x7 ASCII FONT ENGINE (Numbers & HUD Text)
// =========================================================================
static const uint8_t font5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // P
    {0x26, 0x49, 0x49, 0x49, 0x32}, // S
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // M
    {0x00, 0x00, 0x00, 0x00, 0x00}  // Space
};

uint8_t getCharIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '.') return 10;
    if (c == ':') return 11;
    if (c == 'F') return 12;
    if (c == 'P') return 13;
    if (c == 'S') return 14;
    if (c == 'M' || c == 'm') return 15;
    return 16;
}

inline void drawPixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < W && y >= 0 && y < H) {
        fb[y * W + x] = color;
    }
}

void drawFastChar(int x, int y, char c, uint16_t color) {
    uint8_t idx = getCharIndex(c);
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            if (line & 0x01) drawPixel(x + col, y + row, color);
            line >>= 1;
        }
    }
}

void drawString(int x, int y, const char *str, uint16_t color) {
    while (*str) {
        drawFastChar(x, y, *str++, color);
        x += 6;
    }
}

// Fast Bresenham Line Drawing
void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// =========================================================================
// 3D WIREFRAME CUBE DATA
// =========================================================================
const float cubeVerts[8][3] = {
    {-28, -28, -28}, { 28, -28, -28}, { 28,  28, -28}, {-28,  28, -28},
    {-28, -28,  28}, { 28, -28,  28}, { 28,  28,  28}, {-28,  28,  28}
};

const uint8_t cubeEdges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, // Back square
    {4,5}, {5,6}, {6,7}, {7,4}, // Front square
    {0,4}, {1,5}, {2,6}, {3,7}  // Cross ribs
};

// =========================================================================
// BACKGROUND STARFIELD (WARP EFFECT)
// =========================================================================
#define NUM_STARS 40
struct Star { float x, y, z; };
Star stars[NUM_STARS];

void initStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = (rand() % 160) - 80;
        stars[i].y = (rand() % 128) - 64;
        stars[i].z = (rand() % 100) + 1;
    }
}

void renderStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= 2.0f; // Star speed
        if (stars[i].z <= 2.0f) {
            stars[i].z = 100.0f;
            stars[i].x = (rand() % 160) - 80;
            stars[i].y = (rand() % 128) - 64;
        }
        int sx = (int)(W / 2 + (stars[i].x * 60.0f) / stars[i].z);
        int sy = (int)(H / 2 + (stars[i].y * 60.0f) / stars[i].z);
        uint16_t color = (stars[i].z < 40) ? COLOR_WHITE : COLOR_GRAY;
        drawPixel(sx, sy, color);
    }
}

// =========================================================================
// SETUP & ANIMATION LOOP
// =========================================================================
void setup() {
    Serial.begin(115200);

    // Run at full 26 MHz SPI bus speed in 160x128 Landscape
    lcd.begin(26 * 1000 * 1000);
    lcd.setRotation(1);
    initStars();
}

void loop() {
    static float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
    static uint32_t frameCount = 0;
    static uint32_t lastFPSTime = 0;
    static float currentFPS = 0.0f;
    static float frameTimeMs = 0.0f;

    uint32_t frameStart = millis();

    // 1. Clear off-screen framebuffer
    for (int i = 0; i < W * H; i++) fb[i] = BG_COLOR;

    // 2. Draw Warp Starfield
    renderStars();

    // 3. Project & Rotate 3D Cube Vertices
    int projX[8], projY[8];
    float cx = cos(angleX), sx = sin(angleX);
    float cy = cos(angleY), sy = sin(angleY);
    float cz = cos(angleZ), sz = sin(angleZ);

    for (int i = 0; i < 8; i++) {
        float x = cubeVerts[i][0];
        float y = cubeVerts[i][1];
        float z = cubeVerts[i][2];

        // 3D Rotations
        float y1 = y * cx - z * sx;
        float z1 = y * sx + z * cx;
        float x2 = x * cy + z1 * sy;
        float z2 = -x * sy + z1 * cy;
        float x3 = x2 * cz - y1 * sz;
        float y3 = x2 * sz + y1 * cz;

        // Perspective Projection
        float distance = 110.0f;
        float fov = 130.0f;
        projX[i] = (int)(W / 2 + (x3 * fov) / (z2 + distance));
        projY[i] = (int)(H / 2 - 2 + (y3 * fov) / (z2 + distance));
    }

    // 4. Draw Cube Neon Edges
    for (int i = 0; i < 12; i++) {
        uint16_t edgeColor = (i < 4) ? COLOR_CYAN : (i < 8 ? COLOR_MAGENTA : COLOR_YELLOW);
        drawLine(projX[cubeEdges[i][0]], projY[cubeEdges[i][0]],
                 projX[cubeEdges[i][1]], projY[cubeEdges[i][1]], edgeColor);
    }

    // 5. Draw Animated Audio Spectrum Bar Graph at the bottom
    for (int b = 0; b < 16; b++) {
        int barHeight = (int)(sin(angleX * 2.5f + b * 0.45f) * 8.0f + 9.0f);
        int bx = 16 + b * 8;
        for (int h = 0; h < barHeight; h++) {
            uint16_t barColor = (h > 12) ? COLOR_MAGENTA : COLOR_GREEN;
            drawLine(bx, H - 4 - h, bx + 5, H - 4 - h, barColor);
        }
    }

    // 6. Real-time Benchmark HUD
    char hudBuf[24];
    snprintf(hudBuf, sizeof(hudBuf), "FPS: %d.%d", (int)currentFPS, (int)(currentFPS * 10) % 10);
    drawString(6, 6, hudBuf, COLOR_GREEN);

    snprintf(hudBuf, sizeof(hudBuf), "%d.%d MS", (int)frameTimeMs, (int)(frameTimeMs * 10) % 10);
    drawString(104, 6, hudBuf, COLOR_YELLOW);

    // 7. Blast entire buffer to Nokia LCD in one contiguous SPI burst
    lcd.setAddrWindow(0, 0, W - 1, H - 1);
    lcd.pushColors(fb, W * H);

    // Increment 3D rotation angles
    angleX += 0.045f;
    angleY += 0.065f;
    angleZ += 0.025f;

    // Track frame times and calculate running FPS
    frameCount++;
    frameTimeMs = (float)(millis() - frameStart);

    if (millis() - lastFPSTime >= 500) {
        currentFPS = (frameCount * 1000.0f) / (millis() - lastFPSTime);
        frameCount = 0;
        lastFPSTime = millis();
    }
}
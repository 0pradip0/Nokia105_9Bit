#include <Arduino.h>
#include <Nokia105_9Bit.h>
#include <lvgl.h>
#include "esp_timer.h"
#include <math.h>

// CS=5, CLK=18, SDA=23, RES=4, Native Res=128x160
Nokia105_9Bit lcd(13, 18, 23, 12, 128, 160, true);

// 20-line draw buffer for LVGL (Only ~6.4 KB RAM!)
static uint8_t lv_buf[160 * 20 * 2]; 

// UI Objects
static lv_obj_t *time_label;
static lv_obj_t *sec_label;
static lv_obj_t *activity_arc;
static lv_obj_t *fps_label;

// High-Speed LVGL 9 Flush Callback
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t width  = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);

    lcd.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    lcd.pushColors((uint16_t *)px_map, width * height);

    lv_display_flush_ready(disp);
}

// Background 2ms hardware timer for smooth LVGL animations
static void lv_tick_task(void *arg) {
    lv_tick_inc(2);
}

void init_lvgl_tick() {
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 2000);
}

void create_watchface() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0E17), LV_PART_MAIN); // Deep watch dark

    // 1. Animated Activity Outer Arc
    activity_arc = lv_arc_create(scr);
    lv_obj_set_size(activity_arc, 122, 122);
    lv_arc_set_rotation(activity_arc, 135);
    lv_arc_set_bg_angles(activity_arc, 0, 270);
    lv_arc_set_range(activity_arc, 0, 100);
    lv_obj_align(activity_arc, LV_ALIGN_CENTER, 0, -4);
    lv_obj_remove_style(activity_arc, NULL, LV_PART_KNOB); // Hide draggable knob
    lv_obj_set_style_arc_width(activity_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(activity_arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    lv_obj_set_style_arc_width(activity_arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(activity_arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);

    // 2. Digital Main Time (HH:MM)
    time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "10:42");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_CENTER, -10, -6);

    // 3. Ticking Seconds
    sec_label = lv_label_create(scr);
    lv_label_set_text(sec_label, ":38");
    lv_obj_set_style_text_color(sec_label, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_align_to(sec_label, time_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 0);

    // 4. Step Counter / Battery readout
    lv_obj_t *step_label = lv_label_create(scr);
    lv_label_set_text(step_label, "8,420 STEPS");
    lv_obj_set_style_text_color(step_label, lv_color_hex(0x94A3B8), LV_PART_MAIN);
    lv_obj_align(step_label, LV_ALIGN_CENTER, 0, 16);

    // 5. Live Frame Rate readout
    fps_label = lv_label_create(scr);
    lv_label_set_text(fps_label, "FPS: --");
    lv_obj_set_style_text_color(fps_label, lv_color_hex(0xFACC15), LV_PART_MAIN);
    lv_obj_align(fps_label, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void setup() {
    Serial.begin(115200);

    // 1. Initialize display at 26 MHz
    lcd.begin(26 * 1000 * 1000);
    lcd.setRotation(1); // 160x128 Landscape Watch Mode
    lcd.fillScreen(0x0A0E17);

    // 2. Start LVGL Engine
    lv_init();
    init_lvgl_tick();

    // 3. Register Display Buffer
    lv_display_t *disp = lv_display_create(lcd.width(), lcd.height());
    lv_display_set_buffers(disp, lv_buf, NULL, sizeof(lv_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // 4. Build Watch Interface
    create_watchface();
}

void loop() {
    lv_timer_handler(); // Renders UI

    // Watchface Animation Loop
    static uint32_t last_tick = 0;
    static int arc_val = 0;
    static int sec_count = 0;
    static uint32_t frames = 0;
    static uint32_t last_fps_time = 0;

    frames++;

    if (millis() - last_tick > 50) { // 20 updates/second for animations
        last_tick = millis();

        // Animate arc
        arc_val = (arc_val + 1) % 100;
        lv_arc_set_value(activity_arc, arc_val);

        // Update seconds text
        sec_count = (sec_count + 1) % 60;
        char s_buf[8];
        snprintf(s_buf, sizeof(s_buf), ":%02d", sec_count);
        lv_label_set_text(sec_label, s_buf);
    }

    // Update FPS Counter every 500 ms
    if (millis() - last_fps_time >= 500) {
        float fps = (frames * 1000.0f) / (millis() - last_fps_time);
        char fps_str[16];
        snprintf(fps_str, sizeof(fps_str), "FPS: %d", (int)fps);
        lv_label_set_text(fps_label, fps_str);

        frames = 0;
        last_fps_time = millis();
    }

    delay(2);
}
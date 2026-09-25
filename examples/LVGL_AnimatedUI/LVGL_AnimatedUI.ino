#include <Arduino.h>
#include <Nokia105_9Bit.h>
#include <lvgl.h>
#include "esp_timer.h"

// Landscape layout (160x128)
Nokia105_9Bit lcd(5, 18, 23, 4, 128, 160, false);

// 20-line draw buffer
static uint8_t draw_buf[160 * 20 * 2]; 

static lv_obj_t *bar;
static lv_obj_t *val_label;

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t width  = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);

    lcd.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    lcd.pushColors((uint16_t *)px_map, width * height);

    lv_display_flush_ready(disp);
}

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

void create_dashboard() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), LV_PART_MAIN); // Slate navy

    // Title Label
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SYSTEM STATUS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x94A3B8), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Animated Progress Bar
    bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 120, 14);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, -2);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x06B6D4), LV_PART_INDICATOR); // Cyan indicator

    // Value Label below the bar
    val_label = lv_label_create(scr);
    lv_label_set_text(val_label, "0 %");
    lv_obj_set_style_text_color(val_label, lv_color_hex(0x38BDF8), LV_PART_MAIN);
    lv_obj_align(val_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void setup() {
    Serial.begin(115200);

    lcd.begin(26 * 1000 * 1000);
    lcd.setRotation(1); // 160x128 Landscape
    lcd.fillScreen(0x0F172A);

    lv_init();
    init_lvgl_tick();

    lv_display_t *disp = lv_display_create(lcd.width(), lcd.height());
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    create_dashboard();
}

void loop() {
    lv_timer_handler();

    // Animate the progress bar dynamically
    static uint32_t last_update = 0;
    static int32_t progress = 0;
    static int8_t step = 2;

    if (millis() - last_update > 40) { // ~25 FPS logic update
        last_update = millis();
        progress += step;
        if (progress >= 100 || progress <= 0) step = -step;

        lv_bar_set_value(bar, progress, LV_ANIM_OFF);

        char buf[12];
        snprintf(buf, sizeof(buf), "%d %%", progress);
        lv_label_set_text(val_label, buf);
    }

    delay(2);
}
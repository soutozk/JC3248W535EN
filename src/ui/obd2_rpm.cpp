#include "obd2_rpm.h"

#include "navigation.h"
#include "obd2.h"
#include "obd2_data.h"
#include "audio_beep.h"
#include "theme.h"

#include "lvgl.h"

#include <stdio.h>

namespace ui {
namespace obd2_rpm {

static constexpr int kMaxRpm = 9000;
static constexpr int kAlertRpm = 7500;
static constexpr size_t kSegmentCount = 36;
static constexpr size_t kLedCount = 16;

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_value = nullptr;
static lv_obj_t *s_shift_overlay = nullptr;
static lv_obj_t *s_shift_label = nullptr;
static lv_obj_t *s_segments[kSegmentCount] = {};
static lv_obj_t *s_leds[kLedCount] = {};
static lv_timer_t *s_update_timer = nullptr;
static lv_timer_t *s_alert_timer = nullptr;
static bool s_alert_active = false;
static bool s_alert_phase = true;
static size_t s_last_active_segments = kSegmentCount + 1;
static size_t s_last_active_leds = kLedCount + 1;

// Curva ascendente e progressivamente mais plana, como no painel de referencia.
static constexpr lv_coord_t kArcY[kSegmentCount] = {
    145, 141, 137, 133, 129, 125, 121, 117, 113,
    109, 105, 101, 97, 94, 91, 88, 85, 82,
    79, 77, 75, 73, 71, 69, 68, 67, 66,
    66, 66, 67, 68, 70, 72, 75, 78, 82,
};

static constexpr size_t kScaleSegment[10] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 35};

static lv_color_t amber()
{
    return theme::colors().cyan;
}

static lv_color_t amber_dim()
{
    return theme::colors().panel_alt;
}

static lv_color_t green()
{
    return lv_color_hex(0x79D616);
}

static lv_color_t red()
{
    return lv_color_hex(0xFF3218);
}

static lv_color_t zone_color(size_t index)
{
    const int segment_rpm = static_cast<int>((index + 1) * kMaxRpm / kSegmentCount);
    if(segment_rpm <= 5500) return green();
    if(segment_rpm <= 7000) return amber();
    return red();
}

static lv_obj_t *block(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                       lv_color_t fill, lv_opa_t opacity)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, fill, 0);
    lv_obj_set_style_bg_opa(obj, opacity, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
                      lv_color_t text_color, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, text_color, 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

static void back_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        obd2::show();
    }
}

static lv_obj_t *back_button(lv_obj_t *parent, bool alert_style)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 48, 24);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -12, 11);
    lv_obj_set_style_bg_color(button,
                              alert_style ? lv_color_hex(0x5C0000) : theme::colors().panel, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, alert_style ? lv_color_white() : amber_dim(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 3, 0);
    lv_obj_add_event_cb(button, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *caption = text(button, "<", theme::font_body(),
                             alert_style ? lv_color_white() : amber(), LV_ALIGN_CENTER, 0, -1);
    lv_obj_clear_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

static lv_obj_t *status_box(lv_obj_t *parent, const char *caption, lv_color_t box_color,
                            lv_align_t align, lv_coord_t x)
{
    lv_obj_t *box = block(parent, 64, 30, lv_color_black(), LV_OPA_COVER);
    lv_obj_align(box, align, x, -15);
    lv_obj_set_style_border_color(box, box_color, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 2, 0);
    text(box, caption, theme::font_body(), box_color, LV_ALIGN_CENTER, 0, 0);
    return box;
}

static void set_shift_visual(bool visible)
{
    if(s_shift_overlay == nullptr) return;
    if(visible) {
        lv_obj_clear_flag(s_shift_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_shift_overlay);
    } else {
        lv_obj_add_flag(s_shift_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_alert_state(int rpm)
{
    const bool active = rpm >= kAlertRpm;
    if(active == s_alert_active) return;

    s_alert_active = active;
    s_alert_phase = true;
    if(active) {
        audio_beep_play_shift();
        lv_obj_set_style_bg_color(s_shift_overlay, lv_color_hex(0xE00000), 0);
        lv_obj_set_style_bg_opa(s_shift_overlay, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(s_shift_label, LV_OPA_COVER, 0);
    }
    set_shift_visual(active);
}

static void update_visuals(int rpm)
{
    if(rpm < 0) rpm = 0;
    if(rpm > kMaxRpm) rpm = kMaxRpm;

    char value[12];
    snprintf(value, sizeof(value), "%d", rpm);
    lv_label_set_text(s_value, value);

    size_t active_segments = static_cast<size_t>((rpm * kSegmentCount) / kMaxRpm);
    if(rpm == kMaxRpm) active_segments = kSegmentCount;
    if(active_segments != s_last_active_segments) {
        for(size_t i = 0; i < kSegmentCount; ++i) {
            const bool active = i < active_segments;
            const lv_color_t segment_color = zone_color(i);
            lv_obj_set_style_bg_color(s_segments[i], active ? segment_color : amber_dim(), 0);
            lv_obj_set_style_bg_opa(s_segments[i], active ? LV_OPA_COVER : LV_OPA_30, 0);
            lv_obj_set_style_border_color(s_segments[i], segment_color, 0);
            lv_obj_set_style_border_opa(s_segments[i], active ? LV_OPA_90 : LV_OPA_20, 0);
        }
        s_last_active_segments = active_segments;
    }

    size_t active_leds = static_cast<size_t>((rpm * kLedCount) / kMaxRpm);
    if(rpm == kMaxRpm) active_leds = kLedCount;
    if(active_leds != s_last_active_leds) {
        for(size_t i = 0; i < kLedCount; ++i) {
            const bool active = i < active_leds;
            const int led_rpm = static_cast<int>((i + 1) * kMaxRpm / kLedCount);
            const lv_color_t led_color = led_rpm > 7000 ? red() : (led_rpm > 5500 ? amber() : green());
            lv_obj_set_style_bg_color(s_leds[i], active ? led_color : theme::colors().panel, 0);
            lv_obj_set_style_bg_opa(s_leds[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_leds[i], led_color, 0);
            lv_obj_set_style_border_opa(s_leds[i], active ? LV_OPA_COVER : LV_OPA_60, 0);
            lv_obj_set_style_shadow_color(s_leds[i], led_color, 0);
            lv_obj_set_style_shadow_width(s_leds[i], active ? 9 : 0, 0);
            lv_obj_set_style_shadow_opa(s_leds[i], active ? LV_OPA_70 : LV_OPA_TRANSP, 0);
        }
        s_last_active_leds = active_leds;
    }

    update_alert_state(rpm);
}

static void update_timer_cb(lv_timer_t *timer)
{
    (void) timer;
    obd2::data_provider().tick(40);
    update_visuals(obd2::data_provider().get_rpm());
}

static void alert_timer_cb(lv_timer_t *timer)
{
    (void) timer;
    if(!s_alert_active) return;

    s_alert_phase = !s_alert_phase;
    lv_obj_set_style_bg_color(s_shift_overlay,
                              lv_color_hex(s_alert_phase ? 0xF00000 : 0x260000), 0);
    lv_obj_set_style_opa(s_shift_label, s_alert_phase ? LV_OPA_COVER : LV_OPA_70, 0);
}

static void cleanup_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DELETE) return;

    if(s_update_timer != nullptr) {
        lv_timer_del(s_update_timer);
        s_update_timer = nullptr;
    }
    if(s_alert_timer != nullptr) {
        lv_timer_del(s_alert_timer);
        s_alert_timer = nullptr;
    }
    s_screen = nullptr;
    s_value = nullptr;
    s_shift_overlay = nullptr;
    s_shift_label = nullptr;
    for(size_t i = 0; i < kSegmentCount; ++i) s_segments[i] = nullptr;
    for(size_t i = 0; i < kLedCount; ++i) s_leds[i] = nullptr;
    s_alert_active = false;
    s_alert_phase = true;
    s_last_active_segments = kSegmentCount + 1;
    s_last_active_leds = kLedCount + 1;
}

void show()
{
    theme::init();
    obd2::data_provider().reset();

    const lv_coord_t screen_w = lv_disp_get_hor_res(nullptr);
    const lv_coord_t screen_h = lv_disp_get_ver_res(nullptr);

    lv_obj_t *screen = lv_obj_create(nullptr);
    s_screen = screen;
    lv_obj_add_event_cb(screen, cleanup_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, theme::colors().background, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Moldura preta espessa e visor interno âmbar, reproduzindo o painel automotivo.
    lv_obj_t *bezel = block(screen, screen_w - 8, screen_h - 8,
                            theme::colors().panel, LV_OPA_COVER);
    lv_obj_align(bezel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(bezel, theme::colors().panel_alt, 0);
    lv_obj_set_style_border_width(bezel, 3, 0);
    lv_obj_set_style_radius(bezel, 15, 0);
    lv_obj_set_style_shadow_color(bezel, lv_color_black(), 0);
    lv_obj_set_style_shadow_width(bezel, 12, 0);
    lv_obj_set_style_shadow_opa(bezel, LV_OPA_80, 0);

    lv_obj_t *display = block(bezel, screen_w - 30, screen_h - 28,
                              lv_color_hex(0x030200), LV_OPA_COVER);
    lv_obj_align(display, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(display, amber_dim(), 0);
    lv_obj_set_style_border_width(display, 2, 0);
    lv_obj_set_style_radius(display, 9, 0);

    text(display, "RPM", theme::font_title(), amber(),
         LV_ALIGN_TOP_LEFT, 15, 10);
    back_button(display, false);

    // Escala 0..9 distribuida sobre a mesma curva dos segmentos.
    for(int i = 0; i <= 9; ++i) {
        char scale[3];
        snprintf(scale, sizeof(scale), "%d", i);
        const size_t segment = kScaleSegment[static_cast<size_t>(i)];
        lv_obj_t *tick = text(display, scale, theme::font_body(), zone_color(segment),
                              LV_ALIGN_TOP_LEFT,
                              7 + static_cast<lv_coord_t>(segment * 12),
                              kArcY[segment] - 27);
        lv_obj_set_width(tick, 18);
        lv_obj_set_style_text_align(tick, LV_TEXT_ALIGN_CENTER, 0);
    }

    for(size_t i = 0; i < kSegmentCount; ++i) {
        s_segments[i] = block(display, 9, 56, amber_dim(), LV_OPA_30);
        lv_obj_set_style_radius(s_segments[i], 1, 0);
        lv_obj_set_style_border_width(s_segments[i], 1, 0);
        lv_obj_align(s_segments[i], LV_ALIGN_TOP_LEFT,
                     12 + static_cast<lv_coord_t>(i * 12), kArcY[i] - 6);
    }

    s_value = text(display, "0", theme::font_body(), amber(),
                   LV_ALIGN_TOP_RIGHT, -72, 170);
    lv_obj_set_width(s_value, 210);
    lv_obj_set_style_text_align(s_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_transform_pivot_x(s_value, 210, 0);
    lv_obj_set_style_transform_pivot_y(s_value, 8, 0);
    lv_obj_set_style_transform_zoom(s_value, 640, 0);
    text(display, "RPM", theme::font_body(), amber(),
         LV_ALIGN_TOP_RIGHT, -16, 178);

    // status_box(display, "IDLE", green(), LV_ALIGN_BOTTOM_LEFT, 10);
    // status_box(display, "SHIFT", red(), LV_ALIGN_BOTTOM_RIGHT, -10);

    const lv_coord_t led_size = 15;
    const lv_coord_t led_margin = 12;
    const lv_coord_t display_w = screen_w - 30;
    const lv_coord_t led_span = display_w - (2 * led_margin) - led_size;
    for(size_t i = 0; i < kLedCount; ++i) {
        s_leds[i] = block(display, led_size, led_size, theme::colors().panel, LV_OPA_COVER);
        lv_obj_set_style_radius(s_leds[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_leds[i], 2, 0);
        const lv_coord_t led_x = led_margin + static_cast<lv_coord_t>(
            (i * led_span) / (kLedCount - 1));
        lv_obj_align(s_leds[i], LV_ALIGN_BOTTOM_LEFT,
                     led_x, -22);
    }

    // Suave textura horizontal de visor sem alterar a legibilidade.
    for(lv_coord_t y = 4; y < screen_h - 38; y += 5) {
        lv_obj_t *line = block(display, screen_w - 36, 1, amber_dim(), LV_OPA_10);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_move_background(line);
    }

    s_shift_overlay = block(screen, screen_w, screen_h, red(), LV_OPA_COVER);
    lv_obj_add_flag(s_shift_overlay, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_set_style_border_color(s_shift_overlay, lv_color_white(), 0);
    // lv_obj_set_style_border_width(s_shift_overlay, 0, 0);
    s_shift_label = text(s_shift_overlay, "SHIFT", &lv_font_montserrat_48,
                         lv_color_white(), LV_ALIGN_CENTER, 0, -3);
    lv_obj_set_style_text_letter_space(s_shift_label, 6, 0);
    // text(s_shift_overlay, "5000+ RPM", theme::font_body(),
    //      lv_color_hex(0xFFE6A0), LV_ALIGN_CENTER, 0, 50);
    back_button(s_shift_overlay, true);

    update_visuals(0);
    s_update_timer = lv_timer_create(update_timer_cb, 40, nullptr);
    s_alert_timer = lv_timer_create(alert_timer_cb, 110, nullptr);

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
}

} // namespace obd2_rpm
} // namespace ui

#include "obd_screens.h"
#include "obd_constants.h"
#include "obd_telemetry.h"
#include "theme.h"

#include "esp_timer.h"
#include "lvgl.h"

#include <math.h>
#include <stdio.h>

namespace obd {
namespace screens {
namespace {

namespace theme = ui::theme;

constexpr int kMaxSpeedKmh = 180;
constexpr size_t kTickCount = 73;
constexpr size_t kMajorTickEvery = 8;
constexpr float kStartAngleDeg = 150.0f;
constexpr float kEndAngleDeg = 30.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr lv_coord_t kValueCanvasWidth = 144;
constexpr lv_coord_t kValueCanvasHeight = 48;
constexpr int kValuePixelScale = 3;

static lv_obj_t *s_valueCanvas = nullptr;
static lv_color_t s_valueCanvasBuffer[kValueCanvasWidth * kValueCanvasHeight];
static lv_obj_t *s_mode = nullptr;
static lv_timer_t *s_updateTimer = nullptr;
static lv_point_t s_tickPoints[kTickCount][2];
static lv_obj_t *s_ticks[kTickCount] = {};
static lv_color_t s_tickColors[kTickCount];
static bool s_tickIsMajor[kTickCount] = {};
static lv_obj_t *s_scaleLabels[10] = {};
static float s_shownSpeed = 0.0f;
static int s_lastDisplayed = -2;

lv_obj_t *block(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                lv_color_t color, lv_opa_t opacity = LV_OPA_COVER)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opacity, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
               lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

lv_color_t amber()
{
    return theme::colors().cyan;
}

lv_color_t amberDim()
{
    return theme::colors().panel_alt;
}

lv_color_t green()
{
    return lv_color_hex(0x79D616);
}

lv_color_t red()
{
    return lv_color_hex(0xFF3218);
}

lv_color_t speedColor(float speed)
{
    if(speed <= 80.0f) return green();
    if(speed <= 140.0f) return amber();
    return red();
}

void polarPoint(lv_point_t &point, float angleDeg, float radius)
{
    const float angle = angleDeg * kPi / 180.0f;
    point.x = static_cast<lv_coord_t>(lroundf(240.0f + cosf(angle) * radius));
    point.y = static_cast<lv_coord_t>(lroundf(309.0f - sinf(angle) * radius));
}

void createScale(lv_obj_t *screen)
{
    for(size_t i = 0; i < kTickCount; ++i) {
        const float ratio = static_cast<float>(i) / static_cast<float>(kTickCount - 1);
        const float speed = ratio * kMaxSpeedKmh;
        const float angle = kStartAngleDeg + (kEndAngleDeg - kStartAngleDeg) * ratio;
        const bool major = (i % kMajorTickEvery) == 0;
        polarPoint(s_tickPoints[i][0], angle, major ? 190.0f : 199.0f);
        polarPoint(s_tickPoints[i][1], angle, 211.0f);

        s_tickColors[i] = speedColor(speed);
        s_tickIsMajor[i] = major;
        s_ticks[i] = lv_line_create(screen);
        lv_line_set_points(s_ticks[i], s_tickPoints[i], 2);
        lv_obj_set_style_line_width(s_ticks[i], major ? 5 : 3, 0);
        lv_obj_set_style_line_color(s_ticks[i], amberDim(), 0);
        lv_obj_set_style_line_opa(s_ticks[i], LV_OPA_30, 0);
        lv_obj_set_style_line_rounded(s_ticks[i], false, 0);
    }

    for(int speed = 0; speed <= kMaxSpeedKmh; speed += 20) {
        const float ratio = static_cast<float>(speed) / kMaxSpeedKmh;
        const float angle = kStartAngleDeg + (kEndAngleDeg - kStartAngleDeg) * ratio;
        lv_point_t position;
        polarPoint(position, angle, 229.0f);
        char value[4];
        snprintf(value, sizeof(value), "%d", speed);
        const size_t labelIndex = static_cast<size_t>(speed / 20);
        s_scaleLabels[labelIndex] = text(screen, value, theme::font_body(),
                                         amberDim(), LV_ALIGN_TOP_LEFT,
                                         position.x - 32, position.y - 10);
        lv_label_set_long_mode(s_scaleLabels[labelIndex], LV_LABEL_LONG_CLIP);
        lv_obj_set_size(s_scaleLabels[labelIndex], 64, 16);
        lv_obj_set_style_text_align(s_scaleLabels[labelIndex], LV_TEXT_ALIGN_CENTER, 0);
    }
}

void updateScale(int speed, bool valid)
{
    for(size_t i = 0; i < kTickCount; ++i) {
        const float tickSpeed = static_cast<float>(i * kMaxSpeedKmh) /
                                static_cast<float>(kTickCount - 1);
        const bool active = valid && tickSpeed <= static_cast<float>(speed);
        lv_obj_set_style_line_color(s_ticks[i], active ? s_tickColors[i] : amberDim(), 0);
        lv_obj_set_style_line_opa(s_ticks[i], active ? LV_OPA_COVER
            : (s_tickIsMajor[i] ? LV_OPA_40 : LV_OPA_20), 0);
    }

    for(size_t i = 0; i < 10; ++i) {
        const int labelSpeed = static_cast<int>(i * 20);
        const bool active = valid && labelSpeed <= speed;
        lv_obj_set_style_text_color(s_scaleLabels[i], active
            ? speedColor(static_cast<float>(labelSpeed)) : amberDim(), 0);
    }
}

void drawValueGlyph(char character, size_t cell)
{
    const lv_font_t *font = theme::font_body();
    lv_font_glyph_dsc_t glyph{};
    if(!lv_font_get_glyph_dsc(font, &glyph, static_cast<uint32_t>(character), 0)) return;
    const uint8_t *bitmap = lv_font_get_glyph_bitmap(font, static_cast<uint32_t>(character));
    if(bitmap == nullptr || glyph.bpp != 1) return;

    constexpr int cellWidth = kValueCanvasWidth / 3;
    const int drawnWidth = glyph.box_w * kValuePixelScale;
    const int drawnHeight = glyph.box_h * kValuePixelScale;
    const int originX = static_cast<int>(cell * cellWidth) + (cellWidth - drawnWidth) / 2;
    const int originY = (kValueCanvasHeight - drawnHeight) / 2;

    for(uint16_t y = 0; y < glyph.box_h; ++y) {
        for(uint16_t x = 0; x < glyph.box_w; ++x) {
            const size_t bit = static_cast<size_t>(y) * glyph.box_w + x;
            if((bitmap[bit >> 3] & (0x80u >> (bit & 7))) == 0) continue;
            for(int py = 0; py < kValuePixelScale; ++py) {
                for(int px = 0; px < kValuePixelScale; ++px) {
                    lv_canvas_set_px_color(s_valueCanvas,
                        originX + x * kValuePixelScale + px,
                        originY + y * kValuePixelScale + py, amber());
                }
            }
        }
    }
}

void drawValue(const char value[4])
{
    lv_canvas_fill_bg(s_valueCanvas, lv_color_hex(0x030200), LV_OPA_COVER);
    for(size_t i = 0; i < 3; ++i) drawValueGlyph(value[i], i);
}

void updateReading(int speed, bool valid)
{
    const int displayed = valid ? speed : -1;
    if(displayed == s_lastDisplayed) return;
    s_lastDisplayed = displayed;
    updateScale(speed, valid);
    char value[4];
    if(valid) snprintf(value, sizeof(value), "%03d", speed);
    else snprintf(value, sizeof(value), "---");
    drawValue(value);
}

void backCb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) showMenu();
}

void updateTimerCb(lv_timer_t *)
{
    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    const ObdTelemetry telemetry = telemetrySnapshot(now);
    const bool valid = (telemetry.validMask & Speed) != 0;
    if(valid) {
        float target = telemetry.speedKmh;
        if(target < 0.0f) target = 0.0f;
        if(target > kMaxSpeedKmh) target = kMaxSpeedKmh;
        s_shownSpeed += (target - s_shownSpeed) * 0.28f;
    }
    updateReading(static_cast<int>(lroundf(s_shownSpeed)), valid);

    if(telemetryPresentationActive()) lv_obj_clear_flag(s_mode, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_mode, LV_OBJ_FLAG_HIDDEN);
}

void cleanupCb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_DELETE) return;
    if(s_updateTimer != nullptr) {
        lv_timer_del(s_updateTimer);
        s_updateTimer = nullptr;
    }
    s_valueCanvas = nullptr;
    for(auto &tick : s_ticks) tick = nullptr;
    for(auto &caption : s_scaleLabels) caption = nullptr;
    s_mode = nullptr;
    s_shownSpeed = 0.0f;
    s_lastDisplayed = -2;
}

} // namespace

void showSpeed()
{
    theme::init();
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_add_event_cb(screen, cleanupCb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, theme::colors().background, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *frame = block(screen, 470, 310, lv_color_hex(0x030200));
    lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(frame, amberDim(), 0);
    lv_obj_set_style_border_width(frame, 3, 0);
    lv_obj_set_style_radius(frame, 15, 0);
    lv_obj_set_style_shadow_color(frame, lv_color_black(), 0);
    lv_obj_set_style_shadow_width(frame, 3, 0);
    lv_obj_set_style_shadow_opa(frame, LV_OPA_30, 0);

    text(screen, "KM/H", theme::font_title(), amber(),
         LV_ALIGN_TOP_LEFT, 23, 17);
    text(screen, "VELOCIDADE", theme::font_body(), amberDim(),
         LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *back = lv_btn_create(screen);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 52, 35);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -19, 13);
    lv_obj_set_style_bg_color(back, theme::colors().panel, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(back, amberDim(), 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_radius(back, 7, 0);
    lv_obj_add_event_cb(back, backCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *backText = text(back, "<", theme::font_body(), amber(),
                              LV_ALIGN_CENTER, 0, -1);
    lv_obj_clear_flag(backText, LV_OBJ_FLAG_CLICKABLE);

    createScale(screen);

    s_valueCanvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_valueCanvas, s_valueCanvasBuffer,
                         kValueCanvasWidth, kValueCanvasHeight, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_valueCanvas, 130, 215);
    text(screen, "KM/H", theme::font_body(), amber(),
         LV_ALIGN_TOP_LEFT, 286, 231);
    s_mode = text(screen, "APRESENTACAO", theme::font_small(), amberDim(),
                  LV_ALIGN_BOTTOM_MID, 0, -10);

    updateReading(0, true);
    s_updateTimer = lv_timer_create(updateTimerCb, 40, nullptr);
    updateTimerCb(nullptr);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
}

} // namespace screens
} // namespace obd

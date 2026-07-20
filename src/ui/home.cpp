#include "home.h"

#include "assets.h"
#include "gif_player.h"
#include "home_icons.h"
#include "navigation.h"
#include "sd_browser.h"
#include "settings.h"
#include "spotify_ui.h"
#include "theme.h"

#include "lvgl.h"
#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>

namespace ui {
namespace home {

static const char *TAG = "ui_home";

void show();

static lv_color_t amber()
{
    return theme::colors().blue;
}

static lv_color_t amber_hot()
{
    return theme::colors().text;
}

static lv_obj_t *block(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_letter_space(obj, 0, 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

static void route_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    uintptr_t route = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    switch(route) {
        case 1:
            gif_player::show_list();
            break;
        case 2:
            ui_spotify_show();
            break;
        case 3:
            settings::show();
            break;
        case 4:
            sd_browser::mount();
            show();
            break;
        default:
            break;
    }
}

static lv_obj_t *home_button(lv_obj_t *parent, const lv_img_dsc_t *icon, const char *subtitle,
                             lv_align_t align, lv_coord_t x, lv_coord_t y, uintptr_t route)
{
    lv_obj_t *btn = lv_btn_create(parent);
    theme::apply_button(btn, true);
    lv_obj_set_size(btn, 132, 70);
    lv_obj_align(btn, align, x, y);
    lv_obj_add_event_cb(btn, route_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(route));

    lv_obj_set_style_bg_color(btn, theme::colors().panel, 0);
    lv_obj_set_style_border_color(btn, amber(), 0);
    lv_obj_set_style_shadow_color(btn, amber(), 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);

    lv_obj_t *icon_image = lv_img_create(btn);
    lv_img_set_src(icon_image, icon);
    lv_obj_set_size(icon_image, 64, 64);
    lv_obj_align(icon_image, LV_ALIGN_CENTER, 0, -2);
    lv_obj_clear_flag(icon_image, LV_OBJ_FLAG_CLICKABLE);

    if(subtitle != nullptr && subtitle[0] != '\0') {
        lv_obj_t *subtitle_label = label(btn, subtitle, theme::font_small(), theme::colors().muted,
                                         LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_width(subtitle_label, 104);
        lv_obj_set_style_text_align(subtitle_label, LV_TEXT_ALIGN_CENTER, 0);
    }

    lv_obj_t *lamp = block(btn, 5, 42, amber(), LV_OPA_80);
    lv_obj_align(lamp, LV_ALIGN_RIGHT_MID, -8, 0);
    return btn;
}

static void vu_bar_anim_cb(void *target, int32_t value)
{
    static constexpr lv_coord_t kVuBaselineY = 108;
    lv_obj_t *bar = static_cast<lv_obj_t *>(target);
    lv_obj_set_height(bar, static_cast<lv_coord_t>(value));
    lv_obj_set_y(bar, kVuBaselineY - static_cast<lv_coord_t>(value));
}

static void animate_vu_bar(lv_obj_t *bar, lv_coord_t low, lv_coord_t high,
                           uint32_t time_ms, uint32_t delay_ms)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, bar);
    lv_anim_set_values(&anim, low, high);
    lv_anim_set_time(&anim, time_ms);
    lv_anim_set_playback_time(&anim, time_ms + 80);
    lv_anim_set_delay(&anim, delay_ms);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, vu_bar_anim_cb);
    lv_anim_start(&anim);
}

static void create_equalizer(lv_obj_t *parent)
{
    static constexpr lv_coord_t kVuBaselineY = 108;
    static const lv_coord_t lows[] = {12, 18, 10, 24, 16, 28, 14, 22, 30, 18, 26, 12};
    static const lv_coord_t highs[] = {34, 56, 42, 68, 54, 78, 46, 70, 82, 58, 74, 38};

    for(size_t i = 0; i < sizeof(lows) / sizeof(lows[0]); ++i) {
        lv_obj_t *bar = block(parent, 10, lows[i], (i % 5 == 0) ? theme::colors().purple : amber(), LV_OPA_80);
        lv_obj_set_pos(bar, static_cast<lv_coord_t>(10 + i * 16), kVuBaselineY - lows[i]);
        lv_obj_set_style_shadow_color(bar, amber(), 0);
        lv_obj_set_style_shadow_width(bar, 5, 0);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_20, 0);
        animate_vu_bar(bar, lows[i], highs[i],
                       static_cast<uint32_t>(260 + (i % 4) * 90),
                       static_cast<uint32_t>(i * 55));
    }
}

static void create_oel_logo(lv_obj_t *parent)
{
    if(assets::home_logo_available()) {
        const char *logo_path = assets::home_logo_lvgl_path();
        ESP_LOGI(TAG, "loading home logo: %s", logo_path);
        // The SD file may have been replaced while keeping the same name.
        // Drop LVGL's decoded PNG cache so the new logo is loaded.
        lv_img_cache_invalidate_src(logo_path);
        lv_obj_t *logo = lv_img_create(parent);
        lv_img_set_src(logo, logo_path);
        lv_img_set_zoom(logo, 196);
        lv_obj_align(logo, LV_ALIGN_CENTER, 76, 0);
        lv_obj_clear_flag(logo, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    ESP_LOGW(TAG, "home logo unavailable; using text logo");
    label(parent, "SOUTOZK", theme::font_title(), amber_hot(), LV_ALIGN_CENTER, 76, 0);
}

static void create_oel_display(lv_obj_t *screen)
{
    lv_obj_t *display = theme::create_panel(screen);
    lv_obj_set_size(display, 438, 118);
    lv_obj_align(display, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(display, theme::colors().panel, 0);
    lv_obj_set_style_border_color(display, amber(), 0);
    lv_obj_set_style_shadow_color(display, amber(), 0);
    lv_obj_set_style_shadow_width(display, 18, 0);
    lv_obj_set_style_shadow_opa(display, LV_OPA_20, 0);

    label(display, "", theme::font_title(), amber_hot(), LV_ALIGN_TOP_LEFT, 10, 6);
    label(display, "", theme::font_small(), theme::colors().muted, LV_ALIGN_TOP_RIGHT, -10, 12);

    lv_obj_t *wave = label(display, "~  ~  ~", theme::font_title(), theme::colors().blue, LV_ALIGN_CENTER, -130, 6);
    lv_obj_set_style_opa(wave, LV_OPA_30, 0);
    theme::pulse_opacity(wave, LV_OPA_10, LV_OPA_50, 900);

    create_oel_logo(display);
    label(display, "", theme::font_small(), theme::colors().muted, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    create_equalizer(display);
}

static void create_status_strip(lv_obj_t *screen)
{
    lv_obj_t *strip = block(screen, lv_disp_get_hor_res(nullptr) - 24, 34, theme::colors().panel, LV_OPA_COVER);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_border_color(strip, amber(), 0);
    lv_obj_set_style_border_width(strip, 1, 0);

    label(strip, sd_browser::is_mounted() ? "SD ONLINE" : "SD OFFLINE",
          theme::font_small(), amber(), LV_ALIGN_LEFT_MID, 10, 0);

    char theme_text[40];
    snprintf(theme_text, sizeof(theme_text), "THEME %s", theme::palette_name());
    label(strip, theme_text, theme::font_small(), theme::colors().muted, LV_ALIGN_CENTER, 0, 0);
    label(strip, "SWIPE NAV: L/R/T/B", theme::font_small(), amber(), LV_ALIGN_RIGHT_MID, -10, 0);
}

static void create_background(lv_obj_t *screen)
{
    if(!assets::home_background_available()) {
        return;
    }

    lv_obj_t *background = lv_img_create(screen);
    lv_img_set_src(background, assets::home_background_lvgl_path());
    lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *overlay = block(screen, lv_disp_get_hor_res(nullptr), lv_disp_get_ver_res(nullptr),
                              lv_color_hex(0x000000),
                              theme::palette_id() == theme::PaletteId::Blue ? LV_OPA_30 : LV_OPA_60);
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    create_background(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *top = block(screen, lv_disp_get_hor_res(nullptr) - 24, 40, theme::colors().panel, LV_OPA_COVER);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_border_color(top, amber(), 0);
    lv_obj_set_style_border_width(top, 1, 0);

    label(top, "HOME", theme::font_title(), amber_hot(), LV_ALIGN_LEFT_MID, 10, 0);
    label(top, assets::project_name(), theme::font_small(), theme::colors().muted, LV_ALIGN_RIGHT_MID, -10, 0);

    create_oel_display(screen);

    lv_obj_t *button_deck = block(screen, lv_disp_get_hor_res(nullptr) - 24, 112, theme::colors().panel, LV_OPA_COVER);
    lv_obj_align(button_deck, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_set_style_border_color(button_deck, amber(), 0);
    lv_obj_set_style_border_width(button_deck, 1, 0);

    home_button(button_deck, &home_icon_play, "", LV_ALIGN_LEFT_MID, 10, 0, 1);
    home_button(button_deck, &home_icon_spotify, "", LV_ALIGN_CENTER, 0, 0, 2);
    home_button(button_deck, &home_icon_settings, "", LV_ALIGN_RIGHT_MID, -10, 0, 3);

    create_status_strip(screen);
    navigation::attach(screen);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace home
} // namespace ui

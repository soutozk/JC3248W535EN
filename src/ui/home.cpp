#include "home.h"

#include "assets.h"
#include "gif_player.h"
#include "navigation.h"
#include "sd_browser.h"
#include "settings.h"
#include "spotify_ui.h"
#include "theme.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>

namespace ui {
namespace home {

void show();

static lv_color_t amber()
{
    return theme::palette_id() == theme::PaletteId::Orange
        ? theme::colors().cyan
        : lv_color_hex(0xFFB22A);
}

static lv_color_t amber_hot()
{
    return theme::palette_id() == theme::PaletteId::Orange
        ? theme::colors().purple
        : lv_color_hex(0xFFE06B);
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

static lv_obj_t *home_button(lv_obj_t *parent, const char *title, const char *subtitle,
                             lv_align_t align, lv_coord_t x, lv_coord_t y, uintptr_t route)
{
    lv_obj_t *btn = lv_btn_create(parent);
    theme::apply_button(btn, true);
    lv_obj_set_size(btn, 178, 70);
    lv_obj_align(btn, align, x, y);
    lv_obj_add_event_cb(btn, route_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(route));

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x181006), 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x040302), 0);
    lv_obj_set_style_border_color(btn, amber(), 0);
    lv_obj_set_style_shadow_color(btn, amber(), 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);

    label(btn, title, &lv_font_montserrat_20, amber_hot(), LV_ALIGN_TOP_LEFT, 8, 8);
    label(btn, subtitle, &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_BOTTOM_LEFT, 8, -8);

    lv_obj_t *lamp = block(btn, 5, 42, amber(), LV_OPA_80);
    lv_obj_align(lamp, LV_ALIGN_RIGHT_MID, -8, 0);
    return btn;
}

static void create_equalizer(lv_obj_t *parent)
{
    static const lv_coord_t heights[] = {14, 28, 18, 42, 34, 55, 26, 47, 62, 33, 50, 21};
    for(size_t i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
        lv_obj_t *bar = block(parent, 10, heights[i], (i % 5 == 0) ? theme::colors().purple : amber(), LV_OPA_80);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, static_cast<lv_coord_t>(10 + i * 16), -10);
        lv_obj_set_style_shadow_color(bar, amber(), 0);
        lv_obj_set_style_shadow_width(bar, 5, 0);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_20, 0);
    }
}

static void create_oel_display(lv_obj_t *screen)
{
    lv_obj_t *display = theme::create_panel(screen);
    lv_obj_set_size(display, 438, 118);
    lv_obj_align(display, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(display, lv_color_hex(0x030805), 0);
    lv_obj_set_style_border_color(display, amber(), 0);
    lv_obj_set_style_shadow_color(display, amber(), 0);
    lv_obj_set_style_shadow_width(display, 18, 0);
    lv_obj_set_style_shadow_opa(display, LV_OPA_20, 0);

    label(display, "PIONEER", &lv_font_montserrat_24, amber_hot(), LV_ALIGN_TOP_LEFT, 10, 6);
    label(display, "DOLPHIN OEL", &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_TOP_RIGHT, -10, 12);

    lv_obj_t *wave = label(display, "~  ~  ~", &lv_font_montserrat_36, theme::colors().blue, LV_ALIGN_CENTER, -130, 6);
    lv_obj_set_style_opa(wave, LV_OPA_30, 0);
    theme::pulse_opacity(wave, LV_OPA_10, LV_OPA_50, 900);

    label(display, "GOLFINHO", &lv_font_montserrat_34, amber_hot(), LV_ALIGN_CENTER, 76, 0);
    label(display, "DSP  CD-MD  AUX  NAV", &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    create_equalizer(display);
}

static void create_status_strip(lv_obj_t *screen)
{
    lv_obj_t *strip = block(screen, lv_disp_get_hor_res(nullptr) - 24, 34, lv_color_hex(0x080603), LV_OPA_90);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_border_color(strip, amber(), 0);
    lv_obj_set_style_border_width(strip, 1, 0);

    label(strip, sd_browser::is_mounted() ? "SD ONLINE" : "SD OFFLINE",
          &lv_font_montserrat_12, amber(), LV_ALIGN_LEFT_MID, 10, 0);

    char theme_text[40];
    snprintf(theme_text, sizeof(theme_text), "THEME %s", theme::palette_name());
    label(strip, theme_text, &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_CENTER, 0, 0);
    label(strip, "SWIPE NAV: L/R/T/B", &lv_font_montserrat_12, amber(), LV_ALIGN_RIGHT_MID, -10, 0);
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *top = block(screen, lv_disp_get_hor_res(nullptr) - 24, 40, lv_color_hex(0x070604), LV_OPA_90);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_border_color(top, amber(), 0);
    lv_obj_set_style_border_width(top, 1, 0);

    label(top, "PIONEER OEL-90", &lv_font_montserrat_24, amber_hot(), LV_ALIGN_LEFT_MID, 10, 0);
    label(top, assets::project_name(), &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_RIGHT_MID, -10, 0);

    create_oel_display(screen);

    lv_obj_t *button_deck = block(screen, lv_disp_get_hor_res(nullptr) - 24, 112, lv_color_hex(0x050403), LV_OPA_60);
    lv_obj_align(button_deck, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_set_style_border_color(button_deck, amber(), 0);
    lv_obj_set_style_border_width(button_deck, 1, 0);

    home_button(button_deck, LV_SYMBOL_VIDEO " GIF", "MIDIA DO SD", LV_ALIGN_LEFT_MID, 10, 0, 1);
    home_button(button_deck, LV_SYMBOL_AUDIO " SPOT", "CONTROLE BLE", LV_ALIGN_CENTER, 0, 0, 2);
    home_button(button_deck, LV_SYMBOL_SETTINGS " SET", "TEMA / SD", LV_ALIGN_RIGHT_MID, -10, 0, 3);

    create_status_strip(screen);
    navigation::attach(screen);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace home
} // namespace ui

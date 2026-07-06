#include "home.h"

#include "assets.h"
#include "gif_player.h"
#include "sd_browser.h"
#include "theme.h"

#include "lvgl.h"

#include <string.h>

namespace ui {
namespace home {

static constexpr int kBarCount = 18;
static lv_obj_t *s_bars[kBarCount] = {};
static lv_timer_t *s_meter_timer = nullptr;
static uint8_t s_phase = 0;

static void cleanup_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if(s_meter_timer != nullptr) {
        lv_timer_del(s_meter_timer);
        s_meter_timer = nullptr;
    }
    memset(s_bars, 0, sizeof(s_bars));
}

static void meter_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    s_phase++;
    for(int i = 0; i < kBarCount; ++i) {
        if(s_bars[i] == nullptr) {
            continue;
        }
        uint8_t wave = static_cast<uint8_t>((s_phase * 17 + i * 29 + ((i % 3) * s_phase)) % 96);
        lv_coord_t height = 18 + (wave % 94);
        lv_obj_set_size(s_bars[i], 5, height);
        lv_obj_set_y(s_bars[i], 184 - height);
        lv_obj_set_style_bg_color(s_bars[i], (i % 5 == 0) ? theme::colors().purple : theme::colors().cyan, 0);
    }
}

static void open_gif_player_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        gif_player::show_list();
    }
}

static lv_obj_t *create_menu_button(lv_obj_t *parent, const char *text, bool enabled, lv_event_cb_t cb)
{
    lv_obj_t *button = lv_btn_create(parent);
    theme::apply_button(button, enabled);
    lv_obj_set_size(button, LV_PCT(100), 42);
    if(cb != nullptr && enabled) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_center(label);
    return button;
}

static void create_meter_panel(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "FAKE VU METER");
    theme::apply_muted_text(title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 6);

    lv_obj_t *status = lv_label_create(parent);
    lv_label_set_text(status, sd_browser::is_mounted() ? "SD: ONLINE" : "SD: OFFLINE");
    theme::apply_muted_text(status);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -6, 6);

    for(int i = 0; i < kBarCount; ++i) {
        lv_obj_t *bar = lv_obj_create(parent);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 5, 24);
        lv_obj_set_pos(bar, 8 + (i * 8), 160);
        lv_obj_set_style_bg_color(bar, theme::colors().cyan, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_90, 0);
        lv_obj_set_style_shadow_color(bar, theme::colors().cyan, 0);
        lv_obj_set_style_shadow_width(bar, 5, 0);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_40, 0);
        s_bars[i] = bar;
    }

    lv_obj_t *cursor = lv_label_create(parent);
    lv_label_set_text(cursor, "_");
    lv_obj_set_style_text_font(cursor, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(cursor, theme::colors().purple, 0);
    lv_obj_align(cursor, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    theme::pulse_opacity(cursor, LV_OPA_10, LV_OPA_COVER, 360);

    s_meter_timer = lv_timer_create(meter_timer_cb, 75, nullptr);
    meter_timer_cb(s_meter_timer);
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    lv_obj_add_event_cb(screen, cleanup_cb, LV_EVENT_DELETE, nullptr);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 24, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "JC3248W535EN CYBER DECK");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *mode = lv_label_create(header);
    lv_label_set_text(mode, "OEL-90");
    theme::apply_muted_text(mode);
    lv_obj_align(mode, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *menu = theme::create_panel(screen);
    lv_obj_set_size(menu, 284, 246);
    lv_obj_align(menu, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(menu, 7, 0);

    create_menu_button(menu, LV_SYMBOL_VIDEO "  GIF PLAYER", true, open_gif_player_cb);
    create_menu_button(menu, "SPOTIFY  /  COMING SOON", false, nullptr);
    create_menu_button(menu, "MP3  /  COMING SOON", false, nullptr);
    create_menu_button(menu, "CONFIGURACOES  /  COMING SOON", false, nullptr);
    create_menu_button(menu, "SOBRE  /  COMING SOON", false, nullptr);

    lv_obj_t *meter = theme::create_panel(screen);
    lv_obj_set_size(meter, 168, 246);
    lv_obj_align(meter, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    create_meter_panel(meter);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "PIONEER OEL / WINAMP / XPLOD STYLE");
    theme::apply_muted_text(footer);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -1);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace home
} // namespace ui

#include "spotify_ui.h"
#include "spotify_ble.h"
#include "navigation.h"
#include "theme.h"
#include "home.h"
#include "lvgl.h"

#include <string.h>

namespace ui {
namespace spotify {

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_decor_layer = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_img_cover = nullptr;
static lv_obj_t *s_info_panel = nullptr;
static lv_obj_t *s_vu_layer = nullptr;
static lv_obj_t *s_stage_layer = nullptr;
static lv_obj_t *s_stage_cover = nullptr;
static lv_obj_t *s_stage_shadow = nullptr;
static lv_obj_t *s_stage_title = nullptr;
static lv_obj_t *s_stage_artist = nullptr;

static lv_obj_t *s_title_lbl = nullptr;
static lv_obj_t *s_artist_lbl = nullptr;
static lv_obj_t *s_status_lbl = nullptr;

static lv_obj_t *s_btn_play = nullptr;
static lv_obj_t *s_btn_play_lbl = nullptr;

static lv_timer_t *s_update_timer = nullptr;

enum class ViewMode {
    Normal = 0,
    CoverOnly,
    CoverVu,
    SideVu,
};

static ViewMode s_view_mode = ViewMode::Normal;
static constexpr int kVuBarCount = 22;
static lv_obj_t *s_vu_bars[kVuBarCount] = {};
static uint8_t s_vu_phase = 0;

// Custom image descriptor pointing to the BLE raw RGB565 cover buffer
static lv_img_dsc_t s_cover_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = SPOTIFY_COVER_SIZE,
        .h = SPOTIFY_COVER_SIZE
    },
    .data_size = SPOTIFY_COVER_SIZE * SPOTIFY_COVER_SIZE * 2,
    .data = nullptr
};

// State variables to prevent redraw flicker
static char s_last_title[128] = "";
static char s_last_artist[128] = "";
static bool s_last_connected = false;

static void cleanup_cb(lv_event_t *event)
{
    if (s_update_timer != nullptr) {
        lv_timer_del(s_update_timer);
        s_update_timer = nullptr;
    }
    s_screen = nullptr;
    s_decor_layer = nullptr;
    s_header = nullptr;
    s_img_cover = nullptr;
    s_info_panel = nullptr;
    s_vu_layer = nullptr;
    s_stage_layer = nullptr;
    s_stage_cover = nullptr;
    s_stage_shadow = nullptr;
    s_stage_title = nullptr;
    s_stage_artist = nullptr;
    s_title_lbl = nullptr;
    s_artist_lbl = nullptr;
    s_status_lbl = nullptr;
    s_btn_play = nullptr;
    s_btn_play_lbl = nullptr;
    s_view_mode = ViewMode::Normal;
    memset(s_vu_bars, 0, sizeof(s_vu_bars));
    s_vu_phase = 0;
    memset(s_last_title, 0, sizeof(s_last_title));
    memset(s_last_artist, 0, sizeof(s_last_artist));
    s_last_connected = false;
}

static void set_cover_chrome(bool enabled)
{
    if (s_img_cover == nullptr) return;

    if (enabled) {
        lv_obj_set_style_border_color(s_img_cover, theme::colors().cyan, 0);
        lv_obj_set_style_border_width(s_img_cover, 2, 0);
        lv_obj_set_style_border_opa(s_img_cover, LV_OPA_40, 0);
        lv_obj_set_style_shadow_color(s_img_cover, theme::colors().cyan, 0);
        lv_obj_set_style_shadow_width(s_img_cover, 8, 0);
        lv_obj_set_style_shadow_opa(s_img_cover, LV_OPA_20, 0);
    } else {
        lv_obj_set_style_border_width(s_img_cover, 0, 0);
        lv_obj_set_style_border_opa(s_img_cover, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(s_img_cover, 0, 0);
        lv_obj_set_style_shadow_opa(s_img_cover, LV_OPA_TRANSP, 0);
    }
}

static void configure_stage_cover(lv_align_t align, lv_coord_t x, lv_coord_t y, uint16_t zoom,
                                  bool shadow, bool show_text)
{
    if (s_stage_layer == nullptr || s_stage_cover == nullptr || s_stage_shadow == nullptr) {
        return;
    }

    lv_obj_clear_flag(s_stage_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_stage_cover, align, x, y);
    lv_img_set_zoom(s_stage_cover, zoom);
    lv_obj_move_foreground(s_stage_cover);

    if (shadow) {
        lv_obj_clear_flag(s_stage_shadow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_stage_shadow, 300, 300);
        lv_obj_align_to(s_stage_shadow, s_stage_cover, LV_ALIGN_CENTER, 12, 14);
        lv_obj_set_style_bg_opa(s_stage_shadow,
                                s_view_mode == ViewMode::CoverVu ? LV_OPA_80 : LV_OPA_60,
                                0);
        lv_obj_set_style_shadow_width(s_stage_shadow,
                                      s_view_mode == ViewMode::CoverVu ? 40 : 26,
                                      0);
        lv_obj_set_style_shadow_opa(s_stage_shadow,
                                    s_view_mode == ViewMode::CoverVu ? LV_OPA_COVER : LV_OPA_80,
                                    0);
        lv_obj_move_background(s_stage_shadow);
        lv_obj_move_foreground(s_stage_cover);
    } else {
        lv_obj_add_flag(s_stage_shadow, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_stage_title != nullptr && s_stage_artist != nullptr) {
        if (show_text) {
            lv_obj_clear_flag(s_stage_title, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_stage_artist, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_stage_title, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_stage_artist, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void hide_stage_layer(void)
{
    if (s_stage_layer != nullptr) {
        lv_obj_add_flag(s_stage_layer, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_vu_overlay(void)
{
    if (s_vu_layer == nullptr ||
        (s_view_mode != ViewMode::CoverVu && s_view_mode != ViewMode::SideVu)) {
        return;
    }

    s_vu_phase += 5;
    const bool side_mode = s_view_mode == ViewMode::SideVu;
    const lv_coord_t screen_h = lv_disp_get_ver_res(nullptr);
    const lv_coord_t base_y = side_mode ? 220 : screen_h - 18;
    const lv_coord_t max_h = side_mode ? 152 : 104;
    const lv_coord_t start_x = side_mode ? 292 : 12;
    const lv_coord_t step = side_mode ? 8 : 21;
    const lv_coord_t bar_w = side_mode ? 5 : 14;

    for (int i = 0; i < kVuBarCount; ++i) {
        if (s_vu_bars[i] == nullptr) continue;

        uint8_t wave = static_cast<uint8_t>((s_vu_phase + i * 23 + ((i % 4) * s_vu_phase / 3)) % 110);
        lv_coord_t h = 16 + (wave % max_h);
        lv_obj_set_width(s_vu_bars[i], bar_w);
        lv_obj_set_height(s_vu_bars[i], h);
        lv_obj_set_x(s_vu_bars[i], start_x + (i * step));
        lv_obj_set_y(s_vu_bars[i], base_y - h);
        lv_obj_set_style_bg_color(s_vu_bars[i],
                                  (i % 5 == 0) ? theme::colors().purple : theme::colors().cyan,
                                  0);
    }
}

static void update_fullscreen_layout(void)
{
    if (s_img_cover == nullptr) return;

    const bool fullscreen = s_view_mode != ViewMode::Normal;

    if (fullscreen) {
        // Hide UI elements
        if (s_header != nullptr) lv_obj_add_flag(s_header, LV_OBJ_FLAG_HIDDEN);
        if (s_info_panel != nullptr) lv_obj_add_flag(s_info_panel, LV_OBJ_FLAG_HIDDEN);
        if (s_decor_layer != nullptr) lv_obj_add_flag(s_decor_layer, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(s_img_cover, LV_OBJ_FLAG_HIDDEN);
        set_cover_chrome(false);

        if (s_vu_layer != nullptr) {
            if (s_view_mode == ViewMode::CoverVu || s_view_mode == ViewMode::SideVu) {
                lv_obj_clear_flag(s_vu_layer, LV_OBJ_FLAG_HIDDEN);
                if (s_view_mode == ViewMode::CoverVu) {
                    lv_obj_move_foreground(s_vu_layer);
                }
            } else {
                lv_obj_add_flag(s_vu_layer, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (s_view_mode == ViewMode::CoverOnly) {
            configure_stage_cover(LV_ALIGN_CENTER, 0, 0, 410, false, false);
        } else if (s_view_mode == ViewMode::CoverVu) {
            configure_stage_cover(LV_ALIGN_CENTER, 0, -18, 245, true, false);
        } else {
            configure_stage_cover(LV_ALIGN_LEFT_MID, 18, -8, 128, true, true);
            if (s_vu_layer != nullptr) {
                lv_obj_move_background(s_vu_layer);
                if (s_stage_layer != nullptr) lv_obj_move_foreground(s_stage_layer);
            }
        }
        update_vu_overlay();
    } else {
        // Show UI elements
        if (s_header != nullptr) lv_obj_clear_flag(s_header, LV_OBJ_FLAG_HIDDEN);
        if (s_info_panel != nullptr) lv_obj_clear_flag(s_info_panel, LV_OBJ_FLAG_HIDDEN);
        if (s_decor_layer != nullptr) lv_obj_clear_flag(s_decor_layer, LV_OBJ_FLAG_HIDDEN);
        if (s_vu_layer != nullptr) lv_obj_add_flag(s_vu_layer, LV_OBJ_FLAG_HIDDEN);
        hide_stage_layer();

        // The demo music layout keeps metadata and controls on the left.
        lv_obj_clear_flag(s_img_cover, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_img_cover, LV_ALIGN_RIGHT_MID, -16, 20);
        lv_img_set_zoom(s_img_cover, 205);
        set_cover_chrome(true);
    }
}

static void cover_click_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        if (s_view_mode == ViewMode::Normal) {
            s_view_mode = ViewMode::CoverOnly;
        } else if (s_view_mode == ViewMode::CoverOnly) {
            s_view_mode = ViewMode::CoverVu;
        } else if (s_view_mode == ViewMode::CoverVu) {
            s_view_mode = ViewMode::SideVu;
        } else {
            s_view_mode = ViewMode::Normal;
        }
        update_fullscreen_layout();
    }
}

static void create_vu_overlay(lv_obj_t *parent)
{
    s_vu_layer = lv_obj_create(parent);
    lv_obj_remove_style_all(s_vu_layer);
    lv_obj_set_size(s_vu_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_vu_layer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_vu_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_vu_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_vu_layer, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < kVuBarCount; ++i) {
        lv_obj_t *bar = lv_obj_create(s_vu_layer);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 8, 18);
        lv_obj_set_pos(bar, 58 + (i * 16), lv_disp_get_ver_res(nullptr) - 36);
        lv_obj_set_style_bg_color(bar, theme::colors().cyan, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
        lv_obj_set_style_shadow_color(bar, theme::colors().cyan, 0);
        lv_obj_set_style_shadow_width(bar, 8, 0);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_60, 0);
        s_vu_bars[i] = bar;
    }
}

static void create_stage_layer(lv_obj_t *parent)
{
    s_stage_layer = lv_obj_create(parent);
    lv_obj_remove_style_all(s_stage_layer);
    lv_obj_set_size(s_stage_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_stage_layer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_stage_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_stage_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_stage_layer, LV_OBJ_FLAG_HIDDEN);

    s_stage_shadow = lv_obj_create(s_stage_layer);
    lv_obj_remove_style_all(s_stage_shadow);
    lv_obj_set_size(s_stage_shadow, 300, 300);
    lv_obj_set_style_bg_color(s_stage_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_stage_shadow, LV_OPA_80, 0);
    lv_obj_set_style_shadow_color(s_stage_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(s_stage_shadow, 40, 0);
    lv_obj_set_style_shadow_opa(s_stage_shadow, LV_OPA_COVER, 0);

    s_stage_cover = lv_img_create(s_stage_layer);
    lv_img_set_src(s_stage_cover, &s_cover_dsc);
    lv_obj_add_flag(s_stage_cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_stage_cover, cover_click_cb, LV_EVENT_CLICKED, nullptr);

    s_stage_title = lv_label_create(s_stage_layer);
    lv_label_set_text(s_stage_title, "Sem Musica");
    lv_label_set_long_mode(s_stage_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_stage_title, 238);
    lv_obj_set_style_text_font(s_stage_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_stage_title, theme::colors().text, 0);
    lv_obj_align(s_stage_title, LV_ALIGN_BOTTOM_LEFT, 18, -58);

    s_stage_artist = lv_label_create(s_stage_layer);
    lv_label_set_text(s_stage_artist, "Desconectado");
    lv_label_set_long_mode(s_stage_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_stage_artist, 238);
    lv_obj_set_style_text_font(s_stage_artist, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_stage_artist, theme::colors().muted, 0);
    lv_obj_align(s_stage_artist, LV_ALIGN_BOTTOM_LEFT, 18, -32);
}

static void control_click_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    
    uintptr_t cmd = (uintptr_t)lv_event_get_user_data(event);
    if (cmd == 1) {
        spotify_ble_send_next();
    } else if (cmd == 2) {
        spotify_ble_send_prev();
    } else if (cmd == 3) {
        spotify_ble_send_play_pause();
    } else if (cmd == 4) {
        // Return to Home
        home::show();
    }
}

static void update_timer_cb(lv_timer_t *timer)
{
    bool connected = spotify_ble_is_connected();
    
    // Status update
    if (connected != s_last_connected) {
        s_last_connected = connected;
        if (connected) {
            lv_label_set_text(s_status_lbl, "STATUS: CONECTADO");
            lv_obj_set_style_text_color(s_status_lbl, theme::colors().cyan, 0);
        } else {
            lv_label_set_text(s_status_lbl, "STATUS: DESCONECTADO");
            lv_obj_set_style_text_color(s_status_lbl, theme::colors().purple, 0);
        }
    }

    // Title / Artist update
    char title[128];
    char artist[128];
    spotify_ble_get_title(title, sizeof(title));
    spotify_ble_get_artist(artist, sizeof(artist));

    if (strcmp(title, s_last_title) != 0) {
        strcpy(s_last_title, title);
        lv_label_set_text(s_title_lbl, title);
        if (s_stage_title != nullptr) lv_label_set_text(s_stage_title, title);
    }

    if (strcmp(artist, s_last_artist) != 0) {
        strcpy(s_last_artist, artist);
        lv_label_set_text(s_artist_lbl, artist);
        if (s_stage_artist != nullptr) lv_label_set_text(s_stage_artist, artist);
    }

    // Cover image update
    if (spotify_ble_has_new_cover()) {
        spotify_ble_clear_new_cover_flag();
        // Invalidate image cache and redraw
        lv_img_cache_invalidate_src(&s_cover_dsc);
        lv_obj_invalidate(s_img_cover);
        if (s_stage_cover != nullptr) {
            lv_obj_invalidate(s_stage_cover);
        }
    }

    update_vu_overlay();
}

void show(void)
{
    theme::init();

    s_screen = lv_obj_create(nullptr);
    theme::apply_screen(s_screen);
    lv_obj_add_event_cb(s_screen, cleanup_cb, LV_EVENT_DELETE, nullptr);

    s_decor_layer = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_decor_layer);
    lv_obj_set_size(s_decor_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_decor_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_decor_layer, LV_OBJ_FLAG_SCROLLABLE);
    theme::add_frame_ticks(s_decor_layer);
    theme::add_scanlines(s_decor_layer, LV_OPA_10);

    // --- Header ---
    s_header = theme::create_panel(s_screen);
    lv_obj_set_size(s_header, lv_disp_get_hor_res(nullptr) - 24, 44);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *title = lv_label_create(s_header);
    lv_label_set_text(title, "SPOTIFY PLAYER");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *mode = lv_label_create(s_header);
    lv_label_set_text(mode, "BLUETOOTH BLE");
    theme::apply_muted_text(mode);
    lv_obj_align(mode, LV_ALIGN_RIGHT_MID, -8, 0);

    // Set cover raw data pointer
    s_cover_dsc.data = spotify_ble_get_cover_data();
    create_stage_layer(s_screen);

    // --- Right-side Cover Art ---
    s_img_cover = lv_img_create(s_screen);
    lv_img_set_src(s_img_cover, &s_cover_dsc);
    lv_obj_align(s_img_cover, LV_ALIGN_RIGHT_MID, -16, 20);
    lv_img_set_zoom(s_img_cover, 205); // Zoom to 240x240
    lv_obj_add_flag(s_img_cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_img_cover, cover_click_cb, LV_EVENT_CLICKED, nullptr);

    // Styled border around cover art
    set_cover_chrome(true);
    create_vu_overlay(s_screen);

    // --- Left-side Metadata and Controls ---
    s_info_panel = theme::create_panel(s_screen);
    lv_obj_set_size(s_info_panel, 192, 246);
    lv_obj_align(s_info_panel, LV_ALIGN_BOTTOM_LEFT, 12, -12);

    // Song Title
    s_title_lbl = lv_label_create(s_info_panel);
    lv_label_set_text(s_title_lbl, "Sem Musica");
    lv_label_set_long_mode(s_title_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_title_lbl, 176);
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_title_lbl, theme::colors().text, 0);
    lv_obj_align(s_title_lbl, LV_ALIGN_TOP_LEFT, 8, 12);

    // Artist
    s_artist_lbl = lv_label_create(s_info_panel);
    lv_label_set_text(s_artist_lbl, "Desconectado");
    lv_label_set_long_mode(s_artist_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_artist_lbl, 176);
    theme::apply_muted_text(s_artist_lbl);
    lv_obj_align(s_artist_lbl, LV_ALIGN_TOP_LEFT, 8, 38);

    // Connection Status
    s_status_lbl = lv_label_create(s_info_panel);
    lv_label_set_text(s_status_lbl, "STATUS: DESCONECTADO");
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_status_lbl, theme::colors().purple, 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_TOP_LEFT, 8, 64);

    // Pulse connection status indicator
    theme::pulse_opacity(s_status_lbl, LV_OPA_30, LV_OPA_COVER, 800);

    // Controls Buttons Row
    lv_obj_t *btn_row = lv_obj_create(s_info_panel);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, 176, 50);
    lv_obj_align(btn_row, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 8, 0);

    // Prev Button
    lv_obj_t *btn_prev = lv_btn_create(btn_row);
    theme::apply_subtle_button(btn_prev);
    lv_obj_set_size(btn_prev, 50, 40);
    lv_obj_add_event_cb(btn_prev, control_click_cb, LV_EVENT_CLICKED, (void *)2); // 2 = Prev
    lv_obj_t *prev_lbl = lv_label_create(btn_prev);
    lv_label_set_text(prev_lbl, LV_SYMBOL_PREV);
    lv_obj_center(prev_lbl);

    // Play/Pause Button
    s_btn_play = lv_btn_create(btn_row);
    theme::apply_button(s_btn_play, true);
    lv_obj_set_size(s_btn_play, 60, 40);
    lv_obj_add_event_cb(s_btn_play, control_click_cb, LV_EVENT_CLICKED, (void *)3); // 3 = Play/Pause
    s_btn_play_lbl = lv_label_create(s_btn_play);
    lv_label_set_text(s_btn_play_lbl, LV_SYMBOL_PLAY "/" LV_SYMBOL_PAUSE);
    lv_obj_center(s_btn_play_lbl);

    // Next Button
    lv_obj_t *btn_next = lv_btn_create(btn_row);
    theme::apply_subtle_button(btn_next);
    lv_obj_set_size(btn_next, 50, 40);
    lv_obj_add_event_cb(btn_next, control_click_cb, LV_EVENT_CLICKED, (void *)1); // 1 = Next
    lv_obj_t *next_lbl = lv_label_create(btn_next);
    lv_label_set_text(next_lbl, LV_SYMBOL_NEXT);
    lv_obj_center(next_lbl);

    // Back to Home Button
    lv_obj_t *btn_back = lv_btn_create(s_info_panel);
    theme::apply_subtle_button(btn_back);
    lv_obj_set_size(btn_back, 176, 42);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(btn_back, control_click_cb, LV_EVENT_CLICKED, (void *)4); // 4 = Back

    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_HOME "  VOLTAR");
    lv_obj_center(back_lbl);

    lv_obj_t *btn_mode = lv_btn_create(s_info_panel);
    theme::apply_subtle_button(btn_mode);
    lv_obj_set_size(btn_mode, 176, 32);
    lv_obj_align(btn_mode, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(btn_mode, cover_click_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *mode_lbl = lv_label_create(btn_mode);
    lv_label_set_text(mode_lbl, LV_SYMBOL_EYE_OPEN "  MODO DA CAPA");
    lv_obj_center(mode_lbl);

    // Initialize layout positions
    update_fullscreen_layout();

    // Start background update timer
    s_update_timer = lv_timer_create(update_timer_cb, 200, nullptr);
    update_timer_cb(s_update_timer); // Execute first run immediately

    navigation::attach(s_screen);
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace spotify
} // namespace ui

extern "C" void ui_spotify_show(void)
{
    ui::spotify::show();
}

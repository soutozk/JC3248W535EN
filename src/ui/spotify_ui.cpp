#include "spotify_ui.h"
#include "spotify_ble.h"
#include "theme.h"
#include "home.h"
#include "lvgl.h"

#include <string.h>

namespace ui {
namespace spotify {

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_img_cover = nullptr;
static lv_obj_t *s_info_panel = nullptr;

static lv_obj_t *s_title_lbl = nullptr;
static lv_obj_t *s_artist_lbl = nullptr;
static lv_obj_t *s_status_lbl = nullptr;

static lv_obj_t *s_btn_play = nullptr;
static lv_obj_t *s_btn_play_lbl = nullptr;

static lv_timer_t *s_update_timer = nullptr;
static bool s_fullscreen = false;

// Custom image descriptor pointing to the BLE raw RGB565 cover buffer
static lv_img_dsc_t s_cover_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
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
    s_header = nullptr;
    s_img_cover = nullptr;
    s_info_panel = nullptr;
    s_title_lbl = nullptr;
    s_artist_lbl = nullptr;
    s_status_lbl = nullptr;
    s_btn_play = nullptr;
    s_btn_play_lbl = nullptr;
    s_fullscreen = false;
    memset(s_last_title, 0, sizeof(s_last_title));
    memset(s_last_artist, 0, sizeof(s_last_artist));
    s_last_connected = false;
}

static void update_fullscreen_layout(void)
{
    if (s_img_cover == nullptr) return;

    if (s_fullscreen) {
        // Hide UI elements
        if (s_header != nullptr) lv_obj_add_flag(s_header, LV_OBJ_FLAG_HIDDEN);
        if (s_info_panel != nullptr) lv_obj_add_flag(s_info_panel, LV_OBJ_FLAG_HIDDEN);

        // Adjust cover art to fullscreen mode (300x300, zoom 256, centered)
        lv_obj_align(s_img_cover, LV_ALIGN_CENTER, 0, 0);
        lv_img_set_zoom(s_img_cover, 256);
        
        // Remove frame ticks in fullscreen for total clean screen
        // (Just reposition or keep style simple)
    } else {
        // Show UI elements
        if (s_header != nullptr) lv_obj_clear_flag(s_header, LV_OBJ_FLAG_HIDDEN);
        if (s_info_panel != nullptr) lv_obj_clear_flag(s_info_panel, LV_OBJ_FLAG_HIDDEN);

        // Adjust cover art to standard mode (240x240, zoom 205, left aligned)
        lv_obj_align(s_img_cover, LV_ALIGN_LEFT_MID, 16, 20);
        lv_img_set_zoom(s_img_cover, 205);
    }
}

static void cover_click_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        s_fullscreen = !s_fullscreen;
        update_fullscreen_layout();
    }
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
    }

    if (strcmp(artist, s_last_artist) != 0) {
        strcpy(s_last_artist, artist);
        lv_label_set_text(s_artist_lbl, artist);
    }

    // Cover image update
    if (spotify_ble_has_new_cover()) {
        spotify_ble_clear_new_cover_flag();
        // Invalidate image cache and redraw
        lv_img_cache_invalidate_src(&s_cover_dsc);
        lv_obj_invalidate(s_img_cover);
    }
}

void show(void)
{
    theme::init();

    s_screen = lv_obj_create(nullptr);
    theme::apply_screen(s_screen);
    lv_obj_add_event_cb(s_screen, cleanup_cb, LV_EVENT_DELETE, nullptr);
    theme::add_frame_ticks(s_screen);
    theme::add_scanlines(s_screen, LV_OPA_10);

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

    // --- Left-side Cover Art ---
    s_img_cover = lv_img_create(s_screen);
    lv_img_set_src(s_img_cover, &s_cover_dsc);
    // Align to the left
    lv_obj_align(s_img_cover, LV_ALIGN_LEFT_MID, 16, 20);
    lv_img_set_zoom(s_img_cover, 205); // Zoom to 240x240
    lv_obj_add_flag(s_img_cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_img_cover, cover_click_cb, LV_EVENT_CLICKED, nullptr);

    // Styled border around cover art
    lv_obj_set_style_border_color(s_img_cover, theme::colors().cyan, 0);
    lv_obj_set_style_border_width(s_img_cover, 2, 0);
    lv_obj_set_style_border_opa(s_img_cover, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(s_img_cover, theme::colors().cyan, 0);
    lv_obj_set_style_shadow_width(s_img_cover, 8, 0);
    lv_obj_set_style_shadow_opa(s_img_cover, LV_OPA_20, 0);

    // --- Right-side Controls Panel ---
    s_info_panel = theme::create_panel(s_screen);
    lv_obj_set_size(s_info_panel, 192, 246);
    lv_obj_align(s_info_panel, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

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

    // Initialize layout positions
    update_fullscreen_layout();

    // Start background update timer
    s_update_timer = lv_timer_create(update_timer_cb, 200, nullptr);
    update_timer_cb(s_update_timer); // Execute first run immediately

    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace spotify
} // namespace ui

extern "C" void ui_spotify_show(void)
{
    ui::spotify::show();
}

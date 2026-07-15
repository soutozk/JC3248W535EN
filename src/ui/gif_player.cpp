#include "gif_player.h"

#include "home.h"
#include "navigation.h"
#include "sd_browser.h"
#include "theme.h"

#include "lvgl.h"
#include "src/extra/libs/gif/lv_gif.h"

#include <string.h>

namespace ui {
namespace gif_player {

static sd_browser::MediaCatalog s_catalog = {};
static sd_browser::MediaFile s_current_file = {};
static lv_timer_t *s_placeholder_timer = nullptr;
static lv_obj_t *s_placeholder_bars[16] = {};
static uint8_t s_placeholder_phase = 0;

static void back_to_home_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        home::show();
    }
}

static void back_to_list_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        show_list();
    }
}

static void gif_ready_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_READY) {
        lv_gif_restart(lv_event_get_target(event));
    }
}

static void placeholder_cleanup_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if(s_placeholder_timer != nullptr) {
        lv_timer_del(s_placeholder_timer);
        s_placeholder_timer = nullptr;
    }
    memset(s_placeholder_bars, 0, sizeof(s_placeholder_bars));
}

static void placeholder_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    s_placeholder_phase++;
    for(size_t i = 0; i < 16; ++i) {
        if(s_placeholder_bars[i] == nullptr) {
            continue;
        }
        lv_coord_t height = 18 + ((s_placeholder_phase * 11 + i * 23) % 128);
        lv_obj_set_size(s_placeholder_bars[i], 14, height);
        lv_obj_align(s_placeholder_bars[i], LV_ALIGN_BOTTOM_MID, static_cast<lv_coord_t>((static_cast<int>(i) - 8) * 18), -38);
    }
}

static lv_obj_t *create_overlay_button(lv_obj_t *screen, lv_event_cb_t cb)
{
    lv_obj_t *button = lv_btn_create(screen);
    theme::apply_subtle_button(button);
    lv_obj_set_size(button, 46, 34);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(label, theme::font_icon(), 0);
    lv_obj_center(label);
    return button;
}

static void create_filename_label(lv_obj_t *screen, const sd_browser::MediaFile *file)
{
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, file->name);
    theme::apply_muted_text(label);
    lv_obj_set_style_bg_color(label, theme::colors().background, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_50, 0);
    lv_obj_set_style_pad_left(label, 6, 0);
    lv_obj_set_style_pad_right(label, 6, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 8, -8);
}

static void show_unsupported_player(const sd_browser::MediaFile *file)
{
    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    lv_obj_add_event_cb(screen, placeholder_cleanup_cb, LV_EVENT_DELETE, nullptr);

    for(size_t i = 0; i < 16; ++i) {
        lv_obj_t *bar = lv_obj_create(screen);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 14, 40);
        lv_obj_set_style_bg_color(bar, (i % 4 == 0) ? theme::colors().purple : theme::colors().cyan, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
        lv_obj_set_style_shadow_color(bar, theme::colors().cyan, 0);
        lv_obj_set_style_shadow_width(bar, 8, 0);
        lv_obj_set_style_shadow_opa(bar, LV_OPA_30, 0);
        s_placeholder_bars[i] = bar;
    }

    lv_obj_t *notice = lv_label_create(screen);
    lv_label_set_text_fmt(notice, "%s DECODER FUTURO", sd_browser::media_type_label(file->type));
    theme::apply_title(notice);
    lv_obj_center(notice);

    create_overlay_button(screen, back_to_list_cb);
    create_filename_label(screen, file);
    navigation::attach(screen);

    s_placeholder_timer = lv_timer_create(placeholder_timer_cb, 80, nullptr);
    placeholder_timer_cb(s_placeholder_timer);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
}

static void show_gif_player(const sd_browser::MediaFile *file)
{
    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);

    lv_obj_t *gif = lv_gif_create(screen);
    lv_obj_add_event_cb(gif, gif_ready_cb, LV_EVENT_READY, nullptr);
    lv_gif_set_src(gif, file->lvgl_path);
    lv_obj_center(gif);

    create_overlay_button(screen, back_to_list_cb);
    create_filename_label(screen, file);
    navigation::attach(screen);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
}

static void open_media_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const sd_browser::MediaFile *file = static_cast<const sd_browser::MediaFile *>(lv_event_get_user_data(event));
    if(file == nullptr) {
        return;
    }

    s_current_file = *file;
    if(sd_browser::media_type_playable(s_current_file.type)) {
        show_gif_player(&s_current_file);
    } else {
        show_unsupported_player(&s_current_file);
    }
}

static void refresh_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        show_list();
    }
}

static lv_obj_t *create_row(lv_obj_t *parent, sd_browser::MediaFile *file)
{
    lv_obj_t *row = lv_btn_create(parent);
    theme::apply_subtle_button(row);
    lv_obj_set_size(row, LV_PCT(100), 42);
    lv_obj_add_event_cb(row, open_media_cb, LV_EVENT_CLICKED, file);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, file->name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 250);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *type = lv_label_create(row);
    lv_label_set_text(type, sd_browser::media_type_label(file->type));
    theme::apply_muted_text(type);
    lv_obj_align(type, LV_ALIGN_RIGHT_MID, -8, 0);
    return row;
}

void show_list()
{
    theme::init();
    sd_browser::scan_media(&s_catalog);

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 24, 46);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back = lv_btn_create(header);
    theme::apply_subtle_button(back);
    lv_obj_set_size(back, 42, 30);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, back_to_home_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "GIF PLAYER");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 54, 0);

    lv_obj_t *refresh = lv_btn_create(header);
    theme::apply_subtle_button(refresh);
    lv_obj_set_size(refresh, 42, 30);
    lv_obj_align(refresh, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(refresh, refresh_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *refresh_label = lv_label_create(refresh);
    lv_label_set_text(refresh_label, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(refresh_label, theme::font_icon(), 0);
    lv_obj_center(refresh_label);

    lv_obj_t *panel = theme::create_panel(screen);
    lv_obj_set_size(panel, lv_disp_get_hor_res(nullptr) - 24, 246);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 7, 0);

    if(!s_catalog.sd_ready) {
        lv_obj_t *empty = lv_label_create(panel);
        lv_label_set_text(empty, "SD OFFLINE");
        theme::apply_title(empty);
        lv_obj_center(empty);
    } else if(!s_catalog.directory_found) {
        lv_obj_t *empty = lv_label_create(panel);
        lv_label_set_text(empty, "PASTA /sd/gifs NAO ENCONTRADA");
        theme::apply_title(empty);
        lv_obj_center(empty);
    } else if(s_catalog.count == 0) {
        lv_obj_t *empty = lv_label_create(panel);
        lv_label_set_text(empty, "SEM GIFS EM /sd/gifs");
        theme::apply_title(empty);
        lv_obj_center(empty);
    } else {
        for(size_t i = 0; i < s_catalog.count; ++i) {
            create_row(panel, &s_catalog.files[i]);
        }
    }

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, true);
}

} // namespace gif_player
} // namespace ui

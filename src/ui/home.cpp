#include "home.h"

#include "assets.h"
#include "gif_player.h"
#include "spotify_ui.h"
#include "sd_browser.h"
#include "theme.h"

#include "lvgl.h"

#include <stddef.h>
#include <stdio.h>

namespace ui {
namespace home {

void show();

static void cleanup_cb(lv_event_t *event)
{
    LV_UNUSED(event);
}

static void cycle_theme_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        theme::cycle_palette();
        show();
    }
}

static void refresh_sd_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        sd_browser::mount();
        show();
    }
}

static void navigator_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *matrix = lv_event_get_target(event);
    uint16_t id = lv_btnmatrix_get_selected_btn(matrix);
    switch(id) {
        case 0:
            gif_player::show_list();
            break;
        case 1:
            ui_spotify_show();
            break;
        case 2:
            theme::cycle_palette();
            show();
            break;
        case 3:
            sd_browser::mount();
            show();
            break;
        default:
            break;
    }
}

static void create_function_navigator(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "NAVEGADOR");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 6);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "FUNCOES DO DECK");
    theme::apply_muted_text(subtitle);
    lv_obj_align(subtitle, LV_ALIGN_TOP_RIGHT, -6, 11);

    static const char *map[] = {
        LV_SYMBOL_VIDEO " GIF",
        LV_SYMBOL_AUDIO " SPOTIFY",
        "\n",
        "TEMA",
        "SCAN SD",
        "",
    };

    lv_obj_t *matrix = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(matrix, map);
    lv_btnmatrix_set_btn_ctrl_all(matrix, LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_set_size(matrix, LV_PCT(100), 168);
    lv_obj_align(matrix, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_clear_flag(matrix, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(matrix, navigator_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_set_style_bg_color(matrix, theme::colors().panel_alt, 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_60, 0);
    lv_obj_set_style_border_color(matrix, theme::colors().cyan, 0);
    lv_obj_set_style_border_width(matrix, 1, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 6, 0);
    lv_obj_set_style_pad_row(matrix, 8, 0);
    lv_obj_set_style_pad_column(matrix, 8, 0);

    lv_obj_set_style_bg_color(matrix, theme::colors().blue, LV_PART_ITEMS);
    lv_obj_set_style_bg_grad_color(matrix, theme::colors().panel, LV_PART_ITEMS);
    lv_obj_set_style_bg_grad_dir(matrix, LV_GRAD_DIR_HOR, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_80, LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, theme::colors().cyan, LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, theme::colors().text, LV_PART_ITEMS);
    lv_obj_set_style_text_font(matrix, &lv_font_montserrat_16, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(matrix, theme::colors().cyan, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(matrix, theme::colors().background, LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_t *rail = lv_obj_create(parent);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, LV_PCT(100), 3);
    lv_obj_align(rail, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_set_style_bg_color(rail, theme::colors().purple, 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_80, 0);
    lv_obj_clear_flag(rail, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_status_panel(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "SISTEMA");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 6);

    lv_obj_t *status = lv_label_create(parent);
    lv_label_set_text(status, sd_browser::is_mounted() ? "SD: ONLINE" : "SD: OFFLINE");
    theme::apply_muted_text(status);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 8, 42);

    lv_obj_t *theme_label = lv_label_create(parent);
    char theme_text[48];
    snprintf(theme_text, sizeof(theme_text), "TEMA: %s", theme::palette_name());
    lv_label_set_text(theme_label, theme_text);
    theme::apply_text(theme_label);
    lv_obj_align(theme_label, LV_ALIGN_TOP_LEFT, 8, 70);

    lv_obj_t *swatch_row = lv_obj_create(parent);
    lv_obj_remove_style_all(swatch_row);
    lv_obj_set_size(swatch_row, 144, 28);
    lv_obj_align(swatch_row, LV_ALIGN_TOP_LEFT, 8, 104);
    lv_obj_set_flex_flow(swatch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(swatch_row, 8, 0);

    lv_color_t swatches[] = {
        theme::colors().cyan,
        theme::colors().blue,
        theme::colors().purple,
    };
    for(size_t i = 0; i < sizeof(swatches) / sizeof(swatches[0]); ++i) {
        lv_obj_t *swatch = lv_obj_create(swatch_row);
        lv_obj_remove_style_all(swatch);
        lv_obj_set_size(swatch, 28, 24);
        lv_obj_set_style_bg_color(swatch, swatches[i], 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(swatch, theme::colors().text, 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
    }

    lv_obj_t *theme_btn = lv_btn_create(parent);
    theme::apply_button(theme_btn, true);
    lv_obj_set_size(theme_btn, 144, 42);
    lv_obj_align(theme_btn, LV_ALIGN_BOTTOM_MID, 0, -58);
    lv_obj_add_event_cb(theme_btn, cycle_theme_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *theme_btn_lbl = lv_label_create(theme_btn);
    lv_label_set_text(theme_btn_lbl, "TROCAR TEMA");
    lv_obj_center(theme_btn_lbl);

    lv_obj_t *sd_btn = lv_btn_create(parent);
    theme::apply_subtle_button(sd_btn);
    lv_obj_set_size(sd_btn, 144, 36);
    lv_obj_align(sd_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(sd_btn, refresh_sd_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *sd_btn_lbl = lv_label_create(sd_btn);
    lv_label_set_text(sd_btn_lbl, "SCAN SD");
    lv_obj_center(sd_btn_lbl);
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

    lv_obj_t *navigator = theme::create_panel(screen);
    lv_obj_set_size(navigator, 284, 246);
    lv_obj_align(navigator, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    create_function_navigator(navigator);

    lv_obj_t *status_panel = theme::create_panel(screen);
    lv_obj_set_size(status_panel, 168, 246);
    lv_obj_align(status_panel, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    create_status_panel(status_panel);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "PIONEER OEL / THEMEABLE CYBERDECK");
    theme::apply_muted_text(footer);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -1);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 260, 0, true);
}

} // namespace home
} // namespace ui

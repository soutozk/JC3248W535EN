#include "navigation.h"

#include "gif_player.h"
#include "home.h"
#include "sd_browser.h"
#include "settings.h"
#include "spotify_ui.h"
#include "theme.h"

#include <stdint.h>

namespace ui {
namespace navigation {

enum class GestureEdge {
    Left,
    Right,
    Top,
    Bottom,
};

static lv_obj_t *s_overlay = nullptr;
static lv_obj_t *s_panel = nullptr;
static lv_point_t s_press_point = {};
static GestureEdge s_edge = GestureEdge::Left;
static bool s_pressed = false;

static constexpr lv_coord_t kEdgeSize = 34;
static constexpr lv_coord_t kSwipeThreshold = 58;

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
    lv_obj_align(obj, align, x, y);
    return obj;
}

static void close()
{
    if(s_overlay != nullptr) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void open()
{
    if(s_overlay == nullptr || s_panel == nullptr) {
        return;
    }

    lv_coord_t start_x = 0;
    lv_coord_t start_y = 0;
    switch(s_edge) {
        case GestureEdge::Left:
            start_x = -48;
            break;
        case GestureEdge::Right:
            start_x = 48;
            break;
        case GestureEdge::Top:
            start_y = -38;
            break;
        case GestureEdge::Bottom:
            start_y = 38;
            break;
    }

    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
    lv_obj_set_pos(s_panel, start_x, start_y);

    lv_anim_t anim_x;
    lv_anim_init(&anim_x);
    lv_anim_set_var(&anim_x, s_panel);
    lv_anim_set_values(&anim_x, start_x, 0);
    lv_anim_set_time(&anim_x, 170);
    lv_anim_set_exec_cb(&anim_x, [](void *target, int32_t value) {
        lv_obj_set_x(static_cast<lv_obj_t *>(target), static_cast<lv_coord_t>(value));
    });
    lv_anim_start(&anim_x);

    lv_anim_t anim_y;
    lv_anim_init(&anim_y);
    lv_anim_set_var(&anim_y, s_panel);
    lv_anim_set_values(&anim_y, start_y, 0);
    lv_anim_set_time(&anim_y, 170);
    lv_anim_set_exec_cb(&anim_y, [](void *target, int32_t value) {
        lv_obj_set_y(static_cast<lv_obj_t *>(target), static_cast<lv_coord_t>(value));
    });
    lv_anim_start(&anim_y);
}

static void route(uint16_t id)
{
    close();

    switch(id) {
        case 0:
            home::show();
            break;
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
            home::show();
            break;
        default:
            break;
    }
}

static void matrix_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        route(lv_btnmatrix_get_selected_btn(lv_event_get_target(event)));
    }
}

static void overlay_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED && lv_event_get_target(event) == s_overlay) {
        close();
    }
}

static void handle_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if(indev == nullptr) {
            return;
        }
        lv_indev_get_point(indev, &s_press_point);
        s_edge = static_cast<GestureEdge>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
        s_pressed = true;
    } else if(code == LV_EVENT_RELEASED && s_pressed) {
        lv_indev_t *indev = lv_indev_get_act();
        if(indev == nullptr) {
            s_pressed = false;
            return;
        }

        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);
        lv_coord_t dx = release_point.x - s_press_point.x;
        lv_coord_t dy = release_point.y - s_press_point.y;
        bool inward = false;

        switch(s_edge) {
            case GestureEdge::Left:
                inward = dx > kSwipeThreshold;
                break;
            case GestureEdge::Right:
                inward = dx < -kSwipeThreshold;
                break;
            case GestureEdge::Top:
                inward = dy > kSwipeThreshold;
                break;
            case GestureEdge::Bottom:
                inward = dy < -kSwipeThreshold;
                break;
        }

        if(inward) {
            open();
        }
        s_pressed = false;
    }
}

static void add_handle(lv_obj_t *screen, GestureEdge edge)
{
    lv_coord_t w = (edge == GestureEdge::Left || edge == GestureEdge::Right)
        ? kEdgeSize
        : LV_PCT(100);
    lv_coord_t h = (edge == GestureEdge::Top || edge == GestureEdge::Bottom)
        ? kEdgeSize
        : LV_PCT(100);
    lv_obj_t *handle = block(screen, w, h, amber(), LV_OPA_0);

    switch(edge) {
        case GestureEdge::Left:
            lv_obj_align(handle, LV_ALIGN_LEFT_MID, 0, 0);
            break;
        case GestureEdge::Right:
            lv_obj_align(handle, LV_ALIGN_RIGHT_MID, 0, 0);
            break;
        case GestureEdge::Top:
            lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);
            break;
        case GestureEdge::Bottom:
            lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, 0);
            break;
    }

    lv_obj_add_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(handle, handle_cb, LV_EVENT_PRESSED, reinterpret_cast<void *>(static_cast<uintptr_t>(edge)));
    lv_obj_add_event_cb(handle, handle_cb, LV_EVENT_RELEASED, reinterpret_cast<void *>(static_cast<uintptr_t>(edge)));
}

static void add_hint_tabs(lv_obj_t *screen)
{
    lv_obj_t *left = block(screen, 4, 74, amber(), LV_OPA_50);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_t *right = block(screen, 4, 74, amber(), LV_OPA_50);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -1, 0);
    lv_obj_t *top = block(screen, 86, 4, amber(), LV_OPA_30);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 1);
    lv_obj_t *bottom = block(screen, 86, 4, amber(), LV_OPA_30);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -1);
}

void attach(lv_obj_t *screen)
{
    if(screen == nullptr) {
        return;
    }

    add_handle(screen, GestureEdge::Left);
    add_handle(screen, GestureEdge::Right);
    add_handle(screen, GestureEdge::Top);
    add_handle(screen, GestureEdge::Bottom);
    add_hint_tabs(screen);

    s_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_overlay, overlay_cb, LV_EVENT_CLICKED, nullptr);

    s_panel = theme::create_panel(s_overlay);
    lv_obj_set_size(s_panel, 338, 230);
    lv_obj_center(s_panel);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x080603), 0);
    lv_obj_set_style_border_color(s_panel, amber(), 0);
    lv_obj_set_style_shadow_color(s_panel, amber(), 0);
    lv_obj_set_style_shadow_width(s_panel, 18, 0);
    lv_obj_set_style_shadow_opa(s_panel, LV_OPA_30, 0);

    label(s_panel, "OEL NAVI", &lv_font_montserrat_24, amber_hot(), LV_ALIGN_TOP_LEFT, 8, 6);
    label(s_panel, "PUXE DA BORDA", &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_TOP_RIGHT, -8, 14);

    static const char *map[] = {
        LV_SYMBOL_HOME " HOME",
        LV_SYMBOL_VIDEO " GIF",
        "\n",
        LV_SYMBOL_AUDIO " SPOT",
        LV_SYMBOL_SETTINGS " CONFIG",
        "\n",
        "SCAN SD",
        "FECHAR",
        "",
    };

    lv_obj_t *matrix = lv_btnmatrix_create(s_panel);
    lv_btnmatrix_set_map(matrix, map);
    lv_btnmatrix_set_btn_ctrl_all(matrix, LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_set_size(matrix, 314, 154);
    lv_obj_align(matrix, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_clear_flag(matrix, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(matrix, matrix_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_set_style_bg_color(matrix, lv_color_hex(0x120D05), 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_80, 0);
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 6, 0);
    lv_obj_set_style_pad_row(matrix, 7, 0);
    lv_obj_set_style_pad_column(matrix, 7, 0);
    lv_obj_set_style_bg_color(matrix, lv_color_hex(0x251805), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_90, LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, amber(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, amber_hot(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(matrix, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, amber_hot(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(matrix, lv_color_hex(0x050505), LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_move_foreground(s_overlay);
}

} // namespace navigation
} // namespace ui

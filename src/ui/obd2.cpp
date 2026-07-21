#include "obd2.h"

#include "home.h"
#include "navigation.h"
#include "obd2_data.h"
#include "obd2_rpm.h"
#include "theme.h"

#include "lvgl.h"

namespace ui {
namespace obd2 {

static lv_obj_t *block(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                       lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font,
                      lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

static void back_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        home::show();
    }
}

static void rpm_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        obd2_rpm::show();
    }
}

static lv_obj_t *back_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_btn_create(parent);
    theme::apply_subtle_button(button);
    lv_obj_set_size(button, 66, 28);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_add_event_cb(button, back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *caption = text(button, "VOLTAR", theme::font_small(), theme::colors().text,
                             LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

static lv_obj_t *rpm_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_btn_create(parent);
    theme::apply_button(button, true);
    lv_obj_set_size(button, 160, 108);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(button, rpm_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(button, theme::colors().panel_alt, 0);
    lv_obj_set_style_border_color(button, theme::colors().blue, 0);

    lv_obj_t *title = text(button, "RPM", &lv_font_montserrat_28, theme::colors().text,
                           LV_ALIGN_CENTER, 0, -12);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *subtitle = text(button, "CONTA-GIROS", theme::font_small(), theme::colors().muted,
                              LV_ALIGN_CENTER, 0, 30);
    lv_obj_clear_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = block(screen, lv_disp_get_hor_res(nullptr) - 24, 40,
                             theme::colors().panel, LV_OPA_COVER);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_border_color(header, theme::colors().blue, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    text(header, "HOME OBD2", theme::font_title(), theme::colors().text,
         LV_ALIGN_LEFT_MID, 10, 0);
    text(header, "OBD SIM", theme::font_small(), theme::colors().muted,
         LV_ALIGN_RIGHT_MID, -82, 0);
    back_button(header);

    lv_obj_t *panel = theme::create_panel(screen);
    lv_obj_set_size(panel, lv_disp_get_hor_res(nullptr) - 24, 260);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 24);
    text(panel, "FUNCOES AUTOMOTIVAS", theme::font_small(), theme::colors().cyan,
         LV_ALIGN_TOP_LEFT, 12, 10);
    rpm_button(panel);

    text(screen, "STATUS: CONNECTED / SIMULACAO", theme::font_small(), theme::colors().muted,
         LV_ALIGN_BOTTOM_MID, 0, -30);

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 220, 0, true);
}

} // namespace obd2
} // namespace ui

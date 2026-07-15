#include "settings.h"

#include "display.h"
#include "home.h"
#include "navigation.h"
#include "sd_browser.h"
#include "theme.h"

#include "lvgl.h"

#include <stdio.h>

namespace ui {
namespace settings {

static int s_brightness = 100;
static lv_disp_rot_t s_rotation = LV_DISP_ROT_90;

static lv_color_t amber()
{
    return theme::colors().blue;
}

static lv_color_t amber_hot()
{
    return theme::colors().text;
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

static lv_obj_t *button(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h,
                        lv_align_t align, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb, void *data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    theme::apply_button(btn, true);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, align, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, data);

    lv_obj_t *txt = lv_label_create(btn);
    lv_label_set_text(txt, text);
    lv_obj_center(txt);
    return btn;
}

static void palette_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    uintptr_t id = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    theme::set_palette(static_cast<theme::PaletteId>(id));
    show();
}

static const char *rotation_name()
{
    switch(s_rotation) {
        case LV_DISP_ROT_NONE:
            return "0";
        case LV_DISP_ROT_90:
            return "90";
        case LV_DISP_ROT_180:
            return "180";
        case LV_DISP_ROT_270:
            return "270";
        default:
            return "?";
    }
}

static void apply_rotation(lv_disp_rot_t rotation)
{
    s_rotation = rotation;
    lv_disp_t *disp = lv_disp_get_default();
    if(disp != nullptr) {
        lv_disp_set_rotation(disp, rotation);
    }
}

static void next_rotation()
{
    switch(s_rotation) {
        case LV_DISP_ROT_NONE:
            apply_rotation(LV_DISP_ROT_90);
            break;
        case LV_DISP_ROT_90:
            apply_rotation(LV_DISP_ROT_180);
            break;
        case LV_DISP_ROT_180:
            apply_rotation(LV_DISP_ROT_270);
            break;
        case LV_DISP_ROT_270:
        default:
            apply_rotation(LV_DISP_ROT_NONE);
            break;
    }
}

static void set_brightness(int value)
{
    if(value < 10) {
        value = 10;
    }
    if(value > 100) {
        value = 100;
    }
    s_brightness = value;
    bsp_display_brightness_set(s_brightness);
}

static void action_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    uintptr_t action = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    if(action == 1) {
        sd_browser::mount();
        show();
    } else if(action == 2) {
        home::show();
    } else if(action == 3) {
        set_brightness(s_brightness - 10);
        show();
    } else if(action == 4) {
        set_brightness(s_brightness + 10);
        show();
    } else if(action == 5) {
        next_rotation();
        show();
    }
}

static void create_palette_card(lv_obj_t *parent, const char *name, theme::PaletteId id,
                                lv_coord_t x, lv_coord_t y, lv_color_t c1, lv_color_t c2, lv_color_t c3)
{
    lv_obj_t *card = lv_btn_create(parent);
    theme::apply_subtle_button(card);
    lv_obj_set_size(card, 166, 42);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_add_event_cb(card, palette_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(id)));

    label(card, name, &lv_font_montserrat_14,
          theme::palette_id() == id ? lv_color_white() : theme::colors().text,
          LV_ALIGN_LEFT_MID, 8, 0);

    lv_color_t colors[] = {c1, c2, c3};
    for(size_t i = 0; i < 3; ++i) {
        lv_obj_t *swatch = lv_obj_create(card);
        lv_obj_remove_style_all(swatch);
        lv_obj_set_size(swatch, 16, 22);
        lv_obj_align(swatch, LV_ALIGN_RIGHT_MID, static_cast<lv_coord_t>(-8 - (2 - i) * 24), 0);
        lv_obj_set_style_bg_color(swatch, colors[i], 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(swatch, theme::colors().text, 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
    }
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 24, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_border_color(header, amber(), 0);
    label(header, "CONFIGURACOES", &lv_font_montserrat_24, amber_hot(), LV_ALIGN_LEFT_MID, 8, 0);
    label(header, "OEL SETUP", &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *panel = theme::create_panel(screen);
    lv_obj_set_size(panel, lv_disp_get_hor_res(nullptr) - 24, 230);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(panel, theme::colors().panel, 0);
    lv_obj_set_style_border_color(panel, amber(), 0);

    label(panel, "TEMA ATUAL", &lv_font_montserrat_12, theme::colors().muted, LV_ALIGN_TOP_LEFT, 10, 8);
    label(panel, theme::palette_name(), &lv_font_montserrat_24, amber_hot(), LV_ALIGN_TOP_LEFT, 10, 24);

    char brightness_text[32];
    snprintf(brightness_text, sizeof(brightness_text), "BRILHO %d%%", s_brightness);
    label(panel, brightness_text, &lv_font_montserrat_16, amber_hot(), LV_ALIGN_TOP_RIGHT, -10, 14);

    create_palette_card(panel, "LARANJA", theme::PaletteId::Orange, 10, 106,
                        lv_color_hex(0xFF9F1C), lv_color_hex(0xFF5A1F), lv_color_hex(0xFFD166));
    create_palette_card(panel, "VERMELHO", theme::PaletteId::Red, 190, 106,
                        lv_color_hex(0xFF3B30), lv_color_hex(0xB80F1D), lv_color_hex(0xFF8A80));
    create_palette_card(panel, "VERDE", theme::PaletteId::Green, 190, 106,
                        lv_color_hex(0x32D74B), lv_color_hex(0x0A7F3F), lv_color_hex(0xB7F25C));

    button(panel, "BRILHO -", 104, 34, LV_ALIGN_BOTTOM_LEFT, 10, -48, action_cb, reinterpret_cast<void *>(3));
    button(panel, "BRILHO +", 104, 34, LV_ALIGN_BOTTOM_LEFT, 122, -48, action_cb, reinterpret_cast<void *>(4));
    button(panel, "GIRAR", 104, 34, LV_ALIGN_BOTTOM_RIGHT, -10, -48, action_cb, reinterpret_cast<void *>(5));

    button(panel, "SCAN SD", 132, 36, LV_ALIGN_BOTTOM_LEFT, 10, -8, action_cb, reinterpret_cast<void *>(1));
    button(panel, LV_SYMBOL_HOME " VOLTAR", 132, 36, LV_ALIGN_BOTTOM_RIGHT, -10, -8, action_cb, reinterpret_cast<void *>(2));

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, true);
}

} // namespace settings
} // namespace ui

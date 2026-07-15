#include "theme.h"

#include <stdint.h>

namespace ui {
namespace theme {

static bool s_ready = false;
static PaletteId s_palette_id = PaletteId::Orange;

static Palette s_palette = {
    lv_color_hex(0xF2F4F7),
    lv_color_hex(0xFFFFFF),
    lv_color_hex(0xE7EDF2),
    lv_color_hex(0x03A9C2),
    lv_color_hex(0x2196F3),
    lv_color_hex(0xF44336),
    lv_color_hex(0x263238),
    lv_color_hex(0x87909A),
};

static lv_style_t s_panel;
static lv_style_t s_button;
static lv_style_t s_button_pressed;
static lv_style_t s_button_disabled;
static lv_style_t s_subtle_button;
static lv_style_t s_title;
static lv_style_t s_text;
static lv_style_t s_muted;

static Palette make_palette(PaletteId id)
{
    switch(id) {
        case PaletteId::Orange:
            return {
                lv_color_hex(0x060402),
                lv_color_hex(0x16100A),
                lv_color_hex(0x21150B),
                lv_color_hex(0xFF9F1C),
                lv_color_hex(0xFF5A1F),
                lv_color_hex(0xFFD166),
                lv_color_hex(0xFFF8EF),
                lv_color_hex(0xB89A78),
            };
        case PaletteId::Red:
            return {
                lv_color_hex(0x070203),
                lv_color_hex(0x17090B),
                lv_color_hex(0x220D10),
                lv_color_hex(0xFF3B30),
                lv_color_hex(0xB80F1D),
                lv_color_hex(0xFF8A80),
                lv_color_hex(0xFFF3F2),
                lv_color_hex(0xB78282),
            };
        case PaletteId::Green:
            return {
                lv_color_hex(0x020704),
                lv_color_hex(0x07160D),
                lv_color_hex(0x0C2114),
                lv_color_hex(0x32D74B),
                lv_color_hex(0x0A7F3F),
                lv_color_hex(0xB7F25C),
                lv_color_hex(0xF2FFF5),
                lv_color_hex(0x7BA887),
            };
    }
}

static void reset_styles()
{
    if(!s_ready) {
        return;
    }
    lv_style_reset(&s_panel);
    lv_style_reset(&s_button);
    lv_style_reset(&s_button_pressed);
    lv_style_reset(&s_button_disabled);
    lv_style_reset(&s_subtle_button);
    lv_style_reset(&s_title);
    lv_style_reset(&s_text);
    lv_style_reset(&s_muted);
    s_ready = false;
}

const Palette &colors()
{
    return s_palette;
}

PaletteId palette_id()
{
    return s_palette_id;
}

const char *palette_name()
{
    switch(s_palette_id) {
        case PaletteId::Demo:
            return "DEMO";
        case PaletteId::Orange:
            return "LARANJA";
        case PaletteId::Red:
            return "VERMELHO";
        case PaletteId::Green:
            return "VERDE";
        case PaletteId::Cyber:
        default:
            return "CYBER";
    }
}

void set_palette(PaletteId id)
{
    if(s_palette_id == id && s_ready) {
        return;
    }
    s_palette_id = id;
    s_palette = make_palette(id);
    reset_styles();
    init();
}

PaletteId cycle_palette()
{
    PaletteId next = PaletteId::Demo;
    switch(s_palette_id) {
        case PaletteId::Demo:
            next = PaletteId::Cyber;
            break;
        case PaletteId::Cyber:
            next = PaletteId::Orange;
            break;
        case PaletteId::Orange:
            next = PaletteId::Red;
            break;
        case PaletteId::Red:
            next = PaletteId::Green;
            break;
        case PaletteId::Green:
        default:
            next = PaletteId::Demo;
            break;
    }
    set_palette(next);
    return next;
}

void init()
{
    if(s_ready) {
        return;
    }

    lv_style_init(&s_panel);
    lv_style_set_bg_color(&s_panel, s_palette.panel);
    lv_style_set_bg_opa(&s_panel, LV_OPA_COVER);
    lv_style_set_border_color(&s_panel, s_palette.cyan);
    lv_style_set_border_width(&s_panel, 1);
    lv_style_set_radius(&s_panel, 6);
    lv_style_set_pad_all(&s_panel, 8);
    lv_style_set_shadow_color(&s_panel, s_palette.blue);
    lv_style_set_shadow_width(&s_panel, 8);
    lv_style_set_shadow_opa(&s_panel, LV_OPA_20);

    lv_style_init(&s_button);
    lv_style_set_bg_color(&s_button, s_palette.blue);
    lv_style_set_bg_opa(&s_button, LV_OPA_COVER);
    lv_style_set_border_color(&s_button, s_palette.blue);
    lv_style_set_border_width(&s_button, 1);
    lv_style_set_radius(&s_button, 8);
    lv_style_set_pad_left(&s_button, 12);
    lv_style_set_pad_right(&s_button, 12);
    lv_style_set_text_color(&s_button, s_palette.text);
    lv_style_set_text_font(&s_button, &lv_font_montserrat_20);
    lv_style_set_shadow_color(&s_button, s_palette.cyan);
    lv_style_set_shadow_width(&s_button, 4);
    lv_style_set_shadow_opa(&s_button, LV_OPA_10);

    lv_style_init(&s_button_pressed);
    lv_style_set_bg_color(&s_button_pressed, s_palette.cyan);
    lv_style_set_text_color(&s_button_pressed, s_palette.text);
    lv_style_set_shadow_opa(&s_button_pressed, LV_OPA_50);

    lv_style_init(&s_button_disabled);
    lv_style_set_bg_color(&s_button_disabled, s_palette.panel_alt);
    lv_style_set_bg_grad_color(&s_button_disabled, s_palette.panel);
    lv_style_set_border_color(&s_button_disabled, lv_color_hex(0x2C3B42));
    lv_style_set_text_color(&s_button_disabled, lv_color_hex(0x53656C));
    lv_style_set_shadow_opa(&s_button_disabled, LV_OPA_TRANSP);

    lv_style_init(&s_subtle_button);
    lv_style_set_bg_color(&s_subtle_button, s_palette.panel);
    lv_style_set_bg_opa(&s_subtle_button, LV_OPA_COVER);
    lv_style_set_border_color(&s_subtle_button, s_palette.panel_alt);
    lv_style_set_border_width(&s_subtle_button, 1);
    lv_style_set_radius(&s_subtle_button, 8);
    lv_style_set_text_color(&s_subtle_button, s_palette.text);
    lv_style_set_text_font(&s_subtle_button, &lv_font_montserrat_16);
    lv_style_set_pad_all(&s_subtle_button, 6);

    lv_style_init(&s_title);
    lv_style_set_text_color(&s_title, s_palette.cyan);
    lv_style_set_text_font(&s_title, &lv_font_montserrat_24);

    lv_style_init(&s_text);
    lv_style_set_text_color(&s_text, s_palette.text);
    lv_style_set_text_font(&s_text, &lv_font_montserrat_16);

    lv_style_init(&s_muted);
    lv_style_set_text_color(&s_muted, s_palette.muted);
    lv_style_set_text_font(&s_muted, &lv_font_montserrat_12);

    s_ready = true;
}

void apply_screen(lv_obj_t *obj)
{
    init();
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, s_palette.background, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

void apply_panel(lv_obj_t *obj)
{
    init();
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &s_panel, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void apply_button(lv_obj_t *obj, bool enabled)
{
    init();
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &s_button, 0);
    lv_obj_add_style(obj, &s_button_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &s_button_disabled, LV_STATE_DISABLED);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    if(enabled) {
        lv_obj_clear_state(obj, LV_STATE_DISABLED);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}

void apply_subtle_button(lv_obj_t *obj)
{
    init();
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &s_subtle_button, 0);
    lv_obj_add_style(obj, &s_button_pressed, LV_STATE_PRESSED);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void apply_title(lv_obj_t *obj)
{
    init();
    lv_obj_add_style(obj, &s_title, 0);
}

void apply_text(lv_obj_t *obj)
{
    init();
    lv_obj_add_style(obj, &s_text, 0);
}

void apply_muted_text(lv_obj_t *obj)
{
    init();
    lv_obj_add_style(obj, &s_muted, 0);
}

lv_obj_t *create_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    apply_panel(panel);
    return panel;
}

static void make_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, w, h);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, color, 0);
    lv_obj_set_style_bg_opa(line, opa, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(line, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

void add_scanlines(lv_obj_t *parent, lv_opa_t opa)
{
    if(s_palette_id == PaletteId::Demo) return;
    init();
    lv_coord_t h = lv_disp_get_ver_res(NULL);
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    for(lv_coord_t y = 6; y < h; y += 12) {
        make_line(parent, 0, y, w, 1, s_palette.cyan, opa);
    }
}

void add_frame_ticks(lv_obj_t *parent)
{
    if(s_palette_id == PaletteId::Demo) return;
    init();
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    lv_coord_t h = lv_disp_get_ver_res(NULL);

    make_line(parent, 6, 6, 54, 2, s_palette.cyan, LV_OPA_80);
    make_line(parent, 6, 6, 2, 32, s_palette.cyan, LV_OPA_80);
    make_line(parent, w - 60, 6, 54, 2, s_palette.purple, LV_OPA_80);
    make_line(parent, w - 8, 6, 2, 32, s_palette.purple, LV_OPA_80);
    make_line(parent, 6, h - 8, 54, 2, s_palette.purple, LV_OPA_80);
    make_line(parent, 6, h - 38, 2, 32, s_palette.purple, LV_OPA_80);
    make_line(parent, w - 60, h - 8, 54, 2, s_palette.cyan, LV_OPA_80);
    make_line(parent, w - 8, h - 38, 2, 32, s_palette.cyan, LV_OPA_80);
}

void pulse_opacity(lv_obj_t *obj, lv_opa_t low, lv_opa_t high, uint32_t time_ms)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, low, high);
    lv_anim_set_time(&anim, time_ms);
    lv_anim_set_playback_time(&anim, time_ms);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, [](void *target, int32_t value) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(target), static_cast<lv_opa_t>(value), 0);
    });
    lv_anim_start(&anim);
}

} // namespace theme
} // namespace ui

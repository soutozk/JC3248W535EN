#pragma once

#include "lvgl.h"

namespace ui {
namespace theme {

struct Palette {
    lv_color_t background;
    lv_color_t panel;
    lv_color_t panel_alt;
    lv_color_t cyan;
    lv_color_t blue;
    lv_color_t purple;
    lv_color_t text;
    lv_color_t muted;
};

enum class PaletteId {
    Blue = 0,
    Yellow,
    Orange,
    Green,
};

void init();
const Palette &colors();
PaletteId palette_id();
const char *palette_name();
void set_palette(PaletteId id);
PaletteId cycle_palette();
const lv_font_t *font_small();
const lv_font_t *font_body();
const lv_font_t *font_title();
const lv_font_t *font_icon();

void apply_screen(lv_obj_t *obj);
void apply_panel(lv_obj_t *obj);
void apply_button(lv_obj_t *obj, bool enabled);
void apply_subtle_button(lv_obj_t *obj);
void apply_title(lv_obj_t *obj);
void apply_text(lv_obj_t *obj);
void apply_muted_text(lv_obj_t *obj);

lv_obj_t *create_panel(lv_obj_t *parent);
void add_scanlines(lv_obj_t *parent, lv_opa_t opa);
void add_frame_ticks(lv_obj_t *parent);
void pulse_opacity(lv_obj_t *obj, lv_opa_t low, lv_opa_t high, uint32_t time_ms);

} // namespace theme
} // namespace ui

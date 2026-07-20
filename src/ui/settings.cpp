#include "settings.h"

#include "assets.h"
#include "display.h"
#include "home.h"
#include "navigation.h"
#include "preferences.h"
#include "sd_browser.h"
#include "theme.h"

#include "lvgl.h"
#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ui {
namespace settings {

static const char *TAG = "ui_settings";

static int s_brightness = 100;
static sd_browser::ImageCatalog s_images = {};

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
                        lv_align_t align, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb, uintptr_t data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    theme::apply_button(btn, true);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, align, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(data));

    lv_obj_t *txt = lv_label_create(btn);
    lv_label_set_text(txt, text);
    lv_obj_set_style_text_font(txt, theme::font_small(), 0);
    lv_obj_center(txt);
    return btn;
}

static lv_color_t palette_color(theme::PaletteId id)
{
    switch(id) {
        case theme::PaletteId::Blue:
            return lv_color_hex(0x1688FF);
        case theme::PaletteId::Yellow:
            return lv_color_hex(0xFFE04B);
        case theme::PaletteId::Orange:
            return lv_color_hex(0xFF9F1C);
        case theme::PaletteId::Green:
        default:
            return lv_color_hex(0x32D74B);
    }
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

static void brightness_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    uintptr_t direction = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    s_brightness += direction == 1 ? -10 : 10;
    if(s_brightness < 10) {
        s_brightness = 10;
    }
    if(s_brightness > 100) {
        s_brightness = 100;
    }
    preferences::set_brightness(s_brightness);
    bsp_display_brightness_set(s_brightness);
    show();
}

static const char *file_name(const char *path)
{
    if(path == nullptr || path[0] == '\0') {
        return "PADRAO";
    }
    const char *slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

static int selected_image_index(const char *path)
{
    if(path == nullptr || path[0] == '\0') {
        return -1;
    }
    for(size_t i = 0; i < s_images.count; ++i) {
        if(strcmp(path, s_images.files[i].path) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void image_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED || s_images.count == 0) {
        return;
    }

    bool boot = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) == 1;
    const char *current = boot ? preferences::boot_image_path() : preferences::logo_image_path();
    int index = selected_image_index(current);
    index = (index + 1) % static_cast<int>(s_images.count);

    if(boot) {
        preferences::set_boot_image_path(s_images.files[index].path);
    } else {
        preferences::set_logo_image_path(s_images.files[index].path);
    }
    ESP_LOGI(TAG, "selected %s image: %s", boot ? "boot" : "logo", s_images.files[index].path);
    show();
}

static void back_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        home::show();
    }
}

static lv_obj_t *palette_button(lv_obj_t *parent, const char *text, theme::PaletteId id,
                                lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    theme::apply_subtle_button(btn);
    lv_obj_set_size(btn, 96, 34);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, palette_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(id)));
    lv_color_t color = palette_color(id);
    lv_obj_set_style_border_color(btn, color, 0);
    lv_obj_set_style_border_width(btn, theme::palette_id() == id ? 2 : 1, 0);

    lv_obj_t *swatch = lv_obj_create(btn);
    lv_obj_remove_style_all(swatch);
    lv_obj_set_size(swatch, 12, 18);
    lv_obj_align(swatch, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(swatch, color, 0);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);

    lv_obj_t *txt = lv_label_create(btn);
    lv_label_set_text(txt, text);
    lv_obj_set_style_text_font(txt, theme::font_small(), 0);
    lv_obj_set_style_text_color(txt, theme::colors().text, 0);
    lv_obj_align(txt, LV_ALIGN_LEFT_MID, 25, 0);
    return btn;
}

static lv_obj_t *image_value(lv_obj_t *parent, const char *path, lv_coord_t y)
{
    lv_obj_t *value = lv_label_create(parent);
    lv_label_set_text(value, file_name(path));
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(value, 198);
    lv_obj_set_style_text_font(value, theme::font_body(), 0);
    lv_obj_set_style_text_color(value, theme::colors().text, 0);
    lv_obj_align(value, LV_ALIGN_TOP_LEFT, 10, y);
    return value;
}

static void create_images_panel(lv_obj_t *panel)
{
    label(panel, "IMAGENS DO SD", theme::font_title(), theme::colors().text,
          LV_ALIGN_TOP_LEFT, 10, 10);
    label(panel, "ARQUIVOS PNG", theme::font_small(), theme::colors().muted,
          LV_ALIGN_TOP_LEFT, 10, 31);

    label(panel, "BOOT", theme::font_small(), theme::colors().blue, LV_ALIGN_TOP_LEFT, 10, 55);
    image_value(panel, assets::intro_image_posix_path(), 70);
    button(panel, "PROXIMA FOTO", 198, 30, LV_ALIGN_TOP_LEFT, 10, 101, image_cb, 1);

    label(panel, "LOGO DA HOME", theme::font_small(), theme::colors().blue, LV_ALIGN_TOP_LEFT, 10, 141);
    image_value(panel, assets::home_logo_posix_path(), 156);
    button(panel, "PROXIMA FOTO", 198, 30, LV_ALIGN_TOP_LEFT, 10, 187, image_cb, 2);

    if(s_images.count == 0) {
        label(panel, "SD SEM IMAGENS COMPATIVEIS", theme::font_small(), theme::colors().muted,
              LV_ALIGN_BOTTOM_LEFT, 10, -10);
    } else {
        char status[40];
        snprintf(status, sizeof(status), "%u ARQUIVO(S) ENCONTRADO(S)", static_cast<unsigned>(s_images.count));
        label(panel, status, theme::font_small(), theme::colors().muted,
              LV_ALIGN_BOTTOM_LEFT, 10, -10);
    }
}

void init()
{
    preferences::init();
    s_brightness = preferences::brightness();
    bsp_display_brightness_set(s_brightness);
}

void show()
{
    preferences::init();
    s_brightness = preferences::brightness();
    sd_browser::mount();
    sd_browser::scan_images(&s_images);
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 16, 38);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_border_color(header, theme::colors().blue, 0);
    label(header, "CONFIGURACOES", theme::font_title(), theme::colors().text,
          LV_ALIGN_LEFT_MID, 8, 0);
    label(header, "OEL / SETUP", theme::font_small(), theme::colors().muted,
          LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *display_panel = theme::create_panel(screen);
    lv_obj_set_size(display_panel, 224, 250);
    lv_obj_align(display_panel, LV_ALIGN_TOP_LEFT, 8, 56);
    lv_obj_set_style_border_color(display_panel, theme::colors().blue, 0);

    label(display_panel, "COR DA INTERFACE", theme::font_title(), theme::colors().text,
          LV_ALIGN_TOP_LEFT, 10, 10);
    label(display_panel, theme::palette_name(), theme::font_small(), theme::colors().muted,
          LV_ALIGN_TOP_RIGHT, -10, 13);

    palette_button(display_panel, "AZUL", theme::PaletteId::Blue, 10, 40);
    palette_button(display_panel, "AMARELO", theme::PaletteId::Yellow, 112, 40);
    palette_button(display_panel, "LARANJA", theme::PaletteId::Orange, 10, 80);
    palette_button(display_panel, "VERDE", theme::PaletteId::Green, 112, 80);

    label(display_panel, "BRILHO", theme::font_small(), theme::colors().muted,
          LV_ALIGN_TOP_LEFT, 10, 128);
    char brightness_text[24];
    snprintf(brightness_text, sizeof(brightness_text), "%d%%", s_brightness);
    label(display_panel, brightness_text, theme::font_title(), theme::colors().text,
          LV_ALIGN_TOP_RIGHT, -10, 124);
    button(display_panel, "- 10", 92, 30, LV_ALIGN_TOP_LEFT, 10, 159, brightness_cb, 1);
    button(display_panel, "+ 10", 92, 30, LV_ALIGN_TOP_LEFT, 112, 159, brightness_cb, 2);
    label(display_panel, "SALVO NA MEMORIA", theme::font_small(), theme::colors().muted,
          LV_ALIGN_TOP_LEFT, 10, 198);
    button(display_panel, "VOLTAR", 204, 32, LV_ALIGN_BOTTOM_MID, 0, -8, back_cb, 0);

    lv_obj_t *images_panel = theme::create_panel(screen);
    lv_obj_set_size(images_panel, 224, 250);
    lv_obj_align(images_panel, LV_ALIGN_TOP_RIGHT, -8, 56);
    lv_obj_set_style_border_color(images_panel, theme::colors().blue, 0);
    create_images_panel(images_panel);

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, true);
}

} // namespace settings
} // namespace ui

#include "boot.h"

#include "assets.h"
#include "home.h"
#include "theme.h"

#include "lvgl.h"
#include "src/extra/libs/gif/lv_gif.h"

namespace ui {
namespace boot {

static void load_home_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    home::show();
}

static void finish_boot_cb(lv_timer_t *timer)
{
    lv_obj_t *screen = static_cast<lv_obj_t *>(timer->user_data);
    if(screen != nullptr) {
        lv_obj_fade_out(screen, 360, 0);
    }

    lv_timer_t *load_timer = lv_timer_create(load_home_cb, 380, nullptr);
    lv_timer_set_repeat_count(load_timer, 1);
}

static void create_text_logo(lv_obj_t *screen)
{
    lv_obj_t *glow = lv_label_create(screen);
    lv_label_set_text(glow, assets::project_name());
    lv_obj_set_style_text_font(glow, theme::font_title(), 0);
    lv_obj_set_style_text_color(glow, theme::colors().purple, 0);
    lv_obj_set_style_opa(glow, LV_OPA_50, 0);
    lv_obj_align(glow, LV_ALIGN_CENTER, 2, 2);

    lv_obj_t *logo = lv_label_create(screen);
    lv_label_set_text(logo, assets::project_name());
    lv_obj_set_style_text_font(logo, theme::font_title(), 0);
    lv_obj_set_style_text_color(logo, theme::colors().cyan, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);
    theme::pulse_opacity(glow, LV_OPA_20, LV_OPA_70, 420);
}

void show()
{
    theme::init();

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *glow_panel = lv_obj_create(screen);
    lv_obj_remove_style_all(glow_panel);
    lv_obj_set_size(glow_panel, 300, 108);
    lv_obj_center(glow_panel);
    lv_obj_set_style_bg_color(glow_panel, theme::colors().blue, 0);
    lv_obj_set_style_bg_opa(glow_panel, LV_OPA_10, 0);
    lv_obj_set_style_border_color(glow_panel, theme::colors().purple, 0);
    lv_obj_set_style_border_width(glow_panel, 1, 0);
    lv_obj_set_style_shadow_color(glow_panel, theme::colors().cyan, 0);
    lv_obj_set_style_shadow_width(glow_panel, 18, 0);
    lv_obj_set_style_shadow_opa(glow_panel, LV_OPA_30, 0);
    theme::pulse_opacity(glow_panel, LV_OPA_20, LV_OPA_80, 700);

    if(assets::intro_gif_available()) {
        lv_obj_t *intro = lv_gif_create(screen);
        lv_gif_set_src(intro, assets::intro_gif_lvgl_path());
        lv_obj_center(intro);
    } else if(assets::intro_image_available()) {
        lv_obj_t *logo = lv_img_create(screen);
        lv_img_set_src(logo, assets::intro_image_lvgl_path());
        lv_obj_center(logo);
        lv_obj_fade_in(logo, 420, 80);
    } else if(assets::logo_available()) {
        lv_obj_t *logo = lv_img_create(screen);
        lv_img_set_src(logo, assets::logo_lvgl_path());
        lv_obj_center(logo);
        lv_obj_fade_in(logo, 420, 80);
    } else {
        create_text_logo(screen);
    }

    lv_obj_t *tag = lv_label_create(screen);
    lv_label_set_text(tag, "LVGL DEMO BOOT");
    theme::apply_muted_text(tag);
    lv_obj_align(tag, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 420, 0, true);
    lv_obj_fade_in(screen, 420, 0);

    lv_timer_t *timer = lv_timer_create(finish_boot_cb, 2000, screen);
    lv_timer_set_repeat_count(timer, 1);
}

} // namespace boot
} // namespace ui

#include "media_menu.h"

#include "audio_player.h"
#include "gif_player.h"
#include "home.h"
#include "navigation.h"
#include "theme.h"

#include "lvgl.h"

namespace ui {
namespace media_menu {
namespace {

static void back_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) home::show();
}

static void open_gif_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) gif_player::show_list();
}

static void open_audio_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) audio_player::show_list();
}

static lv_obj_t *choice(lv_obj_t *parent, const char *title, const char *description,
                        lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    theme::apply_button(button, true);
    lv_obj_set_size(button, LV_PCT(100), 78);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *title_label = lv_label_create(button);
    lv_label_set_text(title_label, title);
    theme::apply_title(title_label);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t *description_label = lv_label_create(button);
    lv_label_set_text(description_label, description);
    theme::apply_muted_text(description_label);
    lv_obj_align(description_label, LV_ALIGN_BOTTOM_LEFT, 14, -12);
    return button;
}

} // namespace

void show()
{
    theme::init();
    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);

    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 24, 52);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back = lv_btn_create(header);
    theme::apply_subtle_button(back);
    lv_obj_set_size(back, 42, 30);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, theme::font_icon(), 0);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "MIDIA");
    theme::apply_title(title);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 54, 0);

    lv_obj_t *question = lv_label_create(screen);
    lv_label_set_text(question, "O QUE VOCE QUER ABRIR?");
    theme::apply_text(question);
    lv_obj_align(question, LV_ALIGN_TOP_MID, 0, 86);

    lv_obj_t *panel = theme::create_panel(screen);
    lv_obj_set_size(panel, lv_disp_get_hor_res(nullptr) - 48, 186);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 26);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 14, 0);
    choice(panel, "GIF", "ANIMACOES EM /sd/gifs", open_gif_cb);
    choice(panel, "AUDIO", "MUSICAS MP3/WAV EM /sd/music", open_audio_cb);

    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, true);
}

} // namespace media_menu
} // namespace ui

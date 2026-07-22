#include "audio_player.h"

#include "audio_output.h"
#include "home.h"
#include "navigation.h"
#include "sd_browser.h"
#include "theme.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ui {
namespace audio_player {
namespace {

static const char *TAG = "audio_player";

enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Error,
};

struct WavInfo {
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
};

sd_browser::AudioCatalog s_catalog = {};
sd_browser::AudioFile s_current_file = {};
std::atomic<PlaybackState> s_state{PlaybackState::Stopped};
std::atomic_bool s_stop_requested{false};
lv_timer_t *s_status_timer = nullptr;
lv_obj_t *s_status_label = nullptr;
int16_t s_stereo_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME] = {};

static uint16_t read_u16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

static bool parse_wav(FILE *file, WavInfo *info)
{
    uint8_t header[12] = {};
    if(file == nullptr || info == nullptr || fread(header, 1, sizeof(header), file) != sizeof(header) ||
       memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool format_found = false;
    bool data_found = false;
    while(!data_found) {
        uint8_t chunk_header[8] = {};
        if(fread(chunk_header, 1, sizeof(chunk_header), file) != sizeof(chunk_header)) {
            return false;
        }
        uint32_t chunk_size = read_u32(chunk_header + 4);
        if(memcmp(chunk_header, "fmt ", 4) == 0) {
            uint8_t format[16] = {};
            if(chunk_size < sizeof(format) || fread(format, 1, sizeof(format), file) != sizeof(format)) {
                return false;
            }
            const uint16_t format_tag = read_u16(format);
            info->channels = read_u16(format + 2);
            info->sample_rate = read_u32(format + 4);
            info->bits_per_sample = read_u16(format + 14);
            if(format_tag != 1 || (info->channels != 1 && info->channels != 2) ||
               info->bits_per_sample != 16 || info->sample_rate < 8000 || info->sample_rate > 48000) {
                return false;
            }
            uint32_t remaining = chunk_size - sizeof(format);
            if(remaining > 0 && fseek(file, remaining, SEEK_CUR) != 0) {
                return false;
            }
            format_found = true;
        } else if(memcmp(chunk_header, "data", 4) == 0) {
            info->data_offset = static_cast<uint32_t>(ftell(file));
            info->data_size = chunk_size;
            data_found = true;
        } else if(fseek(file, chunk_size, SEEK_CUR) != 0) {
            return false;
        }

        if((chunk_size & 1U) != 0U && !data_found && fseek(file, 1, SEEK_CUR) != 0) {
            return false;
        }
    }
    return format_found;
}

static bool start_output(uint32_t sample_rate, bool *started)
{
    if(*started) {
        return true;
    }
    if(audio_output::start_music(sample_rate) != ESP_OK) {
        return false;
    }
    *started = true;
    return true;
}

static bool write_frames(const int16_t *samples, size_t frames, uint16_t channels)
{
    if(samples == nullptr || (channels != 1 && channels != 2)) {
        return false;
    }
    if(channels == 2) {
        return audio_output::write(samples, frames * 2) == ESP_OK;
    }

    if(frames * 2 > sizeof(s_stereo_buffer) / sizeof(s_stereo_buffer[0])) {
        return false;
    }
    for(size_t i = 0; i < frames; ++i) {
        s_stereo_buffer[2 * i] = samples[i];
        s_stereo_buffer[2 * i + 1] = samples[i];
    }
    return audio_output::write(s_stereo_buffer, frames * 2) == ESP_OK;
}

static bool play_wav(FILE *file)
{
    WavInfo info = {};
    if(!parse_wav(file, &info)) {
        return false;
    }

    bool output_started = false;
    if(!start_output(info.sample_rate, &output_started)) {
        return false;
    }

    uint8_t input[1024] = {};
    uint32_t remaining = info.data_size;
    while(!s_stop_requested.load() && remaining > 0) {
        const size_t requested = remaining < sizeof(input) ? remaining : sizeof(input);
        size_t read = fread(input, 1, requested, file);
        if(read == 0) {
            break;
        }
        remaining -= static_cast<uint32_t>(read);
        read -= read % (info.channels * sizeof(int16_t));
        if(read > 0 && !write_frames(reinterpret_cast<const int16_t *>(input),
                                     read / (info.channels * sizeof(int16_t)), info.channels)) {
            return false;
        }
    }
    return true;
}

static bool play_mp3(FILE *file)
{
    static constexpr size_t kInputSize = 4096;
    uint8_t input[kInputSize] = {};
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME] = {};
    mp3dec_t decoder = {};
    mp3dec_init(&decoder);

    size_t available = 0;
    bool output_started = false;
    bool decoded_frame = false;
    bool end_of_file = false;
    while(!s_stop_requested.load()) {
        if(available < 2048 && !end_of_file) {
            const size_t read = fread(input + available, 1, kInputSize - available, file);
            available += read;
            if(read == 0) {
                end_of_file = true;
            }
            if(end_of_file && available == 0) {
                break;
            }
        }

        mp3dec_frame_info_t frame_info = {};
        const int samples = mp3dec_decode_frame(&decoder, input, static_cast<int>(available),
                                                pcm, &frame_info);
        if(frame_info.frame_bytes > 0 && static_cast<size_t>(frame_info.frame_bytes) <= available) {
            const size_t consumed = static_cast<size_t>(frame_info.frame_bytes);
            available -= consumed;
            memmove(input, input + consumed, available);
        } else if(available == kInputSize) {
            // Skip a byte while looking for the first valid frame (for example
            // after a large ID3 tag) so a malformed file cannot stall playback.
            memmove(input, input + 1, --available);
            continue;
        } else {
            if(end_of_file) {
                break;
            }
            continue;
        }

        if(samples <= 0) {
            continue;
        }
        if(frame_info.channels != 1 && frame_info.channels != 2) {
            return false;
        }
        if(!start_output(static_cast<uint32_t>(frame_info.hz), &output_started) ||
           !write_frames(pcm, static_cast<size_t>(samples),
                         static_cast<uint16_t>(frame_info.channels))) {
            return false;
        }
        decoded_frame = true;
    }
    return decoded_frame;
}

static void playback_task(void *)
{
    FILE *file = fopen(s_current_file.path, "rb");
    bool ok = file != nullptr;
    if(ok) {
        ok = s_current_file.type == sd_browser::AudioType::Mp3
            ? play_mp3(file)
            : play_wav(file);
        fclose(file);
    }
    audio_output::stop_music();
    s_state.store(ok ? PlaybackState::Stopped : PlaybackState::Error);
    vTaskDelete(nullptr);
}

static const char *status_text()
{
    switch(s_state.load()) {
    case PlaybackState::Playing:
        return "TOCANDO DO SD";
    case PlaybackState::Error:
        return "AUDIO INVALIDO OU ERRO I2S";
    case PlaybackState::Stopped:
    default:
        return "PRONTO PARA TOCAR";
    }
}

static void stop()
{
    s_stop_requested.store(true);
}

static void status_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if(s_status_label != nullptr) {
        lv_label_set_text(s_status_label, status_text());
    }
}

static void player_cleanup_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    stop();
    if(s_status_timer != nullptr) {
        lv_timer_del(s_status_timer);
        s_status_timer = nullptr;
    }
    s_status_label = nullptr;
}

static void show_player();

static void back_to_list_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        stop();
        show_list();
    }
}

static void play_stop_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if(s_state.load() == PlaybackState::Playing) {
        stop();
        return;
    }

    s_stop_requested.store(false);
    s_state.store(PlaybackState::Playing);
    if(xTaskCreate(playback_task, "sd_wav", 6144, nullptr, 4, nullptr) != pdPASS) {
        s_state.store(PlaybackState::Error);
        ESP_LOGE(TAG, "Unable to create audio task");
    }
}

static lv_obj_t *create_header(lv_obj_t *screen, const char *title, lv_event_cb_t back_cb)
{
    lv_obj_t *header = theme::create_panel(screen);
    lv_obj_set_size(header, lv_disp_get_hor_res(nullptr) - 24, 46);
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

    lv_obj_t *label = lv_label_create(header);
    lv_label_set_text(label, title);
    theme::apply_title(label);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 54, 0);
    return header;
}

static void open_audio_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    const auto *file = static_cast<const sd_browser::AudioFile *>(lv_event_get_user_data(event));
    if(file == nullptr) {
        return;
    }
    s_current_file = *file;
    show_player();
}

static void refresh_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        show_list();
    }
}

} // namespace

void show_list()
{
    stop();
    theme::init();
    sd_browser::scan_audio(&s_catalog);

    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);
    lv_obj_t *header = create_header(screen, "AUDIO DO SD", [](lv_event_t *event) {
        if(lv_event_get_code(event) == LV_EVENT_CLICKED) home::show();
    });

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

    if(!s_catalog.sd_ready || !s_catalog.directory_found || s_catalog.count == 0) {
        lv_obj_t *empty = lv_label_create(panel);
        lv_label_set_text(empty, !s_catalog.sd_ready ? "SD OFFLINE" :
                          !s_catalog.directory_found ? "PASTA /sd/music NAO ENCONTRADA" :
                          "SEM MP3/WAV EM /sd/music");
        theme::apply_title(empty);
        lv_obj_center(empty);
    } else {
        for(size_t i = 0; i < s_catalog.count; ++i) {
            lv_obj_t *row = lv_btn_create(panel);
            theme::apply_subtle_button(row);
            lv_obj_set_size(row, LV_PCT(100), 42);
            lv_obj_add_event_cb(row, open_audio_cb, LV_EVENT_CLICKED, &s_catalog.files[i]);
            lv_obj_t *name = lv_label_create(row);
            lv_label_set_text(name, s_catalog.files[i].name);
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_width(name, 300);
            lv_obj_align(name, LV_ALIGN_LEFT_MID, 8, 0);
        }
    }
    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, true);
}

namespace {

static void show_player()
{
    theme::init();
    lv_obj_t *screen = lv_obj_create(nullptr);
    theme::apply_screen(screen);
    theme::add_frame_ticks(screen);
    theme::add_scanlines(screen, LV_OPA_10);
    lv_obj_add_event_cb(screen, player_cleanup_cb, LV_EVENT_DELETE, nullptr);
    create_header(screen, "REPRODUTOR AUDIO", back_to_list_cb);

    lv_obj_t *panel = theme::create_panel(screen);
    lv_obj_set_size(panel, lv_disp_get_hor_res(nullptr) - 48, 180);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 18);

    lv_obj_t *name = lv_label_create(panel);
    lv_label_set_text(name, s_current_file.name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 340);
    theme::apply_title(name);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 20);

    s_status_label = lv_label_create(panel);
    lv_label_set_text(s_status_label, status_text());
    theme::apply_muted_text(s_status_label);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t *play = lv_btn_create(panel);
    theme::apply_button(play, true);
    lv_obj_set_size(play, 188, 44);
    lv_obj_align(play, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_add_event_cb(play, play_stop_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *play_label = lv_label_create(play);
    lv_label_set_text(play_label, "TOCAR / PARAR");
    lv_obj_center(play_label);

    s_status_timer = lv_timer_create(status_timer_cb, 250, nullptr);
    navigation::attach(screen);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
}

} // namespace
} // namespace audio_player
} // namespace ui

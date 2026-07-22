#include "audio_beep.h"

#include "audio_output.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cmath>
#include <cstdint>

namespace {

static const char *TAG = "audio_beep";

constexpr uint32_t kSampleRate = 16000;
constexpr size_t kFramesPerWrite = 256;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kShiftBeepFrequency = 1300.0f;

static std::atomic_bool s_beep_running{false};

static void write_tone(float frequency_hz, uint32_t duration_ms)
{
    int16_t samples[kFramesPerWrite * 2] = {};
    const size_t total_frames = (kSampleRate * duration_ms) / 1000;
    size_t frame = 0;

    while(frame < total_frames) {
        const size_t frames = (total_frames - frame < kFramesPerWrite)
            ? total_frames - frame
            : kFramesPerWrite;

        for(size_t i = 0; i < frames; ++i) {
            const float t = static_cast<float>(frame + i) /
                            static_cast<float>(kSampleRate);
            const int16_t sample = static_cast<int16_t>(
                std::sin(2.0f * kPi * frequency_hz * t) * 7000.0f);

            // The amplifier is mono, but the I2S stream is sent in stereo so
            // either channel-selection configuration of the NS4168 can hear it.
            samples[2 * i] = sample;
            samples[2 * i + 1] = sample;
        }

        audio_output::write(samples, frames * 2);
        frame += frames;
    }
}

static void write_silence(uint32_t duration_ms)
{
    int16_t silence[kFramesPerWrite * 2] = {};
    const size_t total_frames = (kSampleRate * duration_ms) / 1000;
    size_t frame = 0;

    while(frame < total_frames) {
        const size_t frames = (total_frames - frame < kFramesPerWrite)
            ? total_frames - frame
            : kFramesPerWrite;
        audio_output::write(silence, frames * 2);
        frame += frames;
    }
}

static void beep_task(void *)
{
    if(audio_output::prepare_effect(kSampleRate) == ESP_OK) {
        // "bip, bip bip": tones a little longer, with shorter pauses so the
        // warning is more noticeable without making the sequence sluggish.
        write_tone(kShiftBeepFrequency, 150);
        vTaskDelay(pdMS_TO_TICKS(95));
        write_tone(kShiftBeepFrequency, 110);
        vTaskDelay(pdMS_TO_TICKS(45));
        write_tone(kShiftBeepFrequency, 150);
        write_silence(30);
    } else {
        ESP_LOGE(TAG, "Nao foi possivel inicializar o I2S do alto-falante");
    }

    audio_output::finish_effect();
    s_beep_running.store(false);
    vTaskDelete(nullptr);
}

} // namespace

void audio_beep_play_shift()
{
    if(audio_output::music_active()) return;
    bool expected = false;
    if(!s_beep_running.compare_exchange_strong(expected, true)) return;

    if(xTaskCreate(beep_task, "shift_beep", 3072, nullptr, 4, nullptr) != pdPASS) {
        s_beep_running.store(false);
        ESP_LOGE(TAG, "Nao foi possivel criar a tarefa do beep");
    }
}

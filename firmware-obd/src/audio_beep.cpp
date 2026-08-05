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

static std::atomic_bool s_beepRunning{false};

void writeTone(float frequencyHz, uint32_t durationMs)
{
    int16_t samples[kFramesPerWrite * 2] = {};
    const size_t totalFrames = (kSampleRate * durationMs) / 1000;
    size_t frame = 0;

    while(frame < totalFrames) {
        const size_t frames = (totalFrames - frame < kFramesPerWrite)
            ? totalFrames - frame : kFramesPerWrite;
        for(size_t i = 0; i < frames; ++i) {
            const float t = static_cast<float>(frame + i) / static_cast<float>(kSampleRate);
            const int16_t sample = static_cast<int16_t>(
                std::sin(2.0f * kPi * frequencyHz * t) * 7000.0f);
            samples[2 * i] = sample;
            samples[2 * i + 1] = sample;
        }
        audio_output::write(samples, frames * 2);
        frame += frames;
    }
}

void writeSilence(uint32_t durationMs)
{
    int16_t silence[kFramesPerWrite * 2] = {};
    const size_t totalFrames = (kSampleRate * durationMs) / 1000;
    size_t frame = 0;
    while(frame < totalFrames) {
        const size_t frames = (totalFrames - frame < kFramesPerWrite)
            ? totalFrames - frame : kFramesPerWrite;
        audio_output::write(silence, frames * 2);
        frame += frames;
    }
}

void beepTask(void *)
{
    if(audio_output::prepare_effect(kSampleRate) == ESP_OK) {
        writeTone(kShiftBeepFrequency, 150);
        vTaskDelay(pdMS_TO_TICKS(95));
        writeTone(kShiftBeepFrequency, 110);
        vTaskDelay(pdMS_TO_TICKS(45));
        writeTone(kShiftBeepFrequency, 150);
        writeSilence(30);
    } else {
        ESP_LOGE(TAG, "Nao foi possivel inicializar o I2S do alto-falante");
    }

    audio_output::finish_effect();
    s_beepRunning.store(false);
    vTaskDelete(nullptr);
}

} // namespace

void audio_beep_play_shift()
{
    bool expected = false;
    if(!s_beepRunning.compare_exchange_strong(expected, true)) return;
    if(xTaskCreate(beepTask, "shift_beep", 3072, nullptr, 4, nullptr) != pdPASS) {
        s_beepRunning.store(false);
        ESP_LOGE(TAG, "Nao foi possivel criar a tarefa do beep");
    }
}

#include "audio_beep.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cmath>
#include <cstdint>

namespace {

static const char *TAG = "audio_beep";

// Audio wiring of the on-board NS4168 amplifier and Speak connector.
constexpr gpio_num_t kI2sBclk = GPIO_NUM_42;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_2;
constexpr gpio_num_t kI2sData = GPIO_NUM_41;
constexpr uint32_t kSampleRate = 16000;
constexpr size_t kFramesPerWrite = 256;
constexpr float kPi = 3.14159265358979323846f;

static i2s_chan_handle_t s_tx_channel = nullptr;
static std::atomic_bool s_beep_running{false};

static esp_err_t init_i2s()
{
    if(s_tx_channel != nullptr) return ESP_OK;

    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_config.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_config, &s_tx_channel, nullptr);
    if(err != ESP_OK) return err;

    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = kI2sBclk,
            .ws = kI2sLrclk,
            .dout = kI2sData,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_tx_channel, &std_config);
    if(err != ESP_OK) return err;

    return i2s_channel_enable(s_tx_channel);
}

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

        size_t bytes_written = 0;
        i2s_channel_write(s_tx_channel, samples, frames * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
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
        size_t bytes_written = 0;
        i2s_channel_write(s_tx_channel, silence, frames * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        frame += frames;
    }
}

static void beep_task(void *)
{
    if(init_i2s() == ESP_OK) {
        // "bip, bip bip": one longer beep followed by two short beeps.
        write_tone(1100.0f, 100);
        vTaskDelay(pdMS_TO_TICKS(150));
        write_tone(1100.0f, 75);
        vTaskDelay(pdMS_TO_TICKS(80));
        write_tone(1100.0f, 110);
        write_silence(30);
    } else {
        ESP_LOGE(TAG, "Nao foi possivel inicializar o I2S do alto-falante");
    }

    s_beep_running.store(false);
    vTaskDelete(nullptr);
}

} // namespace

void audio_beep_play_shift()
{
    bool expected = false;
    if(!s_beep_running.compare_exchange_strong(expected, true)) return;

    if(xTaskCreate(beep_task, "shift_beep", 3072, nullptr, 4, nullptr) != pdPASS) {
        s_beep_running.store(false);
        ESP_LOGE(TAG, "Nao foi possivel criar a tarefa do beep");
    }
}

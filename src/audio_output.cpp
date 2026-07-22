#include "audio_output.h"

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"

#include <atomic>

namespace audio_output {
namespace {

constexpr gpio_num_t kI2sBclk = GPIO_NUM_42;
constexpr gpio_num_t kI2sLrclk = GPIO_NUM_2;
constexpr gpio_num_t kI2sData = GPIO_NUM_41;

i2s_chan_handle_t s_tx_channel = nullptr;
uint32_t s_sample_rate = 0;
std::atomic_bool s_music_active{false};
std::atomic_bool s_effect_active{false};

esp_err_t init_channel(uint32_t sample_rate)
{
    if(s_tx_channel != nullptr) {
        return ESP_OK;
    }

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
    esp_err_t err = i2s_new_channel(&channel_config, &s_tx_channel, nullptr);
    if(err != ESP_OK) {
        return err;
    }

    i2s_std_config_t config = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
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

    err = i2s_channel_init_std_mode(s_tx_channel, &config);
    if(err != ESP_OK) {
        return err;
    }
    err = i2s_channel_enable(s_tx_channel);
    if(err == ESP_OK) {
        s_sample_rate = sample_rate;
    }
    return err;
}

esp_err_t set_sample_rate(uint32_t sample_rate)
{
    esp_err_t err = init_channel(sample_rate);
    if(err != ESP_OK || s_sample_rate == sample_rate) {
        return err;
    }

    err = i2s_channel_disable(s_tx_channel);
    if(err != ESP_OK) {
        return err;
    }
    i2s_std_clk_config_t clock_config = {
        .sample_rate_hz = sample_rate,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .ext_clk_freq_hz = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    err = i2s_channel_reconfig_std_clock(s_tx_channel, &clock_config);
    if(err == ESP_OK) {
        err = i2s_channel_enable(s_tx_channel);
    }
    if(err == ESP_OK) {
        s_sample_rate = sample_rate;
    }
    return err;
}

} // namespace

esp_err_t start_music(uint32_t sample_rate)
{
    if(s_music_active.load() || s_effect_active.load()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = set_sample_rate(sample_rate);
    if(err == ESP_OK) {
        s_music_active.store(true);
    }
    return err;
}

void stop_music()
{
    s_music_active.store(false);
}

bool music_active()
{
    return s_music_active.load();
}

esp_err_t prepare_effect(uint32_t sample_rate)
{
    if(s_music_active.load() || s_effect_active.load()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = set_sample_rate(sample_rate);
    if(err == ESP_OK) {
        s_effect_active.store(true);
    }
    return err;
}

void finish_effect()
{
    s_effect_active.store(false);
}

esp_err_t write(const int16_t *samples, size_t sample_count)
{
    if(s_tx_channel == nullptr || samples == nullptr || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bytes_written = 0;
    return i2s_channel_write(s_tx_channel, samples, sample_count * sizeof(int16_t),
                             &bytes_written, portMAX_DELAY);
}

} // namespace audio_output

#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

// Shared access to the NS4168 speaker amplifier on the JC3248W535 board.
// Music playback owns the output while active, so alert beeps do not corrupt
// the audio stream.
namespace audio_output {

esp_err_t start_music(uint32_t sample_rate);
void stop_music();
bool music_active();
esp_err_t prepare_effect(uint32_t sample_rate);
void finish_effect();
esp_err_t write(const int16_t *samples, size_t sample_count);

} // namespace audio_output

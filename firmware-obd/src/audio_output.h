#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

// Acesso ao amplificador NS4168 da placa JC3248W535.
namespace audio_output {

esp_err_t prepare_effect(uint32_t sample_rate);
void finish_effect();
esp_err_t write(const int16_t *samples, size_t sample_count);

} // namespace audio_output

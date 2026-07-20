#pragma once

#include <stddef.h>

namespace ui {
namespace preferences {

void init();

int palette_index();
void set_palette_index(int index);

int brightness();
void set_brightness(int value);

const char *boot_image_path();
const char *logo_image_path();
void set_boot_image_path(const char *path);
void set_logo_image_path(const char *path);

} // namespace preferences
} // namespace ui

#pragma once

#include <stddef.h>
#include <stdbool.h>

namespace ui {
namespace sd_browser {

constexpr size_t kMaxMediaFiles = 64;
constexpr size_t kMaxPathLength = 320;
constexpr size_t kMaxNameLength = 256;

enum class MediaType {
    Gif,
    Mjpeg,
    Mp4,
    Unknown,
};

struct MediaFile {
    char name[kMaxNameLength];
    char path[kMaxPathLength];
    char lvgl_path[kMaxPathLength];
    MediaType type;
};

struct MediaCatalog {
    MediaFile files[kMaxMediaFiles];
    size_t count;
    bool sd_ready;
    bool directory_found;
};

constexpr size_t kMaxImageFiles = 32;

struct ImageFile {
    char name[kMaxNameLength];
    char path[kMaxPathLength];
    char lvgl_path[kMaxPathLength];
};

struct ImageCatalog {
    ImageFile files[kMaxImageFiles];
    size_t count;
    bool sd_ready;
};

bool mount();
bool is_mounted();
bool file_exists(const char *path);
void scan_media(MediaCatalog *catalog);
void scan_images(ImageCatalog *catalog);
const char *media_type_label(MediaType type);
bool media_type_playable(MediaType type);

} // namespace sd_browser
} // namespace ui

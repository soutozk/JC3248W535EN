#include "sd_browser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace ui {
namespace sd_browser {

static const char *TAG = "ui_sd";
static const char *kMountPoint = "/sd";
static const char *kMediaDir = "/sd/gifs";
static const char *kAudioDir = "/sd/music";
static const char *kImageDirs[] = {"/sd", "/sd/intro", "/sd/backgrounds"};

static bool s_mounted = false;
static sdmmc_card_t *s_card = nullptr;

static bool has_extension(const char *name, const char *extension)
{
    size_t name_len = strlen(name);
    size_t ext_len = strlen(extension);
    if(name_len <= ext_len) {
        return false;
    }

    const char *tail = name + name_len - ext_len;
    for(size_t i = 0; i < ext_len; ++i) {
        if(tolower(static_cast<unsigned char>(tail[i])) != tolower(static_cast<unsigned char>(extension[i]))) {
            return false;
        }
    }
    return true;
}

static MediaType detect_type(const char *name)
{
    if(has_extension(name, ".gif")) {
        return MediaType::Gif;
    }
    if(has_extension(name, ".mjpeg") || has_extension(name, ".mjpg")) {
        return MediaType::Mjpeg;
    }
    if(has_extension(name, ".mp4")) {
        return MediaType::Mp4;
    }
    return MediaType::Unknown;
}

static AudioType detect_audio_type(const char *name)
{
    if(has_extension(name, ".wav")) {
        return AudioType::Wav;
    }
    if(has_extension(name, ".mp3")) {
        return AudioType::Mp3;
    }
    return AudioType::Unknown;
}

static void make_lvgl_path(const char *posix_path, char *out, size_t out_len)
{
    if(strncmp(posix_path, kMountPoint, strlen(kMountPoint)) == 0) {
        snprintf(out, out_len, "S:%s", posix_path + strlen(kMountPoint));
    } else {
        snprintf(out, out_len, "S:%s", posix_path);
    }
}

static bool image_extension(const char *name)
{
    return has_extension(name, ".png");
}

static void scan_image_dir(const char *directory, ImageCatalog *catalog)
{
    if(catalog->count >= kMaxImageFiles) {
        return;
    }

    DIR *dir = opendir(directory);
    if(dir == nullptr) {
        return;
    }

    struct dirent *entry = nullptr;
    while((entry = readdir(dir)) != nullptr && catalog->count < kMaxImageFiles) {
        if(entry->d_name[0] == '.' || !image_extension(entry->d_name)) {
            continue;
        }

        ImageFile *file = &catalog->files[catalog->count];
        const char *relative_dir = directory + strlen(kMountPoint);
        if(relative_dir[0] == '/') {
            relative_dir++;
        }

        // Build the display name with explicit bounds so -Wformat-truncation
        // remains enabled for the firmware build.
        size_t name_offset = 0;
        if(relative_dir[0] != '\0') {
            size_t prefix_len = strlen(relative_dir);
            if(prefix_len > sizeof(file->name) - 2) {
                prefix_len = sizeof(file->name) - 2;
            }
            memcpy(file->name, relative_dir, prefix_len);
            name_offset = prefix_len;
            file->name[name_offset++] = '/';
        }
        size_t entry_len = strlen(entry->d_name);
        size_t available = sizeof(file->name) - name_offset - 1;
        if(entry_len > available) {
            entry_len = available;
        }
        memcpy(file->name + name_offset, entry->d_name, entry_len);
        file->name[name_offset + entry_len] = '\0';
        snprintf(file->path, sizeof(file->path), "%s/%s", directory, entry->d_name);
        make_lvgl_path(file->path, file->lvgl_path, sizeof(file->lvgl_path));
        catalog->count++;
    }

    closedir(dir);
}

bool mount()
{
    if(s_mounted) {
        return true;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = GPIO_NUM_12;
    slot_config.cmd = GPIO_NUM_11;
    slot_config.d0 = GPIO_NUM_13;
    slot_config.d1 = GPIO_NUM_NC;
    slot_config.d2 = GPIO_NUM_NC;
    slot_config.d3 = GPIO_NUM_NC;
    slot_config.d4 = GPIO_NUM_NC;
    slot_config.d5 = GPIO_NUM_NC;
    slot_config.d6 = GPIO_NUM_NC;
    slot_config.d7 = GPIO_NUM_NC;
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 6;
    mount_config.allocation_unit_size = 16 * 1024;
    mount_config.disk_status_check_enable = false;

    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &s_card);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed at %s: %s", kMountPoint, esp_err_to_name(err));
        s_card = nullptr;
        s_mounted = false;
        return false;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD mounted at %s", kMountPoint);
    sdmmc_card_print_info(stdout, s_card);
    return true;
}

bool is_mounted()
{
    return s_mounted;
}

bool file_exists(const char *path)
{
    struct stat st;
    return path != nullptr && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void scan_media(MediaCatalog *catalog)
{
    if(catalog == nullptr) {
        return;
    }

    memset(catalog, 0, sizeof(MediaCatalog));
    catalog->sd_ready = s_mounted;
    if(!s_mounted) {
        return;
    }

    DIR *dir = opendir(kMediaDir);
    if(dir == nullptr) {
        ESP_LOGW(TAG, "Media directory not found: %s", kMediaDir);
        return;
    }

    catalog->directory_found = true;

    struct dirent *entry = nullptr;
    while((entry = readdir(dir)) != nullptr && catalog->count < kMaxMediaFiles) {
        if(entry->d_name[0] == '.') {
            continue;
        }

        MediaType type = detect_type(entry->d_name);
        if(type == MediaType::Unknown) {
            continue;
        }

        MediaFile *file = &catalog->files[catalog->count];
        snprintf(file->name, sizeof(file->name), "%s", entry->d_name);
        snprintf(file->path, sizeof(file->path), "%s/%s", kMediaDir, entry->d_name);
        make_lvgl_path(file->path, file->lvgl_path, sizeof(file->lvgl_path));
        file->type = type;
        catalog->count++;
    }

    closedir(dir);
    ESP_LOGI(TAG, "Found %u media files", static_cast<unsigned>(catalog->count));
}

void scan_images(ImageCatalog *catalog)
{
    if(catalog == nullptr) {
        return;
    }

    memset(catalog, 0, sizeof(ImageCatalog));
    catalog->sd_ready = s_mounted;
    if(!s_mounted) {
        return;
    }

    for(const char *directory : kImageDirs) {
        scan_image_dir(directory, catalog);
    }
}

void scan_audio(AudioCatalog *catalog)
{
    if(catalog == nullptr) {
        return;
    }

    memset(catalog, 0, sizeof(AudioCatalog));
    catalog->sd_ready = s_mounted;
    if(!s_mounted) {
        return;
    }

    DIR *dir = opendir(kAudioDir);
    if(dir == nullptr) {
        ESP_LOGW(TAG, "Audio directory not found: %s", kAudioDir);
        return;
    }
    catalog->directory_found = true;

    struct dirent *entry = nullptr;
    while((entry = readdir(dir)) != nullptr && catalog->count < kMaxAudioFiles) {
        if(entry->d_name[0] == '.') {
            continue;
        }
        AudioType type = detect_audio_type(entry->d_name);
        if(type == AudioType::Unknown) {
            continue;
        }

        AudioFile *file = &catalog->files[catalog->count];
        snprintf(file->name, sizeof(file->name), "%s", entry->d_name);
        snprintf(file->path, sizeof(file->path), "%s/%s", kAudioDir, entry->d_name);
        file->type = type;
        catalog->count++;
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %u audio files", static_cast<unsigned>(catalog->count));
}

const char *media_type_label(MediaType type)
{
    switch(type) {
    case MediaType::Gif:
        return "GIF";
    case MediaType::Mjpeg:
        return "MJPEG";
    case MediaType::Mp4:
        return "MP4";
    default:
        return "FILE";
    }
}

bool media_type_playable(MediaType type)
{
    return type == MediaType::Gif;
}

const char *audio_type_label(AudioType type)
{
    switch(type) {
    case AudioType::Wav:
        return "WAV";
    case AudioType::Mp3:
        return "MP3";
    default:
        return "FILE";
    }
}

bool audio_type_playable(AudioType type)
{
    return type == AudioType::Wav || type == AudioType::Mp3;
}

} // namespace sd_browser
} // namespace ui

#ifndef SPOTIFY_BLE_H
#define SPOTIFY_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPOTIFY_COVER_SIZE (300)

/**
 * @brief Initialize the NimBLE Bluetooth stack and register the Spotify control service.
 */
void spotify_ble_init(void);

/**
 * @brief Check if a client (phone) is currently connected.
 */
bool spotify_ble_is_connected(void);

/**
 * @brief Get the current song title.
 * @param buf Buffer to store the title (null-terminated).
 * @param max_len Maximum length of the buffer.
 */
void spotify_ble_get_title(char *buf, size_t max_len);

/**
 * @brief Get the current artist name.
 * @param buf Buffer to store the artist (null-terminated).
 * @param max_len Maximum length of the buffer.
 */
void spotify_ble_get_artist(char *buf, size_t max_len);

/**
 * @brief Get the pointer to the raw RGB565 cover art image buffer (300x300 pixels).
 * @return Const pointer to the RGB565 byte array (size is 300 * 300 * 2 bytes).
 */
const uint8_t* spotify_ble_get_cover_data(void);

/**
 * @brief Check if a new cover image has been successfully decompressed since the last check.
 */
bool spotify_ble_has_new_cover(void);

/**
 * @brief Clear the new cover notification flag.
 */
void spotify_ble_clear_new_cover_flag(void);

/**
 * @brief Send a "Next Song" command notification to the connected client.
 */
void spotify_ble_send_next(void);

/**
 * @brief Send a "Previous Song" command notification to the connected client.
 */
void spotify_ble_send_prev(void);

/**
 * @brief Send a "Play/Pause" command notification to the connected client.
 */
void spotify_ble_send_play_pause(void);

#ifdef __cplusplus
}
#endif

#endif // SPOTIFY_BLE_H

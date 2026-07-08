#include "spotify_ble.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "rom/tjpgd.h"

#include <string.h>
#include "nvs_flash.h"

extern "C" void ble_store_config_init(void);

static const char *TAG = "SPOTIFY_BLE";

// Custom UUIDs
// Service: F38A0001-82EB-4A73-A38C-CE98C9438012
static const ble_uuid128_t spotify_svc_uuid =
    BLE_UUID128_INIT(0x12, 0x80, 0x43, 0xc9, 0x98, 0xce, 0x8c, 0xa3, 0x73, 0x4a, 0xeb, 0x82, 0x01, 0x00, 0x8a, 0xf3);

// Track Metadata: F38A0002-82EB-4A73-A38C-CE98C9438012
static const ble_uuid128_t track_chr_uuid =
    BLE_UUID128_INIT(0x12, 0x80, 0x43, 0xc9, 0x98, 0xce, 0x8c, 0xa3, 0x73, 0x4a, 0xeb, 0x82, 0x02, 0x00, 0x8a, 0xf3);

// Playback Control: F38A0003-82EB-4A73-A38C-CE98C9438012
static const ble_uuid128_t control_chr_uuid =
    BLE_UUID128_INIT(0x12, 0x80, 0x43, 0xc9, 0x98, 0xce, 0x8c, 0xa3, 0x73, 0x4a, 0xeb, 0x82, 0x03, 0x00, 0x8a, 0xf3);

// Cover Size: F38A0004-82EB-4A73-A38C-CE98C9438012
static const ble_uuid128_t cover_size_chr_uuid =
    BLE_UUID128_INIT(0x12, 0x80, 0x43, 0xc9, 0x98, 0xce, 0x8c, 0xa3, 0x73, 0x4a, 0xeb, 0x82, 0x04, 0x00, 0x8a, 0xf3);

// Cover Data: F38A0005-82EB-4A73-A38C-CE98C9438012
static const ble_uuid128_t cover_data_chr_uuid =
    BLE_UUID128_INIT(0x12, 0x80, 0x43, 0xc9, 0x98, 0xce, 0x8c, 0xa3, 0x73, 0x4a, 0xeb, 0x82, 0x05, 0x00, 0x8a, 0xf3);

static uint16_t control_val_handle;
static bool s_ble_connected = false;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static const uint32_t MAX_COVER_JPEG_SIZE = 512 * 1024;

// Forward declaration of GATT access callback
static int spotify_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_cb(struct ble_gap_event *event, void *arg);

// GATT Service definition
static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid = &track_chr_uuid.u,
        .access_cb = spotify_gatt_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
    {
        .uuid = &control_chr_uuid.u,
        .access_cb = spotify_gatt_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &control_val_handle,
        .cpfd = NULL,
    },
    {
        .uuid = &cover_size_chr_uuid.u,
        .access_cb = spotify_gatt_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
    {
        .uuid = &cover_data_chr_uuid.u,
        .access_cb = spotify_gatt_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
    {
        .uuid = NULL,
        .access_cb = NULL,
        .arg = NULL,
        .descriptors = NULL,
        .flags = 0,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
    },
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &spotify_svc_uuid.u,
        .includes = NULL,
        .characteristics = gatt_chrs,
    },
    {
        .type = 0,
        .uuid = NULL,
        .includes = NULL,
        .characteristics = NULL,
    },
};

// Shared state with Mutex protection
static SemaphoreHandle_t s_spotify_mutex = NULL;
static char s_track_title[128] = "Sem Musica";
static char s_track_artist[128] = "Desconectado";
static uint8_t *s_cover_rgb565_buf = NULL; // 300x300x2 bytes
static bool s_has_new_cover = false;

// JPEG Reception state
static uint8_t *s_jpeg_input_buf = NULL;
static uint32_t s_jpeg_input_len = 0;
static uint32_t s_jpeg_received_len = 0;

// Structs for JPEG input callback
struct JpegInputSource {
    const uint8_t *data;
    uint32_t len;
    uint32_t offset;
};

struct JpegDecodeContext {
    JpegInputSource input;
    uint8_t *output_rgb565;
};

// --- TJpgDec Callback Functions ---

static UINT tjpgd_input_cb(JDEC *decoder, BYTE *buf, UINT len)
{
    JpegDecodeContext *ctx = (JpegDecodeContext *)decoder->device;
    JpegInputSource *source = &ctx->input;
    if (source->offset >= source->len) {
        return 0;
    }
    UINT to_read = len;
    if (source->offset + to_read > source->len) {
        to_read = source->len - source->offset;
    }
    if (buf) {
        memcpy(buf, source->data + source->offset, to_read);
    }
    source->offset += to_read;
    return to_read;
}

static UINT tjpgd_output_cb(JDEC *decoder, void *bitmap, JRECT *rect)
{
    JpegDecodeContext *ctx = (JpegDecodeContext *)decoder->device;
    uint16_t *dst = (uint16_t *)ctx->output_rgb565;
    uint16_t *src = (uint16_t *)bitmap;
    int dst_w = SPOTIFY_COVER_SIZE; // 300
    
    int rect_w = rect->right - rect->left + 1;
    int rect_h = rect->bottom - rect->top + 1;
    
    for (int y = 0; y < rect_h; y++) {
        for (int x = 0; x < rect_w; x++) {
            int out_x = rect->left + x;
            int out_y = rect->top + y;
            if (out_x < dst_w && out_y < dst_w) {
                uint16_t color = src[y * rect_w + x];
                // Swap bytes for LVGL big-endian compatibility
                uint16_t swapped = (color >> 8) | (color << 8);
                dst[out_y * dst_w + out_x] = swapped;
            }
        }
    }
    return 1; // Continue decompression
}

// Function to decode JPEG bytes into raw RGB565 in s_cover_rgb565_buf
static void decode_jpeg(const uint8_t *jpeg_data, uint32_t jpeg_len)
{
    if (s_cover_rgb565_buf == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Decoding JPEG image (%ld bytes)...", jpeg_len);

    // TJpgDec requires ~3100 bytes work buffer
    const size_t work_buf_size = 4096;
    void *work_buf = malloc(work_buf_size);
    if (!work_buf) {
        ESP_LOGE(TAG, "Failed to allocate TJpgDec work buffer");
        return;
    }

    JpegDecodeContext decode_ctx;
    decode_ctx.input.data = jpeg_data;
    decode_ctx.input.len = jpeg_len;
    decode_ctx.input.offset = 0;
    decode_ctx.output_rgb565 = s_cover_rgb565_buf;

    JDEC decoder;
    JRESULT res = jd_prepare(&decoder, tjpgd_input_cb, work_buf, work_buf_size, &decode_ctx);
    if (res == JDR_OK) {
        // Run decompressor, scale factor 0 (raw size).
        res = jd_decomp(&decoder, tjpgd_output_cb, 0);
        if (res == JDR_OK) {
            ESP_LOGI(TAG, "JPEG decoded successfully to 300x300 RGB565");
            if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
                s_has_new_cover = true;
                xSemaphoreGive(s_spotify_mutex);
            }
        } else {
            ESP_LOGE(TAG, "jd_decomp failed with code: %d", res);
        }
    } else {
        ESP_LOGE(TAG, "jd_prepare failed with code: %d", res);
    }

    free(work_buf);
}

// --- GATT Callback Handler ---

static int spotify_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &control_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            uint8_t idle_cmd = 0;
            return os_mbuf_append(ctxt->om, &idle_cmd, sizeof(idle_cmd)) == 0
                ? 0
                : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return 0;
    }

    if (ble_uuid_cmp(uuid, &track_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            char temp_buf[256];
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len >= sizeof(temp_buf)) {
                len = sizeof(temp_buf) - 1;
            }
            int rc = ble_hs_mbuf_to_flat(ctxt->om, temp_buf, len, NULL);
            if (rc == 0) {
                temp_buf[len] = '\0';
                
                // Track metadata is sent in "Title;Artist" format
                char *semicolon = strchr(temp_buf, ';');
                if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
                    if (semicolon != NULL) {
                        *semicolon = '\0';
                        strncpy(s_track_title, temp_buf, sizeof(s_track_title) - 1);
                        strncpy(s_track_artist, semicolon + 1, sizeof(s_track_artist) - 1);
                    } else {
                        strncpy(s_track_title, temp_buf, sizeof(s_track_title) - 1);
                        strcpy(s_track_artist, "Desconhecido");
                    }
                    s_track_title[sizeof(s_track_title) - 1] = '\0';
                    s_track_artist[sizeof(s_track_artist) - 1] = '\0';
                    xSemaphoreGive(s_spotify_mutex);
                }
                ESP_LOGI(TAG, "Received track update: %s by %s", s_track_title, s_track_artist);
            }
        }
        return 0;
    }

    if (ble_uuid_cmp(uuid, &cover_size_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint8_t raw_size[4] = {0};
            int rc = ble_hs_mbuf_to_flat(ctxt->om, raw_size, sizeof(raw_size), NULL);
            if (rc == 0) {
                uint32_t size_le = ((uint32_t)raw_size[0]) |
                                   ((uint32_t)raw_size[1] << 8) |
                                   ((uint32_t)raw_size[2] << 16) |
                                   ((uint32_t)raw_size[3] << 24);
                uint32_t size_be = ((uint32_t)raw_size[0] << 24) |
                                   ((uint32_t)raw_size[1] << 16) |
                                   ((uint32_t)raw_size[2] << 8) |
                                   ((uint32_t)raw_size[3]);
                uint32_t size = size_le;

                if (size == 0 || size > MAX_COVER_JPEG_SIZE) {
                    size = size_be;
                }

                if (size == 0 || size > MAX_COVER_JPEG_SIZE) {
                    ESP_LOGE(TAG, "Invalid cover image size: le=%ld be=%ld", size_le, size_be);
                    return BLE_ATT_ERR_UNLIKELY;
                }

                ESP_LOGI(TAG, "Expecting cover image of size: %ld bytes", size);
                
                if (s_jpeg_input_buf != NULL) {
                    free(s_jpeg_input_buf);
                }
                
                s_jpeg_input_buf = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
                if (s_jpeg_input_buf == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory in PSRAM for JPEG input (%ld bytes)", size);
                    s_jpeg_input_len = 0;
                } else {
                    s_jpeg_input_len = size;
                    s_jpeg_received_len = 0;
                }
            }
        }
        return 0;
    }

    if (ble_uuid_cmp(uuid, &cover_data_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (s_jpeg_input_buf != NULL && s_jpeg_received_len + len <= s_jpeg_input_len) {
                int rc = ble_hs_mbuf_to_flat(ctxt->om, s_jpeg_input_buf + s_jpeg_received_len, len, NULL);
                if (rc == 0) {
                    s_jpeg_received_len += len;
                    // ESP_LOGD(TAG, "Received chunk: %d bytes (%ld/%ld)", len, s_jpeg_received_len, s_jpeg_input_len);
                    if (s_jpeg_received_len >= s_jpeg_input_len) {
                        decode_jpeg(s_jpeg_input_buf, s_jpeg_input_len);
                        free(s_jpeg_input_buf);
                        s_jpeg_input_buf = NULL;
                        s_jpeg_input_len = 0;
                        s_jpeg_received_len = 0;
                    }
                }
            }
        }
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// --- GAP & BLE Server Setup ---

static void start_advertising(void)
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type: %d", rc);
        return;
    }

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &spotify_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields;
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.name = (uint8_t *)"CyberDeck_Spotify";
    rsp_fields.name_len = strlen("CyberDeck_Spotify");
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting scan response fields: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started as CyberDeck_Spotify");
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE Client Connected (status=%d)", event->connect.status);
            if (event->connect.status == 0) {
                s_ble_connected = true;
                s_conn_handle = event->connect.conn_handle;
            } else {
                // Resume advertising if connection failed
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE Client Disconnected (reason=%d)", event->disconnect.reason);
            s_ble_connected = false;
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            
            // Clean up JPEG buffers
            if (s_jpeg_input_buf != NULL) {
                free(s_jpeg_input_buf);
                s_jpeg_input_buf = NULL;
            }
            s_jpeg_input_len = 0;
            s_jpeg_received_len = 0;

            // Reset track details
            if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
                strcpy(s_track_title, "Sem Musica");
                strcpy(s_track_artist, "Desconectado");
                xSemaphoreGive(s_spotify_mutex);
            }

            // Restart advertising
            start_advertising();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete; reason=%d", event->adv_complete.reason);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Client subscribed to notifications");
            break;

        default:
            break;
    }
    return 0;
}

static void on_sync_cb(void)
{
    start_advertising();
}

static void on_reset_cb(int reason)
{
    ESP_LOGE(TAG, "BLE Host reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// --- Public APIs ---

void spotify_ble_init(void)
{
    if (s_spotify_mutex != NULL) {
        return; // Already initialized
    }

    // Initialize NVS flash for BLE bonding/storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS flash: %d", ret);
        return;
    }

    s_spotify_mutex = xSemaphoreCreateMutex();
    
    // Allocate RGB565 cover art buffer in PSRAM (300 x 300 x 2 bytes = 180KB)
    s_cover_rgb565_buf = (uint8_t *)heap_caps_malloc(SPOTIFY_COVER_SIZE * SPOTIFY_COVER_SIZE * 2, MALLOC_CAP_SPIRAM);
    if (s_cover_rgb565_buf != NULL) {
        // Set it to a default dark gray background pattern
        memset(s_cover_rgb565_buf, 0x1f, SPOTIFY_COVER_SIZE * SPOTIFY_COVER_SIZE * 2);
    } else {
        ESP_LOGE(TAG, "Failed to allocate s_cover_rgb565_buf in PSRAM!");
    }

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to init NimBLE port: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb = on_sync_cb;
    ble_hs_cfg.reset_cb = on_reset_cb;

    // Enable Security Manager (SM) bonding & secure connections for pairing
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT; // Just Works
    ble_hs_cfg.sm_bonding = 1;                         // Enable bonding to remember paired phone
    ble_hs_cfg.sm_mitm = 0;                            // MITM protection not required
    ble_hs_cfg.sm_sc = 1;                              // Enable LE Secure Connections
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Initialize NimBLE configuration store for storing bonding keys
    ble_store_config_init();

    // Define services and characteristics
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    // Set device name in GAP service
    rc = ble_svc_gap_device_name_set("CyberDeck_Spotify");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set GAP device name: %d", rc);
        return;
    }

    // Start NimBLE Host Task
    xTaskCreate(ble_host_task, "ble_host", 4096, NULL, 5, NULL);
}

bool spotify_ble_is_connected(void)
{
    return s_ble_connected;
}

void spotify_ble_get_title(char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0) return;
    if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
        strncpy(buf, s_track_title, max_len - 1);
        buf[max_len - 1] = '\0';
        xSemaphoreGive(s_spotify_mutex);
    }
}

void spotify_ble_get_artist(char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0) return;
    if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
        strncpy(buf, s_track_artist, max_len - 1);
        buf[max_len - 1] = '\0';
        xSemaphoreGive(s_spotify_mutex);
    }
}

const uint8_t* spotify_ble_get_cover_data(void)
{
    return s_cover_rgb565_buf;
}

bool spotify_ble_has_new_cover(void)
{
    bool val = false;
    if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
        val = s_has_new_cover;
        xSemaphoreGive(s_spotify_mutex);
    }
    return val;
}

void spotify_ble_clear_new_cover_flag(void)
{
    if (xSemaphoreTake(s_spotify_mutex, portMAX_DELAY) == pdTRUE) {
        s_has_new_cover = false;
        xSemaphoreGive(s_spotify_mutex);
    }
}

static void send_notification(uint8_t cmd)
{
    if (!s_ble_connected) {
        ESP_LOGW(TAG, "Cannot send command %d; BLE not connected", cmd);
        return;
    }
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&cmd, 1);
    if (om != NULL) {
        int rc = ble_gatts_notify_custom(s_conn_handle, control_val_handle, om);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to send notification (rc=%d)", rc);
        } else {
            ESP_LOGI(TAG, "Sent notification: command %d", cmd);
        }
    }
}

void spotify_ble_send_next(void)
{
    send_notification(1); // 1 = Next
}

void spotify_ble_send_prev(void)
{
    send_notification(2); // 2 = Prev
}

void spotify_ble_send_play_pause(void)
{
    send_notification(3); // 3 = Play/Pause
}

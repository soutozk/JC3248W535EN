#include "obd_ble.h"
#include "obd_protocol.h"
#include "obd_telemetry.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

namespace {
static const char *TAG = "OBD_BLE";
static uint8_t s_addrType;

static const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x12,0x80,0x43,0xc9,0x98,0xce,0x8c,0xa3,0x73,0x4a,0xeb,0x82,0x01,0x00,0x8a,0xf3);
static const ble_uuid128_t kTelemetryUuid = BLE_UUID128_INIT(
    0x12,0x80,0x43,0xc9,0x98,0xce,0x8c,0xa3,0x73,0x4a,0xeb,0x82,0x06,0x00,0x8a,0xf3);
static const ble_uuid128_t kStatusUuid = BLE_UUID128_INIT(
    0x12,0x80,0x43,0xc9,0x98,0xce,0x8c,0xa3,0x73,0x4a,0xeb,0x82,0x07,0x00,0x8a,0xf3);
static const ble_uuid128_t kDtcUuid = BLE_UUID128_INIT(
    0x12,0x80,0x43,0xc9,0x98,0xce,0x8c,0xa3,0x73,0x4a,0xeb,0x82,0x08,0x00,0x8a,0xf3);

static int accessCb(uint16_t, uint16_t attr, ble_gatt_access_ctxt *ctxt, void *)
{
    if(ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    const uint16_t length = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t buffer[sizeof(obd::protocol::DtcFrame)]{};
    if(length > sizeof(buffer) || ble_hs_mbuf_to_flat(ctxt->om, buffer, length, nullptr) != 0)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if(ble_uuid_cmp(ctxt->chr->uuid, &kTelemetryUuid.u) == 0) {
        obd::protocol::TelemetryFrame frame;
        if(!obd::protocol::validateTelemetry(buffer, length, frame) ||
           !obd::acceptTelemetryFrame(frame, now)) return BLE_ATT_ERR_UNLIKELY;
        return 0;
    }
    if(ble_uuid_cmp(ctxt->chr->uuid, &kStatusUuid.u) == 0) {
        obd::protocol::StatusFrame frame;
        if(!obd::protocol::validateStatus(buffer, length, frame) ||
           !obd::acceptStatusFrame(frame, now)) return BLE_ATT_ERR_UNLIKELY;
        return 0;
    }
    if(ble_uuid_cmp(ctxt->chr->uuid, &kDtcUuid.u) == 0) {
        obd::protocol::DtcFrame frame;
        if(!obd::protocol::validateDtc(buffer, length, frame) ||
           !obd::acceptDtcFrame(frame, now)) return BLE_ATT_ERR_UNLIKELY;
        return 0;
    }
    (void) attr;
    return BLE_ATT_ERR_UNLIKELY;
}

static const ble_gatt_chr_def kCharacteristics[] = {
    {.uuid=&kTelemetryUuid.u, .access_cb=accessCb,
     .flags=BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
    {.uuid=&kStatusUuid.u, .access_cb=accessCb,
     .flags=BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
    {.uuid=&kDtcUuid.u, .access_cb=accessCb,
     .flags=BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
    {0}
};
static const ble_gatt_svc_def kServices[] = {
    {.type=BLE_GATT_SVC_TYPE_PRIMARY, .uuid=&kServiceUuid.u, .characteristics=kCharacteristics},
    {0}
};

static void advertise();
static int gapCb(ble_gap_event *event, void *)
{
    switch(event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if(event->connect.status == 0) {
                obd::telemetrySetBleConnected(true);
                ESP_LOGI(TAG, "Android conectado");
            } else advertise();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            obd::telemetrySetBleConnected(false);
            advertise();
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
        default: break;
    }
    return 0;
}

static void advertise()
{
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t *>(&kServiceUuid);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if(rc != 0) {
        ESP_LOGE(TAG, "Falha nos dados de advertising: %d", rc);
        return;
    }

    // Nome + UUID + cabecalhos ultrapassam os 31 bytes de um unico pacote BLE.
    // O UUID fica no advertising e o nome completo segue na scan response.
    ble_hs_adv_fields response{};
    const char *name = ble_svc_gap_device_name();
    response.name = reinterpret_cast<const uint8_t *>(name);
    response.name_len = strlen(name);
    response.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if(rc != 0) {
        ESP_LOGE(TAG, "Falha na scan response: %d", rc);
        return;
    }
    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_addrType, nullptr, BLE_HS_FOREVER, &params, gapCb, nullptr);
    if(rc != 0) ESP_LOGE(TAG, "Falha ao anunciar BLE: %d", rc);
}

static void onSync()
{
    const int rc = ble_hs_id_infer_auto(0, &s_addrType);
    if(rc != 0) {
        ESP_LOGE(TAG, "Falha ao obter endereco BLE: %d", rc);
        return;
    }
    advertise();
}
static void hostTask(void *) { nimble_port_run(); nimble_port_freertos_deinit(); }
}

void obd_ble_init()
{
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }
    obd::telemetryInit();
    if(nimble_port_init() != ESP_OK) { ESP_LOGE(TAG, "Falha NimBLE"); return; }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("CyberDeck_OBD");
    ble_hs_cfg.sync_cb = onSync;
    if(ble_gatts_count_cfg(kServices) != 0 || ble_gatts_add_svcs(kServices) != 0) {
        ESP_LOGE(TAG, "Falha ao registrar GATT"); return;
    }
    nimble_port_freertos_init(hostTask);
}

#include "heart_service.h"

#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include "../event_handler.h"

#define HEART_ATTR_IDX_PACKET_VALUE 2
#define HEART_ATTR_IDX_ALERT_VALUE  5
#define HEART_ATTR_AUDIO_VAL 8
#define AUDIO_CHUNK_SIZE 244  // Max payload per audio notification


LOG_MODULE_REGISTER(heart_service);

static bool notify_enabled_packet = false;
static bool notify_enabled_alert = false;

static void packet_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_packet = (value == BT_GATT_CCC_NOTIFY);
}

static void alert_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_alert = (value == BT_GATT_CCC_NOTIFY);
}

//BLE audio transmit callback: 
// static void control_point_write_cb(struct bt_conn *conn,
// 	const struct bt_gatt_attr *attr,
// 	const void *buf, uint16_t len,
// 	uint16_t offset, uint8_t flags)
// {
static void control_point_write_cb(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf, uint16_t len,
	uint16_t offset, uint8_t flags)
{
	const uint8_t *cmd = buf;

	if (len < 1) return;

	switch (cmd[0]) {
		case 0x01: // Start recording
			AppEvent rec_ev = { .type = EVENT_BLE_RECORD};
			event_handler_post(rec_ev);   
		break;

		case 0x02: // Start upload
			AppEvent up_ev = { .type = EVENT_BLE_TRANSMIT};
			event_handler_post(up_ev); 
		break;

		default:
			LOG_ERR("Unknown BLE control opcode: 0x%02X\n", cmd[0]);
		break;
	}
}


BT_GATT_SERVICE_DEFINE(heart_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HEART_SERVICE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HEART_PACKET,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(packet_ccc_cfg_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HEART_ALERT,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(alert_ccc_cfg_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HEART_AUDIO,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	
	BT_GATT_CHARACTERISTIC(BT_UUID_HEART_CONTROL,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE,
		NULL, control_point_write_cb, NULL),
);

//@TO-DO Register callbacks here if needed
int bt_heart_service_init()
{
	LOG_INF("Heart Service initialized");
	return 0;
}

int bt_heart_service_notify_packet(const struct heart_packet *pkt)
{
	if (!notify_enabled_packet) {
		LOG_ERR("Heart Packet Failed to send");
		return -EACCES;
	}
    const struct bt_gatt_attr *packet_attr = &heart_svc.attrs[HEART_ATTR_IDX_PACKET_VALUE];


	return bt_gatt_notify(NULL, packet_attr, pkt, sizeof(*pkt));
}

int bt_heart_service_notify_alert(uint8_t code)
{
	if (!notify_enabled_alert) {
		LOG_ERR("Alert Code Failed to send");
		return -EACCES;
	}

    const struct bt_gatt_attr *alert_attr  = &heart_svc.attrs[HEART_ATTR_IDX_ALERT_VALUE];

	return bt_gatt_notify(NULL, alert_attr, &code, sizeof(code));
}

int transmit_audio_buffer(const uint8_t *buffer, size_t length)
{
	if (!buffer || length == 0) {
		return -EINVAL;
	}

	for (size_t offset = 0; offset < length; offset += AUDIO_CHUNK_SIZE) {
		uint16_t chunk_size = MIN(AUDIO_CHUNK_SIZE, length - offset);
		int err = bt_gatt_notify(NULL, &heart_svc.attrs[HEART_ATTR_AUDIO_VAL],
		                         &buffer[offset], chunk_size);

		if (err) {
			printk("BLE notify failed at offset %d: %d\n", offset, err);
			return err;
		}

		k_sleep(K_MSEC(6)); // BLE buffer pacing
	}

	return 0;
}


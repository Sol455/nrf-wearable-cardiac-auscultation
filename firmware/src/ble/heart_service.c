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
static bool notify_enabled_audio = false;
static struct bt_heart_service_cb registered_callbacks;

static void packet_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_packet = (value == BT_GATT_CCC_NOTIFY);
}

static void alert_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_alert = (value == BT_GATT_CCC_NOTIFY);
}

static void alert_audio_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_audio = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t control_point_write_cb(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf, uint16_t len,
	uint16_t offset, uint8_t flags)
{
	if (len < 1) return 0;

	const uint8_t *cmd = buf;

	if (registered_callbacks.run_on_control_command) {
		registered_callbacks.run_on_control_command(cmd[0]);
	}

	return len;
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
	BT_GATT_CCC(alert_audio_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	
	BT_GATT_CHARACTERISTIC(BT_UUID_HEART_CONTROL,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE,
		NULL, control_point_write_cb, NULL),
);

int bt_heart_service_init(const struct bt_heart_service_cb *callbacks)
{
	if (!callbacks || !callbacks->run_on_control_command) {
		return -EINVAL;
	}
	registered_callbacks = *callbacks; 
	LOG_INF("Heart Service initialized");
	return 0;
}

int bt_heart_service_notify_packet(const struct heart_packet *pkt)
{
	if (!notify_enabled_packet) {
		LOG_ERR("Heart Packet Failed to send: not enabled");
		return -EACCES;
	}
    const struct bt_gatt_attr *packet_attr = &heart_svc.attrs[HEART_ATTR_IDX_PACKET_VALUE];


	return bt_gatt_notify(NULL, packet_attr, pkt, sizeof(*pkt));
}

int bt_heart_service_notify_alert(uint8_t code)
{
	if (!notify_enabled_alert) {
		LOG_ERR("Alert Code Failed to send: not enabled");
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

	if (!notify_enabled_audio) {
		LOG_ERR("Audio Code Failed to send: not enabled");
		return -EACCES;
	}

	LOG_INF("Audio notify enabled: %d", notify_enabled_audio);

	for (size_t offset = 0; offset < length; offset += AUDIO_CHUNK_SIZE) {
		uint16_t chunk_size = MIN(AUDIO_CHUNK_SIZE, length - offset);
		LOG_INF("Notifying offset %d / %d", offset, length);
		int err = bt_gatt_notify(NULL, &heart_svc.attrs[HEART_ATTR_AUDIO_VAL],
		                         &buffer[offset], chunk_size);

		if (err) {
			LOG_ERR("BLE notify failed at offset %d: %d\n", offset, err);
			return err;
		}
		LOG_INF("sent packet %d", offset);
		k_sleep(K_MSEC(6)); // BLE buffer pacing
	}

	return 0;
}


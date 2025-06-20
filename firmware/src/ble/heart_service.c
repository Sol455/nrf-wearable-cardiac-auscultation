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

uint8_t dummy[244] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3
};

	bt_gatt_notify(NULL, &heart_svc.attrs[HEART_ATTR_AUDIO_VAL], dummy, sizeof(dummy));	

	return 0;
}


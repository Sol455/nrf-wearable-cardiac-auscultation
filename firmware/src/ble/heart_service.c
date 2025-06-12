#include "heart_service.h"

#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#define HEART_ATTR_IDX_PACKET_VALUE 2
#define HEART_ATTR_IDX_ALERT_VALUE  5

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
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
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

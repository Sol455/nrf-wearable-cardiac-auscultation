#include "ble_manager.h"
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "heart_service.h"

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include "../event_handler.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

LOG_MODULE_REGISTER(ble);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_HEART_SERVICE_VAL),
};

//========================================Connection Callbacks==============================================

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_ERR("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
        return;
    }

    AppEvent ev = {.type = EVENT_BLE_CONNECTED};
    event_handler_post(ev);

    LOG_INF("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

    AppEvent ev = {.type = EVENT_BLE_DISCONNECTED};
    event_handler_post(ev);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        LOG_INF("Security changed: %s level %u\n", addr, level);
    }
    else
    {
        LOG_ERR("Security failed: %s level %u err %d %s\n", addr, level, err,
                bt_security_err_to_str(err));
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

//========================================Security and pairing callbacks==============================================
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing failed conn: %s, reason %d %s\n", addr, reason,
            bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .passkey_display = auth_passkey_display,
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed};

int ble_init()
{
    int ret;
    LOG_INF("Starting Bluetooth Peripheral LBS Example\n");

    ret = bt_conn_auth_cb_register(&conn_auth_callbacks);
    if (ret)
    {
        LOG_ERR("Failed to register authorization callbacks.\n");
        return -1;
    }

    ret = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (ret)
    {
        LOG_ERR("Failed to register authorization info callbacks.\n");
        return -1;
    }

    ret = bt_enable(NULL);
    if (ret)
    {
        LOG_ERR("Bluetooth init failed (err %d)\n", ret);
        return -1;
    }

    LOG_INF("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    ret = bt_heart_service_init();
    if (ret)
    {
        LOG_ERR("Failed to init Heart Service (err:%d)\n", ret);
        return -1;
    }

    LOG_INF("Heart Service sucessfully initialised\n");

    return ret;
}

int ble_advertise()
{
    int ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    if (ret)
    {
        LOG_ERR("Advertising failed to start (err %d)\n", ret);
        return 0;
    }
    LOG_INF("Advertising successfully started\n");
    return ret;
}

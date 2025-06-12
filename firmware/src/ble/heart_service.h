#ifndef HEART_SERVICE_H_
#define HEART_SERVICE_H_

#include <zephyr/types.h>

//09BD3E92-235B-4A5B-B00C-BD50E1749A44
#define BT_UUID_HEART_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x09BD3E92, 0x235B, 0x4A5B, 0xB00C, 0xBD50E1749A44)

//24CA214A-C44B-40D6-80D0-2A4E7B0EF0E3
#define BT_UUID_HEART_PACKET_VAL \
	BT_UUID_128_ENCODE(0x24CA214A, 0xC44B, 0x40D6, 0x80D0, 0x2A4E7B0EF0E3)

//359502D4-F343-48CE-97D9-D78FC37D69EE
#define BT_UUID_HEART_ALERT_VAL \
	BT_UUID_128_ENCODE(0x359502D4, 0xF343, 0x48CE, 0x97D9, 0xD78FC37D69EE)

#define BT_UUID_HEART_SERVICE     BT_UUID_DECLARE_128(BT_UUID_HEART_SERVICE_VAL)
#define BT_UUID_HEART_PACKET      BT_UUID_DECLARE_128(BT_UUID_HEART_PACKET_VAL)
#define BT_UUID_HEART_ALERT       BT_UUID_DECLARE_128(BT_UUID_HEART_ALERT_VAL)

struct heart_packet {
	float rms;
	float centroid;
	uint32_t timestamp_ms;
} __packed;

int bt_heart_service_init();
int bt_heart_service_notify_packet(const struct heart_packet *pkt);
int bt_heart_service_notify_alert(uint8_t code);


#endif 


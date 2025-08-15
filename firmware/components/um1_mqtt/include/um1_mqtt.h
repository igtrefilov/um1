#ifndef UM1_MQTT_H
#define UM1_MQTT_H

#include <stdint.h>
#include "mqtt_client.h"
#include "esp_log.h"

#include "um1_config.h"
#include "um1_uart.h"
#include "um1_router.h"

esp_mqtt_client_handle_t get_mqtt_client_handle(void);
void start_mqtt(void);
void um1_mqtt_publish(const uint8_t *data, int len);

#endif // UM1_MQTT_H

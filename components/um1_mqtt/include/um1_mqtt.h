#include <stdint.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "um1_config.h"

esp_mqtt_client_handle_t get_mqtt_client_handle(void);
void start_mqtt(void);
void um1_mqtt_publish(const uint8_t *data, int len);

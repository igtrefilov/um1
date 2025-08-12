#include "um1_mqtt.h"
#include "um1_uart.h"
#include "um1_router.h"

static const char *TAG = "um1_mqtt";
esp_mqtt_client_handle_t global_mqtt_client = NULL;
extern mqtt_config_t global_mqtt_config;

esp_mqtt_client_handle_t get_mqtt_client_handle() {
    return global_mqtt_client;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (global_mqtt_config.rx_enabled && strlen(global_mqtt_config.topic) > 0) {
            esp_mqtt_client_subscribe(((esp_mqtt_event_handle_t)event_data)->client,
                                      global_mqtt_config.topic, 1);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
        break;
    case MQTT_EVENT_DATA:
        {
            esp_mqtt_event_handle_t event = event_data;
            char topic[64];
            int len = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
            memcpy(topic, event->topic, len);
            topic[len] = '\0';

            ESP_LOGI(TAG, "MQTT_EVENT_DATA topic=%s", topic);

            route_data("MQTT", (const uint8_t *)event->data, event->data_len);

            int uart_port = 0;
            if (sscanf(topic, "uart/%d", &uart_port) == 1) {
                int port_num = (uart_port == 2) ? UART_PORT_NUM_2 : UART_PORT_NUM_1;
                uart_write_bytes(port_num, event->data, event->data_len);
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event");
        break;
    }
}

void start_mqtt(void) {
    if (!global_mqtt_config.enabled) {
        ESP_LOGI(TAG, "MQTT is disabled by config.");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = global_mqtt_config.broker,
        .credentials.username = global_mqtt_config.username,
        .credentials.authentication.password = global_mqtt_config.password
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    global_mqtt_client = client;
}

#include "um1_spiffs.h"

static const char *TAG = "spiffs";

void start_spiffs(void){
	esp_vfs_spiffs_conf_t spiffs_cfg = {
	    .base_path = "/spiffs",
	    .partition_label = NULL,
	    .max_files = 5,
	    .format_if_mount_failed = true
	};
	ESP_ERROR_CHECK( esp_vfs_spiffs_register(&spiffs_cfg) );
	size_t total, used;
	ESP_ERROR_CHECK( esp_spiffs_info(NULL, &total, &used) );
	ESP_LOGI(TAG, "SPIFFS: total: %d, used: %d", total, used);
}

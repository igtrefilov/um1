#ifndef UM1_SNTP_H
#define UM1_SNTP_H

#include "um1_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "um1_config.h"

void time_sync_notification_cb(struct timeval *tv);
void start_sntp(void);
bool is_sntp_enabled(void);
uint64_t get_ntp_time_us(void);

#endif // UM1_SNTP_H

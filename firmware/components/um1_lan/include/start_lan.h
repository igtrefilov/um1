#ifndef START_LAN_H_
#define START_LAN_H_

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

#include "um1_lan.h"
#include "um1_config.h"
#include "um1_uart.h"

void start_lan(void);

#endif /* START_LAN_H_ */

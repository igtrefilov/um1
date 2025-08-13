#ifndef UM1_EXTERN_UTILITY_H
#define UM1_EXTERN_UTILITY_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "um1_spiffs.h"

void util_client_task(void *pvParameters);

#endif // UM1_EXTERN_UTILITY_H

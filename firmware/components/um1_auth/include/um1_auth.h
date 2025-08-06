#ifndef UM1_AUTH_H
#define UM1_AUTH_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *username;
    const char *password;          // store hashed in production
    uint32_t token_ttl_sec;        // e.g., 8*3600
    bool cookie_secure;            // Secure flag (HTTPS only)
} token_auth_config_t;

extern token_auth_config_t auth_config;

void token_auth_init(void);
bool is_public_uri(const char *uri);
esp_err_t handle_login(httpd_req_t *req);
esp_err_t handle_logout(httpd_req_t *req);
esp_err_t handle_change_password(httpd_req_t *req);
esp_err_t token_auth_register_endpoints(httpd_handle_t server);
bool token_auth_check(httpd_req_t *req);
void token_auth_set_no_store(httpd_req_t *req);
bool token_is_valid(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // UM1_AUTH_H

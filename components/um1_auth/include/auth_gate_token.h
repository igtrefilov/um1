#include "esp_http_server.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

#include "../components/um1_auth/include/um1_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline bool token_gate_is_protected(const char *uri){
#if CONFIG_TOKEN_AUTH_ENABLED
    if(!uri) return false;
    const char *pfx[] = { "/api", "/config", "/upload", "/ota", "/reboot", "/ws", "/config.json" };
    for (size_t i=0;i<sizeof(pfx)/sizeof(pfx[0]);++i){
        size_t n = strlen(pfx[i]);
        if (strncmp(uri, pfx[i], n)==0) return true;
    }
#endif
    return false;
}

typedef struct { httpd_uri_t orig; void *orig_user_ctx; } token_wrap_t;

static inline esp_err_t token_proxy(httpd_req_t *req){
#if CONFIG_TOKEN_AUTH_ENABLED
    if(!token_auth_check(req)) return ESP_OK;
    token_wrap_t *w = (token_wrap_t*)req->user_ctx;
    if(!w || !w->orig.handler) return ESP_FAIL;
    void *saved = req->user_ctx;
    req->user_ctx = w->orig_user_ctx;
    esp_err_t rc = w->orig.handler(req);
    req->user_ctx = saved;
    return rc;
#else
    return ESP_OK;
#endif
}

static inline esp_err_t auth_register_uri_handler_token(httpd_handle_t server, const httpd_uri_t *uri){
    if(!uri) return ESP_ERR_INVALID_ARG;
#if CONFIG_TOKEN_AUTH_ENABLED
    if(token_gate_is_protected(uri->uri)){
        token_wrap_t *w = (token_wrap_t*)calloc(1,sizeof(token_wrap_t));
        if(!w) return ESP_ERR_NO_MEM;
        w->orig = *uri; w->orig_user_ctx = uri->user_ctx;
        httpd_uri_t proxy = *uri; proxy.handler = token_proxy; proxy.user_ctx = w;
        return httpd_register_uri_handler(server, &proxy);
    }
#endif
    return httpd_register_uri_handler(server, uri);
}

#ifdef __cplusplus
}
#endif

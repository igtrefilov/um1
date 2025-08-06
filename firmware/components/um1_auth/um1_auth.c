#include "um1_auth.h"
#include "cJSON.h"

static const char *TAG = "token_auth";

token_auth_config_t auth_config = {
    .username      = "admin",
    .password      = "admin",
    .token_ttl_sec = 8u * 3600u,
    .cookie_secure = false         /* true — только если HTTPS */
};

static struct {
    char user[64];
    char pass[128];
    uint64_t exp_us;
    char token[65]; // 32 bytes hex -> 64 chars + 0
    uint32_t ttl_sec;
    bool cookie_secure;
    bool inited;
} TA = {0};

void hex_of_bytes(const uint8_t *in, size_t len, char *out, size_t out_sz){
    static const char *H="0123456789abcdef";
    if(out_sz < len*2+1) return;
    for(size_t i=0;i<len;i++){ out[i*2]=H[(in[i]>>4)&0xF]; out[i*2+1]=H[in[i]&0xF]; }
    out[len*2]=0;
}

void token_auth_set_no_store(httpd_req_t *req){
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
}

bool read_body(httpd_req_t *req, char *buf, size_t cap){
    size_t received = 0;
    int ret;
    while (received < cap - 1) {
        ret = httpd_req_recv(req, buf + received, cap - 1 - received);
        if (ret <= 0) break;
        received += ret;
        if (received >= req->content_len) break;
    }
    buf[received] = 0;
    return received > 0;
}

bool parse_field(const char *body, const char *name, char *out, size_t out_sz){
    // naive parse: name=value (urlencoded) OR "name":"value" (json-like)
    const char *p = strstr(body, name);
    if(!p) return false;
    const char *eq = strchr(p, '=');
    if(eq){
        eq++;
        const char *e = strpbrk(eq, "& \r\n");
        size_t n = e ? (size_t)(e-eq) : strlen(eq);
        if(n >= out_sz) n = out_sz-1;
        memcpy(out, eq, n); out[n]=0;
        return true;
    }
    p = strstr(body, name);
    if(!p) return false;
    p = strchr(p, ':');
    if(!p) return false;
    p++;
    while(*p==' '){ p++; }
    if(*p=='"'){
        p++; const char *e = strchr(p,'"'); if(!e) return false;
        size_t n = e-p; if(n>=out_sz) n=out_sz-1; memcpy(out,p,n); out[n]=0; return true;
    }
    return false;
}

void issue_new_token(){
    uint8_t raw[32];
    for (int i=0;i<32;i++){ raw[i]= (uint8_t) (esp_random() & 0xFF); }
    hex_of_bytes(raw, 32, TA.token, sizeof(TA.token));
    TA.exp_us = esp_timer_get_time() + ((uint64_t)TA.ttl_sec)*1000000ULL;
}

bool token_valid(const char *tok){
    if(!tok || !tok[0] || !TA.token[0]) return false;
    if(strcmp(tok, TA.token)!=0) return false;
    if(esp_timer_get_time() > TA.exp_us) return false;
    return true;
}

const char* cookie_value(const char *cookie_hdr, const char *name){
    if(!cookie_hdr||!name) return NULL;
    const char *p = strstr(cookie_hdr, name);
    if(!p) return NULL;
    p += strlen(name);
    if(*p!='=') return NULL;
    return p+1;
}

esp_err_t handle_login(httpd_req_t *req){
    token_auth_set_no_store(req);
    char body[256];
    if(!read_body(req, body, sizeof(body))){
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "no body");
    }
    char u[96]={0}, p[160]={0};
    parse_field(body, "username", u, sizeof(u));
    parse_field(body, "password", p, sizeof(p));

    if(strcmp(u, TA.user)==0 && strcmp(p, TA.pass)==0){
        issue_new_token();
        char ck[256];
        snprintf(ck, sizeof(ck), "UM1SESS=%s; Path=/; HttpOnly; SameSite=Strict%s; Max-Age=%u",
                 TA.token, TA.cookie_secure ? "; Secure" : "", (unsigned)TA.ttl_sec);
        httpd_resp_set_hdr(req, "Set-Cookie", ck);
        httpd_resp_set_hdr(req, "Content-Type", "application/json");
        const char *ok="{\"ok\":true}";
        return httpd_resp_send(req, ok, strlen(ok));
    }else{
        httpd_resp_set_status(req, "401 Unauthorized");
        return httpd_resp_sendstr(req, "unauthorized");
    }
}

bool is_public_uri(const char *uri) {
    return
        strcmp(uri, "/login.html") == 0 ||
        strstr(uri, ".css") != NULL ||
        strstr(uri, ".js") != NULL ||
        strstr(uri, ".png") != NULL ||
        strstr(uri, ".jpg") != NULL ||
        strstr(uri, "favicon") != NULL;
}

esp_err_t handle_logout(httpd_req_t *req){
    TA.token[0] = 0;
    TA.exp_us = 0;
    httpd_resp_set_hdr(req, "Set-Cookie", "UM1SESS=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    httpd_resp_sendstr(req, "bye");
    return ESP_OK;
}

esp_err_t handle_change_password(httpd_req_t *req){
    token_auth_set_no_store(req);
    if(!token_is_valid(req)){
        httpd_resp_set_status(req, "401 Unauthorized");
        return httpd_resp_sendstr(req, "unauthorized");
    }
    char body[256];
    if(!read_body(req, body, sizeof(body))){
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "no body");
    }
    char oldp[160]={0}, newp[160]={0};
    parse_field(body, "old_password", oldp, sizeof(oldp));
    parse_field(body, "new_password", newp, sizeof(newp));
    if(strcmp(oldp, TA.pass)!=0){
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "wrong password");
    }
    strlcpy(TA.pass, newp, sizeof(TA.pass));
    auth_config.password = strdup(newp);

    FILE *f = fopen("/spiffs/src/config.json", "r");
    cJSON *root = NULL;
    if(f){
        fseek(f,0,SEEK_END); long len=ftell(f); rewind(f);
        char *buf = malloc(len+1);
        if(buf){ fread(buf,1,len,f); buf[len]=0; root = cJSON_Parse(buf); free(buf); }
        fclose(f);
    }
    if(!root) root = cJSON_CreateObject();
    cJSON *auth = cJSON_GetObjectItem(root, "auth");
    if(!auth){ auth = cJSON_CreateObject(); cJSON_AddItemToObject(root, "auth", auth); }
    cJSON_ReplaceItemInObject(auth, "username", cJSON_CreateString(TA.user));
    cJSON_ReplaceItemInObject(auth, "password", cJSON_CreateString(TA.pass));
    char *out = cJSON_Print(root);
    f = fopen("/spiffs/src/config.json", "w");
    if(f){ fwrite(out,1,strlen(out),f); fclose(f); }
    free(out);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

void token_auth_init(void) {
    memset(&TA, 0, sizeof(TA));
    strlcpy(TA.user, auth_config.username ? auth_config.username : "admin", sizeof(TA.user));
    strlcpy(TA.pass, auth_config.password ? auth_config.password : "admin", sizeof(TA.pass));
    TA.ttl_sec = auth_config.token_ttl_sec ? auth_config.token_ttl_sec : (8 * 3600);
    TA.cookie_secure = auth_config.cookie_secure;
    TA.inited = true;
    ESP_LOGI(TAG, "Token auth initialized for user '%s', TTL=%u sec", TA.user, (unsigned)TA.ttl_sec);
}

bool token_is_valid(httpd_req_t *req){
    char cookie[256] = {0};
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return false;

    const char *val = cookie_value(cookie, "UM1SESS");
    return token_valid(val);
}

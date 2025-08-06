idf_component_register(
  SRCS "um1_auth.c"
  INCLUDE_DIRS "include"
  REQUIRES esp_http_server mbedtls esp_timer json
)
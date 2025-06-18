idf_component_register(SRCS "um1_http_server.c" "protocol_examples_utils.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_netif esp-tls esp_http_server nvs_flash
                    )

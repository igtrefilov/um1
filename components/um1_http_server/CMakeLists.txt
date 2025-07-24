idf_component_register(SRCS "um1_http_server.c" "protocol_examples_utils.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_system esp_hw_support esp_timer spi_flash esp_netif esp-tls esp_http_server nvs_flash vfs esp_eth esp_wifi app_update um1_lan um1_uart json
                    )

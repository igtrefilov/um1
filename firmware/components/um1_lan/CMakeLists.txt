idf_component_register(SRCS um1_lan.c start_lan.c ethernet_init.c
                    INCLUDE_DIRS "include"
                    REQUIRES esp_netif esp_eth esp_event um1_spiffs um1_config um1_uart um1_socket_server
                    )


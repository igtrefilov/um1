idf_component_register(SRCS um1_wifi.c start_softap.c
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash um1_config
                    )


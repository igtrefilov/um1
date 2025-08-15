idf_component_register(SRCS "um1_router.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_netif um1_config um1_uart um1_mqtt um1_extern_utility
                    )


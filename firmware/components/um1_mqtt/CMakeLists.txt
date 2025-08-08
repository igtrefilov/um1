idf_component_register(SRCS "um1_mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt um1_config um1_uart driver
                    )


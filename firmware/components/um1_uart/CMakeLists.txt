idf_component_register(SRCS "um1_uart.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver um1_http_server um1_mqtt um1_sntp
                    )


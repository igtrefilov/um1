idf_component_register(
    SRCS "um1_ssh.c"
    INCLUDE_DIRS "include"
    REQUIRES um1_config um1_auth
)

# Depend on libssh (placeholder for actual library)
target_link_libraries(${COMPONENT_LIB} INTERFACE ssh)

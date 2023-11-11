
idf_build_get_property(project_dir PROJECT_DIR)

# Initialize an empty list to hold conditional requirements
set(conditionally_required_components "")

if(EXISTS "${project_dir}/components/drv_nvs")
    list(APPEND conditionally_required_components "drv_nvs")
endif()

if(EXISTS "${project_dir}/components/lib_iperf")
    list(APPEND conditionally_required_components "lib_iperf")
endif()

if(EXISTS "${project_dir}/components/drv_esptouch")
    list(APPEND conditionally_required_components "drv_esptouch")
endif()

if(EXISTS "${project_dir}/components/drv_http")
    list(APPEND conditionally_required_components "drv_http")
endif()

if(EXISTS "${project_dir}/components/app_socket_udp")
    list(APPEND conditionally_required_components "app_socket_udp")
endif()

idf_component_register(SRCS "drv_wifi.c" "cmd_wifi.c"
                    INCLUDE_DIRS "." 
                    REQUIRES 
                        ${conditionally_required_components}
                        "esp_netif"
                        "esp_wifi"
                                      )
                 


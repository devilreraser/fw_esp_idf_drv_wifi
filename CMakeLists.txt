
idf_build_get_property(project_dir PROJECT_DIR)

# Initialize an empty list to hold conditional requirements
set(conditionally_required_components "")

if(EXISTS "${project_dir}/components/drv_nvs")
    # set(component_drv_nvs "drv_nvs")
    list(APPEND conditionally_required_components "drv_nvs")
endif()

idf_component_register(SRCS "drv_wifi.c" 
                    INCLUDE_DIRS "." 
                    REQUIRES 
                        ${conditionally_required_components}
                        # ${component_drv_nvs} 
                        "esp_netif"
                        "esp_wifi"
                                      )
                 


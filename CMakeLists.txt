idf_component_register(
    SRCS 
        "src/boot_display.c"
        "src/lp_boot_display.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        esp_driver_gpio 
        esp_driver_spi 
        driver 
        esp_timer 
        freertos 
        log 
        ulp
)

# Output library target name
set_target_properties(${COMPONENT_LIB} PROPERTIES OUTPUT_NAME boot_display)

set(ulp_app_name ulp_main)
set(ulp_sources "ulp/lp_main.c")
set(ulp_exp_dep_srcs "src/boot_display.c")

ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")

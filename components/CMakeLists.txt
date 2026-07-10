set(include_dirs
    LCD
    MyBLE
    Touch
    BMSData
)
set(src_dirs
    LCD
    MyBLE
    Touch
    BMSData
)
set(requires
    driver
    lvgl
    bt
    nvs_flash
)
idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
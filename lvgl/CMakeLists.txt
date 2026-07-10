# LVGL v8.3.0 ESP-IDF Component

# Collect all .c source files recursively
file(GLOB_RECURSE lvgl_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE lvgl_imgs ${CMAKE_CURRENT_SOURCE_DIR}/img/*.c)

idf_component_register(
    SRCS ${lvgl_srcs} ${lvgl_imgs}
    INCLUDE_DIRS
        .
        src
        src/extra
        src/extra/layouts
        src/extra/themes
        src/extra/widgets
        src/extra/libs
        src/extra/others
    REQUIRES
        driver
        esp_timer
)

component_compile_options(-ffast-math -Wno-format)
set(src_dirs 
    ./
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(include_dirs 
    ./
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Embed the model file
set(embed_files "models/yolov8_bumblebee_quantized.espdl")

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES 
        esp_http_server
        espressif__esp32-camera
        espressif__esp-dl
        espressif__esp_jpeg
        nvs_flash
        fatfs
        esp_timer
        esp_lcd
        esp_wifi
        esp_event
        esp_netif
    PRIV_REQUIRES 
        nvs_flash 
        fatfs
    EMBED_FILES ${embed_files}
)

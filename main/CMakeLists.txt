idf_component_register(
    SRC
        "main.cpp"
	"audio_manager.cpp"
    SRC_DIRS
        "."
        "Config"
        "GUI"
        "Fonts"
        "e-Paper"
	"audio_bsp"
	"board_power_bsp"
	"codec_board"
	"i2c_bsp"
    INCLUDE_DIRS
        "."
        "Config"
        "GUI"
        "Fonts"
        "e-Paper"
	"audio_bsp"
	"board_power_bsp"
	"codec_board"
	"codec_board/include"
	"codec_board/drv"
	"i2c_bsp"
    EMBED_TXTFILES
        "codec_board/board_cfg.txt"
    REQUIRES driver esp_timer esp_codec_dev fatfs esp_lcd esp_adc esp_wifi nvs_flash esp_http_client mbedtls json)

# Desactivar treat-warnings-as-errors para código de terceros
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-maybe-uninitialized
    -Wno-unused-variable
    -Wno-type-limits
    -Wno-format
    -Wno-error
)

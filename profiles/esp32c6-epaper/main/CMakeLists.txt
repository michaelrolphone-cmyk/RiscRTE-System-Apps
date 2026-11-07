idf_component_register(SRCS "biomerte_main.c" "bm_protocol.c" "panel.c" "idle_guard.c" INCLUDE_DIRS "."
 EMBED_FILES "assets/idle_logo.bin"
 REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls json driver esp_timer esp_app_format)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)

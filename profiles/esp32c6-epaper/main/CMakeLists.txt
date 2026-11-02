idf_component_register(SRCS "main.c" "panel.c" "frame.c" "presentation.c" "identity.c" "idle_guard.c" INCLUDE_DIRS "."
 EMBED_FILES "assets/idle_logo.bin"
 REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_http_server mbedtls json driver esp_timer esp_app_format)

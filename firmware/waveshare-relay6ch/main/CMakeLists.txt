get_filename_component(apps "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)
set(node "${apps}/Services/infra_node")
idf_component_register(SRCS main.cpp Board.cpp BiomeProvider.cpp OutputStream.cpp "${node}/Node.cpp" "${node}/Relay.cpp" "${node}/Enrollment.cpp" "${node}/Setup.cpp"
 "${apps}/lib/EnrollmentCrypto/monocypher.c" "${apps}/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c"
 INCLUDE_DIRS "." "${node}" "${RISC_RUNTIME_ROOT}/sdk/app" "${RISC_RUNTIME_ROOT}/sdk/driver" "${RISC_RUNTIME_ROOT}/sdk/hardware"
 "${RISC_RUNTIME_ROOT}/src" "${RISC_RUNTIME_ROOT}/lib/ArduinoJson/src" "${RISC_RUNTIME_ROOT}/src/ports/esp32s3"
 REQUIRES esp_websocket_client esp_wifi esp_event esp_netif nvs_flash esp_timer esp_system esp_driver_uart esp_driver_gpio esp_driver_ledc esp_driver_rmt esp-tls mbedtls http_parser)
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(${COMPONENT_LIB} PRIVATE WITH_SHA256 RISC_NATIVE_ESP_IDF=1 RISC_HTTP_INTERNAL_MEMORY=1 ARDUINOJSON_ENABLE_STD_STRING=0 ARDUINOJSON_ENABLE_STD_STREAM=0)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations -Wno-misleading-indentation)
idf_component_get_property(tls_root esp-tls COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${tls_root}/private_include")

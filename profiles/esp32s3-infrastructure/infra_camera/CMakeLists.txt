idf_component_register(SRCS "StillCamera.cpp" REQUIRES esp32-camera esp_http_client mbedtls)
target_include_directories(${COMPONENT_LIB} PRIVATE "${RISC_RUNTIME_ROOT}/sdk/driver")

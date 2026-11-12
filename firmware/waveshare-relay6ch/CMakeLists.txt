cmake_minimum_required(VERSION 3.22)
set(IDF_TARGET esp32s3)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
if(NOT RISC_RUNTIME_ROOT)
 message(FATAL_ERROR "Set RISC_RUNTIME_ROOT to the pinned runtime checkout")
endif()
get_filename_component(RISC_RUNTIME_ROOT "${RISC_RUNTIME_ROOT}" REALPATH)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "$ENV{IDF_PATH}" OUTPUT_VARIABLE sdk_sha OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT sdk_sha STREQUAL "fff9895c82d744c7237be8847347bdd1b07c6643")
 message(FATAL_ERROR "ESP-IDF must be the pinned v6.1 source commit")
endif()
set(COMPONENTS main)
set(PROJECT_VER "2.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(riscrte_relay6ch VERSION 2.1.0)
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
 message(FATAL_ERROR "Requires pinned ESP-IDF 6.1")
endif()
# Preserve the existing transport's cross-signed-root certificate verification.
idf_component_get_property(bundle_lib mbedtls COMPONENT_LIB)
idf_component_get_property(bundle_dir mbedtls COMPONENT_DIR)
execute_process(COMMAND "${PYTHON}" "${RISC_RUNTIME_ROOT}/scripts/native_idf_crt_bundle.py"
 "${bundle_dir}/esp_crt_bundle/esp_crt_bundle.c" "${CMAKE_BINARY_DIR}/risc_esp_crt_bundle.c" COMMAND_ERROR_IS_FATAL ANY)
get_target_property(bundle_sources ${bundle_lib} SOURCES)
list(FILTER bundle_sources EXCLUDE REGEX "esp_crt_bundle[.]c$")
set_property(TARGET ${bundle_lib} PROPERTY SOURCES "${bundle_sources}")
target_sources(${bundle_lib} PRIVATE "${CMAKE_BINARY_DIR}/risc_esp_crt_bundle.c")

# Control freshness needs an accurate subsecond clock, not server transmit time
# delayed by Wi-Fi/WAN delivery. Apply these supported lwIP options to the SDK
# library itself; setting them only on the application would not change SNTP.
idf_component_get_property(lwip_lib lwip COMPONENT_LIB)
target_compile_definitions(${lwip_lib} PUBLIC SNTP_COMP_ROUNDTRIP=1 SNTP_CHECK_RESPONSE=2)

# Pinned AES DMA driver: normal TLS records use bounded, aligned stack
# descriptors rather than a per-record allocation from the busy shared heap.
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/aes_dma_workspace.py"
 "${bundle_dir}/port/aes/dma/esp_aes_dma_core.c" "${CMAKE_BINARY_DIR}/risc_relay_aes_dma.c"
 COMMAND_ERROR_IS_FATAL ANY)
get_target_property(aes_sources tfpsacrypto SOURCES)
set(aes_original_sources "${aes_sources}")
list(FILTER aes_original_sources INCLUDE REGEX "esp_aes_dma_core[.]c$")
list(LENGTH aes_original_sources aes_original_count)
if(NOT aes_original_count EQUAL 1)
 message(FATAL_ERROR "Expected exactly one pinned AES DMA translation unit")
endif()
list(FILTER aes_sources EXCLUDE REGEX "esp_aes_dma_core[.]c$")
set_property(TARGET tfpsacrypto PROPERTY SOURCES "${aes_sources}")
target_sources(tfpsacrypto PRIVATE "${CMAKE_BINARY_DIR}/risc_relay_aes_dma.c")

# Separate RX/TX locks require teardown to wait for any active TLS writer.
idf_component_get_property(ws_lib espressif__esp_websocket_client COMPONENT_LIB)
idf_component_get_property(ws_dir espressif__esp_websocket_client COMPONENT_DIR)
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/websocket_lifetime.py"
 "${ws_dir}/esp_websocket_client.c" "${CMAKE_BINARY_DIR}/risc_relay_websocket.c"
 COMMAND_ERROR_IS_FATAL ANY)
get_target_property(ws_sources ${ws_lib} SOURCES)
list(FILTER ws_sources EXCLUDE REGEX "esp_websocket_client[.]c$")
set_property(TARGET ${ws_lib} PROPERTY SOURCES "${ws_sources}")
target_sources(${ws_lib} PRIVATE "${CMAKE_BINARY_DIR}/risc_relay_websocket.c")

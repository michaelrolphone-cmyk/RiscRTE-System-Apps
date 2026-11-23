#pragma once
struct esp_partition_t {const char *label;};
inline const esp_partition_t *esp_ota_get_running_partition(){
 static const esp_partition_t partition{"app0"};return &partition;
}
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *){
 static const esp_partition_t partition{"app1"};return &partition;
}

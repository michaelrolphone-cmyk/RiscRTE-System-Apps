// Independently paced JPEG connection. Socket I/O never runs on the sensor worker.
// Capture on the Arduino loop to serialize with camera initialization and OTA.
struct CameraMessage {
    uint8_t *buffer;
    size_t length;
    uint64_t captureUs;
    uint32_t session;
};

QueueHandle_t cameraMailbox = nullptr;
std::atomic<uint32_t> cameraCaptureSession{0}, cameraFrames{0}, cameraAcks{0}, cameraSuperseded{0};
WebSocketsClient videoSocket;
String videoHeaders;
uint32_t cameraSession = 0;

bool videoConnected = false, videoReady = false;

void nextCameraSession() {
    cameraCaptureSession.store(0);
    if (++cameraSession == 0) ++cameraSession;
    videoReady = false;
}

void cameraEvent(WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED || type == WStype_DISCONNECTED) {
        nextCameraSession();
        videoConnected = type == WStype_CONNECTED;
        return;
    }
    if (type != WStype_TEXT) return;
    const uint64_t received = esp_timer_get_time();
    StaticJsonDocument<512> message;
    if (deserializeJson(message, payload, length)) return;
    const String kind = message["type"] | "";
    if (kind == "clock") {
        StaticJsonDocument<384> reply;
        reply["type"] = "clock";
        reply["id"] = message["id"];
        reply["boot_id"] = boot;
        reply["received_monotonic_us"] = received;
        reply["served_monotonic_us"] = static_cast<uint64_t>(esp_timer_get_time());
        String bytes;
        serializeJson(reply, bytes);
        videoSocket.sendTXT(bytes);
    } else if (kind == "camera_ready" && String(message["boot_id"] | "") == boot) {
        videoReady = true;
        cameraCaptureSession.store(cameraSession);
    } else if (kind == "camera_ack" && String(message["frame_id"] | "").startsWith(boot + ":")) {
        ++cameraAcks;
    }
}

void cameraWorker(void *) {
    uint32_t revision = UINT32_MAX;
    Config route;
    videoSocket.onEvent(cameraEvent);
    videoSocket.setReconnectInterval(1000);
    videoSocket.enableHeartbeat(5000, 2000, 2);
    bool wifi = false;
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const bool changed = revision != config.revision;
        if (changed) route = config;
        xSemaphoreGive(mutex);
        const bool online = WiFi.status() == WL_CONNECTED;
        if (changed || online != wifi) {
            videoSocket.disconnect();
            nextCameraSession();
            videoConnected = false;
            revision = route.revision;
            wifi = online;
            if (online && route.host.length() && route.token.length()) {
                videoHeaders = "Authorization: Sensor " + route.token;
                videoSocket.setExtraHeaders(videoHeaders.c_str());
                videoSocket.begin(route.host, route.port, "/camera");
            }
        }
        if (wifi && route.host.length()) videoSocket.loop();
        CameraMessage frame{};
        if (xQueueReceive(cameraMailbox, &frame, 0) == pdTRUE) {
            if (videoConnected && videoReady && frame.session == cameraSession) {
                if (videoSocket.sendBIN(frame.buffer, frame.length, true)) {
                    ++cameraFrames;
                } else {
                    videoSocket.disconnect();
                }
            }
            heap_caps_free(frame.buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void startCameraPush() {
    cameraMailbox = xQueueCreate(1, sizeof(CameraMessage));
    if (cameraMailbox) xTaskCreate(cameraWorker, "camera-websocket", 8192, nullptr, 1, nullptr);
}

void captureForSocket(uint32_t intervalMs) {
    static uint32_t lastCapture = 0;
    const uint32_t session = cameraCaptureSession.load();
    if (!session || millis() - lastCapture < intervalMs) return;
    lastCapture = millis();
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;
    if (fb->format == PIXFORMAT_JPEG && fb->len) {
        const uint64_t captureUs = static_cast<uint64_t>(fb->timestamp.tv_sec) * 1000000ULL + fb->timestamp.tv_usec;
        Document metadata(131072);
        JsonObject packet = metadata.to<JsonObject>();
        if (!captureUs || !radio_telemetry::writeFrameJson(packet, captureUs, captureUs)) {
            esp_camera_fb_return(fb);
            return;
        }
        packet["camera_id"] = CAMERA_SOCKET_ROLE;
        packet["width"] = fb->width;
        packet["height"] = fb->height;
        packet["capture_timestamp_semantics"] = "camera_driver_first_dma_buffer";
        packet["image_available"] = true;
        packet["served_monotonic_us"] = static_cast<uint64_t>(esp_timer_get_time());
        if (!metadata.overflowed()) {
            const size_t size = measureJson(metadata);
            const size_t length = 4 + size + fb->len;
            uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(
                WEBSOCKETS_MAX_HEADER_SIZE + length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (buffer) {
                uint8_t *body = buffer + WEBSOCKETS_MAX_HEADER_SIZE;
                body[0] = size >> 24; body[1] = size >> 16;
                body[2] = size >> 8; body[3] = size;
                serializeJson(metadata, reinterpret_cast<char *>(body + 4), size + 1);
                memcpy(body + 4 + size, fb->buf, fb->len);
                CameraMessage frame{buffer, length, captureUs, session};
                if (xQueueSend(cameraMailbox, &frame, 0) != pdTRUE) {
                    CameraMessage old{};
                    if (xQueueReceive(cameraMailbox, &old, 0) == pdTRUE) {
                        heap_caps_free(old.buffer);
                        ++cameraSuperseded;
                    }
                    xQueueSend(cameraMailbox, &frame, 0);
                }
            }
        }
    }
    esp_camera_fb_return(fb);
}

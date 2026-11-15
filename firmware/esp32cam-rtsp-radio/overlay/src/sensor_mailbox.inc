// One producer owns sampling. Socket I/O can only hold its in-flight buffer.
struct SensorMessage {
    uint8_t *buffer;
    size_t size;
    uint64_t captureUs;
    uint64_t sequence;
};

QueueHandle_t sensorMailbox = nullptr;
std::atomic<uint32_t> samplesPrepared{0}, samplesSuperseded{0}, prepareFailures{0};
std::atomic<uint32_t> lastPrepareMs{0}, maxSampleGapMs{0};

void sampleSensors(void *) {
    const TickType_t period = pdMS_TO_TICKS(100);
    TickType_t deadline = xTaskGetTickCount();
    uint32_t lastStart = 0;
    for (;;) {
        if (sensorPaused() || Update.isRunning()) {
            lastStart = 0;
        } else {
            const uint32_t start = millis();
            if (lastStart) {
                const uint32_t gap = start - lastStart;
                if (gap > maxSampleGapMs.load()) maxSampleGapMs.store(gap);
            }
            lastStart = start;
            Document document(131072);
            JsonObject packet = document.to<JsonObject>();
            writePacket(packet);
            SensorMessage frame{};
            if (!document.overflowed() && packet["capture_monotonic_us"].as<uint64_t>()) {
                frame.sequence = packet["stream_sequence"].as<uint64_t>();
                frame.captureUs = packet["capture_monotonic_us"].as<uint64_t>();
                frame.size = measureJson(document);
                frame.buffer = static_cast<uint8_t *>(heap_caps_malloc(
                    WEBSOCKETS_MAX_HEADER_SIZE + frame.size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            }
            if (frame.buffer) {
                char *body = reinterpret_cast<char *>(frame.buffer + WEBSOCKETS_MAX_HEADER_SIZE);
                serializeJson(document, body, frame.size + 1);
                // Only this task enqueues. If the writer claims the old frame first,
                // receive simply finds the slot empty and the fresh frame still fits.
                if (xQueueSend(sensorMailbox, &frame, 0) != pdTRUE) {
                    SensorMessage old{};
                    if (xQueueReceive(sensorMailbox, &old, 0) == pdTRUE) {
                        heap_caps_free(old.buffer);
                        ++samplesSuperseded;
                    }
                    xQueueSend(sensorMailbox, &frame, 0);
                }
                ++samplesPrepared;
            } else {
                ++prepareFailures;
            }
            lastPrepareMs.store(millis() - start);
        }
        if (xTaskGetTickCount() - deadline >= period) deadline = xTaskGetTickCount();
        vTaskDelayUntil(&deadline, period);
    }
}

void sendLatestSample() {
    SensorMessage frame{};
    if (xQueueReceive(sensorMailbox, &frame, 0) != pdTRUE) return;
    const uint32_t start = millis();
    const bool delivered = socket.sendTXT(frame.buffer, frame.size, true);
    lastSendDurationMs = millis() - start;
    if (delivered) {
        ++sent;
        lastPacketBytes = frame.size;
        portENTER_CRITICAL(&sampleLock);
        lastSentSampleUs = frame.captureUs;
        lastSentSequence = frame.sequence;
        portEXIT_CRITICAL(&sampleLock);
    } else {
        ++failed;
        // A partially written WebSocket frame cannot be followed by a new frame.
        socket.disconnect();
    }
    heap_caps_free(frame.buffer);
}

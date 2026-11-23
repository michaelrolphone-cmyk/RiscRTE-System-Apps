#include "sensor_push.h"
#include "settings.h"
#include <atomic>
#include <Update.h>
#include <esp_camera.h>
#include <freertos/queue.h>
#include "radio_telemetry.h"
#include "bno_telemetry.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#define LIVE_WEBSOCKET_SEND_TIMING 1
#define LIVE_WEBSOCKET_TCP_DIAGNOSTIC 1
#include "live_websocket.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace sensor_push {
namespace {
struct Psram {
    void *allocate(size_t n) { return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
    void deallocate(void *p) { heap_caps_free(p); }
    void *reallocate(void *p,size_t n) { return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
};
using Document=BasicJsonDocument<Psram>;
struct Config { String host,token; uint16_t port=9877; uint32_t revision=0; } config;
SemaphoreHandle_t mutex=nullptr;
WebServer *server=nullptr;
bool (*authorizeRequest)()=nullptr;
LiveWebSocket socket;
// Fixed storage outlives every queued callback; allocate on the first connection.
RearTcpDiagnostic sensorTcpDiagnostic, cameraTcpDiagnostic;
String headers,boot,tcpReceiver;
volatile bool connected=false,paused=false;
bool workerReady=false;


bool clockReady=false;
volatile uint32_t stationDisconnects=0, stationDisconnectReason=0;
uint32_t socketConnections=0, socketDisconnects=0;
uint32_t lastSocketDisconnectMs=0, lastSendDurationMs=0;
portMUX_TYPE sampleLock = portMUX_INITIALIZER_UNLOCKED;
uint32_t dialFailures = 0;
uint64_t lastSentSampleUs=0;
uint64_t lastSentSequence=0;
size_t lastPacketBytes=0;
uint64_t sequence=0;
uint32_t sent=0,failed=0;

#define CAMERA_SOCKET_ROLE "rear"
#define CAMERA_SOCKET_YIELD_BETWEEN_FRAGMENTS false
#include "camera_websocket.inc"

void writePacket(JsonObject packet,bool streaming=true) {
    const uint64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&sampleLock);
    const uint64_t cursor = lastSentSampleUs;
    const uint64_t sample = streaming ? ++sequence : sequence;
    portEXIT_CRITICAL(&sampleLock);
    radio_telemetry::writeFrameJson(packet, now, streaming && cursor ? cursor : 1);
    packet["wire_mode"] = "interleaved_fragments";
    packet["sampling_mode"] = "latest_mailbox";
    packet["delivery_mode"] = "continuous";
    packet["camera_id"]="rear";
    packet["schema"]="biome.sensor_tick.v1";
    packet["image_available"]=false;
    packet["capture_timestamp_semantics"]="sensor_snapshot_tick_no_camera_frame";
    packet["stream_sequence"]=sample;
    packet["served_monotonic_us"]=static_cast<uint64_t>(esp_timer_get_time());
}

bool sensorPaused() { return paused; }
#include "sensor_mailbox.inc"

void event(WStype_t type,uint8_t *payload,size_t length) {
    if (type == WStype_CONNECTED) {
        socket.transportChanged(true);
        socket.diagnosticTransportChanged(true);
        portENTER_CRITICAL(&sampleLock);
        lastSentSampleUs = 0;
        portEXIT_CRITICAL(&sampleLock);
        connected = true;
        clockReady = false;
        ++socketConnections;
    }
    if (type == WStype_DISCONNECTED) {
        socket.transportChanged(false);
        socket.diagnosticTransportChanged(false);
        if (connected) {
            ++socketDisconnects;
            lastSocketDisconnectMs = millis();
        } else {
            ++dialFailures;
        }
        connected = false;
        clockReady = false;
    }
    if(type!=WStype_TEXT) return;
    const uint64_t received=esp_timer_get_time();
    DynamicJsonDocument request(1024);
    if(deserializeJson(request,payload,length)) return;
    const String kind=request["type"]|"";
    if(kind=="bno") {
        StaticJsonDocument<512> reply;
        bno_telemetry::enqueueCommand(request.as<JsonObjectConst>(),reply.to<JsonObject>());
        reply["type"]="bno";
        String encoded;serializeJson(reply,encoded);socket.sendTXT(encoded);
    }
    if(kind=="clock") {
        socket.noteClock(request["id"], received);
    }
}

void worker(void *) {
    uint32_t revision = UINT32_MAX;
    Config route;
    bool wifi = false;
    socket.onEvent(event);
    socket.setReconnectInterval(1000);
    socket.enableHeartbeat(5000, 2000, 2);
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const bool changed = revision != config.revision;
        if (changed) route = config;
        xSemaphoreGive(mutex);
        const bool online = WiFi.status() == WL_CONNECTED;
        if (changed || online != wifi) {
            socket.disconnect();
            connected = false;
            clockReady = false;
            revision = route.revision;
            wifi = online;
            if (online && route.host.length() && route.token.length()) {
                headers = "Authorization: Sensor " + route.token;
                socket.setExtraHeaders(headers.c_str());
                socket.begin(route.host, route.port, "/sensors");
            }
        }
        if (wifi && route.host.length()) socket.loop();
        if (connected && socket.flushClock(boot)) clockReady = true;
        if (connected && !sensorPaused() && !Update.isRunning()) sendLatestSample();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// Detailed sender timings are HTTP-only. Do not add these fields to writePacket
// or radio_telemetry frame metadata: changing wire sizes would bias the measure.
void writeSendCounters(JsonObject object, const LiveSendCounters &c) {
#define LIVE_SEND_JSON(field) object[#field] = c.field
    LIVE_SEND_JSON(total_us); LIVE_SEND_JSON(loop_us);
    LIVE_SEND_JSON(fragment_yield_us); LIVE_SEND_JSON(send_syscall_us);
    LIVE_SEND_JSON(send_wait_us); LIVE_SEND_JSON(other_us);
    LIVE_SEND_JSON(payload_bytes); LIVE_SEND_JSON(completed_payload_bytes);
    LIVE_SEND_JSON(wire_bytes); LIVE_SEND_JSON(loop_calls);
    LIVE_SEND_JSON(fragments_attempted); LIVE_SEND_JSON(fragments_completed);
    LIVE_SEND_JSON(send_calls); LIVE_SEND_JSON(would_block);
    LIVE_SEND_JSON(interrupted); LIVE_SEND_JSON(send_waits);
    LIVE_SEND_JSON(fragment_yields);
#undef LIVE_SEND_JSON
}

void writeCameraSendTiming(JsonObject object) {
    const LiveSendTimingSnapshot snapshot = cameraSendTimingSnapshot();
    const uint64_t now = esp_timer_get_time();
    object["schema"] = "rover.camera_send_timing.v1";
    object["clock"] = "boot_monotonic_us";
    object["measurement"] = "elapsed_wall_time";
    object["attempts"] = snapshot.attempts;
    object["delivered"] = snapshot.delivered;
    object["failed"] = snapshot.failed;
    object["invalid_accounting"] = snapshot.invalid_accounting;
    if (snapshot.attempts) {
        const LiveSendTiming &last = snapshot.last;
        JsonObject recent = object.createNestedObject("last");
        recent["capture_us"] = last.capture_us;
        recent["started_us"] = last.started_us;
        recent["completed_us"] = last.completed_us;
        recent["age_us"] = now >= last.completed_us ? now - last.completed_us : 0;
        recent["delivered"] = last.delivered;
        recent["accounting_valid"] = last.accounting_valid;
        writeSendCounters(recent, last.counters);
    } else {
        object["last"] = nullptr;
    }
    writeSendCounters(object.createNestedObject("cumulative"), snapshot.cumulative);
}

// Cached TCP observations only; this endpoint never touches sockets or PCBs.
void writeTcpSegments(JsonObject out, const RearTcpDiagnostic::Segments &s) {
    out["visited"] = s.visited;
    out["payload_bytes"] = s.payload_bytes;
    out["truncated"] = s.truncated;
    out["not_visited"] = s.not_visited;
    if (s.visited) { out["first_seq"] = s.first_seq; out["last_end_seq"] = s.last_end_seq; }
    else { out["first_seq"] = nullptr; out["last_end_seq"] = nullptr; }
}

void writeTcpDiagnostic(JsonObject out, const RearTcpDiagnostic::Snapshot &view, uint64_t now) {
    out["initialized"] = view.initialized;
    out["message_ready"] = view.message_ready;
    out["connected"] = view.connected;
    out["generation"] = view.generation;
    out["tuple_valid"] = view.tuple_valid;
    out["tuple_error"] = view.tuple_error;
    if (view.tuple_valid) {
        out["local_port"] = view.tuple.localPort; out["remote_port"] = view.tuple.remotePort;
    } else { out["local_port"] = nullptr; out["remote_port"] = nullptr; }
    out["pending"] = view.pending; // Includes READY awaiting owner consumption.
    if (view.pending) {
        out["pending_generation"] = view.pending_generation;
        out["pending_age_us"] = now >= view.pending_since_us ? now - view.pending_since_us : 0;
    } else { out["pending_generation"] = nullptr; out["pending_age_us"] = nullptr; }
    if (view.totals.attempts) out["last_post_error"] = view.last_post_error;
    else out["last_post_error"] = nullptr;
    const auto &t = view.totals;
    JsonObject totals = out.createNestedObject("totals");
#define REAR_TCP_TOTAL(field) totals[#field] = t.field
    REAR_TCP_TOTAL(attempts); REAR_TCP_TOTAL(queued); REAR_TCP_TOTAL(queue_rejected);
    REAR_TCP_TOTAL(completed); REAR_TCP_TOTAL(stale_discarded); REAR_TCP_TOTAL(not_found);
    REAR_TCP_TOTAL(matched); REAR_TCP_TOTAL(blocked_trigger); REAR_TCP_TOTAL(truncated);
    REAR_TCP_TOTAL(snd_buf_zero); REAR_TCP_TOTAL(snd_wnd_zero); REAR_TCP_TOTAL(retransmitting_samples);
    REAR_TCP_TOTAL(dispatch_us_sum); REAR_TCP_TOTAL(dispatch_us_max);
    REAR_TCP_TOTAL(callback_us_sum); REAR_TCP_TOTAL(callback_us_max);
#undef REAR_TCP_TOTAL
    if (t.matched) {
        totals["min_snd_buf"] = t.min_snd_buf; totals["min_cwnd"] = t.min_cwnd;
        totals["min_snd_wnd"] = t.min_snd_wnd;
        totals["max_unsent_bytes"] = t.max_unsent_bytes;
        totals["max_unacked_bytes"] = t.max_unacked_bytes;
        totals["max_nrtx"] = t.max_nrtx; totals["max_dupacks"] = t.max_dupacks;
    } else {
        totals["min_snd_buf"] = nullptr; totals["min_cwnd"] = nullptr;
        totals["min_snd_wnd"] = nullptr; totals["max_unsent_bytes"] = nullptr;
        totals["max_unacked_bytes"] = nullptr; totals["max_nrtx"] = nullptr;
        totals["max_dupacks"] = nullptr;
    }
    if (!view.observed) { out["last"] = nullptr; return; }
    const auto &s = view.last;
    JsonObject last = out.createNestedObject("last");
    last["generation"] = s.generation;
    last["current_generation"] = view.connected && s.generation == view.generation;
    last["requested_us"] = s.requested_us;
    last["source_snapshot_us"] = s.started_us;
    last["source_complete_us"] = s.completed_us;
    last["age_us"] = now >= s.started_us ? now - s.started_us : 0;
    last["stale"] = !view.connected || s.generation != view.generation ||
                    now < s.started_us || now - s.started_us > RearTcpDiagnostic::staleAfterUs;
    last["post_attempt_to_callback_us"] = s.started_us - s.requested_us;
    last["callback_us"] = s.completed_us - s.started_us;
    last["trigger"] = s.would_block ? "would_block" : "not_would_block";
    last["found"] = s.found; last["pcbs_visited"] = s.pcbs_visited;
    last["pcb_walk_truncated"] = s.pcb_walk_truncated;
    last["tcp_ticks"] = s.tcp_ticks;
    if (!s.found) { last["tcp"] = nullptr; return; }
    JsonObject tcp = last.createNestedObject("tcp");
#define REAR_TCP_VALUE(field) tcp[#field] = s.field
    REAR_TCP_VALUE(state); REAR_TCP_VALUE(flags); REAR_TCP_VALUE(mss);
    REAR_TCP_VALUE(snd_buf); REAR_TCP_VALUE(cwnd); REAR_TCP_VALUE(snd_wnd);
    REAR_TCP_VALUE(ssthresh); REAR_TCP_VALUE(snd_queuelen);
    REAR_TCP_VALUE(lastack); REAR_TCP_VALUE(snd_nxt); REAR_TCP_VALUE(snd_lbb);
    REAR_TCP_VALUE(rto_end); REAR_TCP_VALUE(bytes_acked); REAR_TCP_VALUE(rtseq);
    REAR_TCP_VALUE(nrtx); REAR_TCP_VALUE(dupacks);
#undef REAR_TCP_VALUE
    tcp["rtime_ticks"] = s.rtime; tcp["rto_ticks"] = s.rto;
    tcp["sa_raw"] = s.sa; tcp["sv_raw"] = s.sv; tcp["rttest_ticks"] = s.rttest;
    writeTcpSegments(tcp.createNestedObject("unacked"), s.unacked);
    writeTcpSegments(tcp.createNestedObject("unsent"), s.unsent);
}

void tcpStatus() {
    if (!authorizeRequest || !authorizeRequest()) return;
    const auto camera = cameraTcpDiagnostic.snapshot();
    const auto sensor = sensorTcpDiagnostic.snapshot();
    const uint64_t now = esp_timer_get_time();
    DynamicJsonDocument d(8192);
    d["schema"] = "rover.rear_tcp_diagnostic.v1";
    d["receiver_id"] = tcpReceiver; d["boot_id"] = boot; d["firmware"] = APP_VERSION;
    d["clock"] = "esp_timer"; d["observed_at_monotonic_us"] = now;
    d["sampling_interval_us"] = static_cast<uint64_t>(RearTcpDiagnostic::intervalUs);
    d["stale_after_us"] = static_cast<uint64_t>(RearTcpDiagnostic::staleAfterUs);
    d["tcp_slow_tick_ms"] = TCP_SLOW_INTERVAL;
    d["configured_send_buffer_bytes"] = TCP_SND_BUF;
    d["configured_send_queue_pbufs"] = TCP_SND_QUEUELEN;
    d["configured_mss_bytes"] = TCP_MSS;
    d["max_pcbs"] = static_cast<unsigned>(RearTcpDiagnostic::maxPcbs);
    d["max_segments_total"] = static_cast<unsigned>(RearTcpDiagnostic::maxSegments);
    writeTcpDiagnostic(d.createNestedObject("camera"), camera, now);
    writeTcpDiagnostic(d.createNestedObject("sensor"), sensor, now);
    if (d.overflowed()) {
        server->send(503, "application/json", "{\"error\":\"TCP diagnostic allocation failed\"}");
        return;
    }
    String text; serializeJson(d, text);
    server->sendHeader("Cache-Control", "no-store"); server->send(200, "application/json", text);
}

void status() {
    if(!authorizeRequest || !authorizeRequest()) return;
    Config current;xSemaphoreTake(mutex,portMAX_DELAY);current=config;xSemaphoreGive(mutex);
    DynamicJsonDocument d(6144);
    writeCameraSendTiming(d.createNestedObject("camera_send_timing"));
    d["camera_connected"]=videoConnected.load();d["camera_ready"]=videoReady.load();
    d["camera_socket_connects"]=cameraConnections.load();
    d["camera_socket_disconnects"]=cameraDisconnects.load();
    d["camera_socket_dial_failures"]=cameraDialFailures.load();
    d["camera_socket_write_timeouts"]=videoSocket.writeTimeoutCount();
    d["camera_socket_last_progress_ms"]=videoSocket.lastWriteProgressMs();
    d["camera_capture_attempts"]=cameraCaptureAttempts.load();
    d["camera_capture_count"]=cameraCaptures.load();
    d["camera_capture_failures"]=cameraCaptureFailures.load();
    d["camera_capture_last_duration_ms"]=cameraLastCaptureDurationMs.load();
    d["camera_last_jpeg_bytes"]=cameraLastJpegBytes.load();
    d["camera_last_payload_bytes"]=cameraLastPayloadBytes.load();
    d["camera_send_duration_ms"]=cameraLastSendDurationMs.load();
    d["socket_write_timeouts"]=socket.writeTimeoutCount();
    d["socket_last_progress_ms"]=socket.lastWriteProgressMs();
    d["camera_frames_superseded"]=cameraSuperseded.load();
    d["camera_frames_sent"]=cameraFrames.load();d["camera_frames_acknowledged"]=cameraAcks.load();
    d["camera_capture_period_ms"]=cameraCapturePeriodMs.load();
    d["camera_capture_width"]=cameraCaptureWidth.load();
    d["camera_capture_height"]=cameraCaptureHeight.load();
    d["rssi_dbm"]=WiFi.RSSI();d["channel"]=WiFi.channel();
    d["transport"]="websocket_push";d["camera_id"]="rear";d["worker_ready"]=workerReady;d["paused"]=paused;d["configured"]=!current.host.isEmpty();
    d["host"]=current.host;d["port"]=current.port;d["connected"]=connected;
    d["sent"]=sent;d["failed"]=failed;
    d["delivery_mode"]="continuous";d["awaiting_ack"]=false;
    portENTER_CRITICAL(&sampleLock);
    const uint64_t sentSequence = lastSentSequence;
    const uint64_t preparedSequence = sequence;
    portEXIT_CRITICAL(&sampleLock);
    d["wire_mode"]="interleaved_fragments";
    d["sampling_mode"]="latest_mailbox";
    d["producer_sequence"]=preparedSequence;
    d["samples_prepared"]=samplesPrepared.load();
    d["superseded_before_send"]=samplesSuperseded.load();
    d["prepare_failures"]=prepareFailures.load();
    d["prepare_ms"]=lastPrepareMs.load();
    d["max_sample_gap_ms"]=maxSampleGapMs.load();
    d["socket_dial_failures"]=dialFailures;
    d["last_sent_sequence"]=sentSequence;d["send_duration_ms"]=lastSendDurationMs;
    d["socket_connects"]=socketConnections;d["socket_disconnects"]=socketDisconnects;
    d["last_socket_disconnect_ms"]=lastSocketDisconnectMs;
    d["station_disconnects"]=stationDisconnects;d["station_disconnect_reason"]=stationDisconnectReason;d["clock_ready"]=clockReady;
    d["last_packet_bytes"]=lastPacketBytes;d["inflight_sequence"]=0;
    if (d.overflowed()) {
        server->send(503,"application/json","{\"error\":\"stream status allocation failed\"}");
        return;
    }
    String text;serializeJson(d,text);server->send(200,"application/json",text);
}

void configure() {
    if(!authorizeRequest || !authorizeRequest()) return;
    DynamicJsonDocument d(2048);
    if(deserializeJson(d,server->arg("plain")) || !d["host"].is<const char *>() ||
       !d["token"].is<const char *>() || !d["port"].is<unsigned int>()) {
        server->send(400,"application/json","{\"error\":\"host, port and token required\"}");return;
    }
    Config next;next.host=String(d["host"].as<const char *>());next.token=String(d["token"].as<const char *>());
    const unsigned int port=d["port"].as<unsigned int>();
    if(next.host.isEmpty() || next.host.length()>253 || next.host.indexOf('/')>=0 ||
       next.token.length()<32 || next.token.length()>128 || next.token.indexOf('\r')>=0 || next.token.indexOf('\n')>=0 || !port || port>65535) {
        server->send(400,"application/json","{\"error\":\"invalid stream configuration\"}");return;
    }
    next.port=port;
    xSemaphoreTake(mutex,portMAX_DELAY);
    const bool unchanged=config.host==next.host && config.port==next.port && config.token==next.token;
    xSemaphoreGive(mutex);
    if(unchanged) {status();return;}
    Preferences prefs;
    if(!prefs.begin("rover-push",false)) {server->send(503,"application/json","{\"error\":\"storage unavailable\"}");return;}
    const bool stored=prefs.putString("host",next.host)>0 && prefs.putString("token",next.token)>0 && prefs.putUShort("port",next.port)>0;
    prefs.end();
    if(!stored) {server->send(503,"application/json","{\"error\":\"stream configuration not persisted\"}");return;}
    xSemaphoreTake(mutex,portMAX_DELAY);next.revision=config.revision+1;config=next;xSemaphoreGive(mutex);
    status();
}

void capture() {
    if(!authorizeRequest || !authorizeRequest()) return;
    Document d(131072);writePacket(d.to<JsonObject>(),false);
    if(d.overflowed()) {server->send(503,"application/json","{\"error\":\"sensor allocation failed\"}");return;}
    String text;serializeJson(d,text);server->sendHeader("Cache-Control","no-store");server->send(200,"application/json",text);
}
} // namespace

void offerCamera(uint32_t intervalMs) { captureForSocket(intervalMs); }

void begin(WebServer &http,bool (*authorize)()) {
    server=&http;authorizeRequest=authorize;mutex=xSemaphoreCreateMutex();
    if(!mutex) return;
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        stationDisconnectReason=info.wifi_sta_disconnected.reason;
        ++stationDisconnects;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    Preferences prefs;
    if(prefs.begin("rover-push",true)) {config.host=prefs.getString("host","");config.token=prefs.getString("token","");config.port=prefs.getUShort("port",9877);prefs.end();}
    Document initial(131072);writePacket(initial.to<JsonObject>(),false);boot=String(initial["boot_id"]|"");
    tcpReceiver=String(initial["receiver_id"]|"");
    socket.attachTcpDiagnostic(sensorTcpDiagnostic); videoSocket.attachTcpDiagnostic(cameraTcpDiagnostic);
    startCameraPush();
    http.on("/api/sensors/capture",HTTP_GET,capture);
    http.on("/api/telemetry/stream",HTTP_GET,status);
    http.on("/api/telemetry/tcp",HTTP_GET,tcpStatus);
    http.on("/api/telemetry/stream",HTTP_POST,configure);
    sensorMailbox = xQueueCreate(1, sizeof(SensorMessage));
    constexpr UBaseType_t sensorPriority = tskIDLE_PRIORITY + 2;
    if (!sensorMailbox || xTaskCreate(sampleSensors, "sensor-sampling", 8192, nullptr, sensorPriority, nullptr) != pdPASS) return;
    workerReady=xTaskCreate(worker,"rear-sensor-websocket",8192,nullptr,sensorPriority,nullptr)==pdPASS;
}
void pause(bool value) {paused=value;}
} // namespace sensor_push

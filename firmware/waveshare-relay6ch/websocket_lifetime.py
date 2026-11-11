"""Pinned WebSocket TLS close lifetime repair (esp_websocket_client 1.8.0).
Normal receive/transmit keep separate locks. Closing waits for the active writer
before freeing TLS state; queued writers recheck connection under the TX lock.
"""
import hashlib,json,pathlib,sys
EXPECTED = "18041ec37d3456bc1ecb3275a78c3ae64679f57f049b5b05af169ef6b1857c77"
def patch(source):
    if hashlib.sha256(source.encode()).hexdigest() != EXPECTED:
        raise ValueError("Unrecognized pinned WebSocket client")
    anchor = "static esp_err_t esp_websocket_client_abort_connection("
    helper = """// PRECONDITION: caller holds client->lock exactly once. Publish a
// non-connected state before this call. Follow the SDK's TX -> state lock
// order, allowing an in-flight write to finish before TLS is destroyed.
static void relay_close_transport(esp_websocket_client_handle_t client)
{
#ifdef CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK
    xSemaphoreGiveRecursive(client->lock);
    xSemaphoreTakeRecursive(client->tx_lock, portMAX_DELAY);
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
#endif
    esp_transport_close(client->transport);
#ifdef CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK
    xSemaphoreGiveRecursive(client->tx_lock);
#endif
}

"""
    assert source.count(anchor)==1
    # Three original close sites: abort, reconnect-on-close, and task exit.
    assert source.count("esp_transport_close(client->transport);")==3
    source=source.replace("esp_transport_close(client->transport);","relay_close_transport(client);")
    source=source.replace(anchor,helper+anchor)
    old="""    relay_close_transport(client);

    if (!client->config->auto_reconnect) {"""
    new="""    if (!client->config->auto_reconnect) {"""
    assert source.count(old)==1
    source=source.replace(old,new)
    old="""    client->error_handle.error_type = error_type;
    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_DISCONNECTED, NULL, 0);"""
    new="""    relay_close_transport(client);
    client->error_handle.error_type = error_type;
    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_DISCONNECTED, NULL, 0);"""
    assert source.count(old)==1
    source=source.replace(old,new)
    old="""    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_FINISH, NULL, 0);
    relay_close_transport(client);
    client->state = WEBSOCKET_STATE_UNKNOW;"""
    new="""    esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_FINISH, NULL, 0);
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    client->state = WEBSOCKET_STATE_UNKNOW;
    relay_close_transport(client);
    xSemaphoreGiveRecursive(client->lock);"""
    assert source.count(old)==1
    source=source.replace(old,new)
    old="""    if (esp_websocket_new_buf(client, true) != ESP_OK) {"""
    new="""    // A sender can have queued behind the writer that triggered teardown.
    if (!esp_websocket_client_is_connected(client)) {
        goto unlock_and_return;
    }

    if (esp_websocket_new_buf(client, true) != ESP_OK) {"""
    assert source.count(old)==1
    source=source.replace(old,new)
    return source
if __name__=="__main__":
    s=pathlib.Path(sys.argv[1]).read_text()
    out=patch(s);pathlib.Path(sys.argv[2]).write_text(out)
    print("WS_TLS_LIFETIME_SOURCE_PROOF "+json.dumps({"original_sha256":EXPECTED,"patched_sha256":hashlib.sha256(out.encode()).hexdigest(),"protected_close_sites":3,"post_tx_lock_connection_check":True}))

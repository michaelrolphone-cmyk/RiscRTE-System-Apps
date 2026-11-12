#include "camera_exposure.h"
#include <ArduinoJson.h>
#include <esp_camera.h>

namespace camera_exposure {
namespace {
WebServer *server=nullptr;
bool (*authorized)()=nullptr;
void reply(sensor_t *s,int status=200,bool applied=true){
    StaticJsonDocument<1024> d;
    d["schema"]=1; d["available"]=s!=nullptr; d["applied"]=applied;
    d["persistent"]=false;
    d["settings_semantics"]="driver_control_state_not_measured_exposure_duration";
    d["exposure_units"]="sensor_driver_aec_value_not_microseconds";
    d["capture_timestamp_semantics"]="first_DMA_buffer_not_exposure_start_midpoint_or_end";
    d["physical_exposure_time_uncertainty_s"]=nullptr;
    if(s){
        d["auto_exposure"]=bool(s->status.aec);
        d["auto_exposure_dsp"]=bool(s->status.aec2);
        d["exposure_value"]=s->status.aec_value;
        d["exposure_level"]=s->status.ae_level;
        d["auto_gain"]=bool(s->status.agc);
        d["gain"]=s->status.agc_gain;
    }
    String body;serializeJson(d,body);
    server->sendHeader("Cache-Control","no-store");
    server->send(status,"application/json",body);
}
void get(){if(authorized && !authorized())return;auto *s=esp_camera_sensor_get();reply(s,s?200:503);}
void post(){
    if(authorized && !authorized())return;
    auto *s=esp_camera_sensor_get();if(!s){reply(s,503,false);return;}
    if(server->arg("plain").length()>512){server->send(413,"application/json","{\"error\":\"body_too_large\"}");return;}
    StaticJsonDocument<512> d;
    if(deserializeJson(d,server->arg("plain")) || !d.is<JsonObject>() || d.size()==0){
        server->send(400,"application/json","{\"error\":\"nonempty_object_required\"}");return;
    }
    for(JsonPair p:d.as<JsonObject>()){
        String key=p.key().c_str();JsonVariant value=p.value();
        bool okay=false;
        if(key=="auto_exposure" || key=="auto_exposure_dsp" || key=="auto_gain")
            okay=value.is<bool>();
        else if(key=="exposure_value")okay=value.is<int>() && value.as<int>()>=9 && value.as<int>()<=1200;
        else if(key=="exposure_level")okay=value.is<int>() && value.as<int>()>=-2 && value.as<int>()<=2;
        else if(key=="gain")okay=value.is<int>() && value.as<int>()>=0 && value.as<int>()<=30;
        if(!okay){server->send(400,"application/json","{\"error\":\"invalid_exposure_field_or_value\"}");return;}
    }
    // Validate the complete request before changing any driver setting.
    bool okay=true;
    if(d.containsKey("auto_exposure"))okay=(s->set_exposure_ctrl(s,d["auto_exposure"].as<bool>())==0)&&okay;
    if(d.containsKey("auto_exposure_dsp"))okay=(s->set_aec2(s,d["auto_exposure_dsp"].as<bool>())==0)&&okay;
    if(d.containsKey("exposure_level"))okay=(s->set_ae_level(s,d["exposure_level"].as<int>())==0)&&okay;
    if(d.containsKey("exposure_value"))okay=(s->set_aec_value(s,d["exposure_value"].as<int>())==0)&&okay;
    if(d.containsKey("auto_gain"))okay=(s->set_gain_ctrl(s,d["auto_gain"].as<bool>())==0)&&okay;
    if(d.containsKey("gain"))okay=(s->set_agc_gain(s,d["gain"].as<int>())==0)&&okay;
    reply(s,okay?200:503,okay);
}
}
void begin(WebServer &http,bool (*authorize)()){
    server=&http;authorized=authorize;
    http.on("/api/camera/exposure",HTTP_GET,get);
    http.on("/api/camera/exposure",HTTP_POST,post);
}
}

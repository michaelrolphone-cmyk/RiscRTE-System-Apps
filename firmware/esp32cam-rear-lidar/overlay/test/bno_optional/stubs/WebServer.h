#pragma once
#include <functional>
#include <map>
#include <string>
#include <utility>
enum HTTPMethod { HTTP_GET, HTTP_POST };
class WebServer {
public:
    std::map<std::pair<std::string,HTTPMethod>,std::function<void()>> handlers;
    int responseCode=0;
    std::string responseBody, requestBody;
    void on(const char *path,HTTPMethod method,std::function<void()> callback){
        handlers[{path,method}]=callback;
    }
    std::string arg(const char *) const{return requestBody;}
    void send(int code,const char *,const std::string &body){
        responseCode=code;responseBody=body;
    }
    void request(const char *path,HTTPMethod method,const std::string &body=""){
        responseCode=0;responseBody.clear();requestBody=body;
        handlers.at({path,method})();
    }
};

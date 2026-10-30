#include "Policy.h"
#include "Journal.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
int main(int argc,char** argv){assert(argc==2);std::ifstream f(argv[1]);std::string source((std::istreambuf_iterator<char>(f)),{});Infra::Allocator memory(196608);JsonDocument doc(&memory);assert(Infra::parse(doc,source.data(),source.size()));assert(Infra::policy(doc));doc["configuration"]["bindings"][0]["timeout_ms"]=19000;assert(!Infra::policy(doc));doc["configuration"]["bindings"][0]["timeout_ms"]=5000;assert(Infra::policy(doc));doc["configuration"]["bindings"][0]["authorization_scope"][0]="admin:*";assert(!Infra::policy(doc));doc["configuration"]["bindings"][0]["authorization_scope"][0]=doc["endpoints"][0]["authorization_scope"][0];assert(Infra::policy(doc));doc["configuration"]["bindings"][0]["unexpected_safety_field"]=true;assert(!Infra::policy(doc));puts("Policy fixture: accepted; overload and unknown safety fields rejected");}

#include "bootstrap/AppDeclarations.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1]);std::ostringstream s;s<<f.rdbuf();JsonDocument doc;assert(!deserializeJson(doc,s.str()));RiscBoot::AppDeclarations declarations(doc.as<JsonObjectConst>());assert(declarations.error(32)==nullptr);std::cout<<"Actual Runtime declaration parser PASS: "<<declarations.size()<<" optional rows\n";}

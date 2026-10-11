#include "WebDavProperties.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
using namespace RiscWebDav;
static PropertyRequest parse(const std::string&s,unsigned expected){PropertyRequest p;assert(parseProperties(s.data(),s.size(),p)==expected);if(expected)assert(p.mode==PropertyMode::All&&!p.count);return p;}
int main(){
 assert(parse("",0).mode==PropertyMode::All);
 assert(parse("<propfind xmlns='DAV:'><propname/></propfind>",0).mode==PropertyMode::Names);
 auto p=parse("<D:propfind xmlns:D='DAV:'><D:prop><D:getetag/><X:custom xmlns:X='urn:a&amp;b'/><D:getetag/></D:prop></D:propfind>",0);
 assert(p.mode==PropertyMode::Named&&p.count==2&&!strcmp(p.namespaceUri(1),"urn:a&b")&&!strcmp(p.localName(1),"custom"));
 std::string xml="<propfind xmlns='DAV:'><prop><getetag/></prop></propfind>";
 auto copied=parse(xml,0);xml.assign(xml.size(),'x');assert(!strcmp(copied.localName(0),"getetag"));
 for(const char*s:{"<!DOCTYPE x><propfind xmlns='DAV:'/>","<propfind xmlns='DAV:'><allprop/><propname/></propfind>","<propfind xmlns='DAV:'><prop><bad:a/></prop></propfind>","<propfind xmlns='DAV:'><prop><a>&custom;</a></prop></propfind>","<propfind xmlns='DAV:'><prop></propfind>","<propfind xmlns='DAV:'><prop><a/></prop></propfind>trailing"})parse(s,400);
 parse(std::string(PropertyXmlMax+1,' '),413);
 std::string many="<propfind xmlns='DAV:'><prop>";for(unsigned i=0;i<PropertyCountMax+1;i++)many+="<getetag/>";many+="</prop></propfind>";parse(many,413);
 std::puts("PROPFIND parser: expanded names, duplicate coalescing, copied lifetime, unsafe XML and size/occurrence bounds PASS");
}

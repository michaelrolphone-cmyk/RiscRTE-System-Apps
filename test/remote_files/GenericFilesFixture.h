#pragma once
#include <RiscStorageFilesV1.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct GenericFilesFixture {
 struct Node {std::string bytes;bool directory=false;};
 struct Handle {std::string path;uint64_t offset=0;uint32_t flags=0;bool directory=false;std::vector<std::string> names;};
 std::map<std::string,Node> nodes{{"/",{"",true}},{"/book.txt",{"generic content",false}},{"/folder",{"",true}}};
 std::map<uint32_t,Handle> handles;uint32_t serial=0,calls=0;int32_t fault=0;bool closeFails=false;uint32_t readLimit=17,writeLimit=13;
 risc_storage_files_v1 api{};
 int32_t check(){++calls;return fault;}
 int32_t missing(const char*p){return nodes.count(p)?RISC_FILES_OK:RISC_FILES_NOT_FOUND;}
 GenericFilesFixture(){api.api_version=1;api.struct_size=sizeof(api);api.context=this;
 api.open=[](void*c,const char*p,uint32_t flags,uint32_t*out)->int32_t{auto&f=*(GenericFilesFixture*)c;*out=0;if(auto rc=f.check())return rc;auto i=f.nodes.find(p);if(i==f.nodes.end()){if(!(flags&RISC_FILES_CREATE))return RISC_FILES_NOT_FOUND;f.nodes[p]={};}else if(i->second.directory)return RISC_FILES_INVALID;else if((flags&(RISC_FILES_CREATE|RISC_FILES_EXCLUSIVE))==(RISC_FILES_CREATE|RISC_FILES_EXCLUSIVE))return RISC_FILES_EXISTS;if(flags&RISC_FILES_TRUNCATE)f.nodes[p].bytes.clear();*out=++f.serial;f.handles[*out]={p,0,flags,false,{}};return 0;};
 api.close=[](void*c,uint32_t h)->int32_t{auto&f=*(GenericFilesFixture*)c;++f.calls;if(f.closeFails||f.fault==RISC_FILES_RETAINED)return RISC_FILES_RETAINED;return f.handles.erase(h)?0:RISC_FILES_INVALID;};
 api.read=[](void*c,uint32_t h,void*out,uint32_t n,uint32_t*done)->int32_t{auto&f=*(GenericFilesFixture*)c;*done=0;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||!(i->second.flags&RISC_FILES_READ))return RISC_FILES_INVALID;auto&v=i->second;auto&s=f.nodes.at(v.path).bytes;n=std::min<uint64_t>(std::min(n,f.readLimit),s.size()-v.offset);memcpy(out,s.data()+v.offset,n);v.offset+=n;*done=n;return 0;};
 api.write=[](void*c,uint32_t h,const void*in,uint32_t n,uint32_t*done)->int32_t{auto&f=*(GenericFilesFixture*)c;*done=0;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||!(i->second.flags&RISC_FILES_WRITE))return RISC_FILES_INVALID;auto&v=i->second;auto&s=f.nodes.at(v.path).bytes;if(v.flags&RISC_FILES_APPEND)v.offset=s.size();n=std::min(n,f.writeLimit);if(v.offset+n>s.size())s.resize(v.offset+n);memcpy(&s[0]+v.offset,in,n);v.offset+=n;*done=n;return 0;};
 api.seek=[](void*c,uint32_t h,uint64_t n)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||n>f.nodes.at(i->second.path).bytes.size())return RISC_FILES_INVALID;i->second.offset=n;return 0;};
 api.info=[](void*c,uint32_t h,uint64_t*s,uint64_t*o)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end())return RISC_FILES_INVALID;*s=f.nodes.at(i->second.path).bytes.size();*o=i->second.offset;return 0;};
 api.sync=[](void*c,uint32_t h)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;return f.handles.count(h)?0:RISC_FILES_INVALID;};
 api.truncate=[](void*c,uint32_t h,uint64_t n)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||n>f.nodes.at(i->second.path).bytes.size())return RISC_FILES_INVALID;f.nodes.at(i->second.path).bytes.resize(n);return 0;};
 api.stat=[](void*c,const char*p,risc_storage_files_stat_v1*out)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.nodes.find(p);if(i==f.nodes.end())return RISC_FILES_NOT_FOUND;out->flags=i->second.directory?RISC_FILES_DIRECTORY:0;out->size=i->second.bytes.size();return 0;};
 api.mkdir=[](void*c,const char*p)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;return f.nodes.emplace(p,Node{"",true}).second?0:RISC_FILES_EXISTS;};
 api.rename=[](void*c,const char*a,const char*b)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;if(f.nodes.count(b))return RISC_FILES_EXISTS;auto i=f.nodes.find(a);if(i==f.nodes.end())return RISC_FILES_NOT_FOUND;f.nodes[b]=i->second;f.nodes.erase(i);return 0;};
 api.remove=[](void*c,const char*p)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;return f.nodes.erase(p)?0:RISC_FILES_NOT_FOUND;};
 api.dir_open=[](void*c,const char*p,uint32_t*out)->int32_t{auto&f=*(GenericFilesFixture*)c;*out=0;if(auto rc=f.check())return rc;auto i=f.nodes.find(p);if(i==f.nodes.end())return RISC_FILES_NOT_FOUND;if(!i->second.directory)return RISC_FILES_INVALID;Handle h{p,0,0,true,{}};std::string pre=std::string(p)=="/"?"/":std::string(p)+"/";for(auto&node:f.nodes)if(node.first.find(pre)==0){auto n=node.first.substr(pre.size());if(!n.empty()&&n.find('/')==std::string::npos)h.names.push_back(n);}*out=++f.serial;f.handles[*out]=h;return 0;};
 api.dir_next=[](void*c,uint32_t h,risc_storage_files_entry_v1*out)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||!i->second.directory)return RISC_FILES_INVALID;auto&v=i->second;if(v.offset==v.names.size())return RISC_FILES_END;auto&name=v.names[v.offset++];auto&node=f.nodes.at(v.path+(v.path=="/"?"":"/")+name);strcpy(out->name,name.c_str());out->flags=node.directory?RISC_FILES_DIRECTORY:0;out->size=node.bytes.size();return 0;};
 api.dir_close=api.close;
 api.dir_seek=[](void*c,uint32_t h,uint64_t n)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||!i->second.directory||n>i->second.names.size())return RISC_FILES_INVALID;i->second.offset=n;return 0;};
 api.dir_tell=[](void*c,uint32_t h,uint64_t*n)->int32_t{auto&f=*(GenericFilesFixture*)c;if(auto rc=f.check())return rc;auto i=f.handles.find(h);if(i==f.handles.end()||!i->second.directory)return RISC_FILES_INVALID;*n=i->second.offset;return 0;};
 api.dir_rewind=[](void*c,uint32_t h)->int32_t{auto&f=*(GenericFilesFixture*)c;return f.api.dir_seek(c,h,0);};
 }
};

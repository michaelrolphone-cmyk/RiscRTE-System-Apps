#pragma once
/* Shared File Browser ordering and normalized-path model. Natural ordering is
 * extracted from Apps/file_browser.c; no firmware UI or storage access. */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
static inline char fb_lower(char c) { return c>='A' && c<='Z' ? (char)(c+'a'-'A') : c; }
static inline int fb_name_compare(const char *a, bool ad, const char *b, bool bd) {
    if(ad!=bd)return ad?-1:1;
    while(*a && *b) {
        if(*a>='0' && *a<='9' && *b>='0' && *b<='9') {
            while(*a=='0')++a;
            while(*b=='0')++b;
            size_t an=0,bn=0;
            while(a[an]>='0' && a[an]<='9')++an;
            while(b[bn]>='0' && b[bn]<='9')++bn;
            if(an!=bn)return an<bn?-1:1;
            for(size_t i=0;i<an;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;
            a+=an;b+=bn;
        } else { char ac=fb_lower(*a++),bc=fb_lower(*b++);if(ac!=bc)return ac<bc?-1:1; }
    }
    return *a?1:*b?-1:0;
}
static inline bool fb_name_valid(const char *name,size_t capacity) {
    if(!name || !capacity || !name[0])return false;
    size_t n=0;for(;n<capacity && name[n];n++)
        if(name[n]=='/' || name[n]=='\\' || (unsigned char)name[n]<32)return false;
    return n<capacity && strcmp(name,".") && strcmp(name,"..");
}
static inline bool fb_join(const char *dir,const char *name,char *out,size_t cap) {
    if(!dir || dir[0]!='/' || !fb_name_valid(name,128))return false;
    size_t d=strlen(dir),n=strlen(name),separator=d>1;
    if(d+separator+n>=cap)return false;
    memcpy(out,dir,d);if(separator)out[d++]='/';memcpy(out+d,name,n+1);return true;
}
static inline bool fb_parent(char *path) {
    if(!path || path[0]!='/' || !strcmp(path,"/"))return false;
    char *last=strrchr(path,'/');if(last==path)path[1]=0;else *last=0;return true;
}
static inline bool fb_contains(const char *name,const char *query) {
    if(!*query)return true;
    for(;*name;name++) {size_t i=0;while(query[i] && name[i] && fb_lower(query[i])==fb_lower(name[i]))++i;if(!query[i])return true;}
    return false;
}

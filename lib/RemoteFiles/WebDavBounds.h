#pragma once
#include <cstddef>
namespace RiscWebDav { namespace detail {
inline size_t boundedLength(const char* text,size_t capacity){size_t n=0;while(n<capacity&&text[n])++n;return n;}
inline bool asciiAlphaNumeric(unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');}
inline unsigned char asciiLower(unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
} }

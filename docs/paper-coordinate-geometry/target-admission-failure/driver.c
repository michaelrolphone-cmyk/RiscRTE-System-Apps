#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
extern bool esp_elf_validate_file(const uint8_t *,size_t);
int main(int argc,char **argv){if(argc!=2)return 2; FILE*f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);uint8_t*b=malloc(n);if(!b||fread(b,1,n,f)!=n)return 2;fclose(f);bool ok=esp_elf_validate_file(b,n);free(b);printf("valid=%d\n",ok);return ok?0:1;}

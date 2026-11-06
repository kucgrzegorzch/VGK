#include "sha256.h"

#include <stdio.h>
#include <string.h>

static int expect_hex(const char *label, const uint8_t digest[32], const char *expected)
{
    static const char hex[]="0123456789abcdef";
    char actual[65];
    for (size_t i=0;i<32;++i) {
        actual[i*2]=hex[digest[i]>>4];
        actual[i*2+1]=hex[digest[i]&15U];
    }
    actual[64]='\0';
    if (strcmp(actual,expected)!=0) {
        fprintf(stderr,"BŁĄD %s: otrzymano %s\n",label,actual);
        return 1;
    }
    return 0;
}

int main(void)
{
    uint8_t digest[32];
    int failures=0;
    vgk_sha256("",0,digest);
    failures+=expect_hex("pusty tekst",digest,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    vgk_sha256("abc",3,digest);
    failures+=expect_hex("abc",digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    VGK_SHA256_CONTEXT context;
    vgk_sha256_init(&context);
    vgk_sha256_update(&context,"a",1);
    vgk_sha256_update(&context,"b",1);
    vgk_sha256_update(&context,"c",1);
    vgk_sha256_finish(&context,digest);
    failures+=expect_hex("dane dzielone na części",digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    if (failures!=0) return 1;
    puts("OK: wszystkie testy SHA-256 zakończone powodzeniem.");
    return 0;
}

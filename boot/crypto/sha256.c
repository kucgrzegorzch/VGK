#include "sha256.h"

/* Small freestanding byte helpers: the UEFI build has no C library. */
static void copy_bytes(void *destination, const void *source, size_t length)
{
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    for (size_t i = 0; i < length; ++i)
        out[i] = in[i];
}

static void set_bytes(void *destination, uint8_t value, size_t length)
{
    uint8_t *out = (uint8_t *)destination;
    for (size_t i = 0; i < length; ++i)
        out[i] = value;
}

static const uint32_t round_constants[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
    0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
    0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
    0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
    0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
    0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
    0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
    0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
    0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

static uint32_t rotate_right(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32U - count));
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void transform(VGK_SHA256_CONTEXT *context, const uint8_t block[64])
{
    uint32_t words[64];
    for (size_t i = 0; i < 16; ++i)
        words[i] = read_be32(block + i * 4);
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(words[i - 15], 7) ^
                      rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
        uint32_t s1 = rotate_right(words[i - 2], 17) ^
                      rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    uint32_t a=context->state[0], b=context->state[1];
    uint32_t c=context->state[2], d=context->state[3];
    uint32_t e=context->state[4], f=context->state[5];
    uint32_t g=context->state[6], h=context->state[7];

    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1=rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25);
        uint32_t choice=(e&f)^(~e&g);
        uint32_t t1=h+s1+choice+round_constants[i]+words[i];
        uint32_t s0=rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22);
        uint32_t majority=(a&b)^(a&c)^(b&c);
        uint32_t t2=s0+majority;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    context->state[0]+=a; context->state[1]+=b;
    context->state[2]+=c; context->state[3]+=d;
    context->state[4]+=e; context->state[5]+=f;
    context->state[6]+=g; context->state[7]+=h;
}

void vgk_sha256_init(VGK_SHA256_CONTEXT *context)
{
    static const uint32_t initial[8] = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    copy_bytes(context->state, initial, sizeof(initial));
    context->bit_count=0;
    context->block_used=0;
}

void vgk_sha256_update(VGK_SHA256_CONTEXT *context, const void *data, size_t length)
{
    const uint8_t *input=(const uint8_t *)data;
    if (length == 0) return;
    context->bit_count += (uint64_t)length * 8U;
    while (length != 0) {
        size_t available=64U-context->block_used;
        size_t take=length<available?length:available;
        copy_bytes(context->block+context->block_used,input,take);
        context->block_used+=take; input+=take; length-=take;
        if (context->block_used==64U) {
            transform(context,context->block);
            context->block_used=0;
        }
    }
}

void vgk_sha256_finish(VGK_SHA256_CONTEXT *context, uint8_t digest[32])
{
    uint64_t original_bits=context->bit_count;
    context->block[context->block_used++]=0x80U;
    if (context->block_used>56U) {
        set_bytes(context->block+context->block_used,0,64U-context->block_used);
        transform(context,context->block);
        context->block_used=0;
    }
    set_bytes(context->block+context->block_used,0,56U-context->block_used);
    for (size_t i=0;i<8;++i)
        context->block[63U-i]=(uint8_t)(original_bits>>(i*8U));
    transform(context,context->block);
    for (size_t i=0;i<8;++i) write_be32(digest+i*4,context->state[i]);
    set_bytes(context,0,sizeof(*context));
}

void vgk_sha256(const void *data, size_t length, uint8_t digest[32])
{
    VGK_SHA256_CONTEXT context;
    vgk_sha256_init(&context);
    vgk_sha256_update(&context,data,length);
    vgk_sha256_finish(&context,digest);
}

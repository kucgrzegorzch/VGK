#ifndef VGK_ED25519_H
#define VGK_ED25519_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    VGK_ED25519_VALID = 0,
    VGK_ED25519_INVALID_ARGUMENT = 1,
    VGK_ED25519_INVALID_SIGNATURE = 2
} VGK_ED25519_RESULT;

/* Public key: 32 bytes. Signature: 64 bytes. Private keys are never needed here. */
VGK_ED25519_RESULT vgk_ed25519_verify(
    const uint8_t public_key[32],
    const uint8_t *message,
    size_t message_length,
    const uint8_t signature[64]
);

#endif

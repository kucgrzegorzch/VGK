#include "ed25519.h"
#include "monocypher/monocypher-ed25519.h"

VGK_ED25519_RESULT vgk_ed25519_verify(
    const uint8_t public_key[32],
    const uint8_t *message,
    size_t message_length,
    const uint8_t signature[64])
{
    if (public_key == NULL || signature == NULL ||
        (message == NULL && message_length != 0)) {
        return VGK_ED25519_INVALID_ARGUMENT;
    }

    return crypto_ed25519_check(signature, public_key, message, message_length) == 0
        ? VGK_ED25519_VALID
        : VGK_ED25519_INVALID_SIGNATURE;
}

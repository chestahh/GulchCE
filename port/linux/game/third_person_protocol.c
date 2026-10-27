#include "cseries.h"
#include "third_person_protocol.h"
#include "mcc_protocol.h"
#include "networking/network_messages.h"
#include "bungie_net/common/message_header.h"

/* MCC retains ownership of its optional inner envelope and capability checks.
 * This outer envelope only establishes the camera branch's wire contract. */
static int third_person_protocol_inner_valid(byte const *message, unsigned short bytes)
{
    word header;
    if (bytes <= sizeof(header) || bytes > THIRD_PERSON_PROTOCOL_JOIN_MAX - 8) return 0;
    memcpy(&header, message, sizeof(header));
    if (GET_MESSAGE_FLAGS(header) || GET_MESSAGE_SIZE(header) != bytes) return 0;
    if (GET_MESSAGE_TYPE(header) == _message_type_packet)
        return message[bytes - 1] == _message_client_join_game_request;
    return GET_MESSAGE_TYPE(header) == 2 && bytes > 10 &&
        bytes <= MCC_PROTOCOL_JOIN_MAX && message[2] == MCC_PROTOCOL_JOIN_KIND;
}

void const *third_person_protocol_join(void const *message, unsigned short bytes, void *buffer)
{
    byte *out = buffer;
    if (!third_person_protocol_inner_valid(message, bytes)) return NULL;
    build_message_header((word *)out, bytes + 8, 2, 0);
    out[2] = THIRD_PERSON_PROTOCOL_JOIN_KIND;
    out[3] = THIRD_PERSON_PROTOCOL_JOIN_REVISION;
    out[4] = HALO_PORT_NETWORK_VERSION & 255;
    out[5] = HALO_PORT_NETWORK_VERSION >> 8;
    out[6] = out[7] = 0;
    memcpy(out + 8, message, bytes);
    return out;
}

/* Stateless and required before admission, including first-person-only games.
 * Advertising a version alone cannot protect a host from a direct join. */
int third_person_protocol_unwrap(void const *message, unsigned short bytes,
    void *inner, unsigned short *inner_bytes)
{
    byte const *in = message;
    word header;
    if (bytes <= 10 || bytes > THIRD_PERSON_PROTOCOL_JOIN_MAX ||
        in[2] != THIRD_PERSON_PROTOCOL_JOIN_KIND || in[3] != THIRD_PERSON_PROTOCOL_JOIN_REVISION ||
        in[4] != (HALO_PORT_NETWORK_VERSION & 255) ||
        in[5] != (HALO_PORT_NETWORK_VERSION >> 8) || in[6] || in[7]) return 0;
    memcpy(&header, in, sizeof(header));
    if (GET_MESSAGE_FLAGS(header) || GET_MESSAGE_TYPE(header) != 2 ||
        GET_MESSAGE_SIZE(header) != bytes || !third_person_protocol_inner_valid(in + 8, bytes - 8)) return 0;
    *inner_bytes = bytes - 8;
    memcpy(inner, in + 8, *inner_bytes);
    return 1;
}

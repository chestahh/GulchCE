#include "cseries.h"
#include "mutator_protocol.h"
#include "mutator_share.h"
#include "third_person_protocol.h"
#include "networking/network_messages.h"
#include "bungie_net/common/message_header.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"

int config_boolean(char const *name);
int port_mutators_selected(void);
struct network_game *network_game_server_get_game(struct network_game_server *server);
boolean network_game_server_send_game_data_pregame(struct network_game_server *server);

/* Menu selection is explicit opt-in too. Setup can enable it only before
 * remote admission/lobby lock; afterward the agreement is immutable. */
static int mutator_host, mutator_locked;
static unsigned int mutator_host_version, mutator_client_version;

void mutator_session_host_begin(void)
{
    mutator_share_host_reset();
    mutator_host = 1;
    mutator_locked = 0;
    mutator_client_version = 0;
    mutator_host_version = (config_boolean("network.mutators") || port_mutators_selected()) ?
        (third_person_session_active() ? MUTATOR_CAMERA_PROTOCOL_VERSION : MUTATOR_PROTOCOL_VERSION) : 0;
    if (mutator_host_version) third_person_session_lock();
}

void mutator_session_host_end(void)
{
    mutator_share_host_reset();
    mutator_host = mutator_locked = 0;
    mutator_host_version = mutator_client_version = 0;
}

/* Called after the user keeps MUTATOR OPTIONS. A local-only setup host has
 * no remote client whose accepted stock contract could be invalidated. The
 * loopback client uses host-owned session state, just as camera setup does. */
int mutator_session_enable_selected(void)
{
    struct network_game_server *server;
    struct network_game *game;
    if (!mutator_host || mutator_host_version || !port_mutators_selected()) return 1;
    server = global_network_game_server_get();
    if (!server || mutator_locked) return 0;
    game = network_game_server_get_game(server);
    if (!game || game->machine_count > 1) return 0;
    mutator_host_version = third_person_session_active() ?
        MUTATOR_CAMERA_PROTOCOL_VERSION : MUTATOR_PROTOCOL_VERSION;
    third_person_session_lock();
    network_game_server_send_game_data_pregame(server);
    return 1;
}

void mutator_session_lock(void)
{
    if (mutator_host) mutator_locked = 1;
    if (mutator_host && mutator_host_version) third_person_session_lock();
}

int mutator_session_active(void)
{
    return (mutator_host ? mutator_host_version : mutator_client_version) != 0;
}

void mutator_session_client_begin(unsigned int version)
{
    mutator_client_version = version == MUTATOR_PROTOCOL_VERSION ||
        version == MUTATOR_CAMERA_PROTOCOL_VERSION ? version : 0;
}

void mutator_session_client_clear(void) { mutator_client_version = 0; }

unsigned int mutator_session_version(char const *map)
{
    unsigned int version = mutator_host ? mutator_host_version : mutator_client_version;
    return version ? version : third_person_session_version(map);
}

int mutator_session_compatible(char const *map, unsigned int version, unsigned int flags)
{
    if (!(flags & HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG)) return 0;
    return version == MUTATOR_PROTOCOL_VERSION || version == MUTATOR_CAMERA_PROTOCOL_VERSION ||
        third_person_session_compatible(map, version, flags);
}

static int mutator_protocol_inner_valid(byte const *message, unsigned short bytes, unsigned int version)
{
    word header;
    word camera_inner[THIRD_PERSON_PROTOCOL_JOIN_MAX / sizeof(word)];
    unsigned short camera_bytes;
    if (version == MUTATOR_CAMERA_PROTOCOL_VERSION)
        return third_person_protocol_unwrap(message, bytes, camera_inner, &camera_bytes);
    if (version != MUTATOR_PROTOCOL_VERSION || bytes <= sizeof(header) ||
        bytes > THIRD_PERSON_PROTOCOL_JOIN_MAX - 8) return 0;
    memcpy(&header, message, sizeof(header));
    return !GET_MESSAGE_FLAGS(header) && GET_MESSAGE_SIZE(header) == bytes &&
        GET_MESSAGE_TYPE(header) == _message_type_packet &&
        message[bytes - 1] == _message_client_join_game_request;
}

void const *mutator_protocol_join(void const *message, unsigned short bytes, void *buffer)
{
    byte *out = buffer;
    unsigned int version = mutator_session_version(NULL);
    if (!mutator_session_active() || !mutator_protocol_inner_valid(message, bytes, version)) return NULL;
    build_message_header((word *)out, bytes + 8, 2, 0);
    out[2] = MUTATOR_PROTOCOL_JOIN_KIND;
    out[3] = MUTATOR_PROTOCOL_JOIN_REVISION;
    out[4] = version & 255;
    out[5] = version >> 8;
    out[6] = out[7] = 0;
    memcpy(out + 8, message, bytes);
    return out;
}

int mutator_protocol_unwrap(void const *message, unsigned short bytes,
    void *inner, unsigned short *inner_bytes)
{
    byte const *in = message;
    word header;
    unsigned int version = mutator_session_version(NULL);
    /* Never admit an opt-in envelope on a stock or camera-only host. */
    if (!mutator_host || !mutator_host_version || bytes <= 10 || bytes > MUTATOR_PROTOCOL_JOIN_MAX ||
        in[2] != MUTATOR_PROTOCOL_JOIN_KIND || in[3] != MUTATOR_PROTOCOL_JOIN_REVISION ||
        in[4] != (version & 255) || in[5] != (version >> 8) || in[6] || in[7]) return 0;
    memcpy(&header, in, sizeof(header));
    if (GET_MESSAGE_FLAGS(header) || GET_MESSAGE_TYPE(header) != 2 || GET_MESSAGE_SIZE(header) != bytes ||
        !mutator_protocol_inner_valid(in + 8, bytes - 8, version)) return 0;
    *inner_bytes = bytes - 8;
    memcpy(inner, in + 8, *inner_bytes);
    return 1;
}

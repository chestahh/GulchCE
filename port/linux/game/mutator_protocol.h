/* Opt-in mutator admission, separate from the base and camera wire agreements. */
#ifndef MUTATOR_PROTOCOL_H
#define MUTATOR_PROTOCOL_H

#define MUTATOR_PROTOCOL_VERSION 32747
#define MUTATOR_CAMERA_PROTOCOL_VERSION 32748
#define MUTATOR_PROTOCOL_JOIN_KIND 103
#define MUTATOR_PROTOCOL_JOIN_REVISION 1
#define MUTATOR_PROTOCOL_JOIN_MAX 528

int mutator_session_active(void);
void mutator_session_host_begin(void);
void mutator_session_host_end(void);
void mutator_session_lock(void);
void mutator_session_client_begin(unsigned int version);
void mutator_session_client_clear(void);
unsigned int mutator_session_version(char const *map);
int mutator_session_compatible(char const *map, unsigned int version, unsigned int flags);
/* Word-aligned buffers must hold JOIN_MAX bytes. Camera wrapping comes first. */
void const *mutator_protocol_join(void const *message, unsigned short bytes, void *buffer);
int mutator_protocol_unwrap(void const *message, unsigned short bytes,
    void *inner, unsigned short *inner_bytes);
#endif

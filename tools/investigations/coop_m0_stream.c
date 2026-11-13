/* M0 fixture: actual P2P stream functions and vendored KCP over loopback UDP.
   Peer encryption/admission and game TCP sockets are controlled boundaries.
   The relay uses a deterministic virtual clock; timings are not benchmarks. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ikcp.h"
#define CHECK(c,m) do { if(!(c)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,m);exit(1);} } while(0)
#include "stream_types.inc"

struct peer { int unused; };
static struct {
    struct peer peers[2];
    struct stream streams[MAXIMUM_STREAMS];
    IUINT32 finished[16];
    unsigned finished_next;
} p2p;
static unsigned long clock_ms;
static unsigned long p2p_now(void) { return clock_ms; }
static int elapsed(unsigned long start,unsigned long duration) {return clock_ms-start>=duration;}
static void close_socket(int *socket) {*socket=-1;}
static void remember_closed(int a,int b,int c,int d) {(void)a;(void)b;(void)c;(void)d;}
static int would_block(void) {return 1;}
static void stream_opened(struct stream *s,void *bytes,int size) {
    (void)s;(void)bytes;(void)size;CHECK(0,"open negotiation outside this fixture");
}

enum { PAYLOAD_SIZE=1609776, QUEUE_CAPACITY=8192 };
static SOCKET wire[2],relay;
static struct sockaddr_in endpoints[2],relay_address;
static struct queued_packet {
    unsigned long due,sequence;
    int size,destination,used;
    unsigned char bytes[MAXIMUM_INNER_SIZE];
} queue[QUEUE_CAPACITY];
static unsigned long random_state,serial,last_sequence[2];
static unsigned long emitted,delivered,dropped,duplicated,reordered,queued,queue_peak;
static unsigned long input_bytes,output_bytes,heartbeats,stall_heartbeats;
static int peak_pending,peak_waiting,peak_receive,blocked_writes;
static int impair,wire_stall,sink_stall;
static unsigned char value_at(unsigned long offset) {return (unsigned char)((offset*37+(offset>>8)*13)^0xa5);}
static unsigned long random_next(void) {
    random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
    return random_state;
}
static int stalled(void) {return clock_ms>=100&&clock_ms<2100;}
static int sink_blocked(void) {
    /* In the combined case, follow the wire stall with a slow reader so both
       boundaries are exercised; blocking an empty sink proves nothing. */
    return wire_stall?(clock_ms>=2100&&clock_ms<4100):stalled();
}

static int posix_socket_recv(int socket,void *buffer,int capacity,int flags) {
    unsigned long i,size;
    (void)flags;
    if(socket!=0||input_bytes==PAYLOAD_SIZE)return -1;
    size=PAYLOAD_SIZE-input_bytes;if(size>(unsigned)capacity)size=(unsigned)capacity;
    for(i=0;i<size;i++)((unsigned char *)buffer)[i]=value_at(input_bytes+i);
    input_bytes+=size;return (int)size;
}
static int posix_socket_send(int socket,const void *buffer,int size,int flags) {
    int i;
    (void)flags;CHECK(socket==1,"data only flows to receiver");
    if(sink_stall&&sink_blocked()){blocked_writes++;return -1;}
    /* Force partial writes, retaining the production pending-buffer handling. */
    if(size>173)size=173;
    CHECK(output_bytes+(unsigned)size<=PAYLOAD_SIZE,"no duplicate delivered bytes");
    for(i=0;i<size;i++)CHECK(((const unsigned char *)buffer)[i]==value_at(output_bytes+i),"exact ordered payload");
    output_bytes+=size;return size;
}
static void peer_send(struct peer *peer,const unsigned char *bytes,int size) {
    int sender=(int)(peer-p2p.peers);
    CHECK(sender>=0&&sender<2,"known peer");
    CHECK(sendto(wire[sender],(const char *)bytes,size,0,(struct sockaddr *)&relay_address,sizeof(relay_address))==size,
        "loopback sender writes whole UDP datagram");
    emitted++;
}
#include "stream_functions.inc"

static SOCKET udp_socket(struct sockaddr_in *address) {
    SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    u_long nonblocking=1;int length=sizeof(*address),buffer=1024*1024;
    CHECK(s!=INVALID_SOCKET,"UDP socket");
    memset(address,0,sizeof(*address));address->sin_family=AF_INET;address->sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(bind(s,(struct sockaddr *)address,sizeof(*address))==0,"loopback ephemeral bind");
    CHECK(getsockname(s,(struct sockaddr *)address,&length)==0,"bound address");
    CHECK(ioctlsocket(s,FIONBIO,&nonblocking)==0,"nonblocking UDP");
    CHECK(setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char *)&buffer,sizeof(buffer))==0,"receive buffer");
    return s;
}
static void enqueue(const unsigned char *bytes,int size,int destination,unsigned long sequence) {
    int slot;
    for(slot=0;slot<QUEUE_CAPACITY&&queue[slot].used;slot++){}
    CHECK(slot<QUEUE_CAPACITY,"bounded fault relay");
    queue[slot].used=1;queue[slot].destination=destination;queue[slot].size=size;queue[slot].sequence=sequence;
    queue[slot].due=clock_ms+(impair?30+random_next()%61:10);
    if(wire_stall&&bytes[0]==_packet_stream&&queue[slot].due>=100&&queue[slot].due<2100)queue[slot].due=2100;
    memcpy(queue[slot].bytes,bytes,(size_t)size);
    queued++;if(queued>queue_peak)queue_peak=queued;
}
static void relay_receive(void) {
    unsigned char bytes[MAXIMUM_INNER_SIZE];
    struct sockaddr_in from;int length,size,destination;
    for(;;){
        length=sizeof(from);size=recvfrom(relay,(char *)bytes,sizeof(bytes),0,(struct sockaddr *)&from,&length);
        if(size==SOCKET_ERROR){CHECK(WSAGetLastError()==WSAEWOULDBLOCK,"relay receive");break;}
        CHECK(size>0,"nonempty relay datagram");
        CHECK(from.sin_port==endpoints[0].sin_port||from.sin_port==endpoints[1].sin_port,"private sender");
        destination=from.sin_port==endpoints[0].sin_port?1:0;serial++;
        if(impair&&bytes[0]==_packet_stream&&random_next()%10==0){dropped++;continue;}
        enqueue(bytes,size,destination,serial);
        if(impair&&bytes[0]==_packet_stream&&random_next()%13==0){enqueue(bytes,size,destination,serial);duplicated++;}
    }
}
static void relay_deliver(void) {
    int i,slot;
    for(slot=0;slot<QUEUE_CAPACITY;slot++){
        i=impair?QUEUE_CAPACITY-1-slot:slot;
        struct queued_packet *p=&queue[i];
        if(!p->used||p->due>clock_ms)continue;
        CHECK(sendto(relay,(const char *)p->bytes,p->size,0,(struct sockaddr *)&endpoints[p->destination],sizeof(endpoints[0]))==p->size,
            "loopback relay writes whole datagram");
        if(p->sequence<last_sequence[p->destination])reordered++;
        last_sequence[p->destination]=p->sequence;p->used=0;queued--;delivered++;
    }
}
static void endpoint_receive(int endpoint) {
    unsigned char bytes[MAXIMUM_INNER_SIZE];int size;
    for(;;){
        size=recv(wire[endpoint],(char *)bytes,sizeof(bytes),0);
        if(size==SOCKET_ERROR){CHECK(WSAGetLastError()==WSAEWOULDBLOCK,"endpoint receive");break;}
        CHECK(size>0,"nonempty endpoint datagram");
        if(bytes[0]==0x7e){heartbeats++;if(stalled())stall_heartbeats++;continue;}
        CHECK(bytes[0]==_packet_stream,"production stream envelope");
        CHECK(ikcp_input(p2p.streams[endpoint].kcp,(char *)bytes+1,size-1)==0,"KCP datagram admitted");
    }
}

int main(int argc,char **argv) {
    WSADATA ws;struct stream *sender,*receiver;int i,completed=0;
    const char *name=argc>1?argv[1]:"normal";
    random_state=argc>2?(unsigned long)strtoul(argv[2],NULL,10):1;
    impair=!strcmp(name,"impaired")||!strcmp(name,"combined");
    wire_stall=!strcmp(name,"wire-stall")||!strcmp(name,"combined");
    sink_stall=!strcmp(name,"sink-stall")||!strcmp(name,"combined");
    CHECK(WSAStartup(MAKEWORD(2,2),&ws)==0,"Winsock initialize");
    wire[0]=udp_socket(&endpoints[0]);wire[1]=udp_socket(&endpoints[1]);relay=udp_socket(&relay_address);
    sender=stream_new(0,0x4d300001);receiver=stream_new(1,0x4d300001);
    CHECK(sender&&receiver,"production stream allocation");
    sender->socket=0;receiver->socket=1;sender->state=receiver->state=_stream_open_state;
    for(clock_ms=0;clock_ms<60000;clock_ms+=LOOP_INTERVAL){
        unsigned char heartbeat=0x7e;
        stream_readable(sender);
        for(i=0;i<2;i++){
            stream_update(&p2p.streams[i]);
            CHECK(p2p.streams[i].used,"stream remains live");
            if(p2p.streams[i].pending_size>peak_pending)peak_pending=p2p.streams[i].pending_size;
            if(ikcp_waitsnd(p2p.streams[i].kcp)>peak_waiting)peak_waiting=ikcp_waitsnd(p2p.streams[i].kcp);
            if((int)(p2p.streams[i].kcp->nrcv_que+p2p.streams[i].kcp->nrcv_buf)>peak_receive)
                peak_receive=(int)(p2p.streams[i].kcp->nrcv_que+p2p.streams[i].kcp->nrcv_buf);
        }
        CHECK(peak_pending<=STREAM_BUFFER_SIZE&&peak_waiting<=STREAM_WINDOW,"production backpressure bounds");
        if(clock_ms%100==0)peer_send(&p2p.peers[0],&heartbeat,1);
        relay_receive();relay_deliver();endpoint_receive(0);endpoint_receive(1);
        if(output_bytes==PAYLOAD_SIZE&&!ikcp_waitsnd(sender->kcp)){completed=1;break;}
    }
    if(!completed)fprintf(stderr,"timeout input=%lu output=%lu emitted=%lu delivered=%lu queued=%lu waiting=%d\n",
        input_bytes,output_bytes,emitted,delivered,queued,ikcp_waitsnd(sender->kcp));
    CHECK(completed&&input_bytes==PAYLOAD_SIZE,"whole payload delivered within virtual deadline");
    CHECK(heartbeats>0,"independent datagrams delivered");
    if(wire_stall||sink_stall)CHECK(stall_heartbeats>10,"unreliable traffic continues through reliable stall");
    if(sink_stall)CHECK(blocked_writes>0&&peak_pending>0,"receiver backpressure exercised");
    if(impair)CHECK(dropped>0&&duplicated>0&&reordered>0,"all requested faults exercised");
    printf("{\"case\":\"%s\",\"bytes\":%lu,\"virtual_ms\":%lu,\"emitted\":%lu,\"delivered\":%lu,"
        "\"dropped\":%lu,\"duplicates\":%lu,\"reordered\":%lu,\"relay_peak\":%lu,\"pending_peak\":%d,"
        "\"waiting_peak\":%d,\"receive_peak\":%d,\"heartbeats\":%lu,\"stall_heartbeats\":%lu,\"blocked_writes\":%d}\n",
        name,output_bytes,clock_ms,emitted,delivered,dropped,duplicated,reordered,queue_peak,peak_pending,
        peak_waiting,peak_receive,heartbeats,stall_heartbeats,blocked_writes);
    stream_free(sender);stream_free(receiver);
    closesocket(wire[0]);closesocket(wire[1]);closesocket(relay);WSACleanup();return 0;
}

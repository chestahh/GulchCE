#include "harness.h"
#define _transport_result_operation_would_block (-2)
#define _transport_error_connection_lost (-3)
#define _connection_closed_bit 0
#define SET_FLAG(f,b,v) ((f)|=FLAG(b))
#define MILLISECONDS_PER_SECOND 1000
struct queue {long free,bytes;};
struct network_connection {void *reliable_endpoint;struct queue *reliable_incoming_queue;long flags;unsigned long last_keep_alive_time;};
static int reads,read_result;
static long requested;
static unsigned long system_milliseconds(void) {return 20;}
static long circular_queue_free_space(struct queue *q) {return q->free;}
static int endpoint_connected(void *e) {(void)e;return 1;}
static int endpoint_blocking(void *e) {(void)e;return 0;}
static int endpoint_readable(void *e,int t) {(void)e;(void)t;return 1;}
static long read_endpoint(void *e,void *b,long size) {(void)e;reads++;requested=size;if(read_result)return read_result;memset(b,42,size);return size;}
static int circular_queue_queue_data(struct queue *q,void const *b,long bytes) {CHECK(bytes<=q->free,"queue bounds");CHECK(((byte const *)b)[bytes-1]==42,"last byte received");q->free-=bytes;q->bytes+=bytes;return 1;}
#define network_connection_log_traffic_event(...) ((void)0)
#define error(...) ((void)0)
#include "under_test.inc"
int main(int argc,char **argv) {
 struct queue q={32768,0};struct network_connection c={&q,&q,0,0};
 CHECK(argc==2,"case");char const *case_name=argv[1];
 CASE("partial") {q.free=123;}
 CASE("would-block") {read_result=_transport_result_operation_would_block;}
 CASE("closed") {read_result=_transport_error_connection_lost;}
 int success=network_connection_idle_client_reliable_endpoint(&c);
 CHECK(reads==1,"one socket read fills the available queue");
 CHECK(requested==(!strcmp(case_name,"partial")?123:32768),"bounded 32 KB request");
 if(read_result)CHECK(!q.bytes&&!c.last_keep_alive_time,"no phantom data or keepalive on error");
 else CHECK(!q.free&&q.bytes==requested&&c.last_keep_alive_time==20,"complete read queued");
 CHECK(success==!!strcmp(case_name,"closed"),"would-block versus connection loss");
 CHECK(!!c.flags==!strcmp(case_name,"closed"),"closed flag retained");return 0;
}

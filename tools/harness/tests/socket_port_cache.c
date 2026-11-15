#include "harness.h"
typedef unsigned int SOCKET;
#define WSAAPI
#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOL_SOCKET 1
#define SO_TYPE 1
struct sockaddr_in {int sin_family;unsigned short sin_port;};
static int queries,closed,failed;
static unsigned short port,noted;
static int posix_socket_getsockname(int s,struct sockaddr_in *b,int *n) {(void)s;(void)n;queries++;b->sin_family=AF_INET;b->sin_port=port;return failed?-1:0;}
static int posix_socket_getsockopt(int s,int a,int b,int *type,int *n) {(void)s;(void)a;(void)b;(void)n;*type=SOCK_DGRAM;return 0;}
static void p2p_socket_closed(int s,unsigned short p) {(void)s;noted=p;}
static void delayed_closed(int s) {(void)s;}
static int posix_socket_close(int s) {(void)s;closed++;return 0;}
static int winsock_result(int r) {return r;}
#include "under_test.inc"
int main(void) {
 CHECK(!socket_port(7)&&queries==1,"unbound port zero");
 failed=1;CHECK(!socket_port(7)&&queries==2,"lookup failure");failed=0;
 port=1234;CHECK(socket_port(7)==1234&&queries==3,"implicit bind observed");
 CHECK(socket_port(7)==1234&&queries==3,"bound socket avoids syscall");
 CHECK(!halo_ws_closesocket(7)&&closed==1&&noted==1234,"close notifies original port");
 port=2345;CHECK(socket_port(7)==2345&&queries==4,"reused socket does not inherit port");
 CHECK(socket_port(CACHED_SOCKET_PORTS)==2345,"large handle fallback");
 port=3456;CHECK(socket_port(CACHED_SOCKET_PORTS)==3456,"large handle re-queries");
 CHECK(!halo_ws_closesocket((SOCKET)-1),"invalid handle stays outside array");return 0;
}

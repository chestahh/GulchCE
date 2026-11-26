"""Compile real opt-in session/envelope code and the real ingress gates."""
from pathlib import Path
import re
import subprocess
from harness import enum_with, function
from test_mcc_identity import compile_tool

ROOT = Path(__file__).resolve().parents[1]


def test_mutator_admission(tmp_path):
    def implementation(name):
        return '\n'.join(line for line in (ROOT / ('port/linux/game/' + name + '.c')).read_text().splitlines()
                         if not line.startswith('#include'))
    server = (ROOT / 'source/networking/network_server_message_handler.c').read_text()
    gate = function(server, 'network_game_server_handle_client_message')
    gate = gate[:gate.index('\tmessage_type =')] + '(void)result;(void)message_type;(void)packet_type;return TRUE;\n}\n'
    menu_source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    menu_save = function(menu_source, 'mutators_save')
    lobby_open = function(menu_source, 'lobby_join_reset')
    limits = (ROOT / 'port/linux/include/halo_port_limits.h').read_text()
    version = re.search(r'^#define HALO_PORT_NETWORK_VERSION \d+$', limits, re.M).group()
    code = r'''
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "third_person_protocol.h"
#include "mutator_protocol.h"
typedef unsigned char byte;
typedef unsigned short word;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define match_assert(...) ((void)0)
#define HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG 1
#define THIRD_PERSON_POLICY_MASK 48
#define THIRD_PERSON_POLICY_SHIFT 4
struct network_game {int machine_count;unsigned char cooperative_flags;};
struct network_game_server {struct network_game game;};
struct network_game_server_client_machine {int joined;};
static struct network_game_server *host;
static int optin, selected, settings_sent, notices;
void mutator_share_host_reset(void){}
static struct {int adding,controller;} lobby_join;
static void halo_virtual_gamepads_start(void){}
#define NONE -1
static int port_mutators_save(void){return 1;}
static boolean campaign_fail(void){return FALSE;}
static void display_error_text_deferred(wchar_t const *text,int unused){(void)text;(void)unused;notices++;}
int port_mutators_selected(void){return selected;}
static char const *mode="opence_crossplay";
char const *config_string(char const *name){(void)name;return mode;}
int config_boolean(char const *name){return !strcmp(name,"network.mutators") && optin;}
int config_write(char const *name,char const *value){(void)name;mode=value;return 1;}
static struct network_game_server *global_network_game_server_get(void){return host;}
struct network_game *network_game_server_get_game(struct network_game_server *s){return &s->game;}
boolean network_game_server_send_game_data_pregame(struct network_game_server *s){(void)s;settings_sent++;return 1;}
static short third_person_host_policy(void){return 1;}
static void third_person_reset(void){}
static int network_game_server_client_machine_is_joined_to_game(struct network_game_server *s,
 struct network_game_server_client_machine *m){(void)s;return m->joined;}
static void network_event(char const *fmt,...){(void)fmt;}
''' + version + '\n' + (ROOT / 'source/bungie_net/common/message_header.h').read_text().replace('#pragma once', '') + '\n' + enum_with(
        (ROOT / 'source/networking/network_messages.h').read_text(), '_message_client_broadcast_game_search') + r'''
void build_message_header(word *out,word bytes,byte type,byte flags){*out=(word)((bytes<<4)|(type<<2)|flags);}
''' + implementation('third_person_protocol') + '\n' + implementation('mutator_protocol') + '\n' + menu_save + '\n' + lobby_open + '\n' + gate + r'''
int main(void){
 struct network_game_server server={0};struct network_game_server_client_machine machine={0};
 word join[256],camera[260],outer[264],inner[264],bad[264];unsigned short size;int combined,bytes,i;
 host=&server;
 CHECK(mutator_session_compatible(NULL,HALO_PORT_NETWORK_VERSION,1));
 CHECK(mutator_session_compatible(NULL,32746,1));
 CHECK(mutator_session_compatible(NULL,32747,1));CHECK(mutator_session_compatible(NULL,32748,1));
 CHECK(!mutator_session_compatible(NULL,32748,0));CHECK(!mutator_session_compatible(NULL,32749,1));
 for(combined=0;combined<2;combined++){
  mode=combined?"gulchce_third_person":"opence_crossplay";optin=1;
  third_person_session_host_begin();mutator_session_host_begin();
  CHECK(mutator_session_active());CHECK(mutator_session_version(NULL)==(unsigned)(32747+combined));
  optin=0;mutator_session_lock();CHECK(mutator_session_active());CHECK(!third_person_session_set(!combined));
  for(bytes=3;bytes<=512;bytes++){
   void const *payload=join;unsigned short payload_size=bytes;
   build_message_header(join,bytes,_message_type_packet,0);((byte *)join)[bytes-1]=_message_client_join_game_request;
   CHECK(!network_game_server_handle_client_message(&server,&machine,join,bytes));
   if(combined){payload=third_person_protocol_join(join,bytes,camera);payload_size+=8;CHECK(payload);}
   CHECK(mutator_protocol_join(payload,payload_size,outer)==outer);
   CHECK(mutator_protocol_unwrap(outer,payload_size+8,inner,&size));CHECK(size==payload_size);
   CHECK(!memcmp(payload,inner,size));
   CHECK(network_game_server_handle_client_message(&server,&machine,outer,payload_size+8));
   for(i=2;i<8;i++){
    memcpy(bad,outer,payload_size+8);((byte *)bad)[i]^=1;
    CHECK(!mutator_protocol_unwrap(bad,payload_size+8,inner,&size));
   }
   CHECK(!mutator_protocol_unwrap(outer,payload_size+7,inner,&size));
  }
  mutator_session_host_end();third_person_session_host_end();
  CHECK(!mutator_session_active());CHECK(!mutator_protocol_unwrap(outer,bytes+7+combined*8,inner,&size));
  optin=0;third_person_session_host_begin();mutator_session_host_begin();
  CHECK(!mutator_session_active());CHECK(!mutator_protocol_unwrap(outer,520,inner,&size));
  CHECK(!network_game_server_handle_client_message(&server,&machine,outer,520));
  mutator_session_host_end();third_person_session_host_end();
 }
 /* Saved menu selection must activate a bot host with the default config. */
 mode="opence_crossplay";optin=0;selected=1;server.game.machine_count=1;
 third_person_session_host_begin();mutator_session_host_begin();
 CHECK(mutator_session_active() && mutator_session_version(NULL)==32747);
 mutator_session_host_end();third_person_session_host_end();
 /* Host setup opens its socket before MUTATOR OPTIONS. Keeping a selection
    while alone must upgrade the agreement, and publish fresh settings. */
 selected=0;third_person_session_host_begin();mutator_session_host_begin();
 CHECK(!mutator_session_active());selected=1;
 CHECK(mutators_save());CHECK(mutator_session_active());
 CHECK(settings_sent==1 && mutator_session_version(NULL)==32747);
 mutator_session_host_end();third_person_session_host_end();
 /* A remote stock player or a locked/started lobby forbids upgrading. */
 selected=0;third_person_session_host_begin();mutator_session_host_begin();
 selected=1;server.game.machine_count=2;
 CHECK(!mutators_save() && !mutator_session_active());CHECK(notices==1);
 server.game.machine_count=1;CHECK(lobby_join_reset());
 CHECK(!mutators_save() && !mutator_session_active());CHECK(notices==2);
 CHECK(settings_sent==1);
 mutator_session_host_end();third_person_session_host_end();
 /* Selection after camera setup must use the combined agreement. */
 selected=0;mode="gulchce_third_person";
 third_person_session_host_begin();mutator_session_host_begin();selected=1;
 CHECK(mutator_session_enable_selected() && mutator_session_version(NULL)==32748);
 mutator_session_host_end();third_person_session_host_end();selected=0;
 host=NULL;mutator_session_client_begin(32748);third_person_session_client_begin(32748);
 CHECK(mutator_session_active() && third_person_session_active());CHECK(mutator_session_version(NULL)==32748);
 mutator_session_client_clear();third_person_session_client_clear();CHECK(!mutator_session_active());
 mutator_session_client_begin(32747);CHECK(mutator_session_active());
 mutator_session_client_begin(44);CHECK(!mutator_session_active());
 return 0;
}
'''
    source = tmp_path / 'test.c'
    source.write_text(code)
    exe = compile_tool(tmp_path, [source])
    subprocess.run([str(exe)], check=True, timeout=15)

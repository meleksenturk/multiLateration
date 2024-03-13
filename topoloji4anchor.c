#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/linkaddr.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <stdio.h> 
#include "sys/node-id.h"
#include "net/mac/tsch/tsch.h"

#include "sys/log.h"
#define LOG_MODULE "Node"
#define LOG_LEVEL LOG_LEVEL_INFO

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL       (20 * CLOCK_SECOND)
#define SEND_TIME       (random_rand() % (SEND_INTERVAL))



struct NodeLocation {
  double x;
  double y;
};

static struct NodeLocation nodeLocations[] = {
  {87.55498774609129, 31.85268348830057},
  
  {46.90207972146422, 56.502412085659195},

  {48.70017840714566, 17.492107952953607},
  
  {30.357490730029912,71.35342326887948},
  
  {23.06997620067279, 26.239573198701095},
  
  {79.70486401799066, 4.738978301891628},
  
  {81.12696873063314, 94.97095048319736},
  
  {17.41597491560336, 52.33386740738967},
  
  {64.9825636318644, 22.641926604138607}

};


static struct simple_udp_connection broadcast_connection;

PROCESS(broadcast_example_process, "Client");
AUTOSTART_PROCESSES(&broadcast_example_process);


PROCESS_THREAD(broadcast_example_process, ev, data)
{


  static struct etimer periodic_timer;
  char message[32];
  uip_ipaddr_t addr; //dest
  static uint32_t tx_count;
  // int is_coordinator;
  
  struct NodeLocation nodeLocation = nodeLocations[node_id-1];
  double node_x = nodeLocation.x;
  double node_y = nodeLocation.y;

  LOG_INFO("NodeIdmelek: %d", node_id);
  PROCESS_BEGIN();

  if(!tsch_is_coordinator && node_id == 1) { /* Eğer düğüm koordinatör değilse ve node_id'si 1 ise koordinatör yapalim*/
    tsch_set_coordinator(1);   
    // NETSTACK_ROUTING.root_start();  // Aği baslatir ve diğer düğümlerin katilmasina izin verir. Dugum aginin koku.
    // Ayrıca, kök düğümün zamanlama bilgilerini yayınlamasını ve hangi kanalların ne zaman kullanılacağını belirlemesini sağlar. 
    //Bu, ağın düzgün çalışmasını sağlar ve veri iletiminin güvenilir ve enerji verimli olmasını sağlar.
    LOG_INFO("Ben artik koordinatorum.\n");
  }

  if(tsch_is_coordinator) {  /* Running on the root? */
    NETSTACK_ROUTING.root_start();
    LOG_INFO(" Ben koordinatorum.\n");
  } else {
    LOG_INFO("Ben koordinator degilim.\n");
  }
 
  // is_coordinator = 0;
  
  // #if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_SKY
  //   is_coordinator = (node_id == 1);
  // #endif
  

 
  
  // if(is_coordinator) {  /* Running on the root? */
  //   NETSTACK_ROUTING.root_start();
  // }
  NETSTACK_MAC.on();
  
  /* Initialize UDP connection */
  simple_udp_register(&broadcast_connection, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);
  
  etimer_set(&periodic_timer, SEND_INTERVAL);
  
  while(1) { 
    LOG_INFO("X: %lf", node_x);
   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));       
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)) {
      printf("Sending broadcast %lu\n", (unsigned long)tx_count);
      snprintf(message,sizeof(message), "Node ID:%d X:%lf Y:%lf", node_id, node_x, node_y );
      uip_create_linklocal_allnodes_mcast(&addr);
      uip_ip6addr(&addr, 0xfe80, 0x0, 0x0, 0x0, 0x20a, 0xa, 0xa, 0xa);
      simple_udp_sendto(&broadcast_connection, message, strlen(message),&addr);
      tx_count++;
    } else {
      LOG_INFO("Not reachable yet\n");
    }
    etimer_set(&periodic_timer, SEND_INTERVAL);
  }
  PROCESS_END();
}







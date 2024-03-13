#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/linkaddr.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>
#include "net/packetbuf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <math.h>



#include "net/mac/tsch/tsch.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "UnknownNode"
#define LOG_LEVEL LOG_LEVEL_INFO

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
// #define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define A_REF -45.0 // Reference signal strength in dBm at 1m distance
#define N_FACTOR  2.0 // Signal propagation factor (varies depending on the environment)
// #define C 28


// Referans noktasının koordinatları, RSSI değeri, node_id'si ve bir sonraki elemanın işareti
struct Anchors {
    uint8_t node_id;
    double x;
    double y;
    double rssi;
    double distance;
    struct Anchors* next;
};

// Uç düğüm tahmini konumu
struct Coordinates {
    double x;
    double y;
};

struct Anchors anchorList;
// struct Anchors* sortAnchorsByNodeId(struct Anchors* anchorList);
struct Anchors* anchorListnew;


static struct simple_udp_connection broadcast_connection;
double point[2][1] = {{0.0}, {0.0}};

// double distanceArray[5]= [10.0, 20.0, 30.0, 40.0, 50.0];
void matrix_inverse( double matrix[][2], double inverse[][2]) {
    double determinant = matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    
    printf("Det: %3.2f\n",determinant);
    
    double inverseDeterminant = 1.0 / determinant;

    inverse[0][0] =  matrix[1][1] * inverseDeterminant;
    inverse[0][1] = -matrix[0][1] * inverseDeterminant;
    inverse[1][0] = -matrix[1][0] * inverseDeterminant;
    inverse[1][1] =  matrix[0][0] * inverseDeterminant;
    
    // printf("tersi \n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < 2; j++) {
    //             printf("%3.4f\t", inverse[i][j]);
    //         }
    //         printf("\n");
    //     }
    
    
    
}

// Matris çarpımı
static void matmul(int ROW, double a[][2],double transposeA[][ROW], double b[][1], double c[][1]) {
   //a matrisi (A^T*A) ^-1
   // b matrisi 2x1 lik matris
   // c matrisi koordinatların olusacagı matris
   // 
   //once A nın transpozu ile B matrisi carp sonra tersi alınan matrisle yani gelen a matrisiyle carp 
   
   double temp[1][2];
  for (int i = 0; i < 2 ; i++) {
        for (int j = 0; j < 1; j++) {
            temp[i][j] = 0;
            for (int k = 0; k < ROW; k++) {
                temp[i][j] += transposeA[i][k] * b[k][j];
            }
        }
    }
    // printf("B\n");
    //   for (int i = 0; i < ROW; i++) {
    //         for (int j = 0; j < 1; j++) {
    //             printf("%3.2f\t", b[i][j]);
    //         }
    //         printf("\n");
    //     }
   
    
    // printf("Temp\n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < 1; j++) {
    //             printf("%3.2f\t", temp[i][j]);
    //         }
    //         printf("\n");
    //     }
        
    //temp ile matrisin tersi olan a yı çarp c ye ata.
    
    for (int i = 0; i < 2; i++) {
        c[i][0] = 0;
        for (int j = 0; j < 2; j++) {
            c[i][0] += a[i][j] * temp[j][0];
        }
    }
    
    //  printf("C\n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < 1; j++) {
    //             printf("%3.2f\t", c[i][j]);
    //         }
    //         printf("\n");
    //     }
   
}

static void transpose_matmul( int ROW,double transpose[][ROW], double A[][2], double ATA[][2]) {
    int i, j, k;
    // printf("Matmuldaki A\n");
    //   for (int i = 0; i < ROW; i++) {
    //         for (int j = 0; j < 2; j++) {
    //             printf("%3.2f\t", A[i][j]);
    //         }
    //         printf("\n");
    //     }
    
    // printf("Row %d\n", ROW);
    // printf("Matmuldaki transposeA\n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < ROW; j++) {
    //             printf("%3.2f\t", transpose[i][j]);
    //         }
    //         printf("\n");
    //     }
    for (i = 0; i < 2 ; i++) {
        for (j = 0; j < 2; j++) {
            ATA[i][j] = 0;
            for (k = 0; k < ROW; k++) {
                ATA[i][j] += transpose[i][k] * A[k][j];
            }
        }
    }
    
    //  printf("Matmuldaki ATA\n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < 2; j++) {
    //             printf("%3.2f\t", ATA[i][j]);
    //         }
    //         printf("\n");
    //     }
}


static void matmulForThreeAnchor( double a[][2], double b[][1], double c[][1]){
    for (int i = 0; i < 2; i++) {
        c[i][0] = 0;
        for (int j = 0; j < 2; j++) {
            c[i][0] += a[i][j] * b[j][0];
        }
    }
    // printf("3 anchor icin\n");
    //   for (int i = 0; i < 2; i++) {
    //         for (int j = 0; j < 1; j++) {
    //             printf("%3.2f\t", c[i][j]);
    //         }
    //         printf("\n");
    //     }
    
}
// Multi-trilaterasyon fonksiyonu

//sonradan eklendi

// Bağlı liste elemanı oluşturma
// struct Anchors* createAnchor(uint8_t node_id, double x, double y, double rssi, double distance, struct Anchors* anchorList) {
//     //yeni node oluştur
//     struct Anchors* newNode = (struct Anchors*)malloc(sizeof(struct Anchors));
    
//     //yeterli bellek var mi bak
//     if (newNode == NULL)
//     {
//         LOG_INFO("Bellek tahsis hatasi");
//         exit(1);
//     }
    
//     newNode->node_id = node_id;
//     newNode->x = x;
//     newNode->y = y;
//     newNode->rssi = rssi;
//     newNode->distance = distance;
//     newNode->next = NULL;
    
//     // Anchor listesi boşsa, yeni düğümü başlangıç ​​noktası olarak ayarlayın
//     if (anchorList == NULL ) {
//     	anchorList = newNode;
//         return newNode;
//     }
    
//     // node_id'ye bakılarak kayıtlı veri var mı, varsa node güncelle
//     struct Anchors* current = anchorList;
//     while (current != NULL) {
//         if (current->node_id == node_id) {

//             current->x = x;
//             current->y = y;
//             current->rssi = rssi;
//             current->distance = distance;
//             free(newNode);
//             return anchorList; 
//         }
//         current = current->next;
//     }
    
//         // Anchor listesini dolaşarak yeni düğümü ekleyin
//         // Yeni elemanın ekleneceği konumu tutacak geçici bir düğüm olustur
//         struct Anchors *temp = anchorList;

//         // anchorList'in eleman sayısını sayalim
//         int count = 1;
//         while (temp->next != NULL)
//         {
//             temp = temp->next;
//             count++;
//         }

//         // Eleman sayısı 6'dan fazlaysa, ilk elemanı sil
//         if (count > 11)
//         {
//             struct Anchors *first = anchorList;
//             anchorList = anchorList->next;
//             free(first);
//         }

//         // Yeni dügümü anchorList'in sonuna ekle
//         temp = anchorList;
//         printf("Temp %p", temp);
//         printf("anchorList %p", temp);
//         while (temp->next != NULL)
//         {
//             temp = temp->next;
//         }
//         temp->next = newNode;
    
//     return anchorList; 
// }

struct Anchors* createAnchor(uint8_t node_id, double x, double y, double rssi, double distance, struct Anchors* anchorList) {
    // Yeni düğüm oluştur
    struct Anchors* newNode = (struct Anchors*)malloc(sizeof(struct Anchors));

    // Yeterli bellek kontrolü
    if (newNode == NULL) {
        LOG_INFO("Bellek tahsis hatasi");
        exit(1);
    }

    newNode->node_id = node_id;
    newNode->x = x;
    newNode->y = y;
    newNode->rssi = rssi;
    newNode->distance = distance;
    newNode->next = NULL;

    // Anchor listesi boşsa veya yeni düğümün node_id'si listenin başındaki düğümden küçükse, yeni düğümü başlangıç ​​noktası olarak ayarlayın
    if (anchorList == NULL || node_id < anchorList->node_id) {
        newNode->next = anchorList;
        return newNode;
    }

    // Eğer yeni düğümün node_id'si listenin başındaki düğümden büyükse, listenin başından başlayarak uygun konumu bulun
    struct Anchors* prev = NULL;
    struct Anchors* current = anchorList;

    while (current != NULL && node_id > current->node_id) {
        prev = current;
        current = current->next;
    }

    // Eğer bu node_id ile eşleşen bir düğüm bulursak, verileri güncelleyin ve yeni düğümü serbest bırakın
    if (current != NULL && node_id == current->node_id) {
        current->x = x;
        current->y = y;
        current->rssi = rssi;
        current->distance = distance;
        free(newNode);
    } else {
        // Eğer yeni bir node_id ekliyorsak, uygun konuma yeni düğümü ekleyin
        if (prev != NULL) {
            prev->next = newNode;
            newNode->next = current;
        }
    }

    // Eğer while döngüsünden çıkarsak, yeni düğüm en sona eklenmelidir
    if (current == NULL) {
        prev->next = newNode;
    }

    return anchorList;
}




//------------------------------------------------------------
struct Coordinates multilateration(struct Anchors* anchorList) {
    
    struct Coordinates coordinate;
    double point[2][1];

    int numAnchors = -1;
    struct Anchors* current = anchorList;
    struct Anchors* lastAnchor = NULL;


    // Tüm referans noktalarını dolaşarak sayın ve son referans noktasını kaydet
    while (current != NULL) {
         printf("CurrentX %f CurrentY: %f\n", current->x, current->y);
        numAnchors++;
        lastAnchor = current;
        current = current->next;
       
        
    }

    printf("numAnchors: %d\n", numAnchors);

    if (numAnchors < 3) {
        printf("En az üç referans noktası gereklidir.\n");
        coordinate.x = 0.0;
        coordinate.y = 0.0;
        return coordinate;
    }

    double ATA[2][2];
    double transposeA[2][numAnchors - 1];
    double A[numAnchors - 1][2];
    double B[numAnchors - 1][1];
    current = anchorList-> next ;
    //burası
    int index = 0;
    printf("current %p", current);
    printf("CurentID %d x: %f\n", current->node_id, current->x);
    printf("lastAnchorID:%d x:%f\n ",lastAnchor->node_id, lastAnchor->x);
    while (current != NULL) {
        
        if (current != lastAnchor) {
            // A[index][0] =  (lastAnchor->x - current->x);
            // A[index][1] = (lastAnchor->y - current->y);
            // B[index][0] = 0.5*(lastAnchor->x * lastAnchor->x - current->x * current->x + lastAnchor->y * lastAnchor->y - current->y * current->y + current->distance * current->distance - lastAnchor->distance * lastAnchor->distance);
             A[index][0] = (lastAnchor->x - current->x);
            A[index][1] = (lastAnchor->y - current->y);
            B[index][0] = 0.5*(lastAnchor->x * lastAnchor->x + lastAnchor->y * lastAnchor->y - current->x * current->x - current->y * current->y - lastAnchor->distance * lastAnchor->distance + current->distance * current->distance);
            
            index++;
        }
        current = current->next;
      
    }
    
    printf("A matrtisi\n");
        for (int i = 0; i < numAnchors-1; i++) {
            for (int j = 0; j < 2; j++) {
                printf("%3.1f\t", A[i][j]);
            }
            printf("\n");
        }

   

    if (numAnchors == 3) {
        printf("3 adet numAnchors: %d\n", numAnchors);
        double inversedA[2][2];

        matrix_inverse( A, inversedA);
        matmulForThreeAnchor( inversedA, B, point);

        coordinate.x = point[0][0];
        coordinate.y = point[1][0];
        

    } else if(numAnchors >= 4) {
       printf("Burdayim");
         for (int i = 0; i < numAnchors-1 ; i++) {
            for (int j = 0; j < 2 ; j++) {
                transposeA[j][i] = A[i][j];
            }
        }
        
        printf("A matrtisi transpozu\n");
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < numAnchors-1; j++) {
                printf("%3.1f\t", transposeA[i][j]);
            }
            printf("\n");
        }
        
        
        transpose_matmul(numAnchors - 1,transposeA, A, ATA);//doğru

        double inversedATA[2][2];

        matrix_inverse(ATA, inversedATA);
        matmul(numAnchors-1,inversedATA, transposeA, B, point);

        coordinate.x = point[0][0];
        coordinate.y = point[1][0];
        
    }
    printf("CoordinateX: %f\n", coordinate.x);
    printf("CoordinateY: %f\n", coordinate.y);
    return coordinate;
}

/*---------------------------------------------------------------------------*/
// struct Anchors* sortAnchorsByNodeId(struct Anchors* anchorList) {
//     // Anchor listesini geçici bir diziye kopyala
//     int count = 0;
//     struct Anchors* current = anchorList;
//     while (current != NULL) {
//         count++;
//         current = current->next;
//     }
    
//     struct Anchors* anchorArray = (struct Anchors*)malloc(count * sizeof(struct Anchors));
//     if (anchorArray == NULL) {
//         // Bellek tahsis hatası
//         return anchorList;
//     }
    
//     current = anchorList;
//     int index = 0;
//     while (current != NULL) {
//         anchorArray[index] = *current;
//         current = current->next;
//         index++;
//     }
    
//     // Node_id'ye göre kabarcık sıralaması yap
//     for (int i = 0; i < count - 1; i++) {
//         for (int j = 0; j < count - i - 1; j++) {
//             if (anchorArray[j].node_id > anchorArray[j + 1].node_id) {
//                 struct Anchors temp = anchorArray[j];
//                 anchorArray[j] = anchorArray[j + 1];
//                 anchorArray[j + 1] = temp;
//             }
//         }
//     }
    
//     // Sıralanmış düğümleri yeni bir anchor listesi olarak oluştur
//     // struct Anchors* sortedAnchorList = NULL;
//     for (int i = 0; i < count; i++) {
//         anchorListnew = createAnchor(anchorArray[i].node_id, anchorArray[i].x, anchorArray[i].y, anchorArray[i].rssi, anchorArray[i].distance, sortedAnchorList);
//     }
    
//     // Bellek temizle
//     free(anchorArray);
    
//     return anchorListnew;
// }



//yeni



// d=A*(r/t)^B+C
/*---------------------------------------------------------------------------*/
// Function to calculate the distance from RSSI value
static double calculate_distance(double rssi)
{
    double distance = powf(10, (-1 * ((rssi + 86.90876) / 93.6264) + log10(50)));
    return distance;
}

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_example_process, "Server");
AUTOSTART_PROCESSES(&broadcast_example_process);

static void receiver(struct simple_udp_connection *c,
                    const uip_ipaddr_t *sender_addr,
                    uint16_t sender_port,
                    const uip_ipaddr_t *receiver_addr,
                    uint16_t receiver_port,
                    const uint8_t *data,
                    uint16_t datalen)
{




    LOG_INFO("Paket alindi.");
    char buffer[100];  // Veriyi işlemek için bir tampon
    LOG_INFO("Gelen Veri: %s\n", buffer);

    if (datalen >= sizeof(buffer)) {
        LOG_INFO("Alınan veri çok büyük\n");
        return;
    }

    memcpy(buffer, data, datalen);
    buffer[datalen] = '\0'; 

    int node_id = 0;
    double node_x = 0;
    double node_y = 0;

    if (sscanf(buffer, "Node ID: %d X: %lf Y: %lf", &node_id, &node_x, &node_y) == 3) {
        LOG_INFO("Node ID: %d\n", node_id);

        // LOG_INFO("NodeX: %d\n", node_x);
        // LOG_INFO("NodeY: %d\n", node_y);

    } else {
        LOG_INFO("Veri ayrıştırma hatası\n");
        return;  
    }
    


    uint8_t hop_count = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl;
    // uint8_t hop_count = uip_if.cur_hop_limit - UIP_IP_BUF->ttl;

    // LOG_INFO("CurHop: %d",uip_ds6_if.cur_hop_limit);
    // LOG_INFO("TTL: %d", UIP_IP_BUF->ttl);
    LOG_INFO("Hop: %d NodeId: %d", hop_count, node_id);
    if (hop_count == 0) {
        int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
        
        double d_rssi = rssi;
        printf("RSSI: %f", d_rssi);
        double d_x = node_x;
        double d_y = node_y;
        double distance = calculate_distance(d_rssi);

        printf("Distance: %f\n", distance);
        createAnchor(node_id, d_x, d_y, d_rssi, distance, &anchorList);
        multilateration(&anchorList);
       
    }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_example_process, ev, data)
{

  PROCESS_BEGIN();
  
  simple_udp_register(&broadcast_connection, UDP_SERVER_PORT, NULL, UDP_CLIENT_PORT, receiver);

  while(1) {
    PROCESS_WAIT_EVENT();
  }
  
  PROCESS_END();
}

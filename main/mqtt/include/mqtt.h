
#ifndef __MQTT_H_
#define __MQTT_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>


/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*                              Typedefs                                    */
/****************************************************************************/
typedef struct 
{
    char *apn;
    char *apn_user;
    char *apn_pass;
} sim_t;

typedef struct 
{
    char *apn;
    char *apn_user;
    char *apn_pass;
} wifi_t;

typedef struct 
{
    char *host;
    uint16_t port;
    uint8_t connect;
} mqtt_server_t;

typedef struct 
{
    char *username;
    char *pass;
    char *clientID;
    unsigned short keepAliveInterval;
} mqtt_client_t;

typedef struct 
{
    uint8_t newEvent;
    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short msgId;
    unsigned char payload[64];
    int payloadLen;
    unsigned char topic[64];
    int topicLen;
} mqtt_receive_t;

typedef struct 
{
    sim_t sim;
    mqtt_server_t mqttServer;
    mqtt_client_t mqttClient;
    mqtt_receive_t mqttReceive;
} sim800_t;
/****************************************************************************/
/*							Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*                      Exproted Functions                                   */
/****************************************************************************/


#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
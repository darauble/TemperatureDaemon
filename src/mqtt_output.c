#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dallas.h"
#include "temp_types.h"

#include "MQTTAsync.h"


#define SERVER_PATTERN "tcp://%s:%d"

#define TEMP_BASE_TOPIC "%s/ds18x20/%02X%02X%02X%02X%02X%02X%02X%02X/"
#define TEMP_SCRATCHPAD_TOPIC TEMP_BASE_TOPIC "scratchpad"
#define TEMP_TEMPERATURE_TOPIC TEMP_BASE_TOPIC "temperature"
#define TEMP_INFO_TOPIC TEMP_BASE_TOPIC "info"
#define DEV_INFO_TOPIC "%s/device/%d"

#define TEMP_SCRATCHPAD_TPL "%02X%02X%02X%02X%02X%02X%02X%02X%02X"
#define TEMP_TEMPERATURE_TPL "%.5f"
#define TEMP_INFO_TPL "{\"num\":%d,\"device_num\":%d,\"status\":%d}"
#define DEV_INFO_TPL "{\"device\":\"%s\",\"status\":%d,\"thermo_count\":%d}"

#define TOPIC_SIZE 256
#define PAYLOAD_SIZE 64

static char topic[TOPIC_SIZE];
static char payload[PAYLOAD_SIZE];
static char lwt_topic[TOPIC_SIZE];

static MQTTAsync client;
static char *main_topic;

static void onConnect(void* context, MQTTAsync_successData5* response);
static void onConnectFailure(void* context, MQTTAsync_successData5* response);
static void onDisconnect(void* context, MQTTAsync_successData5* response);
static void onSend(void* context, MQTTAsync_successData5* response);
static void onSendFail(void* context, MQTTAsync_successData5* response);

static volatile int published = 0;

void mqtt_open(char *server, int port, char *topic_base)
{
    main_topic = topic_base;

    char *url = malloc(TOPIC_SIZE);
    
    if (url == NULL) {
        printf("Cannot allocate memory for MQTT URL, connection will not be established.\n");
        return;
    }

    snprintf(url, TOPIC_SIZE, SERVER_PATTERN, server, port);


    int rc = MQTTAsync_create(&client, url, "temp_daemon", MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTAsync_willOptions will_opts = MQTTAsync_willOptions_initializer;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

    snprintf(lwt_topic, TOPIC_SIZE, "%s/lwt", main_topic);

    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
    
    conn_opts.will = &will_opts;
    will_opts.topicName = lwt_topic;
    will_opts.message = "offline";
    // will_opts.retain = 1;

    rc = MQTTAsync_connect(client, &conn_opts);

    free(url);
}

void mqtt_send(wire_t *wires, int wire_count)
{
    int t = 0;

    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.qos = 1;
    msg.retained = 0;

    MQTTAsync_responseOptions response;
    response.onSuccess = onSend;
    response.onFailure = onSendFail;
    response.context = client;

    struct timespec read_wait = { .tv_sec = 0, .tv_nsec = 100000 };

    for (int i = 0; i < wire_count; i++) {
        /*** Send the device information ***/
        snprintf(topic, TOPIC_SIZE, DEV_INFO_TOPIC, main_topic, i);

        snprintf(payload, PAYLOAD_SIZE, DEV_INFO_TPL,
            wires[i].device, wires[i].status, wires[i].thermo_count
        );

        msg.payload = payload;
        msg.payloadlen = strlen(payload);
        
        MQTTAsync_sendMessage(client, topic, &msg, &response);

        while (!published) nanosleep(&read_wait, NULL);
        published = 0;

        for (int j = 0; j < wires[i].thermo_count; j++, t++) {
            
            uint8_t *addr = wires[i].thermometers[j].address;
            uint8_t *scr = wires[i].thermometers[j].scratchpad;

            /*** Send the scratchpad ***/
            snprintf(topic, TOPIC_SIZE, TEMP_SCRATCHPAD_TOPIC,
                main_topic,
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]
            );

            snprintf(payload, PAYLOAD_SIZE, TEMP_SCRATCHPAD_TPL,
                scr[SCR_L], scr[SCR_H], scr[SCR_HI_ALARM], scr[SCR_LO_ALARM], scr[SCR_CFG],
                scr[SCR_FFH], scr[SCR_RESERVED], scr[SCR_10H], scr[SCR_CRC]
            );

            msg.payload = payload;
            msg.payloadlen = strlen(payload);
            
            MQTTAsync_sendMessage(client, topic, &msg, &response);

            while (!published) nanosleep(&read_wait, NULL);
            published = 0;

            /*** Send the temperature ***/
            snprintf(topic, TOPIC_SIZE, TEMP_TEMPERATURE_TOPIC,
                main_topic,
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]
            );

            snprintf(payload, PAYLOAD_SIZE, TEMP_TEMPERATURE_TPL,
                wires[i].thermometers[j].temperature
            );

            msg.payload = payload;
            msg.payloadlen = strlen(payload);
            
            MQTTAsync_sendMessage(client, topic, &msg, &response);

            while (!published) nanosleep(&read_wait, NULL);
            published = 0;

            /*** Send the other information ***/
            snprintf(topic, TOPIC_SIZE, TEMP_INFO_TOPIC,
                main_topic,
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]
            );

            snprintf(payload, PAYLOAD_SIZE, TEMP_INFO_TPL,
                t, i, wires[i].thermometers[j].status
            );

            msg.payload = payload;
            msg.payloadlen = strlen(payload);
            
            MQTTAsync_sendMessage(client, topic, &msg, &response);

            while (!published) nanosleep(&read_wait, NULL);
            published = 0;
        }
    }
}

void mqtt_close()
{
    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess = onDisconnect;
    opts.context = client;
    int rc = MQTTAsync_disconnect(client, &opts);
}

static void onConnect(void* context, MQTTAsync_successData5* response)
{
    printf("Connected to MQTT server\n");

    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = "online";
    msg.payloadlen = strlen(msg.payload);
    msg.qos = 1;
    msg.retained = 0;

    MQTTAsync_sendMessage(client, lwt_topic, &msg, NULL);
}

static void onConnectFailure(void* context, MQTTAsync_successData5* response)
{
    printf("Failed to connect to MQTT server, messages will not be delivered.\n");
}

static void onDisconnect(void* context, MQTTAsync_successData5* response)
{
    MQTTAsync_destroy(&client);
}

static void onSend(void* context, MQTTAsync_successData5* response)
{
    // printf("Message sent.\n");
    published = 1;
}

static void onSendFail(void* context, MQTTAsync_successData5* response)
{
    printf("Message sending failed.\n");
    published = 1;
}

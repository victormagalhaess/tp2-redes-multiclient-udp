/*
Copyright © 2022 Victor Magalhães hello@victordias.dev
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define INPUT_ARGS 2
#define MAX_THREADS 15
#define MAX_PENDING 5

int serverSock;
int broadcastServerSock;
int numberOfThreads = 0;
int numberOfClients = 0;
int equipmentIdCounter = 1;
int equipmentsIds[MAX_CLIENTS] = {0};
struct sockaddr_in equipmentsAdresses[MAX_CLIENTS] = {0};
socklen_t clientLen = sizeof(struct sockaddr_in);
struct sockaddr_in broadcastAddr;

void *ThreadMain(void *arg);

struct ThreadArgs
{
    socklen_t clientLen;
    struct sockaddr_in clientCon;
    char buffer[BUFFER_SIZE_BYTES];
};

void sendBroadcastMessage(char *message)
{
    int bytesSent = sendto(broadcastServerSock, message, strlen(message), 0, (struct sockaddr *)&broadcastAddr, sizeof broadcastAddr);
    validateCommunication(bytesSent);
}

void buildERROR(char *buffer, int idDestination, int payload)
{
    struct Message message;
    message.IdMsg = ERROR_ID;
    message.IdOrigin = EQUIPMENT_NONE;
    message.IdDestination = idDestination;
    message.Payload = payload;
    assembleMessage(buffer, &message);
}

void buildRESADD(char *buffer)
{
    struct Message message;
    message.IdMsg = RES_ADD_ID;
    message.IdOrigin = EQUIPMENT_NONE;
    message.IdDestination = EQUIPMENT_NONE;
    message.Payload = equipmentIdCounter;
    assembleMessage(buffer, &message);
}

void buildOK(char *buffer, int idOrigin, int status)
{
    struct Message message;
    message.IdMsg = OK_ID;
    message.IdOrigin = idOrigin;
    message.IdDestination = EQUIPMENT_NONE;
    message.Payload = status;
    assembleMessage(buffer, &message);
}

void buildRESLIST(char *buffer)
{
    struct Message message;
    message.IdMsg = RES_LIST_ID;
    message.IdOrigin = EQUIPMENT_NONE;
    message.IdDestination = EQUIPMENT_NONE;
    message.Payload = 0;
    assembleMessage(buffer, &message);
    buffer[strlen(buffer) - 1] = '\0';
    for (int i = 0; i < numberOfClients; i++)
    {
        sprintf(buffer, "%s%d ", buffer, equipmentsIds[i]);
    }
    sprintf(buffer, "%s\n", buffer);
}

void buildREQREM(char *buffer, int idOrigin)
{
    struct Message message;
    message.IdMsg = REQ_REM_ID;
    message.IdOrigin = idOrigin;
    message.IdDestination = 0;
    message.Payload = 0;
    assembleMessage(buffer, &message);
}

void AddEquipment(char *response, struct sockaddr_in clientCon)
{
    equipmentsIds[numberOfClients] = equipmentIdCounter;
    equipmentsAdresses[numberOfClients] = clientCon;
    buildRESADD(response);
    equipmentIdCounter++;
    numberOfClients++;
    int bytesSent = sendUdpMessage(serverSock, response, &clientCon);
    validateCommunication(bytesSent);
    sendBroadcastMessage(response);
    buildRESLIST(response);
    bytesSent = sendUdpMessage(serverSock, response, &clientCon);
    validateCommunication(bytesSent);
    printf("Equipment %d added\n", equipmentIdCounter - 1);
}

int findEquipmentAddress(struct sockaddr_in *clientCon, int id)
{
    int found = 0;
    for (int i = 0; i < numberOfClients; i++)
    {
        if (equipmentsIds[i] == id)
        {
            *clientCon = equipmentsAdresses[i];
            found = 1;
            break;
        }
    }
    return found;
}

void RemoveEquipment(char *response, struct sockaddr_in originalCon)
{
    char *command = strtok(NULL, " ");
    char broadcastMessage[BUFFER_SIZE_BYTES];
    int originId = atoi(command);
    int deleted = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) // logical removal of equipment
    {
        if (equipmentsIds[i] == originId)
        {
            equipmentsIds[i] = equipmentsIds[numberOfClients - 1];
            equipmentsIds[numberOfClients - 1] = 0;
            equipmentsAdresses[i] = equipmentsAdresses[numberOfClients - 1];
            memset(&equipmentsAdresses[numberOfClients - 1], 0, sizeof(equipmentsAdresses[numberOfClients - 1]));
            deleted = 1;
            numberOfClients--;
        }
    }
    struct sockaddr_in clientCon;
    int found = findEquipmentAddress(&clientCon, originId);
    if (!found)
    {
        clientCon = originalCon;
    }

    if (deleted)
    {
        buildOK(response, originId, SUCCESFUL_REMOVAL);
        buildREQREM(broadcastMessage, originId);
        sendBroadcastMessage(broadcastMessage);
        printf("Equipment %d removed\n", originId);
    }
    else
    {
        buildERROR(response, EQUIPMENT_NONE, EQUIPMENT_NOT_FOUND);
    }

    int bytesSent = sendUdpMessage(serverSock, response, &clientCon);
    validateCommunication(bytesSent);
}

void GetEquipmentInfo(char *response, struct sockaddr_in clientCon, char *inputBuffer)
{
    int originId = atoi(strtok(NULL, " "));
    int destinationId = atoi(strtok(NULL, " "));
    struct sockaddr_in originCon;
    int foundOrigin = findEquipmentAddress(&originCon, originId);
    struct sockaddr_in destinationCon;
    int foundDestination = findEquipmentAddress(&destinationCon, destinationId);
    if (!foundOrigin)
    {
        buildERROR(response, EQUIPMENT_NONE, SOURCE_EQUIPMENT_NOT_FOUND);
        printf("Equipment %d not found\n", originId);
        int bytesSent = sendUdpMessage(serverSock, response, &clientCon);
        validateCommunication(bytesSent);
    }
    else if (!foundDestination)
    {
        buildERROR(response, EQUIPMENT_NONE, TARGET_EQUIPMENT_NOT_FOUND);
        printf("Equipment %d not found\n", destinationId);
        int bytesSent = sendUdpMessage(serverSock, response, &originCon);
        validateCommunication(bytesSent);
    }
    else
    {
        int bytesSent = sendUdpMessage(serverSock, inputBuffer, &destinationCon);
        validateCommunication(bytesSent);
    }
}

void *ThreadMain(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int idMessage = IdentifyMessage(strdup(threadArgs->buffer));
    char response[BUFFER_SIZE_BYTES] = "";
    switch (idMessage)
    {
    case REQ_ADD_ID:
        AddEquipment(response, threadArgs->clientCon);
        break;
    case REQ_REM_ID:
        RemoveEquipment(response, threadArgs->clientCon);
        break;
    case REQ_INF_ID:
    case RES_INF_ID:
        GetEquipmentInfo(response, threadArgs->clientCon, threadArgs->buffer);
        break;
    }

    free(threadArgs);
    numberOfThreads--;
    return NULL;
}

int main(int argc, char const *argv[])
{
    validateInputArgs(argc, 2);
    char *port = strdup(argv[1]);
    int portNumber = atoi(port);
    serverSock = buildUDPSocket(portNumber, UNICAST);
    broadcastServerSock = buildUDPSocket(0, BROADCAST);

    memset(&broadcastAddr, 0, sizeof broadcastAddr);
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons((in_port_t)(atoi(port) + 1));
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    pthread_t threads[MAX_THREADS];

    for (;;)
    {
        struct ThreadArgs *threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
        threadArgs->clientLen = clientLen;
        int bytesReceived = recvfrom(serverSock, threadArgs->buffer, BUFFER_SIZE_BYTES, 0, (struct sockaddr *)&threadArgs->clientCon, &threadArgs->clientLen);
        validateCommunication(bytesReceived);
        int firstReqId = IdentifyMessage(strdup(threadArgs->buffer));
        if (numberOfClients == MAX_CLIENTS && firstReqId == REQ_ADD_ID)
        {
            char response[BUFFER_SIZE_BYTES] = "";
            buildERROR(response, EQUIPMENT_NONE, EQUIPMENT_LIMIT_EXCEEDED);
            int bytesSent = sendUdpMessage(serverSock, response, &threadArgs->clientCon);
            validateCommunication(bytesSent);
            continue;
        }

        int threadStatus = pthread_create(&threads[numberOfThreads], NULL, ThreadMain, (void *)threadArgs);
        if (threadStatus != 0)
        {
            dieWithMessage("pthread_create failed");
        }
        else
        {
            numberOfThreads++;
        }
    }
    return 0;
}
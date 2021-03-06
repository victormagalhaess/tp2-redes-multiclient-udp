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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

void dieWithMessage(char *message)
{
    printf("%s\n", message);
    exit(-1);
}

void validateInputArgs(int argc, int minArgs)
{
    if (argc < minArgs)
    {
        dieWithMessage("Invalid number of arguments");
    }
}

void validateCommunication(int status)
{
    if (status < 1)
    {
        dieWithMessage("Error during communication");
    }
}

int getPort(char *portString)
{
    int port = atoi(portString);
    if ((port >= MIN_PORT_VALUE && port <= MAX_PORT_VALUE) || port == 0)
    {
        return port;
    }
    dieWithMessage("Port in invalid range. You must use a non-root valid Unix port betwheen 1025 and 65535");
    return -1; // never reached
}

int buildUDPSocket(int port, int isBroadcast)
{

    int sock;
    int opt = 1;

    struct sockaddr_in address;

    int domain = AF_INET;

    if ((sock = socket(domain, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        dieWithMessage("socket failed");
    }

    if (isBroadcast)
    {
        int broadcast = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &broadcast, sizeof(broadcast)) == -1)
        {
            dieWithMessage("setsockopt BROADCAST failed");
        }
    }
    else
    {
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
            dieWithMessage("setsockopt UNICAST failed");
        }
    }

    address.sin_family = domain;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    struct sockaddr *conAddress = (struct sockaddr *)&address;
    int conSize = sizeof(address);

    if (bind(sock, conAddress, conSize) < 0)
    {
        dieWithMessage("bind failed");
    }

    return sock;
}

int sendUdpMessage(int originSock, char *response, struct sockaddr_in *targetSock)
{
    return sendto(originSock, response, strlen(response), 0, (struct sockaddr *)targetSock, ADDR_LEN);
}

void assembleMessage(char *buffer, struct Message *message)
{
    char finalMessage[BUFFER_SIZE_BYTES] = "";
    char partialMessage[BUFFER_SIZE_BYTES] = "";
    if (message->IdMsg)
    {
        sprintf(partialMessage, "%d ", message->IdMsg);
        strcat(finalMessage, partialMessage);
    }
    if (message->IdOrigin)
    {
        sprintf(partialMessage, "%d ", message->IdOrigin);
        strcat(finalMessage, partialMessage);
    }
    if (message->IdDestination)
    {
        sprintf(partialMessage, "%d ", message->IdDestination);
        strcat(finalMessage, partialMessage);
    }
    if (message->Payload)
    {
        sprintf(partialMessage, "%d", message->Payload);
        strcat(finalMessage, partialMessage);
    }
    strcat(finalMessage, "\n");
    strcpy(buffer, finalMessage);
}

int IdentifyMessage(char *buffer)
{
    char *command = strtok(buffer, " ");
    int id = atoi(command);
    return id;
}
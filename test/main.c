#include "../libeom.h"

#include "../Networking/Nodes/PeerToPeer.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

static pthread_mutex_t hosts_mutex = PTHREAD_MUTEX_INITIALIZER;

void * server_function(void *arg)
{
    printf("Server running.\n");
    struct PeerToPeer *p2p = (struct PeerToPeer *)arg;
    struct sockaddr *address = (struct sockaddr *)&p2p->server.address;
    socklen_t address_length = (socklen_t)sizeof(p2p->server.address);

    while (1)
    {
        const int client = accept(p2p->server.socket, address, &address_length);
        if (client < 0) continue;

        // peer address
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        getpeername(client, (struct sockaddr *)&peer_addr, &peer_len);
        char client_address[INET_ADDRSTRLEN];
        strncpy(client_address, inet_ntoa(peer_addr.sin_addr), INET_ADDRSTRLEN);


        char request[255];
        memset(request, 0, 255);
        read(client, request, 255);
        printf("\t\t\t%s says: %s\n", client_address, request);
        close(client);

        /*
         * Es mensaje find?
         * * Recibir hash1 y hash2
         * * Buscarlo en local, sino preguntar a los known_hosts
         */

        pthread_mutex_lock(&hosts_mutex);
        short found = 0;
        for (int i = 0; i < p2p->known_hosts.length && !found; i++)
        {
            if (strcmp(client_address, p2p->known_hosts.retrieve(&p2p->known_hosts, i)) == 0)
            {
                found = 1;
            }
        }
        if (!found)
        {
            p2p->known_hosts.insert(&p2p->known_hosts, p2p->known_hosts.length, client_address, strlen(client_address)+1);
        }
        pthread_mutex_unlock(&hosts_mutex);
    }
    return NULL;
}

void * client_function(void *arg)
{
    struct PeerToPeer *p2p = arg;
    while (1)
    {
        struct Client client = client_constructor(p2p->domain, p2p->service, p2p->protocol, p2p->port, p2p->interface);
        char request[255];
        memset(request, 0, 255);
        fgets(request, 255, stdin);

        /*
         * Detectar si el request es de tipo find
         * * Si si -> generar el double_hash
         * * Enviar el mensaje: find hash1 hash2
         */

        pthread_mutex_lock(&hosts_mutex);
        for (int i = 0; i < p2p->known_hosts.length; i++)
        {
            char *host_ip = p2p->known_hosts.retrieve(&p2p->known_hosts, i);
            client.request(&client, host_ip, request, 255);
        }
        pthread_mutex_unlock(&hosts_mutex);
    }
}

int main(int argc, char *argv[]) {
    char *c = "hola todos";
    char *c2 = "hola todos me cago en sus muertos";
    char *ip1 = "127.0.0.1";
    hash_constructor();

    hash_insert(c, strlen(c), ip1);
    hash_insert(c2, strlen(c), ip1);

    HashEntry *entry = file_search_string(c);
    HashEntry *entry2 = file_search_hash(hash_generate(c));

    if (entry != NULL) {
        printf("%s\n%s\n", entry->ip, entry->path);
    }
    if (entry != NULL) {
        printf("%s\n%s\n", entry2->ip, entry2->path);
    }

    int port = argc > 1 ? atoi(argv[1]) : 1248;

    struct PeerToPeer p2p = peer_to_peer_constructor(
        AF_INET, SOCK_STREAM, 0, port, INADDR_ANY,
        server_function, client_function);

    // extraer ip y puerto, y insertarlo en known_hosts
    // i.e 127.0.0.1:1234
    if (argc > 2) {
        char ip[64]; int peer_port;
        sscanf(argv[2], "%63[^:]:%d", ip, &peer_port);
        p2p.known_hosts.insert(&p2p.known_hosts,  p2p.known_hosts.length, ip, strlen(ip) + 1);
    }

    p2p.user_portal(&p2p);
}
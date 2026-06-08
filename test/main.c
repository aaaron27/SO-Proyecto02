#include "../libeom.h"
#include "../Networking/Nodes/PeerToPeer.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

static pthread_mutex_t hosts_mutex = PTHREAD_MUTEX_INITIALIZER;

// Seteado desde argv[2] (ip:puerto del servidor central)
static char server_ip[INET_ADDRSTRLEN] = "";
static int  server_port = 0;

// Abrir una nueva conexión TCP al servidor central. 
// Devuelve un fd (file descriptor) conectado, o -1 en caso de error.
static int connect_to_server(void) {
    if (server_port == 0) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(server_port);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Scanea ./shared/, hashea cada archivo regular, 
// y envía un mensaje REGISTER al servidor central por cada uno.
static void scan_and_register(int my_port) {
    if (server_port == 0) return;

    DIR *dir = opendir("shared");
    if (!dir) {
        printf("shared/ folder not found, skipping registration.\n");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        char path[512];
        snprintf(path, sizeof(path), "shared/%s", ent->d_name);

        struct stat st;
        if (stat(path, &st) < 0 || !S_ISREG(st.st_mode)) continue;

        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        uint8_t *buf = malloc(size);
        if (!buf) { fclose(f); continue; }
        fread(buf, 1, size, f);
        fclose(f);

        unsigned long long hash = hash_generate(buf, size);
        // Guarda copia en hash_files/shared_data/<hash>, inserta en BST, persiste en hash_log
        hash_insert(buf, size, "127.0.0.1");
        free(buf);

        char msg[512];
        snprintf(msg, sizeof(msg), "REGISTER %s %ld %llu %d",
                 ent->d_name, size, hash, my_port);

        int fd = connect_to_server();
        if (fd >= 0) {
            send(fd, msg, strlen(msg), 0);
            close(fd);
            printf("Registered: %s  hash=%llu  size=%ld\n",
                   ent->d_name, hash, size);
        } 
        else {
            printf("Could not reach server to register %s\n", ent->d_name);
        }
    }
    closedir(dir);
}

void *server_function(void *arg)
{
    printf("Server running.\n");
    struct PeerToPeer *p2p = (struct PeerToPeer *)arg;
    struct sockaddr *address       = (struct sockaddr *)&p2p->server.address;
    socklen_t        address_length = (socklen_t)sizeof(p2p->server.address);

    while (1)
    {
        const int client = accept(p2p->server.socket, address, &address_length);
        if (client < 0) continue;

        // Identificar al par remoto
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        getpeername(client, (struct sockaddr *)&peer_addr, &peer_len);
        char client_address[INET_ADDRSTRLEN];
        strncpy(client_address, inet_ntoa(peer_addr.sin_addr), INET_ADDRSTRLEN);

        char request[1024];
        memset(request, 0, sizeof(request));
        int n = read(client, request, sizeof(request) - 1);
        if (n > 0) request[n] = '\0';

        printf("[%s]: %s\n", client_address, request);

        if (strncmp(request, "REGISTER ", 9) == 0) {
            // REGISTER <filename> <size> <hash> <port>
            char filename[256];
            size_t size;
            unsigned long long hash;
            int client_port;
            if (sscanf(request + 9, "%255s %zu %llu %d",
                       filename, &size, &hash, &client_port) == 4) {
                hash_register(hash, size, client_address, client_port, filename);
                printf("  -> stored %s from %s:%d\n",
                       filename, client_address, client_port);
            }

        } 
        else if (strncmp(request, "FIND ", 5) == 0) {
            // FIND <name>: responde con entradas del registro que coincidan con el nombre
            char name[256] = "";
            sscanf(request + 5, "%255s", name);
            hash_find(name, client);

        } 
        else if (strncmp(request, "LOCATE ", 7) == 0) {
            // LOCATE <hash>: responde con todos los peers que tienen ese archivo
            unsigned long long hash = 0;
            sscanf(request + 7, "%llu", &hash);
            hash_locate(hash, client);

        } else if (strncmp(request, "FILEGET ", 8) == 0) {
            // FILEGET <hash> <offset> <length>: sirve el rango de bytes pedido
            unsigned long long hash;
            size_t offset, length;
            if (sscanf(request + 8, "%llu %zu %zu", &hash, &offset, &length) == 3) {
                HashEntry *e = file_search_hash(hash);
                if (e) {
                    FILE *f = fopen(e->path, "rb");
                    if (f) {
                        fseek(f, (long)offset, SEEK_SET);
                        uint8_t *chunk = malloc(length);
                        if (chunk) {
                            size_t got = fread(chunk, 1, length, f);
                            send(client, chunk, got, 0);
                            free(chunk);
                        }
                        fclose(f);
                    }
                }
            }

        } 
        else {
            // Mensaje desconocido: auto-agregar el remitente a known_hosts
            pthread_mutex_lock(&hosts_mutex);
            short found = 0;
            for (int i = 0; i < p2p->known_hosts.length && !found; i++) {
                if (strcmp(client_address,
                           p2p->known_hosts.retrieve(&p2p->known_hosts, i)) == 0)
                    found = 1;
            }
            if (!found)
                p2p->known_hosts.insert(&p2p->known_hosts,
                                        p2p->known_hosts.length,
                                        client_address,
                                        strlen(client_address) + 1);
            pthread_mutex_unlock(&hosts_mutex);
        }

        close(client);
    }
    return NULL;
}

// Argumento para cada hilo de descarga de un chunk
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int  port;
    unsigned long long hash;
    size_t offset;
    size_t length;
    uint8_t *dst;   // posición correcta dentro del buffer ensamblado
    int success;
} ChunkArg;

static void *download_chunk(void *arg) {
    ChunkArg *ca = arg;
    ca->success = 0;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ca->port);
    inet_pton(AF_INET, ca->ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return NULL;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "FILEGET %llu %zu %zu",
             ca->hash, ca->offset, ca->length);
    send(fd, msg, strlen(msg), 0);
    shutdown(fd, SHUT_WR);

    size_t received = 0;
    while (received < ca->length) {
        int n = read(fd, ca->dst + received, ca->length - received);
        if (n <= 0) break;
        received += n;
    }
    close(fd);
    ca->success = (received == ca->length);
    return NULL;
}

void *client_function(void *arg)
{
    struct PeerToPeer *p2p = arg;

    // Registra todos los archivos locales con el servidor al iniciar
    scan_and_register(p2p->port);

    printf("Commands: find <name>  |  request <size> <hash>\n> ");
    fflush(stdout);

    char input[512];
    while (fgets(input, sizeof(input), stdin))
    {
        input[strcspn(input, "\n")] = '\0';
        if (input[0] == '\0') { printf("> "); fflush(stdout); continue; }

        if (strncmp(input, "find ", 5) == 0) {
            // Busqueda centralizada: 
            // envía un mensaje FIND al servidor central y muestra los resultados.
            char name[256];
            sscanf(input + 5, "%255s", name);

            char msg[512];
            snprintf(msg, sizeof(msg), "FIND %s", name);

            int fd = connect_to_server();
            if (fd < 0) {
                printf("Could not connect to server.\n");
            } else {
                send(fd, msg, strlen(msg), 0);
                // Indica que se ha terminado de escribir para que el read() del servidor retorne
                shutdown(fd, SHUT_WR);

                // Lee todas las líneas de respuesta hasta que se cierre la conexión
                char buf[30000];
                int total = 0, r;
                while ((r = read(fd, buf + total,
                                 sizeof(buf) - total - 1)) > 0)
                    total += r;
                buf[total] = '\0';
                close(fd);

                printf("Results for \"%s\":\n", name);
                int count = 0;
                char *line = strtok(buf, "\n");
                while (line) {
                    if (strcmp(line, "END") == 0) break;
                    unsigned long long hash;
                    size_t size;
                    char ip[64];
                    int port;
                    char filename[256];
                    if (sscanf(line, "%llu %zu %63s %d %255s",
                               &hash, &size, ip, &port, filename) == 5) {
                        printf("  %-30s  size=%-10zu  hash=%-20llu  peer=%s:%d\n",
                               filename, size, hash, ip, port);
                        count++;
                    }
                    line = strtok(NULL, "\n");
                }
                if (count == 0) printf("  (no results)\n");
            }

        } else if (strncmp(input, "request ", 8) == 0) {
            // request <size> <hash>: descarga distribuida entre todos los peers
            size_t size;
            unsigned long long hash;
            if (sscanf(input + 8, "%zu %llu", &size, &hash) != 2) {
                printf("Uso: request <size> <hash>\n");
                printf("> "); fflush(stdout); continue;
            }

            // 1. LOCATE: obtener lista de peers que tienen el archivo
            int fd = connect_to_server();
            if (fd < 0) { printf("No hay conexión con el servidor.\n"); printf("> "); fflush(stdout); continue; }

            char lmsg[64];
            snprintf(lmsg, sizeof(lmsg), "LOCATE %llu", hash);
            send(fd, lmsg, strlen(lmsg), 0);
            shutdown(fd, SHUT_WR);

            char buf[4096]; int total = 0, r;
            while ((r = read(fd, buf + total, sizeof(buf) - total - 1)) > 0) total += r;
            buf[total] = '\0';
            close(fd);

            // 2. Parsear peers: cada línea es "<ip> <port> <filename>"
            #define MAX_PEERS 32
            char peers_ip[MAX_PEERS][INET_ADDRSTRLEN];
            int  peers_port[MAX_PEERS];
            char filename[256] = "";
            int  n_peers = 0;

            char *ln = strtok(buf, "\n");
            while (ln && n_peers < MAX_PEERS) {
                if (strcmp(ln, "END") == 0) break;
                char ip[64]; int port; char fn[256];
                if (sscanf(ln, "%63s %d %255s", ip, &port, fn) == 3) {
                    strncpy(peers_ip[n_peers], ip, INET_ADDRSTRLEN - 1);
                    peers_port[n_peers] = port;
                    if (filename[0] == '\0') strncpy(filename, fn, 255);
                    n_peers++;
                }
                ln = strtok(NULL, "\n");
            }

            if (!n_peers) {
                printf("Ningún peer tiene hash=%llu.\n", hash);
                printf("> "); fflush(stdout); continue;
            }
            printf("Descargando \"%s\" (%zu bytes) de %d peer(s)...\n",
                   filename, size, n_peers);

            // 3. Buffer de ensamblado
            uint8_t *assembled = malloc(size);
            if (!assembled) { printf("Sin memoria.\n"); printf("> "); fflush(stdout); continue; }

            // 4. Dividir en chunks y lanzar un hilo por peer
            ChunkArg args[MAX_PEERS];
            pthread_t threads[MAX_PEERS];
            size_t chunk = size / n_peers;

            for (int i = 0; i < n_peers; i++) {
                args[i].offset  = (size_t)i * chunk;
                args[i].length  = (i == n_peers - 1) ? (size - args[i].offset) : chunk;
                args[i].dst     = assembled + args[i].offset;
                args[i].hash    = hash;
                args[i].port    = peers_port[i];
                strncpy(args[i].ip, peers_ip[i], INET_ADDRSTRLEN - 1);
                pthread_create(&threads[i], NULL, download_chunk, &args[i]);
            }

            // 5. Esperar todos los hilos
            int ok = 1;
            for (int i = 0; i < n_peers; i++) {
                pthread_join(threads[i], NULL);
                if (args[i].success) {
                    printf("  chunk %d/%d  %s:%d  bytes %zu-%zu  OK\n",
                           i + 1, n_peers, args[i].ip, args[i].port,
                           args[i].offset, args[i].offset + args[i].length - 1);
                } else {
                    printf("  chunk %d/%d  %s:%d  FALLO\n",
                           i + 1, n_peers, args[i].ip, args[i].port);
                    ok = 0;
                }
            }

            // 6. Guardar archivo ensamblado en shared/
            if (ok) {
                hash_insert(assembled, size, "127.0.0.1");
            } else {
                printf("Descarga incompleta, archivo no guardado.\n");
            }
            free(assembled);

        } else {
            // Transmitir el mensaje tal cual a todos los hosts conocidos
            pthread_mutex_lock(&hosts_mutex);
            for (int i = 0; i < p2p->known_hosts.length; i++) {
                char host[72];
                strncpy(host,
                        p2p->known_hosts.retrieve(&p2p->known_hosts, i),
                        sizeof(host) - 1);
                host[sizeof(host) - 1] = '\0';

                char host_ip[INET_ADDRSTRLEN];
                int  host_port = p2p->port;
                sscanf(host, "%15[^:]:%d", host_ip, &host_port);

                struct Client c = client_constructor(p2p->domain, p2p->service,
                                                     p2p->protocol,
                                                     host_port, p2p->interface);
                c.request(&c, host_ip, input, strlen(input) + 1);
            }
            pthread_mutex_unlock(&hosts_mutex);
        }

        printf("> ");
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Se asegura de que existan los directorios necesarios
    mkdir("hash_files", 0755);
    mkdir("hash_files/shared_data", 0755);
    mkdir("shared", 0755);

    // hash_log limpio → hash_constructor lee vacío → scan_and_register lo reconstruye
    FILE *hlog = fopen("hash_files/hash_log", "w");
    if (hlog) fclose(hlog);

    // rellenamos el BST
    hash_constructor();

    int port = argc > 1 ? atoi(argv[1]) : 1248;

    // Servidor: registro centralizado limpio al arrancar
    if (argc <= 2) {
        FILE *reg = fopen("hash_files/registry", "w");
        if (reg) fclose(reg);
    }

    struct PeerToPeer p2p = peer_to_peer_constructor(
        AF_INET, SOCK_STREAM, 0, port, INADDR_ANY,
        server_function, client_function);

    if (argc > 2) {
        // Parsea "ip:puerto" y lo guarda globalmente para connect_to_server()
        sscanf(argv[2], "%15[^:]:%d", server_ip, &server_port);
        // Agrega a known_hosts para que los broadcasts lleguen al nodo servidor
        p2p.known_hosts.insert(&p2p.known_hosts,
                                p2p.known_hosts.length,
                                argv[2], strlen(argv[2]) + 1);
    }

    p2p.user_portal(&p2p);
}

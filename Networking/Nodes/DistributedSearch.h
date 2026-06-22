#ifndef DISTRIBUTEDSEARCH_H
#define DISTRIBUTEDSEARCH_H

#include <time.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char query_id[128];
    char search_term[256];
    char origin_ip[64];
    int origin_port;
    int ttl;
} DistributedSearchQuery;

// Control de propagacion
#define MAX_SEEN_QUERIES 100
#define QUERY_EXPIRATION_SECONDS 60 

// guardar el ID y timestamp
typedef struct {
    char query_id[128];
    time_t timestamp; 
} SeenQuery;

static SeenQuery seen_queries[MAX_SEEN_QUERIES];
static int seen_queries_index = 0;

// Funcion para revisar IDs rep
static int is_query_seen(const char* query_id) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SEEN_QUERIES; i++) {
        // Verificar la entrada y tiempo de antiguedad
        if (seen_queries[i].timestamp > 0 && difftime(now, seen_queries[i].timestamp) < QUERY_EXPIRATION_SECONDS) {
            if (strcmp(seen_queries[i].query_id, query_id) == 0) return 1;
        }
    }
    return 0;
}

// Funcion para registrar un nuevo ID
static void mark_query_seen(const char* query_id) {
    int idx = seen_queries_index % MAX_SEEN_QUERIES;
    strncpy(seen_queries[idx].query_id, query_id, 128);
    seen_queries[idx].timestamp = time(NULL); // Guardar la hora actual
    seen_queries_index++;
}

#endif //DISTRIBUTEDSEARCH_H
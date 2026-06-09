
#ifndef DISTRIBUTEDSEARCH_H
#define DISTRIBUTEDSEARCH_H

#endif //DISTRIBUTEDSEARCH_H
#include <time.h>
#include <string.h>
#include <stdio.h>

typedef struct {
	char query_id[128];     // ID único: ej. "192.168.1.5:8080-16845321"
	char search_term[256];  // El nombre del archivo que buscamos (el *****)
	char origin_ip[64];     // IP del cliente original
	int origin_port;        // Puerto del cliente original
	int ttl;                // Time To Live (saltos restantes)
} DistributedSearchQuery;

// --- Control de propagación (Caché de Query IDs) ---
#define MAX_SEEN_QUERIES 100
char seen_queries[MAX_SEEN_QUERIES][128];
int seen_queries_index = 0;

// Función para revisar si ya vimos este ID
int is_query_seen(const char* query_id) {
	for (int i = 0; i < MAX_SEEN_QUERIES; i++) {
		if (strcmp(seen_queries[i], query_id) == 0) return 1;
	}
	return 0;
}

// Función para registrar un nuevo ID
void mark_query_seen(const char* query_id) {
	strncpy(seen_queries[seen_queries_index % MAX_SEEN_QUERIES], query_id, 128);
	seen_queries_index++;
}
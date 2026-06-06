//
// Created by aaron on 27/5/26.
//

#ifndef PROYECTO02_HASH_H
#define PROYECTO02_HASH_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned long long key;
    char *ip; // owner
    char *path;
    size_t size;
} HashEntry;

void hash_constructor();
unsigned long long hash_generate(uint8_t *file, size_t size);
int hash_insert(uint8_t *file, size_t size, char *ip);
HashEntry *file_search_hash(unsigned long long hash);

int compare_long_long_keys(void *entry_one, void *entry_two);

// Registra un archivo de un par remoto en el registro del servidor.
// Llamado por el servidor cuando recibe un mensaje REGISTER.
int hash_register(unsigned long long hash, size_t size, const char *ip, int port, const char *filename);

// Busca en el registro por archivos cuyo nombre contenga <name>.
// Llamado por el servidor cuando recibe un mensaje FIND.
// Formato de línea: "<hash> <size> <ip> <port> <filename>\n"
void hash_find(const char *name, int fd);

// Buscá en el registro por todos los peers que tengan <hash>.
// Llamado por el servidor cuando recibe un mensaje REQUEST.
// Formato de línea: "<ip> <port> <filename>\n"
void hash_locate(unsigned long long hash, int fd);

#endif //PROYECTO02_HASH_H

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

#endif //PROYECTO02_HASH_H

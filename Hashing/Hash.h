//
// Created by aaron on 27/5/26.
//

#ifndef PROYECTO02_HASH_H
#define PROYECTO02_HASH_H
#include <stddef.h>

typedef struct {
    long long key;
    char *ip; // owner
    char *path;
    size_t l; size_t r;
} HashEntry;

void hash_constructor();
long long hash_generate(char *string);
int hash_insert(char* file, size_t size, char *ip);
HashEntry *file_search_hash(long long hash);
HashEntry *file_search_string(char *file);

int compare_long_long_keys(void *entry_one, void *entry_two);

#endif //PROYECTO02_HASH_H

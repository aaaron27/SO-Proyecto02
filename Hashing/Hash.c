//
// Created by aaron on 27/5/26.
//
#include "Hash.h"
#include "../DataStructures/Dictionary/Dictionary.h"
#include "../DataStructures/Dictionary/Entry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int p = 12289;

struct Dictionary dictionary;

int compare_long_long_keys(void *entry_one, void *entry_two) {
    const long long a = *((long long *)(((struct Entry *)entry_one)->key));
    const long long b = *((long long *)(((struct Entry *)entry_two)->key));

    if (a > b) return 1;
    if (a < b) return -1;
    return 0;
}

int create_file(long long hash, char *file) {
    char *title = malloc(20 + sizeof("shared_data/"));
    snprintf(title, sizeof("shared_data/") + 20, "shared_data/%lld", hash);

    FILE *f = fopen(title, "w");
    free(title);

    if (f == NULL) {
        return 0;
    }

    fputs(file, f);
    fclose(f);
    return 1;
}

void hash_constructor() {
    dictionary = dictionary_constructor(compare_long_long_keys);
}

long long hash_generate(char *string) {
    const int m = 1000000009;
    long long hash_value = 0;
    long long p_pow = 1;
    for (int i = 0; i < strlen(string); i++) {
        hash_value = (hash_value + ((unsigned char)tolower(string[i] - 'a' + 1)) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
}

int hash_insert(char* file, const size_t size, char *ip) {
    HashEntry entry;
    entry.ip = ip;
    entry.l = 0;
    entry.r = size-1;
    entry.key = hash_generate(file);

    const int file_result = create_file(entry.key, file);

    if (!file_result) {
        printf("Couldnt find %lld file\n", entry.key);
        return 0;
    }

    printf("Created file %lld\n", entry.key);

    char *title = malloc(20 + sizeof("shared_data/"));
    snprintf(title, sizeof("shared_data/") + 20, "shared_data/%lld", entry.key);
    entry.path = title;

    long long key = entry.key;
    dictionary.insert(&dictionary, &key, sizeof(long long), &entry, sizeof(entry));

    return 1;
}

HashEntry *file_search_hash(long long hash) {
    if (dictionary.binary_search_tree.head == NULL) {
        printf("Dictionary is empty\n");
        return NULL;
    }

    HashEntry *entry = dictionary.search(&dictionary, &hash, sizeof(long long));
    return entry;
}

HashEntry *file_search_string(char* file) {
    const long long hash = hash_generate(file);
    return file_search_hash(hash);
}


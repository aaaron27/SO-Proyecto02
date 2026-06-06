//
// Created by aaron on 27/5/26.
//
#include "Hash.h"
#include "../DataStructures/Dictionary/Dictionary.h"
#include "../DataStructures/Dictionary/Entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int p = 12289;

struct Dictionary dictionary;

int compare_long_long_keys(void *entry_one, void *entry_two) {
    const long long a = *((long long *)(((struct Entry *)entry_one)->key));
    const long long b = *((long long *)(((struct Entry *)entry_two)->key));

    if (a > b) return 1;
    if (a < b) return -1;
    return 0;
}

int create_file(unsigned long long hash, uint8_t *file, size_t size) {
    char title[64];
    snprintf(title, sizeof(title), "hash_files/shared_data/%lld", hash);

    FILE *f = fopen(title, "wb");
    if (f == NULL) {
        return 0;
    }

    const size_t written = fwrite(file, 1, size, f);
    fclose(f);

    return written == size;
}

void refill_dictionary() {
    FILE *f = fopen("hash_files/hash_log", "r");
    if (f == NULL) return;

    char line[256];
    HashEntry entry;

    printf("Refilling BST\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "-") == 0) continue;
        entry.key = strtoull(line, NULL, 10);

        fgets(line, sizeof(line), f);
        line[strcspn(line, "\n")] = '\0';
        entry.ip = strdup(line);

        fgets(line, sizeof(line), f);
        line[strcspn(line, "\n")] = '\0';
        entry.path = strdup(line);

        fgets(line, sizeof(line), f);
        line[strcspn(line, "\n")] = '\0';
        entry.size = (size_t)strtoull(line, NULL, 10);

        printf("hash: %lld | ip_owner: %s | pathfile: %s | size: %lu\n",
               entry.key, entry.ip, entry.path, entry.size);

        // Reinsertar en el BST (los punteros ip/path pertenecen al nodo del árbol)
        unsigned long long key = entry.key;
        dictionary.insert(&dictionary, &key, sizeof(unsigned long long),
                          &entry, sizeof(entry));
    }

    fclose(f);
}

void insert_hash_in_log(HashEntry hashEntry) {
    FILE *f = fopen("hash_files/hash_log", "ab");

    if (f == NULL) {
        printf("Error opening hash_log\n");
        return;
    }

    fputs("-\n", f);
    fprintf(f, "%llu\n", hashEntry.key);
    fprintf(f, "%s\n", hashEntry.ip);
    fprintf(f, "%s\n", hashEntry.path);
    fprintf(f, "%lu\n", hashEntry.size);

    fclose(f);
}

void hash_constructor() {
    dictionary = dictionary_constructor(compare_long_long_keys);
    refill_dictionary();
}

unsigned long long hash_generate(uint8_t *file, size_t size) {
    const unsigned long long m = 1000000009;
    unsigned long long hash_value = 0;
    unsigned long long p_pow = 1;
    for (size_t i = 0; i < size; i++) {
        hash_value = (hash_value + (file[i] + 1ULL) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
}

int hash_insert(uint8_t *file, const size_t size, char *ip) {
    HashEntry entry;
    entry.ip = ip;
    entry.size = size;
    entry.key = hash_generate(file, size);

    const int file_result = create_file(entry.key, file, size);

    if (!file_result) {
        printf("Couldnt find %lld file\n", entry.key);
        return 0;
    }

    printf("Created file %lld\n", entry.key);

    char title[64];
    snprintf(title, sizeof(title), "hash_files/shared_data/%lld", entry.key);
    entry.path = strdup(title);

    unsigned long long key = entry.key;
    dictionary.insert(&dictionary, &key, sizeof(unsigned long long), &entry, sizeof(entry));

    insert_hash_in_log(entry);

    return 1;
}

HashEntry *file_search_hash(unsigned long long hash) {
    if (dictionary.binary_search_tree.head == NULL) {
        printf("Dictionary is empty\n");
        return NULL;
    }

    HashEntry *entry = dictionary.search(&dictionary, &hash, sizeof(unsigned long long));
    return entry;
}

int hash_register(unsigned long long hash, size_t size, const char *ip, int port, const char *filename) {
    FILE *f = fopen("hash_files/registry", "a");
    if (!f) return 0;
    fprintf(f, "%llu %zu %s %d %s\n", hash, size, ip, port, filename);
    fclose(f);
    return 1;
}

void hash_locate(unsigned long long hash, int fd) {
    FILE *f = fopen("hash_files/registry", "r");
    if (!f) {
        write(fd, "END\n", 4);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        unsigned long long h;
        size_t size;
        char ip[64];
        int port;
        char filename[256];
        if (sscanf(line, "%llu %zu %63s %d %255s", &h, &size, ip, &port, filename) != 5)
            continue;
        if (h == hash) {
            char result[512];
            int len = snprintf(result, sizeof(result), "%s %d %s\n", ip, port, filename);
            write(fd, result, len);
        }
    }
    fclose(f);
    write(fd, "END\n", 4);
}

void hash_find(const char *name, int fd) {
    FILE *f = fopen("hash_files/registry", "r");
    if (!f) {
        write(fd, "END\n", 4);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        unsigned long long hash;
        size_t size;
        char ip[64];
        int port;
        char filename[256];
        if (sscanf(line, "%llu %zu %63s %d %255s", &hash, &size, ip, &port, filename) != 5)
            continue;
        if (strstr(filename, name)) {
            char result[512];
            int len = snprintf(result, sizeof(result), "%llu %zu %s %d %s\n", hash, size, ip, port, filename);
            write(fd, result, len);
        }
    }
    fclose(f);
    write(fd, "END\n", 4);
}
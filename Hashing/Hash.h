//
// Created by aaron on 27/5/26.
//

#ifndef PROYECTO02_HASH_H
#define PROYECTO02_HASH_H

const int p1 = 257;
const int p2 = 53;

typedef struct {
    char *ip; // owner
    void *value; // info
    struct HashEntry *next;
} HashEntry;

typedef struct {
    HashEntry **buckets;
    int count;
} HashTable;

typedef struct {
    long long hash1;
    long long hash2;
} DoubleHash;

DoubleHash generate_double_hash(char *string);
long long generate_hash(char *string, int p);

#endif //PROYECTO02_HASH_H

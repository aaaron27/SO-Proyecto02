//
// Created by aaron on 27/5/26.
//
#include "Hash.h"

#include <ctype.h>
#include <string.h>

DoubleHash generate_double_hash(char *string) {
    const DoubleHash hashes = {generate_hash(string, p1), generate_hash(string, p2)};
    return hashes;
}

long long generate_hash(char *string, int p) {
    const int m = 1e9 + 9;
    long long hash_value = 0;
    long long p_pow = 1;
    for (int i = 0; i < strlen(string); i++) {
        hash_value = (hash_value + ((unsigned char)tolower(string[i] - 'a' + 1)) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
}

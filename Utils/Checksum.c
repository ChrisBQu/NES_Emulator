#include "Checksum.h"

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdio.h>

// See: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
uint32_t fnv1a_checksum(const char *buffer, size_t length) {
    uint32_t hash = 2166136261U; // FNV offset basis
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)buffer[i];
        hash *= 16777619U;
    }
   
    return hash;
}

#endif
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct Hashmap Hashmap;

typedef uint64_t (*Hashmap_hash_fn)(const void *key);
typedef int (*Hashmap_eq_fn)(const void *a, const void *b);

Hashmap* create_hashmap(size_t bucket_count, size_t key_size, Hashmap_hash_fn hash, Hashmap_eq_fn eq);
uint8_t hashmap_set(Hashmap *map, void *key, void *value);
void* hashmap_get(Hashmap *map, void *key);
uint8_t hashmap_remove(Hashmap *map, void *key);
uint8_t hashmap_destroy(Hashmap *map);

#endif
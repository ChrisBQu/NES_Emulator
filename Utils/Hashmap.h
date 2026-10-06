#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stddef.h>

// This is not cuurrently used for anything, but it's here for future use

typedef struct Hashmap Hashmap;

typedef uint64_t (*Hashmap_hash_fn)(const void *key);
typedef int (*Hashmap_eq_fn)(const void *a, const void *b);
typedef void (*Hashmap_free_fn)(void *value);

Hashmap* create_hashmap(size_t bucket_count, size_t key_size, Hashmap_hash_fn hash, Hashmap_eq_fn eq);
uint8_t hashmap_set(Hashmap *map, void *key, void *value);
void* hashmap_get(Hashmap *map, void *key);
uint8_t hashmap_remove(Hashmap *map, void *key);

// Frees the hashmap and its keys. Values are only pointed to by the hashmap, so pass free_value if they should be freed too (or NULL)
uint8_t hashmap_destroy(Hashmap *map, Hashmap_free_fn free_value);

#endif
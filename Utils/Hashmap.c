#include "Hashmap.h"
#include <stdlib.h>
#include <string.h>

struct HashmapNode {
    void *key;
    void *value;
    struct HashmapNode *next;
};

struct Hashmap {
    size_t bucket_count;
    size_t key_size;
    Hashmap_hash_fn hash;
    Hashmap_eq_fn eq;
    struct HashmapNode **buckets;
};

// Create a new hashmap with the given bucket count
Hashmap* create_hashmap(size_t bucket_count, size_t key_size, Hashmap_hash_fn hash, Hashmap_eq_fn eq) {
    if (bucket_count < 1 || key_size < 1 || hash == NULL || eq == NULL) { return 0; }

    // Allocate memory for the hashmap and its buckets
    Hashmap *newmap;
    newmap = malloc(sizeof(Hashmap));
    if (newmap == NULL) { return 0; }
    newmap->bucket_count = bucket_count;
    newmap->key_size = key_size;
    newmap->hash = hash;
    newmap->eq = eq;
    newmap->buckets = calloc(bucket_count, sizeof(struct HashmapNode *));
    if (newmap->buckets == NULL) {
        free(newmap);
        return 0;
    }

    return newmap;
}


// Set a key-value pair in the hashmap
uint8_t hashmap_set(Hashmap *map, void *key, void *value) {
    if (map == NULL || key == NULL || value == NULL) { return 1; } // Sanity check

    uint64_t hash = map->hash(key);
    size_t bucket_index = hash % map->bucket_count;

    // First, search for a node with thee same key
    for (struct HashmapNode *n = map->buckets[bucket_index]; n != NULL; n = n->next) {
        if (map->eq(n->key, key)) {
            n->value = value;
            return 0;
        }
    }

    // No such node was found, so add a new node to the top of the chain
    struct HashmapNode *new_node = malloc(sizeof(struct HashmapNode));
    if (new_node == NULL) { return 1; } // Allocation failed
    new_node->key = malloc(map->key_size);
    if (new_node->key == NULL) {
        free(new_node);
        return 1;
    }
    memcpy(new_node->key, key, map->key_size);
    new_node->value = value;
    new_node->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = new_node;
    return 0;
}

// Get a value from the hashmap
void* hashmap_get(Hashmap *map, void *key) {
    if (map == NULL || key == NULL) { return NULL; } // Sanity check
    uint64_t hash = map->hash(key);
    size_t bucket_index = hash % map->bucket_count;
    struct HashmapNode *current_node = map->buckets[bucket_index];
    while (current_node != NULL) {
        if (map->eq(current_node->key, key)) {
            return current_node->value;
        }
        current_node = current_node->next;
    }
    return NULL;
}

uint8_t hashmap_remove(Hashmap *map, void *key) {
    if (map == NULL || key == NULL) { return 1; } // Sanity check
    uint64_t hash = map->hash(key);
    size_t bucket_index = hash % map->bucket_count;
    struct HashmapNode *previous_node = NULL;
    struct HashmapNode *current_node = map->buckets[bucket_index];
    while (current_node != NULL) {
        if (map->eq(current_node->key, key)) {
            if (previous_node != NULL) { previous_node->next = current_node->next; }
            else { map->buckets[bucket_index] = current_node->next; }
            free(current_node->key);
            free(current_node);
            return 0;
        }
        previous_node = current_node;
        current_node = current_node->next;
    }
    return 1;
}

// Destroy the hashmap, freeeing the memory
uint8_t hashmap_destroy(Hashmap *map) {
    if (map == NULL) { return 1; } // Sanity check
    for (size_t i = 0; i < map->bucket_count; i++) {
        struct HashmapNode *current_node = map->buckets[i];
        while (current_node != NULL) {
            struct HashmapNode *next_node = current_node->next;
            free(current_node->key);
            free(current_node);
            current_node = next_node;
        }
    }
    free(map->buckets);
    free(map);
    return 0;
}
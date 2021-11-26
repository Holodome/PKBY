/*
Author: Holodome
Date: 21.11.2021
File: src/string_storage.h
Version: 0
*/
#ifndef STRING_STORAGE_H
#define STRING_STORAGE_H

#include "types.h"

#define STRING_STORAGE_BUFFER_SIZE (1llu << 14)

/**
 * @brief Tags used in the string_storage_string
 * 
 */
typedef enum {
    STRING_STORAGE_IDENTIFIER = 0x1,
    STRING_STORAGE_STRING     = 0x2,
    STRING_STORAGE_TAG_MAX
} string_storage_tag;


// Structure describing how strings are stored in the string storage
typedef struct {
    // Length of the string not including zero-terminator
    uint32_t size;
    // User-defined tag
    uint8_t  tag;
    // String storage
    char     data[];
} string_storage_string;

// Buffer for storing strings in it. 
// Uses constant size storage, which is being populated until it is filled up,
// then new buffer is used;
typedef struct string_storage_buffer {
    uint8_t  storage[STRING_STORAGE_BUFFER_SIZE];
    uint32_t used;
    // Linked list pointer
    struct string_storage_buffer *next;
} string_storage_buffer;

// Used internally for hash table of strings inside string_storage
typedef struct string_storage_hash_entry {
    str_hash hash;
    string_storage_string *string;
    struct string_storage_hash_entry *next;
} string_storage_hash_entry;

// Structure holding data connected to storing and accessing unique strings
// 
// This is a low-level implementation of _array of strings_ concept specifically for use in compiler
// Strings in compiler have property of being repeated many times throughout the program,
// and because of that it makes sence to add hash table to ensure uniqueness of each stored string, 
// to save some space
typedef struct string_storage {
    // Debug information about number of created buffers
    uint32_t buffer_count;
    // Linked list of buffers
    string_storage_buffer *first_buffer;
    // Size of hash table
    uint32_t hash_size;
    // Hash table
    string_storage_hash_entry *hash;
#if INTERNAL_BUILD
    // Total number of bytes used
    uint64_t total_strings_size;
    // Debug information about unique string count
    uint32_t string_count;
#endif
} string_storage;

string_storage *init_string_storage(uint32_t hash_size);
void destroy_string_storage(string_storage *ss);

string string_storage_add(string_storage *ss, char *str, uint32_t length, uint8_t tag);
string string_storage_add_str(string_storage *ss, string str, uint8_t tag);

#endif 
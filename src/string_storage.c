#include "string_storage.h"
#include "lib/strings.h"
#include "lib/lists.h"

static String_Storage_Buffer *
get_buffer_for_writing(String_Storage *ss, u32 size) {
    String_Storage_Buffer *buffer = ss->first_buffer;
    if (!buffer || buffer->used + size > STRING_STORAGE_BUFFER_SIZE) {
        buffer = ss->first_free_buffer;
        if (buffer) {
            STACK_POP(ss->first_free_buffer);
            mem_zero(buffer, sizeof(*buffer));
        } else {
            buffer = arena_alloc_struct(ss->arena, String_Storage_Buffer);
        }
        STACK_ADD(ss->first_buffer, buffer);
        ++ss->buffer_count;
    } 
    return buffer;
}

static String_Storage_Buffer *
get_buffer_by_idx(String_Storage *ss, u32 idx) {
    u32 cursor = ss->buffer_count;
    String_Storage_Buffer *bf = ss->first_buffer;
    assert(cursor >= idx);
    while (cursor > idx) {
        --cursor;
        bf = bf->next;
    }
    return bf;
}

static void
roll_back_to_location(String_Storage *ss, u64 loc) {
    u32 buf_idx = loc >> 32;
    u32 loc_in_buf = loc & 0xFFFFFFFF;
    while (ss->buffer_count > buf_idx) {
        String_Storage_Buffer *buf = ss->first_buffer;
        STACK_POP(ss->first_buffer);
        STACK_ADD(ss->first_free_buffer, buf);
        --ss->buffer_count;
    }
    ss->first_buffer->used = loc_in_buf;
}

String_Storage * 
create_string_storage(Memory_Arena *arena) {
    String_Storage *storage = arena_alloc_struct(arena, String_Storage);
    storage->arena = arena;
    storage->hash = create_hash64(STRING_STORAGE_HASH_SIZE, arena);
    return storage;
}

void 
string_storage_begin_write(String_Storage *storage) {
    assert(!storage->is_inside_write);
    storage->is_inside_write = true;
    String_Storage_Buffer *bf = get_buffer_for_writing(storage, sizeof(u32));
    storage->current_write_start = ((u64)storage->buffer_count << 32) | bf->used;
    storage->current_write_crc = 0;
    storage->current_write_len = 0;
    bf->used += sizeof(u32);
}

void
string_storage_write(String_Storage *storage, const void *src, u32 src_sz) {
    assert(storage->is_inside_write);
    u8 *src_cursror = (u8 *)src;
    storage->current_write_crc = crc32(storage->current_write_crc, src, src_sz);
    while (src_sz) {
        String_Storage_Buffer *bf = get_buffer_for_writing(storage, 1);
        u32 bf_size_remaing = STRING_STORAGE_BUFFER_SIZE - bf->used;
        u32 size_to_write = bf_size_remaing;
        if (src_sz < size_to_write) {
            size_to_write = src_sz;
        }
        mem_copy(bf->storage + bf->used, src_cursror, size_to_write);
        bf->used += size_to_write;
        src_sz -= size_to_write;
        src_cursror += size_to_write;
        storage->current_write_len += size_to_write;
    }
}

String_ID 
string_storage_end_write(String_Storage *ss) {
    assert(ss->is_inside_write);
    ss->is_inside_write = false;
    String_ID id = {0};
    String_Storage_Buffer *bf = get_buffer_for_writing(ss, 1);
    bf->storage[bf->used++] = 0;
    u64 hash_default = (u64)-1;
    u64 hash_value = hash64_get(&ss->hash, ss->current_write_crc, hash_default);
    if (hash_value != hash_default) {
        roll_back_to_location(ss, ss->current_write_start);
        id.value = hash_value;
    } else {
        hash64_set(&ss->hash, ss->current_write_crc, ss->current_write_start);
        u32 start_write_bf = ss->current_write_start >> 32;
        u32 start_write_bf_idx = ss->current_write_start & 0xFFFFFFFF;
        String_Storage_Buffer *start_bf = get_buffer_by_idx(ss, start_write_bf);
        *((u32 *)(start_bf->storage + start_write_bf_idx)) = ss->current_write_len;
        id.value = ss->current_write_start;
        ss->is_inside_write = false;
    
    }
    return id;
}

u32 
string_storage_get(String_Storage *ss, String_ID id, void *bf, uptr bf_sz) {
    u32 result = 0;
    if (id.value) {
        u32 start_write_bf = id.value >> 32;
        u32 start_write_bf_idx = id.value & 0xFFFFFFFF;
        u32 buffer_idx = start_write_bf;
        String_Storage_Buffer *sbf = get_buffer_by_idx(ss, buffer_idx);
        u32 string_length = *(u32 *)(sbf->storage + start_write_bf_idx) + 1;
        result = string_length;
        u8 *text_cursor = sbf->storage + start_write_bf_idx + sizeof(u32);
        u8 *bf_cursor = (u8 *)bf;
        while (bf_sz && string_length) {
            u32 current_bf_size_remaining = STRING_STORAGE_BUFFER_SIZE - (text_cursor - sbf->storage);
            u32 current_bf_read_size = current_bf_size_remaining;
            if (string_length < current_bf_read_size) {
                current_bf_read_size = string_length;
            } 
            if (bf_sz < current_bf_read_size) {
                current_bf_read_size = bf_sz;
            }
            mem_copy(bf_cursor, text_cursor, current_bf_read_size);
            string_length -= current_bf_read_size;
            bf_sz -= current_bf_read_size;
            sbf = get_buffer_by_idx(ss, buffer_idx++);
        }
    }
    return result;
}

String_ID string_storage_add(String_Storage *storage, const char *str) {
    u32 len = str_len(str);
    string_storage_begin_write(storage);
    string_storage_write(storage, str, len);
    return string_storage_end_write(storage);
}
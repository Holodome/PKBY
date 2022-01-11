/*
Author: Holodome
Date: 06.12.2021
File: src/unicode.h
Version: 0

Defines functions used when working with unicode 
*/
#ifndef UNICODE_H
#define UNICODE_H

#include <stdint.h>

// Decodes from utf8 unicode codepoint
// Retuns pointer to byte past decoded codepoint
void *utf8_decode(void *src, uint32_t *codepoint_p);

// Encodes unciode codepoint as utf8. 
// Writes encoded bytes to dst, returns pointer past last written byte 
void *utf8_encode(void *dst, uint32_t codepoint);

void *utf16_encode(void *dst, uint32_t codepoint);


#endif 

#pragma once
#include <stdint.h>


uint32_t substitute(uint32_t x);
uint32_t rotate(uint32_t x);
void key_expansion(uint8_t key[16], uint32_t w[44]);
uint8_t mul(uint8_t a, uint8_t b);
void copy_input(const uint8_t* inp, uint8_t b[4][4]);
void copy_key(const uint32_t* rk, uint8_t rkey[4][4]);
void add_key(uint8_t b[4][4], const uint8_t rkey[4][4]);
void substitute_block(uint8_t b[4][4], const uint8_t* box);
void shift_rows(uint8_t b[4][4]);
void shift_rev_rows(uint8_t b[4][4]);
void mix_columns(uint8_t b[4][4]);
void copy_output(uint8_t b[4][4], uint8_t* outp);
void aes_encrypt(uint8_t* data, size_t len, uint8_t* out, uint8_t key[16]);
void aes_decrypt(uint8_t* data, size_t len, uint8_t* out, uint8_t key[16]);
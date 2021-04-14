#include "Mix.h"
#include "AES.h"
#include <iostream>
#include <cassert>


void print_array(uint8_t* arr, int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (i != 0 && i % 16 == 0)
            printf("\t");
        printf("%2x ", arr[i]);
    }
    printf("\n");
}


void test_aes(void)
{
    uint8_t key[16] = {
       0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };

    uint8_t data[32] = {
        0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34,
        'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r',  'l',  'd',  '!',  0x00, 0x00, 0x00, 0x00
    };

    uint8_t out[32];
    uint8_t back[32];

    aes_encrypt(data, sizeof(data), out, key);
    aes_decrypt(out, sizeof(out), back, key);

    for (int i = 0; i < sizeof(data); ++i) {
        assert(data[i] == back[i]);
    }

    printf("Data:\n");
    print_array(data, sizeof(data));
    printf("Ciphertext:\n");
    print_array(out, sizeof(out));
    printf("Plaintext:\n");
    print_array(back, sizeof(back));
    printf("\nKey:\n");
    print_array(key, sizeof(key));
}
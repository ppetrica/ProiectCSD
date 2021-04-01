#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>


int send_file(SOCKET sock, const char* path) {
    unsigned long len = strlen(path);
    send(sock, (char*)len, sizeof(len), 0);
    send(sock, path, len, 0);

    HANDLE f = CreateFile(path,
        GENERIC_READ,
        0,
        NULL,
        0,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (!f) {
        printf("File not found\n");
        return 1;
    }

    DWORD file_size_high;
    DWORD file_size_low = GetFileSize(f, &file_size_high);

    size_t file_size = (file_size_high << 32) | file_size_low;
    send(sock, (char*)&file_size, sizeof(file_size), 0);

    size_t rest = file_size % 16;
    size_t total_sent = 0;
    while (total_sent < file_size - rest) {
        char buf[16];
        DWORD read;
        ReadFile(f, buf, sizeof(buf), &read, NULL);
        send(sock, buf, sizeof(buf), 0);
    }

    if (rest != 0) {
        char buf[16];
        DWORD read;
        ReadFile(f, buf, rest, &read, NULL);
        memset(buf + read, 0, 16 - read);
        send(sock, buf, sizeof(buf), 0);
    }
}


/* aes sbox and invert-sbox */
static const uint8_t sbox[256] = {
       0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
       0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
       0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
       0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
       0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
       0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
       0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
       0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
       0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
       0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
       0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
       0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
       0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
       0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
       0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
       0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};


static const uint8_t ibox[256] = {
       0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
       0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
       0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
       0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
       0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
       0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
       0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
       0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
       0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
       0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
       0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
       0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
       0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
       0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
       0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
       0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};


uint32_t substitute(uint32_t x) {
    return (sbox[(x & 0xff000000) >> 24] << 24) |
           (sbox[(x & 0x00ff0000) >> 16] << 16) |
           (sbox[(x & 0x0000ff00) >> 8] << 8) |
            sbox[(x & 0x000000ff)];
}


uint32_t rotate(uint32_t x) {
    return ((x & 0xff000000) >> 24) | (x << 8);
}


void key_expansion(uint8_t key[16], uint32_t w[44]) {
    uint8_t rcon[10] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};
    for (int i = 0; i < 4; ++i)
        w[i] = (key[4 * i] << 24) | (key[4 * i + 1] << 16) | (key[4 * i + 2] << 8) | key[4 * i + 3];

    for (int i = 4; i < 44; ++i) {
        uint32_t tmp = w[i - 1];
        if (i % 4 == 0)
            tmp = substitute(rotate(tmp)) ^ ((uint32_t)rcon[(i / 4) - 1] << 24);
        
        w[i] = w[i - 4] ^ tmp;
    }
}


uint8_t mul(uint8_t a, uint8_t b) {
    uint8_t res = 0;
    while (a) {
        if (a & 1) res ^= b;
        
        uint8_t carry = b & 0x80;

        b <<= 1;

        if (carry) b ^= 0x1B;

        a >>= 1;
    }

    return res;
}


void copy_input(const uint8_t *inp, uint8_t b[4][4]) {
    for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k) {
            b[j][k] = inp[k * 4 + j];
        }
    }
}


void copy_key(const uint32_t *rk, uint8_t rkey[4][4]) {
    for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k) {
            uint8_t shift = (3 - j) << 3;
            rkey[j][k] = (rk[k] & (0xff << shift)) >> shift;
        }
    }
}


void add_key(uint8_t b[4][4], const uint8_t rkey[4][4]) {
    for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k) {
            b[j][k] = b[j][k] ^ rkey[j][k];
        }
    }
}


void substitute_block(uint8_t b[4][4], const uint8_t* box) {
    for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k) {
            b[j][k] = box[b[j][k]];
        }
    }
}


void shift_rows(uint8_t b[4][4]) {
    for (int j = 1; j < 4; ++j) {
        uint8_t tmp[4];
        tmp[3] = b[j][(j + 3) & 3];
        tmp[2] = b[j][(j + 2) & 3];
        tmp[1] = b[j][(j + 1) & 3];
        tmp[0] = b[j][j];

        b[j][3] = tmp[3];
        b[j][2] = tmp[2];
        b[j][1] = tmp[1];
        b[j][0] = tmp[0];
    }
}

void shift_rev_rows(uint8_t b[4][4]) {
    for (int j = 1; j < 4; ++j) {
        uint8_t tmp[4];
        tmp[3] = b[j][(3 - j + 4) & 3];
        tmp[2] = b[j][(2 - j + 4) & 3];
        tmp[1] = b[j][(1 - j + 4) & 3];
        tmp[0] = b[j][(0 - j + 4) & 3];

        b[j][3] = tmp[3];
        b[j][2] = tmp[2];
        b[j][1] = tmp[1];
        b[j][0] = tmp[0];
    }
}

void mix_columns(uint8_t b[4][4]) {
    for (int k = 0; k < 4; ++k) {
        uint8_t tmp[4];

        tmp[0] = mul(2, b[0][k]) ^ mul(3, b[1][k]) ^ b[2][k] ^ b[3][k];
        tmp[1] = b[0][k] ^ mul(2, b[1][k]) ^ mul(3, b[2][k]) ^ b[3][k];
        tmp[2] = b[0][k] ^ b[1][k] ^ mul(2, b[2][k]) ^ mul(3, b[3][k]);
        tmp[3] = mul(3, b[0][k]) ^ b[1][k] ^ b[2][k] ^ mul(2, b[3][k]);

        b[0][k] = tmp[0];
        b[1][k] = tmp[1];
        b[2][k] = tmp[2];
        b[3][k] = tmp[3];
    }
}

void mix_rev_columns(uint8_t b[4][4]) {
    for (int k = 0; k < 4; ++k) {
        uint8_t tmp[4];

        tmp[0] = mul(0x0e, b[0][k]) ^ mul(0x0b, b[1][k]) ^ mul(0x0d, b[2][k]) ^ mul(0x09, b[3][k]);
        tmp[1] = mul(0x09, b[0][k]) ^ mul(0x0e, b[1][k]) ^ mul(0x0b, b[2][k]) ^ mul(0x0d, b[3][k]);
        tmp[2] = mul(0x0d, b[0][k]) ^ mul(0x09, b[1][k]) ^ mul(0x0e, b[2][k]) ^ mul(0x0b, b[3][k]);
        tmp[3] = mul(0x0b, b[0][k]) ^ mul(0x0d, b[1][k]) ^ mul(0x09, b[2][k]) ^ mul(0x0e, b[3][k]);

        b[0][k] = tmp[0];
        b[1][k] = tmp[1];
        b[2][k] = tmp[2];
        b[3][k] = tmp[3];
    }
}

void copy_output(uint8_t b[4][4], uint8_t* outp) {
    for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k) {
            outp[k * 4 + j] = b[j][k];
        }
    }
}


void aes_encrypt(uint8_t *data, size_t len, uint8_t *out, uint8_t key[16]) {
    uint32_t rk[44];
    key_expansion(key, rk);
    uint8_t b[4][4];
    uint8_t rkey[4][4];

    const size_t n_rounds = 10;
    size_t n_blocks = len >> 4;
    for (size_t i = 0; i < n_blocks; ++i) {
        uint8_t* inp  = data + (i << 4);
        uint8_t* outp = out  + (i << 4);

        copy_input(inp, b);
        copy_key(rk, rkey);
        
        add_key(b, rkey);

        for (int r = 1; r <= n_rounds - 1; ++r) {
            substitute_block(b, sbox);
            shift_rows(b);
            mix_columns(b);
            copy_key(rk + (r << 2), rkey);
            add_key(b, rkey);
        }

        substitute_block(b, sbox);
        shift_rows(b);
        
        int r = 10;
        copy_key(rk + (r << 2), rkey);
        add_key(b, rkey);
        copy_output(b, outp);
    }
}

void aes_decrypt(uint8_t* data, size_t len, uint8_t* out, uint8_t key[16]) {
    uint32_t rk[44];
    key_expansion(key, rk);
    uint8_t b[4][4];
    uint8_t rkey[4][4];

    const size_t n_rounds = 10;
    size_t n_blocks = len >> 4;
    for (size_t i = 0; i < n_blocks; ++i) {
        uint8_t* inp = data + (i << 4);
        uint8_t* outp = out + (i << 4);
        
        copy_input(inp, b);

        int r = 10;
        copy_key(rk + (r << 2), rkey);
        add_key(b, rkey);

        shift_rev_rows(b);
        substitute_block(b, ibox);

        for (int r = 9; r >= 1; --r) {
            copy_key(rk + (r << 2), rkey);
            add_key(b, rkey);
            mix_rev_columns(b);
            shift_rev_rows(b);
            substitute_block(b, ibox);
        }

        copy_key(rk, rkey);
        add_key(b, rkey);

        copy_output(b, outp);
    }
}

int main(int argc, char *argv[]) {
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

    exit(0);

    if (argc < 2) {
        printf("No IP provided.\nUsage: %s <ip>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];

    bool server = false;
    if (argc == 3) {
        if (strcmp(argv[2], "-s") == 0) {
            server = true;
        }
    }

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(16969);
    
    if (!inet_pton(AF_INET, ip, &sa.sin_addr.s_addr)) {
        printf("Invalid ip %s\n", ip);
        return 1;
    }

    // Initialize Winsock
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        printf("WSAStartup failed with error: %d\n", res);
        return 1;
    }

    SOCKET sock = INVALID_SOCKET;
    if (!server) {
        // Create a SOCKET for connecting to server
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        printf("Trying to connect to server\n");
        // Connect to server.
        res = connect(sock, (struct sockaddr*)&sa, sizeof(sa));
    }

    if (server || res == SOCKET_ERROR) {
        closesocket(sock);
        
        printf("Waiting for client on %s\n", ip);

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        sa.sin_addr.s_addr = 0;

        if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
            printf("Bind failed on %s: %d\n", ip, WSAGetLastError());
            exit(1);
        }

        if (listen(sock, 1) == -1) {
            printf("Listen failed on %s: %d\n", ip, WSAGetLastError());
            exit(1);
        }

        SOCKET conn = accept(sock, NULL, NULL);
        if (conn == -1) {
            printf("Failed to accept connection from %s: %d\n", ip, WSAGetLastError());
            exit(1);
        }

        closesocket(sock);
        sock = conn;
    } else {
        printf("Connected to server on %s\n", ip);
    }

    char line[256];

    bool exit_requested = false;
    while (!exit_requested) {
        printf("%s> ", ip);
        fgets(line, sizeof(line) - 1, stdin);
        size_t l = strlen(line);
        const char* path = line + 2;

        switch (line[0]) {
            case 's':
                if (send(sock, line + 2, l - 3, 0) == -1) {
                }
                break;
            case 'r':
                res = recv(sock, line, 128, 0); 
                if (res == -1) {
                    if (WSAGetLastError() == WSAECONNRESET) {
                        printf("Peer disconnected\n");
                        return 0;
                    }
                }

                printf("%.*s\n", res, line);
                break;
            case 'f':
                if (send_file(sock, path)) continue;
            case 'g':
                break;
                //if (read_file)
        }
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
#include <iostream>
#include <cassert>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Communication.h"
#include "AES.h"


int send_message(SOCKET sock, char* message, size_t len, uint8_t* key) {
    size_t n_blocks = (len >> 4) + 1;
    size_t total_size = n_blocks << 4;
    size_t padding = total_size - len;

    memset(message + len, (int)padding, padding);
    if (key)
        aes_encrypt((uint8_t*)message, total_size, (uint8_t*)message, key);

    assert(send(sock, message, (int)total_size, 0) == total_size);

    return 0;
}

int recv_message(SOCKET sock, char* message, size_t len, uint8_t* key) {
    int res = recv(sock, message, (int)len, 0);
    assert(res > 0 && res <= len && (res & 0xf) == 0);

    if (key)
        aes_decrypt((uint8_t*)message, res, (uint8_t*)message, key);

    uint8_t padding = message[res - 1];
    size_t total_size = (size_t)res - padding;

    return (int)total_size;
}


int send_file(SOCKET sock, const char* path, uint8_t* key) {
    HANDLE f = CreateFile(path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (f == INVALID_HANDLE_VALUE) {
        printf("File not found\n");
        return 1;
    }

    DWORD file_size_high;
    DWORD file_size_low = GetFileSize(f, &file_size_high);

    uint64_t file_size = ((uint64_t)file_size_high << 32) | file_size_low;
    uint64_t n_packets = (file_size >> 4) + 1;

    char buf[16] = { 0 };
    memcpy(buf + 8, (char*)&n_packets, 8);

    if (key)
        aes_encrypt((uint8_t*)buf, sizeof(buf), (uint8_t*)buf, key);

    assert(send(sock, buf, sizeof(buf), 0) == 16);

    size_t rest = file_size % 16;
    size_t total_sent = 0;
    while (total_sent < file_size - rest) {
        DWORD read;
        assert(ReadFile(f, buf, sizeof(buf), &read, NULL));

        if (key)
            aes_encrypt((uint8_t*)buf, sizeof(buf), (uint8_t*)buf, key);

        assert(send(sock, buf, sizeof(buf), 0) > 0);
        total_sent += read;
    }

    DWORD read = 0;
    if (rest != 0)
        assert(ReadFile(f, buf, (DWORD)rest, &read, NULL));

    memset(buf + read, 16 - read, 16 - read);

    if (key)
        aes_encrypt((uint8_t*)buf, sizeof(buf), (uint8_t*)buf, key);

    assert(send(sock, buf, sizeof(buf), 0) > 0);

    CloseHandle(f);

    return 0;
}

int recv_file(SOCKET sock, const char* path, uint8_t* key) {
    HANDLE f = CreateFile(path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (!f) {
        printf("File could not be opened\n");
        return 1;
    }

    char block[16];
    assert(recv(sock, block, sizeof(block), 0) == sizeof(block));
    if (key)
        aes_decrypt((uint8_t*)block, sizeof(block), (uint8_t*)block, key);

    size_t n_packets;
    memcpy(&n_packets, block + 8, 8);

    for (int i = 0; i < n_packets - 1; ++i) {
        assert(recv(sock, block, sizeof(block), 0) == 16);
        if (key)
            aes_decrypt((uint8_t*)block, sizeof(block), (uint8_t*)block, key);

        DWORD written;
        assert(WriteFile(f, block, sizeof(block), &written, nullptr));
    }

    assert(recv(sock, block, sizeof(block), 0) == 16);
    if (key)
        aes_decrypt((uint8_t*)block, sizeof(block), (uint8_t*)block, key);

    uint8_t padding = block[15];

    if (padding != 16) {
        DWORD written;
        assert(WriteFile(f, block, 16 - padding, &written, nullptr));
    }

    CloseHandle(f);

    return 0;
}


SOCKET create_socket(const char* ip, int port, bool server) {
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(16969);

    if (!inet_pton(AF_INET, ip, &sa.sin_addr.s_addr)) {
        printf("Invalid ip %s\n", ip);
        return 1;
    }

    // Initialize Winsock
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
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

        if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
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
    }
    else {
        printf("Connected to server on %s\n", ip);
    }

    return sock;
}

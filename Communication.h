#pragma once
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

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

SOCKET create_socket(const char* ip, int port, bool server)
{

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
}
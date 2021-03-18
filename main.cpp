#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("No IP provided.\nUsage: %s <ip>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];

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

    // Create a SOCKET for connecting to server
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    printf("Trying to connect to server\n");
    // Connect to server.
    res = connect(sock, (struct sockaddr *)&sa, sizeof(sa));
    if (res == SOCKET_ERROR) {
        printf("Waiting for client on %s", ip);
        closesocket(sock);

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        sa.sin_addr.s_addr = 0;

        if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
            printf("Bind failed on %s: %d", ip, WSAGetLastError());
            exit(1);
        }

        if (listen(sock, 1) == -1) {
            printf("Listen failed on %s: %d", ip, WSAGetLastError());
            exit(1);
        }

        SOCKET conn = accept(sock, NULL, NULL);
        if (conn == -1) {
            printf("Failed to accept connection from %s: %d", ip, WSAGetLastError());
            exit(1);
        }

        closesocket(sock);
        sock = conn;
    } else {
        printf("Connected to server on %s", ip);
    }

    char line[256];

    bool exit_requested = false;
    while (!exit_requested) {
        printf("%s> ", ip);
        fgets(line, sizeof(line) - 1, stdin);
        size_t l = strlen(line);

        switch (line[0]) {
            case 's':
                if (send(sock, line + 2, l - 3, 0) == -1) {
                }
                break;
            case 'r':
                res = recv(sock, line, 128, 0); 
                if (res == -1) {
                }

                printf("%.*s", res, line);
                break;
        }
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
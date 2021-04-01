#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "AES.h"
#include "Mix.h"
#include "Communication.h"


int main(int argc, char *argv[]) {
    test_aes();
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
    int port = 16969;

    // doar de SOCKET avem nevoie?
    SOCKET sock = create_socket(ip, port, server);


    char line[256];

    int res;
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
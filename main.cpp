#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <iostream>
#include "AES.h"
#include "Mix.h"
#include "Communication.h"


uint8_t key[16] = {
   0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};



int main(int argc, char *argv[]) {
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

    SOCKET sock = create_socket(ip, port, server);

    // max 256 de caractere mesajul
    char line[2 + 256 + 16];

    bool exit_requested = false;
    while (!exit_requested) {
        printf("%s> ", ip);
        fgets(line, 256 + 2, stdin);
        size_t l = strlen(line);
        char* message = line + 2;
        int res;
        switch (line[0]) {
            case 's':
                send_message(sock, message, l - 3, key);
                break;
            case 'r':
                res = recv_message(sock, line, 256 + 16, key);
                printf("%.*s\n", res, line);
                break;
            case 'f':
                line[l - 1] = '\0';
                if (send_file(sock, message, key)) continue;
                break;
            case 'g':
                line[l - 1] = '\0';
                if (recv_file(sock, message, key)) continue;
                break;
        }
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
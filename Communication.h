#pragma once
#include <windows.h>


int send_message(SOCKET sock, char* message, size_t len, uint8_t* key);
int recv_message(SOCKET sock, char* message, size_t len, uint8_t* key);

int send_file(SOCKET sock, const char* path, uint8_t* key);
int recv_file(SOCKET sock, const char* path, uint8_t* key);

SOCKET create_socket(const char* ip, int port, bool server);
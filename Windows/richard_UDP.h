// Copyright (c) 2013-2018 United States Government as represented by the Administrator of the
// National Aeronautics and Space Administration. All Rights Reserved.
#ifndef RICHARD_UPD_H_
#define RICHARD_UPD_H_

//#include <cstdlib>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // Winsock Library

#define UDP_VID6606_ID 60
#define UDP_LIGHT_ID   61
#define UDP_SWITCH_ID  62
#define UDP_A3967_ID  63
#define UDP_ARDUINO_RESET_ID  64
#define UDP_THREEPHASE_ID  65

#define DEBUG_MSG_SIZE 500

#define UDP_SOCKET_DISCONNECTED 0
#define UDP_SOCKET_CONNECTED 1

#define UDP_RECV_BUF_SIZE 2048
#define UDP_SEND_BUF_SIZE 2048

//Three highest bits define the board type - max 8:
#define BOARD_VID6606     0b00100000
#define BOARD_A3967       0b01000000
#define BOARD_LIGHTS      0b01100000
#define BOARD_SWITCHES    0b10000000
#define BOARD_HUEY_MASTER 0b10100000

struct UDPconnection {
	uint8_t SendBuf[UDP_SEND_BUF_SIZE];
	WSABUF SendDataBuf;
	WSAOVERLAPPED SendOverlapped;
	struct sockaddr_in TargetAddr;
	char target_ip[32];
	bool msg_pending;
};

#ifdef RHG_MAIN
int g_UDPSocketStatus = UDP_SOCKET_DISCONNECTED;
#else
extern int g_UDPSocketStatus;
#endif

/*
struct USBinfo {
	FILE* fpLog;		//Logfile Handle
	HANDLE USBhandle;
	char directory_path[300];
	int status;
	char port_list[MAX_COM_NUM][MAX_COM_LEN];
	int num_ports;
	int current_port;
	int num_beacon_tries;
};
*/

//Function Declarations:
int	 SetupSocket(void);
int  SetupConnection(struct UDPconnection* myUDP);
void ResetSocket(void);
int	 SendUDPMessage(struct UDPconnection* myUDP, uint8_t* UDPMessage, int UDPMessageLen);
void CloseUDP(void);




#endif
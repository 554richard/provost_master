#include "richard_UDP.h"
#include "richard_logging.h"

//#define PORT 49009
static SOCKET g_UDPSocket = INVALID_SOCKET;
int g_Port = 3333; //was 49009;

int SetupSocket(void)
{
	WSADATA wsaData;
	struct sockaddr_in localAddr;
	char debug_buffer[DEBUG_MSG_SIZE];

	// Initialize Winsock version 2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		sprintf_s(debug_buffer, sizeof(debug_buffer), "Server: WSAStartup failed with error %ld\n", WSAGetLastError());
		my_log(debug_buffer, g_log_dest);
		return -1;
	}
	else
	{
		sprintf_s(debug_buffer, sizeof(debug_buffer), "Server: The Winsock DLL status is %s.\n", wsaData.szSystemStatus);
		my_log(debug_buffer, g_log_dest);
	}

	//-----------------------------------------------
	// Create a socket to send and receive datagrams
	g_UDPSocket = WSASocket(AF_INET,
		SOCK_DGRAM,
		IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (g_UDPSocket == INVALID_SOCKET)
	{
		sprintf_s(debug_buffer, sizeof(debug_buffer), "Server: Error at socket(): %ld\n", WSAGetLastError());
		my_log(debug_buffer, g_log_dest);

		// Clean up
		WSACleanup();
		// Exit with error
		return -1;
	}
	else
	{
		sprintf_s(debug_buffer, sizeof(debug_buffer), "Server: socket() is OK!\n");
		my_log(debug_buffer, g_log_dest);
	}
	
	//-----------------------------------------------
	// Bind the socket to any address and the specified port.
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddr.sin_port = htons(g_Port);

	//We bind if this so it can receive as well as send:
	if (bind(g_UDPSocket, (struct sockaddr*)&(localAddr), sizeof(localAddr)) != 0)
	{
		int err = WSAGetLastError();
		sprintf_s(debug_buffer, sizeof(debug_buffer), "ERROR: Failed to bind socket. (Error code %i)", err);
		my_log(debug_buffer, g_log_dest);
		WSACleanup();
		return -1;
	}
	else
	{
		sprintf_s(debug_buffer, sizeof(debug_buffer), "Server: bind() is OK!\n");
		my_log(debug_buffer, g_log_dest);
	}

	g_UDPSocketStatus = UDP_SOCKET_CONNECTED;
	return 0;
}

int SetupConnection(struct UDPconnection* myUDP)
{
	char debug_buffer[DEBUG_MSG_SIZE];
/*
	// Make sure the Overlapped struct is zeroed out
	SecureZeroMemory((PVOID)&(myUDP->SendOverlapped), sizeof(WSAOVERLAPPED));

	// Create an event handle and setup the send overlapped structure.
	myUDP->SendOverlapped.hEvent = WSACreateEvent();
	if (myUDP->SendOverlapped.hEvent == WSA_INVALID_EVENT) {
		sprintf_s(debug_buffer, sizeof(debug_buffer), "WSACreateEvent for send failed with error: %d\n", WSAGetLastError());
		my_log(debug_buffer, g_log_dest);
		WSACleanup();
		return 1;
	}
*/
	//Set up target address for send:
	myUDP->TargetAddr.sin_family = AF_INET;
	myUDP->TargetAddr.sin_port = htons(g_Port);
	inet_pton(AF_INET, myUDP->target_ip, &(myUDP->TargetAddr.sin_addr));

	if (myUDP->TargetAddr.sin_addr.s_addr == INADDR_NONE) {
		sprintf_s(debug_buffer, sizeof(debug_buffer), "The target ip address entered must be a legal IPv4 address\n");
		my_log(debug_buffer, g_log_dest);
		WSACloseEvent(myUDP->SendOverlapped.hEvent);
		WSACleanup();
		return 1;
	}

	if (myUDP->TargetAddr.sin_port == 0) {
		sprintf_s(debug_buffer, sizeof(debug_buffer), "The target port must be a legal UDP port number\n");
		my_log(debug_buffer, g_log_dest);
		WSACloseEvent(myUDP->SendOverlapped.hEvent);
		WSACleanup();
		return 1;
	}

	myUDP->msg_pending = false;
	return 0;
}

void ResetSocket(void)
{
	char debug_buffer[DEBUG_MSG_SIZE];

	sprintf_s(debug_buffer, sizeof(debug_buffer), "Resetting Socket\n");
	my_log(debug_buffer, g_log_dest);
	closesocket(g_UDPSocket);
	SetupSocket();

	return;
}

void CloseUDP(void)
{
	char debug_buffer[DEBUG_MSG_SIZE];

	sprintf_s(debug_buffer, sizeof(debug_buffer), "Closing Socket\n");
	my_log(debug_buffer, g_log_dest);
	closesocket(g_UDPSocket);

	WSACleanup();
}

int	SendUDPMessage(struct UDPconnection* myUDP, uint8_t* UDPMessage, int UDPMessageLen)
{
	WSADATA wsaData;
	WSABUF RecvDataBuf;
//	WSABUF SendDataBuf;
//	WSAOVERLAPPED SendOverlapped;

//	struct sockaddr_in TargetAddr;

	DWORD BytesRecv = 0;
	DWORD Flags = 0;

	int err = 0;
	int rc;
	int retval = 0;

	if (myUDP->msg_pending)
	{
		rc = WSAWaitForMultipleEvents(1, &myUDP->SendOverlapped.hEvent, TRUE, INFINITE, TRUE);
		if (rc == WSA_WAIT_FAILED)
		{
			printf("WSAWaitForMultipleEvents failed with error: %d\n", WSAGetLastError());
		}
	}

	// Make sure the Overlapped struct is zeroed out
	SecureZeroMemory((PVOID)&myUDP->SendOverlapped, sizeof(WSAOVERLAPPED));

	// Create an event handle and setup the overlapped structure.
	myUDP->SendOverlapped.hEvent = WSACreateEvent();
	if (myUDP->SendOverlapped.hEvent == WSA_INVALID_EVENT) {
		printf("WSACreateEvent for send failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	myUDP->SendDataBuf.len = UDPMessageLen;
	myUDP->SendDataBuf.buf = (char *)myUDP->SendBuf;

	printf("Sending %d:", myUDP->SendDataBuf.len);
	for (int ii = 0; ii < myUDP->SendDataBuf.len; ii++)
	{
		myUDP->SendDataBuf.buf[ii] = UDPMessage[ii];
		printf(" %d", myUDP->SendDataBuf.buf[ii]);
	}
	printf("\n");
	printf("g_UDPSocket %lld SendDataBuf.len %d Flags %d\n", g_UDPSocket, myUDP->SendDataBuf.len, Flags);
	printf("sin_family: %d sin_port: %d sin_addr: %d\n", myUDP->TargetAddr.sin_family, myUDP->TargetAddr.sin_port, myUDP->TargetAddr.sin_addr.S_un);
	rc = WSASendTo(g_UDPSocket, &myUDP->SendDataBuf, 1,
		NULL, Flags, (SOCKADDR*)&myUDP->TargetAddr,
		sizeof(struct sockaddr_in), &myUDP->SendOverlapped, NULL);

	if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
		printf("WSASendTo failed with error: %d\n", err);
		WSACloseEvent(myUDP->SendOverlapped.hEvent);
		closesocket(g_UDPSocket);
		WSACleanup();
		return 1;
	}

	myUDP->msg_pending = true;

	return 0;
}

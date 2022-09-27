#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <cstdint>
#include <cstring>
//see this page if wanting to use a header which offers useful IP handling/information
// https://docs.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
#pragma comment(lib, "Ws2_32.lib")

#define DEF_PORT "40666"
#define BUFFER_SIZE 4096

//This is here if the data is large and fragmented
int receivingMessage(int initialSize, char* array, int socket)
{
	uint32_t totalSize;
	std::memcpy(&totalSize, array, 4);
	//std::cout << "total message size: " << totalSize << '\n';
	//char tempBuffer[bufferChunk];
	int received = 0;
	int delta = totalSize - initialSize;
	while (delta > 0)
	{
		received = recv(socket, (array + initialSize), BUFFER_SIZE, 0);
		if (received > 0)
		{
			initialSize += received;
			delta = totalSize - initialSize;
		}
		else if (received < 0)break;
	}
	if (received < 0) return 0;
	//std::cout << "Total Bytes Gathered: " << initialSize << '\n';
	return initialSize;
}

int main()
{
	//Initialize the dynamic library so that function calls can actually be made
	//	Doubles to verify that the functionality is present.
	WSADATA wsaData;
	int result;
	if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)//the function call loads the DLL, make word is version number
	{
		std::cerr << "Could not initialize/use Windows Sockets: " << result << " returned\n";
		return -1;
	}

	//Create a Socket
	struct addrinfo* resultInfo = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;//IPv4 address family
	hints.ai_socktype = SOCK_STREAM;//stream socket for TCP
	hints.ai_protocol = IPPROTO_TCP;//TCP protocol
	hints.ai_flags = AI_PASSIVE;//socket address struct will be used during binding
	result = getaddrinfo(NULL, DEF_PORT, &hints, &resultInfo);
	if (result != 0)
	{
		std::cerr << "Could not get local address and port for the server:"
			<< result << " returned\n";
		WSACleanup();
		return -2;
	}//else success on gather the local address and port to use with the server, so create Socket
	SOCKET listener = INVALID_SOCKET;
	listener = socket(resultInfo->ai_family, resultInfo->ai_socktype, resultInfo->ai_protocol);
	if (listener == INVALID_SOCKET)
	{
		std::cerr << "Could not create socket: Last error was " << WSAGetLastError() << "\n";
		freeaddrinfo(resultInfo);
		WSACleanup();
		return -3;
	}

	//Bind the Socket to a network address within the system
	result = bind(listener, resultInfo->ai_addr, (int)resultInfo->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		std::cerr << "Could not bind the socket: Last error was " << WSAGetLastError() << "\n";
		freeaddrinfo(resultInfo);
		closesocket(listener);
		WSACleanup();
		return -4;
	}
	freeaddrinfo(resultInfo);//address information no longer need - C-style free up

	//Listen for Client as the Socket is Bound to a Port
	if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "Could not listen for a client: Last error was " << WSAGetLastError() << "\n";
		closesocket(listener);
		WSACleanup();
		return -5;
	}//else we are waiting for a client

	fd_set original;
	FD_ZERO(&original);
	//Add the listening socket to the set
	FD_SET(listener, &original);

	char message[BUFFER_SIZE];
	int clientCount = 0;
	bool quit = false;
	while (!quit)
	{
		fd_set temp = original;

		int val = select(0, &temp, nullptr, nullptr, nullptr);

		for (unsigned int i = 0; i < val; ++i)
		{
			SOCKET socket = temp.fd_array[i];
			if (socket == listener)
			{
				//New Client to add
				SOCKET client = accept(listener, nullptr, nullptr);
				FD_SET(client, &original);
				++clientCount;
				std::cout << "new client added\n";
			}
			else
			{
				//Message to process
				int len = recv(socket, message, 4, 0);
				if (len > 0)
				{
					len = receivingMessage(len, message, socket);
					for (unsigned int j = 0; j < original.fd_count; ++j)
					{
						SOCKET relay = original.fd_array[j];
						if (relay != listener)
						{
							//std::cout << message << '\n';
							std::cout << "length: " << len << '\n';
							send(relay, message, len, 0);
						}
					}
				}
				else
				{
					//Client gone, remove this client
					closesocket(socket);
					FD_CLR(socket, &original);
					--clientCount;
					std::cout << "client dropped\n";
					if (clientCount <= 0)quit = true;
				}
			}
		}
	}
	
	closesocket(listener);
	FD_CLR(listener, &original);

	WSACleanup();
	std::cout << "All clients disconnected, enter any character to exit...\n";
	char wait = 'c';
	std::cin >> wait;
	return 0;
}

/*!
\file		Network.cpp
\author		Elton Leosantosa (leosantosa.e@digipen.edu)
\co-author
\par		Assignment 4
\date		01/04/2025
\brief
	Functions that deal with the networking stuff

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN 1000

#include "Windows.h"		// Entire Win32 API...
//#include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()
// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")


#include "Network.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

#include "Global.h"
#include "gameobject.h"

std::mutex _gameObjectMutex{};
std::mutex _stdoutMutex{};
std::mutex _eventMutex{};
std::queue<float> event_queue{};
float latest_server_update{};

namespace {
	sockaddr_in server_dest{};
	SOCKET sock{};

	std::thread recvThread{};
	std::thread sendThread{};

	std::atomic<bool> connected = false;
}

namespace {
	void RecvThread(SOCKET);
	void SendThread(SOCKET);
}

bool ConnectServer() {

	std::cout << "Connecting to server..." << std::endl;
	//connect to server
	std::ifstream ifs("Server.txt");
	if (!ifs.is_open()) {
		std::cerr << "Server config file not found!!!" << std::endl;
		return false;
	}
	std::string serverIP, serverPort, clientPort;
	std::getline(ifs, serverIP);
	std::getline(ifs, serverPort);
	std::getline(ifs, clientPort);
	ifs.close();

	//Initialise Winsock
	WSADATA wsaData{};
	int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
	if (errorCode != NO_ERROR)
	{
		std::cerr << "WSAStartup() failed: " << errorCode << std::endl;
		return false;
	}

	//Check is server is available
	sock = { socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) };
	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Error code: " << WSAGetLastError() << std::endl;
		std::cerr << "UDP socket() failed." << std::endl;
		return false;
	}
	sockaddr_in client_addr{};
	// Set up server address
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = INADDR_ANY;
	client_addr.sin_port = htons(std::stoi(clientPort));

	// Bind the socket
	if (bind(sock, reinterpret_cast<sockaddr*>(&client_addr), sizeof(client_addr)) != NO_ERROR)
	{
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		//WSACleanup();
		//throw std::runtime_error("Bind failed");
		return false;
	}
	u_long enable = 1;
	ioctlsocket(sock, FIONBIO, &enable);	//make socket non-blocking

	//setup client's connection with server
	memset(&server_dest, 0, sizeof(server_dest));
	server_dest.sin_family = AF_INET;		//ipv4
	server_dest.sin_port = htons((u_short)std::stoi(serverPort));
	inet_pton(AF_INET, serverIP.c_str(), &server_dest.sin_addr);

	//Send to server connection req - this will also bind the socket
	
	std::string connection_req = CreateReqConnect();
	int bytes = { sendto(sock, connection_req.c_str(), (int)connection_req.size(), 0, (sockaddr*)&server_dest, sizeof(server_dest)) };
	if (bytes == SOCKET_ERROR || bytes == 0)
	{
		std::cerr << "UDP send fail: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		return false;
	}
	//Sleep to wait for recv
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(200ms);
	}
	//Connection ack
	char buff[MAX_STR_LEN]{};
	sockaddr src{};
	socklen_t len{ sizeof(src) };
	bytes = { recvfrom(sock, buff, MAX_STR_LEN, 0, &src, &len) };
	if (bytes == SOCKET_ERROR || bytes == 0 || buff[0] != CommandID::C_RSP_CONNECT) {	//No ack received
		std::cerr << "UDP recv fail: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		return false;
	}
	//process which player you are... else disconnect
	int playerNum = ntohl(*(ULONG*)(buff + 1));
	if (playerNum < 0 || playerNum > 3) {
		std::cerr << "Wrong player number: " << std::endl;
		closesocket(sock);
		return false;
	}
	playerNO = playerNum;

	return true;
}

bool WaitGameStart() {
	char buff[MAX_STR_LEN];
	while (true) {
		sockaddr src{};
		socklen_t len{ sizeof(src) };
		const int bytesReceived = recvfrom(sock, buff, MAX_STR_LEN, 0, &src, &len);

		if (bytesReceived == SOCKET_ERROR)
		{
			size_t errorCode = WSAGetLastError();
			if (errorCode == WSAEWOULDBLOCK)
			{
				// A non-blocking call returned no data; sleep and try again.
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(200ms);

				std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
				//std::cerr << "trying again..." << std::endl;
				continue;
			}
			std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
			std::cerr << "recv() failed." << std::endl;
			return false;
		}

		if (bytesReceived > 0) {
			if (buff[0] == CommandID::C_GAME_START) {
				break;
			}
		}
	}

	//Setup threads here
	connected = true;
	gameRunning = true;
	recvThread = std::thread{ RecvThread, sock };
	sendThread = std::thread{ SendThread, sock };

	return true;
}

void DisconnectServer() {
	{
		std::lock_guard<std::mutex> outLock(_stdoutMutex);
		std::cout << "Disconnecting to server..." << std::endl;
	}
	connected = false;
	if (recvThread.joinable()) { 
		recvThread.join(); 
	}
	if (sendThread.joinable()) {
		sendThread.join();
	}
	WSACleanup();
}

namespace {
	//2 kinds of packets can be received, regular updates from server, and events from server - firing/asteroid
	void RecvThread(SOCKET sock) {
		{
			std::lock_guard<std::mutex> outMut(_stdoutMutex);
			std::cout << "Init Recv Thread.." << std::endl;
		}

		char buff[MAX_STR_LEN]{};
		while (connected) {
			sockaddr src{};
			socklen_t len{ sizeof(src) };
			const int bytesReceived = recvfrom(sock, buff, MAX_STR_LEN, 0, &src, &len);

			if (bytesReceived == SOCKET_ERROR)
			{
				size_t errorCode = WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK)
				{
					// A non-blocking call returned no data; sleep and try again.
					using namespace std::chrono_literals;
					std::this_thread::sleep_for(200ms);

					std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
					//std::cerr << "trying again..." << std::endl;
					continue;
				}
				std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
				std::cerr << "recv() failed." << std::endl;
				break;
			}
			//Ignore if the packet is not from the server
			sockaddr server_addr = *(sockaddr*)(&server_dest);
			bool fromServer = true;
			for (int i = 0; i < 6; ++i) {
				if (server_addr.sa_data[i] != src.sa_data[i]) {
					fromServer = false;
					break;
				}
			}
			if (!fromServer) {
				continue;
			}

			//Process the packet
			unsigned char cmd = (unsigned char)buff[0];
			switch (cmd) {
			case CommandID::C_ALL_UPDATE:
				ProcessAllState(buff);
				break;
			case CommandID::C_ASTEROID_SPAWN:
				ProcessAsteroidSpawn(buff);
				break;
			case CommandID::C_ASTEROID_DESTROY:
				ProcessAsteroidDestroy(buff);
				break;
			case CommandID::C_RSP_FIRE:
				ProcessRspFire(buff);
				break;
			case CommandID::C_TIME_SYNC:
				ProcessTimeSync(buff);
				break;
			case CommandID::C_GAME_END:
				ProcessGameEnd(buff);
				break;
			}
		}

		{
			std::lock_guard<std::mutex> outMut(_stdoutMutex);
			std::cout << "Close Recv Thread.." << std::endl;
		}
	}

	//2 kinds of packets can be sent, regular state update to server, and client 
	void SendThread(SOCKET sock) {
		{
			std::lock_guard<std::mutex> outMut(_stdoutMutex);
			std::cout << "Init Send Thread.." << std::endl;
		}

		double timer = 0.0;
		std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
		while (connected) {
			std::chrono::time_point<std::chrono::system_clock> newTime = std::chrono::system_clock::now();
			timer -= (double)std::chrono::duration_cast<std::chrono::milliseconds>(newTime - now).count();

			if (timer <= 0.0) {	//Simply broadcast state every 50ms
				std::string packet = CreateUpdate();
				int bytes{ sendto(sock, packet.c_str(), (int)packet.size(), 0, (sockaddr*)&server_dest, sizeof(server_dest)) };
				if (bytes == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						std::cerr << "UDP send fail" << std::endl;
						closesocket(sock);
						connected = false;
						break;
					}
				}

				timer = 50.0;
			}
		
			//Broadcast events - firing
			//Check queue, send if any events
			{
				std::lock_guard<std::mutex> queueMutx{ _eventMutex };
				while (!event_queue.empty()) {
					//send event...
					float t = event_queue.front();
					event_queue.pop();
					std::string packet = CreateReqFire(t);
					int bytes{ sendto(sock, packet.c_str(), (int)packet.size(), 0, (sockaddr*)&server_dest, sizeof(server_dest)) };
					if (bytes == SOCKET_ERROR)
					{
						if(WSAGetLastError() != WSAEWOULDBLOCK) {
							std::cerr << "UDP send fail" << std::endl;
							closesocket(sock);
							connected = false;
							break;
						}
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> outMut(_stdoutMutex);
			std::cout << "Close Send Thread.." << std::endl;
		}
		//recvfrom()
	}
}

//		id - 1b, timestamp - 4b, pos - 8b, scale - 8b, rot - 4b, vel - 8b
std::string CreateUpdate() {
	std::lock_guard<std::mutex> mut(_gameObjectMutex);
	std::string packet;
	packet.push_back(CommandID::C_STATE_UPDATE);

	UINT32 time = htonf(appTime);
	packet.append((char*)(&time), (char*)(&time) + 4);		//append length as bytes

	Player& player = players[playerNO];
	//Pos
	UINT32 tmp = htonf(player.go.t.pos.x);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes
	tmp = htonf(player.go.t.pos.y);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes

	//Scale
	tmp = htonf(player.go.t.scale.x);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes
	tmp = htonf(player.go.t.scale.y);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes

	//Rot
	tmp = htonf(player.go.t.rot);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes

	//Vel
	tmp = htonf(player.go.vel.x);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes
	tmp = htonf(player.go.vel.y);
	packet.append((char*)(&tmp), (char*)(&tmp) + 4);		//append length as bytes

	return packet;
}

std::string CreateReqFire(float fire_time) {
	std::string packet;
	packet.push_back(CommandID::C_REQ_FIRE);

	UINT32 time = htonf(fire_time);
	packet.append((char*)(&time), (char*)(&time) + 4);		//append length as bytes

	return packet;
}

std::string CreateReqConnect() {
	std::string packet;
	packet.push_back(CommandID::C_REQ_CONNECT);

	return packet;
}

//	id - 1b, timestamp - 4b, (pos - 8b, scale - 8b, rot - 4b, vel - 8b) * 4
void ProcessAllState(const char* buffer) {
	std::lock_guard<std::mutex> goMutx(_gameObjectMutex);

	FLOAT timestamp = ntohf(*(uint32_t*)(buffer + 1));

	//First check if timestamp is greater then latest update
	if (timestamp < latest_server_update) {
		return;
	}

	latest_server_update = timestamp;


	const int initial = 5;
	const int offset = 8 + 8 + 4 + 8;

	for (int i = 0; i < 4; ++i) {
		if (i == playerNO) {	//dont update self
			continue;
		}
		AEVec2 pos{}, scale{}, vel{}; float rot{};

		int itOffset = initial + offset * i;
		//Pos
		float tmp = ntohf(*(uint32_t*)(buffer + itOffset));
		pos.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 4));
		pos.y = tmp;
		//Scale
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 8));
		scale.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 12));
		scale.y = tmp;
		//rot
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 16));
		rot = tmp;
		//vel
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 20));
		vel.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 24));
		vel.y = tmp;

		players[i].go.vel = vel;
		players[i].go.t.pos = pos;
		players[i].go.t.rot = rot;
		InterpolateGameobject(players[i].go, timestamp);
	}
}

//	id - 1b, timestamp - 4b, playerid - 4b
void ProcessRspFire(const char* buffer) {
	std::lock_guard<std::mutex> mut(_gameObjectMutex);
	FLOAT timestamp = ntohf(*(uint32_t*)(buffer + 1));
	int playerID = (int)ntohl(*(uint32_t*)(buffer + 5));

	InterpolatedShoot(playerID, timestamp);
}

//	id - 1b, timestamp - 4b, (pos - 8b, scale - 8b, rot - 4b, vel - 8b) * 4
void ProcessTimeSync(const char* buffer) {
	std::lock_guard<std::mutex> goMutx(_gameObjectMutex);

	FLOAT timestamp = ntohf(*(uint32_t*)(buffer + 1));
	latest_server_update = timestamp;

	const int initial = 5;
	const int offset = 8 + 8 + 4 + 8;
	for (int i = 0; i < 4; ++i) {
		//if (i == playerNO) {	//dont update self
		//	continue;
		//}
		AEVec2 pos{}, scale{}, vel{}; float rot{};

		int itOffset = initial + offset * i;
		//Pos
		float tmp = ntohf(*(uint32_t*)(buffer + itOffset));
		pos.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 4));
		pos.y = tmp;
		//Scale
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 8));
		scale.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 12));
		scale.y = tmp;
		//rot
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 16));
		rot = tmp;
		//vel
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 20));
		vel.x = tmp;
		tmp = ntohf(*(uint32_t*)(buffer + itOffset + 24));
		vel.y = tmp;

		players[i].go.vel = vel;
		players[i].go.t.pos = pos;
		players[i].go.t.rot = rot;
	}
	//Interpolate all the asteroids
	InterpolateGOsync(timestamp);
	appTime = timestamp;
}

//C_GAME_END
//	id - 1b, (highscore - 4b, date - 8b) * 5, playerscore - 4b * 4
void ProcessGameEnd(const char* buffer) {
	std::lock_guard<std::mutex> goMutx(_gameObjectMutex);
	ULONG score{}; double date{};
	int initial = 1; int offset = 4 + 8;

	for (int i = 0; i < 5; ++i) {
		const int itOffset = initial + offset * i;
		highscores[i].first = ntohl(*(uint32_t*)(buffer + itOffset));
		highscores[i].second = ntohll(*(uint64_t*)(buffer + itOffset + 4));
	}
	initial = 61; offset = 4;
	for (int i = 0; i < 4; ++i) {
		const int itOffset = initial + offset * i;
		players[i].score = (int)ntohl(*(uint32_t*)(buffer + itOffset));
	}

	connected = false;
	gameRunning = false;
}

//C_ASTEROID_SPAWN
//	id - 1b, timestamp - 4b
void ProcessAsteroidSpawn(const char* buffer) {
	std::lock_guard<std::mutex> goMutx(_gameObjectMutex);
	FLOAT timestamp = ntohf(*(uint32_t*)(buffer + 1));

	SpawnInterpolatedAsteroid(timestamp);
}

//C_ASTEROID_DESTROY
//  id - 1b, index - 4b
void ProcessAsteroidDestroy(const char* buffer) {
	std::lock_guard<std::mutex> goMutx(_gameObjectMutex);

	int goID = (int)ntohl(*(uint32_t*)(buffer + 1));
	if (goID < golist.size()) {
		golist[goID].isActive = false;
	}
}
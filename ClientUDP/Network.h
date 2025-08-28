/*!
\file		Network.h
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

#ifndef NETWORK_H
#define	NETWORK_H

#include <chrono>
#include <mutex>
#include <thread>
#include <queue>

bool ConnectServer();
void DisconnectServer();	//close all connections
bool WaitGameStart();

//For between the recv and send and main thread when reading/writing to 
extern std::mutex _gameObjectMutex;
extern std::mutex _stdoutMutex;
extern std::mutex _eventMutex;
extern std::queue<float> event_queue;
extern float latest_server_update;

enum CommandID : unsigned char {
    C_ERROR = 0,
    C_STATE_UPDATE = 1,	//Regular client update
    C_ALL_UPDATE = 2,	//Regular server update
    C_REQ_FIRE = 3,		//Sends fire req to server
    C_RSP_FIRE = 4,		//Send fire rsp from server to client
    C_ASTEROID_SPAWN = 5,		//Send asteroid destroyed from server to client
    C_ASTEROID_DESTROY = 6,
    C_REQ_CONNECT = 7,	//Send to server to req connection
    C_RSP_CONNECT = 8,	//Send to client which player client is, or not connected
    C_GAME_START = 9,
    C_GAME_END = 10,
    C_TIME_SYNC = 11	//highest authority, syncing time and position and vel values for all
};

//Structure for a packet to be sent to the server
//	C_ERROR
//	-ignored
//C_STATE_UPDATE
//	id - 1b, timestamp - 4b, pos - 8b, scale - 8b, rot - 4b, vel - 8b
//C_ALL_UPDATE
//	id - 1b, timestamp - 4b, (pos - 8b, scale - 8b, rot - 4b, vel - 8b) * 4
//C_REQ_FIRE
//	id - 1b, timestamp - 4b
//C_RSP_FIRE
//	id - 1b, timestamp - 4b, playerid - 4b
//C_ASTEROID_SPAWN
//	id - 1b, timestamp - 4b
//C_ASTEROID_DESTROY
//  id - 1b, index - 4b
//C_REQ_CONNECT
//	id - 1b
//C_RSP_CONNECT
//	id - 1b, playerid - 4b
//C_GAME_START
//	id - 1b
//C_GAME_END
//	id - 1b, (highscore - 4b, date - 8b) * 5, playerscore - 4b * 4
//C_TIME_SYNC
//	id - 1b, timestamp - 4b, (pos - 8b, scale - 8b, rot - 4b, vel - 8b) * 4


std::string CreateUpdate();
std::string CreateReqFire(float);

std::string CreateReqConnect();

void ProcessAllState(const char* buffer);
void ProcessRspFire(const char* buffer);
void ProcessTimeSync(const char* buffer);
void ProcessGameEnd(const char* buffer);

void ProcessAsteroidSpawn(const char* buffer);
void ProcessAsteroidDestroy(const char* buffer);

#endif
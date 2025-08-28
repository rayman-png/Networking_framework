/*!
\file		main_server.cpp
\author		darius (d.chan@digipen.edu)
\co-author  
\par		Assignment 4
\date		01/04/2025
\brief
this is the server file for the multiplayer space shooter game

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
#include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <map>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <array>
#include <cmath>
#include <chrono>
#include <ctime>
#include <random>

#include "taskqueue.h"
#include "highscore.h"
#include "collision.h"
#include "gameobject.h"
#include "AlphaEngine/include/AEEngine.h"

#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000

#define MAX_PLAYERS         4
#define TOTAL_PLAYERS       1
#define UPDATE_RATE         50
#define TIME_SYNC           5
#define TOTAL_TIME          60





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

// Global variable
std::unordered_map<std::string, sockaddr_in> clients;  // Map of "IP:Port" -> SOCKET
std::mutex Mutex;
std::unordered_map<std::string, int> playersIndex;  // Map of player "IP:Port" -> index
std::array<Player, MAX_PLAYERS> playersInfo;                  // Array of player information, index based on above index, pair of player info and score

std::vector<Bullet> bulletlist{};			//every bullet in the game
std::vector<GameObject> golist{};			//every other gameobject in the game

bool keep_running = true;
bool game_start = false;
bool has_started = false;
f32 timer = TOTAL_TIME;

const float BULLET_SPEED = 1000.f;
const float ASTEROID_SPAWN_SPEED = 2.f; //seconds
const float ASTEROID_MIN_SIZE = 50.f;	//min radius
const float ASTEROID_MAX_SIZE = 100.f;	//max radius
const float ASTEROID_MOVE_SPEED = 100.f;
const int SCORE_PER_ASTEROID = 50;
const int NEG_SCORE_PER_HIT = 10;		//player got hit

float latest_timestamp{};

const AEVec2 screen{ 1600.f, 900.f };	//application window width & height

f32 appTime{};

// Forward declares
void ReceiveThread(SOCKET serverSock);
void SendThread(SOCKET serverSock);
void Spawn_Asteroids(SOCKET serverSock);
void InterpolateGameobject(GameObject& go, float timestamp);
void Destroy_Asteroids(SOCKET serverSock, int astId);
void SimpleDynamicCollisionCheck(float dt, SOCKET serverSocket);
Bullet& Shoot(AEVec2 const& pos, float angle, int playerID);
GameObject& SpawnAsteroid();
float RandomFloat(std::mt19937& rng, float min, float max);
int RandomInt(std::mt19937& rng, int min, int max);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AESysInit(hInstance, nCmdShow, 10, 10, 1, 60, true, NULL);

    sockaddr_in server_addr{};
    //char buffer[1024];

	std::string portNumber{};
    std::ifstream file("ServerPort.txt");
    if (!file)
    {
        std::cerr << "cannot open port file" << std::endl;
        return 0;
    }
    std::getline(file, portNumber);

    // Initialize Winsock
    WSADATA wsaData{};
    int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
    if (errorCode != NO_ERROR)
    {
        std::cerr << "WSAStartup() failed." << std::endl;
        return errorCode;
    }

    // Object hints indicates which protocols to use to fill in the info.
    addrinfo hints{};
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;			// IPv4
    // For UDP use SOCK_DGRAM instead of SOCK_STREAM.
    hints.ai_socktype = SOCK_STREAM;	// Reliable delivery
    // Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
    hints.ai_protocol = IPPROTO_UDP;	// UDP
    // Create a passive socket that is suitable for bind() and listen().
    hints.ai_flags = AI_PASSIVE;

	SOCKET soc{ socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) };
	if (soc == INVALID_SOCKET)
	{
		std::cerr << "UDP socket() failed." << std::endl;
		return false;
	}


    char host[MAX_STR_LEN];
    gethostname(host, MAX_STR_LEN);

    addrinfo* info = nullptr;
    errorCode = getaddrinfo(host, portNumber.c_str(), &hints, &info);
    if ((errorCode) || (info == nullptr))
    {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return errorCode;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(std::stoi(portNumber));

    // Bind the socket
    if (bind(soc, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != NO_ERROR) 
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(soc);
        WSACleanup();
        throw std::runtime_error("Bind failed");
    }

    u_long enable = 1;
    ioctlsocket(soc, FIONBIO, &enable);

    char serverIPAddr[MAX_STR_LEN];
    inet_ntop(AF_INET, &(server_addr.sin_addr), serverIPAddr, INET_ADDRSTRLEN);
    getnameinfo(info->ai_addr, static_cast <socklen_t> (info->ai_addrlen), serverIPAddr, sizeof(serverIPAddr), nullptr, 0, NI_NUMERICHOST);\

    std::cout << "Server is listening on port " << portNumber <<" ip "<<serverIPAddr<< " ...\n";

    std::thread recv_thread(ReceiveThread,soc);
    std::thread send_thread(SendThread,soc);

    send_thread.detach();
    recv_thread.detach();
    
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Player player{ {{{0.f, 0.f},{50.f, 50.f}, 0.f}, {}, "player", {1.f, 0.f, 0.f, 1.f}, true}, 0 };
        playersInfo[i] = player;
    }

    while (true)
    {
        if (!game_start)
        {
            continue;
        }

        f32 dt = static_cast<f32>(AEFrameRateControllerGetFrameTime());
        appTime += dt;
        timer -= dt;

        AESysFrameStart();

        static float countDown{};
        countDown -= dt;
        if (countDown <= 0.f)
        {
            countDown = ASTEROID_SPAWN_SPEED;
            Spawn_Asteroids(soc);
            
        }

        //update
        for (auto& p : playersInfo) 
        {
            p.go.Update(screen, dt);
        }
        for (auto& a : golist) {
            a.Update(screen, dt);
        }
        for (auto& b : bulletlist) {
            b.Update(screen, dt);
        }
        //RandomizeAsteroidSpawn(dt);

        //collision check
        SimpleDynamicCollisionCheck(dt,soc);


        if (timer <= 0.f)
        {
            keep_running = false;
            game_start = false;
            std::cout << " finish" << std::endl;

            std::string message{};
            message += C_GAME_END;

            HIGHSCORE::ReadFromHighscoreFile();

            // add player to highscore
            for (int i =0;i<TOTAL_PLAYERS;i++)
            {
                std::string name{ "player " };
                name += (i+1);
                HIGHSCORE::AddHighscore(playersInfo[i].score,name);
            }
            // send highscores
            for (auto& score : HIGHSCORE::highScores)
            {
                unsigned int tmp = htonl(score.score);
                message.append((char*)(&tmp), (char*)(&tmp) + 4);

                long long tmpll = htonll(score.playDate);
                message.append((char*)(&tmpll), (char*)(&tmpll) + 8);

            }

            // send player scores
            for (auto& player : playersInfo)
            {
                unsigned int tmp = htonl(player.score);
                message.append((char*)(&tmp), (char*)(&tmp) + 4);
            }
            // send all clients
            for (auto& client : clients)
            {
                sendto(soc, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client.second), sizeof(client.second));
            }

            HIGHSCORE::WriteToHighscoreFile();
        }

        AESysFrameEnd();
    }

    AESysExit();
}

void ReceiveThread(SOCKET serverSock) 
{
    char buffer[MAX_STR_LEN];
    sockaddr_in client_addr{};
    int client_addr_len = sizeof(client_addr);

    while (keep_running) 
    {
        int bytes_received = recvfrom(serverSock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*> (&client_addr), &client_addr_len);

        if (bytes_received == SOCKET_ERROR) {
            //std::cerr << "Recvfrom failed: " << WSAGetLastError() << std::endl;
            //std::this_thread::sleep_for(std::chrono::milliseconds(40)); // Avoid tight loop
            //continue;

            size_t errorCode = WSAGetLastError();
            if (errorCode == WSAEWOULDBLOCK)
            {
                // A non-blocking call returned no data; sleep and try again.
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_RATE));

                std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
                //std::cerr << "trying again..." << std::endl;
                continue;
            }
            std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
            std::cerr << "recv() failed." << std::endl;
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        std::string IpPort = client_ip;
        IpPort += ":";
        IpPort += std::to_string(ntohs(client_addr.sin_port));

        if (buffer[0] == C_REQ_CONNECT)
        {
            if (clients.size() < TOTAL_PLAYERS)
            {
                std::pair<std::string, int> newIndex{};

                {
                    std::lock_guard<std::mutex> lock(Mutex);
                    newIndex = std::pair<std::string, int>(IpPort, (int)clients.size());
                    playersIndex.insert(newIndex);

                    std::pair<std::string, sockaddr_in> newClient(IpPort, client_addr);
                    clients.insert(newClient);
                }

                std::string message{};
                message += C_RSP_CONNECT;

                int tmp = htonl(newIndex.second);
                message.append((char*)(&tmp), (char*)(&tmp) + 4);

                sendto(serverSock, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client_addr), sizeof(client_addr));

                if (clients.size() == TOTAL_PLAYERS)
                {
                    game_start = true;
                    appTime = 0;

                    // send all clients
                  /*  for (auto& client : clients)
                    {
                        sendto(serverSock, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client.second), sizeof(client.second));
                    }*/
                }
            }
        }

        // Player fire
        if (buffer[0] == C_REQ_FIRE)
        {
            int tmpId{};
            // find player ID who sent

            {
                std::lock_guard<std::mutex> lock(Mutex);
                for (auto& player : playersIndex)
                {
                    if (player.first == IpPort)
                    {
                        tmpId = player.second;
                    }
                }
            }

            float timestamp = ntohf(*(uint32_t*)(buffer + 1));

            std::string message{};
            message += C_RSP_FIRE;

            unsigned int tmp = htonf(appTime);
            message.append((char*)(&tmp), (char*)(&tmp) + 4);

            tmp = htonl(tmpId);
            message.append((char*)(&tmp), (char*)(&tmp) + 4);
            
            // Send to all that player ID fire
            for (auto& ips : clients)
            {
                sendto(serverSock, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&ips.second), sizeof(ips.second));
            }

            Shoot(playersInfo[tmpId].go.t.pos, playersInfo[tmpId].go.t.rot, tmpId);
        }

        // State update from client
        // id - 1b, timestamp - 4b, pos - 8b, scale - 8b, rot - 4b, vel - 8b
        if (buffer[0] == C_STATE_UPDATE)
        {
            int tmpId{};
            // find player ID who sent
            {
                std::lock_guard<std::mutex> lock(Mutex);
                for (auto& player : playersIndex)
                {
                    if (player.first == IpPort)
                    {
                        tmpId = player.second;
                    }
                }
            }

            latest_timestamp = ntohf(*(uint32_t*)(buffer + 1));

            {
                std::lock_guard<std::mutex> lock(Mutex);
                if (latest_timestamp > playersInfo[tmpId].timestamp)
                {
                    playersInfo[tmpId].timestamp = latest_timestamp;
                }
                else
                {
                    continue;
                }
            }

            AEVec2 pos{};
            pos.x = ntohf(*(uint32_t*)(buffer + 5));
            pos.y = ntohf(*(uint32_t*)(buffer + 9));

            AEVec2 scale{};
            scale.x = ntohf(*(uint32_t*)(buffer + 13));
            scale.y = ntohf(*(uint32_t*)(buffer + 17));

            float rot = ntohf(*(uint32_t*)(buffer + 21));

            AEVec2 vel{};
            vel.x = ntohf(*(uint32_t*)(buffer + 25));
            vel.y = ntohf(*(uint32_t*)(buffer + 29));

            {
                std::lock_guard<std::mutex> lock(Mutex);
                playersInfo[tmpId].go.t.pos = pos;
                playersInfo[tmpId].go.t.scale = scale;
                playersInfo[tmpId].go.t.rot = rot;
                playersInfo[tmpId].go.vel = vel;

                // interpolate
                InterpolateGameobject(playersInfo[tmpId].go, latest_timestamp);
            }
        }


        {
            //std::lock_guard<std::mutex> lock(Mutex); // RAII lock
            //buffer[bytes_received] = '\0';
            
            //std::cout << "Received from " << IpPort << std::endl;
        }
    }
}

// Sender thread function
void SendThread(SOCKET serverSocket) 
{
    while (keep_running) 
    {
        if (game_start)
        {
            if (!has_started)
            {
                std::string message{};
                message += C_GAME_START;

                for (auto client : clients)
                {
                    int bytes_sent = sendto(serverSocket, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client.second), sizeof(client.second));
                    if (bytes_sent == SOCKET_ERROR)
                    {
                        std::lock_guard<std::mutex> lock(Mutex);
                        std::cerr << "Sendto failed: " << WSAGetLastError() << std::endl;
                    }
                    else {
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &client.second.sin_addr, client_ip, INET_ADDRSTRLEN);
                        std::lock_guard<std::mutex> lock(Mutex);
                        std::cout << "Game start sent to " << client.first << " - " << message << std::endl;
                    }
                }

                has_started = true;
            }

            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_RATE));

                std::string message{};
                message += C_ALL_UPDATE;

                // timestamp
                unsigned int tmp = htonf(appTime);
                message.append((char*)(&tmp), (char*)(&tmp) + 4);

                // send pos, scale, rot, vel by order of player index 
                {
                    std::lock_guard<std::mutex> lock(Mutex);
                    for (auto& player : playersInfo)
                    {
                        tmp = htonf(player.go.t.pos.x);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);
                        tmp = htonf(player.go.t.pos.y);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);

                        tmp = htonf(player.go.t.scale.x);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);
                        tmp = htonf(player.go.t.scale.y);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);

                        tmp = htonf(player.go.t.rot);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);

                        tmp = htonf(player.go.vel.x);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);
                        tmp = htonf(player.go.vel.y);
                        message.append((char*)(&tmp), (char*)(&tmp) + 4);
                    }
                }

                // sending to clients
                {
                    std::lock_guard<std::mutex> lock(Mutex);
                    for (auto client : clients)
                    {
                        int bytes_sent = sendto(serverSocket, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client.second), sizeof(client.second));
                        if (bytes_sent == SOCKET_ERROR)
                        {
                            std::lock_guard<std::mutex> lock(Mutex);
                            std::cerr << "Sendto failed: " << WSAGetLastError() << std::endl;
                        }
                        else {
                            char client_ip[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &client.second.sin_addr, client_ip, INET_ADDRSTRLEN);
                            //std::lock_guard<std::mutex> lock(Mutex);
                            //std::cout << "Update All sent to " << client.first << " - " << message << std::endl;
                        }
                    }
                }


                // time sync every 1 min
                if (std::fmod(appTime,TIME_SYNC) < 0.005f && game_start)
                {
                    std::string message{};
                    message += C_TIME_SYNC;

                    // timestamp
                    unsigned int tmp = htonf(appTime);
                    message.append((char*)(&tmp), (char*)(&tmp) + 4);

                    // send pos, scale, rot, vel by order of player index 
                    {
                        std::lock_guard<std::mutex> lock(Mutex);
                        for (auto& player : playersInfo)
                        {
                            tmp = htonf(player.go.t.pos.x);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);
                            tmp = htonf(player.go.t.pos.y);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);

                            tmp = htonf(player.go.t.scale.x);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);
                            tmp = htonf(player.go.t.scale.y);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);

                            tmp = htonf(player.go.t.rot);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);

                            tmp = htonf(player.go.vel.x);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);
                            tmp = htonf(player.go.vel.y);
                            message.append((char*)(&tmp), (char*)(&tmp) + 4);
                        }
                    }

                    // sending to clients
                    {
                        std::lock_guard<std::mutex> lock(Mutex);
                        for (auto client : clients)
                        {
                            int bytes_sent = sendto(serverSocket, message.c_str(), (int)message.length(), 0, reinterpret_cast<sockaddr*>(&client.second), sizeof(client.second));
                            if (bytes_sent == SOCKET_ERROR)
                            {
                                std::lock_guard<std::mutex> lock(Mutex);
                                std::cerr << "Sendto failed: " << WSAGetLastError() << std::endl;
                            }
                            else {
                                char client_ip[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &client.second.sin_addr, client_ip, INET_ADDRSTRLEN);
                                //std::lock_guard<std::mutex> lock(Mutex);
                                //std::cout << "Update All sent to " << client.first << " - " << message << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }
}

void InterpolateGameobject(GameObject& go, float timestamp) {
    float deltaTime = appTime - timestamp;
    go.t.pos.x += go.vel.x * deltaTime;
    go.t.pos.y += go.vel.y * deltaTime;
}

//player shooting bullet logic - returns ref to the bullet that was shot
Bullet& Shoot(AEVec2 const& pos, float angle, int playerID)
{
    float rad{ AEDegToRad(angle) };
    AEVec2 vel{ cosf(rad) * BULLET_SPEED, sinf(rad) * BULLET_SPEED };
    //bullet - go, lifetime, isactive
    Bullet tmp{ { { pos, { 10.f, 10.f }, angle }, vel, "bullet", playersInfo[playerID].go.col, true}, 1.f, playerID };
    //finds non active bullet in list to replace
    for (auto& b : bulletlist)
    {
        if (!b.go.isActive)
        {
            b = tmp;
            return b;
        }
    }
    bulletlist.push_back(tmp);

    return bulletlist.back();
}

float RandomFloat(std::mt19937& rng, float min, float max)
{
    std::uniform_real_distribution<float> uf(min, max);
    return uf(rng);
}
int RandomInt(std::mt19937& rng, int min, int max)
{
    std::uniform_int_distribution<int> ui(min, max);
    return ui(rng);
}

GameObject& SpawnAsteroid() {
    //init the random number engine
    //static std::random_device rd;
    static std::mt19937 rng(1);	//seed 1
    enum SpawnLocation : int
    {
        UP = 0,
        DOWN = 1,
        LEFT = 2,
        RIGHT = 3
    };

    int sl{ RandomInt(rng, UP, RIGHT) };
    float radius{ RandomFloat(rng, ASTEROID_MIN_SIZE, ASTEROID_MAX_SIZE) };
    GameObject asteroid{ {{0.f, 0.f},{radius, radius}, RandomFloat(rng, 0.f, 359.f)}, {}, "asteroid", {0.f, 0.f, 0.f, 1.f}, true };
    switch (sl)
    {
    case UP:
        //spawn TOP region, moving downwards prio
    {
        asteroid.t.pos.x = RandomFloat(rng, -screen.x * 0.5f, screen.x * 0.5f);
        asteroid.t.pos.y = screen.y * 0.5f + radius;
        float hSpeed{ ASTEROID_MOVE_SPEED * 0.4f };
        asteroid.vel.x = RandomFloat(rng, -hSpeed, hSpeed);
        asteroid.vel.y = -1.f * (ASTEROID_MOVE_SPEED - hSpeed);
    }
    break;
    case DOWN:
        //spawn BTM region, moving upwards prio
    {
        asteroid.t.pos.x = RandomFloat(rng, -screen.x * 0.5f, screen.x * 0.5f);
        asteroid.t.pos.y = -screen.y * 0.5f - radius;
        float hSpeed{ ASTEROID_MOVE_SPEED * 0.4f };
        asteroid.vel.x = RandomFloat(rng, -hSpeed, hSpeed);
        asteroid.vel.y = ASTEROID_MOVE_SPEED - hSpeed;
    }
    break;
    case LEFT:
        //spawn LEFT region, moving right prio
    {
        asteroid.t.pos.x = -screen.x * 0.5f - radius;
        asteroid.t.pos.y = RandomFloat(rng, -screen.y * 0.5f, screen.y * 0.5f);
        float vSpeed{ ASTEROID_MOVE_SPEED * 0.4f };
        asteroid.vel.y = RandomFloat(rng, -vSpeed, vSpeed);
        asteroid.vel.x = ASTEROID_MOVE_SPEED - vSpeed;
    }
    break;
    case RIGHT:
        //spawn RIGHT region, moving left prio
    {
        asteroid.t.pos.x = screen.x * 0.5f + radius;
        asteroid.t.pos.y = RandomFloat(rng, -screen.y * 0.5f, screen.y * 0.5f);
        float vSpeed{ ASTEROID_MOVE_SPEED * 0.4f };
        asteroid.vel.y = RandomFloat(rng, -vSpeed, vSpeed);
        asteroid.vel.x = -1.f * (ASTEROID_MOVE_SPEED - vSpeed);
    }
    break;
    }

    //find inactive in golist to replace, if no space pushback
    for (GameObject& go : golist)
    {
        if (!go.isActive && go.texid != "player")
        {
            go = asteroid;
            return go;
        }
    }
    golist.push_back(asteroid);
    return golist.back();
}

//collision check
void SimpleDynamicCollisionCheck(float dt, SOCKET serverSocket)
{
    //Player& player = playersInfo[];
    //check if player hit an asteroid
    for (int i = 0; i < golist.size(); i++)
    {
        GameObject& go = golist[i];
        if (!go.isActive || go.texid != "asteroid") {
            continue;
        }
        for(auto& player : playersInfo)
        {
            if (COLLISION::IsWithinDistanceCheckDynamic(player.go, go, dt))
            {
                player.score -= NEG_SCORE_PER_HIT;
                go.isActive = false;
                Destroy_Asteroids(serverSocket, i);
            }
        }
    }

    //check if any bullet hit an asteroid
    for (Bullet& b : bulletlist)
    {
        if (!b.go.isActive) {
            continue;
        }
        for (int i =0; i<golist.size();i++)
        {
            GameObject& go = golist[i];
            if (!go.isActive || go.texid != "asteroid") {
                continue;
            }
            if (COLLISION::IsWithinDistanceCheckDynamic(b.go, go, dt))
            {
                b.go.isActive = go.isActive = false;
                playersInfo[b.playerNO].score += SCORE_PER_ASTEROID;
                Destroy_Asteroids(serverSocket, i);
                break;
            }
        }
    }
}

void Spawn_Asteroids(SOCKET serverSock)
{
    std::lock_guard<std::mutex> lock(Mutex);

    std::string msg{};
    msg += C_ASTEROID_SPAWN;

    unsigned int tmp = htonf(appTime);
    msg.append((char*)(&tmp), (char*)(&tmp) + 4);

    // Send to all to start spawning asteroid
    for (auto& ips : clients)
    {
        sendto(serverSock, msg.c_str(), (int)msg.length(), 0, reinterpret_cast<sockaddr*>(&ips.second), sizeof(ips.second));
    }

    SpawnAsteroid();
}

void Destroy_Asteroids(SOCKET serverSock, int astId)
{
    std::lock_guard<std::mutex> lock(Mutex);

    std::string msg{};
    msg += C_ASTEROID_DESTROY;

    unsigned int tmp = htonl(astId);
    msg.append((char*)(&tmp), (char*)(&tmp) + 4);

    // Send to all to start spawning asteroid
    for (auto& ips : clients)
    {
        sendto(serverSock, msg.c_str(), (int)msg.length(), 0, reinterpret_cast<sockaddr*>(&ips.second), sizeof(ips.second));
    }
}
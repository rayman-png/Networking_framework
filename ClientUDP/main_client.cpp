/*!
\file		main_client.cpp
\author		darius (d.chan@digipen.edu)
\co-author	
\par		Assignment 4
\date		01/04/2025
\brief
this is the client file for the multiplayer space shooter game

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
#include <iostream> // console output
#include <sstream>  //ease of manipulating strings
#include <iomanip>  //floating point display precision
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"  //uses winsock(disabled via macro)
#include "gameobject.h"
#include <map>
#include <vector>
#include <random>
#include "collision.h"

#include "Math.h"
#include "Network.h"
#include "Global.h"


std::vector<Bullet> bulletlist{};			//every bullet in the game
std::vector<GameObject> golist{};			//every other gameobject in the game
int playerNO{ 0 };							//[0,3] the id number of the current player, also decides the player's color
Player players[4]{};
float appTime{ 0.f };
std::pair<ULONG, ULONGLONG> highscores[5]{};

std::atomic<bool> gameRunning = false;
namespace
{

	//CONST DEFINITIONS
	const AEVec2 screen{ 1600.f, 900.f };	//application window width & height
	const float BULLET_SPEED = 1000.f;		//player bullet speed
	
	const float ACCELERATION = 100.f;		//player acceleration - increase by 10 per second
	const float MAX_SPEED = 200.f;			//player max speed

	const float ROTATE_SPEED = 100.f;		//player rotate
	const float ASTEROID_SPAWN_SPEED = 2.f; //seconds
	const float ASTEROID_MIN_SIZE = 50.f;	//min radius
	const float ASTEROID_MAX_SIZE = 100.f;	//max radius
	const float ASTEROID_MOVE_SPEED = 100.f;
	const int SCORE_PER_ASTEROID = 50;		//player shot asteroid
	const int NEG_SCORE_PER_HIT = 10;		//player got hit

	//gameobject data
	std::string playerName{ std::to_string(playerNO + 1) }; //player chosen name(needed for highscore)
	//Player player{ {{{0.f, 0.f},{50.f, 50.f}, 0.f}, {}, "player", {1.f, 0.f, 0.f, 1.f}, true}, 0 };

	//mesh and texture data
	AEGfxVertexList* meshList[2];
	std::map<std::string, AEGfxTexture*> texMap;
	s8 dFont;

	void InitMesh()
	{
		//rect mesh
		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 1.0f,
			0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
			-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);

		AEGfxTriAdd(
			0.5f, -0.5f, 0xFFFFFFFF, 1.0f, 1.0f,
			0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,
			-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
		meshList[0] = AEGfxMeshEnd();

		//triangle mesh
		AEGfxMeshStart();
		AEGfxTriAdd(
			-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f,
			-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f,
			0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);
		meshList[1] = AEGfxMeshEnd();
	}
	void InitTexture()
	{
		texMap["player"] = AEGfxTextureLoad("Assets/ship.png");
		texMap["asteroid"] = AEGfxTextureLoad("Assets/planet.png");
		texMap["bullet"] = nullptr;
		dFont = AEGfxCreateFont("Assets/liberation-mono.ttf", 75);
	}
	void Free()
	{
		AEGfxDestroyFont(dFont);
		for (int i{}; i < 2; ++i) {
			AEGfxMeshFree(meshList[i]);
		}

		for (auto p : texMap) {
			if (p.second) {
				AEGfxTextureUnload(p.second);
			}
		}
	}

	//player shooting bullet logic - returns ref to the bullet that was shot
	Bullet& Shoot(AEVec2 const& pos, float angle, int playerID)
	{
		float rad{ AEDegToRad(angle) };
		AEVec2 vel{ cosf(rad) * BULLET_SPEED, sinf(rad) * BULLET_SPEED};
		//bullet - go, lifetime, isactive
		Bullet tmp{ { { pos, { 10.f, 10.f }, angle }, vel, "bullet", players[playerID].go.col, true}, 1.f, playerID };
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

	//update game data based on player's input
	void UpdateInput(float dt)
	{
		Player& player = players[playerNO];
		int inputDir = 0;
		int rotateDir = 0;
		//movement
		if (AEInputCheckCurr(AEVK_UP))
		{
			inputDir++;
		}
		if (AEInputCheckCurr(AEVK_DOWN))
		{
			inputDir--;
		}
		if (AEInputCheckCurr(AEVK_LEFT))
		{
			rotateDir++;
		}
		if (AEInputCheckCurr(AEVK_RIGHT))
		{
			rotateDir--;
		}

		//Update rotation first
		if (rotateDir) {
			player.go.t.rot += ROTATE_SPEED * dt * (float)(rotateDir);
			player.go.t.rot = Wrap(player.go.t.rot, 0.f, 360.f);
		}
		//Update acceleration next
		if (inputDir) {
			float rad{ AEDegToRad(player.go.t.rot) };
			AEVec2 dir = { cosf(rad), sinf(rad) };
			AEVec2 dv;	//change in vel
			AEVec2Scale(&dv, &dir, ACCELERATION * dt * (float)inputDir);
			player.go.vel.x += dv.x;
			player.go.vel.y += dv.y;
			float mag = AEVec2Length(&player.go.vel);
			if (mag > MAX_SPEED) {
				float factor = (MAX_SPEED) / mag;
				player.go.vel.x *= factor;
				player.go.vel.y *= factor;
			}
		}
		
		//shoot logic
		static float shootCooldown{ 0.5f };
		if (shootCooldown <= 0.f && AEInputCheckCurr(AEVK_SPACE))
		{
			//Send fire event
			{
				std::lock_guard<std::mutex> eventMut(_eventMutex);
				event_queue.push(appTime);
			}

			//Shoot(player.go.t.pos, player.go.t.rot);
			shootCooldown = 0.5f;
		}
		else if (shootCooldown > 0.f) shootCooldown -= dt;
	}

	//TO BE PUT IN SERVER: asteroid spawn + collision check:
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

	//asteroids will spawn endlessly out of screen at first and will always move in one direction
	void RandomizeAsteroidSpawn(float dt)
	{
		//init the random number engine
		static std::random_device rd;
		static std::mt19937 rng(rd());
		enum SpawnLocation : int
		{
			UP    = 0,
			DOWN  = 1,
			LEFT  = 2,
			RIGHT = 3
		};

		static float countDown{ ASTEROID_SPAWN_SPEED };
		if ((countDown -= dt) <= 0.f)
		{
			countDown = ASTEROID_SPAWN_SPEED;
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
					return;
				}
			}
			golist.push_back(asteroid);
		}
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
	void SimpleDynamicCollisionCheck(float dt)
	{
		Player& player = players[playerNO];
		//check if player hit an asteroid
		for (GameObject& go : golist)
		{
			if (!go.isActive || go.texid != "asteroid") { 
				continue; 
			}
			if (COLLISION::IsWithinDistanceCheckDynamic(player.go, go, dt))
			{
				player.score -= NEG_SCORE_PER_HIT;
				go.isActive = false;
			}
		}

		//check if any bullet hit an asteroid
		for (Bullet& b : bulletlist)
		{
			if (!b.go.isActive) {
				continue;
			}
			for (GameObject& go : golist)
			{
				if (!go.isActive || go.texid != "asteroid") {
					continue; 
				}
				if (COLLISION::IsWithinDistanceCheckDynamic(b.go, go, dt))
				{
					b.go.isActive = go.isActive = false;
					player.score += SCORE_PER_ASTEROID;
					break;
				}
			}
		}
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	
	//f32 appTime{};



	enum : s32
	{
		INIT_HIDE_CONSOLE = 0,
		INIT_SHOW_CONSOLE = 1
	}option{INIT_SHOW_CONSOLE};
	AESysInit(hInstance, nCmdShow, (s32)screen.x, (s32)screen.y, option, 60, true, NULL);
	AESysSetWindowTitle("Space Shooter");

	if (!ConnectServer()) {
		std::cerr << "Unable to connect to server" << std::endl;
		DisconnectServer();
		AESysExit();
		return -1;
	}


	//initialize game
	InitMesh();
	InitTexture();
	GameObject background{ {{0.f, 0.f}, screen, 0.f}, {}, "", {.0f, .0f, .0f, 1.f}, true };
	GameObject shade{ {{0.f, 0.f}, screen, 0.f}, {}, "", {.5f, .5f, .5f, .5f}, true };

	//Init players
	for (int i = 0; i < 4; ++i) {
		players[i] = { {{{0.f, 0.f},{50.f, 50.f}, 0.f}, {}, "player", {1.f, 0.f, 0.f, 1.f}, true}, 0 };
		switch (i) //defined in empty namespace
		{
		case 0:
			players[i].go.col = RED;
			break;
		case 1:
			players[i].go.col = GREEN;
			break;
		case 2:
			players[i].go.col = BLUE;
			break;
		case 3:
			players[i].go.col = PURPLE;
			break;
		}
	}

	if (!WaitGameStart()) {
		DisconnectServer();
		AESysExit();
		return -1;
	}

	AESysReset();
	while (gameRunning)
	{
		//quit
		if (!AESysDoesWindowExist() || AEInputCheckCurr(AEVK_ESCAPE)) {
			break;
		}
		AESysFrameStart();
		f32 dt = static_cast<f32>(AEFrameRateControllerGetFrameTime());
		appTime += dt;
		
		//input
		UpdateInput(dt);
		{
			std::lock_guard<std::mutex> mut(_gameObjectMutex);
			//update
			for (Player& p : players) {
				p.go.Update(screen, dt);
			}
			for (auto& a : golist) {
				a.Update(screen, dt);
			}
			for (auto& b : bulletlist) {
				b.Update(screen, dt);
			}
		}

		//render
		background.Render(meshList[0]);

		{
			std::lock_guard<std::mutex> mut(_gameObjectMutex);
			for (Player& p : players) {
				p.go.Render(meshList[0], texMap[p.go.texid]);
			}
			//player.go.Render(meshList[0], texMap[player.go.texid]);

			for (auto const& a : golist) {
				a.Render(meshList[0], texMap[a.texid]);
			}

			for (auto const& b : bulletlist) {
				b.Render(meshList[0], texMap[b.go.texid]);
			}
		}

		AESysFrameEnd();
	}

	DisconnectServer();



	//Display highscore
	while (true) {
		AESysFrameStart();
		if (!AESysDoesWindowExist() || AEInputCheckCurr(AEVK_ESCAPE)) {
			break;
		}
		background.Render(meshList[0]);
		{
			std::lock_guard<std::mutex> mut(_gameObjectMutex);
			for (Player& p : players) {
				p.go.Render(meshList[0], texMap[p.go.texid]);
			}
			//player.go.Render(meshList[0], texMap[player.go.texid]);

			for (auto const& a : golist) {
				a.Render(meshList[0], texMap[a.texid]);
			}

			for (auto const& b : bulletlist) {
				b.Render(meshList[0], texMap[b.go.texid]);
			}
		}

		//print highscores gotten from server
		shade.Render(meshList[0]);
		AEGfxPrint(dFont, "Highscores", -.15f, .8f, .5f, 1.f, 1.f, 1.f, 1.f);
		std::ostringstream description{};
		description << std::left << std::setw(8) << "Scores" << std::right << std::setw(11) << "Date";
		AEGfxPrint(dFont, description.str().c_str(), -.3f, 0.7f, .5f, 1.f, 1.f, 1.f, 1.f);
		int arSize{ sizeof(highscores) / sizeof(highscores[0]) };
		for (int i{}; i < arSize; ++i)
		{
			std::tm localTime{};
			if (localtime_s(&localTime, (std::time_t*)&highscores[i].second) != 0)
			{
				std::cerr << "localtime_s FAILED" << std::endl;
				return EXIT_FAILURE; //error handling
			}

			std::ostringstream date{};
			date << std::left << std::setw(9) << highscores[i].first << std::right << std::setw(11) << std::put_time(&localTime, "%Y-%m-%d"); //example: "9990  2025-04-01"
			AEGfxPrint(dFont, date.str().c_str(), -.3f, .6f - i * 0.15f, .5f, 1.f, 1.f, 1.f, 1.f);
		}

		//print current game players' scores
		int parSize{ sizeof(players) / sizeof(players[0]) };
		for (int i{}; i < parSize; ++i)
		{
			std::string out{ "Player" + std::to_string(i) + ':' + std::to_string(players[i].score) };
			AEGfxPrint(dFont, out.c_str(), -.9f + i * 0.5f, -.8f, .5f, 1.f, 1.f, 1.f, 1.f);
		}
		AESysFrameEnd();
	}

	Free();

	AESysExit();

	return 0;
}

//Sync timestamp into current app time
void InterpolateGameobject(GameObject& go, float timestamp) {
	float deltaTime = appTime - timestamp;
	go.t.pos.x += go.vel.x * deltaTime;
	go.t.pos.y += go.vel.y * deltaTime;
}

//Sync bullet drawing
void InterpolatedShoot(int playerID, float timestamp) {
	Bullet& bullet = Shoot(players[playerID].go.t.pos, players[playerID].go.t.rot, playerID);
	if (playerID != playerNO) {	//DONT interpolate if same id
		float deltaTime = appTime - timestamp;
		bullet.go.t.pos.x += bullet.go.vel.x * deltaTime;
		bullet.go.t.pos.y += bullet.go.vel.y * deltaTime;

	}
}

//Interpolating Asteroids to the current timesync
void InterpolateGOsync(float timestamp) {
	//mutex acquired from stack below

	float dt_offset = timestamp - appTime;
	//bullet
	for (auto& bul : bulletlist) {
		if (!bul.go.isActive) {
			continue;
		}
		bul.go.Update(screen, dt_offset);
	}

	for (auto& asteroid : golist) {
		if (!asteroid.isActive) {
			continue;
		}
		asteroid.Update(screen, dt_offset);
	}
}

void SpawnInterpolatedAsteroid(float timestamp) {
	//std::lock_guard<std::mutex> goLock{ _gameObjectMutex };
	//Got mutex from stack

	GameObject& asteroid = SpawnAsteroid();
	float deltaTime = appTime - timestamp;
	asteroid.t.pos.x += asteroid.vel.x * deltaTime;
	asteroid.t.pos.y += asteroid.vel.y * deltaTime;
}
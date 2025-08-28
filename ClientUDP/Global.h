/*!
\file		Global.h
\author		Elton Leosantosa (leosantosa.e@digipen.edu)
\co-author
\par		Assignment 4
\date		01/04/2025
\brief
	Global variables that are shared across different files

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
#ifndef GLOBAL_H
#define	GLOBAL_H

#include <vector>
#include <chrono>
#include <utility>

struct GameObject;
struct Bullet;
struct Player;

extern std::vector<Bullet> bulletlist;			//every bullet in the game
extern std::vector<GameObject> golist;			//every other gameobject in the game
extern int playerNO;							//number for what is the current player
extern Player players[4];
extern float appTime;						//Total amount of time that has passed in the game
extern std::atomic<bool> gameRunning;
extern std::pair<ULONG, ULONGLONG> highscores[5];

void InterpolateGameobject(GameObject& go, float timestamp);
void InterpolatedShoot(int playerID, float timestamp);
void InterpolateGOsync(float newTimestamp);

void SpawnInterpolatedAsteroid(float newTimestamp);
#endif
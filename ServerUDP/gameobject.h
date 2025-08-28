/*!
\file		gameobject.h
\author		darius (d.chan@digipen.edu)
\co-author
\par		Assignment 4
\date		01/04/2025
\brief
contains gameobject data: transform
and other struct that is a gameobject: bullet, player

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
#pragma once
#include "AEEngine.h"
#include <string>

//r,g,b,a
struct Color
{
	float r, g, b, a;
};
const Color RED{ 1.f, 0.f, 0.f, 1.f }, GREEN{ 0.f, 1.f, 0.f, 1.f }, BLUE{ 0.f, 0.f, 1.f, 1.f }, PURPLE{ 1.f, 0.f, 1.f, 1.f };

//pos, scale, rotation
struct Transform
{
	AEVec2 pos, scale;
	float rot;
};

//Transform, vel, texString, color, isactive
struct GameObject
{
	Transform t;
	AEVec2 vel;
	std::string texid;
	Color col;
	bool isActive;

	//simple update pos based on vel, will loop over to other side
	void Update(AEVec2 const& screenSize, float dt);
	void Render(AEGfxVertexList* meshPtr, AEGfxTexture* texPtr = nullptr) const;
};

//GameObject(Transform, vel, texString, color, isactive), score
struct Player
{
	GameObject go;
	int score;
	float timestamp;
};

//GameObject(Transform, vel, texString, color, isactive), lifetime, playerNO
struct Bullet
{
	GameObject go;
	float lifeTime;
	int playerNO;
	void Update(AEVec2 const& screenSize, float dt);
	void Render(AEGfxVertexList* meshPtr, AEGfxTexture* texPtr = nullptr) const;
};

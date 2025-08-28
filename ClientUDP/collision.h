/*!
\file		collision.h
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
#pragma once
#include "gameobject.h"

namespace COLLISION
{
	bool IsWithinDistanceCheck(Transform const& a, Transform const& b);
	bool IsWithinDistanceCheckDynamic(GameObject const& a, GameObject const& b, float dt);

}
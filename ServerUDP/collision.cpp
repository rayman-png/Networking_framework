/*!
\file		collision.cpp
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
#include "collision.h"

namespace COLLISION
{
	float DSquared(AEVec2 const& a, AEVec2 const& b)
	{
		return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
	}
	bool IsWithinDistanceCheck(Transform const& a, Transform const& b)
	{
		//chooses the bigger size of a and b to be the radius
		float
			sizeA{ a.scale.x > a.scale.y ? 0.5f * a.scale.x : 0.5f * a.scale.y },
			sizeB{ b.scale.x > b.scale.y ? 0.5f * b.scale.x : 0.5f * b.scale.y },
			totalRadius{ (sizeA + sizeB) * (sizeA + sizeB) };

		//check if distance squared is smaller than the two radius combined
		return DSquared(a.pos, b.pos) <= totalRadius;
	}
	bool IsWithinDistanceCheckDynamic(GameObject const& a, GameObject const& b, float dt)
	{
		//check if is already within distance
		if (IsWithinDistanceCheck(a.t, b.t)) return true;

		//now do collision based on time

		AEVec2 relativeVel{ a.vel.x - b.vel.x, a.vel.y - b.vel.y };
		//if no movement = will never collide
		if (relativeVel.x == 0.f && relativeVel.y == 0.f) return false;

		AEVec2 relativePos{ a.t.pos.x - b.t.pos.x, a.t.pos.y - b.t.pos.y };

		float
			aDotV{ AEVec2DotProduct(&relativePos, &relativeVel) },
			vDotV{ AEVec2DotProduct(&relativeVel, &relativeVel) };

		//check if moving away from each other
		if (aDotV >= 0) return false;

		float t1{ -aDotV / vDotV };
		AEVec2 cPos1{ relativePos.x + relativeVel.x * t1, relativePos.y + relativeVel.y * t1 };
		float
			sizeA{ a.t.scale.x > a.t.scale.y ? 0.5f * a.t.scale.x : 0.5f * a.t.scale.y },
			sizeB{ b.t.scale.x > b.t.scale.y ? 0.5f * b.t.scale.x : 0.5f * b.t.scale.y },
			totalRadius{ (sizeA + sizeB) * (sizeA + sizeB) };

		if (DSquared(cPos1, {}) > totalRadius) return false;

		//check if collision is this frame
		if (t1 >= 0.f && t1 <= dt) return true;

		/*
		AEVec2 cPos2{ relativePos.x + relativeVel.x * dt, relativePos.y + relativeVel.y * dt };
		if (DSquared(cPos2, {}) <= totalRadius) return true;
		*/

		return false;
	}
}
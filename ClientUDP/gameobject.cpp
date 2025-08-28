/*!
\file		gameobject.cpp
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
#include "gameobject.h"

namespace //helper function
{
	AEMtx33 TransformToMat(Transform const& t)
	{
		AEMtx33 result{};

		AEMtx33 translate{}, rot{}, scale{};
		AEMtx33Trans(&translate, t.pos.x, t.pos.y);
		AEMtx33RotDeg(&rot, t.rot);
		AEMtx33Scale(&scale, t.scale.x, t.scale.y);

		AEMtx33Concat(&result, &rot, &scale);
		AEMtx33Concat(&result, &translate, &result);
		return result;
	}
}

void GameObject::Update(AEVec2 const& screenSize, float dt)
{
	//skips update when not active
	if (!isActive) return;

	t.pos.x += vel.x * dt;
	t.pos.y += vel.y * dt;

	//loop over to other side logic (only check with two sides of the wall at max according to velocity)
	float hW{ screenSize.x * 0.5f }, hH{ screenSize.y * 0.5f };
	//if going right
	if (vel.x > 0.f && t.pos.x - t.scale.x * 0.5f > hW) t.pos.x -= screenSize.x + t.scale.x;
	//else if going left
	else if (vel.x < 0.f && t.pos.x + t.scale.x * 0.5f < -hW) t.pos.x += screenSize.x + t.scale.x;
	//if going up
	if (vel.y > 0.f && t.pos.y - t.scale.y * 0.5f > hH) t.pos.y -= screenSize.y + t.scale.y;
	//else if going down
	else if (vel.y < 0.f && t.pos.y + t.scale.y * 0.5f < -hH) t.pos.y += screenSize.y + t.scale.y;
}

void GameObject::Render(AEGfxVertexList* meshPtr, AEGfxTexture* texPtr) const
{
	//skips rendering when not active
	if (!isActive) return;

	AEMtx33 mat{ TransformToMat(t) };

	if (texPtr == nullptr)
		AEGfxSetRenderMode(AEGfxRenderMode::AE_GFX_RM_COLOR);
	else
		AEGfxSetRenderMode(AEGfxRenderMode::AE_GFX_RM_TEXTURE);
	AEGfxTextureSet(texPtr, 0, 0);

	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	if (texPtr == nullptr)
	{
		AEGfxSetBlendColor(1.f, 1.f, 1.f, 1.f);
		AEGfxSetColorToMultiply(col.r, col.g, col.b, col.a);
	}
	else
	{
		AEGfxSetBlendColor(0.f, 0.f, 0.f, 0.f);
		AEGfxSetColorToMultiply(1.f, 1.f, 1.f, 1.f);
		AEGfxSetColorToAdd(col.r, col.g, col.b, col.a);
	}
	AEGfxSetTransparency(1.f);
	AEGfxSetTransform(mat.m);
	AEGfxMeshDraw(meshPtr, AEGfxMeshDrawMode::AE_GFX_MDM_TRIANGLES);
}

void Bullet::Update(AEVec2 const& screenSize, float dt)
{
	if (go.isActive)
	{
		go.Update(screenSize, dt);
		if ((lifeTime -= dt) < 0.f) go.isActive = false;
	}
}

void Bullet::Render(AEGfxVertexList* meshPtr, AEGfxTexture* texPtr) const
{
	go.Render(meshPtr, texPtr);
}
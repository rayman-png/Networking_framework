/*!
\file		highscore.h
\author		darius (d.chan@digipen.edu)
\co-author
\par		Assignment 4
\date		01/04/2025
\brief
highscore will save the top 5 player name, score and play date/time.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
#pragma once
#include <string>
#include <array>

namespace HIGHSCORE
{
	struct Highscore
	{
		std::string name;
		int score;
		long long playDate;
	};
	extern std::array<Highscore, 5> highScores;

	//function will read from file and set highscores
	bool ReadFromHighscoreFile();
	//function will save highscores to the file
	bool WriteToHighscoreFile();
	//function is to be called when adding a new highscore, will sort
	void AddHighscore(int score, std::string name);
}
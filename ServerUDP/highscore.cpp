/*!
\file		highscore.cpp
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
#include "highscore.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip> //put_time
#include <sstream>
#include <algorithm> //sort

namespace HIGHSCORE
{
	std::array<Highscore, 5> highScores{};

	std::string fileName{ "highscore.txt" };

	bool ReadFromHighscoreFile()
	{
		std::ifstream file(fileName);
		if (!file) 
		{
			std::cerr << "READ FAILED: " << fileName << std::endl;
			return false;
		}

		//save highscore read from file
		for (Highscore& h : highScores)
		{
			std::string line{};
			std::getline(file, line);
			
			//name
			size_t size{ line.find('|') };
			h.name = line.substr(0, size);
			
			//score
			line = line.substr(size + 1, line.find('|', size + 1));
			size = line.find('|');
			h.score = std::stoi(line.substr(0, size));

			//play date
			line = line.substr(size + 1);
			h.playDate = std::stoull(line);
		}
		file.close();
		return true;
	}
	bool WriteToHighscoreFile()
	{
		std::ofstream file(fileName);
		if (!file) 
		{
			std::cerr << "WRITE FAILED: " << fileName << std::endl;
			return false;
		}
		for (Highscore& h : highScores)
		{
			file << h.name << '|' << h.score << '|' << h.playDate  <<'\n';
		}
		file.close();
		return true;
	}

	void AddHighscore(int score, std::string name)
	{
		//determine if score is high enough to be added
		if (score <= highScores.back().score) return;

		//get current date of this system
		auto now{ std::chrono::system_clock::now() };
		//auto n = std::chrono::time_point<std::chrono::system_clock>();
		std::time_t tt{ std::chrono::system_clock::to_time_t(now) };
		std::tm localTime{}; 
		if (localtime_s(&localTime, &tt) != 0)
		{
			std::cerr << "localtime_s FAILED" << std::endl;
			return; //error handling
		}

		//std::ostringstream date{};
		//date << std::put_time(&localTime, "%Y-%m-%d"); //example: "2025-04-01"
		//date << tt;

		//replaces last score and then sort
		highScores.back().name = name;
		highScores.back().score = score;
		highScores.back().playDate = tt;
		std::sort(highScores.begin(), highScores.end(),
			[](Highscore const& a, Highscore const& b) {
				return a.score > b.score;
			});
	}
}
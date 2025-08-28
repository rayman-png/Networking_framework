/*!
\file		Network.h
\author		Elton Leosantosa (leosantosa.e@digipen.edu)
\co-author
\par		Assignment 4
\date		01/04/2025
\brief
	Some simple functions that are needed

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#ifndef MATH_H
#define	MATH_H

template<typename T>
T Wrap(T const& val, T const& min, T const& max) {
	if (min > max) {
		return Wrap(val, max, min);
	}
	return val >= max ? Wrap(val - max, min, max) : (val < min ? Wrap(max - min + val, min, max) : val);
}
#endif // ! MATH_H
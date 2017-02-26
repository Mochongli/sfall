/*
 *    sfall
 *    Copyright (C) 2008, 2009  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Module.h"

class Stats : public Module {
	const char* name() { return "Stats"; }
	void init();
};

// TODO: move
void StatsReset();

void _stdcall SetPCStatMax(int stat, int i);
void _stdcall SetPCStatMin(int stat, int i);
void _stdcall SetNPCStatMax(int stat, int i);
void _stdcall SetNPCStatMin(int stat, int i);
extern unsigned short StandardApAcBonus;
extern unsigned short ExtraApAcBonus;
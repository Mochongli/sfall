/*
* sfall
* Copyright (C) 2008-2016 The sfall team
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ScriptValue.h"

namespace sfall
{
namespace script
{

ScriptValue::ScriptValue( SfallDataType type, unsigned long value )
{
	_val.dw = value;
	_type = type;
}

ScriptValue::ScriptValue() {
	_val.dw = 0;
	_type = DATATYPE_NONE;
}

ScriptValue::ScriptValue(const char* strval) {
	_val.str = strval;
	_type = DATATYPE_STR;
}

ScriptValue::ScriptValue(int val) {
	_val.i = val;
	_type = DATATYPE_INT;
}

ScriptValue::ScriptValue(float val) {
	_val.f = val;
	_type = DATATYPE_FLOAT;
}

ScriptValue::ScriptValue(bool val) {
	_val.i = val ? 1 : 0;
	_type = DATATYPE_INT;
}

ScriptValue::ScriptValue(TGameObj* obj) {
	_val.gObj = obj;
	_type = DATATYPE_INT;
}

bool ScriptValue::isInt() const
{
	return _type == DATATYPE_INT;
}

bool ScriptValue::isFloat() const {
	return _type == DATATYPE_FLOAT;
}

bool ScriptValue::isString() const {
	return _type == DATATYPE_STR;
}

unsigned long ScriptValue::rawValue() const {
	return _val.dw;
}

int ScriptValue::asInt() const {
	switch (_type) {
	case DATATYPE_FLOAT:
		return static_cast<int>(_val.f);
	case DATATYPE_INT:
		return _val.i;
	default:
		return 0;
	}
}

bool ScriptValue::asBool() const {
	switch (_type) {
	case DATATYPE_FLOAT:
		return static_cast<int>(_val.f) != 0;
	case DATATYPE_INT:
		return _val.i != 0;
	default:
		return true;
	}
}

float ScriptValue::asFloat() const {
	switch (_type) {
	case DATATYPE_FLOAT:
		return _val.f;
	case DATATYPE_INT:
		return static_cast<float>(_val.i);
	default:
		return 0.0;
	}
}

const char* ScriptValue::asString() const {
	return (_type == DATATYPE_STR)
		? _val.str
		: "";
}

TGameObj* ScriptValue::asObject() const {
	return (_type == DATATYPE_INT)
		? _val.gObj
		: nullptr;
}

SfallDataType ScriptValue::type() const {
	return _type;
}

}
}
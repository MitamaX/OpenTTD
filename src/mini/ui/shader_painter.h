/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shader_painter.h Painters an RCSS shader decorator names, drawing the element's box with their own GL program. */

#ifndef MINI_UI_SHADER_PAINTER_H
#define MINI_UI_SHADER_PAINTER_H

#include <string_view>

struct ShaderArea {
	float left;
	float top;
	float right;
	float bottom;
};

class ShaderPainter {
public:
	virtual ~ShaderPainter() = default;
	virtual void Paint(const ShaderArea &area) = 0;
	virtual void Release() = 0;
};

void RegisterShaderPainter(std::string_view name, ShaderPainter &painter);
ShaderPainter *FindShaderPainter(std::string_view name);
void ReleaseShaderPainters();

#endif /* MINI_UI_SHADER_PAINTER_H */

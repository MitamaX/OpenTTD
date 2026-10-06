/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shader_painter.cpp Painters an RCSS shader decorator names, drawing the element's box with their own GL program. */

#include "../../stdafx.h"
#include "shader_painter.h"

#include <map>
#include <string>

#include "../../safeguards.h"

using PainterMap = std::map<std::string, ShaderPainter *, std::less<>>;

static PainterMap &Painters()
{
	static PainterMap painters;
	return painters;
}

void RegisterShaderPainter(std::string_view name, ShaderPainter &painter)
{
	Painters().insert_or_assign(std::string(name), &painter);
}

ShaderPainter *FindShaderPainter(std::string_view name)
{
	auto it = Painters().find(name);
	return it == Painters().end() ? nullptr : it->second;
}

/* The GL context is going, and with it every program and texture a painter made. */
void ReleaseShaderPainters()
{
	for (const auto &[name, painter] : Painters()) painter->Release();
}

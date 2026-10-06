/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tunnel_models.h A tunnel's portal in 3D: a stone face with an arched mouth, set into the hillside over the cut its way runs into. */

#ifndef MINI_WORLD_TUNNEL_MODELS_H
#define MINI_WORLD_TUNNEL_MODELS_H

#include "../../tile_type.h"
#include "../model/model_mesh.h"

void LayTunnelPortal(ModelMesh &mesh, TileIndex entrance);

#endif /* MINI_WORLD_TUNNEL_MODELS_H */

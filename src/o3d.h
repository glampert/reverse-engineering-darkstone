
/* ================================================================================================
 * -*- C -*-
 * File: o3d.h
 * Created on: 27/08/15
 * Brief: Simple importer for DarkStone O3D models/meshes.
 *
 * Source code licensed under the MIT license.
 * Copyright (C) 2015 Guilherme R. Lampert
 *
 * This software is provided "as is" without express or implied
 * warranties. You may freely copy and compile this source into
 * applications you distribute provided that this copyright text
 * is included in the resulting source code.
 * ================================================================================================ */

#ifndef DARKSTONE_O3D_H
#define DARKSTONE_O3D_H

#include <stdbool.h>
#include <stdint.h>

/* ========================================================
 * DarkStone O3D data structures:
 * ======================================================== */

// Pragma pack is Visual Studio compatible and
// also supported by the most recent versions of
// Clang and GCC, so this should be fairly portable.
#pragma pack (push, 1)

/*
 * Model vertex position (XYZ).
 */
typedef struct o3d_vertex {
	float x, y, z;
} o3d_vertex_t;

/*
 * Model texture coordinates (UV).
 */
typedef struct o3d_texcoord {
	float u, v;
} o3d_texcoord_t;

/*
 * Model vertex color (BGRA-U8).
 */
typedef struct o3d_color {
	uint8_t b, g, r, a;
} o3d_color_t;

/*
 * Model face/polygon (triangle or quadrilateral).
 */
typedef struct o3d_face {
	// BGR(A) face color, it seems.
	// For models like the Knight and a couple weapons
	// I've tested, the color seems to match the expected,
	// but for some meshes we get some crazy/random values
	// that don't seem like what the object should look
	// like... Maybe in those cases the color is being used
	// as some kind of surface flag, and instead not applied
	// for rendering?...
	o3d_color_t color;

	// Texture coordinates for each vertex making up this face.
	// If it is a triangular face, then the last coordinate will
	// be (0,0). The coordinates are scaled by the size of the
	// texture map, which seems to be always 256, so multiply
	// each by 1.0/256.0 before sending them to the GL.
	o3d_texcoord_t texCoords[4];

	// Vertex indexing for a triangle/quadrilateral.
	// If the face is triangular, the last index is equal to UINT16_MAX
	// (or O3D_INVALID_FACE_INDEX for a more descriptive constant).
	uint16_t index[4];

	// Unknown value. Surface flags perhaps?
	// Seems to be 37 (0x25) on most models.
	// Also not sure if this is an uint32 or
	// a pair of uint16s...
	uint32_t unknown;

	// Texture/material index. This is the only thing identifying the
	// texture applied to this face. Each texture image starts with
	// either "Kxyzw_" or "Rxyzw_" then some name following the underscore.
	// The `xyzw` part will be this number. So for instance, texture for
	// the Knight model will be "K0015_KNIGHT.TGA" or "R0015_KNIGHT.TGA".
	// Only way to select the texture in an automated fashion is to build
	// a list of filenames and then search for one containing the texture number.
	uint16_t texNumber;
} o3d_face_t;

// face.index[3] will be set to this value for a triangle.
enum { O3D_INVALID_FACE_INDEX = UINT16_MAX };

/*
 * Axis Aligned Bounding Box computed from
 * the model vertexes. This is not read from
 * the file, but computed dynamically.
 */
typedef struct o3d_aabb {
	o3d_vertex_t mins;
	o3d_vertex_t maxs;
} o3d_aabb_t;

/*
 * A complete O3D model.
 */
typedef struct o3d_model {
	o3d_vertex_t * vertexes;
	o3d_face_t   * faces;
	uint32_t       vertexCount;
	uint32_t       faceCount;
	o3d_aabb_t     aabb;
	o3d_vertex_t   centerPoint;
} o3d_model_t;

#pragma pack (pop)

/* ========================================================
 * Model importer functions:
 * ======================================================== */

/*
 * Import from O3D model file.
 * You can still call o3d_free() even if this fails.
 * Any previous contents of `o3d` param are overwritten.
 *
 * If successful, this also computes the model's AABB and center of mass.
 */
bool o3d_load_from_file(o3d_model_t * o3d, const char * filename);

/*
 * Cleanup a model previously importer by o3d_load_from_file().
 */
void o3d_free(o3d_model_t * o3d);

/*
 * o3d_load_from_file() will set a global string with
 * an error description if something goes wrong. You can
 * recover the error description by calling this function
 * after a failure happens.
 *
 * Calling this function will clear the internal error string.
 */
const char * o3d_get_last_error(void);

#endif // DARKSTONE_O3D_H


/* ================================================================================================
 * -*- C -*-
 * File: o3d.c
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

#include "o3d.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================
 * o3d_get_last_error()/o3d_error():
 * ======================================================== */

// Some compilers have Thread Local Storage support
// which can be used to make this error string thread
// safe, so that parallel file processing wouldn't step
// in each other's toes when reporting errors...
static const char * o3dLastErrorStr = "";

static inline bool o3d_error(const char * message) {
	o3dLastErrorStr = (message != NULL) ? message : "";
	// Always returns false to allow using "return o3d_error(...);"
	return false;
}

const char * o3d_get_last_error(void) {
	const char * err = o3dLastErrorStr;
	o3dLastErrorStr = "";
	return err;
}

/* ========================================================
 * o3d_compute_aabb_center_pt() / AABB helpers:
 * ======================================================== */

static inline void o3d_clear_aabb_center_pt(o3d_model_t * o3d) {
	// A large float (could also be HUGE_VAL, but
	// this avoid the math.h dependency...)
	const float INF = 1e30f;

	o3d->aabb.mins.x =  INF;
	o3d->aabb.mins.y =  INF;
	o3d->aabb.mins.z =  INF;

	o3d->aabb.maxs.x = -INF;
	o3d->aabb.maxs.y = -INF;
	o3d->aabb.maxs.z = -INF;

	o3d->centerPoint.x = 0.0f;
	o3d->centerPoint.y = 0.0f;
	o3d->centerPoint.z = 0.0f;
}

static inline void o3d_min_per_element(o3d_vertex_t * res,
         const o3d_vertex_t * v0, const o3d_vertex_t * v1) {

	res->x = (v0->x < v1->x) ? v0->x : v1->x;
	res->y = (v0->y < v1->y) ? v0->y : v1->y;
	res->z = (v0->z < v1->z) ? v0->z : v1->z;
}

static inline void o3d_max_per_element(o3d_vertex_t * res,
         const o3d_vertex_t * v0, const o3d_vertex_t * v1) {

	res->x = (v0->x > v1->x) ? v0->x : v1->x;
	res->y = (v0->y > v1->y) ? v0->y : v1->y;
	res->z = (v0->z > v1->z) ? v0->z : v1->z;
}

static inline void o3d_compute_aabb_center_pt(o3d_model_t * o3d) {
	assert(o3d != NULL);
	assert(o3d->vertexes != NULL);
	assert(o3d->vertexCount != 0);

	o3d_clear_aabb_center_pt(o3d);
	o3d_aabb_t * aabb = &o3d->aabb;

	const o3d_vertex_t * verts = o3d->vertexes;
	const uint32_t vertCount   = o3d->vertexCount;

	o3d_vertex_t sum = { 0.0f, 0.0f, 0.0f };

	for (uint32_t v = 0; v < vertCount; ++v) {
		const o3d_vertex_t * xyz = &verts[v];

		o3d_min_per_element(&aabb->mins, xyz, &aabb->mins);
		o3d_max_per_element(&aabb->maxs, xyz, &aabb->maxs);

		sum.x += xyz->x;
		sum.y += xyz->y;
		sum.z += xyz->z;
	}

	o3d->centerPoint.x = sum.x / (float)vertCount;
	o3d->centerPoint.y = sum.y / (float)vertCount;
	o3d->centerPoint.z = sum.z / (float)vertCount;
}

/* ========================================================
 * o3d_read32():
 * ======================================================== */

static inline bool o3d_read32(FILE * fileIn, uint32_t * iword) {
	if (fread(iword, sizeof(*iword), 1, fileIn) != 1) {
		return o3d_error("o3d_read32() failed!");
	}
	return true;
}

/* ========================================================
 * o3d_load_from_file():
 * ======================================================== */

bool o3d_load_from_file(o3d_model_t * o3d, const char * filename) {
	assert(o3d != NULL);
	assert(filename != NULL && *filename != '\0');

	// Should be static asserts instead...
	assert(sizeof(o3d_face_t)   == 50);
	assert(sizeof(o3d_vertex_t) == 12);

	FILE * fileIn = fopen(filename, "rb");
	if (fileIn == NULL) {
		return o3d_error("Can't open input O3D file!");
	}

	uint32_t vertexCount = 0;
	uint32_t faceCount   = 0;
	uint32_t unknown     = 0;

	if (!o3d_read32(fileIn, &vertexCount)) {
		return o3d_error("Can't read vertex count!");
	}

	if (!o3d_read32(fileIn, &faceCount)) {
		return o3d_error("Can't read face count!");
	}

	// Two 32bit words of unknown contents follow.
	o3d_read32(fileIn, &unknown);
	o3d_read32(fileIn, &unknown);

	o3d->vertexes = malloc(vertexCount * sizeof(o3d->vertexes[0]));
	if (o3d->vertexes == NULL) {
		o3d_free(o3d);
		return o3d_error("Unable to malloc model vertexes!");
	}

	o3d->faces = malloc(faceCount * sizeof(o3d->faces[0]));
	if (o3d->faces == NULL) {
		o3d_free(o3d);
		return o3d_error("Unable to malloc model faces!");
	}

	o3d->vertexCount = vertexCount;
	o3d->faceCount   = faceCount;

	// Next up is the vertex packet:
	if (fread(o3d->vertexes, sizeof(o3d->vertexes[0]), vertexCount, fileIn) != vertexCount) {
		o3d_free(o3d);
		return o3d_error("Failed to read model vertexes!");
	}

	// Model faces follow immediately:
	if (fread(o3d->faces, sizeof(o3d->faces[0]), faceCount, fileIn) != faceCount) {
		o3d_free(o3d);
		return o3d_error("Failed to read model faces!");
	}

	// Axis-Aligned bounds and center point / center of mass:
	o3d_compute_aabb_center_pt(o3d);

	// That's it!
	fclose(fileIn);
	return true;
}

/* ========================================================
 * o3d_free():
 * ======================================================== */

void o3d_free(o3d_model_t * o3d) {
	if (o3d == NULL) {
		return;
	}

	if (o3d->vertexes != NULL) {
		free(o3d->vertexes);
		o3d->vertexes = NULL;
		o3d->vertexCount = 0;
	}

	if (o3d->faces != NULL) {
		free(o3d->faces);
		o3d->faces = NULL;
		o3d->faceCount = 0;
	}

	o3d_clear_aabb_center_pt(o3d);
}

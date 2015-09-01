
/* ================================================================================================
 * -*- C -*-
 * File: o3d_viewer.c
 * Created on: 27/08/15
 * Brief: OpenGL + GLFW viewer for Darkstone O3D static meshes.
 *
 * Source code licensed under the MIT license.
 * Copyright (C) 2015 Guilherme R. Lampert
 *
 * This software is provided "as is" without express or implied
 * warranties. You may freely copy and compile this source into
 * applications you distribute provided that this copyright text
 * is included in the resulting source code.
 * ================================================================================================ */

#include "gl_utils.h"
#include "o3d.h"

/* ========================================================
 * Application context data / helper constants:
 * ======================================================== */

// Fixed window size (NOT resizeable).
enum { WINDOW_WIDTH = 800, WINDOW_HEIGHT = 600 };

// Available render modes. Cycle through
// them by clicking the right mouse button.
enum {
	RENDER_TEXTURED      = 0,
	RENDER_WIREFRAME     = 1,
	RENDER_O3D_COLOR     = 2,
	RENDER_DEFAULT_COLOR = 3,
	RENDER_MODE_COUNT    = 4
};

// For printing in the window title.
static const char * renderModeStrings[RENDER_MODE_COUNT] = {
	"Textured",
	"Wireframe",
	"Model Color",
	"Default Color"
};

// O3D texture coordinates seem to be scaled by the size
// of the texture map (assuming all textures are 256^2 pixels).
static const float TEXCOORD_SCALE = (1.0f / 256.0f);

// Amount to move forward/back when zooming with the mouse wheel.
static const float ZOOM_AMOUNT = 0.1f;

/*
 * Application context:
 */
static struct {
	// Hose keeping app/window data:
	glfw_app_t    app;

	// Current loaded model and aux render data:
	const char  * modelFileName;
	const char  * textureFileName;
	o3d_model_t   o3d;
	o3d_vertex_t  centerPoint;
	o3d_vertex_t  vertexSum;
	float         modelScale;
	float         modelZ;
	float         degreesRotationZ;
	float         degreesRotationY;
	int           renderMode;

	// GL render data:
	gl_vbo_t      vbo;
	gl_texture_t  texture;
	gl_program_t  program;

	// Render matrices:
	VmathMatrix4  modelToWorldMatrix;
	VmathMatrix4  viewMatrix;
	VmathMatrix4  projMatrix;
	VmathMatrix4  vpMatrix;
	VmathMatrix4  mvpMatrix;
} viewer;

/*
 * Current mouse states:
 */
static const int MAX_MOUSE_DELTA = 100;
static struct {
	int  deltaX;
	int  deltaY;
	int  lastPosX;
	int  lastPosY;
	bool leftButtonDown;
	bool rightButtonDown;
} mouse;

/* ========================================================
 * Local functions:
 * ======================================================== */

static void refresh_window_title(void) {
	if (viewer.o3d.vertexes != NULL) {
		if (viewer.renderMode == RENDER_TEXTURED) {
			set_window_title(
				"Darkstone O3D Model Viewer -- %s -- %u verts, %u faces -- %s (%s)",
				viewer.modelFileName,
				viewer.o3d.vertexCount,
				viewer.o3d.faceCount,
				renderModeStrings[viewer.renderMode],
				viewer.textureFileName);
		} else {
			set_window_title(
				"Darkstone O3D Model Viewer -- %s -- %u verts, %u faces -- %s",
				viewer.modelFileName,
				viewer.o3d.vertexCount,
				viewer.o3d.faceCount,
				renderModeStrings[viewer.renderMode]);
		}
	} else {
		set_window_title("Darkstone O3D Model Viewer");
	}
}

static void set_gl_vert(gl_draw_vertex_t * glVert, const o3d_vertex_t * o3dVert,
                        const o3d_color_t * o3dColor, const o3d_texcoord_t * o3dTexCoords,
                        float nx, float ny, float nz) {

	// Scale to a more manageable size. Darkstone models used a big scale.
	glVert->px = o3dVert->x * viewer.modelScale;
	glVert->py = o3dVert->y * viewer.modelScale;
	glVert->pz = o3dVert->z * viewer.modelScale;

	viewer.vertexSum.x += glVert->px;
	viewer.vertexSum.y += glVert->py;
	viewer.vertexSum.z += glVert->pz;

	// Using the "barycentric coordinates" trick shown here:
	//   http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
	// To display an outline around the unshaded triangles.
	//
	glVert->nx = nx;
	glVert->ny = ny;
	glVert->nz = nz;

	// O3D stores it as BGR, it seems.
	glVert->r = (float)(o3dColor->r * (1.0f / 255.0f));
	glVert->g = (float)(o3dColor->g * (1.0f / 255.0f));
	glVert->b = (float)(o3dColor->b * (1.0f / 255.0f));

	// UVs stored scaled by the size in pixels of the texture map.
	glVert->u = o3dTexCoords->u * TEXCOORD_SCALE;
	glVert->v = o3dTexCoords->v * TEXCOORD_SCALE;
}

static void setup_model_vbo(void) {
	// Shortcut variables:
	const o3d_model_t  * o3d   = &viewer.o3d;
	const o3d_face_t   * faces = o3d->faces;
	const o3d_vertex_t * verts = o3d->vertexes;
	const uint32_t faceCount   = o3d->faceCount;
	const uint32_t vertCount   = o3d->vertexCount;

	// Triangular faces will require only 3 verts, but
	// a quadrilateral face will have to be split into
	// two triangles, so allocate for the worst case
	// where all faces are made of quads.
	const uint32_t vboSize = faceCount * 6;
	gl_draw_vertex_t * vboVerts = malloc(sizeof(vboVerts[0]) * vboSize);

	if (vboVerts == NULL) {
		fatal_error("Unable to malloc temp VBO data! Out-of-memory!");
	}

	viewer.vertexSum.x = 0.0f;
	viewer.vertexSum.y = 0.0f;
	viewer.vertexSum.z = 0.0f;

	uint32_t finalVertCount = 0;
	uint32_t i0, i1, i2, i3;

	for (uint32_t f = 0; f < faceCount; ++f) {
		const o3d_face_t * face = &faces[f];

		if (face->index[3] == O3D_INVALID_FACE_INDEX) {
			// Triangle face
			i0 = face->index[0];
			i1 = face->index[1];
			i2 = face->index[2];

			if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount) {
				printf("WARNING: Bad face indexing at #%u ( %u, %u, %u )!\n", f, i0, i1, i2);
				continue;
			}

			set_gl_vert(&vboVerts[finalVertCount++], &verts[i0], &face->color, &face->texCoords[0], 1.0f, 0.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i1], &face->color, &face->texCoords[1], 0.0f, 1.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i2], &face->color, &face->texCoords[2], 0.0f, 0.0f, 1.0f);
			assert(finalVertCount <= vboSize);
		} else {
			// Quadrilateral face (break into two tris)
			i0 = face->index[0];
			i1 = face->index[1];
			i2 = face->index[2];
			i3 = face->index[3];

			if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount || i3 >= vertCount) {
				printf("WARNING: Bad face indexing at #%u ( %u, %u, %u, %u )!\n", f, i0, i1, i2, i3);
				continue;
			}

			// First triangle: 0,1,3
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i0], &face->color, &face->texCoords[0], 1.0f, 0.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i1], &face->color, &face->texCoords[1], 0.0f, 1.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i3], &face->color, &face->texCoords[3], 0.0f, 0.0f, 1.0f);
			assert(finalVertCount <= vboSize);

			// First triangle: 3,1,2
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i3], &face->color, &face->texCoords[3], 1.0f, 0.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i1], &face->color, &face->texCoords[1], 0.0f, 1.0f, 0.0f);
			set_gl_vert(&vboVerts[finalVertCount++], &verts[i2], &face->color, &face->texCoords[2], 0.0f, 0.0f, 1.0f);
			assert(finalVertCount <= vboSize);
		}
	}

	// Translate back to the origin using the center of mass as reference:
	viewer.centerPoint.x = viewer.vertexSum.x / (float)finalVertCount;
	viewer.centerPoint.y = viewer.vertexSum.y / (float)finalVertCount;
	viewer.centerPoint.z = viewer.vertexSum.z / (float)finalVertCount;

	for (uint32_t v = 0; v < finalVertCount; ++v) {
		vboVerts[v].px -= viewer.centerPoint.x;
		vboVerts[v].py -= viewer.centerPoint.y;
		vboVerts[v].pz -= viewer.centerPoint.z;
	}

	viewer.vbo = create_gl_vbo(vboVerts, finalVertCount, NULL, 0);
	setup_gl_vertex_format();

	free(vboVerts);
}

static void import_model(void) {
	if (viewer.modelFileName == NULL || *viewer.modelFileName == '\0') {
		fatal_error("No valid filename provided!");
	}

	if (!o3d_load_from_file(&viewer.o3d, viewer.modelFileName)) {
		fatal_error("Failed to load O3D \"%s\": %s", viewer.modelFileName, o3d_get_last_error());
	}

	printf("Model imported successfully...\n");
	printf("AABB.mins  = ( %+f, %+f, %+f )\n",
			viewer.o3d.aabb.mins.x,
			viewer.o3d.aabb.mins.y,
			viewer.o3d.aabb.mins.z);
	printf("AABB.maxs  = ( %+f, %+f, %+f )\n",
			viewer.o3d.aabb.maxs.x,
			viewer.o3d.aabb.maxs.y,
			viewer.o3d.aabb.maxs.z);
	printf("OBJ.center = ( %+f, %+f, %+f )\n",
			viewer.o3d.centerPoint.x,
			viewer.o3d.centerPoint.y,
			viewer.o3d.centerPoint.z);

	// Get the distance between the min/max points:
	VmathVector3 vmin, vmax, dist;
	vmathV3MakeFromElems(&vmin, viewer.o3d.aabb.mins.x, viewer.o3d.aabb.mins.y, viewer.o3d.aabb.mins.z);
	vmathV3MakeFromElems(&vmax, viewer.o3d.aabb.maxs.x, viewer.o3d.aabb.maxs.y, viewer.o3d.aabb.maxs.z);
	vmathV3Sub(&dist, &vmin, &vmax);

	// Scale the model by the length of this distance.
	const float l = vmathV3Length(&dist);
	viewer.modelScale = (1.0f / l);
	printf("OBJ.scale  = %f\n", viewer.modelScale);

	// Projection matrix:
	vmathM4MakePerspective(&viewer.projMatrix, DEG_TO_RAD(60.0f),
		(float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 300.0f);

	// View matrix (looking down the -Z):
	const VmathPoint3  eyePos    = { 0.0f,  0.0f,  0.0f };
	const VmathPoint3  lookAtPos = { 0.0f,  0.0f, -1.0f };
	const VmathVector3 upVec     = { 0.0f,  1.0f,  0.0f };
	vmathM4MakeLookAt(&viewer.viewMatrix, &eyePos, &lookAtPos, &upVec);

	vmathM4MakeIdentity(&viewer.modelToWorldMatrix);
	vmathM4Mul(&viewer.vpMatrix, &viewer.projMatrix, &viewer.viewMatrix);

	printf("Setting up OpenGL Vertex Buffers...\n");
	setup_model_vbo();

	printf("VBO has %u vertexes.\n", viewer.vbo.vertCount);
	printf("New OBJ.center = ( %+f, %+f, %+f )\n",
			viewer.centerPoint.x,
			viewer.centerPoint.y,
			viewer.centerPoint.z);

	printf("Loading shaders...\n");
	viewer.program = load_gl_program("shaders/basic.vert", "shaders/basic.frag");

	if (viewer.program.progHandle == 0) {
		fatal_error("Failed to create the GL render program! Unable to proceed.");
	}

	// Use a default texture if none was provided.
	if (viewer.textureFileName == NULL) {
		viewer.textureFileName = "checkerboard.png";
	}
	viewer.texture = load_gl_texture_from_file(viewer.textureFileName);

	refresh_window_title();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	CHECK_GL_ERRORS();
	printf("---- Ready! ----\n");
}

/* ========================================================
 * Application callbacks:
 * ======================================================== */

static void initialize(void) {
	printf("---- O3D viewer starting up. Model file: \"%s\" ----\n", viewer.modelFileName);
	printf("GL_VENDOR:  %s\n", glGetString(GL_VENDOR));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	import_model();
}

static void shutdown(void) {
	printf("Exiting...\n");
	o3d_free(&viewer.o3d);
	free_gl_texture(&viewer.texture);
	free_gl_program(&viewer.program);
	free_gl_vbo(&viewer.vbo);
}

static void draw_frame(void) {
	if (mouse.leftButtonDown) {
		viewer.degreesRotationY += mouse.deltaX;
		viewer.degreesRotationZ += mouse.deltaY;
		mouse.deltaX = 0;
		mouse.deltaY = 0;
	}

	VmathMatrix4 matTranslation;
	VmathMatrix4 matRotation;

	vmathM4MakeIdentity(&viewer.modelToWorldMatrix);
	vmathM4MakeIdentity(&viewer.mvpMatrix);

	VmathVector3 vt = { 0.0f, 0.0f, viewer.modelZ };
	vmathM4MakeTranslation(&matTranslation, &vt);

	VmathVector3 rv = { DEG_TO_RAD(viewer.degreesRotationZ), DEG_TO_RAD(viewer.degreesRotationY), 0.0f };
	vmathM4MakeRotationZYX(&matRotation, &rv);

	vmathM4Mul(&viewer.modelToWorldMatrix, &matTranslation, &matRotation);
	vmathM4Mul(&viewer.mvpMatrix, &viewer.vpMatrix, &viewer.modelToWorldMatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, viewer.texture.texHandle);

	glBindVertexArray(viewer.vbo.vaHandle);
	glUseProgram(viewer.program.progHandle);

	glUniformMatrix4fv(viewer.program.u_mvpMatrix, 1, GL_FALSE, (const float *)&viewer.mvpMatrix);
	glUniform1i(viewer.program.u_renderModeFlag, viewer.renderMode);

	glDrawArrays((viewer.renderMode == RENDER_WIREFRAME) ? GL_LINE_STRIP : GL_TRIANGLES, 0, viewer.vbo.vertCount);
}

/* ========================================================
 * Input callbacks:
 * ======================================================== */

static void mouse_position_callback(GLFWwindow * window, double xpos, double ypos) {
	(void)window;

	int mx = (int)xpos;
	int my = (int)ypos;

	// Clamp to window bounds:
	if (mx > WINDOW_WIDTH)  { mx = WINDOW_WIDTH;  }
	else if (mx < 0)        { mx = 0; }
	if (my > WINDOW_HEIGHT) { my = WINDOW_HEIGHT; }
	else if (my < 0)        { my = 0; }

	mouse.deltaX   = mx - mouse.lastPosX;
	mouse.deltaY   = my - mouse.lastPosY;
	mouse.lastPosX = mx;
	mouse.lastPosY = my;

	// Clamp between -/+ max delta:
	if      (mouse.deltaX >  MAX_MOUSE_DELTA) { mouse.deltaX =  MAX_MOUSE_DELTA; }
	else if (mouse.deltaX < -MAX_MOUSE_DELTA) { mouse.deltaX = -MAX_MOUSE_DELTA; }
	if      (mouse.deltaY >  MAX_MOUSE_DELTA) { mouse.deltaY =  MAX_MOUSE_DELTA; }
	else if (mouse.deltaY < -MAX_MOUSE_DELTA) { mouse.deltaY = -MAX_MOUSE_DELTA; }
}

static void mouse_scroll_callback(GLFWwindow * window, double xoffset, double yoffset) {
	(void)window;
	(void)xoffset;

	if (yoffset < 0.0) {
		// Scroll forward
		viewer.modelZ -= ZOOM_AMOUNT;
	} else {
		// Scroll back
		viewer.modelZ += ZOOM_AMOUNT;
	}
}

static void mouse_button_callback(GLFWwindow * window, int button, int action, int mods) {
	(void)window;
	(void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			mouse.leftButtonDown = true;
		} else {
			mouse.leftButtonDown = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS && !mouse.rightButtonDown) {
			mouse.rightButtonDown = true;
			viewer.renderMode = (viewer.renderMode + 1) % RENDER_MODE_COUNT;
			refresh_window_title();
		} else {
			mouse.rightButtonDown = false;
		}
	}
}

/* ========================================================
 * main():
 * ======================================================== */

int main(int argc, const char * argv[]) {
	if (argc < 2) {
		printf(
			"Not enough arguments! Specify a file to view.\n"
			" Usage:\n"
			" $ %s <o3d_file> [texture_filename]\n\n",
		argv[0]);
		return EXIT_FAILURE;
	}

	// The O3D file:
	viewer.modelFileName = argv[1];

	// Optionally, a texture to apply:
	if (argc >= 3) {
		viewer.textureFileName = argv[2];
	}

	// Set a couple defaults...
	viewer.renderMode = RENDER_DEFAULT_COLOR;
	viewer.modelZ     = -1.0f;

	viewer.app.windowWidth         = WINDOW_WIDTH;
	viewer.app.windowHeight        = WINDOW_HEIGHT;
	viewer.app.windowTitle         = "Darkstone O3D Model Viewer";
	viewer.app.clearScrColor[0]    = 0.7f;
	viewer.app.clearScrColor[1]    = 0.7f;
	viewer.app.clearScrColor[2]    = 0.7f;
	viewer.app.clearScrColor[3]    = 1.0f;
	viewer.app.onInitCallback      = &initialize;
	viewer.app.onShutdownCallback  = &shutdown;
	viewer.app.onDrawCallback      = &draw_frame;
	viewer.app.mouseButtonCallback = &mouse_button_callback;
	viewer.app.mousePosCallback    = &mouse_position_callback;
	viewer.app.mouseScrollCallback = &mouse_scroll_callback;
	viewer.app.useCustomCursor     = true;

	init_glfw_app(&viewer.app);
}

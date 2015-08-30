
/* ================================================================================================
 * -*- C -*-
 * File: gl_utils.c
 * Created on: 29/08/15
 * Brief: Miscellaneous OpenGL helpers and a tiny GL/GLFW application framework.
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

/* ========================================================
 * Local application context data:
 * ======================================================== */

static char g_windowTitle[1024];
static char g_glslVersionDirective[64];

static GLFWwindow * g_window = NULL;
static GLFWcursor * g_cursor = NULL;
static app_callback_f g_userCleanup = NULL;

/* ========================================================
 * GL error checking / error handling:
 * ======================================================== */

static const char * gl_error_str(GLenum errorCode) {
	switch (errorCode) {
	case GL_NO_ERROR          : return "GL_NO_ERROR";
	case GL_INVALID_ENUM      : return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE     : return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION : return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY     : return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW   : return "GL_STACK_UNDERFLOW"; // Legacy; not used on GL3+
	case GL_STACK_OVERFLOW    : return "GL_STACK_OVERFLOW";  // Legacy; not used on GL3+
	default                   : return "Unknown GL error";
	} // switch (errorCode)
}

void check_gl_errors_helper(const char * function, const char * filename, int lineNum) {
	GLenum errorCode = glGetError();
	while (errorCode != GL_NO_ERROR) {
		printf("OpenGL error %X ( %s ) in %s(), file %s(%d).\n",
			errorCode, gl_error_str(errorCode), function, filename, lineNum);
		errorCode = glGetError();
	}
}

void fatal_error(const char * message, ...) {
	va_list args;

	fprintf(stderr, "[ERROR]: ");
	va_start(args, message);
		vfprintf(stderr, message, args);
	va_end(args);
	fprintf(stderr, "\n");

	quit_glfw_app();
}

/* ========================================================
 * GL shader program helpers:
 * ======================================================== */

static void check_shader_info_logs(GLuint glProgHandle, GLuint glVsHandle, GLuint glFsHandle) {
	enum { INFO_LOG_MAX_CHARS = 2048 };

	GLsizei charsWritten;
	GLchar infoLogBuf[INFO_LOG_MAX_CHARS];

	charsWritten = 0;
	memset(infoLogBuf, 0, sizeof(infoLogBuf));
	glGetProgramInfoLog(glProgHandle, INFO_LOG_MAX_CHARS - 1, &charsWritten, infoLogBuf);
	if (charsWritten > 0) {
		printf("------ GL PROGRAM INFO LOG ----------\n");
		printf("%s\n", infoLogBuf);
	}

	charsWritten = 0;
	memset(infoLogBuf, 0, sizeof(infoLogBuf));
	glGetShaderInfoLog(glVsHandle, INFO_LOG_MAX_CHARS - 1, &charsWritten, infoLogBuf);
	if (charsWritten > 0) {
		printf("------ GL VERT SHADER INFO LOG ------\n");
		printf("%s\n", infoLogBuf);
	}

	charsWritten = 0;
	memset(infoLogBuf, 0, sizeof(infoLogBuf));
	glGetShaderInfoLog(glFsHandle, INFO_LOG_MAX_CHARS - 1, &charsWritten, infoLogBuf);
	if (charsWritten > 0) {
		printf("------ GL FRAG SHADER INFO LOG ------\n");
		printf("%s\n", infoLogBuf);
	}

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(glProgHandle, GL_LINK_STATUS, &linkStatus);
	if (linkStatus == GL_FALSE) {
		printf("Failed to link GL program!");
	}
}

static char * load_shader_file(const char * filename) {
	FILE * fileIn = fopen(filename, "rb");
	if (fileIn == NULL) {
		fatal_error("Can't open shader file \"%s\"!", filename);
	}

	fseek(fileIn, 0, SEEK_END);
	const long fileLength = ftell(fileIn);
	fseek(fileIn, 0, SEEK_SET);

	if (fileLength <= 0 || ferror(fileIn)) {
		fclose(fileIn);
		fatal_error("Error getting length or empty shader file! \"%s\".", filename);
	}

	char * fileContents = malloc(fileLength + 1);
	if (fileContents == NULL) {
		fclose(fileIn);
		fatal_error("Failed to malloc shader file contents!");
	}

	if (fread(fileContents, sizeof(char), fileLength, fileIn) != (size_t)fileLength) {
		fclose(fileIn); free(fileContents);
		fatal_error("Failed to read whole shader file! \"%s\".", filename);
	}

	fclose(fileIn);

	fileContents[fileLength] = '\0';
	return fileContents;
}

gl_program_t load_gl_program(const char * vsFile, const char * fsFile) {
	assert(vsFile  != NULL &&  fsFile != NULL);
	assert(*vsFile != '\0' && *fsFile != '\0');

	// Query once and store for the subsequent shader allocations.
	// This ensures we use the best version available.
	if (g_glslVersionDirective[0] == '\0') {
		int slMajor    = 0;
		int slMinor    = 0;
		int versionNum = 0;
		const char * versionStr = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

		if (sscanf(versionStr, "%d.%d", &slMajor, &slMinor) == 2) {
			versionNum = (slMajor * 100) + slMinor;
		} else {
			// Fall back to the lowest acceptable version.
			// Assume #version 150 - OpenGL 3.2
			versionNum = 150;
		}

		snprintf(g_glslVersionDirective, sizeof(g_glslVersionDirective), "#version %d\n", versionNum);
	}

	char * vsSrc = load_shader_file(vsFile);
	char * fsSrc = load_shader_file(fsFile);

	const GLuint glProgHandle = glCreateProgram();
	if (glProgHandle == 0) {
		fatal_error("Failed to allocate a new GL program handle! Possibly out-of-memory!");
	}

	const GLuint glVsHandle = glCreateShader(GL_VERTEX_SHADER);
	if (glVsHandle == 0) {
		fatal_error("Failed to allocate a new GL shader handle! Possibly out-of-memory!");
	}

	const GLuint glFsHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (glFsHandle == 0) {
		fatal_error("Failed to allocate a new GL shader handle! Possibly out-of-memory!");
	}

	// Vertex shader:
	const char * vsSrcStrings[] = { g_glslVersionDirective, vsSrc };
	glShaderSource(glVsHandle, 2, vsSrcStrings, NULL);
	glCompileShader(glVsHandle);
	glAttachShader(glProgHandle, glVsHandle);

	// Fragment shader:
	const char * fsSrcStrings[] = { g_glslVersionDirective, fsSrc };
	glShaderSource(glFsHandle, 2, fsSrcStrings, NULL);
	glCompileShader(glFsHandle);
	glAttachShader(glProgHandle, glFsHandle);

	// Link the Shader Program then check and print the info logs, if any.
	glLinkProgram(glProgHandle);
	check_shader_info_logs(glProgHandle, glVsHandle, glFsHandle);

	// After a program is linked the shader objects can be safely detached and deleted.
	// This is also recommended to save some memory that would be wasted by keeping the shaders alive.
	glDetachShader(glProgHandle, glVsHandle);
	glDetachShader(glProgHandle, glFsHandle);
	glDeleteShader(glVsHandle);
	glDeleteShader(glFsHandle);

	free(fsSrc);
	free(vsSrc);

	// Store the program uniforms. For simplicity,
	// we assume all programs have the same set of variables.
	gl_program_t prog;
	prog.progHandle       = glProgHandle;
	prog.u_mvpMatrix      = glGetUniformLocation(glProgHandle, "u_mvp_matrix");
	prog.u_renderModeFlag = glGetUniformLocation(glProgHandle, "u_render_mode_flag");

	CHECK_GL_ERRORS();
	return prog;
}

void free_gl_program(gl_program_t * prog) {
	if (prog == NULL) {
		return;
	}

	glUseProgram(0);
	glDeleteProgram(prog->progHandle);

	memset(prog, 0, sizeof(*prog));
}

/* ========================================================
 * GL Vertex Buffer helpers:
 * ======================================================== */

gl_vbo_t create_gl_vbo(const void * vertexData, int vertCount, const void * indexData, int indexCount) {
	// 'indexData/indexCount' are optional.
	assert(vertexData != NULL);
	assert(vertCount  > 0);

	gl_vbo_t vbo = {
		.vertCount  = vertCount,
		.indexCount = indexCount,
		.vaHandle   = 0,
		.vbHandle   = 0,
		.ibHandle   = 0
	};

	glGenVertexArrays(1, &vbo.vaHandle);
	glBindVertexArray(vbo.vaHandle);

	glGenBuffers(1, &vbo.vbHandle);
	glBindBuffer(GL_ARRAY_BUFFER, vbo.vbHandle);
	glBufferData(GL_ARRAY_BUFFER, vertCount * sizeof(gl_draw_vertex_t), vertexData, GL_STATIC_DRAW);

	if (indexData != NULL && indexCount > 0) {
		glGenBuffers(1, &vbo.ibHandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.ibHandle);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint16_t), indexData, GL_STATIC_DRAW);
	}

	CHECK_GL_ERRORS();
	return vbo;
}

void setup_gl_vertex_format(void) {
	// Hard-coded for gl_draw_vertex_t.
	size_t offset = 0;

	// Position:
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		/* index     = */ 0,
		/* size      = */ 3,
		/* type      = */ GL_FLOAT,
		/* normalize = */ GL_FALSE,
		/* stride    = */ sizeof(gl_draw_vertex_t),
		/* offset    = */ (void *)offset);
	offset += sizeof(float) * 3;

	// Normal:
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		/* index     = */ 1,
		/* size      = */ 3,
		/* type      = */ GL_FLOAT,
		/* normalize = */ GL_FALSE,
		/* stride    = */ sizeof(gl_draw_vertex_t),
		/* offset    = */ (void *)offset);
	offset += sizeof(float) * 3;

	// Color:
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		/* index     = */ 2,
		/* size      = */ 3,
		/* type      = */ GL_FLOAT,
		/* normalize = */ GL_FALSE,
		/* stride    = */ sizeof(gl_draw_vertex_t),
		/* offset    = */ (void *)offset);
	offset += sizeof(float) * 3;

	// UV:
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(
		/* index     = */ 3,
		/* size      = */ 2,
		/* type      = */ GL_FLOAT,
		/* normalize = */ GL_FALSE,
		/* stride    = */ sizeof(gl_draw_vertex_t),
		/* offset    = */ (void *)offset);
	offset += sizeof(float) * 2;

	CHECK_GL_ERRORS();
}

void free_gl_vbo(gl_vbo_t * vbo) {
	if (vbo == NULL) {
		return;
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDeleteVertexArrays(1, &vbo->vaHandle);
	glDeleteBuffers(1, &vbo->vbHandle);
	glDeleteBuffers(1, &vbo->ibHandle);

	memset(vbo, 0, sizeof(*vbo));
}

/* ========================================================
 * GL texture loading from image file via STB Image:
 * ======================================================== */

#define STB_IMAGE_IMPLEMENTATION 1
#define STB_IMAGE_STATIC         1
#define STBI_NO_GIF              1
#define STBI_NO_HDR              1
#define STBI_NO_PIC              1
#define STBI_NO_PNM              1
#define STBI_NO_PSD              1
#define STBI_NO_LINEAR           1
#include <stb_image.h>

gl_texture_t load_gl_texture_from_file(const char * filename) {
	assert(filename  != NULL);
	assert(*filename != '\0');

	gl_texture_t tex = { 0, 0, 0 };
	int width, height, comps;

	stbi_uc * data = stbi_load(filename, &width, &height, &comps, /* require RGBA */ 4);
	if (data == NULL) {
		printf("WARNING: Unable to load texture image \"%s\"!\n", filename);
		return tex;
	}

	GLuint glTexHandle = 0;
	glGenTextures(1, &glTexHandle);

	if (glTexHandle == 0) {
		stbi_image_free(data);
		fatal_error("Failed to allocate a new GL texture handle! Possibly out-of-memory!");
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, glTexHandle);

	glTexImage2D(
		/* target   = */ GL_TEXTURE_2D,
		/* level    = */ 0,
		/* internal = */ GL_RGBA,
		/* width    = */ width,
		/* height   = */ height,
		/* border   = */ 0,
		/* format   = */ GL_RGBA,
		/* type     = */ GL_UNSIGNED_BYTE,
		/* data     = */ data);

	if (glGenerateMipmap != NULL) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	// Best filtering available, without resorting to anisotropic, which requires extensions.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	stbi_image_free(data);
	CHECK_GL_ERRORS();

	printf("Loaded new texture from file \"%s\".\n", filename);

	tex.texHandle = glTexHandle;
	tex.width     = width;
	tex.height    = height;
	return tex;
}

void free_gl_texture(gl_texture_t * tex) {
	if (tex == NULL) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &tex->texHandle);

	memset(tex, 0, sizeof(*tex));
}

/* ========================================================
 * Miscellaneous / Application management:
 * ======================================================== */

void set_custom_cursor(void) {
	if (g_window == NULL) {
		printf("WARNING: Null window! Can't set custom cursor!\n");
		return;
	}

	static const char * CURSOR_IMG_FILE = "cursor24.png";

	int width, height, comps;
	stbi_uc * data = stbi_load(CURSOR_IMG_FILE, &width, &height, &comps, /* require RGBA */ 4);
	if (data == NULL) {
		printf("WARNING: Unable to load texture image \"%s\"!\n", CURSOR_IMG_FILE);
		return;
	}

	GLFWimage image;
	image.width  = width;
	image.height = height;
	image.pixels = data;
	g_cursor = glfwCreateCursor(&image, 0, 0);

	if (g_cursor != NULL) {
		glfwSetCursor(g_window, g_cursor);
	}
}

void restore_default_cursor(void) {
	if (g_window != NULL) {
		glfwSetCursor(g_window, NULL);
	}

	if (g_cursor != NULL) {
		glfwDestroyCursor(g_cursor);
		g_cursor = NULL;
	}
}

void set_window_title(const char * fmt, ...) {
	if (g_window == NULL) {
		printf("WARNING: Null window! Can't set window title!\n");
		return;
	}

	va_list args;
	va_start(args, fmt);
		vsnprintf(g_windowTitle, sizeof(g_windowTitle), fmt, args);
	va_end(args);

	g_windowTitle[sizeof(g_windowTitle) - 1] = '\0'; // Ensure it ends somewhere.
	glfwSetWindowTitle(g_window, g_windowTitle);
}

void init_glfw_app(glfw_app_t * app) {
	assert(app != NULL);

	if (app->windowWidth <= 0 || app->windowHeight <= 0) {
		fatal_error("Bad window dimensions!");
	}

	if (!glfwInit()) {
		fatal_error("glfwInit() failed!");
	}

	glfwWindowHint(GLFW_RESIZABLE,    false);
	glfwWindowHint(GLFW_DOUBLEBUFFER, true);
	glfwWindowHint(GLFW_DEPTH_BITS,   32);

	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

	if (app->windowTitle != NULL) {
		strncpy(g_windowTitle, app->windowTitle, sizeof(g_windowTitle));
	} else {
		strncpy(g_windowTitle, "OpenGL Window", sizeof(g_windowTitle));
	}

	g_window = glfwCreateWindow(app->windowWidth, app->windowHeight, g_windowTitle, NULL, NULL);
	if (g_window == NULL) {
		fatal_error("Unable to create GLFW window!");
	}

	// GLFW input callbacks:
	glfwSetCursorPosCallback(g_window,   app->mousePosCallback);
	glfwSetMouseButtonCallback(g_window, app->mouseButtonCallback);
	glfwSetScrollCallback(g_window,      app->mouseScrollCallback);

	// Make the drawing context (OpenGL) current for this thread:
	glfwMakeContextCurrent(g_window);

	if (!gl3wInit()) {
		fatal_error("gl3wInit() failed!");
	}

	if (app->useCustomCursor) {
		set_custom_cursor();
	}

	glClearColor(app->clearScrColor[0],
	             app->clearScrColor[1],
	             app->clearScrColor[2],
	             app->clearScrColor[3]);

	// Store so we can still call it if we get a fatal_error().
	g_userCleanup = app->onShutdownCallback;

	// User initializations run last.
	app->onInitCallback();

	// Enter the main loop, only breaking it when the user closes the window.
	while (!glfwWindowShouldClose(g_window)) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		app->onDrawCallback();

		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}

	quit_glfw_app();
}

void quit_glfw_app(void) {
	restore_default_cursor();

	if (g_userCleanup != NULL) {
		g_userCleanup();
	}

	gl3wShutdown();

	if (g_window != NULL) {
		glfwDestroyWindow(g_window);
		g_window = NULL;
	}

	glfwTerminate();
	exit(0);
}

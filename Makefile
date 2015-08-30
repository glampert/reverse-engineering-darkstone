
#############################
# Flags/variables:
#############################

# Select the proper OpenGL library for Mac (-framework OpenGL)
# or use a default (-lGL) that should work on most Unix-like systems.
UNAME = $(shell uname)
ifeq ($(UNAME), Darwin)
  OPENGL_LIB = -framework CoreFoundation -framework OpenGL
else
  OPENGL_LIB = -lGL
endif

# The command-line unpacker:
MTF_UNPACKER     = mtf_unpacker
MTF_UNPACKER_SRC = src/mtf.c src/mtf_unpacker.c
MTF_UNPACKER_OBJ = $(patsubst %.c, %.o, $(MTF_UNPACKER_SRC))

# The OpenGL viewer for O3D models (requires GLFW v3):
O3D_VIEWER       = o3d_viewer
O3D_VIEWER_SRC   = src/o3d.c src/o3d_viewer.c src/gl_utils.c src/thirdparty/gl3w/src/gl3w.c
O3D_VIEWER_OBJ   = $(patsubst %.c, %.o, $(O3D_VIEWER_SRC))
O3D_VIEWER_LIBS  = $(OPENGL_LIB) -lGLFW3

# Misc compiler flags:
CFLAGS = -Isrc/ -Isrc/thirdparty/stb/ -Isrc/thirdparty/gl3w/include/ -Isrc/thirdparty/vectormath/ \
         -Wall -Wextra -Wformat=2 \
         -Wuninitialized -Winit-self \
         -Wunused -Wshadow -Wstrict-aliasing \
         -pedantic

#############################
# Rules:
#############################

all:
	$(error "Try 'make unpacker' or 'make viewer'!")

unpacker: $(MTF_UNPACKER_OBJ)
	$(CC) -o $(MTF_UNPACKER) $(MTF_UNPACKER_OBJ)

viewer: $(O3D_VIEWER_OBJ)
	$(CC) $(O3D_VIEWER_LIBS) -o $(O3D_VIEWER) $(O3D_VIEWER_OBJ)

clean:
	rm -f $(MTF_UNPACKER) $(MTF_UNPACKER_OBJ)
	rm -f $(O3D_VIEWER)   $(O3D_VIEWER_OBJ)

$(MTF_UNPACKER): $(MTF_UNPACKER_OBJ)
	$(CC) -o $* $(MTF_UNPACKER_OBJ)

$(O3D_VIEWER): $(O3D_VIEWER_OBJ)
	$(CC) $(O3D_VIEWER_LIBS) -o $* $(O3D_VIEWER_OBJ)


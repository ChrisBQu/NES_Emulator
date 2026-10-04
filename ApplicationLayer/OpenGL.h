#ifndef OPENGL_H
#define OPENGL_H

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdbool.h>

// Windows only provides OpenGL 1.1 directly. Everything newer (shaders, vertex arrays) has to be looked up at runtime
// once a context exists. Each function here gets a pointer named loaded_<name>, and a #define below so code can call
// it by its normal name
#define OPENGL_LOADED_FUNCTIONS(X) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM4FPROC, glUniform4f) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)

#define OPENGL_DECLARE_FUNCTION(type, name) extern type loaded_##name;
OPENGL_LOADED_FUNCTIONS(OPENGL_DECLARE_FUNCTION)
#undef OPENGL_DECLARE_FUNCTION

#define glActiveTexture loaded_glActiveTexture
#define glCreateShader loaded_glCreateShader
#define glShaderSource loaded_glShaderSource
#define glCompileShader loaded_glCompileShader
#define glGetShaderiv loaded_glGetShaderiv
#define glGetShaderInfoLog loaded_glGetShaderInfoLog
#define glDeleteShader loaded_glDeleteShader
#define glCreateProgram loaded_glCreateProgram
#define glAttachShader loaded_glAttachShader
#define glBindAttribLocation loaded_glBindAttribLocation
#define glLinkProgram loaded_glLinkProgram
#define glGetProgramiv loaded_glGetProgramiv
#define glGetProgramInfoLog loaded_glGetProgramInfoLog
#define glDeleteProgram loaded_glDeleteProgram
#define glUseProgram loaded_glUseProgram
#define glGetUniformLocation loaded_glGetUniformLocation
#define glUniform1i loaded_glUniform1i
#define glUniform1f loaded_glUniform1f
#define glUniform2f loaded_glUniform2f
#define glUniform4f loaded_glUniform4f
#define glGenVertexArrays loaded_glGenVertexArrays
#define glBindVertexArray loaded_glBindVertexArray
#define glDeleteVertexArrays loaded_glDeleteVertexArrays
#define glGenBuffers loaded_glGenBuffers
#define glBindBuffer loaded_glBindBuffer
#define glBufferData loaded_glBufferData
#define glDeleteBuffers loaded_glDeleteBuffers
#define glVertexAttribPointer loaded_glVertexAttribPointer
#define glEnableVertexAttribArray loaded_glEnableVertexAttribArray

// Every shader gets its vertices through these inputs: a_position in clip space (-1 to 1), and a_texCoord (0 to 1,
// with 0 at the top of the image)
#define OPENGL_POSITION_ATTRIBUTE 0
#define OPENGL_TEXCOORD_ATTRIBUTE 1

// Call once after creating the OpenGL context. Returns false if the graphics driver is missing something we need
bool initOpenGL(void);
void shutdownOpenGL(void);

// Draw a quad covering the whole current viewport. Set glViewport to choose where it goes
void drawOpenGLQuad(void);

// Compile and link a shader program. name is only used in error messages. Returns 0 if it failed, after printing why
GLuint createOpenGLProgram(const char* vertexSource, const char* fragmentSource, const char* name);

// Create a texture from ARGB8888 pixels (0xAARRGGBB, the format the framebuffer and SDL_ttf use).
// pitch is the length of a row in bytes. pixels can be NULL to fill the texture later
GLuint createOpenGLTexture(int width, int height, int pitch, const void* pixels, GLint filter);

#endif
#include "OpenGL.h"
#include <stdio.h>
#include <stdlib.h>

#define OPENGL_DEFINE_FUNCTION(type, name) type loaded_##name = NULL;
OPENGL_LOADED_FUNCTIONS(OPENGL_DEFINE_FUNCTION)
#undef OPENGL_DEFINE_FUNCTION

// One quad, shared by everything that draws. Each vertex is x, y in clip space, then u, v.
// v is 0 at the top, so images uploaded top row first come out the right way up
static const GLfloat quadVertices[] = {
    -1.0f, -1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 1.0f,
    -1.0f,  1.0f,   0.0f, 0.0f,
     1.0f,  1.0f,   1.0f, 0.0f,
};
static GLuint quadVertexArray = 0;
static GLuint quadVertexBuffer = 0;

bool initOpenGL(void) {
    bool allLoaded = true;
#define OPENGL_LOAD_FUNCTION(type, name) \
    loaded_##name = (type)SDL_GL_GetProcAddress(#name); \
    if (loaded_##name == NULL) { printf("Error: The graphics driver doesn't provide %s.\n", #name); allLoaded = false; }
    OPENGL_LOADED_FUNCTIONS(OPENGL_LOAD_FUNCTION)
#undef OPENGL_LOAD_FUNCTION
    if (!allLoaded) { return false; }

    glGenVertexArrays(1, &quadVertexArray);
    glBindVertexArray(quadVertexArray);
    glGenBuffers(1, &quadVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, quadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(OPENGL_POSITION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(OPENGL_POSITION_ATTRIBUTE);
    glVertexAttribPointer(OPENGL_TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(OPENGL_TEXCOORD_ATTRIBUTE);
    return true;
}

void shutdownOpenGL(void) {
    if (quadVertexBuffer != 0) { glDeleteBuffers(1, &quadVertexBuffer); }
    if (quadVertexArray != 0) { glDeleteVertexArrays(1, &quadVertexArray); }
    quadVertexBuffer = 0;
    quadVertexArray = 0;
}

void drawOpenGLQuad(void) {
    glBindVertexArray(quadVertexArray);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Helper function: compile one shader stage. Returns 0 if it failed, after printing the compiler's errors
static GLuint compileShader(GLenum type, const char* source, const char* name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        printf("Error: The %s %s shader didn't compile:\n%s\n", name, (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createOpenGLProgram(const char* vertexSource, const char* fragmentSource, const char* name) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, name);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, name);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) { glDeleteShader(vertexShader); }
        if (fragmentShader != 0) { glDeleteShader(fragmentShader); }
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    // Shaders don't have to give their inputs a location themselves, the names are enough
    glBindAttribLocation(program, OPENGL_POSITION_ATTRIBUTE, "a_position");
    glBindAttribLocation(program, OPENGL_TEXCOORD_ATTRIBUTE, "a_texCoord");
    glLinkProgram(program);

    // The shaders stay alive as long as the program uses them, so they can be let go now
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        printf("Error: The %s shader program didn't link:\n%s\n", name, log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint createOpenGLTexture(int width, int height, int pitch, const void* pixels, GLint filter) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // With this format and type, OpenGL reads each pixel as one 32-bit number with blue in the lowest byte, matching 0xAARRGGBB
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return texture;
}
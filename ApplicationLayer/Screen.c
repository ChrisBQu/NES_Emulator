#include "Screen.h"
#include "Window.h"
#include "OpenGL.h"
#include <math.h>

#define SHADER_PATH_LENGTH 1024

// Used when the shader files can't be loaded: shows the picture unchanged
static const char* builtInVertexShader =
    "#version 330 core\n"
    "in vec2 a_position;\n"
    "in vec2 a_texCoord;\n"
    "out vec2 v_texCoord;\n"
    "void main() {\n"
    "    v_texCoord = a_texCoord;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";
static const char* builtInFragmentShader =
    "#version 330 core\n"
    "in vec2 v_texCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    fragColor = texture(u_texture, v_texCoord);\n"
    "}\n";

// Helper function: build a screen shader from the shared vertex shader and a fragment shader file in the Shaders folder,
// falling back to the built-in one. name is only used in messages. Returns false if even the built-in one failed
static bool loadScreenShader(struct ScreenShaderProgram* shader, const char* fragmentFile, const char* name) {
    char vertexPath[SHADER_PATH_LENGTH];
    char fragmentPath[SHADER_PATH_LENGTH];
    char* basePath = SDL_GetBasePath();
    SDL_snprintf(vertexPath, sizeof(vertexPath), "%sShaders/%s", basePath != NULL ? basePath : "", SCREEN_VERTEX_SHADER_FILE);
    SDL_snprintf(fragmentPath, sizeof(fragmentPath), "%sShaders/%s", basePath != NULL ? basePath : "", fragmentFile);
    SDL_free(basePath);

    // SDL_LoadFile adds a terminating zero, so the contents can be used as strings
    char* vertexSource = (char*)SDL_LoadFile(vertexPath, NULL);
    char* fragmentSource = (char*)SDL_LoadFile(fragmentPath, NULL);
    GLuint program = 0;
    if (vertexSource != NULL && fragmentSource != NULL) {
        program = createOpenGLProgram(vertexSource, fragmentSource, name);
        if (program == 0) { printf("Warning: Using the built-in screen shader instead of the %s shader.\n", name); }
    }
    else {
        printf("Warning: Could not read the %s shader files (%s, %s). Using the built-in screen shader.\n", name, vertexPath, fragmentPath);
    }
    SDL_free(vertexSource);
    SDL_free(fragmentSource);

    if (program == 0) { program = createOpenGLProgram(builtInVertexShader, builtInFragmentShader, "built-in screen"); }
    if (program == 0) { return false; }

    // Uniforms the shader doesn't use have a location of -1, and setting those does nothing
    shader->program = program;
    shader->uniformTexture = glGetUniformLocation(program, "u_texture");
    shader->uniformSourceSize = glGetUniformLocation(program, "u_sourceSize");
    shader->uniformOutputSize = glGetUniformLocation(program, "u_outputSize");
    shader->uniformTime = glGetUniformLocation(program, "u_time");
    shader->uniformHardScan = glGetUniformLocation(program, "hardScan");
    shader->uniformHardPix = glGetUniformLocation(program, "hardPix");
    shader->uniformWarp = glGetUniformLocation(program, "warp");
    shader->uniformMaskDark = glGetUniformLocation(program, "maskDark");
    shader->uniformMaskLight = glGetUniformLocation(program, "maskLight");
    return true;
}

// Constructor
struct Screen* createScreen(int width, int height) {
    struct Screen* screen = (struct Screen*)calloc(1, sizeof(struct Screen));
    if (screen == NULL) { return NULL; }
    screen->width = width;
    screen->height = height;
    screen->framebuffer = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (screen->framebuffer == NULL) {
        printf("Error: Could not allocate frame buffer. Out off memory?\n");
        free(screen);
        return NULL;
    }

    // Create the OpenGL context. Everything is drawn through it from here on
    screen->glContext = SDL_GL_CreateContext(getWindow());
    if (screen->glContext == NULL) {
        printf("Error: OpenGL context could not be created. OpenGL 3.3 is needed. SDL Error: %s\n", SDL_GetError());
        destroyScreen(screen);
        return NULL;
    }
    // No vsync: the emulator paces itself to the NES's speed, and turbo has to be able to run faster
    SDL_GL_SetSwapInterval(0);
    if (!initOpenGL()) {
        destroyScreen(screen);
        return NULL;
    }

    // Nearest-neighbor filtering keeps the NES's pixels sharp when scaled up
    screen->screenTexture = createOpenGLTexture(width, height, width * sizeof(uint32_t), NULL, GL_NEAREST);

    if (!loadScreenShader(&screen->shaders[SCREEN_SHADER_NORMAL], SCREEN_FRAGMENT_SHADER_FILE, "screen") ||
        !loadScreenShader(&screen->shaders[SCREEN_SHADER_CRT], CRT_FRAGMENT_SHADER_FILE, "CRT")) {
        destroyScreen(screen);
        return NULL;
    }
    screen->currentShader = SCREEN_SHADER_NORMAL;

    return screen;
}

void clearScreen(struct Screen* screen) {
    if (screen == NULL) { return; }
    SDL_GL_GetDrawableSize(getWindow(), &screen->outputWidth, &screen->outputHeight);
    glViewport(0, 0, screen->outputWidth, screen->outputHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// Helper function: upload the framebuffer and draw it through the current shader into the current viewport,
// which is pictureWidth x pictureHeight pixels
static void drawPicture(struct Screen* screen, int pictureWidth, int pictureHeight) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen->screenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screen->width, screen->height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, screen->framebuffer);

    const struct ScreenShaderProgram* shader = &screen->shaders[screen->currentShader];
    glDisable(GL_BLEND);
    glUseProgram(shader->program);
    glUniform1i(shader->uniformTexture, 0);
    glUniform2f(shader->uniformSourceSize, (float)screen->width, (float)screen->height);
    glUniform2f(shader->uniformOutputSize, (float)pictureWidth, (float)pictureHeight);
    glUniform1f(shader->uniformTime, SDL_GetTicks64() / 1000.0f);
    glUniform1f(shader->uniformHardScan, screen->crtSettings.hardScan);
    glUniform1f(shader->uniformHardPix, screen->crtSettings.hardPix);
    glUniform2f(shader->uniformWarp, screen->crtSettings.warpH, screen->crtSettings.warpV);
    glUniform1f(shader->uniformMaskDark, screen->crtSettings.maskDark);
    glUniform1f(shader->uniformMaskLight, screen->crtSettings.maskLight);
    drawOpenGLQuad();
}

void drawFrame(struct Screen* screen) {
    if (screen == NULL) { return; }
    clearScreen(screen);

    // Fit the picture to the window without changing its shape, centered with black bars filling the leftover space
    float scale = SDL_min((float)screen->outputWidth / screen->width, (float)screen->outputHeight / screen->height);
    screen->pictureWidth = (int)lroundf(screen->width * scale);
    screen->pictureHeight = (int)lroundf(screen->height * scale);
    screen->pictureX = (screen->outputWidth - screen->pictureWidth) / 2;
    screen->pictureY = (screen->outputHeight - screen->pictureHeight) / 2;

    // OpenGL counts the viewport from the bottom of the window
    glViewport(screen->pictureX, screen->outputHeight - screen->pictureY - screen->pictureHeight, screen->pictureWidth, screen->pictureHeight);
    drawPicture(screen, screen->pictureWidth, screen->pictureHeight);
}

// Helper function: draw the picture through the current shader into an offscreen texture of the given size, and read it
// back as RGB. Returns NULL if it failed
static uint8_t* readShadedPixels(struct Screen* screen, int width, int height) {
    uint8_t* rgb = malloc((size_t)width * height * 3);
    if (rgb == NULL) { return NULL; }

    // Draw into a texture instead of the window, so the screenshot has no black bars or overlay
    GLuint target = createOpenGLTexture(width, height, width * 4, NULL, GL_NEAREST);
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);

    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        glViewport(0, 0, width, height);
        drawPicture(screen, width, height);

        // OpenGL returns the bottom row first, so read the rows into the buffer from the bottom up
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        for (int row = 0; row < height; row++) {
            glReadPixels(0, height - 1 - row, width, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb + (size_t)row * width * 3);
        }
    }

    // Back to drawing into the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &target);

    if (!ok) {
        printf("Error: Could not draw the screen offscreen.\n");
        free(rgb);
        return NULL;
    }
    return rgb;
}

uint8_t* readScreenPixels(struct Screen* screen, bool applyShader, int* width, int* height) {
    if (screen == NULL || width == NULL || height == NULL) { return NULL; }

    if (applyShader) {
        // The size the picture is shown at, so the shader looks the same as on screen. Before the first frame is drawn
        // that isn't known yet, so use the default scale
        *width = (screen->pictureWidth > 0) ? screen->pictureWidth : screen->width * DEFAULT_SCREEN_SCALE;
        *height = (screen->pictureHeight > 0) ? screen->pictureHeight : screen->height * DEFAULT_SCREEN_SCALE;
        return readShadedPixels(screen, *width, *height);
    }

    // The NES's own picture: the framebuffer is ARGB8888 (0xAARRGGBB), and RGB wants each pixel's red, green and blue bytes in that order
    *width = screen->width;
    *height = screen->height;
    int pixelCount = screen->width * screen->height;
    uint8_t* rgb = malloc((size_t)pixelCount * 3);
    if (rgb == NULL) { return NULL; }
    for (int i = 0; i < pixelCount; i++) {
        uint32_t pixel = screen->framebuffer[i];
        rgb[i * 3 + 0] = (uint8_t)(pixel >> 16);
        rgb[i * 3 + 1] = (uint8_t)(pixel >> 8);
        rgb[i * 3 + 2] = (uint8_t)pixel;
    }
    return rgb;
}

void setScreenShader(struct Screen* screen, enum ScreenShader shader) {
    if (screen == NULL || shader < 0 || shader >= NUMBER_OF_SCREEN_SHADERS) { return; }
    screen->currentShader = shader;
}

void setCrtSettings(struct Screen* screen, const struct CrtSettings* settings) {
    if (screen == NULL || settings == NULL) { return; }
    screen->crtSettings = *settings;
}

void presentFrame(struct Screen* screen) {
    if (screen == NULL) { return; }
    SDL_GL_SwapWindow(getWindow());
}

void destroyScreen(struct Screen* screen) {
    if (screen == NULL) { return; }
    if (screen->glContext != NULL) {
        if (screen->screenTexture != 0) { glDeleteTextures(1, &screen->screenTexture); }
        for (int i = 0; i < NUMBER_OF_SCREEN_SHADERS; i++) {
            if (screen->shaders[i].program != 0) { glDeleteProgram(screen->shaders[i].program); }
        }
        shutdownOpenGL();
        SDL_GL_DeleteContext(screen->glContext);
    }
    if (screen->framebuffer != NULL) { free(screen->framebuffer); }
    free(screen);
}
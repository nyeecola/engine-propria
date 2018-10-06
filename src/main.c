#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl3.h"

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

static const struct {
    float x, y;
    float r, g, b;
} vertices[3] =
{
    { -0.6f, -0.4f, 1.f, 0.f, 0.f },
    {  0.6f, -0.4f, 0.f, 1.f, 0.f },
    {   0.f,  0.6f, 0.f, 0.f, 1.f }
};

// TODO: caller must free buffer
char* load_file(char const* path) {
    char* buffer = 0;
    long length;
    FILE * f = fopen (path, "rb"); //was "rb"

    if (f)
    {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      buffer = (char*)malloc ((length+1)*sizeof(char));
      if (buffer)
      {
        fread (buffer, sizeof(char), length, f);
      }
      fclose (f);
    }
    buffer[length] = '\0';

#if 0
    for (int i = 0; i < length; i++) {
        printf("buffer[%d] == %c\n", i, buffer[i]);
    }
    printf("buffer = %s\n", buffer);
#endif

    return buffer;
}

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(void) {
    GLFWwindow* window;
    struct nk_context *ctx;
    struct nk_colorf bg = {0};

    GLuint vertex_buffer, vertex_shader, fragment_shader, program;

    GLint mvp_location, vpos_location, vcol_location;

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Engine Propria", NULL, NULL);

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);

    glewExperimental = 1;
    if (glewInit() != GLEW_OK) exit(EXIT_FAILURE);

    ctx = nk_glfw3_init(window, NK_GLFW3_INSTALL_CALLBACKS);

    struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&atlas);
    nk_glfw3_font_stash_end();

    // display OpenGL context version
    {
        const GLubyte *version_str = glGetString(GL_VERSION);
        printf("%s\n", version_str);
    }

    // currently binding but not using for anything
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glfwSwapInterval(1); // TODO: check
    // NOTE: OpenGL error checks have been omitted for brevity
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // TODO: fix the size
    GLchar shader_info_buffer[200];
    GLint shader_info_len;

    // TODO: free
    char* vertex_shader_text = load_file("src/vertex.glsl");
    char* fragment_shader_text = load_file("src/frag.glsl");

    // vertex shader
    {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, (const char * const *) &vertex_shader_text, NULL);
        glCompileShader(vertex_shader);

        glGetShaderInfoLog(vertex_shader, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Vertex Shader: %s\n", shader_info_buffer);
    }

    // frag shader
    {
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, (const char * const *) &fragment_shader_text, NULL);
        glCompileShader(fragment_shader);

        glGetShaderInfoLog(fragment_shader, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Fragment Shader: %s\n", shader_info_buffer);
    }

    // shader program
    {
        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        glGetProgramInfoLog(program, 200, &shader_info_len, shader_info_buffer);
        if (shader_info_len) printf("Shader Program: %s\n", shader_info_buffer);
    }

    mvp_location = glGetUniformLocation(program, "MVP");
    vpos_location = glGetAttribLocation(program, "vPos");
    vcol_location = glGetAttribLocation(program, "vCol");
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*) 0);
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*) (sizeof(float) * 2));
    glBindVertexArray(0);

    // test code for opening editor on a specific file
    int a = fork();
    if (!a) {
        char *argv[3];
        argv[0] = "/usr/bin/mousepad";
        argv[1] = "/tmp/aaa";
        argv[2] = NULL;
        execvp("/usr/bin/mousepad", (char * const *) &argv[0]);
        return 0;
    }

    while (!glfwWindowShouldClose(window)) {
        {
            int err = glGetError();
            if (err) {
                printf("%d\n", err);
            }
        }

        float ratio;
        int width, height;
        mat4 m, p, mvp;
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float) height;

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            glBindVertexArray(vao);

            glm_mat4_identity(m);
            vec3 axis = {0, 0, 1};
            glm_rotate(m, (float) glfwGetTime(), axis);
            //glm_ortho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f, p);
            float left = -width / 2.0;
            float right = width / 2.0;
            float top = height / 2.0;
            float bottom = -height / 2.0;
            glm_ortho(left, right, top, bottom, 1.f, -1.f, p);
            glm_mat4_mul(p, m, mvp);

            glUseProgram(program);
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) mvp[0]);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glBindVertexArray(0);
        }
        {
            nk_glfw3_new_frame();
            if (nk_begin(ctx, "Engine Propria", nk_rect(50, 50, 230, 250),
                        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE));
            {
                enum {EASY, HARD};
                static int op = EASY;
                static int property = 20;
                nk_layout_row_static(ctx, 30, 80, 1);
                if (nk_button_label(ctx, "button"))
                    fprintf(stdout, "button pressed\n");

                nk_layout_row_dynamic(ctx, 30, 2);
                if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
                if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;

                nk_layout_row_dynamic(ctx, 25, 1);
                nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "background:", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 25, 1);
                if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx),400))) {
                    nk_layout_row_dynamic(ctx, 120, 1);
                    bg = nk_color_picker(ctx, bg, NK_RGBA);
                    nk_layout_row_dynamic(ctx, 25, 1);
                    bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f);
                    bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
                    bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
                    bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);
                    nk_combo_end(ctx);
                }
            }

            nk_end(ctx);
            nk_glfw3_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 1;
}


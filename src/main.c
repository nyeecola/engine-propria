#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

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

typedef enum {
    ADDING_NONE,
    ADDING_PRIMITIVE,
    ADDING_ENTITY,
    ADDING_LOGIC,
    ADDING_MAP,
} NewFileState;

typedef struct {
    char name[20];
    char type[20];
} Attrib;

typedef struct {
    char name[30];
    Attrib attribs[30];
    int num_attribs;

    bool editing;
    bool addingAttribute;
    char addingAttributeBuffer[30];
    char addingAttributeTypeBuffer[30];
} Object;

typedef struct {
    int num_maps;
    char map_name[20][80];

    int num_primitives;
    Object primitives[80];

    int num_entities;
    Object entities[80];

    int num_logics;
    char logic_name[20][80];
} Files;

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

    Files files = {0};

    struct nk_color white = {.r = 255, .g = 255, .b = 255, .a = 255};
    struct nk_style_button file_label_style = {0};
    file_label_style.text_background = white;
    file_label_style.text_normal = white;
    file_label_style.text_hover = white;
    file_label_style.text_active = white;
    file_label_style.text_alignment = NK_TEXT_LEFT;

    NewFileState createFileState = ADDING_NONE;
    char createFileBuffer[50] = {0};
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
            if (nk_begin(ctx, "Game", nk_rect(50, 50, 230, 750),
                        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
                if (nk_tree_push(ctx, NK_TREE_TAB, "Maps", NK_MINIMIZED)) {
                    if (nk_button_label(ctx, "create map")) {
                        createFileState = ADDING_MAP;
                        memset(createFileBuffer, 0, sizeof(createFileBuffer));
                    }

                    if (createFileState == ADDING_MAP) {
                        nk_edit_focus(ctx, 0);
                        if (nk_edit_string_zero_terminated(ctx, NK_EDIT_SIG_ENTER | NK_EDIT_FIELD, createFileBuffer, sizeof(createFileBuffer), nk_filter_default) & NK_EDIT_COMMITED) {
                            strcpy(files.map_name[files.num_maps++], createFileBuffer);
                            createFileState = ADDING_NONE;
                        }
                    }

                    for (int i = 0; i < files.num_maps; i++) {
                        if (nk_button_label_styled(ctx, &file_label_style, files.map_name[i])) {
                        }
                    }

                    nk_tree_pop(ctx);
                }
                if (nk_tree_push(ctx, NK_TREE_TAB, "Objects", NK_MINIMIZED)) {
                    if (nk_tree_push(ctx, NK_TREE_NODE, "Primitives", NK_MINIMIZED)) {
                        if (nk_button_label(ctx, "create primitive")) {
                            createFileState = ADDING_PRIMITIVE;
                            memset(createFileBuffer, 0, sizeof(createFileBuffer));
                        }

                        if (createFileState == ADDING_PRIMITIVE) {
                            nk_edit_focus(ctx, 0);
                            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_SIG_ENTER | NK_EDIT_FIELD, createFileBuffer, sizeof(createFileBuffer), nk_filter_default) & NK_EDIT_COMMITED) {
                                Object p = {0};

                                strcpy(p.name, createFileBuffer);
                                createFileState = ADDING_NONE;

                                files.primitives[files.num_primitives++] = p;
                            }
                        }

                        for (int i = 0; i < files.num_primitives; i++) {
                            if (nk_button_label_styled(ctx, &file_label_style, files.primitives[i].name)) {
                                files.primitives[i].editing = true;
                            }
                        }

                        nk_tree_pop(ctx);
                    }
                    if (nk_tree_push(ctx, NK_TREE_NODE, "Entities", NK_MINIMIZED)) {
                        if (nk_button_label(ctx, "create entity")) {
                            createFileState = ADDING_ENTITY;
                            memset(createFileBuffer, 0, sizeof(createFileBuffer));
                        }

                        if (createFileState == ADDING_ENTITY) {
                            nk_edit_focus(ctx, 0);
                            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_SIG_ENTER | NK_EDIT_FIELD, createFileBuffer, sizeof(createFileBuffer), nk_filter_default) & NK_EDIT_COMMITED) {
                                Object p = {0};

                                strcpy(p.name, createFileBuffer);
                                createFileState = ADDING_NONE;

                                files.entities[files.num_entities++] = p;
                            }
                        }

                        for (int i = 0; i < files.num_entities; i++) {
                            if (nk_button_label_styled(ctx, &file_label_style, files.entities[i].name)) {
                                files.entities[i].editing = true;
                            }
                        }

                        nk_tree_pop(ctx);
                    }
                    nk_tree_pop(ctx);
                }
                if (nk_tree_push(ctx, NK_TREE_TAB, "Logic", NK_MINIMIZED)) {
                    if (nk_button_label(ctx, "create logic file")) {
                        createFileState = ADDING_LOGIC;
                        memset(createFileBuffer, 0, sizeof(createFileBuffer));
                    }

                    if (createFileState == ADDING_LOGIC) {
                        nk_edit_focus(ctx, 0);
                        if (nk_edit_string_zero_terminated(ctx, NK_EDIT_SIG_ENTER | NK_EDIT_FIELD, createFileBuffer, sizeof(createFileBuffer), nk_filter_default) & NK_EDIT_COMMITED) {
                            strcpy(files.logic_name[files.num_logics++], createFileBuffer);
                            createFileState = ADDING_NONE;
                        }
                    }

                    for (int i = 0; i < files.num_logics; i++) {
                        if (nk_button_label_styled(ctx, &file_label_style, files.logic_name[i])) {
                        }
                    }

                    nk_tree_pop(ctx);
                }
            }
            nk_end(ctx);

            for (int i = 0; i < files.num_primitives; i++) {
                if (files.primitives[i].editing) {
                    if (nk_begin(ctx, files.primitives[i].name, nk_rect(100, 50, 230, 250),
                                NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                                NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE)) {
                        nk_layout_row_dynamic(ctx, 15, 1);

                        if (nk_button_label(ctx, "create attribute")) {
                            files.primitives[i].addingAttribute = true;
                            memset(files.primitives[i].addingAttributeBuffer, 0, sizeof(files.primitives[i].addingAttributeBuffer));
                            memset(files.primitives[i].addingAttributeTypeBuffer, 0, sizeof(files.primitives[i].addingAttributeTypeBuffer));
                        }

                        if (files.primitives[i].addingAttribute) {
                            bool ok = false;
                            const float ratio[] = {0.4, 0.6};

                            nk_layout_row(ctx, NK_DYNAMIC, 25, 2, ratio);
                            if (nk_edit_string_zero_terminated(ctx,
                                                               NK_EDIT_SIG_ENTER | NK_EDIT_FIELD,
                                                               files.primitives[i].addingAttributeTypeBuffer,
                                                               sizeof(files.primitives[i].addingAttributeTypeBuffer),
                                                               nk_filter_default)
                                                                & NK_EDIT_COMMITED) {
                                ok = true;
                            }
                            if (nk_edit_string_zero_terminated(ctx,
                                                               NK_EDIT_SIG_ENTER | NK_EDIT_FIELD,
                                                               files.primitives[i].addingAttributeBuffer,
                                                               sizeof(files.primitives[i].addingAttributeBuffer),
                                                               nk_filter_default)
                                                                & NK_EDIT_COMMITED) {
                                ok = true;
                            }

                            if (ok) {
                                strcpy(files.primitives[i].attribs[files.primitives[i].num_attribs].type, files.primitives[i].addingAttributeTypeBuffer);
                                strcpy(files.primitives[i].attribs[files.primitives[i].num_attribs].name, files.primitives[i].addingAttributeBuffer);
                                files.primitives[i].num_attribs++;
                                files.primitives[i].addingAttribute = false;
                            }
                        }

                        for (int j = 0; j < files.primitives[i].num_attribs; j++) {
                            const float ratio[] = {0.4, 0.6};

                            nk_layout_row(ctx, NK_DYNAMIC, 25, 2, ratio);
                            if (nk_button_label_styled(ctx, &file_label_style, files.primitives[i].attribs[j].type));
                            if (nk_button_label_styled(ctx, &file_label_style, files.primitives[i].attribs[j].name));
                        }
                    }
                    nk_end(ctx);

                    if (nk_window_is_hidden(ctx, files.primitives[i].name)) {
                        files.primitives[i].editing = false;
                    }
                }
            }

            for (int i = 0; i < files.num_entities; i++) {
                if (files.entities[i].editing) {
                    if (nk_begin(ctx, files.entities[i].name, nk_rect(100, 50, 230, 250),
                                NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                                NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE)) {
                        nk_layout_row_dynamic(ctx, 15, 1);

                        if (nk_button_label(ctx, "create attribute")) {
                            files.entities[i].addingAttribute = true;
                            memset(files.entities[i].addingAttributeBuffer, 0, sizeof(files.entities[i].addingAttributeBuffer));
                            memset(files.entities[i].addingAttributeTypeBuffer, 0, sizeof(files.entities[i].addingAttributeTypeBuffer));
                        }

                        if (files.entities[i].addingAttribute) {
                            bool ok = false;
                            const float ratio[] = {0.4, 0.6};

                            nk_layout_row(ctx, NK_DYNAMIC, 25, 2, ratio);
                            if (nk_edit_string_zero_terminated(ctx,
                                                               NK_EDIT_SIG_ENTER | NK_EDIT_FIELD,
                                                               files.entities[i].addingAttributeTypeBuffer,
                                                               sizeof(files.entities[i].addingAttributeTypeBuffer),
                                                               nk_filter_default)
                                                                & NK_EDIT_COMMITED) {
                                ok = true;
                            }
                            if (nk_edit_string_zero_terminated(ctx,
                                                               NK_EDIT_SIG_ENTER | NK_EDIT_FIELD,
                                                               files.entities[i].addingAttributeBuffer,
                                                               sizeof(files.entities[i].addingAttributeBuffer),
                                                               nk_filter_default)
                                                                & NK_EDIT_COMMITED) {
                                ok = true;
                            }

                            if (ok) {
                                strcpy(files.entities[i].attribs[files.entities[i].num_attribs].type, files.entities[i].addingAttributeTypeBuffer);
                                strcpy(files.entities[i].attribs[files.entities[i].num_attribs].name, files.entities[i].addingAttributeBuffer);
                                files.entities[i].num_attribs++;
                                files.entities[i].addingAttribute = false;
                            }
                        }

                        for (int j = 0; j < files.entities[i].num_attribs; j++) {
                            const float ratio[] = {0.4, 0.6};

                            nk_layout_row(ctx, NK_DYNAMIC, 25, 2, ratio);
                            if (nk_button_label_styled(ctx, &file_label_style, files.entities[i].attribs[j].type));
                            if (nk_button_label_styled(ctx, &file_label_style, files.entities[i].attribs[j].name));
                        }
                    }
                    nk_end(ctx);

                    if (nk_window_is_hidden(ctx, files.entities[i].name)) {
                        files.entities[i].editing = false;
                    }
                }
            }

            nk_glfw3_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 1;
}


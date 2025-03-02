#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <source_location>

#define cast(t) (t)
#define discard(x) (void) (x)

[[noreturn]]
void die(const char* msg, std::source_location loc = std::source_location::current()) {
    fprintf(stderr, "%s:%d:%d: FATAL ERROR: %s\n", loc.file_name(), loc.line(), loc.column(), msg);
    exit(1);
}

constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;

static const char* vert_src = R"src(#version 330
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 in_color;

uniform float time;
uniform mat4 projection;

out vec3 color;

void main()
{
    const float scale = 3.1415926 * 2 / 1.5;
    float ratio = scale * time;
    vec3 offset = vec3(cos(ratio), sin(ratio), 0);

    gl_Position = projection * vec4(pos + offset, 1);
    color = in_color;
}
)src";

static const char* frag_src = R"src(#version 330
in vec3 color;

out vec4 out_color;

void main()
{
    // out_color = vec4(0.0, 1.0, 0.0, 1.0);
    // out_color = vec4((gl_FragCoord / 800).xyy, 1.0);
    out_color = vec4(color, 1);
}
)src";

#define ENUM_GL_PROCS \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLDETACHSHADERPROC, glDetachShader) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)

#define X(t, name) static t name;
ENUM_GL_PROCS
#undef X

void load_gl_procs() {
#define X(t, name) name = cast(t) glfwGetProcAddress(#name);
ENUM_GL_PROCS
#undef X
}

GLuint create_shader(GLenum type, const char* src) {
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);

    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        char info_log[256];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);

        fprintf(stderr, "Could not compile the shader: %s\n", info_log);
        exit(1);
    }

    return shader;
}

GLuint create_program(GLuint vert, GLuint frag) {
    auto prog = glCreateProgram();

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glLinkProgram(prog);

    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        char info_log[256];
        glGetProgramInfoLog(prog, sizeof(info_log), nullptr, info_log);

        fprintf(stderr, "Could not link the program: %s\n", info_log);
        exit(1);
    }

    glDetachShader(prog, vert);
    glDetachShader(prog, frag);

    return prog;
}

float aspect_ratio = (float)WIN_WIDTH / (float)WIN_HEIGHT;

extern "C" void window_size_callback(GLFWwindow* win, int width, int height) {
    discard(win);

    glViewport(0, 0, width, height);
}

int main() {
    if (!glfwInit()) {
        die("could not initialize GLFW");
    }

    auto* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "HELLO", nullptr, nullptr);

    if (!window) {
        die("could not create glfw window");
    }

    glfwMakeContextCurrent(window);

    glfwSetWindowSizeCallback(window, window_size_callback);

    load_gl_procs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0, 1.0);

    glEnable(GL_DEPTH_CLAMP);

    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    // glFrontFace(GL_CW);

    float piramid_vertices[] = {
        // front face
        -0.5, -0.5, -2,
        0, 0.5, -3,
        0.5, -0.5, -2,

        // left face
        -0.5, -0.5, -2,
        0, 0.5, -3,
        0, -0.5, -4,

        // right face
        0.5, -0.5, -2,
        0, 0.5, -3,
        0, -0.5, -4,

        // colors
        1, 0, 0,
        1, 0, 0,
        1, 0, 0,

        0, 1, 0,
        0, 1, 0,
        0, 1, 0,

        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
    };

    float prism_vertices[] = {
        0.25f,  0.25f, -1.25f,
	0.25f, -0.25f, -1.25f,
	-0.25f,  0.25f, -1.25f,

	0.25f, -0.25f, -1.25f,
	-0.25f, -0.25f, -1.25f,
	-0.25f,  0.25f, -1.25f,

	0.25f,  0.25f, -2.75f,
	-0.25f,  0.25f, -2.75f,
	0.25f, -0.25f, -2.75f,

	0.25f, -0.25f, -2.75f,
	-0.25f,  0.25f, -2.75f,
	-0.25f, -0.25f, -2.75f,

	-0.25f,  0.25f, -1.25f,
	-0.25f, -0.25f, -1.25f,
	-0.25f, -0.25f, -2.75f,

	-0.25f,  0.25f, -1.25f,
	-0.25f, -0.25f, -2.75f,
	-0.25f,  0.25f, -2.75f,

	0.25f,  0.25f, -1.25f,
	0.25f, -0.25f, -2.75f,
	0.25f, -0.25f, -1.25f,

	0.25f,  0.25f, -1.25f,
	0.25f,  0.25f, -2.75f,
	0.25f, -0.25f, -2.75f,

	0.25f,  0.25f, -2.75f,
	0.25f,  0.25f, -1.25f,
	-0.25f,  0.25f, -1.25f,

	0.25f,  0.25f, -2.75f,
	-0.25f,  0.25f, -1.25f,
	-0.25f,  0.25f, -2.75f,

	0.25f, -0.25f, -2.75f,
	-0.25f, -0.25f, -1.25f,
	0.25f, -0.25f, -1.25f,

	0.25f, -0.25f, -2.75f,
	-0.25f, -0.25f, -2.75f,
	-0.25f, -0.25f, -1.25f,


	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,

	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,

	0.8f, 0.8f, 0.8f,
	0.8f, 0.8f, 0.8f,
	0.8f, 0.8f, 0.8f,

	0.8f, 0.8f, 0.8f,
	0.8f, 0.8f, 0.8f,
	0.8f, 0.8f, 0.8f,

	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,

	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,

	0.5f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.0f,

	0.5f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.0f,

	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,

	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,

	0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 1.0f,

	0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 1.0f,
    };

#define vertices piramid_vertices

    GLuint vao;
    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, cast(void*) (sizeof(vertices) / 2));

    auto vert = create_shader(GL_VERTEX_SHADER, vert_src);
    auto frag = create_shader(GL_FRAGMENT_SHADER, frag_src);
    auto prog = create_program(vert, frag);

    glDeleteShader(vert);
    glDeleteShader(frag);

    glUseProgram(prog);

    float mat[16];
    memset(mat, 0, sizeof(mat));

    const float frustum_scale = 0.5;
    const float z_far = 5.0;
    const float z_near = 2.0;

    mat[0] = frustum_scale / aspect_ratio;
    mat[5] = frustum_scale;
    mat[10] = (z_far + z_near) / (z_near - z_far);
    mat[11] = -1;
    mat[14] = (2 * z_far * z_near) / (z_near - z_far);

    auto projection_loc = glGetUniformLocation(prog, "projection");

    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, mat);

    auto time_loc = glGetUniformLocation(prog, "time");

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.8f, 0.f, 0.5f, 1.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double time = glfwGetTime();

        glUniform1f(time_loc, time);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}

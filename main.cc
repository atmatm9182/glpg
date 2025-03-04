#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <source_location>

#include <GLFW/glfw3.h>

#define cast(t) (t)
#define discard (void)

[[noreturn]]
void die(const char* msg, std::source_location loc = std::source_location::current()) {
    fprintf(stderr, "%s:%d:%d: FATAL ERROR: %s\n", loc.file_name(), loc.line(), loc.column(), msg);
    exit(1);
}

constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;

static const char* vert_src = R"src(#version 330
layout(location = 0) in vec4 pos;
layout(location = 1) in vec3 in_color;

uniform float time;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 color;

void main() {
    gl_Position = projection * view * model * pos;
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

const float aspect_ratio = (float)WIN_WIDTH / (float)WIN_HEIGHT;

static inline float deg_to_rad(float angle) {
    return angle * M_PI * 2.0f / 360.0f;
}

struct Vec3 {
    inline Vec3 copy() const {
        return Vec3{x, y, z};
    }

    inline void norm() {
        float len = length();
        x /= len;
        y /= len;
        z /= len;
    }

    inline void cross(Vec3 other) {
        float xx = x;
        float yy = y;

        x = yy * other.z - z  * other.y;
        y = z  * other.x - xx * other.z;
        z = xx * other.y - yy * other.x;
    }

    inline float length() const {
        return sqrtf(x * x + y * y + z * z);
    }

    inline void neg() {
        x = -x;
        y = -y;
        z = -z;
    }

    inline float sum() const {
        return x + y + z;
    }

    inline Vec3 operator -(const Vec3& other) {
        return Vec3{x - other.x, y - other.y, z - other.z};
    }

    inline Vec3 operator *(const Vec3& other) {
        return Vec3{x * other.x, y * other.y, z * other.z};
    }

    inline Vec3 operator *(float scalar) {
        return Vec3{x * scalar, y * scalar, z * scalar};
    }

    inline Vec3 operator +(const Vec3& other) {
        return Vec3{x + other.x, y + other.y, z + other.z};
    }

    inline Vec3& operator +=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;

        return *this;
    }

    inline Vec3& operator -=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;

        return *this;
    }

    float x, y, z;
};

inline Vec3 operator *(float scalar, Vec3 vec) {
    return Vec3{vec.x * scalar, vec.y * scalar, vec.z * scalar};
}

struct Mat4 {
    Mat4() : elems{1, 0, 0, 0,
                   0, 1, 0, 0,
                   0, 0, 1, 0,
                   0, 0, 0, 1}
    {}

    Mat4(float m11, float m12, float m13, float m14,
         float m21, float m22, float m23, float m24,
         float m31, float m32, float m33, float m34,
         float m41, float m42, float m43, float m44
         ) : elems{m11, m21, m31, m41,
                   m12, m22, m32, m42,
                   m13, m23, m33, m43,
                   m14, m24, m34, m44}
    {}

    static Mat4 look_at(Vec3 camera_pos, Vec3 camera_target, Vec3 up) {
        // NOTE: the order is reversed, and the vector points towards the camera
        Vec3 camera_dir = (camera_pos - camera_target);
        camera_dir.norm();

        Vec3 camera_right = up;
        camera_right.cross(camera_dir);
        camera_right.norm();

        Vec3 camera_up = camera_dir;
        camera_up.cross(camera_right);

        camera_pos.neg();

        Vec3 right_perm = camera_pos * camera_right;
        Vec3 up_perm = camera_pos * camera_up;
        Vec3 dir_perm = camera_pos * camera_dir;

        float right_perm_sum = right_perm.sum();
        float up_perm_sum = up_perm.sum();
        float dir_perm_sum = dir_perm.sum();

        return Mat4{camera_right.x, camera_right.y, camera_right.z, right_perm_sum,
                    camera_up.x,    camera_up.y,    camera_up.z,    up_perm_sum,
                    camera_dir.x,   camera_dir.y,   camera_dir.z,   dir_perm_sum,
                    0,              0,              0,              1};
    }

    static Mat4 projection(float fov_x, float z_near, float z_far) {
        Mat4 mat{};

        const float fov_x_rad = deg_to_rad(fov_x);
        const float angle = fov_x_rad / 2;
        float tangent = tanf(angle);

        float right = z_near * tangent;
        float top = right / aspect_ratio;

        mat.elems[0] = z_near / right;
        mat.elems[5] = z_near / top;
        // mat.elems[0] = 1 / tangent;
        // mat.elems[5] = 1 / tangent;
        mat.elems[10] = (z_far + z_near) / (z_near - z_far);
        mat.elems[11] = -1;
        mat.elems[14] = (2 * z_far * z_near) / (z_near - z_far);

        return mat;
    }

    Mat4& translate(float x, float y = 0.0f, float z = 0.0f) {
        elems[12] += x;
        elems[13] += y;
        elems[14] += z;

        return *this;
    }

    Mat4& rotate_x(float angle) {
        angle = deg_to_rad(angle);

        float c = cosf(angle);
        elems[5] *= c;
        elems[10] *= c;

        float s = sinf(angle);
        elems[6] = s;
        elems[9] = -s;

        return *this;
    }

    Mat4& rotate_y(float angle) {
        angle = deg_to_rad(angle);

        float c = cosf(angle);
        elems[0] *= c;
        elems[10] *= c;

        float s = sinf(angle);
        elems[2] = -s;
        elems[8] = s;

        return *this;
    }

    Mat4& rotate_z(float angle) {
        angle = deg_to_rad(angle);

        float c = cosf(angle);
        elems[0] *= c;
        elems[5] *= c;

        float s = sinf(angle);
        elems[1] = s;
        elems[4] = -s;

        return *this;
    }

    float elems[16];
};

extern "C" void window_size_callback(GLFWwindow* win, int width, int height) {
    discard win;

    glViewport(0, 0, width, height);
}

Vec3 camera_pos = {0.f, 0.f, 3.f};
Vec3 camera_front = {0.f, 0.f, -1.f};
Vec3 camera_up = {0.f, 1.f, 0.f};

float camera_yaw = -90.f;
float camera_pitch = 0.f;

constexpr float CAMERA_PITCH_MAX =  89.0f;
constexpr float CAMERA_PITCH_MIN = -89.0f;

float delta_time = 0.f;

float last_x = WIN_WIDTH  / 2.f;
float last_y = WIN_HEIGHT / 2.f;

extern "C" void mouse_callback(GLFWwindow* window, double x, double y) {
    discard window;

    float x_off = x - last_x;
    float y_off = last_y - y;

    last_x = x;
    last_y = y;

    const float camera_sensitivity = 0.1f;

    x_off *= camera_sensitivity;
    y_off *= camera_sensitivity;

    camera_yaw   += x_off;
    camera_pitch += y_off;

    if      (camera_pitch > CAMERA_PITCH_MAX) camera_pitch = CAMERA_PITCH_MAX;
    else if (camera_pitch < CAMERA_PITCH_MIN) camera_pitch = CAMERA_PITCH_MIN;

    float camera_pitch_rad = deg_to_rad(camera_pitch);
    float camera_yaw_rad   = deg_to_rad(camera_yaw);

    float camera_pitch_cos = cosf(camera_pitch_rad);
    float camera_pitch_sin = sinf(camera_pitch_rad);
    float camera_yaw_cos   = cosf(camera_yaw_rad);
    float camera_yaw_sin   = sinf(camera_yaw_rad);

    Vec3 camera_direction = {
        camera_yaw_cos * camera_pitch_cos,
        camera_pitch_sin,
        camera_yaw_sin * camera_pitch_cos,
    };

    camera_front = camera_direction;
    camera_front.norm();
}

void process_input(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

    const float camera_speed = 5.f * delta_time;

    if (glfwGetKey(window, GLFW_KEY_W)) {
        camera_pos += camera_front * camera_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S)) {
        camera_pos -= camera_front * camera_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_A)) {
        Vec3 right = camera_front;
        right.cross(camera_up);
        right.norm();

        camera_pos -= right * camera_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D)) {
        Vec3 right = camera_front;
        right.cross(camera_up);
        right.norm();

        camera_pos += right * camera_speed;
    }
}


int main() {
    if (!glfwInit()) {
        die("could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    GLFWmonitor* monitor = nullptr;

    auto* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "HELLO", monitor, nullptr);

    if (!window) {
        die("could not create glfw window");
    }

    glfwMakeContextCurrent(window);

    // set the cursor to 'captured' mode
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(window, window_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

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
        -0.5, -0.5, 0.5,
        0, 0.5, 0,
        0.5, -0.5, 0.5,

        // left face
        -0.5, -0.5, 0.5,
        0, 0.5, 0,
        0, -0.5, -0.5,

        // right face
        0.5, -0.5, 0.5,
        0, 0.5, 0,
        0, -0.5, -0.5,

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

    const float fov_x = 45.0;
    const float z_far = 10.0;
    const float z_near = 2.0;

    Mat4 projection_mat = Mat4::projection(fov_x, z_near, z_far);

    auto model_loc = glGetUniformLocation(prog, "model");
    auto view_loc = glGetUniformLocation(prog, "view");
    auto projection_loc = glGetUniformLocation(prog, "projection");

    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, projection_mat.elems);

    auto time_loc = glGetUniformLocation(prog, "time");

    double last_frame_time = 0;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.8f, 0.f, 0.5f, 1.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double time = glfwGetTime();
        delta_time = time - last_frame_time;
        last_frame_time = time;

        process_input(window);

        printf("%f\n", delta_time);

        glUniform1f(time_loc, time);

        Mat4 model_mat{};

        // model_mat.rotate_y(30.0f * time);
        model_mat.translate(0, 0, -5.0f);

        glUniformMatrix4fv(model_loc, 1, GL_FALSE, model_mat.elems);

        Mat4 view_mat = Mat4::look_at(camera_pos, camera_pos + camera_front, camera_up);

        glUniformMatrix4fv(view_loc, 1, GL_FALSE, view_mat.elems);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}

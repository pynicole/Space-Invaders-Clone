/*
Space Invaders Clone
By: Nicole Rodgers
Credit/Resources:	http://nicktasios.nl/posts/space-invaders-from-scratch-part-1.html
                    https://www.pcmag.com/encyclopedia/term/double-buffering
                    https://subscription.packtpub.com/book/game-development/9781789340365/1/ch01lvl1sec12/creating-the-opengl-rendering-window-using-glfw
                    https://rauwendaal.net/2014/06/14/rendering-a-screen-covering-triangle-in-opengl/


*/

#define GLEW_STATIC
#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

/* */
#define GL_ERROR_CASE(glerror)\
    case glerror: snprintf(error, sizeof(error), "%s", #glerror)

/* */
inline void gl_debug(const char* file, int line) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char error[128];

        switch (err) {
            GL_ERROR_CASE(GL_INVALID_ENUM); break;
            GL_ERROR_CASE(GL_INVALID_VALUE); break;
            GL_ERROR_CASE(GL_INVALID_OPERATION); break;
            GL_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION); break;
            GL_ERROR_CASE(GL_OUT_OF_MEMORY); break;
        default: snprintf(error, sizeof(error), "%s", "UNKNOWN_ERROR"); break;
        }

        fprintf(stderr, "%s - %s: %d\n", error, file, line);
    }
}

#undef GL_ERROR_CASE

/* */
void validate_shader(GLuint shader, const char* file = 0) {
    static const unsigned int BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

    if (length > 0) {
        printf("Shader %d(%s) compile error: %s\n", shader, (file ? file : ""), buffer);
    }
}
 /* */
bool validate_program(GLuint program) {
    static const GLsizei BUFFER_SIZE = 512;
    GLchar buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

    if (length > 0) {
        printf("Program %d link error: %s\n", program, buffer);
        return false;
    }

    return true;
}

void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

/* Instead of using GPU, we will be using CPU to render everything.
We use a buffer to be passed to the GPU as a texture and then
drawn on the screen. */
struct Buffer {
    size_t width, height;
    /* Each pixel is represented as 'uint32_t' which allows
    each pixel to be stored as 4 8-bit color values. */
    uint32_t* data;
};

/* Define simple sprite */
struct Sprite {
    size_t width, height;
    uint8_t* data;
};

/* Define the evil aliens */
struct Alien {
    size_t x, y;
    uint8_t type;
};
 /* Define player of game */
struct Player {
    size_t x, y;
    size_t life;
};

/* Define game */
struct Game {
    size_t width, height;
    size_t num_aliens;
    Alien* aliens;
    Player player;
};


/* Clears buffer to certain color. */
void buffer_clear(Buffer* buffer, uint32_t color) {
    for (size_t i = 0; i < buffer->width * buffer->height; ++i)
    {
        buffer->data[i] = color;
    }
}

/* To check for overlap of sprite rectangles. If rectangles overlap, it checks if
any pixel of sprite one overlaps with sprite two.*/
bool sprite_overlap_check(
    const Sprite& sp_a, size_t x_a, size_t y_a,
    const Sprite& sp_b, size_t x_b, size_t y_b
)
{
    if (x_a < x_b + sp_b.width && x_a + sp_a.width > x_b &&
        y_a < y_b + sp_b.height && y_a + sp_a.height > y_b)
    {
        return true;
    }

    return false;
}

/* */
void buffer_draw_sprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t color) {
    for (size_t xi = 0; xi < sprite.width; ++xi)
    {
        for (size_t yi = 0; yi < sprite.height; ++yi)
        {
            if (sprite.data[yi * sprite.width + xi] &&
                (sprite.height - 1 + y - yi) < buffer->height &&
                (x + xi) < buffer->width)
            {
                buffer->data[(sprite.height - 1 + y - yi) * buffer->width + (x + xi)] = color;
            }
        }
    }
}

/* To define colors as uint32_t values. Sets left-most 24 bits to
r, g, b values and 8 right-most bits to 255. Note: alpha channel
not being used. */
uint32_t rgb_to_uint32(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 24) | (g << 16) | (b << 8) | 255;
}

/* MAIN */
int main(int argc, char* argv[]) {
    const size_t buffer_width = 224;
    const size_t buffer_height = 256;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        return -1;
    }

    /* We are using OpenGL (Open Graphics Library) which communicates
    with the users hardware. A users hardware may not support some
    OpenGL calls, therefore we need to declare and load the OpenGL
    functions at runtime. We use the 'hints' below to do so.
    It is reccommended to use a loading library, so GLEW will be
    used. */
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    /* Creates a window of resolution 640x480 with title of
    'Space Invaders'. The last two arguments (NULL, NULL)
    are to specify full-screen mode and sharing context
    between different windows. */
    GLFWwindow* window = glfwCreateWindow(buffer_width, buffer_height, "Space Invaders", NULL, NULL);
    /* Tells GLFW that if there is a problem opening the window,
    to destroy its resources. */
    if (!window) {
        glfwTerminate();
        return -1;
    }

    /* This allows subsquent calls to apply directly to the already
    opened window called 'window'. */
    glfwMakeContextCurrent(window);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error initializing GLEW.\n");
        glfwTerminate();
        return -1;
    }
    int glVersion[2] = { -1, 1 };
    glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
    glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

    gl_debug(__FILE__, __LINE__);

    printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);
    printf("Renderer used: %s\n", glGetString(GL_RENDERER));
    printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glClearColor(1.0, 0.0, 0.0, 1.0);

    /* Initializes graphic buffer of set width/height */
    Buffer buffer;
    buffer.width = buffer_width;
    buffer.height = buffer_height;
    buffer.data = new uint32_t[buffer.width * buffer.height];

    buffer_clear(&buffer, 0);

    /* */
    GLuint buffer_texture;
    glGenTextures(1, &buffer_texture);
    glBindTexture(GL_TEXTURE_2D, buffer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, buffer.width, buffer.height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    /* Creating a vertex array object for generating fullscreen
    triangle (VAO: structure in OpenGL that stores the format of the
    vertex data along with the vertex data) */
    GLuint fullscreen_triangle_vao;
    glGenVertexArrays(1, &fullscreen_triangle_vao);


    /* Create fragment shader */
    static const char* fragment_shader =
        "\n"
        "#version 330\n"
        "\n"
        "uniform sampler2D buffer;\n"
        "noperspective in vec2 TexCoord;\n"
        "\n"
        "out vec3 outColor;\n"
        "\n"
        "void main(void){\n"
        "    outColor = texture(buffer, TexCoord).rgb;\n"
        "}\n";

    /* Create vertex shader */
    static const char* vertex_shader =
        "\n"
        "#version 330\n"
        "\n"
        "noperspective out vec2 TexCoord;\n"
        "\n"
        "void main(void){\n"
        "\n"
        "    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
        "    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
        "    \n"
        "    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
        "}\n";

    GLuint shader_id = glCreateProgram();

    {
        /* Creating vertex shader */
        GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(shader_vp, 1, &vertex_shader, 0);
        glCompileShader(shader_vp);
        validate_shader(shader_vp, vertex_shader);
        glAttachShader(shader_id, shader_vp);

        glDeleteShader(shader_vp);
    }

    {
        /* Creating fragment shader */
        GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shader_fp, 1, &fragment_shader, 0);
        glCompileShader(shader_fp);
        validate_shader(shader_fp, fragment_shader);
        glAttachShader(shader_id, shader_fp);

        glDeleteShader(shader_fp);
    }

    glLinkProgram(shader_id);

    /* Error event for validating shader */
    if (!validate_program(shader_id)) {
        fprintf(stderr, "Error while validating shader.\n");
        glfwTerminate();
        glDeleteVertexArrays(1, &fullscreen_triangle_vao);
        delete[] buffer.data;
        return -1;
    }

    glUseProgram(shader_id);

    /* */
    GLint location = glGetUniformLocation(shader_id, "buffer");
    glUniform1i(location, 0);


    /* penGL setup */
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(fullscreen_triangle_vao);

    /* */
    Sprite alien_sprite;
    alien_sprite.width = 11;
    alien_sprite.height = 8;
    alien_sprite.data = new uint8_t[88] {
        0,0,1,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,1,0,0,0,
        0,0,1,1,1,1,1,1,1,0,0,
        0,1,1,0,1,1,1,0,1,1,0,
        1,1,1,1,1,1,1,1,1,1,1,
        1,0,1,1,1,1,1,1,1,0,1,
        1,0,1,0,0,0,0,0,1,0,1,
        0,0,0,1,1,0,1,1,0,0,0,
    };

    uint32_t clear_color = rgb_to_uint32(0, 128, 0);

    /* Creating infinite game loop to keep window open so long
    as the user does not exit. */
    while (!glfwWindowShouldClose(window)) {
        buffer_clear(&buffer, clear_color);

        buffer_draw_sprite(&buffer, alien_sprite, 112, 128, rgb_to_uint32(128, 0, 0));

        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            buffer.width, buffer.height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
            buffer.data
        );
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        /* OpenGL uses double buffering which is a technique
        that uses "two buffers to speed up a computer that can
        overlap I/O with processing = data in one buffer is being
        processed while the next set of data is read into the other
        one." (PCMag)
        This function is used for this. */
        glfwSwapBuffers(window);

        /* Tells GLFW to process any pending events */
        glfwPollEvents();
    }

    /* Makes program exit cleanly */
    glfwDestroyWindow(window);
    glfwTerminate();
    glDeleteVertexArrays(1, &fullscreen_triangle_vao);
    delete[] alien_sprite.data;
    delete[] buffer.data;

    return 0;
}

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <webp/decode.h>
#include <shaders.hpp>
#include <setup-preprocessor.hpp>

const float tex_coords[] =
{
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f
};
WebPBitstreamFeatures image_features;
const uint8_t *image_buffer = nullptr;
GLFWwindow *window = nullptr;
const float *vertices = nullptr;
GLuint shader_program = 0;
GLuint vao = 0;
GLuint vbo[] = {0};
GLuint texture = 0;
int32_t window_width = 800;
int32_t window_height = 600;

void cleanup()
{
    if(image_buffer)
    {
        WebPFree((void *)image_buffer);
    }

    delete[] vertices;

    gl::glDeleteProgram(shader_program);
    gl::glDeleteTextures(1, &texture);
    gl::glDeleteVertexArrays(1, &vao);
    gl::glDeleteBuffers(2, vbo);

    if(window)
    {
        glfwDestroyWindow(window);
    }

    glfwTerminate();
}

const uint8_t *make_image_buffer(WebPBitstreamFeatures &image_features, const std::filesystem::path &path)
{
    std::ifstream istream(path, std::ios::binary | std::ios::ate);
    const size_t file_size = istream.tellg();

    istream.seekg(0);

    char riff_bytes[5] = "";

    istream.read(riff_bytes, 4);

    if(std::strcmp(riff_bytes, "RIFF") != 0)
    {
        std::cerr << "Invalid file" << std::endl;

        return nullptr;
    }

    istream.seekg(8);

    char webp_bytes[8] = "";

    istream.read(webp_bytes, 7);

    if(std::strcmp(webp_bytes, "WEBPVP8") != 0)
    {
        std::cerr << "Invalid file" << std::endl;

        return nullptr;
    }

    istream.seekg(0);

    const uint8_t *const file_buffer = new uint8_t[file_size];

    istream.read((char *)file_buffer, file_size);

    if(WebPGetFeatures(file_buffer, file_size, &image_features) == VP8StatusCode::VP8_STATUS_NOT_ENOUGH_DATA)
    {
        std::cerr << "Invalid file" << std::endl;

        delete[] file_buffer;

        return nullptr;
    }

    const uint8_t *image_buffer = nullptr;

    if(image_features.has_alpha)
    {
        image_buffer = WebPDecodeRGBA(file_buffer, file_size, nullptr, nullptr);
    }
    else
    {
        image_buffer = WebPDecodeRGB(file_buffer, file_size, nullptr, nullptr);
    }

    delete[] file_buffer;

    return image_buffer;
}

const float *make_vertices(const int32_t window_width, const int32_t window_height, const int32_t image_width, const int32_t image_height)
{
    const float window_aspect = window_width / (float)window_height;
    const float image_aspect = image_width / (float)image_height;
    float *vertices = new float[12];

    if(image_aspect > window_aspect)
    {
        vertices[10] = vertices[0] = vertices[2] = -1.0f;
        vertices[4] = vertices[6] = vertices[8] = 1.0f;
        vertices[9] = vertices[11] = vertices[1] = window_aspect / image_aspect;
        vertices[3] = vertices[5] = vertices[7] = -window_aspect / image_aspect;
    }
    else
    {
        vertices[10] = vertices[0] = vertices[2] = -image_aspect / window_aspect;
        vertices[4] = vertices[6] = vertices[8] = image_aspect / window_aspect;
        vertices[9] = vertices[11] = vertices[1] = 1.0f;
        vertices[3] = vertices[5] = vertices[7] = -1.0f;
    }

    return vertices;
}

void on_resize(GLFWwindow *window, int width, int height)
{
    delete[] vertices;

    window_width = width;
    window_height = height;
    vertices = make_vertices(window_width, window_height, image_features.width, image_features.height);
    
    glViewport(0, 0, width, height);
    gl::glBindVertexArray(vao);
    gl::glBindBuffer(gl::GLenum::GL_ARRAY_BUFFER, vbo[0]);
    gl::glBufferData(gl::GLenum::GL_ARRAY_BUFFER, 12 * sizeof(float), vertices, gl::GLenum::GL_DYNAMIC_DRAW);
    gl::glVertexAttribPointer(0, 2, gl::GLenum::GL_FLOAT, false, 2 * sizeof(float), 0);
    gl::glEnableVertexAttribArray(0);
}

int main(int32_t argc, char **argv)
{
    if(argc < 2)
    {
        std::cerr << "Not enough arguments" << std::endl;

        return -1;
    }

    image_buffer = make_image_buffer(image_features, std::filesystem::path(argv[1]));

    if(image_buffer == nullptr)
    {
        return -1;
    }

    vertices = make_vertices(window_width, window_height, image_features.width, image_features.height);

    if(glfwInit() == GLFW_FALSE)
    {
        cleanup();

        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(window_width, window_height, "WebP Viewer", nullptr, nullptr);

    if(window == nullptr)
    {
        cleanup();

        return -1;
    }

    glfwSetWindowSizeCallback(window, on_resize);
    glfwMakeContextCurrent(window);

    glbinding::Binding::initialize(glfwGetProcAddress);
    gl::glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    shader_program = gl::glCreateProgram();

    {
        GLuint vertex_shader = gl::glCreateShader(gl::GLenum::GL_VERTEX_SHADER);
        GLuint fragment_shader = gl::glCreateShader(gl::GLenum::GL_FRAGMENT_SHADER);

        gl::glShaderSource(vertex_shader, 1, &vertex_shader_src, 0);
        gl::glShaderSource(fragment_shader, 1, &fragment_shader_src, 0);
        gl::glCompileShader(vertex_shader);
        gl::glCompileShader(fragment_shader);
        gl::glAttachShader(shader_program, vertex_shader);
        gl::glAttachShader(shader_program, fragment_shader);
        gl::glLinkProgram(shader_program);
        gl::glDeleteShader(vertex_shader);
        gl::glDeleteShader(fragment_shader);
    }

    gl::glGenTextures(1, &texture);
    gl::glGenVertexArrays(1, &vao);
    gl::glGenBuffers(2, vbo);
    gl::glBindVertexArray(vao);
    gl::glBindBuffer(gl::GLenum::GL_ARRAY_BUFFER, vbo[0]);
    gl::glBufferData(gl::GLenum::GL_ARRAY_BUFFER, 12 * sizeof(float), vertices, gl::GLenum::GL_DYNAMIC_DRAW);
    gl::glVertexAttribPointer(0, 2, gl::GLenum::GL_FLOAT, false, 2 * sizeof(float), 0);
    gl::glEnableVertexAttribArray(0);
    gl::glBindBuffer(gl::GLenum::GL_ARRAY_BUFFER, vbo[1]);
    gl::glBufferData(gl::GLenum::GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, gl::GLenum::GL_STATIC_DRAW);
    gl::glVertexAttribPointer(1, 2, gl::GLenum::GL_FLOAT, false, 2 * sizeof(float), 0);
    gl::glEnableVertexAttribArray(1);
    gl::glTexParameteri(gl::GLenum::GL_TEXTURE_2D, gl::GLenum::GL_TEXTURE_WRAP_S, gl::GLenum::GL_CLAMP_TO_BORDER);
    gl::glTexParameteri(gl::GLenum::GL_TEXTURE_2D, gl::GLenum::GL_TEXTURE_WRAP_T, gl::GLenum::GL_CLAMP_TO_BORDER);
    gl::glTexParameteri(gl::GLenum::GL_TEXTURE_2D, gl::GLenum::GL_TEXTURE_MIN_FILTER, gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
    gl::glTexParameteri(gl::GLenum::GL_TEXTURE_2D, gl::GLenum::GL_TEXTURE_MAG_FILTER, gl::GLenum::GL_LINEAR);
    gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, texture);

    if(image_features.has_alpha)
    {
        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGBA, image_features.width, image_features.height, 0, gl::GLenum::GL_RGBA, gl::GLenum::GL_UNSIGNED_BYTE, image_buffer);
    }
    else
    {
        gl::glTexImage2D(gl::GLenum::GL_TEXTURE_2D, 0, gl::GLenum::GL_RGB, image_features.width, image_features.height, 0, gl::GLenum::GL_RGB, gl::GLenum::GL_UNSIGNED_BYTE, image_buffer);
    }

    gl::glGenerateMipmap(gl::GLenum::GL_TEXTURE_2D);

    WebPFree((void *)image_buffer);

    image_buffer = nullptr;

    while(glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        gl::glClear(gl::ClearBufferMask::GL_COLOR_BUFFER_BIT);
        gl::glUseProgram(shader_program);
        gl::glBindTexture(gl::GLenum::GL_TEXTURE_2D, texture);
        gl::glBindVertexArray(vao);
        gl::glDrawArrays(gl::GLenum::GL_TRIANGLES, 0, 6);
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    cleanup();

    return 0;
}

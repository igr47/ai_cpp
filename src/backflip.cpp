#include <cstddef>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <istream>
#include <list>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctime>
#include <unistd.h>
#include <cfloat>

const char* vertexShaderSource = R"(
#version 330 core 
layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 model;

void main() {
    gl_Position = projection * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core 

out vec4 FragColor;

void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

static unsigned int compileShader(unsigned int Shader, const char* shaderSource, const char* name) {
    int ok; char info[512];
    unsigned int shader = glCreateShader(Shader);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, 512, NULL, info);
        std::cout << name <<" COMPILATION::ERROR: " << info << std::endl;
    }
    return shader;
}

struct Engine {
    GLFWwindow* window;
    int WIDTH = 800, HEIGHT = 600;
    unsigned int shaderProgram = 0;

    Engine() {
        if (!glfwInit()) {
            std::cerr << "glfw died" << std::endl;
            exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "AI MOVEMENTS", nullptr, nullptr);
        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "Failed to initialize glad" << std::endl;
            exit(EXIT_FAILURE);
        }

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource, "VERTEX");
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "FRAGMENT");

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);
        {
            int ok; char log[512];
            glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
            if (!ok) { glGetProgramInfoLog(shaderProgram, 512, NULL, log); std::cout << "LINK ERROR: " << log << std::endl; }
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        glUseProgram(shaderProgram);
        glm::mat4 projection = glm::ortho(0.0f, (float)WIDTH, 0.0f, (float)HEIGHT, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    }

    void run() {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
};

struct Vertex {
    glm::vec3 pos;
};

std::vector<Vertex> generateCircles(float radius, int segments) {
    std::vector<Vertex> verts;
    verts.reserve(segments + 2);
    verts.push_back({glm::vec3(0.0f, 0.0f, 0.0f)});
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * 3.14159265f * float(i) / float(segments);
        float x = radius * std::cos(theta);
        float y = radius * std::sin(theta);

        verts.push_back({glm::vec3(x, y, 0.0f)});
    }

    return verts;
}

Engine engine;

struct Bone {
    glm::vec2 pos, vel;
    float angle, angleVel, halfLength, radius, mass;
    unsigned int VAO, VBO;
    int vertexCount;

    Bone(glm::vec2 pos, glm::vec2 vel, float a, float hf, float r, float m) : pos(pos), vel(vel), angle(a), halfLength(hf), radius(r), mass(m) {
        setupMesh();
    }

    void setupMesh() {
        // We create a generic capsule shape: a rectangle with two circles at the ends
        std::vector<Vertex> vertices;
        
        // 1. Rectangle Body
        // We use vertices relative to origin. The bone is drawn along the X-axis, then rotated by angle.
        vertices.push_back({glm::vec3(-halfLength, -radius, 0.0f)});
        vertices.push_back({glm::vec3( halfLength, -radius, 0.0f)});
        vertices.push_back({glm::vec3( halfLength,  radius, 0.0f)});
        
        vertices.push_back({glm::vec3(-halfLength, -radius, 0.0f)});
        vertices.push_back({glm::vec3( halfLength,  radius, 0.0f)});
        vertices.push_back({glm::vec3(-halfLength,  radius, 0.0f)});

        // 2. End Caps (Circles)
        int segments = 32;
        // Left Cap
        for(int i=0; i<segments; i++) {
            float t1 = 2.0f * 3.14159f * float(i) / float(segments);
            float t2 = 2.0f * 3.14159f * float(i+1) / float(segments);
            vertices.push_back({glm::vec3(-halfLength, 0.0f, 0.0f)}); // Center
            vertices.push_back({glm::vec3(-halfLength + radius*cos(t1), radius*sin(t1), 0.0f)});
            vertices.push_back({glm::vec3(-halfLength + radius*cos(t2), radius*sin(t2), 0.0f)});
        }
        // Right Cap
        for(int i=0; i<segments; i++) {
            float t1 = 2.0f * 3.14159f * float(i) / float(segments);
            float t2 = 2.0f * 3.14159f * float(i+1) / float(segments);
            vertices.push_back({glm::vec3(halfLength, 0.0f, 0.0f)}); // Center
            vertices.push_back({glm::vec3(halfLength + radius*cos(t1), radius*sin(t1), 0.0f)});
            vertices.push_back({glm::vec3(halfLength + radius*cos(t2), radius*sin(t2), 0.0f)});
        }

        vertexCount = vertices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw() {
        glBindVertexArray(VAO);
        
        glm::mat4 model = glm::mat4(1.0f);
        // Translate to position
        model = glm::translate(model, glm::vec3(pos, 0.0f));
        // Rotate by angle
        model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));

        glUniformMatrix4fv(glGetUniformLocation(engine.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    glm::vec2 getEnd() {
        return pos + glm::vec2(cos(angle), sin(angle)) * halfLength;
    }
    // Helper to get the world position of the "start" of the bone
    glm::vec2 getStart() {
        return pos - glm::vec2(cos(angle), sin(angle)) * halfLength;
    }
};

struct Joint {
    
};

int main() {

    Bone head({400, 450}, {0,0}, 0.0f, 5.0f, 20.0f, 1.0f); 
    
    // Chest (Vertical)
    Bone chest({400, 390}, {0,0}, -1.5708f, 30.0f, 10.0f, 1.0f); 
    
    // Torso (Vertical, connecting chest to hips)
    Bone torso({400, 320}, {0,0}, -1.5708f, 30.0f, 10.0f, 1.0f);

    // Arms (Horizontal)
    // Left Arm
    Bone lArm({320, 410}, {0,0}, 0.0f, 40.0f, 8.0f, 1.0f);
    // Right Arm
    Bone rArm({480, 410}, {0,0}, 0.0f, 40.0f, 8.0f, 1.0f);

    // Legs (Angled)
    // Left Leg (Angle down-left)
    Bone lLeg({360, 260}, {0,0}, -2.35619f, 50.0f, 10.0f, 1.0f);
    // Right Leg (Angle down-right)
    Bone rLeg({440, 260}, {0,0}, -0.785398f, 50.0f, 10.0f, 1.0f);

    std::vector<Bone*> bones = {&head, &chest, &torso, &lArm, &rArm, &lLeg, &rLeg};

    while (!glfwWindowShouldClose(engine.window)) {
        if (glfwGetKey(engine.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(engine.window, true);
        }

        engine.run();

        for (auto* b : bones) {
            b->draw();
        }
        glfwSwapBuffers(engine.window);                                      
        glfwPollEvents();
    }

    for (auto* b : bones) {
        glDeleteVertexArrays(1, &b->VAO);
        glDeleteBuffers(1, &b->VBO);
    }

    glfwTerminate();
    return 0;
}

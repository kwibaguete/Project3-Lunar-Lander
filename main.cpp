/**
* Author: Belinda Weng
* Assignment: Lunar Lander
* Date due: 2025-3-18, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"                // 4x4 Matrix
#include "glm/gtc/matrix_transform.hpp"  // Matrix transformation methods
#include "ShaderProgram.h"               // We'll talk about these later in the course
#include "Entity.h"
#include "stb_image.h"
#include <vector>
#include <iostream>
#include <ctime>
#include <cstdlib>  // For rand() and srand()
#include <string>

enum GameStatus { RUNNING, MISSION_FAILED, MISSION_ACCOMPLISHED };

// Our window dimensions
constexpr int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

// Background color components - space-like dark blue
constexpr float BG_RED = 0.0f,
BG_BLUE = 0.1f,
BG_GREEN = 0.2f,
BG_OPACITY = 1.0f;

// Our viewport—or our "camera"'s—position and dimensions
constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

// Our shader filepaths
constexpr char V_SHADER_PATH[] = "shaders/vertex.glsl",
F_SHADER_PATH[] = "shaders/fragment.glsl";

// Game constants
constexpr float GRAVITY = -0.05f;  
constexpr float ACCELERATION_X = 0.90f; // Horizontal acceleration
constexpr float ACCELERATION_Y = 0.95f; // Vertical acceleration (thrust)
constexpr float ROTATION_SPEED = 0.5f; 
constexpr float MAX_FUEL = 100.0f;
constexpr float FUEL_CONSUMPTION_RATE = 0.25f;
constexpr float MILLISECONDS_IN_SECOND = 1000.0;
constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
constexpr int PLATFORM_COUNT = 10;
constexpr int ASTEROID_COUNT = 3;
constexpr int FONTBANK_SIZE = 16; // Font sprite sheet is 16x16 characters
constexpr char FONT_FILEPATH[] = "font2.png";
float g_lander_rotation = 0.0f; // Rotation in degrees, 0 = pointing up

GameStatus g_game_status = RUNNING;
SDL_Window* g_display_window;
bool g_game_over = false;
bool g_game_started = false;

ShaderProgram g_shader_program;

glm::mat4 g_view_matrix,
g_model_matrix,
g_projection_matrix;

// Game objects
Entity* g_player;
std::vector<Entity*> g_platforms;
std::vector<Entity*> g_asteroids;
float g_fuel = MAX_FUEL;
float g_elapsed_time = 0.0f;
float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;
GLuint g_font_texture_id;

GLuint load_texture(const char* filepath) {
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL) {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return texture_id;
}

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text, float font_size, float spacing, glm::vec3 position) {
    // Scale the size of the fontbank in the UV-plane
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Arrays to hold the data for the vertices and texture coordinates
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character in the text
    for (int i = 0; i < text.size(); i++) {
        // Get the ASCII value of the character
        int spritesheet_index = (int)text[i];
        float offset = (font_size + spacing) * i;

        // Calculate the UV coordinates in the font texture
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // Add the vertices for the character (two triangles to form a quad)
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        // Add the texture coordinates for the character
        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // Create a model matrix for the text
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    // Set the model matrix and render the text
    program->set_model_matrix(model_matrix);

    // Bind the texture and set up the vertex attributes
    glBindTexture(GL_TEXTURE_2D, font_texture_id);

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());

    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    // Draw the text
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    // Disable the vertex attributes
    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void draw_lander(ShaderProgram* program, glm::vec3 position) {
    g_model_matrix = glm::mat4(1.0f);
    g_model_matrix = glm::translate(g_model_matrix, position);

    // Apply rotation - rotate around the Z axis
    g_model_matrix = glm::rotate(g_model_matrix, glm::radians(g_lander_rotation), glm::vec3(0.0f, 0.0f, 1.0f));

    program->set_model_matrix(g_model_matrix);

    // Set lander color (white)
    program->set_colour(1.0f, 1.0f, 1.0f, 1.0f);

    // Draw lander as a triangle
    float vertices[] = {
        0.0f, 0.5f,   // top
        -0.5f, -0.5f, // bottom left
        0.5f, -0.5f   // bottom right
    };

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->get_position_attribute());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(program->get_position_attribute());
}

// Function to draw a platform
void draw_platform(ShaderProgram* program, glm::vec3 position, float width, float height, bool is_landing_zone) {
    g_model_matrix = glm::mat4(1.0f);
    g_model_matrix = glm::translate(g_model_matrix, position);
    program->set_model_matrix(g_model_matrix);

    // Set platform color (green for landing zone, red for obstacles)
    if (is_landing_zone) {
        program->set_colour(0.0f, 1.0f, 0.0f, 1.0f); // Green
    }
    else {
        program->set_colour(1.0f, 0.0f, 0.0f, 1.0f); // Red
    }

    // Draw platform as a rectangle
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;

    float vertices[] = {
        -half_width, -half_height, // bottom left
        half_width, -half_height,  // bottom right
        half_width, half_height,   // top right

        -half_width, -half_height, // bottom left
        half_width, half_height,   // top right
        -half_width, half_height   // top left
    };

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->get_position_attribute());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(program->get_position_attribute());
}

void draw_asteroid(ShaderProgram* program, glm::vec3 position, float size) {
    g_model_matrix = glm::mat4(1.0f);
    g_model_matrix = glm::translate(g_model_matrix, position);
    program->set_model_matrix(g_model_matrix);

    // Set asteroid color (gray)
    program->set_colour(0.5f, 0.5f, 0.5f, 1.0f);

    // Draw asteroid as a simple square instead of a complex shape
    float half_size = size / 2.0f;

    float vertices[] = {
        -half_size, -half_size, // bottom left
        half_size, -half_size,  // bottom right
        half_size, half_size,   // top right

        -half_size, -half_size, // bottom left
        half_size, half_size,   // top right
        -half_size, half_size   // top left
    };

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->get_position_attribute());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(program->get_position_attribute());
}

void draw_fuel_gauge(ShaderProgram* program, float fuel_level) {
    g_model_matrix = glm::mat4(1.0f);
    g_model_matrix = glm::translate(g_model_matrix, glm::vec3(-4.5f, 3.5f, 0.0f));
    program->set_model_matrix(g_model_matrix);

    // Draw fuel background (gray)
    program->set_colour(0.3f, 0.3f, 0.3f, 1.0f);

    float bg_vertices[] = {
        0.0f, 0.0f,    // bottom left
        3.0f, 0.0f,    // bottom right
        3.0f, 0.3f,    // top right

        0.0f, 0.0f,    // bottom left
        3.0f, 0.3f,    // top right
        0.0f, 0.3f     // top left
    };

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, bg_vertices);
    glEnableVertexAttribArray(program->get_position_attribute());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(program->get_position_attribute());

    // Draw fuel level (yellow)
    program->set_colour(1.0f, 1.0f, 0.0f, 1.0f);

    float fuel_width = (fuel_level / MAX_FUEL) * 3.0f;

    float fuel_vertices[] = {
        0.0f, 0.0f,        // bottom left
        fuel_width, 0.0f,  // bottom right
        fuel_width, 0.3f,  // top right

        0.0f, 0.0f,        // bottom left
        fuel_width, 0.3f,  // top right
        0.0f, 0.3f         // top left
    };

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, fuel_vertices);
    glEnableVertexAttribArray(program->get_position_attribute());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(program->get_position_attribute());
}

void initialise()
{
    // Seed random number generator
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // HARD INITIALISE
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Lunar Lander",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    if (g_display_window == nullptr)
    {
        std::cerr << "ERROR: SDL Window could not be created.\n";
        g_game_status = MISSION_FAILED;

        SDL_Quit();
        exit(1);
    }

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    // SOFT INITIALISE
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // Load up our shaders
    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    // Initialise our view, model, and projection matrices
    g_view_matrix = glm::mat4(1.0f);
    g_model_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    // Load font texture
    g_font_texture_id = load_texture(FONT_FILEPATH);

    // Initialize player (lander)
    g_player = new Entity();
    g_player->set_position(glm::vec3(0.0f, 3.0f, 0.0f));
    g_player->set_acceleration(glm::vec3(0.0f, GRAVITY, 0.0f)); // Initial acceleration is just gravity
    g_player->set_width(0.5f);  // Smaller hitbox for better gameplay
    g_player->set_height(0.5f);
    g_player->set_entity_type(PLAYER);

    // Initialize platforms
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        Entity* platform = new Entity();
        platform->set_position(glm::vec3(-4.75f + (i * 1.0f), -3.5f, 0.0f));
        platform->set_width(0.5f);
        platform->set_height(0.2f);
        platform->set_entity_type(PLATFORM);
        g_platforms.push_back(platform);
    }

    // Add some asteroids (obstacles)
    for (int i = 0; i < ASTEROID_COUNT; i++) {
        float randomX = -4.0f + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / 8.0f));
        float randomY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 3.0f - 1.0f;

        Entity* asteroid = new Entity();
        asteroid->set_position(glm::vec3(randomX, randomY, 0.0f));
        asteroid->set_width(0.3f);
        asteroid->set_height(0.3f);
        asteroid->set_entity_type(ENEMY);
        g_asteroids.push_back(asteroid);
    }

    // Game is started by default now
    g_game_started = true;

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
}

void process_input()
{
    // Reset player movement
    g_player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_status = MISSION_FAILED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                g_game_status = MISSION_FAILED;
                break;
            case SDLK_r:
                if (g_game_over) {
                    // Reset game state
                    g_game_over = false;
                    g_game_status = RUNNING;
                    g_fuel = MAX_FUEL;
                    g_player->set_position(glm::vec3(0.0f, 3.0f, 0.0f));
                    g_player->set_velocity(glm::vec3(0.0f, 0.0f, 0.0f));
                    g_player->set_acceleration(glm::vec3(0.0f, GRAVITY, 0.0f));
                    g_lander_rotation = 0.0f; // Reset rotation
                }
                break;
            case SDLK_SPACE:
                // Start the game when space is pressed
                g_game_started = true;
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }

    // If game is over, don't process movement inputs
    if (g_game_over) return;

    // Get keyboard state
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    // Reset acceleration to just gravity
    g_player->set_acceleration(glm::vec3(0.0f, GRAVITY, 0.0f));

    if (g_game_started) {
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
            if (g_fuel > 0) {
                // IMPORTANT: Don't directly set movement, set acceleration instead
                glm::vec3 current_acc = g_player->get_acceleration();
                g_player->set_acceleration(glm::vec3(current_acc.x - ACCELERATION_X, current_acc.y, 0.0f));

                // Consume fuel
                g_fuel -= FUEL_CONSUMPTION_RATE * FIXED_TIMESTEP;

                // Rotate lander slightly to indicate direction
                g_lander_rotation = 15.0f;
            }
        }
        else if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
            if (g_fuel > 0) {
                // IMPORTANT: Don't directly set movement, set acceleration instead
                glm::vec3 current_acc = g_player->get_acceleration();
                g_player->set_acceleration(glm::vec3(current_acc.x + ACCELERATION_X, current_acc.y, 0.0f));

                // Consume fuel
                g_fuel -= FUEL_CONSUMPTION_RATE * FIXED_TIMESTEP;

                // Rotate lander slightly to indicate direction
                g_lander_rotation = -15.0f;
            }
        }
        else {
            // Reset rotation when not moving horizontally
            g_lander_rotation = 0.0f;
        }

        // Apply thrust with W key or UP arrow (upward movement)
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            if (g_fuel > 0) {
                // Apply upward thrust by adding to the current acceleration
                glm::vec3 current_acc = g_player->get_acceleration();
                g_player->set_acceleration(glm::vec3(current_acc.x, current_acc.y + ACCELERATION_Y, 0.0f));
                g_fuel -= FUEL_CONSUMPTION_RATE * FIXED_TIMESTEP;
            }
        }
    }

    // Ensure fuel doesn't go below 0
    if (g_fuel < 0) g_fuel = 0;
}

void update() {
    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_time_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        // If game is over, don't update physics
        if (!g_game_over && g_game_started) {
            glm::vec3 current_velocity = g_player->get_velocity();
            glm::vec3 acceleration = g_player->get_acceleration();

            // Apply acceleration to velocity
            current_velocity += acceleration * FIXED_TIMESTEP;

            // Apply a small amount of damping to horizontal velocity for better control
            const float HORIZONTAL_DAMPING = 0.995f;
            current_velocity.x *= HORIZONTAL_DAMPING;

            // Update player's velocity
            g_player->set_velocity(current_velocity);

            // Update position based on velocity
            glm::vec3 current_position = g_player->get_position();
            current_position += current_velocity * FIXED_TIMESTEP;
            g_player->set_position(current_position);

            // Check for collisions with platforms
            for (Entity* platform : g_platforms) {
                if (g_player->check_collision(platform)) {
                    
                    glm::vec3 velocity = g_player->get_velocity();

                    
                    if (platform == g_platforms[0] && fabs(velocity.y) < 0.5f && fabs(velocity.x) < 0.3f) {
                        g_game_status = MISSION_ACCOMPLISHED;
                        g_game_over = true;
                    }
                    else {
                        // Collision with any platform at high speed or with obstacles
                        g_game_status = MISSION_FAILED;
                        g_game_over = true;
                    }
                }
            }

            // Check for collisions with asteroids
            for (Entity* asteroid : g_asteroids) {
                if (g_player->check_collision(asteroid)) {
                    g_game_status = MISSION_FAILED;
                    g_game_over = true;
                }
            }

            // Check if player is out of bounds
            glm::vec3 position = g_player->get_position();
            if (position.y < -3.75f || position.x < -5.0f || position.x > 5.0f) {
                g_game_status = MISSION_FAILED;
                g_game_over = true;
            }
        }

        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Render platforms
    for (size_t i = 0; i < g_platforms.size(); i++) {
        draw_platform(&g_shader_program,
            g_platforms[i]->get_position(),
            g_platforms[i]->get_width(),
            g_platforms[i]->get_height(),
            i == 0); // First platform is the landing zone
    }

    // Render asteroids
    for (Entity* asteroid : g_asteroids) {
        draw_asteroid(&g_shader_program,
            asteroid->get_position(),
            asteroid->get_width());
    }

    // Render player
    draw_lander(&g_shader_program, g_player->get_position());

    // Render fuel gauge
    draw_fuel_gauge(&g_shader_program, g_fuel);

    // Render game status messages if game is over
    if (g_game_over) {
        g_shader_program.set_colour(1.0f, 1.0f, 1.0f, 1.0f);

        if (g_game_status == MISSION_ACCOMPLISHED) {
            // Draw mission accomplished message
            draw_text(&g_shader_program, g_font_texture_id, "MISSION ACCOMPLISHED", 0.5f, 0.05f, glm::vec3(-4.0f, 0.0f, 0.0f));
        }
        else if (g_game_status == MISSION_FAILED) {
            // Draw mission failed message
            draw_text(&g_shader_program, g_font_texture_id, "MISSION FAILED", 0.5f, 0.05f, glm::vec3(-3.0f, 0.0f, 0.0f));
        }
    }

    SDL_GL_SwapWindow(g_display_window);
}

void shutdown() {
    // Clean up entities
    delete g_player;

    for (Entity* platform : g_platforms) {
        delete platform;
    }
    g_platforms.clear();

    for (Entity* asteroid : g_asteroids) {
        delete asteroid;
    }
    g_asteroids.clear();

    SDL_Quit();
}

int main(int argc, char* argv[])
{
    initialise();

    while (g_game_status == RUNNING)
    {
        process_input();
        update();
        render();
    }

    // Continue rendering even after game over
    while (g_game_status != RUNNING)
    {
        process_input();
        render();

        // Check for quit events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT ||
                event.type == SDL_WINDOWEVENT_CLOSE ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q))
            {
                g_game_status = RUNNING; // This will exit the loop
                break;
            }

            // Allow restart with R key
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r)
            {
                g_game_over = false;
                g_game_status = RUNNING;
                g_fuel = MAX_FUEL;
                g_player->set_position(glm::vec3(0.0f, 3.0f, 0.0f));
                g_player->set_velocity(glm::vec3(0.0f, 0.0f, 0.0f));
                g_player->set_acceleration(glm::vec3(0.0f, GRAVITY, 0.0f));
                g_lander_rotation = 0.0f;
                break;
            }
        }
    }

    shutdown();
    return 0;
}


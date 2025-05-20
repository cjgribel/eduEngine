#ifndef Game_hpp
#define Game_hpp
#pragma once

#include <entt/fwd.hpp>
#include "GameBase.h"
#include "RenderableMesh.hpp"
#include "ForwardRenderer.hpp"
#include "ShapeRenderer.hpp"

// #define SPONZA_PATH "assets/crytek-sponza_hansen/sponza.obj"
#define CHARACTER_PATH "assets/Ultimate Platformer Pack/Character/Character.fbx"
#define CHARACTER_ANIM 0
#define ENEMY_PATH "assets/Ultimate Platformer Pack/Enemies/Enemy.fbx"
#define ENEMY_ANIM 0
#define EXORED_PATH "assets/ExoRed/exo_red.fbx"
#define EXORED_ANIM 0
//#define EVE_PATH "assets/Eve/Eve By J.Gonzales.fbx"
#define EVE_ANIM -1
#define MANNEQUIN_PATH "assets/UEMannequin/SK_Mannequin_tex.FBX"
#define MANNEQUIN_ANIM -1
#define UE5QUINN_PATH "assets/UE5_Mannequin/SKM_Quinn.fbx"
#define UE5QUINN_ANIM1_PATH "assets/UE5_Mannequin/MF_Idle.fbx"
#define UE5QUINN_ANIM 0

/// @brief A Game may hold, update and render 3D geometry and GUI elements
class Game : public eeng::GameBase
{
public:
    /// @brief For game resource initialization
    /// @return 
    bool init() override;

    /// @brief General update method that is called each frame
    /// @param time Total time elapsed in seconds
    /// @param deltaTime Time elapsed since the last frame
    /// @param input Input from mouse, keyboard and controllers
    void update(
        float time,
        float deltaTime,
        InputManagerPtr input) override;

    /// @brief For rendering of game contents
    /// @param time Total time elapsed in seconds
    /// @param screenWidth Current width of the window in pixels
    /// @param screenHeight Current height of the window in pixels
    void render(
        float time,
        int windowWidth,
        int windowHeight) override;

    /// @brief For destruction of game resources
    void destroy() override;

private:
    /// @brief For rendering of GUI elements
    void renderUI();

    // Renderer for rendering imported animated or non-animated models
    eeng::ForwardRendererPtr forwardRenderer;

    // Immediate-mode renderer for basic 2D or 3D primitives
    ShapeRendererPtr shapeRenderer;

    // Entity registry - to use in labs
    std::shared_ptr<entt::registry> entity_registry;

    // Matrices for view, projection and viewport
    struct Matrices
    {
        glm::mat4 V;
        glm::mat4 P;
        glm::mat4 VP;
        glm::ivec2 windowSize;
    } matrices;

    // Basic third-person camera
    struct Camera
    {
        glm::vec3 lookAt = glm_aux::vec3_000;   // Point of interest
        glm::vec3 up = glm_aux::vec3_010;       // Local up-vector
        float distance = 15.0f;                 // Distance to point-of-interest
        float sensitivity = 0.005f;             // Mouse sensitivity
        const float nearPlane = 1.0f;           // Rendering near plane
        const float farPlane = 500.0f;          // Rendering far plane

        // Position and view angles (computed when camera is updated)
        float yaw = 0.0f;                       // Horizontal angle (radians)
        float pitch = -glm::pi<float>() / 8;    // Vertical angle (radians)
        glm::vec3 pos;                          // Camera position

        // Previous mouse position
        glm::ivec2 mouse_xy_prev{ -1, -1 };
    } camera;

    // Light properties
    struct PointLight
    {
        glm::vec3 pos;
        glm::vec3 color{ 1.0f, 1.0f, 0.8f };
    } pointlight;

    // (Placeholder) Player data
    struct Player
    {
        glm::vec3 pos = glm_aux::vec3_000;
        float velocity{ 6.0f };

        // Local vectors & view ray (computed when camera/player is updated)
        glm::vec3 fwd, right;
        glm_aux::Ray viewRay;
    } player;

    // Game meshes
    std::shared_ptr<eeng::RenderableMesh> grassMesh, horseMesh, characterMesh;
#ifdef SPONZA_PATH
    std::shared_ptr<eeng::RenderableMesh> sponzaMesh;
#endif
#ifdef CHARACTER_PATH
    std::shared_ptr<eeng::RenderableMesh> qcharacterMesh;
#endif
#ifdef ENEMY_PATH
    std::shared_ptr<eeng::RenderableMesh> enemyMesh;
#endif
#ifdef EXORED_PATH
    std::shared_ptr<eeng::RenderableMesh> exoredMesh;
#endif
#ifdef EVE_PATH
    std::shared_ptr<eeng::RenderableMesh> eveMesh;
#endif
#ifdef MANNEQUIN_PATH
    std::shared_ptr<eeng::RenderableMesh> mannequinMesh;
#endif
#ifdef UE5QUINN_PATH
    std::shared_ptr<eeng::RenderableMesh> ue5quinnMesh;
#endif

    // Game entity transformations
    glm::mat4 characterWorldMatrix1, characterWorldMatrix2, characterWorldMatrix3;
    glm::mat4 grassWorldMatrix, horseWorldMatrix;

    // Game entity AABBs (for collision detection or visualization)
    eeng::AABB character_aabb1, character_aabb2, character_aabb3, horse_aabb, grass_aabb;

    // Placeholder animation state
    int characterAnimIndex = -1;
    float characterAnimSpeed = 1.0f;
    float characterAnimBlend = 0.5f;

    // Stats
    int drawcallCount = 0;

    /// @brief Placeholder system for updating the camera position based on inputs
    /// @param input Input from mouse, keyboard and controllers
    void updateCamera(
        InputManagerPtr input);

    /// @brief Placeholder system for updating the 'player' based on inputs
    /// @param deltaTime 
    void updatePlayer(
        float deltaTime,
        InputManagerPtr input);
};

#endif
#ifndef Scene_hpp
#define Scene_hpp
#pragma once

#include <entt/fwd.hpp>
#include "SceneBase.h"
#include "RenderableMesh.hpp"
#include "ForwardRenderer.hpp"
#include "ShapeRenderer.hpp"

/// @brief A Scene may hold, update and render 3D geometry and GUI elements
class Scene : public eeng::SceneBase
{
public:
    /// @brief For scene resource initialization
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

    /// @brief For rendering of GUI elements
    void renderUI() override;

    /// @brief For rendering of scene contents
    /// @param time Total time elapsed in seconds
    /// @param screenWidth Current width of the window in pixels
    /// @param screenHeight Current height of the window in pixels
    void render(
        float time,
        int windowWidth,
        int windowHeight) override;

    /// @brief For destruction of scene resources
    void destroy() override;

private:

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

    // Scene meshes
    std::shared_ptr<eeng::RenderableMesh> grassMesh, horseMesh, characterMesh;

    // Scene entity transformations
    glm::mat4 characterWorldMatrix1, characterWorldMatrix2, characterWorldMatrix3;
    glm::mat4 grassWorldMatrix, horseWorldMatrix;

    // Scene entity AABBs
    eeng::AABB character_aabb1, character_aabb2, character_aabb3, horse_aabb, grass_aabb;

    int characterAnimIndex = -1;
    float characterAnimSpeed = 1.0f;
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
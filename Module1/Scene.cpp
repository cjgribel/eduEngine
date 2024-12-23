#include "glmcommon.h"
#include "imgui.h"
#include "Scene.hpp"

bool Scene::init()
{
    // Do some entt stuff
    entt::registry registry;
    auto ent1 = registry.create();
    struct Tfm
    {
        float x, y, z;
    };
    registry.emplace<Tfm>(ent1, Tfm{});

    // Grass
    grassMesh = std::make_shared<eeng::RenderableMesh>();
    grassMesh->load("assets/grass/grass_trees_merged2.fbx", false);

    // Horse
    horseMesh = std::make_shared<eeng::RenderableMesh>();
    horseMesh->load("assets/Animals/Horse.fbx", false);

    // Character
    characterMesh = std::make_shared<eeng::RenderableMesh>();
#if 0
    // Sponza
    characterMesh->load("/Users/ag1498/Dropbox/MAU/DA307A-CGM/Rendering/eduRend_2022/assets/crytek-sponza/sponza.obj", false);
#endif
#if 0
    // Character
    characterMesh->load("assets/Ultimate Platformer Pack/Character/Character.fbx", false);
#endif
#if 0
    // Enemy
    characterMesh->load("assets/Ultimate Platformer Pack/Enemies/Bee.fbx", false);
#endif
#if 0
    // Dragon
    // Requires MaxBones = 151;
    // https://sketchfab.com/3d-models/tarisland-dragon-high-poly-ecf63885166c40e2bbbcdf11cd14e65f
    // characterMesh->load("assets/tarisland-dragon-high-poly/M_B_44_Qishilong_skin_Skeleton.FBX");
#endif
#if 0
    // ExoRed 5.0.1 PACK FBX, 60fps, No keyframe reduction
    characterMesh->load("assets/ExoRed/exo_red.fbx");
    characterMesh->load("assets/ExoRed/idle (2).fbx", true);
    characterMesh->load("assets/ExoRed/walking.fbx", true);
    // Remove root motion
    characterMesh->remove_translation_keys("mixamorig:Hips");
#endif
#if 1
    // Amy 5.0.1 PACK FBX
    characterMesh->load("assets/Amy/Ch46_nonPBR.fbx");
    characterMesh->load("assets/Amy/idle.fbx", true);
    characterMesh->load("assets/Amy/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 0
    // Eve 5.0.1 PACK FBX
    characterMesh->load("assets/Eve/Eve By J.Gonzales.fbx");
    characterMesh->load("assets/Eve/idle.fbx", true);
    characterMesh->load("assets/Eve/walking.fbx", true);
    // Remove root motion
    characterMesh->remove_translation_keys("mixamorig:Hips");
#endif

    shapeRenderer.init(); // 

    return true;
}

void Scene::update(float time_s, float deltaTime_s)
{
    lightPos = glm::vec3(TRS(
        { 1000.0f, 1000.0f, 1000.0f },
        time_s * 0.0f,
        { 0.0f, 1.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Position of camera/eye
    eyePos = glm::vec3(0.0f, 5.0f, 10.0f);

    // Position to look at
    atPos = glm::vec3(0.0f, 0.0f, 0.0f);

    grassWorldMatrix = TRS(
        { 0.0f, 0.0f, 0.0f },
        0.0f,
        { 0, 1, 0 },
        { 100.0f, 100.0f, 100.0f });

    horseWorldMatrix = TRS(
        { 30.0f, 0.0f, -35.0f },
        35.0f,
        { 0, 1, 0 },
        { 0.01f, 0.01f, 0.01f });

    characterWorldMatrix1 = TRS(
        { 0, 0, 0 },
        time_s * 50.0f,
        { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });

    characterWorldMatrix2 = TRS(
        { -3, 0, 0 },
        0.0f,
        { 0, 1, 0 },
        { 1.0f, 1.0f, 1.0f }) * characterWorldMatrix1;

    characterWorldMatrix3 = TRS(
        { 6, 0, 0 },
        0.0f,
        { 0, 1, 0 },
        { 1.0f, 1.0f, 1.0f }) * characterWorldMatrix2;
}

void Scene::renderUI()
{
    ImGui::Text("Drawcall count %i", drawcallCount);

    if (ImGui::ColorEdit3("Light color",
        glm::value_ptr(lightColor),
        ImGuiColorEditFlags_NoInputs))
    {
    }

    // Combo (drop-down) for animation clip
    if (characterMesh)
    {
        int curAnimIndex = characterAnimIndex;
        std::string label = (curAnimIndex == -1 ? "Bind pose" : characterMesh->getAnimationName(curAnimIndex));
        if (ImGui::BeginCombo("Character animation##animclip", label.c_str()))
        {
            // Bind pose item
            const bool isSelected = (curAnimIndex == -1);
            if (ImGui::Selectable("Bind pose", isSelected))
                curAnimIndex = -1;
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            // Clip items
            for (int i = 0; i < characterMesh->getNbrAnimations(); i++)
            {
                const bool isSelected = (curAnimIndex == i);
                const auto label = characterMesh->getAnimationName(i) + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), isSelected))
                    curAnimIndex = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
            characterAnimIndex = curAnimIndex;
        }
    }

    ImGui::SliderFloat("Animation speed", &characterAnimSpeed, 0.1f, 5.0f);
}

void Scene::render(
    float time_s,
    int screenWidth,
    int screenHeight,
    eeng::ForwardRendererPtr renderer)
{
    // int ANIM_INDEX = -1;
    // float ANIM_SPEED = 1.0f;
    // glm::vec3 LIGHT_COLOR{ 1.0f, 1.0f, 1.0f };

    // If we want to draw AABBs
    eeng::AABB character_aabb1, character_aabb2, character_aabb3, horse_aabb, grass_aabb;

    // Projection matrix
    const float aspectRatio = float(screenWidth) / screenHeight;
    const glm::mat4 P = glm::perspective(glm::radians(60.0f), aspectRatio, nearPlane, farPlane);

    // View matrix
    //glm::mat4 V = glm::inverse(TRS(eyePos, 0.0f, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }));
    const glm::mat4 V = glm::lookAt(eyePos, atPos, upVector);

    // Compute world ray from window position (e.g. mouse), to use for something perhaps ...
    glm::vec4 viewport = { 0, 0, screenWidth, screenHeight };
    glm::vec2 mousePos{ screenWidth / 2, screenHeight / 2 }; // placeholder
    auto [rayOrigin, rayDirection] = ComputeWorldSpaceRay(mousePos, V, P, viewport);
    //std::cout << "rayOrigin " << vec3ToString(rayOrigin) << ")\n";
    //std::cout << "rayDirection " << vec3ToString(rayDirection) << ")\n";

    // Begin rendering pass
    renderer->beginPass(P, V, lightPos, lightColor, eyePos);

    // Grass
    renderer->renderMesh(grassMesh, grassWorldMatrix);
    grass_aabb = grassMesh->m_model_aabb.post_transform(grassWorldMatrix);

    // Horse
    horseMesh->animate(3, time_s);
    renderer->renderMesh(horseMesh, horseWorldMatrix);
    horse_aabb = horseMesh->m_model_aabb.post_transform(horseWorldMatrix);

    // Character, instance 1
    characterMesh->animate(characterAnimIndex, time_s * characterAnimSpeed);
    renderer->renderMesh(characterMesh, characterWorldMatrix1);
    character_aabb1 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix1);

    // Character, instance 2
    characterMesh->animate(1, time_s * characterAnimSpeed);
    renderer->renderMesh(characterMesh, characterWorldMatrix2);
    character_aabb2 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix2);

    // Character, instance 3
    characterMesh->animate(2, time_s * characterAnimSpeed);
    renderer->renderMesh(characterMesh, characterWorldMatrix3);
    character_aabb3 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix3);

    // End rendering pass
    drawcallCount = renderer->endPass();

    //
    glm::vec3 p0{ 0.0f, 0.0f, 0.0f }, p1{ 10.0f, 10.0f, 0.0f };
    shapeRenderer.push_line(p0, p1);

    shapeRenderer.push_basis_basic(characterWorldMatrix1, 1.0f);
    shapeRenderer.push_basis_basic(characterWorldMatrix2, 1.0f);
    shapeRenderer.push_basis_basic(characterWorldMatrix3, 1.0f);
    shapeRenderer.push_basis_basic(grassWorldMatrix, 1.0f);
    shapeRenderer.push_basis_basic(horseWorldMatrix, 1.0f);

    {
        // const ShapeRendering::ArrowDescriptor arrowdesc
        // {
        //     .cone_fraction = 0.2,
        //     .cone_radius = 0.05f,
        //     .cylinder_radius = 0.025f
        // };
        const auto arrowdesc = ShapeRendering::ArrowDescriptor
        {
            .cone_fraction = 0.2,
            .cone_radius = 0.15f,
            .cylinder_radius = 0.075f
        };
        shapeRenderer.push_basis(grassWorldMatrix, 1.0f, arrowdesc);
    }

    // Draw AABBs
    shapeRenderer.push_states(ShapeRendering::Color4u{ 0xFFE61A80 });
    shapeRenderer.push_AABB(character_aabb1.min, character_aabb1.max);
    shapeRenderer.push_AABB(character_aabb2.min, character_aabb2.max);
    shapeRenderer.push_AABB(character_aabb3.min, character_aabb3.max);
    shapeRenderer.push_AABB(horse_aabb.min, horse_aabb.max);
    shapeRenderer.push_AABB(grass_aabb.min, grass_aabb.max);
    shapeRenderer.pop_states<ShapeRendering::Color4u>();

    // Push quads
    {
        glm::vec3 points[4]{ {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f} };
        shapeRenderer.push_states(ShapeRendering::Color4u{ 0x8000ffff });
        shapeRenderer.push_states(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f)));

        shapeRenderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        shapeRenderer.push_quad(points, glm::vec3(0.0f, 0.0f, 1.0f));

        shapeRenderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        shapeRenderer.push_quad_wireframe();

        shapeRenderer.pop_states<ShapeRendering::Color4u, glm::mat4, glm::mat4, glm::mat4>();
    }

    // Push cube
    {
        shapeRenderer.push_states(ShapeRendering::Color4u{ 0x8000ffff });
        shapeRenderer.push_states(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f)));

        shapeRenderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f)));
        shapeRenderer.push_cube();

        shapeRenderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        shapeRenderer.push_cube_wireframe();

        shapeRenderer.pop_states<ShapeRendering::Color4u, glm::mat4, glm::mat4, glm::mat4>();
    }

    // Push cube
    {
        shapeRenderer.push_states(ShapeRendering::Color4u{ 0xff808080 });
        shapeRenderer.push_grid(glm::vec3(0.0f, 1.0e-6f, 0.0f), 20.0f, 21);
        shapeRenderer.pop_states<ShapeRendering::Color4u>();
    }

    // Arrows
    {
        const auto arrowdesc = ShapeRendering::ArrowDescriptor
        {
            .cone_fraction = 0.2,
            .cone_radius = 0.15f,
            .cylinder_radius = 0.075f
        };
        shapeRenderer.push_basis(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f)), 1.0f, arrowdesc);
    }

    shapeRenderer.render(P * V);
    shapeRenderer.post_render();
}

void Scene::destroy()
{

}
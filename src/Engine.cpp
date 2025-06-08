// Licensed under the MIT License. See LICENSE file for details.

#include "Engine.hpp"
#include "glcommon.h"
#ifdef EENG_GLVERSION_43
#include "GLDebugMessageCallback.h"
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <memory>

#include "InputManager.hpp"
#include "Log.hpp"
#include "MetaReg.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng {

    Engine::Engine()
    {
    }

    Engine::~Engine()
    {
        shutdown();
    }

    bool Engine::init(const char* title, int width, int height)
    {
        window_width = width;
        window_height = height;

        if (!init_sdl(title, width, height))
            return false;

        if (!init_opengl())
            return false;

        if (!init_imgui())
            return false;

        // Log some info about the OpenGL context
        LOG_DEFINES(eeng::Log);
        {
            int glMinor, glMajor;
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glMinor);
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &glMajor);
            eeng::Log("GL version %i.%i (requested), %i.%i (actual)", EENG_GLVERSION_MAJOR, EENG_GLVERSION_MINOR, glMajor, glMinor);
        }

        // Log some info about the MSAA settings
#ifdef EENG_MSAA
        {
            int actualMSAA;
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &actualMSAA);
            eeng::Log("MSAA %i (requested), %i (actual)", EENG_MSAA_SAMPLES, actualMSAA);
        }
#endif

        // Log some info about the anisotropic filtering settings
#ifdef EENG_ANISO
        {
            GLfloat maxAniso;
#if defined(EENG_GLVERSION_43)
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
#elif defined(EENG_GLVERSION_41)
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
#endif
            eeng::Log("Anisotropic samples %i (requested), %i (max))", EENG_ANISO_SAMPLES, (int)maxAniso);
        }
#endif

        input = std::make_shared<eeng::InputManager>();

        register_meta_types();

        eeng::Log("Engine initialized successfully.");
        return true;
    }

    void Engine::run(std::unique_ptr<GameBase> game)
    {
        game->init();

        bool running = true;
        float time_s = 0.0f, time_ms, deltaTime_s = 0.016f;

        eeng::Log("Entering main loop...");
        while (running)
        {
            const auto now_ms = SDL_GetTicks();
            const auto now_s = now_ms * 0.001f;
            deltaTime_s = now_s - time_s;
            time_ms = now_ms;
            time_s = now_s;

            process_events(running);
            begin_frame();

            game->update(time_s, deltaTime_s, input);
            game->render(time_s, window_width, window_height);

            end_frame();

            SDL_GL_SwapWindow(window_);

            // Add a delay if frame time was shorter than the target frame time
            const Uint32 elapsed_ms = SDL_GetTicks() - time_ms;
            if (elapsed_ms < min_frametime_ms)
                SDL_Delay(min_frametime_ms - elapsed_ms);
        }

        game->destroy();
    }

    void Engine::shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        if (gl_context_)
            SDL_GL_DeleteContext(gl_context_);
        if (window_)
            SDL_DestroyWindow(window_);

        SDL_Quit();
    }

    bool Engine::init_sdl(const char* title, int width, int height)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        {
            eeng::Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, EENG_GLVERSION_MAJOR);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, EENG_GLVERSION_MINOR);
#ifdef EENG_MSAA
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, EENG_MSAA_SAMPLES);
#endif

        window_ = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!window_)
        {
            eeng::Log("Failed to create SDL window: %s", SDL_GetError());
            return false;
        }

        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_)
        {
            eeng::Log("Failed to create GL context: %s", SDL_GetError());
            return false;
        }

        SDL_GL_MakeCurrent(window_, gl_context_);
        SDL_GL_SetSwapInterval(vsync);

        return true;
    }

    bool Engine::init_opengl()
    {
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            eeng::Log("GLEW initialization failed: %s", glewGetErrorString(err));
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        return true;
    }

    bool Engine::init_imgui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_) ||
            !ImGui_ImplOpenGL3_Init("#version 410 core"))
        {
            eeng::Log("Failed to initialize ImGui.");
            return false;
        }

        return true;
    }

    void Engine::process_events(bool& running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);

            // Skip mouse events if ImGui is capturing mouse input.
            if ((event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP) &&
                ImGui::GetIO().WantCaptureMouse)
            {
                continue;
            }

            // Skip keyboard events if ImGui is capturing keyboard input.
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                ImGui::GetIO().WantCaptureKeyboard)
            {
                continue;
            }

            input->HandleEvent(&event);

            if (event.type == SDL_QUIT)
                running = false;
        }
    }

    void Engine::begin_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window_);
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        render_info_UI();

        eeng::LogDraw("Log");

        // Set up OpenGL state:

        // Face culling - takes place before rasterization
        glEnable(GL_CULL_FACE); // Perform face culling
        glFrontFace(GL_CCW);    // Define winding for a front-facing face
        glCullFace(GL_BACK);    // Cull back-facing faces

        // Rasterization stuff
        glEnable(GL_DEPTH_TEST); // Perform depth test when rasterizing
        glDepthFunc(GL_LESS);    // Depth test pass if z < existing z (closer than existing z)
        glDepthMask(GL_TRUE);    // If depth test passes, write z to z-buffer
        glDepthRange(0, 1);      // Z-buffer range is [0,1], where 0 is at z-near and 1 is at z-far

        // Define viewport transform = Clip -> Screen space (applied before rasterization)
        glViewport(0, 0, window_width, window_height);

        // Bind the default framebuffer (only needed when using multiple render targets)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear depth and color attachments of frame buffer
        glClearColor(0.529f, 0.808f, 0.922f, 1.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Wireframe rendering
        if (wireframe_mode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }
    }

    void Engine::end_frame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void Engine::render_info_UI()
    {
        // Start a ImGui window
        ImGui::Begin("Engine Info");

        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Mouse state
            auto mouse = input->GetMouseState();
            ImGui::Text("Mouse pos (%i, %i) %s%s",
                mouse.x,
                mouse.y,
                mouse.leftButton ? "L" : "",
                mouse.rightButton ? "R" : "");

            // Framerate
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            // Combo (drop-down) for fps settings
            static const char* items[] = { "10", "30", "60", "120", "Uncapped" };
            static int currentItem = 2;
            if (ImGui::BeginCombo("FPS cap##targetfps", items[currentItem]))
            {
                for (int i = 0; i < IM_ARRAYSIZE(items); i++)
                {
                    const bool isSelected = (currentItem == i);
                    if (ImGui::Selectable(items[i], isSelected))
                        currentItem = i;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (currentItem == 0)
                min_frametime_ms = 1000.0f / 10;
            else if (currentItem == 1)
                min_frametime_ms = 1000.0f / 30;
            else if (currentItem == 2)
                min_frametime_ms = 1000.0f / 60;
            else if (currentItem == 3)
                min_frametime_ms = 1000.0f / 120;
            else if (currentItem == 4)
                min_frametime_ms = 0.0f;

            if (ImGui::Checkbox("V-Sync", &vsync))
            {
                SDL_GL_SetSwapInterval(vsync);
            }

            ImGui::SameLine();
            ImGui::Checkbox("Wireframe rendering", &wireframe_mode);

            // if (SOUND_PLAY)
            // {
            //     if (ImGui::Button("Pause sound"))
            //     {
            //         SDL_PauseAudioDevice(deviceId, 1);
            //         SOUND_PLAY = false;
            //     }
            // }
            // else
            // {
            //     if (ImGui::Button("Play sound"))
            //     {
            //         SDL_PauseAudioDevice(deviceId, 0);
            //         SOUND_PLAY = true;
            //     }
            // }
        }

        if (ImGui::CollapsingHeader("Controllers", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Controllers connected: %i", input->GetConnectedControllerCount());

            for (auto& [id, state] : input->GetControllers())
            {
                ImGui::PushID(id);
                ImGui::BeginChild("Controller", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 6), true);

                ImGui::Text("Controller %i: '%s'", id, state.name.c_str());
                ImGui::Text("Left stick:  X: %.2f  Y: %.2f", state.axisLeftX, state.axisLeftY);
                ImGui::Text("Right stick: X: %.2f  Y: %.2f", state.axisRightX, state.axisRightY);
                ImGui::Text("Triggers:    L: %.2f  R: %.2f", state.triggerLeft, state.triggerRight);
                std::string buttons;
                for (const auto& [buttonId, isPressed] : state.buttonStates)
                    buttons += "#" + std::to_string(buttonId) + "(" + (isPressed ? "1) " : "0) ");
                ImGui::Text("Buttons: %s", buttons.c_str());

                ImGui::EndChild();
                ImGui::PopID();
            }
        }

        // End ImGui window
        ImGui::End();
    }

} // namespace eeng
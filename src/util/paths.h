#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace eeng::paths
{

    /**
     * @brief Returns the absolute path to the currently running executable.
     *
     * This function works across Windows, macOS, and Linux.
     *
     * @return Full path to the executable file, or empty path on failure.
     */
    inline std::filesystem::path get_executable_path()
    {
#if defined(_WIN32)
        char path[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
        return (length > 0) ? std::filesystem::canonical(path) : std::filesystem::path();
#elif defined(__APPLE__)
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0)
        {
            return std::filesystem::canonical(path);
        }
        else
        {
            return std::filesystem::path(); // buffer too small
    }
#else // Linux and others
        return std::filesystem::canonical("/proc/self/exe");
#endif
}

    /**
     * @brief Returns the directory containing the currently running executable.
     *
     * @return Directory path of the executable.
     */
    inline std::filesystem::path get_executable_directory()
    {
        return get_executable_path().parent_path();
    }

    /**
     * @brief Returns the meta output directory as defined by the CMake macro META_OUTPUT_DIR.
     *
     * This path is typically set using a target-local CMake definition like:
     * @code
     * target_compile_definitions(MyTarget PRIVATE META_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/meta")
     * @endcode
     *
     * This function also ensures that the meta directory exists by creating it
     * the first time it is accessed.
     *
     * @return Path to the meta output directory, or an empty path if the macro is not defined.
     */
    inline const std::filesystem::path& get_meta_output_directory()
    {
        static const std::filesystem::path base =
#ifdef META_OUTPUT_DIR
            std::filesystem::path{ META_OUTPUT_DIR };
#else
            std::filesystem::path{};
#endif

        static const bool created = []()
            {
                if (!base.empty())
                {
                    std::error_code ec;
                    std::filesystem::create_directories(base, ec);
                    if (ec)
                    {
                        std::cerr << "[eeng::paths] Failed to create base meta directory: " << ec.message() << "\n";
                    }
                }
                return true;
            }();

        return base;
    }

    /**
     * @brief Resolves a full path inside the meta output directory and ensures its parent folder exists.
     *
     * This is useful when writing custom types of content (e.g., images, binaries, cached data)
     * and you want to prepare the path safely before writing.
     *
     * Example:
     * @code
     * auto path = eeng::paths::get_or_create_meta_path("textures/ship_diffuse.png");
     * std::ofstream file(path, std::ios::binary);
     * file.write(...);
     * @endcode
     *
     * @param relative_path Path inside the meta directory (e.g., "textures/image.png").
     * @return Full path to the resolved file, or empty path if the base directory is not available or an error occurs.
     */
    inline std::filesystem::path get_or_create_meta_path(const std::filesystem::path& relative_path)
    {
        const std::filesystem::path root = get_meta_output_directory();
        const std::filesystem::path full_path = root / relative_path;

        std::error_code ec;
        std::filesystem::create_directories(full_path.parent_path(), ec);
        if (ec)
        {
            std::cerr << "[eeng::paths] Failed to create parent directories: " << ec.message() << "\n";
            return {};
        }

        return full_path;
    }

    /**
     * @brief Writes the given text content to a file inside the meta output directory.
     *
     * If necessary, the full directory structure will be created relative to the meta output base.
     *
     * Example:
     * @code
     * eeng::paths::write_to_meta("models/ship/log.txt", "Loading succeeded.");
     * @endcode
     *
     * @param relative_input_file Path relative to the meta output directory (e.g., "models/ship/log.txt").
     * @param contents The content to write to the file.
     * @return True if the file was written successfully, false on error.
     */
    inline bool write_to_meta(const std::filesystem::path& relative_input_file,
        const std::string& contents)
    {
        const std::filesystem::path output_file = get_or_create_meta_path(relative_input_file);
        if (output_file.empty())
        {
            return false;
        }

        std::ofstream out_file(output_file);
        if (!out_file)
        {
            std::cerr << "[eeng::paths] Failed to open file for writing: " << output_file << "\n";
            return false;
        }

        out_file << contents;
        return true;
    }

} // namespace eeng::paths

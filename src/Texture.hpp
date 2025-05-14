// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef texture_hpp
#define texture_hpp

#include <stdio.h>
#include "glcommon.h"
#include "config.h"
#include "parseutil.h"
#include "GLHandleWrappers.h"

struct texture_filter_mode_t { GLuint min_filter, mag_filter; };
struct texture_address_mode_t { GLuint s_mode, t_mode; };

class Texture2D
{
public:
    GLTexturePtr m_texture_ptr = nullptr;
    unsigned m_width = 0, m_height = 0, m_channels = 0;
    std::string m_name = "", m_fullpath = "";

    texture_filter_mode_t m_filter_mode = { GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR };
    texture_address_mode_t m_address_mode{ GL_REPEAT, GL_REPEAT };

    Texture2D(
        const std::string& name,
        const std::string& fullpath) :
        m_name(name), m_fullpath(fullpath)
    {
        std::cout << "Texture2D constructor: " << m_name << ", " << m_fullpath << "\n";
    }

    void set_filter_mode(const texture_filter_mode_t& filter_mode);

    void set_address_mode(const texture_address_mode_t& address_mode);

    void load_from_file();

    void load_from_memory(
        const unsigned char* image,
        int len);

    void load_image(
        const unsigned char* image,
        int w,
        int h,
        int channels);

    GLuint getHandle();

    void bind(GLenum p_texture_slot) const;

    void unbind() const;

    friend std::ostream& operator << (std::ostream& os,
        const Texture2D& t)
    {
        return
            os
            << t.m_name << ", " << t.m_width << "x" << t.m_height
            << ", chan " << t.m_channels;
    }

private:

    void load_to_VRAM(
        const unsigned char* image,
        int w,
        int h,
        GLuint internal_format,
        GLuint format);
};


class gl_cubemap_t
{
public:
    GLuint m_handle = 0;
    unsigned m_width = 0, m_height = 0, m_channels = 0;
    std::string /*m_name = "",*/ m_fullpath = "";

    gl_cubemap_t() = default;

    void load_from_files(const std::string filepaths[]);

private:
    void load_from_file(//const std::string& filename,
        const std::string& file,
        GLenum target);

    void load_image(//const std::string& name,
        const unsigned char* image,
        int w,
        int h,
        int channels,
        GLenum target);

public:
    void bind(GLenum p_texture_slot);

    void unbind();

    //    friend std::ostream& operator << (std::ostream& os,
    //                                      const gl_cubemap_t& t)
    //    {
    //        return
    //        os
    //        << t.m_name << ", " << t.m_width << "x" << t.m_height
    //        << ", chan " << t.m_channels;
    //    }

private:

    void load_to_VRAM(//const std::string& name,
        const unsigned char* image,
        int w,
        int h,
        GLuint internal_format,
        GLuint format,
        GLenum target);
};

#endif /* texture_hpp */

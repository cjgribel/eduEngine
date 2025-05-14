// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

// Excerpt from stb_imaage.h:
//
// Limitations:
//    - no 16-bit-per-channel PNG
//    - no 12-bit-per-channel JPEG
//    - no JPEGs with arithmetic coding
//    - no 1-bit BMP
//    - GIF always returns *comp=4

#include <algorithm>
#include "Texture.hpp"

#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void Texture2D::set_filter_mode(const texture_filter_mode_t& filter_mode)
{
    m_filter_mode = filter_mode;
};

void Texture2D::set_address_mode(const texture_address_mode_t& address_mode)
{
    m_address_mode = address_mode;
};

void Texture2D::load_from_file()
{
    unsigned char* image;
    int w, h, channels;

    if (!(image = stbi_load(m_fullpath.c_str(), &w, &h, &channels, 0)))
    {
        if (!(image = stbi_load(lowercase_of(m_fullpath).c_str(), &w, &h, &channels, 0)))
        {
            throw std::runtime_error("Error loading texture " + m_fullpath + "\n");
        }
    }

    load_image(image, w, h, channels);
    stbi_image_free(image);
}

// Load from an (embedded) aiTexture and not from file
// void gl_texture_t::load_from_memory(const std::string& filename, const aiTexture* ait)
void Texture2D::load_from_memory(
    const unsigned char* data,
    int len)
{
    // Compressed embedded texture

    int w, h, channels;
    unsigned char* image;
    image = stbi_load_from_memory(
        data,
        len,
        &w, &h, &channels, 0);
    if (!image)
    {
        throw std::runtime_error("Error loading texture " + m_name + "\n");
    }

    CheckAndThrowGLErrors();
    load_image(image, w, h, channels);
    stbi_image_free(image);
}

void Texture2D::load_image(
    const unsigned char* image,
    int w,
    int h,
    int channels)
{
    m_channels = channels;

    GLuint internal_format, format;
    if (channels == 1)
    {
        internal_format = GL_R8;
        format = GL_RED;
    }
    else if (channels == 2)
    {
        internal_format = GL_RG8;
        format = GL_RG;
    }
    else if (channels == 3)
    {
        internal_format = GL_RGB8;
        format = GL_RGB;
    }
    else if (channels == 4)
    {
        internal_format = GL_RGBA8;
        format = GL_RGBA;
    }
    else
        throw std::runtime_error("Unsupported texture format, number of channels " + std::to_string(channels) + "\n");
    CheckAndThrowGLErrors();

    load_to_VRAM(image, w, h, internal_format, format);
}

void Texture2D::load_to_VRAM(
    const unsigned char* image,
    int w,
    int h,
    GLuint internal_format,
    GLuint format)
{
    m_width = w;
    m_height = h;

    m_texture_ptr = GLTexture::create(m_name);

    glBindTexture(GL_TEXTURE_2D, m_texture_ptr->id);

    // Minification & magnification filters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter_mode.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter_mode.mag_filter);

    // Address mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_address_mode.s_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_address_mode.t_mode);

#ifdef EENG_ANISO
    // Anisotropic filter
    GLfloat maxAniso;
#if defined(EENG_GLVERSION_43)
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(EENG_ANISO_SAMPLES, (GLint)maxAniso));
#elif defined(EENG_GLVERSION_41)
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(EENG_ANISO_SAMPLES, (GLint)maxAniso));
#endif
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    CheckAndThrowGLErrors();
}

GLuint Texture2D::getHandle()
{
    return m_texture_ptr->id;
}

void Texture2D::bind(GLenum p_texture_slot) const
{
    glActiveTexture(p_texture_slot);
    glBindTexture(GL_TEXTURE_2D, m_texture_ptr->id);
}

void Texture2D::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

// void Texture2D::free()
// {
//     if (m_handle)
//     {
//         glDeleteTextures(1, &m_handle);
//         m_handle = 0;
//     }
// }

void gl_cubemap_t::load_from_files(const std::string filepaths[])
{
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
    CheckAndThrowGLErrors();

    for (int i = 0; i < 6; i++)
        load_from_file(filepaths[i], GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    CheckAndThrowGLErrors();
}

void gl_cubemap_t::load_from_file(const std::string& fullpath,
    GLenum target)
{
    m_fullpath = fullpath;

    unsigned char* image;
    int w, h, channels;

    if (!(image = stbi_load(m_fullpath.c_str(), &w, &h, &channels, 0)))
    {
        if (!(image = stbi_load(lowercase_of(m_fullpath).c_str(), &w, &h, &channels, 0)))
        {
            throw std::runtime_error("Error loading texture " + m_fullpath + "\n");
        }
    }

    load_image(image, w, h, channels, target);
    stbi_image_free(image);
}

void gl_cubemap_t::load_image(const unsigned char* image,
    int w,
    int h,
    int channels,
    GLenum target)
{
    m_channels = channels;

    GLuint internal_format, format;
    if (channels == 1)
    {
        internal_format = GL_R8;
        format = GL_R;
    }
    else if (channels == 2)
    {
        internal_format = GL_RG8;
        format = GL_RG;
    }
    else if (channels == 3)
    {
        internal_format = GL_RGB8;
        format = GL_RGB;
    }
    else if (channels == 4)
    {
        internal_format = GL_RGBA8;
        format = GL_RGBA;
    }
    else
        throw std::runtime_error("Unsupported texture format, number of channels " + std::to_string(channels) + "\n");

    load_to_VRAM(image, w, h, internal_format, format, target);
}

void gl_cubemap_t::load_to_VRAM(const unsigned char* image,
    int w,
    int h,
    GLuint internal_format,
    GLuint format,
    GLenum target)
{
    m_width = w;
    m_height = h;

    glTexImage2D(target, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, image);
}

void gl_cubemap_t::bind(GLenum p_texture_slot)
{
    glActiveTexture(p_texture_slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
}

void gl_cubemap_t::unbind()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

#pragma once

#include <string>
#include <memory>
#include <cassert>
#include <iostream>
#include "glcommon.h"

// Traits to generate and delete specific OpenGL handle types
template<typename Traits>
struct GLHandle
{
    GLuint id   = 0;
    std::string name;

    struct Deleter 
    {
        void operator()(GLHandle* h) noexcept 
        {
            assert(h && "GLHandle is null");
            assert(h->id && "Handle not generated");
            std::cout << "Deleting GL handle '" << h->name << "' (" << h->id << ")\n";
            Traits::del(1, &h->id);
            delete h;
        }
    };

    using Ptr = std::shared_ptr<GLHandle>;

    // Creates a new handle and assigns an optional debug name
    static Ptr create(const std::string& debugName = {}) noexcept 
    {
        GLuint tmp = 0;
        Traits::gen(1, &tmp);
        auto raw = new GLHandle{ tmp, debugName };
        return Ptr(raw, Deleter{});
    }
};

// Specialize traits for buffers
struct BufferTraits 
{
    static void gen(GLsizei n, GLuint* ids) { glGenBuffers(n, ids); }
    static void del(GLsizei n, const GLuint* ids) { glDeleteBuffers(n, ids); }
};

// Specialize traits for textures
struct TextureTraits 
{
    static void gen(GLsizei n, GLuint* ids) { glGenTextures(n, ids); }
    static void del(GLsizei n, const GLuint* ids) { glDeleteTextures(n, ids); }
};

typedef GLHandle<BufferTraits>  GLBuffer;
typedef GLHandle<TextureTraits> GLTexture;

typedef GLBuffer::Ptr  GLBufferPtr;
typedef GLTexture::Ptr GLTexturePtr;

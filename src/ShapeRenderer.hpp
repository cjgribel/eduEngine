// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ShapeRenderer_h
#define ShapeRenderer_h

#include <vector>
#include <unordered_map>
#include <stack>

#include <glm/glm.hpp>

#include "glmcommon.hpp"
#include "hash_combine.h"


namespace ShapeRendering {

    using uint = uint32_t;
    using uchar = unsigned char;

    template<typename... Types>
    class StateStack {

        std::tuple<std::stack<Types>...> stacks;

        template<typename T>
        std::stack<T>& getStack()
        {
            return std::get<std::stack<T>>(stacks);
        }

        template<typename T>
        const std::stack<T>& getStack() const
        {
            return std::get<std::stack<T>>(stacks);
        }

        template<typename T>
        void push_impl(const T& value)
        {
            if constexpr (std::is_same_v<T, glm::mat4>)
            {
                if (!empty<T>())
                {
                    T transform = top_impl<T>() * value;
                    getStack<T>().push(transform);
                }
                else
                    getStack<T>().push(value);
            }
            else
                getStack<T>().push(value);
        }

        template<typename T>
        void pop_impl()
        {
            assert(!empty<T>());
            getStack<T>().pop();

            // The default state should not be popped
            assert(!empty<T>());
        }

        template<typename T>
        T& top_impl()
        {
            assert(!empty<T>());
            return getStack<T>().top();
        }

    public:

        template<typename... Args>
        void push(Args&&... args)
        {
            (push_impl(std::forward<Args>(args)), ...);
        }

        template<typename... Args>
        void pop()
        {
            (pop_impl<Args>(), ...);
        }

        template<typename... Args>
            requires (sizeof...(Args) > 1)
        auto top()
        {
            return std::make_tuple(top_impl<Args>()...);
        }

        template<typename T>
        T& top()
        {
            return top_impl<T>();
        }

        template<typename T>
        bool empty() const
        {
            return getStack<T>().empty();
        }
    };

    struct Color4u
    {
        using uint = uint32_t;
        uint color;

        Color4u(uint color)
            : color(color)
        {

        }

        Color4u(uchar r, uchar g, uchar b, uchar a)
        {
            const uint r_shift = 0;
            const uint g_shift = 8;
            const uint b_shift = 16;
            const uint a_shift = 24;

            color = ((uint)a << a_shift) | ((uint)b << b_shift) | ((uint)g << g_shift) | ((uint)r << r_shift);
        }

        Color4u(const glm::vec4& cf)
            : Color4u((uchar)(255 * cf.x), (uchar)(255 * cf.y), (uchar)(255 * cf.z), (uchar)(255 * cf.w))
        {
        }

        Color4u(const glm::vec3& cf)
            : Color4u(glm::vec4(cf.x, cf.y, cf.x, 1.0f))
        {
        }

        operator uint() const { return color; }

        static const Color4u Black;
        static const Color4u White;
        static const Color4u Red;
        static const Color4u Lime;
        static const Color4u Blue;
        static const Color4u Yellow;
        static const Color4u Cyan;
        static const Color4u Magenta;
        static const Color4u Gray;
        static const Color4u Maroon;
        static const Color4u Olive;
        static const Color4u Green;
        static const Color4u Purple;
        static const Color4u Teal;
        static const Color4u Navy;
        static const Color4u Orange;
        static const Color4u Pink;
        static const Color4u Brown;
        static const Color4u Silver;
        static const Color4u Gold;
        static const Color4u Turquoise;
    };

    struct PolyVertex
    {
        glm::vec3 p;
        glm::vec3 normal;
        uint color;
    };

    struct LineVertex
    {
        glm::vec3 p;
        uint color;
    };

    struct PointVertex
    {
        glm::vec3 p;
        uint color;
    };

    struct ArrowDescriptor
    {
        float cone_fraction;
        float cone_radius;
        float cylinder_radius;
    };

    enum class DepthTest : bool
    {
        True = true,
        False = false
    };

    enum class BackfaceCull : bool
    {
        True = true,
        False = false
    };

    class ShapeRenderer
    {
        long framenbr = 0;

        GLuint lambert_shader;
        GLuint line_shader;
        GLuint point_shader;

        // Pre-initialized primitives
        struct
        {
            std::vector<PolyVertex> vertices;
            std::vector<unsigned> indices;
        }
        unitcone_buffers, unitcylinder_buffers, unitsphere_buffers, unitspherewireframe_buffers;

        struct LineBatch
        {
            GLenum topology = GL_LINES;
            DepthTest depth_test = DepthTest::True;

            bool operator == (const LineBatch& dc) const
            {
                return
                    topology == dc.topology &&
                    depth_test == dc.depth_test;
            }
        };

        struct LineBatchHashFunction
        {
            std::size_t operator () (const LineBatch& ldc) const
            {
                return hash_combine(ldc.topology, ldc.depth_test);
            }
        };

        std::vector<LineVertex> line_vertices;
        std::unordered_map<LineBatch, std::vector<unsigned>, LineBatchHashFunction> line_hash;

        GLuint lines_VBO = 0;
        GLuint lines_IBO = 0;
        GLuint lines_VAO = 0;

        struct PolygonBatch
        {
            GLenum topology = GL_TRIANGLES;
            DepthTest depth_test = DepthTest::True;
            BackfaceCull cull_face = BackfaceCull::True;

            bool operator == (const PolygonBatch& dc) const
            {
                return
                    topology == dc.topology &&
                    depth_test == dc.depth_test &&
                    cull_face == dc.cull_face;
            }
        };

        struct PolygonBatchHashfunction
        {
            std::size_t operator () (const PolygonBatch& pdc) const
            {
                return hash_combine(pdc.topology, pdc.depth_test);
            }
        };

        struct IndexRange
        {
            GLsizei start, size;
            GLint ofs;
        };

        std::vector<PolyVertex> polygon_vertices;
        std::vector<unsigned> polygon_indices;
        // Motivation for unordered_multimap over multimap:
        // insertion: avg constant (N worst case) vs avg logN
        // count (if used in iteration) avg N in nbr of matches (N in size worst case) vs logN, plus N in the number of matches.
        std::unordered_multimap<PolygonBatch, IndexRange, PolygonBatchHashfunction> polygon_hash;
        GLuint polygon_vbo = 0;
        GLuint polygon_ibo = 0;
        GLuint polygon_vao = 0;

        struct PointBatch
        {
            unsigned size;
            DepthTest depth_test = DepthTest::True;

            bool operator == (const PointBatch& pdc) const
            {
                return depth_test == pdc.depth_test && size == pdc.size;
            }
        };

        struct PointBatchHashFunction
        {
            std::size_t operator () (const PointBatch& pdc) const
            {
                return  std::hash<GLenum>{}(pdc.size);
            }
        };

        std::unordered_map<PointBatch, std::vector<PointVertex>, PointBatchHashFunction > point_hash;
        GLuint point_vbo = 0;
        GLuint point_vao = 0;

        StateStack<DepthTest, BackfaceCull, glm::mat4, Color4u> state_stack;

        bool initialized = false;

    public:
        void init();

        template<typename... Args>
        void push_states(Args&&... args)
        {
            state_stack.push(std::forward<Args>(args)...);
        }

        template<typename... Args>
        void pop_states()
        {
            state_stack.pop<Args...>();
        }

        template<typename... Args>
        auto get_states()
        {
            return state_stack.top<Args...>();
        }

        template<class T>
        auto get_state_integral()
        {
            return to_integral(state_stack.top<T>());
        }

        void push_quad(
            const glm::vec3 points[4],
            const glm::vec3& n);

        void push_quad_wireframe();

        void push_cube();

        void push_cube_wireframe();

        void push_AABB(
            const glm::vec3& min,
            const glm::vec3& max);

        template<unsigned N>
        void push_circle_ring()
        {
            const auto [transform, color, depth_test] = get_states<glm::mat4, Color4u, DepthTest>();

            auto vertex_generator =
                []()
                {
                    std::vector<glm::vec4> vertices;
                    for (int i = 0; i < N; i++)
                    {
                        const float theta = i * 2.0f * 3.14159f / (N - 1);
                        vertices.emplace_back(std::cos(theta), std::sin(theta), 0.0f, 1.0f);
                    }
                    return vertices;
                };
            static const auto vertices = vertex_generator();

            auto& index_batch = line_hash[LineBatch{ GL_LINES, depth_test }];
            unsigned vertex_ofs = (unsigned)line_vertices.size();

            for (int i = 0; i < N; i++)
            {
                line_vertices.push_back(LineVertex{
                    glm::vec3(transform * vertices[i]),
                    color
                    });
                if (i < N - 1) {
                    index_batch.push_back(vertex_ofs + i);
                    index_batch.push_back(vertex_ofs + i + 1);
                }
            }
        }

        void push_line(
            const glm::vec3& pos0,
            const glm::vec3& pos1);

        void push_lines_from_cyclic_source(
            const LineVertex* vertices,
            int start_index,
            int nbr_vertices,
            int max_vertices);

        void push_lines(
            const std::vector<glm::vec3>& vertices,
            const std::vector<unsigned>& indices);

        void push_lines(
            const glm::vec3* vertices,
            size_t nbr_vertices,
            const unsigned* indices,
            size_t nbr_indices);

        void push_lines(
            const glm::vec3* vertices,
            size_t nbr_vertices);

        void push_grid(
            const glm::vec3& pos,
            unsigned size,
            unsigned resolution);

        void push_cone(
            const glm::vec3& from,
            const glm::vec3 to,
            float r);

        void push_cone(
            float h,
            float r,
            bool flip_normals = false);

        void push_cylinder(
            float height,
            float radius);

        void push_arrow(
            const glm::vec3& from,
            const glm::vec3& to,
            ArrowDescriptor arrow_desc);

        void push_sphere(float h, float r);

        void push_sphere_wireframe(float h, float r);

#if 0
        void push_helix(const vec3f& from,
            const vec3f& to,
            float r_outer,
            float r_inner,
            float revs);

        void push_helix(float length,
            float r_outer,
            float r_inner,
            float revs);

        void push_frustum(const mat4f& invProjView);
#endif

        void push_basis_basic(
            const glm::mat4& basis,
            float arrlen);

        void push_basis_basic2d(
            const glm::mat4& basis,
            float arrlen);

        void push_basis(
            const ArrowDescriptor& arrow_desc,
            const glm::vec3 arrow_lengths = glm_aux::vec3_111);

        void push_point(const glm::vec3& p, unsigned size);

        void push_point_direct(
            const glm::vec3& p,
            unsigned size);

        void push_points_direct(
            const PointVertex* p,
            unsigned nbr_points,
            unsigned size);

        void render(
            const glm::mat4& PROJ_VIEW);

        void post_render();
    };
}

using ShapeRendererPtr = std::shared_ptr<ShapeRendering::ShapeRenderer>;

namespace ShapeRendering {
    void DemoDraw(ShapeRendererPtr renderer);
}

#endif

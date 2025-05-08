// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#include "RenderableMesh.hpp"

#include <glm/gtx/dual_quaternion.hpp>
#include <assimp/version.h>

#include "ShaderLoader.h"
#include "parseutil.h"
#include "Profiler.hpp"

namespace eeng
{
    namespace
    {
        inline glm::vec3 aivec_to_glmvec(const aiVector3D& vec)
        {
            return glm::vec3(vec.x, vec.y, vec.z);
        }

        inline glm::quat aiquat_to_glmquat(const aiQuaternion& aiq)
        {
            return glm::quat(aiq.w, aiq.x, aiq.y, aiq.z);
        }

        inline glm::mat4 aimat_to_glmmat(const aiMatrix4x4& aim)
        {
            glm::mat4 glmm;
            glmm[0][0] = aim.a1;
            glmm[1][0] = aim.a2;
            glmm[2][0] = aim.a3;
            glmm[3][0] = aim.a4;
            glmm[0][1] = aim.b1;
            glmm[1][1] = aim.b2;
            glmm[2][1] = aim.b3;
            glmm[3][1] = aim.b4;
            glmm[0][2] = aim.c1;
            glmm[1][2] = aim.c2;
            glmm[2][2] = aim.c3;
            glmm[3][2] = aim.c4;
            glmm[0][3] = aim.d1;
            glmm[1][3] = aim.d2;
            glmm[2][3] = aim.d3;
            glmm[3][3] = aim.d4;
            return glmm;
        }

        inline glm::mat4 dualquatToMat4(const glm::dualquat& dq)
        {
            // Extract the real (rotation) part and dual part (translation info)
            glm::quat realPart = dq.real;
            glm::quat dualPart = dq.dual;

            // Compute the translation: t = 2 * (dual * conjugate(real))
            glm::quat t = dualPart * glm::conjugate(realPart);
            glm::vec3 translation(t.x, t.y, t.z);
            translation *= 2.0f;

            // Convert the rotation quaternion to a 4x4 rotation matrix
            glm::mat4 rotationMatrix = glm::mat4_cast(realPart);

            // Create the final transformation matrix
            glm::mat4 transformMatrix = rotationMatrix;
            // In a column-major matrix, the translation vector is set in the 4th column.
            transformMatrix[3] = glm::vec4(translation, 1.0f);

            return transformMatrix;
        }

        void dump_tree_to_stream(
            const VecTree<SkeletonNode>& tree,
            logstreamer_t&& outstream)
        {
            tree.traverse_depthfirst([&](const SkeletonNode& node, size_t index, size_t level)
                {
                    auto [nbr_children, branch_stride, parent_ofs] = tree.get_node_info(node);

                    for (int i = 0; i < level; i++) outstream << "  ";
                    outstream << " [node " << index << "]";
                    if (node.bone_index != EENG_NULL_INDEX)
                        outstream << "[bone " << node.bone_index << "]";
                    if (node.nbr_meshes)
                        outstream << "[" << node.nbr_meshes << " meshes]";
                    outstream << " " << node.name
                        << " (children " << nbr_children
                        << ", stride " << branch_stride
                        << ", parent ofs " << parent_ofs << ")\n";
                });
        }
    }

    void RenderableMesh::SkinData::addWeight(unsigned bone_index, float bone_weight)
    {
        nbr_added++;

        float min_weight = 1;
        unsigned min_index = 0;
        for (uint i = 0; i < numelem(bone_indices); i++)
            if (bone_weights[i] < min_weight)
            {
                min_weight = bone_weights[i];
                min_index = i;
            }
        if (bone_weight > min_weight)
        {
            bone_weights[min_index] = bone_weight;
            bone_indices[min_index] = bone_index;
        }
    }

    RenderableMesh::RenderableMesh()
    {
    }

    void RenderableMesh::load(const std::string& file, bool append_animations)
    {
        std::cout << "Loading mesh: " << file << ", " << append_animations << std::endl;
        unsigned xiflags = (append_animations ? xi_load_animations : (xi_load_meshes | xi_load_animations));

        unsigned aiflags;
        //    aiflags |= aiProcess_Triangulate;
        //    aiflags |= aiProcess_JoinIdenticalVertices;
        //    aiflags |= aiProcess_GenSmoothNormals; // needed for ArmyPilot
        //    //aiflags |= aiProcess_GenUVCoords;
        //    //aiflags |= aiProcess_TransformUVCoords;
        //    aiflags |= aiProcess_RemoveComponent;
        //    aiflags |= aiProcess_FlipUVs;
        //    aiflags |= aiProcess_CalcTangentSpace;

        // aiflags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs;

        aiflags =
            aiProcess_CalcTangentSpace /*  */ |
            aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_Triangulate /* Must be here for render geometry */ |
            aiProcess_GenUVCoords |
            aiProcess_SortByPType |
            aiProcess_FlipUVs | // added
            aiProcess_OptimizeGraph;

        load(file, xiflags, aiflags);
    }

    void RenderableMesh::load(const std::string& file,
        unsigned xiflags,
        unsigned aiflags)

    {
        util::Profiler::start("RenderableMesh::load");

        // Plan is to utilize xiflags with more detail
        bool append_animations = (xiflags == xi_load_animations);

        //
        std::string filepath, filename, fileext;
        decompose_path(file, filepath, filename, fileext);

        // Assimp::Importer owns & destroys the loaded data (as pointed to
        // by aiScene* once loaded).
        Assimp::Importer aiimporter;

        // Prepare the logs
        if (!append_animations)
        {
            // log.add_ostream(std::cout, STRICT);
            log.add_ofstream(filepath + filename + "_log.txt", PRTVERBOSE);
        }

        // Log misc stuff
        log << priority(PRTSTRICT) << "Assimp version: "
            << aiGetVersionMajor() << "."
            << aiGetVersionMinor() << "."
            << aiGetVersionRevision() << std::endl;
        log << priority(PRTSTRICT) << "Assimp about to open file:\n"
            << file << std::endl;
        // File support
        aiString supported_list;
        aiimporter.GetExtensionList(supported_list);
        log << priority(PRTVERBOSE) << "Assimp supported formats: \n"
            << supported_list.C_Str() << std::endl;
        bool ext_supported = aiimporter.IsExtensionSupported(fileext);
        log << priority(PRTVERBOSE) << "Format " << fileext << " supported: " << (ext_supported ? "YES" : "NO") << std::endl;

        // Load
        util::Profiler::start("RenderableMesh::load", "Assimp load");
        const aiScene* aiscene = aiimporter.ReadFile(file, aiflags);
        util::Profiler::stop("RenderableMesh::load", "Assimp load");

        if (!aiscene)
            throw std::runtime_error(aiimporter.GetErrorString());
        log << priority(PRTSTRICT) << "Assimp load OK\n";

        // Load animations to a previously loaded model
        if (append_animations)
        {
            log << priority(PRTSTRICT) << "Appending animations... \n";

            if (!m_meshes.size())
                throw std::runtime_error("Cannot append animations to an empty model\n");

            loadAnimations(aiscene);

            util::Profiler::stop("RenderableMesh::load");
            util::Profiler::reset("RenderableMesh::load");
            log << priority(PRTSTRICT) << "Done appending animations.\n";
            return;
        }

        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);
        glGenBuffers(numelem(m_Buffers), m_Buffers);

        loadScene(aiscene, filepath);
        glBindVertexArray(0);

        util::Profiler::start("RenderableMesh::load", "nodes");
        loadNodes(aiscene->mRootNode);
        util::Profiler::stop("RenderableMesh::load", "nodes");

        //m_nodetree.print_to_stream(logstreamer_t{ filepath + filename + "_nodetree.txt", PRTVERBOSE });
        dump_tree_to_stream(m_nodetree, logstreamer_t{ filepath + filename + "_nodetree.txt", PRTVERBOSE });
        // m_nodetree.debug_print({filepath + filename + "_nodetree.txt", PRTVERBOSE});

        util::Profiler::start("RenderableMesh::load", "animations");
        loadAnimations(aiscene);
        util::Profiler::stop("RenderableMesh::load", "animations");

        // Traverse the hierarchy.
        // Animated meshes must be traversed before each frame.
        animate(-1, 0.0f);

        mSceneAABB = measureScene(aiscene); // Only captures bind pose.

        util::Profiler::stop("RenderableMesh::load");
        util::Profiler::log("RenderableMesh::load");
        util::Profiler::reset("RenderableMesh::load");
    }

    void RenderableMesh::removeTranslationKeys(const std::string& node_name)
    {
        removeTranslationKeys(m_nodetree.find_node_index(node_name));
    }

    void RenderableMesh::removeTranslationKeys(int node_index)
    {
        for (auto& anim : m_animations)
        {
            EENG_ASSERT(node_index <= anim.node_animations.size(), "{0} is not a valid node index", node_index);
            auto& pos_keys = anim.node_animations[node_index].pos_keys;
            for (auto& pk : pos_keys)
                pk = { 0, pk.y, 0 };
        }
    }

    bool RenderableMesh::loadScene(const aiScene* aiscene, const std::string& filename)
    {
        unsigned scene_nbr_meshes = aiscene->mNumMeshes;
        unsigned scene_nbr_mtl = aiscene->mNumMaterials;
        unsigned scene_nbr_vertices = 0;
        unsigned scene_nbr_indices = 0;

        // Print some debug info
        log << priority(PRTSTRICT) << "Scene overview" << std::endl;
        log << "\t" << scene_nbr_meshes << " meshes" << std::endl;
        log << "\t" << scene_nbr_mtl << " materials" << std::endl;
        log << "\t" << aiscene->mNumTextures << " embedded textures" << std::endl;
        log << "\t" << aiscene->mNumAnimations << " animations" << std::endl;
        log << "\t" << aiscene->mNumLights << " lights" << std::endl;
        log << "\t" << aiscene->mNumCameras << " cameras" << std::endl;
        
        // Animations
        log << "Animations:\n";
        for (int i = 0; i < aiscene->mNumAnimations; i++)
        {
            aiAnimation* anim = aiscene->mAnimations[i];
            log
                << "\t" << anim->mName.C_Str()
                << ", channels " << anim->mNumChannels
                << ", duration in ticks " << anim->mDuration
                << ", tps " << anim->mTicksPerSecond
                << std::endl;
        }
        // Throw errors for cases which are not yet supported
        if (!aiscene->HasMeshes())
            throw std::runtime_error("Scene have no meshes (just bones and animations?)...");
        if (!aiscene->HasMaterials())
            throw std::runtime_error("Scene does not have materials...");

        m_meshes.resize(scene_nbr_meshes);
        m_materials.resize(scene_nbr_mtl);

        std::vector<glm::vec3> scene_positions;
        std::vector<glm::vec3> scene_normals;
        std::vector<glm::vec3> scene_tangents;
        std::vector<glm::vec3> scene_binormals;
        std::vector<glm::vec2> scene_texcoords;
        std::vector<SkinData> scene_skinweights;
        std::vector<uint> scene_indices;

        // Count vertices and indices of the whole scene
        for (unsigned i = 0; i < m_meshes.size(); i++)
        {
            unsigned mesh_nbr_vertices = aiscene->mMeshes[i]->mNumVertices;
            unsigned mesh_nbr_indices = aiscene->mMeshes[i]->mNumFaces * 3; // Assume aiProcess_Triangulate
            unsigned mesh_nbr_bones = aiscene->mMeshes[i]->mNumBones;
            unsigned mesh_mtl_index = aiscene->mMeshes[i]->mMaterialIndex;

            m_meshes[i].base_index = scene_nbr_indices;
            m_meshes[i].nbr_indices = mesh_nbr_indices;
            m_meshes[i].base_vertex = scene_nbr_vertices;
            m_meshes[i].nbr_vertices = mesh_nbr_vertices;
            m_meshes[i].mtl_index = mesh_mtl_index;
            m_meshes[i].is_skinned = (bool)mesh_nbr_bones;
            // m_meshes[i].node_index <- set while loading node tree

            scene_nbr_vertices += mesh_nbr_vertices;
            scene_nbr_indices += mesh_nbr_indices;
        }

        // Reserve space in the vectors for the vertex attributes and indices
        scene_positions.reserve(scene_nbr_vertices);
        scene_normals.reserve(scene_nbr_vertices);
        scene_tangents.reserve(scene_nbr_vertices);
        scene_binormals.reserve(scene_nbr_vertices);
        scene_texcoords.reserve(scene_nbr_vertices);
        scene_skinweights.resize(scene_nbr_vertices);
        scene_indices.reserve(scene_nbr_indices);

        // Initialize the meshes in the scene one by one
        for (uint i = 0; i < m_meshes.size(); i++)
        {
            const aiMesh* paiMesh = aiscene->mMeshes[i];
            loadMesh(i,
                paiMesh,
                scene_positions,
                scene_normals,
                scene_tangents,
                scene_binormals,
                scene_texcoords,
                scene_skinweights,
                scene_indices);
        }

        log << priority(PRTSTRICT);
        log << "Scene total vertices " << scene_nbr_vertices << ", triangles " << scene_nbr_indices / 3 << std::endl;
        log << "Bone mapping contains " << m_bonehash.size() << " bones in total\n";

        #if 1
        util::Profiler::start("RenderableMesh::load", "AABBs");
        // Model & bone AABB's
        boneMatrices.resize(m_bones.size());
        m_bone_aabbs_bind.resize(m_bones.size()); // Constructor resets AABB
        m_bone_aabbs_pose.resize(m_bones.size());

        m_mesh_aabbs_bind.resize(m_meshes.size());
        m_mesh_aabbs_pose.resize(m_meshes.size());

        for (int i = 0; i < m_meshes.size(); i++)
        {
            const auto& mesh = m_meshes[i];
            if (mesh.is_skinned)
            {
                // bones
                for (int j = mesh.base_vertex; j < mesh.base_vertex + mesh.nbr_vertices; j++)
                {
                    for (int k = 0; k < BonesPerVertex; k++)
                    {
                        if (scene_skinweights[j].bone_weights[k] > 0)
                            m_bone_aabbs_bind[scene_skinweights[j].bone_indices[k]].grow(scene_positions[j]);
                    }
                }
            }
            else // if ( m_meshes[i].node_index != EENG_NULL_INDEX )
            {
                for (int j = mesh.base_vertex; j < mesh.base_vertex + mesh.nbr_vertices; j++)
                    m_mesh_aabbs_bind[i].grow(scene_positions[j]);
            }
        }
        util::Profiler::stop("RenderableMesh::load", "AABBs");
#endif
        loadMaterials(aiscene, filename);

        util::Profiler::start("RenderableMesh::load", "load GL");
        // Load GL buffers
#define POSITION_LOCATION 0
#define TEXCOORD_LOCATION 1
#define NORMAL_LOCATION 2
#define TANGENT_LOCATION 3
#define BINORMAL_LOCATION 4
#define BONE_INDEX_LOCATION 5
#define BONE_WEIGHT_LOCATION 6

        // Generate and populate the buffers with vertex attributes and the indices
        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[PositionBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_positions[0]) * scene_positions.size(), &scene_positions[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(POSITION_LOCATION);
        glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[TexturecoordBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_texcoords[0]) * scene_texcoords.size(), &scene_texcoords[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(TEXCOORD_LOCATION);
        glVertexAttribPointer(TEXCOORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[NormalBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_normals[0]) * scene_normals.size(), &scene_normals[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(NORMAL_LOCATION);
        glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[TangentBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_tangents[0]) * scene_tangents.size(), &scene_tangents[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(TANGENT_LOCATION);
        glVertexAttribPointer(TANGENT_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[BinormalBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_binormals[0]) * scene_binormals.size(), &scene_binormals[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(BINORMAL_LOCATION);
        glVertexAttribPointer(BINORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[BoneBuffer]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(scene_skinweights[0]) * scene_skinweights.size(), &scene_skinweights[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(BONE_INDEX_LOCATION);
        glVertexAttribIPointer(BONE_INDEX_LOCATION, 4, GL_UNSIGNED_INT, sizeof(SkinData), (const GLvoid*)0);
        glEnableVertexAttribArray(BONE_WEIGHT_LOCATION);
        glVertexAttribPointer(BONE_WEIGHT_LOCATION, 4, GL_FLOAT, GL_FALSE, sizeof(SkinData), (const GLvoid*)16);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Buffers[IndexBuffer]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(scene_indices[0]) * scene_indices.size(), &scene_indices[0], GL_STATIC_DRAW);

        CheckAndThrowGLErrors();
        util::Profiler::stop("RenderableMesh::load", "load GL");

        return true;
    }

    void RenderableMesh::loadMesh(uint meshindex,
        const aiMesh* aimesh,
        std::vector<glm::vec3>& scene_positions,
        std::vector<glm::vec3>& scene_normals,
        std::vector<glm::vec3>& scene_tangents,
        std::vector<glm::vec3>& scene_binormals,
        std::vector<glm::vec2>& scene_texcoords,
        std::vector<SkinData>& scene_skindata,
        std::vector<unsigned int>& scene_indices)
    {
        log << priority(PRTVERBOSE);
        log << "Loading mesh " << aimesh->mName.C_Str() << std::endl;
        log << "\t" << aimesh->mNumVertices << " vertices" << std::endl;
        log << "\t" << aimesh->mNumFaces << " faces" << std::endl;
        log << "\t" << aimesh->mNumBones << " bones" << std::endl;
        log << "\t" << aimesh->mNumAnimMeshes << " anim-meshes*" << std::endl;
        // std::cout << "\t" << paiMesh->mNumUVComponents << " UV components" << std::endl;
        log << "\thas tangents and bitangents: " << (aimesh->HasTangentsAndBitangents() ? "YES" : "NO") << std::endl;
        log << "\thas vertex colors: " << (aimesh->HasVertexColors(0) ? "YES" : "NO") << std::endl;

        // Populate the vertex attribute vectors
        const aiVector3D v3zero(0.0f, 0.0f, 0.0f);
        for (uint i = 0; i < aimesh->mNumVertices; i++)
        {
            const aiVector3D* pPos = &(aimesh->mVertices[i]);
            const aiVector3D* pNormal = (aimesh->HasNormals() ? &(aimesh->mNormals[i]) : &v3zero);
            const aiVector3D* pTangent = (aimesh->HasTangentsAndBitangents() ? &(aimesh->mTangents[i]) : &v3zero);
            const aiVector3D* pBinormal = (aimesh->HasTangentsAndBitangents() ? &(aimesh->mBitangents[i]) : &v3zero);
            const aiVector3D* pTexCoord = (aimesh->HasTextureCoords(0) ? &(aimesh->mTextureCoords[0][i]) : &v3zero);

            scene_positions.push_back({ pPos->x, pPos->y, pPos->z });
            scene_normals.push_back({ pNormal->x, pNormal->y, pNormal->z });
            scene_tangents.push_back({ pTangent->x, pTangent->y, pTangent->z });
            scene_binormals.push_back({ pBinormal->x, pBinormal->y, pBinormal->z });
            scene_texcoords.push_back({ pTexCoord->x, pTexCoord->y });
        }

        loadBones(meshindex, aimesh, scene_skindata);

        // Populate the index buffer
        for (uint i = 0; i < aimesh->mNumFaces; i++)
        {
            const aiFace& Face = aimesh->mFaces[i];
            assert(Face.mNumIndices == 3);
            scene_indices.push_back(Face.mIndices[0]);
            scene_indices.push_back(Face.mIndices[1]);
            scene_indices.push_back(Face.mIndices[2]);
        }
    }

    AABB RenderableMesh::measureScene(const aiScene* aiscene)
    {
        AABB aabb;
        measureNode(aiscene, aiscene->mRootNode, glm::mat4{ 1.0f }, aabb);

        return aabb;
    }

    void RenderableMesh::measureNode(const aiScene* aiscene,
        const aiNode* pNode,
        const glm::mat4& M_roottfm,
        AABB& aabb)
    {
        glm::mat4 M_identity{ 1.0f };
        glm::mat4 M_nodetfm = M_roottfm * aimat_to_glmmat(pNode->mTransformation);

        for (int i = 0; i < pNode->mNumMeshes; i++)
        {
            aiMesh* mesh = aiscene->mMeshes[pNode->mMeshes[i]];

            // Skinned meshes are expressed in world/model space,
            // non-skinned meshes are expressed in node space.
            if (mesh->mNumBones)
                measureMesh(mesh, glm::mat4{ 1.0f }, aabb);
            else
                measureMesh(mesh, M_nodetfm, aabb);
        }

        for (int i = 0; i < pNode->mNumChildren; i++)
        {
            measureNode(aiscene, pNode->mChildren[i], M_nodetfm, aabb);
        }
    }

    void RenderableMesh::measureMesh(const aiMesh* pMesh,
        const glm::mat4& M_roottfm,
        AABB& aabb)
    {
        for (int i = 0; i < pMesh->mNumVertices; i++)
        {
            glm::vec3 v = aivec_to_glmvec(*(pMesh->mVertices + i));
            aabb.grow(glm::vec3{ M_roottfm * glm::vec4(v, 1.0f) });
        }
    }

    // Load node hierarchy and link nodes to bones & meshes
    void RenderableMesh::loadNodes(aiNode* ainode_root)
    {
        // Load node hierarchy recursively from root
        loadNode(ainode_root);

        // Link node->bone (0 or 1) and node->meshes (0+)
        // Link bones<->nodes (1<->1)
#if 1
        m_nodetree.traverse_depthfirst([&](SkeletonNode& node, size_t i, size_t level)
            {
                // Link node<->meshes (re-retrieve the original assimp node by name)
                // Note: the node transform is ignored during rendering if the mesh
                // is skinned, since it is part of the inverse-transpose matrix.
                aiNode* ainode = ainode_root->FindNode(node.name.c_str());
                for (int j = 0; j < ainode->mNumMeshes; j++)
                {
                    m_meshes[ainode->mMeshes[j]].node_index = i;
                }
                node.nbr_meshes = ainode->mNumMeshes;

                // Node<->bone
                auto boneit = m_bonehash.find(node.name);
                if (boneit != m_bonehash.end())
                {
                    m_bones[boneit->second].node_index = i;
                    node.bone_index = boneit->second;
                }
            });

        m_nodetree.traverse_depthfirst([&](SkeletonNode& node, size_t i, size_t level)
            {
                m_nodehash[node.name] = i;
            });
#else
        for (int i = 0; i < m_nodetree.nodes.size(); i++)
        {
            // Link node<->meshes (re-retrieve the original assimp node by name)
            // Note: the node transform is ignored during rendering if the mesh
            // is skinned, since it is part of the inverse-transpose matrix.
            aiNode* ainode = ainode_root->FindNode(m_nodetree.nodes[i].m_payload.name.c_str());
            for (int j = 0; j < ainode->mNumMeshes; j++)
            {
                m_meshes[ainode->mMeshes[j]].node_index = i;
            }
            m_nodetree.nodes[i].m_payload.nbr_meshes = ainode->mNumMeshes;

            // Node<->bone
            auto boneit = m_bonehash.find(m_nodetree.nodes[i].m_payload.name);
            if (boneit != m_bonehash.end())
            {
                m_bones[boneit->second].node_index = i;
                m_nodetree.nodes[i].m_payload.bone_index = boneit->second;
            }
        }

        // Build node name<->index hash
        // Note: Not currently used, though seems sensical to have.
        for (int i = 0; i < m_nodetree.nodes.size(); i++)
            m_nodehash[m_nodetree.nodes[i].m_payload.name] = i;
#endif

    }

    void RenderableMesh::loadNode(aiNode* ainode)
    {
        // Node data
        std::string node_name;   //
        std::string parent_name; //
        glm::mat4 transform;     // Local transform = transform relative parent
        // Fetch node data from assimp
        node_name = std::string(ainode->mName.C_Str());
        const aiNode* parent_node = ainode->mParent;
        parent_name = parent_node ? std::string(parent_node->mName.C_Str()) : "";
        transform = aimat_to_glmmat(ainode->mTransformation);

        // Create & insert node
        SkeletonNode stnode(node_name, transform);
        if (!parent_name.size())
        {
            m_nodetree.insert_as_root(stnode);
        }
        else if (!m_nodetree.insert(stnode, parent_name))
        {
            throw std::runtime_error("Node tree insertion failed, hierarchy corrupt");
        }

        for (int i = 0; i < ainode->mNumChildren; i++)
        {
            loadNode(ainode->mChildren[i]);
        }
    }

    void RenderableMesh::loadBones(uint mesh_index,
        const aiMesh* aimesh,
        std::vector<SkinData>& scene_skindata)
    {
        log << priority(PRTVERBOSE) << aimesh->mNumBones << " bones (nbr weights):\n";

        for (uint i = 0; i < aimesh->mNumBones; i++)
        {
            uint bone_index = 0;

            std::string bone_name(aimesh->mBones[i]->mName.C_Str());

            log << "\t" << bone_name << " (" << aimesh->mBones[i]->mNumWeights << ")\n";

            // Checks if bone is not yet created
            if (m_bonehash.find(bone_name) == m_bonehash.end())
            {
                // Generate an index for a new bone
                bone_index = (unsigned)m_bones.size();
                // Create bone from its inverse bind-pose transform
                Bone bi;
                m_bones.push_back(bi);
                m_bones[bone_index].inversebind_tfm = aimat_to_glmmat(aimesh->mBones[i]->mOffsetMatrix);
                // Hash bone w.r.t. its name
                m_bonehash[bone_name] = bone_index;
            }
            else
            {
                bone_index = m_bonehash[bone_name];
            }

            // For all weights associated with this bone
            for (uint j = 0; j < aimesh->mBones[i]->mNumWeights; j++)
            {
                uint vertex_id = m_meshes[mesh_index].base_vertex + aimesh->mBones[i]->mWeights[j].mVertexId;
                float bone_weight = aimesh->mBones[i]->mWeights[j].mWeight;
                scene_skindata[vertex_id].addWeight(bone_index, bone_weight);
            }
        }
    }

    /// @brief Load textures of a given type
    /// @param aimtl
    /// @param tex_type
    /// @param modelDir
    /// @return An index to the loaded texture
    int RenderableMesh::loadTexture(const aiMaterial* material, aiTextureType textureType, const std::string& modelDir)
    {
        unsigned nbr_textures = material->GetTextureCount(textureType);

        if (!nbr_textures)
            return NoTexture;
        if (nbr_textures > 1)
            throw std::runtime_error("Multiple textures of type " + std::to_string(textureType) + ", aborting. Nbr = " + std::to_string(nbr_textures));

        // Fetch texture properties from assimp
        aiString ai_texpath;            //
        aiTextureMapping ai_texmap;     //
        unsigned int ai_uvindex;        // unused
        float ai_blend;                 // unused
        aiTextureOp ai_texop;           // unused
        aiTextureMapMode ai_texmapmode; //
        if (material->GetTexture(textureType,
            0,
            &ai_texpath,
            &ai_texmap,
            &ai_uvindex,
            &ai_blend,
            &ai_texop,
            &ai_texmapmode) != AI_SUCCESS)
            return NoTexture;

        // Relative texture path, e.g. "/textures/texture.png"
        std::string textureRelPath{ ai_texpath.C_Str() };

        // Find an index to this texture, either by retrieving it (embedded texture)
        // or by creating it (texture on file)
        unsigned textureIndex;

        // Embedded textures are named *[N], where N is an index. Embedded  textures
        // are already loaded at this point, so if we ecounter this format we
        // extract N and use it (plus a buffer offset) as our index.
        int embedded_texture_index = EENG_NULL_INDEX;
        if (sscanf(textureRelPath.c_str(), "*%d", &embedded_texture_index) == 1)
        {
            textureIndex = m_embedded_textures_ofs + embedded_texture_index;
            log << priority(PRTSTRICT) << "\tUsing indexed embedded texture: " << embedded_texture_index << std::endl;
        }
        // Texture is a separate file
        else
        {
            // Texture filename, e.g. "texture.png"
            std::string textureFilename = get_filename(textureRelPath);
            // Absolute texture path, e.g. "C:/sponza/textures/texture.png"
#if 1
            std::string textureAbsPath = modelDir + textureRelPath;
#else
            std::string textureAbsPath = modelDir + textureFilename;
#endif

            log << priority(PRTVERBOSE) << "\traw path: " << textureRelPath << std::endl;
            log << priority(PRTVERBOSE) << "\tlocal file: " << textureAbsPath << std::endl;

            // Look for non-embedded textures (filepath + filename)
            auto tex_it = m_texturehash.find(textureRelPath);

            if (tex_it == m_texturehash.end())
            {
                // Look for embedded texture (just filename)
                tex_it = m_texturehash.find(textureFilename);
            }
            if (tex_it == m_texturehash.end())
            {
                // New texture found: create & hash it
                Texture2D texture;
                texture.load_from_file(textureFilename, textureAbsPath);
                log << priority(PRTSTRICT) << "Loaded texture " << texture << std::endl;
                textureIndex = (unsigned)m_textures.size();
                m_textures.push_back(texture);
                m_texturehash[textureRelPath] = textureIndex;
            }
            else
                textureIndex = tex_it->second;

            // Fetch & set address mode
            GLuint adr_mode;
            switch (ai_texmapmode)
            {
            case aiTextureMapMode_Wrap:
                adr_mode = GL_REPEAT;
                break;
            case aiTextureMapMode_Clamp:
                adr_mode = GL_CLAMP_TO_EDGE;
                break;
            case aiTextureMapMode_Decal:
                adr_mode = GL_CLAMP_TO_BORDER;
                break;
            case aiTextureMapMode_Mirror:
                adr_mode = GL_MIRRORED_REPEAT;
                break;
            default:
                adr_mode = GL_REPEAT;
                break;
            }
            m_textures[textureIndex].set_address_mode({ adr_mode, adr_mode });
        }

        return textureIndex;
    }

    // bool SkinnedMesh::InitMaterials(const aiScene* pScene, const string& Filename)
    void RenderableMesh::loadMaterials(const aiScene* aiscene, const std::string& file)
    {
        std::string local_filepath = get_parentdir(file);

        log << priority(PRTSTRICT) << "Loading materials...\n";
        log << "\tNum materials " << aiscene->mNumMaterials << std::endl;
        log << "\tParent dir: " << local_filepath << std::endl;

        // Load embedded textures to texture array, using plain indices as
        // hash strings. If any regular texture is named e.g. '1', without an
        // extension (which it really shouldn't), there will be a conflict in the
        // name hash.
        log << "Embedded textures: " << aiscene->mNumTextures << std::endl;
        log << priority(PRTVERBOSE);

        util::Profiler::start("RenderableMesh::load", "textures");
        m_embedded_textures_ofs = (unsigned)m_textures.size();
        for (int i = 0; i < aiscene->mNumTextures; i++)
        {
            aiTexture* aitexture = aiscene->mTextures[i];
            std::string filename = get_filename(aitexture->mFilename.C_Str());
            // std::string filename = std::to_string(i);

            Texture2D texture;
            if (aitexture->mHeight)
            {
                // Raw embedded image data
                texture.load_image(filename,
                    (unsigned char*)aitexture->pcData,
                    aitexture->mWidth,
                    aitexture->mHeight,
                    4);
                log << priority(PRTSTRICT) << "Loaded uncompressed embedded texture " << texture << std::endl;
            }
            else
            {
                // Compressed embedded image data
                texture.load_from_memory(filename,
                    (unsigned char*)aitexture->pcData,
                    sizeof(aiTexel) * (aitexture->mWidth));
                log << priority(PRTSTRICT) << "Loaded compressed embedded texture " << texture << std::endl;
            }

            m_texturehash[filename] = (unsigned)m_textures.size();
            m_textures.push_back(texture);
        }
        log << priority(PRTSTRICT) << "Loaded " << aiscene->mNumTextures << " embedded textures\n";
        util::Profiler::stop("RenderableMesh::load", "textures");

        // Initialize the materials
        for (uint i = 0; i < aiscene->mNumMaterials; i++)
        {
            const aiMaterial* pMaterial = aiscene->mMaterials[i];
            PhongMaterial mtl;

            aiString mtlname;
            pMaterial->Get(AI_MATKEY_NAME, mtlname);
            log << priority(PRTVERBOSE);
            log << "Loading material '" << mtlname.C_Str() << "', index " << i << "..." << std::endl;
            log << "Available textures:" << std::endl;
            log << "\tNone " << pMaterial->GetTextureCount(aiTextureType_NONE) << std::endl;
            log << "\tdiffuse " << pMaterial->GetTextureCount(aiTextureType_DIFFUSE) << std::endl;
            log << "\tSpecular " << pMaterial->GetTextureCount(aiTextureType_SPECULAR) << std::endl;
            log << "\tAmbient " << pMaterial->GetTextureCount(aiTextureType_AMBIENT) << std::endl;
            log << "\tEmissive " << pMaterial->GetTextureCount(aiTextureType_EMISSIVE) << std::endl;
            log << "\tHeight " << pMaterial->GetTextureCount(aiTextureType_HEIGHT) << std::endl;
            log << "\tNormals " << pMaterial->GetTextureCount(aiTextureType_NORMALS) << std::endl;
            log << "\tShininess " << pMaterial->GetTextureCount(aiTextureType_SHININESS) << std::endl;
            log << "\tOpacity " << pMaterial->GetTextureCount(aiTextureType_OPACITY) << std::endl;
            log << "\tDisplacement " << pMaterial->GetTextureCount(aiTextureType_DISPLACEMENT) << std::endl;
            log << "\tLightmap " << pMaterial->GetTextureCount(aiTextureType_LIGHTMAP) << std::endl;
            log << "\tReflection " << pMaterial->GetTextureCount(aiTextureType_REFLECTION) << std::endl;
            // Added in https://github.com/assimp/assimp/pull/2640
            log << "\tBase color " << pMaterial->GetTextureCount(aiTextureType_BASE_COLOR) << std::endl;
            log << "\tNormal camera " << pMaterial->GetTextureCount(aiTextureType_NORMAL_CAMERA) << std::endl;
            log << "\tEmission color " << pMaterial->GetTextureCount(aiTextureType_EMISSION_COLOR) << std::endl;
            log << "\tMetalness " << pMaterial->GetTextureCount(aiTextureType_METALNESS) << std::endl;
            log << "\tDiffuse roughness " << pMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) << std::endl;
            log << "\tAO " << pMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) << std::endl;
            log << "\tUnknown " << pMaterial->GetTextureCount(aiTextureType_UNKNOWN) << std::endl;

            // Fetch common color attributes
            aiColor3D aic;
            if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, aic))
                mtl.Ka = glm::vec3{ aic.r, aic.g, aic.b };
            if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aic))
                mtl.Kd = glm::vec3{ aic.r, aic.g, aic.b };
            if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, aic))
                mtl.Ks = glm::vec3{ aic.r, aic.g, aic.b };
            pMaterial->Get(AI_MATKEY_SHININESS, mtl.shininess);

            // Fetch common textures
            log << "Loading textures..." << std::endl;
            using TextureType = PhongMaterial::TextureTypeIndex;
            mtl.textureIndices[TextureType::Diffuse] = loadTexture(pMaterial, aiTextureType_DIFFUSE, local_filepath);
            mtl.textureIndices[TextureType::Normal] = loadTexture(pMaterial, aiTextureType_NORMALS, local_filepath);
            mtl.textureIndices[TextureType::Specular] = loadTexture(pMaterial, aiTextureType_SPECULAR, local_filepath);
            mtl.textureIndices[TextureType::Opacity] = loadTexture(pMaterial, aiTextureType_OPACITY, local_filepath);

            // Fallback: assimp seems to label OBJ normal maps as HEIGHT type textures.
            if (mtl.textureIndices[TextureType::Normal] == NoTexture)
                mtl.textureIndices[TextureType::Normal] = loadTexture(pMaterial, aiTextureType_HEIGHT, local_filepath);

            log << "Done loading textures" << std::endl;

            m_materials[i] = mtl;
        }
        log << "Done loading materials" << std::endl;

        log << priority(PRTSTRICT) << "Num materials " << m_materials.size() << std::endl;

        log << priority(PRTSTRICT) << "Num textures " << m_textures.size() << std::endl;
        log << priority(PRTVERBOSE);
        for (auto& t : m_textures)
            log << "\t" << t.m_name << std::endl;
    }

    void RenderableMesh::loadAnimations(const aiScene* scene)
    {
        log << priority(PRTSTRICT) << "Loading animations..." << std::endl;

        for (int i = 0; i < scene->mNumAnimations; i++)
        {
            aiAnimation* aianim = scene->mAnimations[i];

            AnimationClip anim;
            anim.name = std::string(aianim->mName.C_Str());
            anim.duration_ticks = aianim->mDuration;
            anim.tps = aianim->mTicksPerSecond;
            anim.node_animations.resize(m_nodetree.size());

            log << priority(PRTSTRICT)
                << "Loading animation '" << anim.name
                << "', dur in ticks " << anim.duration_ticks
                << ", tps " << anim.tps
                << ", nbr channels " << aianim->mNumChannels
                << std::endl;

            for (int j = 0; j < aianim->mNumChannels; j++)
            {
                aiNodeAnim* ainode_anim = aianim->mChannels[j];
                NodeKeyframes node_anim;
                node_anim.is_used = true;
                auto name = std::string(ainode_anim->mNodeName.C_Str());

                log << priority(PRTVERBOSE)
                    << "\tLoading channel " << name
                    << ", nbr pos keys  " << ainode_anim->mNumPositionKeys
                    << ", nbr scale keys  " << ainode_anim->mNumScalingKeys
                    << ", nbr rot keys  " << ainode_anim->mNumRotationKeys
                    << std::endl;

                for (int k = 0; k < ainode_anim->mNumPositionKeys; k++)
                {
                    glm::vec3 pos_key = aivec_to_glmvec(ainode_anim->mPositionKeys[k].mValue);
                    node_anim.pos_keys.push_back(pos_key);
                }
                for (int k = 0; k < ainode_anim->mNumScalingKeys; k++)
                {
                    glm::vec3 scale_key = aivec_to_glmvec(ainode_anim->mScalingKeys[k].mValue);
                    node_anim.scale_keys.push_back(scale_key);
                }
                for (int k = 0; k < ainode_anim->mNumRotationKeys; k++)
                {
                    glm::quat rot_key = aiquat_to_glmquat(ainode_anim->mRotationKeys[k].mValue);
                    node_anim.rot_keys.push_back(rot_key);
                }

                auto index = m_nodetree.find_node_index(name);
                if (index != EENG_NULL_INDEX)
                    anim.node_animations[index] = node_anim;
            }

            m_animations.push_back(anim);
        }

        log << priority(PRTSTRICT) << "Animations in total " << m_animations.size() << std::endl;
    }

    glm::mat4 RenderableMesh::animateNode(
        size_t node_index,
        const AnimationClip* anim,
        float ntime) const
    {
        const auto& node = m_nodetree.get_payload_at(node_index);
        if (!anim) return node.local_tfm;
        if (!anim->node_animations[node_index].is_used) return node.local_tfm;

        auto& node_keyframes = anim->node_animations[node_index];
        const auto& rot_keys = node_keyframes.rot_keys;
        const auto& pos_keys = node_keyframes.pos_keys;
        const auto& scale_keys = node_keyframes.scale_keys;
        const size_t nbr_pos_keys = pos_keys.size();
        const size_t nbr_rot_keys = rot_keys.size();
        const size_t nbr_scale_keys = scale_keys.size();

        // Blend translation keys
        float pos_indexf = ntime * (nbr_pos_keys - 1ull);
        size_t pos_index0 = std::floor(pos_indexf);
        size_t pos_index1 = std::min(pos_index0 + 1ull, nbr_pos_keys - 1ull);
        const auto blendpos = glm::mix(pos_keys[pos_index0], pos_keys[pos_index1], pos_indexf - pos_index0);

        // Blend rotation keys
        float rot_indexf = ntime * (nbr_rot_keys - 1ull);
        size_t rot_index0 = std::floor(rot_indexf);
        size_t rot_index1 = std::min(rot_index0 + 1ull, nbr_rot_keys - 1ull);
        const auto blendrot = glm::slerp(rot_keys[rot_index0], rot_keys[rot_index1], rot_indexf - rot_index0);

        // Blend scaling keys
        float scale_indexf = ntime * (nbr_scale_keys - 1ull);
        size_t scale_index0 = std::floor(scale_indexf);
        size_t scale_index1 = std::min(scale_index0 + 1ull, nbr_scale_keys - 1ull);
        const auto blendscale = glm::mix(scale_keys[scale_index0], scale_keys[scale_index1], scale_indexf - scale_index0);

        // Concatenate
        const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), blendpos);
        const glm::mat4 rotationMatrix = glm::mat4_cast(blendrot);
        const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), blendscale);
        return translationMatrix * rotationMatrix * scaleMatrix;
    }

    glm::mat4 RenderableMesh::animateBlendNode(
        size_t node_index,
        const AnimationClip* anim0,
        const AnimationClip* anim1,
        float ntime0,
        float ntime1,
        float frac) const
    {
        assert(frac >= 0.0f && frac <= 1.0f);
        const auto& node = m_nodetree.get_payload_at(node_index);

        assert(anim0 && anim1);
        if (!anim0->node_animations[node_index].is_used) return node.local_tfm;
        if (!anim1->node_animations[node_index].is_used) return node.local_tfm;

        const NodeKeyframes* node_keyframe[] = {
             &anim0->node_animations[node_index],
             &anim1->node_animations[node_index]
        };
        float ntime[] = { ntime0, ntime1 };
        glm::vec3 blendpos[2];
        glm::quat blendrot[2];
        glm::vec3 blendscale[2];

        for (int i = 0; i < 2; i++)
        {
            const auto& pos_keys = node_keyframe[i]->pos_keys;
            const auto& rot_keys = node_keyframe[i]->rot_keys;
            const auto& scale_keys = node_keyframe[i]->scale_keys;
            const size_t nbr_pos_keys = pos_keys.size();
            const size_t nbr_rot_keys = rot_keys.size();
            const size_t nbr_scale_keys = scale_keys.size();

            // Blend translation keys
            float pos_indexf = ntime[i] * (nbr_pos_keys - 1ull);
            size_t pos_index0 = std::floor(pos_indexf);
            size_t pos_index1 = std::min(pos_index0 + 1ull, nbr_pos_keys - 1ull);
            blendpos[i] = glm::mix(pos_keys[pos_index0], pos_keys[pos_index1], pos_indexf - pos_index0);

            // Blend rotation keys
            float rot_indexf = ntime[i] * (nbr_rot_keys - 1ull);
            size_t rot_index0 = std::floor(rot_indexf);
            size_t rot_index1 = std::min(rot_index0 + 1ull, nbr_rot_keys - 1ull);
            blendrot[i] = glm::slerp(rot_keys[rot_index0], rot_keys[rot_index1], rot_indexf - rot_index0);

            // Blend scaling keys
            float scale_indexf = ntime[i] * (nbr_scale_keys - 1ull);
            size_t scale_index0 = std::floor(scale_indexf);
            size_t scale_index1 = std::min(scale_index0 + 1ull, nbr_scale_keys - 1ull);
            blendscale[i] = glm::mix(scale_keys[scale_index0], scale_keys[scale_index1], scale_indexf - scale_index0);
        }

        // Use dual quaternions to blend rotations and translations between clips
        glm::dualquat dqA = glm::dualquat(blendrot[0], blendpos[0]);
        glm::dualquat dqB = glm::dualquat(blendrot[1], blendpos[1]);
        glm::dualquat dq = glm::normalize(glm::lerp(dqA, dqB, frac));

        // Apply blended scaling (not supported by dual quaternions)
        glm::mat4 M = dualquatToMat4(dq);
        M = glm::scale(M, glm::mix(blendscale[0], blendscale[1], frac));

        return M;
    }

    void RenderableMesh::animate(
        int anim_index,
        float time,
        AnmationTimeFormat animTimeFormat)
    {
        AnimationClip* anim = nullptr;
        if (anim_index >= 0 && anim_index < getNbrAnimations())
        {
            anim = &m_animations[anim_index];
        }

        // Convert to normalized time
        float ntime = time;
        if (anim && animTimeFormat == AnmationTimeFormat::RealTime)
        {
            const float dur_ticks = anim->duration_ticks;
            const float animdur_sec = dur_ticks / anim->tps;
            const float animtime_sec = fmod(time, animdur_sec);
            const float animtime_ticks = animtime_sec * anim->tps;
            ntime = animtime_ticks / dur_ticks;
        }

        // Traverse the node tree and animate all nodes
        //m_nodetree.traverse_progressive(
        m_nodetree.traverse_depthfirst(
            [&](SkeletonNode* node, SkeletonNode* parent_node, size_t node_index, size_t parent_index)
            {
                node->global_tfm = animateNode(node_index, anim, ntime);

                if (parent_node)
                    node->global_tfm = parent_node->global_tfm * node->global_tfm;
            });

        m_model_aabb.reset();
        for (int i = 0; i < m_bones.size(); i++)
        {
            const auto& node_tfm = m_nodetree.get_payload_at(m_bones[i].node_index).global_tfm;
            const auto& boneIB_tfm = m_bones[i].inversebind_tfm;
            glm::mat4 M = node_tfm * boneIB_tfm;

            // Bone matrices
            boneMatrices[i] = M;

            // AABBs
            if (m_bone_aabbs_bind[i])
            {
                m_bone_aabbs_pose[i] = m_bone_aabbs_bind[i].post_transform(glm::vec3(M[3]), glm::mat3(M));
                m_model_aabb.grow(m_bone_aabbs_pose[i]);
            }
        }

        // Puts mesh AABB's in pose and have them grow model AABB
        for (int i = 0; i < m_meshes.size(); i++)
        {
            if (!m_mesh_aabbs_bind[i])
                continue;
            if (m_meshes[i].is_skinned)
                continue;

            if (m_meshes[i].node_index > EENG_NULL_INDEX)
            {
                glm::mat4 M = m_nodetree.get_payload_at(m_meshes[i].node_index).global_tfm;
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i].post_transform(glm::vec3(M[3]), glm::mat3(M));
            }
            else
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i];

            m_model_aabb.grow(m_mesh_aabbs_pose[i]);
        }
    }

    void RenderableMesh::animateBlend(
        int anim_index0,
        int anim_index1,
        float time0,
        float time1,
        float frac,
        AnmationTimeFormat animTimeFormat0,
        AnmationTimeFormat animTimeFormat1)
    {
        EENG_ASSERT(anim_index0 >= 0 && anim_index0 < getNbrAnimations(), "{0} is not a valid clip index", anim_index0);
        EENG_ASSERT(anim_index1 >= 0 && anim_index1 < getNbrAnimations(), "{0} is not a valid clip index", anim_index1);

        AnimationClip* anim0 = &m_animations[anim_index0];
        AnimationClip* anim1 = &m_animations[anim_index1];

        // Convert to normalized time
        float ntime0 = time0;
        float ntime1 = time1;
        if (anim0 && animTimeFormat0 == AnmationTimeFormat::RealTime)
        {
            const float dur_ticks = anim0->duration_ticks;
            const float animdur_sec = dur_ticks / anim0->tps;
            const float animtime_sec = fmod(time0, animdur_sec);
            const float animtime_ticks = animtime_sec * anim0->tps;
            ntime0 = animtime_ticks / dur_ticks;
        }
        if (anim1 && animTimeFormat1 == AnmationTimeFormat::RealTime)
        {
            const float dur_ticks = anim1->duration_ticks;
            const float animdur_sec = dur_ticks / anim1->tps;
            const float animtime_sec = fmod(time1, animdur_sec);
            const float animtime_ticks = animtime_sec * anim1->tps;
            ntime1 = animtime_ticks / dur_ticks;
        }

        // Traverse the node tree and animate all nodes
        //m_nodetree.traverse_progressive(
        m_nodetree.traverse_depthfirst(
            [&](SkeletonNode* node, SkeletonNode* parent_node, size_t node_index, size_t parent_index)
            {
                node->global_tfm = animateBlendNode(node_index, anim0, anim1, ntime0, ntime1, frac);

                if (parent_node)
                    node->global_tfm = parent_node->global_tfm * node->global_tfm;
            });
        // m_nodetree.traverse_depthfirst(
        //     [&](SkeletonNode* node, SkeletonNode* parent_node, size_t node_index, size_t parent_index)
        //     {
        //         node->global_tfm = animateBlendNode(node_index, anim0, anim1, ntime0, ntime1, frac);

        //         if (parent_node)
        //             node->global_tfm = parent_node->global_tfm * node->global_tfm;
        //     });

        m_model_aabb.reset();
        for (int i = 0; i < m_bones.size(); i++)
        {
            const auto& node_tfm = m_nodetree.get_payload_at(m_bones[i].node_index).global_tfm;
            const auto& boneIB_tfm = m_bones[i].inversebind_tfm;
            glm::mat4 M = node_tfm * boneIB_tfm;

            // Bone matrices
            boneMatrices[i] = M;

            // AABBs
            if (m_bone_aabbs_bind[i])
            {
                m_bone_aabbs_pose[i] = m_bone_aabbs_bind[i].post_transform(glm::vec3(M[3]), glm::mat3(M));
                m_model_aabb.grow(m_bone_aabbs_pose[i]);
            }
        }

        // Puts mesh AABB's in pose and have them grow model AABB
        for (int i = 0; i < m_meshes.size(); i++)
        {
            if (!m_mesh_aabbs_bind[i])
                continue;
            if (m_meshes[i].is_skinned)
                continue;

            if (m_meshes[i].node_index > EENG_NULL_INDEX)
            {
                glm::mat4 M = m_nodetree.get_payload_at(m_meshes[i].node_index).global_tfm;
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i].post_transform(glm::vec3(M[3]), glm::mat3(M));
            }
            else
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i];

            m_model_aabb.grow(m_mesh_aabbs_pose[i]);
        }
    }

    unsigned RenderableMesh::getNbrAnimations() const
    {
        return (unsigned)m_animations.size();
    }

    std::string RenderableMesh::getAnimationName(unsigned i) const
    {
        return (i < getNbrAnimations() ? m_animations[i].name : "");
    }

    RenderableMesh::~RenderableMesh()
    {
        for (auto& t : m_textures)
            t.free();

        if (m_Buffers[0] != 0)
        {
            glDeleteBuffers((GLsizei)numelem(m_Buffers), m_Buffers);
        }

        if (m_VAO != 0)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
    }

} // namespace eeng
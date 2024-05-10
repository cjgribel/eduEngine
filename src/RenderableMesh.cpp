//
//  assimp_mesh.cpp
//  glfwassimp
//
//  Created by Carl Johan Gribel on 2018-02-21.
//  Copyright © 2018 Carl Johan Gribel. All rights reserved.
//

#include "RenderableMesh.hpp"
#include <assimp/version.h>
#include "interp.h"
#include "ShaderLoader.h"
#include "parseutil.h"
// #include "glyph_renderer.hpp"

namespace eeng
{

    using linalg::dualquatf;

    namespace
    {
        inline v3f lerp_v3f(const v3f &v0, const v3f &v1, float t)
        {
            return v0 * (1.0f - t) + v1 * t;
        }
    }

    inline m4f to_m4f(const aiMatrix4x4 &aim)
    {
        return {
            aim.a1, aim.a2, aim.a3, aim.a4,
            aim.b1, aim.b2, aim.b3, aim.b4,
            aim.c1, aim.c2, aim.c3, aim.c4,
            aim.d1, aim.d2, aim.d3, aim.d4};
    }

    inline quatf to_quatf(const aiQuaternion &aiq)
    {
        return {aiq.w, aiq.x, aiq.y, aiq.z};
    }

    inline v3f to_v3f(const aiVector3D &aiv)
    {
        return {aiv.x, aiv.y, aiv.z};
    }

    void RenderableMesh::SkinData::add_weight(unsigned bone_index, float bone_weight)
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
        //    loggertest();

        // this->dbgrenderer = dbgrenderer;
    }

    // Legacy
    void RenderableMesh::load(const std::string &file, bool append_animations)
    {
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
            aiProcess_CalcTangentSpace |
            aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_Triangulate |
            aiProcess_GenUVCoords |
            aiProcess_SortByPType |
            aiProcess_FlipUVs | // added
            aiProcess_OptimizeGraph;

        load(file, xiflags, aiflags);
    }

    void RenderableMesh::load(const std::string &file,
                              unsigned xiflags,
                              unsigned aiflags)

    {
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
        const aiScene *aiscene = aiimporter.ReadFile(file, aiflags);

        if (!aiscene)
            throw std::runtime_error(aiimporter.GetErrorString());
        log << priority(PRTSTRICT) << "Assimp load OK\n";

        // Load animations to a previously loaded model
        if (append_animations)
        {
            log << priority(PRTSTRICT) << "Appending animations... \n";

            if (!m_meshes.size())
                throw std::runtime_error("Cannot append animations to an empty model\n");

            load_animations(aiscene);

            log << priority(PRTSTRICT) << "Done appending animations.\n";
            return;
        }

        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);
        glGenBuffers(numelem(m_Buffers), m_Buffers);
        load_scene(aiscene, filepath);
        glBindVertexArray(0);

        load_nodes(aiscene->mRootNode);
        m_nodetree.debug_print({filepath + filename + "_nodetree.txt", PRTVERBOSE});

        load_animations(aiscene);

        // default_shader = createShaderProgram(vshader.c_str(), fshader.c_str());
        // placeholder_texture = create_checker_texture();

        mSceneAABB = measure_scene(aiscene); // Only captures bind pose.

        // Traverse the hierarchy
        animate(-1, 0.0f);
    }

    void RenderableMesh::remove_translation_keys(const std::string &node_name)
    {
        remove_translation_keys(m_nodetree.find_node_index(node_name));
    }

    void RenderableMesh::remove_translation_keys(int node_index)
    {
        for (auto &anim : m_animations)
        {
            EENG_ASSERT(node_index <= anim.node_animations.size(), "{0} is not a valid node index", node_index);
            auto &pos_keys = anim.node_animations[node_index].pos_keys;
            for (auto &pk : pos_keys)
                pk = {0, pk.y, 0};
        }
    }

    bool RenderableMesh::load_scene(const aiScene *aiscene, const std::string &filename)
    {
        unsigned scene_nbr_meshes = aiscene->mNumMeshes;
        unsigned scene_nbr_mtl = aiscene->mNumMaterials;
        unsigned scene_nbr_vertices = 0;
        unsigned scene_nbr_indices = 0;

        // Print some debug info
        log << priority(PRTSTRICT) << "Scene overview" << std::endl;
        //    std::cout << "\thas meshes: " << (pScene->HasMeshes()?"yes":"no") << std::endl;
        //    std::cout << "\thas materials: " << (pScene->HasMaterials()?"yes":"no") << std::endl;
        //    std::cout << "\thas embedded textures: " << (pScene->HasTextures()?"yes":"no") << std::endl;
        //    std::cout << "\thas animations: " << (pScene->HasAnimations()?"yes":"no") << std::endl;
        //    std::cout << "\thas lights: " << (pScene->HasLights()?"yes":"no") << std::endl;
        //    std::cout << "\thas cameras: " << (pScene->HasCameras()?"yes":"no") << std::endl;
        log << "\t" << scene_nbr_meshes << " meshes" << std::endl;
        log << "\t" << scene_nbr_mtl << " materials" << std::endl;
        // std::cout << "\t" << scene_nbr_vertices << " vertices" << std::endl;
        // std::cout << "\t" << scene_nbr_indices << " indices" << std::endl;
        log << "\t" << aiscene->mNumTextures << " embedded textures" << std::endl;
        log << "\t" << aiscene->mNumAnimations << " animations" << std::endl;
        log << "\t" << aiscene->mNumLights << " lights" << std::endl;
        log << "\t" << aiscene->mNumCameras << " cameras" << std::endl;
        // Animations
        log << "Animations:\n";
        for (int i = 0; i < aiscene->mNumAnimations; i++)
        {
            aiAnimation *anim = aiscene->mAnimations[i];
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

        std::vector<v3f> scene_positions;
        std::vector<v3f> scene_normals;
        std::vector<v3f> scene_tangents;
        std::vector<v3f> scene_binormals;
        std::vector<v2f> scene_texcoords;
        std::vector<SkinData> scene_skinweights;
        std::vector<uint> scene_indices;

        // Count vertices and indices of the whole scene
        for (unsigned i = 0; i < m_meshes.size(); i++)
        {
            unsigned mesh_nbr_vertices = aiscene->mMeshes[i]->mNumVertices;
            unsigned mesh_nbr_indices = aiscene->mMeshes[i]->mNumFaces * 3; // Assume mesh is triangulated
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
            const aiMesh *paiMesh = aiscene->mMeshes[i];
            load_mesh(i,
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

        //    points = scene_positions;
        //    points_AABB.reset();

        // Model & bone AABB's
        boneMatrices.resize(m_bones.size());
        m_bone_aabbs_bind.resize(m_bones.size()); // Constructor resets AABB
        m_bone_aabbs_pose.resize(m_bones.size());

        m_mesh_aabbs_bind.resize(m_meshes.size());
        m_mesh_aabbs_pose.resize(m_meshes.size());

        for (int i = 0; i < m_meshes.size(); i++)
        {
            const auto &mesh = m_meshes[i];
            if (mesh.is_skinned)
            {
                // bones
                for (int j = mesh.base_vertex; j < mesh.base_vertex + mesh.nbr_vertices; j++)
                {
                    for (int k = 0; k < NUM_BONES_PER_VERTEX; k++)
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

//    for (int i = 0; i < scene_positions.size(); i++)
//    {
////        for (int j=0; j<scene_skinweights.size(); j++)
////        {
//            for (int k=0; k < NUM_BONES_PER_VERTEX; k++) {
//                if ( scene_skinweights[i].bone_weights[k] > 0 )
//                bone_aabbs[ scene_skinweights[i].bone_indices[k] ].grow( scene_positions[i] );
////            }
//        }
//    }
//
#endif

#if 0
    std::vector<m4f> Mbones;
    bone_transform(0, Mbones);
    std::cout << "Nbr bone transforms " << Mbones.size() << "\n";
    for (auto &m : Mbones) {
        std::cout << m << "\n";
    }
#endif

        load_materials(aiscene, filename);
        //    if (!InitMaterials(pScene, Filename)) {
        //        return false;
        //    }

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
        glVertexAttribIPointer(BONE_INDEX_LOCATION, 4, GL_UNSIGNED_INT, sizeof(SkinData), (const GLvoid *)0);
        glEnableVertexAttribArray(BONE_WEIGHT_LOCATION);
        glVertexAttribPointer(BONE_WEIGHT_LOCATION, 4, GL_FLOAT, GL_FALSE, sizeof(SkinData), (const GLvoid *)16);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Buffers[IndexBuffer]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(scene_indices[0]) * scene_indices.size(), &scene_indices[0], GL_STATIC_DRAW);

        CheckAndThrowGLErrors();
        return true;

        // return GLCheckError();
    }

    void RenderableMesh::load_mesh(uint meshindex,
                                   const aiMesh *aimesh,
                                   std::vector<v3f> &scene_positions,
                                   std::vector<v3f> &scene_normals,
                                   std::vector<v3f> &scene_tangents,
                                   std::vector<v3f> &scene_binormals,
                                   std::vector<v2f> &scene_texcoords,
                                   std::vector<SkinData> &scene_skindata,
                                   std::vector<unsigned int> &scene_indices)
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
            const aiVector3D *pPos = &(aimesh->mVertices[i]);
            const aiVector3D *pNormal = (aimesh->HasNormals() ? &(aimesh->mNormals[i]) : &v3zero);
            const aiVector3D *pTangent = (aimesh->HasTangentsAndBitangents() ? &(aimesh->mTangents[i]) : &v3zero);
            const aiVector3D *pBinormal = (aimesh->HasTangentsAndBitangents() ? &(aimesh->mBitangents[i]) : &v3zero);
            const aiVector3D *pTexCoord = (aimesh->HasTextureCoords(0) ? &(aimesh->mTextureCoords[0][i]) : &v3zero);

            scene_positions.push_back({pPos->x, pPos->y, pPos->z});
            scene_normals.push_back({pNormal->x, pNormal->y, pNormal->z});
            scene_tangents.push_back({pTangent->x, pTangent->y, pTangent->z});
            scene_binormals.push_back({pBinormal->x, pBinormal->y, pBinormal->z});
            scene_texcoords.push_back({pTexCoord->x, pTexCoord->y});
        }

        load_bones(meshindex, aimesh, scene_skindata);

#if 0
    // DBG: check min & max nbr of weights per bone
    int min_nbr_weights = 100, max_nbr_weights = 0;
    for (auto &b : scene_skindata)
    {
        min_nbr_weights = fmin(min_nbr_weights, b.nbr_added);
        max_nbr_weights = fmax(max_nbr_weights, b.nbr_added);
    }
    log << priority(PRTVERBOSE) << "\tNbr of bone weights, min " << min_nbr_weights << ", max " << max_nbr_weights << std::endl;
#endif

        // Populate the index buffer
        for (uint i = 0; i < aimesh->mNumFaces; i++)
        {
            const aiFace &Face = aimesh->mFaces[i];
            assert(Face.mNumIndices == 3);
            scene_indices.push_back(Face.mIndices[0]);
            scene_indices.push_back(Face.mIndices[1]);
            scene_indices.push_back(Face.mIndices[2]);
        }
    }

    AABB_t RenderableMesh::measure_scene(const aiScene *aiscene)
    {
        AABB_t aabb;
        measure_node(aiscene, aiscene->mRootNode, linalg::m4f_1, aabb);

        return aabb;
    }

    void RenderableMesh::measure_node(const aiScene *aiscene,
                                      const aiNode *pNode,
                                      const m4f &M_roottfm,
                                      AABB_t &aabb)
    {
        m4f M_identity = linalg::m4f_1;
        m4f M_nodetfm = M_roottfm * to_m4f(pNode->mTransformation);

        for (int i = 0; i < pNode->mNumMeshes; i++)
        {
            aiMesh *mesh = aiscene->mMeshes[pNode->mMeshes[i]];

            // Skinned meshes are expressed in world/model space,
            // non-skinned meshes are expressed in node space.
            if (mesh->mNumBones)
                measure_mesh(mesh, M_identity, aabb);
            else
                measure_mesh(mesh, M_nodetfm, aabb);
        }

        for (int i = 0; i < pNode->mNumChildren; i++)
        {
            measure_node(aiscene, pNode->mChildren[i], M_nodetfm, aabb);
        }
    }

    void RenderableMesh::measure_mesh(const aiMesh *pMesh,
                                      const m4f &M_roottfm,
                                      AABB_t &aabb)
    {
        for (int i = 0; i < pMesh->mNumVertices; i++)
        {
            v3f v = to_v3f(*(pMesh->mVertices + i));
            aabb.grow((M_roottfm * v.xyz1()).xyz());
        }
    }

    // Load node hierarchy and link nodes to bones & meshes
    void RenderableMesh::load_nodes(aiNode *ainode_root)
    {
        // Load node hierarchy recursively from root
        load_node(ainode_root);

#if 0
    // PAHSE OUT
    // Mark nodes corresponding to bones
    for (int i=0; i<m_nodetree.nodes.size(); i++)
    {
        auto boneit = m_bonehash.find(m_nodetree.nodes[i].name);
        if (boneit != m_bonehash.end())
            m_nodetree.nodes[i].bone_index = boneit->second;
    }
#endif

        // Link nodes to bones (0 or 1) and meshes (0+)
        // Link bones to nodes (1)
        for (int i = 0; i < m_nodetree.nodes.size(); i++)
        {
            // Link node<->meshes (re-retrieve the original assimp node by name)
            // Note: the node transform is ignored during rendering if the mesh
            // is skinned.
            aiNode *ainode = ainode_root->FindNode(m_nodetree.nodes[i].name.c_str());
            for (int j = 0; j < ainode->mNumMeshes; j++)
            {
                // if ( !m_meshes[ ainode->mMeshes[j] ].is_skinned ) // !!!
                m_meshes[ainode->mMeshes[j]].node_index = i;
            }
            m_nodetree.nodes[i].nbr_meshes = ainode->mNumMeshes;

            // Link node<->bone
            auto boneit = m_bonehash.find(m_nodetree.nodes[i].name);
            if (boneit != m_bonehash.end())
            {
                m_bones[boneit->second].node_index = i;
                m_nodetree.nodes[i].bone_index = boneit->second;
            }
        }

        // m_nodetree.reduce();

        // Build node name<->index hash
        // Note: Not currently used, though seems sensical to have.
        for (int i = 0; i < m_nodetree.nodes.size(); i++)
            m_nodehash[m_nodetree.nodes[i].name] = i;
    }

    void RenderableMesh::load_node(aiNode *ainode)
    {
        // Node data
        std::string node_name;   //
        std::string parent_name; //
        m4f transform;           // Local transform = transform relative parent
        // Fetch node data from assimp
        node_name = std::string(ainode->mName.C_Str());
        const aiNode *parent_node = ainode->mParent;
        parent_name = parent_node ? std::string(parent_node->mName.C_Str()) : "";
        transform = to_m4f(ainode->mTransformation);

        // Create & insert node
        SkeletonNode stnode(node_name, transform);
        if (!m_nodetree.insert(stnode, parent_name))
            throw std::runtime_error("Node tree insertion failed, hierarchy corrupt");

        for (int i = 0; i < ainode->mNumChildren; i++)
        {
            load_node(ainode->mChildren[i]);
        }
    }

    void RenderableMesh::load_bones(uint mesh_index,
                                    const aiMesh *aimesh,
                                    std::vector<SkinData> &scene_skindata)
    {
        // Fetch mesh's bones and add them to the m_bones vector
        // Bone contains bone-offset and final transformation
        //
        // Bones may be part of multiple meshes (I assume), so hash those already handled
        // m_bonehash maps bones names (str) with index in m_bones vector

        // Each BONE then keeps its own set of WEIGHTS = {weight, vertex index}

        log << priority(PRTVERBOSE) << aimesh->mNumBones << " bones (nbr weights):\n";

        for (uint i = 0; i < aimesh->mNumBones; i++)
        {
            uint bone_index = 0;
            // TODO aiBone* bone = pMesh->mBones[i];

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
                m_bones[bone_index].inversebind_tfm = to_m4f(aimesh->mBones[i]->mOffsetMatrix);
                // Hash bone w.r.t. its name
                m_bonehash[bone_name] = bone_index;
            }
            else
            {
                bone_index = m_bonehash[bone_name];
            }

            // For all WEIGHTS associated with this bone
            // WEIGHT = {weight, vertex id}
            for (uint j = 0; j < aimesh->mBones[i]->mNumWeights; j++)
            {
                uint vertex_id = m_meshes[mesh_index].base_vertex + aimesh->mBones[i]->mWeights[j].mVertexId;
                float bone_weight = aimesh->mBones[i]->mWeights[j].mWeight;
                scene_skindata[vertex_id].add_weight(bone_index, bone_weight);
            }
        }
    }

    /// @brief Load textures of a given type
    /// @param aimtl
    /// @param tex_type
    /// @param modelDir
    /// @return An index to the loaded texture
    int RenderableMesh::load_texture(const aiMaterial *material, aiTextureType textureType, const std::string &modelDir)
    {
        unsigned nbr_textures = material->GetTextureCount(textureType);

        if (!nbr_textures)
            return NO_TEXTURE;
        if (nbr_textures > 1)
            throw std::runtime_error("Multiple textures of type " + std::to_string(textureType) + ", aborting. Nbr = " + std::to_string(nbr_textures));

        // Fetch texture properties from assimp
        aiString ai_texpath;            //
        aiTextureMapping ai_texmap;     //
        unsigned int ai_uvindex;        // currently unused
        float ai_blend;                 // currently unused
        aiTextureOp ai_texop;           // currently unused
        aiTextureMapMode ai_texmapmode; //
        if (material->GetTexture(textureType,
                                 0,
                                 &ai_texpath,
                                 &ai_texmap,
                                 &ai_uvindex,
                                 &ai_blend,
                                 &ai_texop,
                                 &ai_texmapmode) != AI_SUCCESS)
            return NO_TEXTURE;
        // aiReturn tex_ret = material->GetTexture(textureType,
        //                                      0,
        //                                      &ai_texpath,
        //                                      &ai_texmap,
        //                                      &ai_uvindex,
        //                                      &ai_blend,
        //                                      &ai_texop,
        //                                      &ai_texmapmode);
        // if (tex_ret != AI_SUCCESS)
        //     return NO_TEXTURE;

        // Relative texture path, e.g. "/textures/texture.png"
        std::string textureRelPath{ai_texpath.C_Str()};

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
            m_textures[textureIndex].set_address_mode({adr_mode, adr_mode});
        }

        return textureIndex;
    }

    // bool SkinnedMesh::InitMaterials(const aiScene* pScene, const string& Filename)
    void RenderableMesh::load_materials(const aiScene *aiscene, const std::string &file)
    {
        std::string local_filepath = get_parentdir(file);

        log << priority(PRTSTRICT) << "Loading materials...\n";
        log << "\tNum materials " << aiscene->mNumMaterials << std::endl;
        log << "\tParent dir: " << local_filepath << std::endl;

        // (191210) Load embedded textures to texture array, using plain indices as
        // hash strings. If any regular texture is named e.g. '1', without an
        // extension (which it really shouldn't), there will be a conflict in the
        // name hash.
        log << "Embedded textures: " << aiscene->mNumTextures << std::endl;
        log << priority(PRTVERBOSE);

        m_embedded_textures_ofs = (unsigned)m_textures.size();
        for (int i = 0; i < aiscene->mNumTextures; i++)
        {
            aiTexture *aitexture = aiscene->mTextures[i];
            std::string filename = get_filename(aitexture->mFilename.C_Str());
            // std::string filename = std::to_string(i);

            Texture2D texture;
            if (aitexture->mHeight)
            {
                // Raw embedded image data
                texture.load_image(filename,
                                   (unsigned char *)aitexture->pcData,
                                   aitexture->mWidth,
                                   aitexture->mHeight,
                                   4);
                log << priority(PRTSTRICT) << "Loaded uncompressed embedded texture " << texture << std::endl;
            }
            else
            {
                // Compressed embedded image data
                texture.load_from_memory(filename,
                                         (unsigned char *)aitexture->pcData,
                                         sizeof(aiTexel) * (aitexture->mWidth));
                log << priority(PRTSTRICT) << "Loaded compressed embedded texture " << texture << std::endl;
            }
            // texture.load_from_memory(filename, aitexture);

            m_texturehash[filename] = (unsigned)m_textures.size();
            m_textures.push_back(texture);

            // std::cout << "\t\tLoaded embedded texture: " << filename << ", " << aitexture->mFilename.data << std::endl;
        }
        log << priority(PRTSTRICT) << "Loaded " << aiscene->mNumTextures << " embedded textures\n";

        //    bool Ret = true;

        // Initialize the materials
        for (uint i = 0; i < aiscene->mNumMaterials; i++)
        {
            const aiMaterial *pMaterial = aiscene->mMaterials[i];
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
                mtl.Ka = {aic.r, aic.g, aic.b};
            if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aic))
                mtl.Kd = {aic.r, aic.g, aic.b};
            if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, aic))
                mtl.Ks = {aic.r, aic.g, aic.b};
            pMaterial->Get(AI_MATKEY_SHININESS, mtl.shininess);

            // std::cout << "Ka, Kd, Ks, shininess:"
            //           << mtl.Ka << ", " << mtl.Kd << ", " << mtl.Ks << ", " << mtl.shininess << std::endl;
            //         if ( pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, mtl.Kd) == AI_SUCCESS )
            //             std::cout << "HAS DIFFUSE " << mtlname.data << std::endl;

            // Fetch common textures
            log << "Loading textures..." << std::endl;
#if 1
            using TextureType = PhongMaterial::TextureTypeIndex;
            mtl.textureIndices[TextureType::Diffuse] = load_texture(pMaterial, aiTextureType_DIFFUSE, local_filepath);
            mtl.textureIndices[TextureType::Normal] = load_texture(pMaterial, aiTextureType_NORMALS, local_filepath);
            mtl.textureIndices[TextureType::Specular] = load_texture(pMaterial, aiTextureType_SPECULAR, local_filepath);
            mtl.textureIndices[TextureType::Opacity] = load_texture(pMaterial, aiTextureType_OPACITY, local_filepath);
#else
            mtl.diffuse_texture_index = load_texture(pMaterial, aiTextureType_DIFFUSE, local_filepath);
            mtl.normal_texture_index = load_texture(pMaterial, aiTextureType_NORMALS, local_filepath);
            mtl.specular_texture_index = load_texture(pMaterial, aiTextureType_SPECULAR, local_filepath);
            mtl.reflective_texture_index = load_texture(pMaterial, aiTextureType_REFLECTION, local_filepath);
            mtl.opacity_texture_index = load_texture(pMaterial, aiTextureType_OPACITY, local_filepath);
#endif

            // Fallback: assimp seems to label OBJ normal maps as HEIGHT type textures.
            if (mtl.textureIndices[TextureType::Normal] == NO_TEXTURE)
                mtl.textureIndices[TextureType::Normal] = load_texture(pMaterial, aiTextureType_HEIGHT, local_filepath);
            // if (mtl.normal_texture_index == NO_TEXTURE)
            //     mtl.normal_texture_index = load_texture(pMaterial, aiTextureType_HEIGHT, local_filepath);

            log << "Done loading textures" << std::endl;

            m_materials[i] = mtl;
        }
        log << "Done loading materials" << std::endl;

        log << priority(PRTSTRICT) << "Num materials " << m_materials.size() << std::endl;
        //    for (auto& t : m_materials)
        //        std::cout << "\t" << t.diffuse_texture_index << std::endl;

        log << priority(PRTSTRICT) << "Num textures " << m_textures.size() << std::endl;
        log << priority(PRTVERBOSE);
        for (auto &t : m_textures)
            log << "\t" << t.m_name << std::endl;

        //    std::cout << "Num unique textures " << m_texturehash.size() << std::endl;
        //    for (auto t : m_texturehash)
        //        std::cout << "\t" << t.first << std::endl;

        // return Ret;
    }

    void RenderableMesh::load_animations(const aiScene *scene)
    {
        log << priority(PRTSTRICT) << "Loading animations..." << std::endl;

        for (int i = 0; i < scene->mNumAnimations; i++)
        {
            aiAnimation *aianim = scene->mAnimations[i];

            AnimationClip anim;
            anim.name = std::string(aianim->mName.C_Str());
            anim.duration_ticks = aianim->mDuration;
            anim.tps = aianim->mTicksPerSecond;
            anim.node_animations.resize(m_nodetree.nodes.size());

            log << priority(PRTSTRICT)
                << "Loading animation '" << anim.name
                << "', dur in ticks " << anim.duration_ticks
                << ", tps " << anim.tps
                << ", nbr channels " << aianim->mNumChannels
                << std::endl;

            for (int j = 0; j < aianim->mNumChannels; j++)
            {
                aiNodeAnim *ainode_anim = aianim->mChannels[j];
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
                    v3f pos_key = to_v3f(ainode_anim->mPositionKeys[k].mValue);
                    node_anim.pos_keys.push_back(pos_key);
                }
                for (int k = 0; k < ainode_anim->mNumScalingKeys; k++)
                {
                    v3f scale_key = to_v3f(ainode_anim->mScalingKeys[k].mValue);
                    node_anim.scale_keys.push_back(scale_key);
                }
                for (int k = 0; k < ainode_anim->mNumRotationKeys; k++)
                {
                    quatf rot_key = to_quatf(ainode_anim->mRotationKeys[k].mValue);
                    node_anim.rot_keys.push_back(rot_key);
                }

                auto index = m_nodetree.find_node_index(name);
                if (index != EENG_NULL_INDEX)
                    anim.node_animations[index] = node_anim;
            }

#if 0
        // DEV: remove xz pos keys for a (root) node
        auto& pos_keys = anim.node_animations[1].pos_keys;
        //std::vector<v3f> &pos_keys = anim.node_animations[anim.node_animation_hash["mixamorig:Hips"]].pos_keys;
        //std::vector<v3f> &pos_keys = anim.node_animations[anim.node_animation_hash["Hips"]].pos_keys;
        for (auto &pk : pos_keys)
            pk = {0, pk.y, 0};
#endif

            m_animations.push_back(anim);
        }

        log << priority(PRTSTRICT) << "Animations in total " << m_animations.size() << std::endl;
    }

    // get_key_tfm_at_time
    m4f RenderableMesh::blend_transform_at_time(const AnimationClip *anim,
                                                const NodeKeyframes &nodeanim,
                                                float time) const
    {
        float dur_ticks = anim->duration_ticks; /**/ // dur_ticks *= (float)(91-5)/292;
        float animdur_sec = dur_ticks / (anim->tps * 1);
        float animtime_sec = fmod(time, animdur_sec);
        float animtime_ticks = animtime_sec * anim->tps * 1;
        float animtime_nrm = animtime_ticks / dur_ticks;

        //    std::cout << animtime_nrm << std::endl;

        /**/
        //    float l = 292, s = 5/l, e = 91/l;
        //    animtime_nrm = s + animtime_nrm*(e-s);

        return blend_transform_at_frac(anim, nodeanim, animtime_nrm);
    }

    m4f RenderableMesh::blend_transform_at_frac(const AnimationClip *anim,
                                                const NodeKeyframes &nodeanim,
                                                float frac) const
    {
        // Translation
        float pos_indexf = frac * (nodeanim.pos_keys.size() - 1);
        unsigned pos_index0 = std::floor(pos_indexf);
        unsigned pos_index1 = std::min<unsigned>(pos_index0 + 1,
                                                 (unsigned)nodeanim.pos_keys.size() - 1);
        const v3f blendpos = lerp_v3f(nodeanim.pos_keys[pos_index0],
                                      nodeanim.pos_keys[pos_index1],
                                      pos_indexf - pos_index0);

        // Rotation
        float rot_indexf = frac * (nodeanim.rot_keys.size() - 1);
        unsigned rot_index0 = std::floor(rot_indexf);
        unsigned rot_index1 = std::min<unsigned>(rot_index0 + 1, (unsigned)nodeanim.rot_keys.size() - 1);
        const quatf &rot0 = nodeanim.rot_keys[rot_index0];
        const quatf &rot1 = nodeanim.rot_keys[rot_index1];

        const quatf blendrot = qnlerp(rot0,
                                      rot1,
                                      rot_indexf - rot_index0);

        //
        //    aiQuaternion q0 = aiQuaterniont<float>(rot0.qw, rot0.qx, rot0.qy, rot0.qz);
        //    aiQuaternion q1 = aiQuaterniont<float>(rot1.qw, rot1.qx, rot1.qy, rot1.qz);
        //    aiQuaternion qb;
        //    aiQuaterniont<float>::Interpolate(qb, q0, q1, rot_indexf-rot_index0);
        //    const quatf blendrot = quatf_from_aiQuaternion(qb);
        //

        // Scaling
        float scale_indexf = frac * (nodeanim.scale_keys.size() - 1);
        unsigned scale_index0 = std::floor(scale_indexf);
        unsigned scale_index1 = std::min<unsigned>(scale_index0 + 1,
                                                   (unsigned)nodeanim.scale_keys.size() - 1);
        const v3f blendscale = lerp_v3f(nodeanim.scale_keys[scale_index0],
                                        nodeanim.scale_keys[scale_index1],
                                        scale_indexf - scale_index0);

        //    return m4f(blendrot);
        return m4f::translation(blendpos) * m4f(blendrot) * m4f::scaling(blendscale);

#if 0
    // Dual quaternion interpolation
    linalg::dualquatf dq0, dq1, dqblend;
    dq0 = dualquatf(m3f(rot0), nodeanim.pos_keys[pos_index0]);
    dq1 = dualquatf(m3f(rot1), nodeanim.pos_keys[pos_index1]);
    float f0 = rot_indexf-rot_index0, f1 = 1.0f-f0;
    float c0 = f0;
    float c1 = (dq0.real.dot(dq1.real) < 0)? -f1 : f1;
    dqblend = dq0*c1 + dq1*c0;
    dqblend.normalize();
    return dqblend.get_homogeneous_matrix() * m4f::scaling(blendscale);
#else
        // Linear interpolation
        // return M0*bc.x + M1*bc.y;
#endif
    }

    float m4f_maxdiff(const m4f &m0, const m4f &m1)
    {
        float maxdiff = 0;
        for (int i = 0; i < 16; i++)
            maxdiff = fmaxf(maxdiff, fabsf(m0.array[i] - m1.array[i]));
        return maxdiff;
    }

    void RenderableMesh::animate(int anim_index,
                                 float time)
    //  std::vector<m4f> &bone_transforms)
    {
        // TRAVERSE & TRANSFORM NODE nodes
        // Use either node or (if available) keyframe transformations

        AnimationClip *anim = nullptr;
        // float TimeInTicks = 0;
        if (anim_index >= 0 && anim_index < get_nbr_animations())
        {
            anim = &m_animations[anim_index];
            // TimeInTicks = time * anim->tps;
        }

        for (auto &node : m_nodetree.nodes)
            node.global_tfm = m4f_1;

        // std::cout << time << std::endl;

        int node_index = 0;
        while (node_index < m_nodetree.nodes.size())
        {
            m4f node_tfm = m_nodetree.nodes[node_index].local_tfm;
            m4f global_tfm;

            // If an animation key is available, use it to replace the node tfm
            if (anim)
            {
                const auto &node_anim = anim->node_animations[node_index];
                if (node_anim.is_used)
                    node_tfm = blend_transform_at_time(anim, node_anim, time);
            }

            // Apply parent transform
            const auto parent_ofs = m_nodetree.nodes[node_index].m_parent_ofs;
            if (parent_ofs)
            {
                const auto &parent_tfm = m_nodetree.nodes[node_index - parent_ofs].global_tfm;
                node_tfm = parent_tfm * node_tfm;
            }
            m_nodetree.nodes[node_index].global_tfm = node_tfm;

            node_index++;
        }

        m_model_aabb.reset();
        for (int i = 0; i < m_bones.size(); i++)
        {
            const auto &node_tfm = m_nodetree.nodes[m_bones[i].node_index].global_tfm;
            const auto &boneIB_tfm = m_bones[i].inversebind_tfm;
            m4f M = node_tfm * boneIB_tfm;

            // Final bone matrices
            // m_bones[i].global_tfm = M;
            boneMatrices[i] = M;
            // AABB
            m_bone_aabbs_pose[i] = m_bone_aabbs_bind[i].post_transform(M.column(3).xyz(), M.get_3x3());
            m_model_aabb.grow(m_bone_aabbs_pose[i]);
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
                m4f M = m_nodetree.nodes[m_meshes[i].node_index].global_tfm; // * boneIB_tfm;
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i].post_transform(M.column(3).xyz(), M.get_3x3());
            }
            else
                m_mesh_aabbs_pose[i] = m_mesh_aabbs_bind[i];

            m_model_aabb.grow(m_mesh_aabbs_pose[i]);
        }

        // bone_transforms.resize(m_bones.size());
        // for (uint i = 0; i < m_bones.size(); i++)
        //     bone_transforms[i] = m_bones[i].global_tfm;
    }

    unsigned RenderableMesh::get_nbr_animations() const
    {
        return (unsigned)m_animations.size();
    }

    std::string RenderableMesh::get_animation_name(unsigned i) const
    {
        return (i < get_nbr_animations() ? m_animations[i].name : "");
    }

    RenderableMesh::~RenderableMesh()
    {
        for (auto &t : m_textures)
            t.free();
        // glDeleteTextures(1, &placeholder_texture);

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
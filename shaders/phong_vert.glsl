#version 410 core
const int MaxBones = 161; // 128;

layout (location = 0) in vec3 attr_Position;
layout (location = 1) in vec2 attr_Texcoord;
layout (location = 2) in vec3 attr_Normal;
layout (location = 3) in vec3 attr_Tangent;
layout (location = 4) in vec3 attr_Binormal;
layout (location = 5) in ivec4 BoneIDs;
layout (location = 6) in vec4 BoneWeights;

uniform mat4 ProjViewMatrix;
uniform mat4 WorldMatrix;
uniform mat4 BoneMatrices[MaxBones];
uniform int u_is_skinned;

out vec3 wpos;
out vec2 texcoord;
out vec3 normal;
out vec3 tangent;
out vec3 binormal;
out vec3 color;

void main()
{
   mat4 BoneMatrix = mat4(1.0);
   if (u_is_skinned > 0)
   {
       BoneMatrix *=    BoneMatrices[BoneIDs.x] * BoneWeights.x + 
                        BoneMatrices[BoneIDs.y] * BoneWeights.y + 
                        BoneMatrices[BoneIDs.z] * BoneWeights.z + 
                        BoneMatrices[BoneIDs.w] * BoneWeights.w;
       /* Fallback when bone weights are zero */
       if (BoneWeights.x+BoneWeights.y+BoneWeights.z+BoneWeights.w < 0.01)
       {
           BoneMatrix = BoneMatrices[0];
       }
   }

   wpos = (WorldMatrix * BoneMatrix * vec4(attr_Position, 1)).xyz;
   texcoord = attr_Texcoord;
   normal = normalize( (WorldMatrix * BoneMatrix * vec4(attr_Normal, 0)).xyz );
   tangent = normalize( (WorldMatrix * BoneMatrix * vec4(attr_Tangent, 0)).xyz );
   binormal = normalize( (WorldMatrix * BoneMatrix * vec4(attr_Binormal, 0)).xyz );

   gl_Position = ProjViewMatrix * WorldMatrix * BoneMatrix * vec4(attr_Position, 1);
}
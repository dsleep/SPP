#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_GOOGLE_include_directive: require

layout(row_major) uniform;
layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

struct VoxelPushInfo
{
	vec3 RayStart;
	vec3 RayDirection;
	ivec3 Extents;	
	int VoxelStartingDepth;	
};

layout(push_constant) uniform block
{
	VoxelPushInfo pcs;
};

const int MAX_VOXEL_LEVELS = 12;

layout(set = 1, binding = 1) readonly buffer _Voxels
{
	uint8_t voxels[];
} Voxels[MAX_VOXEL_LEVELS];

layout (set = 2, binding = 0, rgba8) uniform image2D DiffuseImage;
layout (set = 2, binding = 1, rgba8) uniform image2D SMREImage;
layout (set = 2, binding = 2, rgba8) uniform image2D NormalImage;
layout (set = 2, binding = 3, r32f) uniform image2D DepthImage;

layout (set = 2, binding = 4) uniform sampler2D samplerDepth;


struct RayInfo
{
	vec3 rayOrg;
	vec3 rayDir;
    vec3 rayDirInv;
    int curMisses;
};


bool rayTraversal(RayInfo& InRayInfo,
        uint8_t InCurrentLevel, 
        uint32_t InIterationsLeft)
{       
    vec3 VoxelSize(1 << InCurrentLevel, 1 << InCurrentLevel, 1 << InCurrentLevel);
    vec3 HalfVoxel = VoxelSize / 2.0f;

    // get in correct voxel spacing
    vec3 voxel = floor(InRayInfo.rayOrg / VoxelSize) * VoxelSize;
    vec3 step = VoxelSize * sign(InRayInfo.rayDir);
    vec3 tMax = (voxel - InRayInfo.rayOrg + HalfVoxel + step * 0.5f) * InRayInfo.rayDirInv;
    vec3 tDelta = VoxelSize * abs(InRayInfo.rayDirInv);

    vec3 dim = vec3(0, 0, 0);
    vec3 samplePos = voxel;

    while (true)
    {
        if (any(step(samplePos, vec3(0,0,0))) || any(step(_dimensions, samplePos)))
        {
            return false;
        }

        InRayInfo.totalTests++;
        InIterationsLeft--;

        if (InIterationsLeft == 0)
        {
            return false;
        }

        bool bRecalcAndTraverse = false;

        // we hit something?
        if (GetUnScaledAtLevel(samplePos.cast<int32_t>(), InCurrentLevel))
        {
            InRayInfo.curMisses = 0;

            if (InCurrentLevel == 0)
            {
                return true;
            }

            InCurrentLevel--;
            bRecalcAndTraverse = true;              
        }
        else
        {
            vec3 tMaxMins =  min(tMax.yzx, tMax.zxy);
            dim = step(tMax, tMaxMins);
            tMax += dim * tDelta;
            InRayInfo.lastStep = dim * step;
            samplePos += InRayInfo.lastStep;

            if (any(step(samplePos, vec3(0,0,0))) || any(step(_dimensions, samplePos)))
            {
                return false;
            }

            InRayInfo.curMisses++;
            InRayInfo.bHasStepped = true;

            if (InCurrentLevel < _levels.size() - 1 &&
                InRayInfo.curMisses > 2 && 
                GetUnScaledAtLevel(samplePos.cast<int32_t>(), InCurrentLevel+1) == 0)
            {
                bRecalcAndTraverse = true;
                InCurrentLevel++;
                //InRayInfo.bHasStepped = false;
            } 
        }
            
        if (bRecalcAndTraverse)
        {
            if (InRayInfo.bHasStepped)
            {
                // did it step already
                vec3 normal = -sign(InRayInfo.lastStep);

                vec3 VoxelCenter = samplePos + HalfVoxel;
                vec3 VoxelPlaneEdge = VoxelCenter + HalfVoxel * normal;

                float denom = normal.dot(InRayInfo.rayDir);
                if (denom == 0)
                    denom = 0.0000001f;

                Vector3 p0l0 = (VoxelPlaneEdge - InRayInfo.rayOrg);
                float t = p0l0.dot(normal) / denom;

                float epsilon = 0.001f;
                InRayInfo.rayOrg = InRayInfo.rayOrg + InRayInfo.rayDir * (t + epsilon);
            }

            return _rayTraversal(InRayInfo, InCurrentLevel, InIterationsLeft);
        }
    }

    return false;
}


bool CastRay(const Ray& InRay, VoxelHitInfo& oInfo)
{
    auto& rayDir = InRay.GetDirection();
    auto& rayOrg = InRay.GetOrigin();

    Vector3 rayOrgf = rayOrg.cast<float>();
    Vector3 vRayStart = (ToVector4(rayOrgf) * _worldToVoxels).head<3>();

    RayInfo info;

    info.rayOrg = vRayStart;
    info.rayDir = rayDir;
    info.rayDirInv = rayDir.cwiseInverse();

    if (!isfinite(info.rayDirInv[0]))
    {
        info.rayDirInv[0] = 100000.0f;
    }
    if (!isfinite(info.rayDirInv[1]))
    {
        info.rayDirInv[1] = 100000.0f;
    }
    if (!isfinite(info.rayDirInv[2]))
    {
        info.rayDirInv[2] = 100000.0f;
    }
        

    if (rayTraversal(info, _levels.size() - 1, 1024))
    {
        Vector3 normal = -hlslSign(info.lastStep);
        oInfo.normal = normal;    

        oInfo.totalChecks = info.totalTests;
        return true;
    }

    oInfo.totalChecks = info.totalTests;
    return false;
}

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
	uint di = gl_GlobalInvocationID.x;
		
	if(Voxels[0].voxels[di] != uint8_t(0))
	{
		uint test = 0;
		test++;
	}
}

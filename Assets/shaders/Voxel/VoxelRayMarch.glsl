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

layout(set = 1, binding = 1) readonly buffer _VoxelLevels
{
	uint8_t voxels[100];
    ivec3 localPageMask;
    ivec3 localPageVoxelDimensions;
    ivec3 localPageVoxelDimensionsP2;
    ivec3 localPageCounts;

   
} VoxelLevels[MAX_VOXEL_LEVELS];

layout(set = 1, binding = 2) readonly buffer _VoxelInfo
{
    int activeLevels;
    uint pageSize;
    ivec3 dimensions;
} VoxelInfo;

layout (set = 2, binding = 0, rgba8) uniform image2D DiffuseImage;
layout (set = 2, binding = 1, rgba8) uniform image2D SMREImage;
layout (set = 2, binding = 2, rgba8) uniform image2D NormalImage;
layout (set = 2, binding = 3, r32f) uniform image2D DepthImage;

layout (set = 2, binding = 4) uniform sampler2D samplerDepth;


struct RayInfo
{
	vec3 rayOrg;
	vec3 rayDir;
    vec3 rayDirSign;
    vec3 rayDirInvAbs;
    vec3 rayDirInv;
    int curMisses;
    
    vec3 lastStep;
    bool bHasStepped;
};

struct PageIdxAndMemOffset
{
    uint pageIDX;
    uint memoffset;
};

void GetOffsets(in ivec3 InPosition, in int InLevel, out PageIdxAndMemOffset oMem)
{           
    // find the local voxel
    ivec3 LocalVoxel = ( InPosition & VoxelLevels[InLevel].localPageMask);
    uint localVoxelIdx = (LocalVoxel.x +
        LocalVoxel.y * VoxelLevels[InLevel].localPageVoxelDimensions.x +
        LocalVoxel.z * VoxelLevels[InLevel].localPageVoxelDimensions.x * VoxelLevels[InLevel].localPageVoxelDimensions.y);

    // find which page we are on
    ivec3 PageCubePos = InPosition >> VoxelLevels[InLevel].localPageVoxelDimensionsP2;
    uint pageIdx = (PageCubePos.x +
        PageCubePos.y * VoxelLevels[InLevel].localPageCounts.x +
        PageCubePos.z * VoxelLevels[InLevel].localPageCounts.x * VoxelLevels[InLevel].localPageCounts.y);
                       
    uint memOffset = (pageIdx * VoxelInfo.pageSize) + localVoxelIdx; // always 1 * _dataTypeSize;

    oMem.pageIDX = pageIdx;
    oMem.memoffset = memOffset;
}


int GetUnScaledAtLevel(in ivec3 InPosition, int InLevel)
{
    //SE_ASSERT(InLevel < _levels.size());
    ivec3 levelPos = InPosition >> InLevel;
    PageIdxAndMemOffset pageAndMem;
    GetOffsets(InPosition, InLevel, pageAndMem);
    return VoxelLevels[InLevel].voxels[pageAndMem.memoffset];
}

bool rayTraversal(inout RayInfo InRayInfo,
        in int InCurrentLevel,
        in int InIterationsLeft)
{       
    vec3 VoxelSize = vec3(1 << InCurrentLevel, 1 << InCurrentLevel, 1 << InCurrentLevel);
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
        /* TODO
        if (any(step(samplePos, vec3(0,0,0))) || any(step(VoxelInfo.dimensions, samplePos)))
        {
            return false;
        } */

        InIterationsLeft--;

        if (InIterationsLeft == 0)
        {
            return false;
        }

        bool bRecalcAndTraverse = false;

        // we hit something?
        if (GetUnScaledAtLevel(ivec3(samplePos), InCurrentLevel) > 0)
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
            //TODO 
            //dim = step(tMax, tMaxMins);
            tMax += dim * tDelta;
            InRayInfo.lastStep = dim * step;
            samplePos += InRayInfo.lastStep;

            /* TODO
            if (any(step(samplePos, vec3(0,0,0))) || any(step(VoxelInfo.dimensions, samplePos)))
            {
                return false;
            } */

            InRayInfo.curMisses++;
            InRayInfo.bHasStepped = true;

            if (InCurrentLevel < VoxelInfo.activeLevels &&
                InRayInfo.curMisses > 2 && 
                GetUnScaledAtLevel( ivec3(samplePos), InCurrentLevel+1) == 0)
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

                float denom = dot(normal,InRayInfo.rayDir);
                if (denom == 0)
                    denom = 0.0000001f;

                vec3 p0l0 = (VoxelPlaneEdge - InRayInfo.rayOrg);
                float t = dot(p0l0, normal) / denom;

                float epsilon = 0.001f;
                InRayInfo.rayOrg = InRayInfo.rayOrg + InRayInfo.rayDir * (t + epsilon);
            }

            return rayTraversal(InRayInfo, InCurrentLevel, InIterationsLeft);
        }
    }

    return false;
}

/* TODO 
bool CastRay(in Ray InRay, out VoxelHitInfo oInfo)
{
    auto& rayDir = InRay.GetDirection();
    auto& rayOrg = InRay.GetOrigin();

    vec3 rayOrgf = rayOrg.cast<float>();
    vec3 vRayStart = (vec4(rayOrgf.xyz,1) * _worldToVoxels);

    RayInfo info;

    info.rayOrg = vRayStart;
    info.rayDir = rayDir;
    info.rayDirSign = hlslSign(info.rayDir);

    float epsilon = 0.001f;
    vec3 ZeroEpsilon = (info.rayDir == vec3(0, 0, 0)) * epsilon;

    info.rayDirInv = 1.0f / (rayDir + ZeroEpsilon);
    info.rayDirInvAbs = abs(info.rayDirInv);

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
*/

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
	uint di = gl_GlobalInvocationID.x;
		
	if(VoxelLevels[0].voxels[di] != uint8_t(0))
	{
		uint test = 0;
		test++;
	}
}

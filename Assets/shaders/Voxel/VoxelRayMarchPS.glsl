#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_GOOGLE_include_directive: require

//layout(row_major) uniform;
//layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"
// include paths based on root of shader directory
//#include "./Deferred/PBRCommon.glsl"

const int MAX_VOXEL_LEVELS = 15;

layout(set = 1, binding = 0) readonly uniform _VoxelInfo
{
	int activeLevels;
    uint pageSize;
    mat4 worldToVoxel;   
    mat4 voxelToWorld;   
    ivec3 dimensions;
} VoxelInfo;

layout(set = 1, binding = 1) readonly uniform _LevelInfo
{
    vec3 VoxelSize[MAX_VOXEL_LEVELS];
    vec3 HalfVoxel[MAX_VOXEL_LEVELS];
    ivec3 localPageMask[MAX_VOXEL_LEVELS];
    ivec3 localPageVoxelDimensions[MAX_VOXEL_LEVELS];
    ivec3 localPageVoxelDimensionsP2[MAX_VOXEL_LEVELS];
    ivec3 localPageCounts[MAX_VOXEL_LEVELS];
} LevelInfos;

layout(set = 1, binding = 2) readonly buffer _LevelVoxels
{
	uint8_t voxels[];
} LevelVoxels[MAX_VOXEL_LEVELS];

//INPUT FROM VS
layout (location = 0) in vec4 inPixelPosition;
layout (location = 1) in vec2 inUV;

//OUTPUT TO MRTs
layout (location = 0) out vec4 outDiffuse;
// specular, metallic, roughness, emissive
layout (location = 1) out vec4 outSMRE;
// 
layout (location = 2) out vec4 outNormal;

struct RayInfo
{
	vec3 rayOrg;
	vec3 rayDir;
    vec3 rayDirSign;
    vec3 rayDirInvAbs;
    vec3 rayDirInv;
    int curMisses;
    
    vec3 lastStep;
};

struct PageIdxAndMemOffset
{
    uint pageIDX;
    uint memoffset;
};

struct VoxelHitInfo
{
    vec3 location;
    vec3 normal;
    uint totalChecks;
};

struct Stepping
{
    vec3 step;
    vec3 tDelta;
};

Stepping stepInfos[MAX_VOXEL_LEVELS];

ivec3 ShiftDown(in ivec3 InValue, in ivec3 ShiftVec)
{
    return ivec3( InValue.x >> ShiftVec.x,
        InValue.y >> ShiftVec.y,
        InValue.z >> ShiftVec.z);
}
ivec3 ShiftDown(in ivec3 InValue, in uint ShiftValue)
{
    return ivec3( InValue.x >> ShiftValue,
        InValue.y >> ShiftValue,
        InValue.z >> ShiftValue);
}

void GetOffsets(in ivec3 InPosition, in uint InLevel, out PageIdxAndMemOffset oMem)
{           
    // find the local voxel
    ivec3 LocalVoxel = ivec3( InPosition.x & LevelInfos.localPageMask[InLevel].x,
        InPosition.y & LevelInfos.localPageMask[InLevel].y,
        InPosition.z & LevelInfos.localPageMask[InLevel].z);

    uint localVoxelIdx = (LocalVoxel.x +
        LocalVoxel.y * LevelInfos.localPageVoxelDimensions[InLevel].x +
        LocalVoxel.z * LevelInfos.localPageVoxelDimensions[InLevel].x * LevelInfos.localPageVoxelDimensions[InLevel].y);

    // find which page we are on
    ivec3 PageCubePos = ShiftDown(InPosition, LevelInfos.localPageVoxelDimensionsP2[InLevel]);
    uint pageIdx = (PageCubePos.x +
        PageCubePos.y * LevelInfos.localPageCounts[InLevel].x +
        PageCubePos.z * LevelInfos.localPageCounts[InLevel].x * LevelInfos.localPageCounts[InLevel].y);
                       
    uint memOffset = (pageIdx * VoxelInfo.pageSize) + localVoxelIdx; // always 1 * _dataTypeSize;

    oMem.pageIDX = pageIdx;
    oMem.memoffset = memOffset;
}

int GetUnScaledAtLevel(in ivec3 InPosition, uint InLevel)
{
    ivec3 levelPos = ShiftDown(InPosition, InLevel);
    PageIdxAndMemOffset pageAndMem;
    GetOffsets(levelPos, InLevel, pageAndMem);
    return LevelVoxels[InLevel].voxels[pageAndMem.memoffset];
}

bool ValidSample(in vec3 InPos) 
{
    if (all(greaterThanEqual(InPos, vec3(0.0f) )) &&
        all(lessThan(InPos, VoxelInfo.dimensions)) )
    {
        return true;
    }

    return false;
}

bool CastRay(in vec3 rayOrg, in vec3 rayDir, out VoxelHitInfo oInfo)
{
    RayInfo rayInfo;

    rayInfo.rayOrg = rayOrg;
    rayInfo.rayDir = rayDir;
    rayInfo.rayDirSign = sign(rayInfo.rayDir);

    float epsilon = 0.001f;
    vec3 ZeroEpsilon = vec3(equal(rayInfo.rayDir, vec3(0, 0, 0))) * epsilon;

    rayInfo.rayDirInv = 1.0f / (rayDir + ZeroEpsilon);
    rayInfo.rayDirInvAbs = abs(rayInfo.rayDirInv);
    
    uint CurrentLevel = VoxelInfo.activeLevels - 1;
    uint LastLevel = 0;

    for (int Iter = 0; Iter < MAX_VOXEL_LEVELS; Iter++)
    {
        stepInfos[Iter].step = LevelInfos.VoxelSize[Iter] * rayInfo.rayDirSign;
        stepInfos[Iter].tDelta = LevelInfos.VoxelSize[Iter] * rayInfo.rayDirInvAbs;
    }

	// get in correct voxel spacing
	vec3 voxel = floor(rayInfo.rayOrg / LevelInfos.VoxelSize[CurrentLevel]) * LevelInfos.VoxelSize[CurrentLevel];
	vec3 tMax = (voxel - rayInfo.rayOrg + LevelInfos.HalfVoxel[CurrentLevel] + stepInfos[CurrentLevel].step * vec3(0.5f, 0.5f, 0.5f)) * (rayInfo.rayDirInv);

	vec3 dim = vec3(0, 0, 0);
	vec3 samplePos = voxel;

    oInfo.totalChecks = 0;
        
    vec3 samplePosWorld = rayInfo.rayOrg;
    float rayTime = 0.0f;
    
    rayInfo.lastStep = rayInfo.rayDir;

    for (int Iter = 0; Iter < 128; Iter++)
    {
        LastLevel = CurrentLevel;

        if (!ValidSample(samplePos))
        {
            return false;
        }

        oInfo.totalChecks++;

        bool bDidStep = false;
        bool bLevelChangeRecalc = false;

        // we hit something?
        if (GetUnScaledAtLevel(ivec3(samplePos), CurrentLevel) != 0)
        {
            rayInfo.curMisses = 0;

            if (CurrentLevel == 0)
            {
                vec3 normal = -sign(rayInfo.lastStep);
                oInfo.normal = normal;

                vec3 VoxelCenter = samplePos + LevelInfos.HalfVoxel[CurrentLevel];
                vec3 VoxelPlaneEdge = VoxelCenter + LevelInfos.HalfVoxel[CurrentLevel] * normal;
                float denom = dot(normal, rayInfo.rayDir);
                if (denom == 0)
                    denom = 0.0000001f;
                vec3 p0l0 = (VoxelPlaneEdge - rayInfo.rayOrg);
                float t = dot(p0l0, normal) / denom;
                oInfo.location = rayInfo.rayOrg + rayInfo.rayDir * t;

                return true;
            }

            CurrentLevel--;
            bLevelChangeRecalc = true;
        }
        else
        {
            bDidStep = true;

            vec3 tMaxMins = min(tMax.yzx, tMax.zxy);
            dim = step(tMax, tMaxMins);
            tMax += dim * stepInfos[CurrentLevel].tDelta;
            rayInfo.lastStep = dim * stepInfos[CurrentLevel].step;
            samplePos += rayInfo.lastStep;

            if (!ValidSample(samplePos))
            {
                return false;
            }

            rayInfo.curMisses++;

            if (CurrentLevel < VoxelInfo.activeLevels - 1 &&
                rayInfo.curMisses > 2 &&
                GetUnScaledAtLevel(ivec3(samplePos), CurrentLevel + 1) == 0)
            {
                bLevelChangeRecalc = true;
                CurrentLevel++;
            }
        }

        if (bDidStep)
        {
            // did it step already
            vec3 normal = -sign(rayInfo.lastStep);

            vec3 VoxelCenter = samplePos + LevelInfos.HalfVoxel[LastLevel];
            vec3 VoxelPlaneEdge = VoxelCenter + LevelInfos.HalfVoxel[LastLevel] * normal;

            float denom = dot(normal, rayInfo.rayDir);
            if (denom == 0)
                denom = 0.0000001f;

            vec3 p0l0 = (VoxelPlaneEdge - rayInfo.rayOrg);
            float t = dot(p0l0, normal) / denom;

            if (t > rayTime)
            {
                const float epsilon = 0.001f;
                rayTime = t + epsilon;
                samplePosWorld = rayInfo.rayOrg + rayInfo.rayDir * rayTime;
            }
        }

        if (bLevelChangeRecalc)
        {
            voxel = floor(samplePosWorld / LevelInfos.VoxelSize[CurrentLevel]) * LevelInfos.VoxelSize[CurrentLevel];
	        tMax = (voxel - samplePosWorld + LevelInfos.HalfVoxel[CurrentLevel] + stepInfos[CurrentLevel].step * vec3(0.5f, 0.5f, 0.5f)) * (rayInfo.rayDirInv);

	        dim = vec3(0, 0, 0);
            samplePos = voxel;
        }
    }

    return false;
}

void main()
{
	vec4 cameraRay = Multiply(vec4(inPixelPosition.xy, 1, 1.0), ViewConstants.InvViewProjectionMatrix);		
    
    cameraRay /= cameraRay.w;

    vec4 cameraInWorld = vec4(cameraRay.xyz + vec3(ViewConstants.ViewPosition.xyz), 1);
    vec3 cameraInVoxel = Multiply( cameraInWorld, VoxelInfo.worldToVoxel ).xyz;

    outDiffuse = vec4( 0,0,0, 1 );
	outSMRE = vec4( 0.5f, 0, 1.0f, 0 );
	outNormal = vec4( 0,0,0, 0 );

    VoxelHitInfo info;
    if(CastRay(cameraInVoxel, normalize(cameraRay.xyz), info))
    {
        outDiffuse.xyz = vec3(0,0.5f,0);
        outNormal.xyz = info.normal.xyz;

        vec3 worldPosition = Multiply( vec4( info.location, 1), VoxelInfo.voxelToWorld ).xyz - vec3(ViewConstants.ViewPosition);
	    vec4 viewLocation = Multiply( vec4(worldPosition,1), ViewConstants.ViewProjectionMatrix );
        gl_FragDepth = viewLocation.z / viewLocation.w;
    }
    else
    {
        discard;
    }
}

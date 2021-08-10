//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#define THREADS_PER_WAVE 32
#define AS_GROUP_SIZE THREADS_PER_WAVE

#define MAX(x, y) (x > y ? x : y)
#define ROUNDUP(x, y) ((x + y - 1) & ~(y - 1))

#define MAX_VERTS 64
#define MAX_PRIMS 126
#define MAX_LOD_LEVELS 8

#define THREADS_PER_WAVE 32 // Assumes availability of wave size of 32 threads

// Pre-defined threadgroup sizes for AS & MS stages
#define AS_GROUP_SIZE THREADS_PER_WAVE
#define MS_GROUP_SIZE ROUNDUP(MAX(MAX_VERTS, MAX_PRIMS), THREADS_PER_WAVE)

struct MeshInfo
{
    uint IndexBytes;
    uint MeshletCount;
};

struct DrawParams
{
    uint MeshIdx;    
    uint IndexBytes;
    uint MeshletStartIdx;
    uint MeshletCount;
    uint MeshletGroup;
};

struct MeshInstances
{
    float4x4    LocalToWorldScaleRotation;
    double3     Translation;
    float4      BoundingSphere;
    uint        InstanceOffset;
};

struct MeshVertex
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texcoord  : TEXCOORD;
};


struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    uint2  MeshletInfo  : COLOR0;
};

struct Meshlet
{
    uint VertCount;
    uint VertOffset;
    uint PrimCount;
    uint PrimOffset;
};

struct Payload
{
    uint MeshletIndices[AS_GROUP_SIZE];
};


ConstantBuffer<DrawParams>          DrawParams                : register(b3);

//meshes
StructuredBuffer<MeshInfo>          Meshes                    : register(t0, space0);
//
StructuredBuffer<MeshVertex>        Vertices[]                : register(t0, space1);
StructuredBuffer<Meshlet>           Meshlets[]                : register(t0, space2);
ByteAddressBuffer                   UniqueVertexIndices[]     : register(t0, space3);
StructuredBuffer<uint>              PrimitiveIndices[]        : register(t0, space4);


// Computes visiblity of an instance
// Performs a simple world-space bounding sphere vs. frustum plane check.
//bool IsVisible(float4 boundingSphere)
//{
//    float4 center = float4(boundingSphere.xyz, 1.0);
//    float radius = boundingSphere.w;
//
//    for (int i = 0; i < 6; ++i)
//    {
//        if (dot(center, Constants.Planes[i]) < -radius)
//        {
//            return false;
//        }
//    }
//
//    return true;
//}

// 
//StructuredBuffer<CullData>  MeshletCullData      : register(t4);

// Computes the LOD for a given instance.
// Calculates the spread of the instance's world-space bounding sphere in screen space.
//uint ComputeLOD(float4 boundingSphere)
//{
//    float3 v = boundingSphere.xyz - Constants.ViewPosition;
//    float r = boundingSphere.w;
//
//    // Sphere radius in screen space
//    float size = Constants.RecipTanHalfFovy * r / sqrt(dot(v, v) - r * r);
//    size = min(size, 1.0);
//
//    return (1.0 - size) * (Constants.LODCount - 1);
//}
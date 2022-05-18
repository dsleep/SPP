// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SDFCommonShapes.hlsl"
#include "SDFOperators.hlsl"

struct DrawParams
{
	float3  ShapeColor;
    uint    ShapeCount;
};

struct SDFShape
{
	float3  translation; 
	float3  eulerRotation;  
    
    
    float4  shapeBlendAndScale;
    float4  params;
	
	uint    shapeType;
    uint    shapeOp;
};

[[vk::binding(2, 0)]]
ConstantBuffer<DrawParams>          DrawParams                : register(b3);

[[vk::binding(3, 0)]]
StructuredBuffer<SDFShape>          Shapes                    : register(t0, space0);

// slight expansion to a buffer of these shapes... very WIP
float map( in float3 pos )
{
    float d = 1e10;

    float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, -DrawConstants.Translation, double3(0,0,0));
    float3 samplePos = mul(float4(pos, 1.0), LocalToWorldTranslated).xyz - float3(Shapes[0].translation);

    //float3 samplePos = pos - (float3(DrawConstants.Translation) + float3(Shapes[0].translation));

    if (Shapes[0].shapeType == 1)
    {
        d = sdSphere(samplePos, Shapes[0].params.x);
    }
    else if (Shapes[0].shapeType == 2)
    {
        d = sdBox(samplePos, Shapes[0].params.xyz);
    }
        
    for (uint i = 1; i < DrawParams.ShapeCount; ++i)
    {
        float cD = 0;

        float3 samplePos = mul(float4(pos, 1.0), LocalToWorldTranslated).xyz - float3(Shapes[i].translation);
        if (Shapes[i].shapeType == 1)
        {
            cD = sdSphere(samplePos, Shapes[i].params.x);
        } 
        else if (Shapes[i].shapeType == 2)
        {
            cD = sdBox(samplePos, Shapes[i].params.xyz);
        }
        
        if (Shapes[i].shapeOp == 0)
        {
            d = opSmoothUnion(d, cD, Shapes[i].shapeBlendAndScale.x);
        }
        else if (Shapes[i].shapeOp == 1)
        {
            d = opSmoothSubtraction(cD, d, Shapes[i].shapeBlendAndScale.x);
        }
        else if (Shapes[i].shapeOp == 2)
        {
            d = opSmoothIntersection(cD, d, Shapes[i].shapeBlendAndScale.x);
        }
    }
	
	return d;
}

// Raymarch along given ray
// ro: ray origin
// rd: ray direction
float raymarch(float3 ro, float3 rd) 
{
    const int maxstep = 64;
    float t = 0; // current distance traveled along ray
    for (int i = 0; i < maxstep; ++i) 
    {
        float3 p = ro + rd * t; // World space position of sample
        float d = map(p);       // Sample of distance field (see map())

        // If the sample <= 0, we have hit something (see map()).
        if (d < 0.001)
        {
            break;
        }

        // If the sample > 0, we haven't hit anything yet so we should march forward
        // We step forward by distance d, because d is the minimum distance possible to intersect
        // an object (see map()).
        t += d;
    }

    return t;
}

float3 calcNormal(float3 pos)
{
    const float ep = 0.001;
    float2 e = float2(1.0, -1.0) * 0.5773;
    return normalize(e.xyy * map(pos + e.xyy * ep) +
        e.yyx * map(pos + e.yyx * ep) +
        e.yxy * map(pos + e.yxy * ep) +
        e.xxx * map(pos + e.xxx * ep));
}

float4 renderSDF( float3 ro, float3 rd ) 
{ 
	float hitDistance = raymarch(ro, rd);

    if (hitDistance < 10000)
    {
        float3 pos = ro + hitDistance * rd;
        float3 outColor = DrawParams.ShapeColor;

        float3 objNormal = calcNormal(pos);
        float3 lig = normalize(float3(1.0, 0.8, -0.2));
        float dif = clamp(dot(objNormal, lig), 0.0, 1.0);
        float amb = 0.5;// +0.5 * objNormal.y;w

        return float4( float3(0.25, 0.25, 0.25) * amb + outColor * dif, hitDistance);
    }
	else
	{
        //no convergence
        clip(-1);
	}

	return float4(rd * 0.5 + 0.5,hitDistance);
}



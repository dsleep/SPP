// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

//Math Operations based on Inigo Quilez work
//https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

float sdSphere(float3 p, float s)
{
    return length(p) - s;
}

float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// can use includes in here as well
#include "SDFOperators.hlsl"

//Scene Call, returns distance to point
float sceneSDF( in float3 TestPos )
{
    float3 SpherePos = float3(0, 10, 0);
	float3 BoxPos = float3(0, 0, 0);
	
    float sphereD = sdSphere(TestPos - SpherePos, 15);
	float boxD = sdBox(TestPos - BoxPos, float3(40,5,40));
	
	return opSmoothUnion(sphereD,boxD, 10);
}

// Raymarch along given ray
// ro: ray origin
// rd: ray direction
float raymarchScene(float3 ro, float3 rd)
{
    const int maxstep = 64;
    float t = 0; // current distance traveled along ray
    for (int i = 0; i < maxstep; ++i)
    {
        float3 p = ro + rd * t; // World space position of sample
        float d = sceneSDF(p);  // Sample of distance field (see sceneSDF())

        // If the sample <= 0, we have hit something (see sceneSDF()).
        if (d < 0.001)
        {
            break;
        }

        // If the sample > 0, we haven't hit anything yet so we should march forward
        // We step forward by distance d, because d is the minimum distance possible to intersect
        // an object (see sceneSDF()).
        t += d;
    }

    return t;
}

// using a small epsilon estimate depth change and derive a normal
float3 calcNormal(float3 pos)
{
    const float ep = 0.001;
    float2 e = float2(1.0, -1.0) * 0.5773;
    return normalize(e.xyy * sceneSDF(pos + e.xyy * ep) +
        e.yyx * sceneSDF(pos + e.yyx * ep) +
        e.yxy * sceneSDF(pos + e.yxy * ep) +
        e.xxx * sceneSDF(pos + e.xxx * ep));
}

// called from a base full screen ray calculating pixel shader 
// return a color with Alpha being hitdistance
float4 renderSDF( float3 ro, float3 rd ) 
{ 
	float hitDistance = raymarchScene(ro, rd);

    if (hitDistance < 10000)
    {
        float3 pos = ro + hitDistance * rd;
        float3 diffuseColor = float3(0.25, 0.25, 0.8);

        float3 objNormal = calcNormal(pos);
		//somewhat random light vector
        float3 lig = normalize(float3(1.0, 0.8, -0.2));
        float dif = clamp(dot(objNormal, lig), 0.0, 1.0);
        float3 amb = float3(0.25, 0.25, 0.25);

        return float4( diffuseColor * dif + amb, hitDistance);
    }
	else
	{
        //no convergence
        clip(-1);
	}

	// should never be hit, avoid any compiler warnings
	return float4( float3(0,0,0), hitDistance );
}
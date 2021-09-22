// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SDFCommonShapes.hlsl"
#include "SDFOperators.hlsl"

// return distance to closest surface
float sceneSDF( in float3 TestPos )
{
    float3 SpherePos = float3(0, 0, 0);
    float d = sdSphere(TestPos, 25);
	return d;
}

#include "SDFHelpers.hlsl"

// returns color and hit distance to write a depth value in A
float4 renderSDF( float3 ro, float3 rd ) 
{ 
	float hitDistance = raymarchScene(ro, rd);

    if (hitDistance < 10000)
    {
        float3 pos = ro + hitDistance * rd;
        float3 diffuseColor = float3(0.25, 0.25, 0.8);

        float3 objNormal = calcNormal(pos);
        float3 lig = normalize(float3(1.0, 0.8, -0.2));
        float dif = clamp(dot(objNormal, lig), 0.0, 1.0);
        float3 amb = float3(0.25, 0.25, 0.25);

        return float4( outColor * dif + amb, hitDistance);
    }
	else
	{
        //no convergence
        clip(-1);
	}

	return float4( float3(0,0,0), hitDistance );
}



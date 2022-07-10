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

void SwapFloat(inout float InA, inout float InB)
{
	float tempA = InA;
	InA = InB;
	InB = tempA;
}

bool solveQuadratic(in float a, in float b, in float c, out float x0, out float x1) 
{ 
    float discr = b * b - 4 * a * c; 
    if (discr < 0) return false; 
    else if (discr == 0) x0 = x1 = - 0.5 * b / a; 
    else 
	{ 
        float q = (b > 0) ? 
            -0.5 * (b + sqrt(discr)) : 
            -0.5 * (b - sqrt(discr)); 
        x0 = q / a; 
        x1 = c / q; 
    } 
	
    if (x0 > x1) 
	{
		SwapFloat(x0, x1); 
	}
 
    return true; 
} 

bool intersect_ray_sphere(float3 rayOrigin, float3 rayDirection, float3 sphereCenter, float sphereRadius, out float t0, out float t1)
{         
	float radius2 = sphereRadius * sphereRadius;
		
#if 1 
    // geometric solution
	float3 L = rayOrigin - sphereCenter; 
    float tca = dot(L,rayDirection); 
	// if (tca < 0) return false;
	float d2 = dot(L,L) - tca * tca;		
	if (d2 > radius2) return false; 
	float thc = sqrt(radius2 - d2); 
	t0 = tca - thc; 
	t1 = tca + thc; 
	if (t0 > t1) SwapFloat(t0, t1); 
#else 
	// analytic solution
	float3 L = rayOrigin - sphereCenter; 
	float a = dot(rayDirection,rayDirection); 
	float b = 2.0f * dot(rayDirection, L); 
	float c = dot(L,L) - radius2; 
	if (!solveQuadratic(a, b, c, t0, t1)) return false; 
#endif 
 
	if (t0 < 0) 
	{ 
		t0 = t1;  //if t0 is negative, let's use t1 instead 
		if (t0 < 0) return false;  //both t0 and t1 are negative 
	} 

	return true; 
} 

/*
bool hit_sphere(const vec3& center, float radius, const ray& r){
    vec3 oc = r.origin() - center;
    float a = dot(r.direction(), r.direction());
    float b = 2.0 * dot(oc, r.direction());
    float c = dot(oc,oc) - radius*radius;
    float discriminant = b*b - 4*a*c;
    return (discriminant>0);
}
*/

// slight expansion to a buffer of these shapes... very WIP
float processShapes( in float3 pos )
{
    float d = 1e10;

    float4x4 LocalToWorldTranslated = GetWorldToLocalViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
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
        float cD = 1e10;

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
			d = opUnion(d, cD);
            //d = opSmoothUnion(d, cD, Shapes[i].shapeBlendAndScale.x);
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
    const int maxstep = 32;
    float t = 0; // current distance traveled along ray
    for (int i = 0; i < maxstep; ++i) 
    {
        float3 p = ro + rd * t; // World space position of sample
        float d = processShapes(p);       // Sample of distance field (see processShapes())

        // If the sample <= 0, we have hit something (see processShapes()).
        if (d < 0.001)
        {
            break;
        }

        // If the sample > 0, we haven't hit anything yet so we should march forward
        // We step forward by distance d, because d is the minimum distance possible to intersect
        // an object (see processShapes()).
        t += d;
    }

    return t;
}

float3 calcNormal(float3 pos)
{
    const float ep = 0.001;
    float2 e = float2(1.0, -1.0) * 0.5773;
    return normalize(e.xyy * processShapes(pos + e.xyy * ep) +
        e.yyx * processShapes(pos + e.yyx * ep) +
        e.yxy * processShapes(pos + e.yxy * ep) +
        e.xxx * processShapes(pos + e.xxx * ep));
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
        //clip(-1);
		hitDistance = -1000;
	}

	return float4(rd * 0.5 + 0.5,hitDistance);
}



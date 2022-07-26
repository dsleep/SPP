// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SDFCommonShapes.hlsl"
#include "SDFOperators.hlsl"

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define DBL_MAX 1.7976931348623158e+308
#define DBL_MIN 2.2250738585072014e-308

struct DrawParams
{
	float3  ShapeColor;
    uint    ShapeCount;
};

struct SDFShape
{
    float4x4 invTransform;

    float4 shapeParams;	
	float3 shapeColor;
	
	uint shapeType;
    uint shapeOp;
};

[[vk::binding(2, 0)]]
ConstantBuffer<DrawParams>          DrawParams                : register(b3);

[[vk::binding(3, 0)]]
StructuredBuffer<SDFShape>          Shapes                    : register(t0, space0);

#define ALLOW_SMOOTHING 0

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

#define MAX_SHAPES 30

struct ShapeProcessed
{
	float4x4 ViewToShapeMatrix;
	float rayScalar;
    bool IsHit;
    float T0;
    float T1;
};

static ShapeProcessed shapeProcessedData[MAX_SHAPES];

uint shapeCullAndProcess(in float3 ro, in float3 rd, out float T0, out float T1)
{
    T0 = 10000;
	T1 = -10000;
	
	uint ShapeHitCount = 0;
	
    for (uint ShapeIdx = 0; ShapeIdx < DrawParams.ShapeCount && ShapeIdx < 32; ++ShapeIdx)
    {
		shapeProcessedData[ShapeIdx].ViewToShapeMatrix = mul( GetTranslationMatrix( float3(ViewConstants.ViewPosition - DrawConstants.Translation) ), Shapes[ShapeIdx].invTransform );
	
		float3 rayStart = mul(float4(ro, 1.0), shapeProcessedData[ShapeIdx].ViewToShapeMatrix).xyz;
		float3 rayUnitEnd = mul(float4(ro + rd, 1.0), shapeProcessedData[ShapeIdx].ViewToShapeMatrix).xyz;
		shapeProcessedData[ShapeIdx].rayScalar = 1.0f / length(rayStart - rayUnitEnd);
		float3 rayDirection = normalize(rayStart - rayUnitEnd);
				
        shapeProcessedData[ShapeIdx].IsHit = intersect_ray_sphere(rayStart, rayDirection, float3(0,0,0), 1.05f, shapeProcessedData[ShapeIdx].T0, shapeProcessedData[ShapeIdx].T1);
        if (shapeProcessedData[ShapeIdx].IsHit)
        {
            ShapeHitCount++;
			
			T0 = min( shapeProcessedData[ShapeIdx].T0 * shapeProcessedData[ShapeIdx].rayScalar, T0 );
			T0 = min( shapeProcessedData[ShapeIdx].T1 * shapeProcessedData[ShapeIdx].rayScalar, T0 );
			
			T1 = max( shapeProcessedData[ShapeIdx].T0 * shapeProcessedData[ShapeIdx].rayScalar, T1 );
			T1 = max( shapeProcessedData[ShapeIdx].T1 * shapeProcessedData[ShapeIdx].rayScalar, T1 );
        }
    }
	
	
    return ShapeHitCount;
}



// slight expansion to a buffer of these shapes... very WIP
float processShapes( in float3 pos, out float3 hitColor )
{
    float d = 1e10;
            
    for (uint i = 0; i < DrawParams.ShapeCount && i < 32; ++i)
    {
		if (shapeProcessedData[i].IsHit == false)
		{
			continue;
		}
	 
        float cD = 1e10;

        float3 samplePos = mul(float4(pos, 1.0), shapeProcessedData[i].ViewToShapeMatrix).xyz;
        if (Shapes[i].shapeType == 1)
        {
            cD = sdSphere(samplePos, 1);
        } 
        else if (Shapes[i].shapeType == 2)
        {
            cD = sdBox(samplePos, float3(1,1,1) );
        }
		else if (Shapes[i].shapeType == 3)
        {
            cD = sdCappedCylinder(samplePos, 1, 1 );
        }
		
		cD *= shapeProcessedData[i].rayScalar;
		
		float lastD = d;
		
        if (Shapes[i].shapeOp == 0)
        {
#if ALLOW_SMOOTHING
			d = opSmoothUnion(d, cD, Shapes[i].shapeParams.x);
#else
            d = opUnion(d, cD);
#endif
        }
        else if (Shapes[i].shapeOp == 1)
        {
#if ALLOW_SMOOTHING
			d = opSmoothSubtraction(cD, d, Shapes[i].shapeParams.x);
#else
            d = opSubtraction(cD, d);
#endif            
        }
        else if (Shapes[i].shapeOp == 2)
        {	
#if ALLOW_SMOOTHING
			d = opSmoothIntersection(cD, d, Shapes[i].shapeParams.x);
#else
            d = opIntersection(cD, d);
#endif 
        }
		
		if(lastD != d)
		{		
			hitColor = Shapes[i].shapeColor;
		}
    }
	
	return d;
}

// Raymarch along given ray
// ro: ray origin
// rd: ray direction
float raymarch(float3 ro, float3 rd, float T0, float T1, out float3 hitColor) 
{
    const int maxstep = 32;
    float t = T0; // current distance traveled along ray
    for (int i = 0; i < maxstep; ++i) 
    {
        float3 p = ro + rd * t; // World space position of sample
        float d = processShapes(p, hitColor);       // Sample of distance field (see processShapes())

        // If the sample <= 0, we have hit something (see processShapes()).
        if (d < 0.001)
        {
            break;
        }

        // If the sample > 0, we haven't hit anything yet so we should march forward
        // We step forward by distance d, because d is the minimum distance possible to intersect
        // an object (see processShapes()).
        t += d;
		
		if(t > T1)
		{
			return 100000;
		}
    }

    return t;
}

float3 calcNormal(float3 pos)
{
    const float ep = 0.001;
    float2 e = float2(1.0, -1.0) * 0.5773;
	
	float3 dummyColor;
    return normalize(e.xyy * processShapes(pos + e.xyy * ep, dummyColor) +
        e.yyx * processShapes(pos + e.yyx * ep, dummyColor) +
        e.yxy * processShapes(pos + e.yxy * ep, dummyColor) +
        e.xxx * processShapes(pos + e.xxx * ep, dummyColor));
}

float4 renderSDF( float3 ro, float3 rd ) 
{ 
	float hitDistance = -1000;
	float T0, T1;
    uint hitCount = shapeCullAndProcess(ro, rd, T0, T1);

	if( hitCount > 0 )
	{	 
		float3 hitColor;
		float hitDistance = raymarch(ro, rd, T0, T1, hitColor);

		if (hitDistance < 10000)
		{
			float3 pos = ro + hitDistance * rd;

			float3 objNormal = calcNormal(pos);
			float3 lig = normalize(float3(1.0, 0.8, -0.2));
			float dif = clamp(dot(objNormal, lig), 0.3, 1.0);

			return float4( hitColor * dif, hitDistance);
		}
		else
		{
			//no convergence
			//clip(-1);
			hitDistance = -1000;
		}
	}

	return float4(rd * 0.5 + 0.5, hitDistance);
}



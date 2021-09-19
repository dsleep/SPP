
struct DrawParams
{
    uint ShapeCount;
};

struct SDFShape
{
    uint    shapeType;
    float3  translation;    
    
    uint    shapeOp;
    float3  eulerRotation;    
    
    float4  shapeBlendAndScale;
    float4  params;
};

ConstantBuffer<DrawParams>          DrawParams                : register(b3);
StructuredBuffer<SDFShape>          Shapes                    : register(t0, space0);

float dot2(in float2 v) { return dot(v, v); }
float dot2(in float3 v) { return dot(v, v); }
float ndot(in float2 a, in float2 b) { return a.x * b.x - a.y * b.y; }

float sdSphere(float3 p, float s)
{
    return length(p) - s;
}

float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdPyramid(float3 p, float h)
{
    float m2 = h * h + 0.25;

    p.xz = abs(p.xz);
    p.xz = (p.z > p.x) ? p.zx : p.xz;
    p.xz -= 0.5;

    float3 q = float3(p.z, h * p.y - 0.5 * p.x, h * p.x + 0.5 * p.y);

    float s = max(-q.x, 0.0);
    float t = clamp((q.y - 0.5 * p.z) / (m2 + 0.25), 0.0, 1.0);

    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5 * t) * (q.x + 0.5 * t) + (q.y - m2 * t) * (q.y - m2 * t);

    float d2 = min(q.y, -q.x * m2 - q.y * 0.5) > 0.0 ? 0.0 : min(a, b);

    return sqrt((d2 + q.z * q.z) / m2) * sign(max(q.z, -p.y));
}

float sdCappedCylinder(float3 p, float h, float r)
{
    float2 d = abs(float2(length(p.xz), p.y)) - float2(h, r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdVerticalCapsule(float3 p, float h, float r)
{
    p.y -= clamp(p.y, 0.0, h);
    return length(p) - r;
}

//NOT EXACT
float sdEllipsoid(float3 p, float3 r)
{
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}


//OPERATIONS

float opUnion(float d1, float d2) { return min(d1, d2); }

float opSubtraction(float d1, float d2) { return max(-d1, d2); }

float opIntersection(float d1, float d2) { return max(d1, d2); }


float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtraction(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return lerp(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersection(float d1, float d2, float k) 
{
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) + k * h * (1.0 - h);
}

float map( in float3 pos )
{
    float d = 1e10;
        
    for (uint i = 0; i < DrawParams.ShapeCount; ++i)
    {
        if (Shapes[i].shapeType == 1)
        {
            //d = opUnion(d, sdSphere(pos - float3(0, 0, 200), 50));
            if (i == 0)
            {
                d = opUnion(d, sdSphere(pos - (float3(DrawConstants.Translation) + float3(Shapes[i].translation)), Shapes[i].params.x));
            }
            else
            {
                d = opSmoothUnion(d, sdSphere(pos - (float3(DrawConstants.Translation) + float3(Shapes[i].translation)), Shapes[i].params.x), 10);
            }
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

/*
[noinline]
float3 calcNormal( float3 pos ) // for function f(p)
{
    const float h = 0.0001;      // replace by an appropriate value
    float3 n = float3(0,0,0);
    for( int i=0; i<4; i++ )
    {
        float3 e = 0.5773*(2.0*float3((((i+3)>>1)&1),((i>>1)&1),(i&1))-1.0);
        n += e*map(pos+e*h).x;
    }
    return normalize(n);
}
*/

float4 renderSDF( float3 ro, float3 rd ) 
{ 
	float hitDistance = raymarch(ro, rd);

    if (hitDistance < 1000)
    {
        float3 pos = ro + hitDistance * rd;
        float3 outColor = float3(1.00, 0.9, 0.80);

        float3 objNormal = calcNormal(pos);
        float3  lig = normalize(float3(1.0, 0.8, -0.2));
        float dif = clamp(dot(objNormal, lig), 0.0, 1.0);
        float amb = 0.5;// +0.5 * objNormal.y;w

        return float4( float3(0.25, 0.25, 0.25) * amb +  
            outColor * dif, hitDistance);
    }
	else
	{
        clip(-1);
	}

	return float4(rd * 0.5 + 0.5,hitDistance);
}




/*
//TRANSFORM
vec3 opTx(in vec3 p, in transform t, in sdf3d primitive)
{
    return primitive(invert(t) * p);
}

//SCALE

float opScale(in vec3 p, in float s, in sdf3d primitive)
{
    return primitive(p / s) * s;
}


//SYMMETRY
float opSymX(in vec3 p, in sdf3d primitive)
{
    p.x = abs(p.x);
    return primitive(p);
}

float opSymXZ(in vec3 p, in sdf3d primitive)
{
    p.xz = abs(p.xz);
    return primitive(p);
}


//REPETITION
float opRep(in vec3 p, in vec3 c, in sdf3d primitive)
{
    vec3 q = mod(p + 0.5 * c, c) - 0.5 * c;
    return primitive(q);
}

//FINITE REPETITION
vec3 opRepLim(in vec3 p, in float c, in vec3 l, in sdf3d primitive)
{
    vec3 q = p - c * clamp(round(p / c), -l, l);
    return primitive(q);
}
*/

//https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
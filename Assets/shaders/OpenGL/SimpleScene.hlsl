// The MIT License
// Copyright © 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// A list of useful distance function to simple primitives. All
// these functions (except for ellipsoid) return an exact
// euclidean distance, meaning they produce a better SDF than
// what you'd get if you were constructing them from boolean
// operations.

// List of other 3D SDFs: https://www.shadertoy.com/playlist/43cXRl
//
// and http://iquilezles.org/www/articles/distfunctions/distfunctions.htm

#version 130

#extension GL_EXT_gpu_shader4 : enable

uniform int iFrame;
uniform float iTime;
uniform vec2 iResolution;
uniform vec2 iMouse;

smooth in vec2 textureCoord;
out vec4 fragColor;

const int MAX_MARCHING_STEPS = 255;
const float MIN_DIST = 0.0;
const float MAX_DIST = 100.0;
float EPSILON = 0.0001;

struct Ray{
	vec3 origin;
    vec3 direction;
};

float SphereSDF(vec3 samplePoint) {
    return length(samplePoint) - 1.;
}

float ShortestDistanceToSurface(Ray ray, float start, float end) {
    float depth = start;
    for (int i = 0; i < MAX_MARCHING_STEPS; i++) {
        float dist = SphereSDF(ray.origin + depth * ray.direction);
        if (dist < EPSILON) {
            return depth;
        }
        depth += dist;
        if (depth >= end) {
            return end;
        }
    }
    return end;
}

void main()
{
	vec2 fragCoord = textureCoord * iResolution;
	
    // With better camera setup
    //vec3 dir = rayDirection(45.0, iResolution.xy, fragCoord);
    //vec3 eye = vec3(0.0, 0.0, 5.0);
    
    // Scale and bias uv
    // [0.0, iResolution.x] -> [0.0, 1.0]
    // [0.0, 1.0] 			-> [-1.0, 1.0]
    vec2 xy = fragCoord / iResolution.xy;
	xy = xy * 2.- vec2(1.);
	xy.x *= iResolution.x/iResolution.y;
    
    // SphereSDF position at (0,0,0)
      
    vec3 pixelPos = vec3(xy, 2.); // Image plane at (0,0,2)
    vec3 eyePos = vec3(0.,0.,5.); // Camera position at (0,0,5)
    vec3 rayDir = normalize(pixelPos - eyePos);
    
    float dist = ShortestDistanceToSurface(Ray(eyePos, rayDir), MIN_DIST, MAX_DIST);

    // Didn't hit anything
    if (dist > MAX_DIST - EPSILON) {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
    }

    // Hit on the surface
    fragColor = vec4(0.,0.,1.,1.);
}

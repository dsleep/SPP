// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPrimitiveShapes.h"

namespace SPP
{
    Sphere Sphere::Transform(const Matrix4x4& transformation) const
    {
        if (!_bValid)
        {
            return Sphere();
        }
        Vector4 cntPt(_center[0], _center[1], _center[2], 1);
        Vector4 transformedCnt = cntPt * transformation;
        float ScaleX = transformation.block<1, 3>(0, 0).norm();
        float ScaleY = transformation.block<1, 3>(1, 0).norm();
        float ScaleZ = transformation.block<1, 3>(2, 0).norm();
        return Sphere(transformedCnt.block<1, 3>(0, 0).cast<double>(), _radius * ScaleX);
    }

    Sphere Sphere::Transform(const Vector3d& Translate, float Scale) const
    {
        if (!_bValid)
        {
            return Sphere();
        }
        return Sphere(_center + Translate, _radius * Scale);
    }

    namespace Intersection
    {
        bool Intersect_RaySphere(const Ray& InRay, const Sphere& InSphere, Vector3d& intersectionPoint, float* timeToHit)
        {            
            Vector3 m = (InRay.GetOrigin() - InSphere.GetCenter()).cast<float>();
            float b = m.dot(InRay.GetDirection());
            float c = m.dot(m) - InSphere.GetRadius() * InSphere.GetRadius();

            // Exit if r’s origin outside s (c > 0) and r pointing away from s (b > 0) 
            if (c > 0.0f && b > 0.0f) return false;
            float discr = b * b - c;

            // A negative discriminant corresponds to ray missing sphere 
            if (discr < 0.0f) return false;

            // Ray now found to intersect sphere, compute smallest t value of intersection
            auto t = -b - std::sqrt(discr);

            // If t is negative, ray started inside sphere so clamp t to zero 
            if (t < 0.0f) t = 0.0f;
            intersectionPoint = InRay.GetOrigin() + (InRay.GetDirection() * t).cast<double>();

            if (timeToHit)
            {
                *timeToHit = t;
            }

            return true;
        }

        bool Intersect_RayTriangle (const Ray& InRay,
            const Vector3& v0, const Vector3& v1, const Vector3& v2,
            float& t, float& u, float& v, float kEpsilon)
        {
            // MOLLER_TRUMBORE 
            Vector3 v0v1 = v1 - v0;
            Vector3 v0v2 = v2 - v0;
            Vector3 pvec = InRay.GetDirection().cross(v0v2);
            float det = v0v1.dot(pvec);

#ifdef USE_CULLING 
            // if the determinant is negative the triangle is backfacing
            // if the determinant is close to 0, the ray misses the triangle
            if (det < kEpsilon) return false;
#else 
            // ray and triangle are parallel if det is close to 0
            if (fabs(det) < kEpsilon) return false;
#endif 
            float invDet = 1 / det;

            Vector3 tvec = InRay.GetOrigin().cast<float>() - v0;
            u = tvec.dot(pvec) * invDet;
            if (u < 0 || u > 1) return false;

            Vector3 qvec = tvec.cross(v0v1);
            v = InRay.GetDirection().dot(qvec) * invDet;
            if (v < 0 || u + v > 1) return false;

            t = v0v2.dot(qvec) * invDet;

            return true;
        }

        bool Intersect_SphereSphere(const Sphere& s1, const Sphere& s2)
        {
            float RadSq = s1.GetRadius() + s2.GetRadius();
            RadSq *= RadSq;
            return (s1.GetCenter() - s2.GetCenter()).squaredNorm() < RadSq;
        }

        //bool Intersect_AABBSphere(const Sphere& InSphere, const AABB &InBox) 
        //{
        //    // get box closest point to sphere center by clamping
        //    Vector3d closestPt = eigenmax(InBox.GetMin(), eigenmin(InSphere.GetCenter(), InBox.GetMax()));

        //    // this is the same as isPointInsideSphere
        //    float distanceSq = (closestPt - InSphere.GetCenter()).squaredNorm();

        //    return distanceSq < (InSphere.GetRadius()* InSphere.GetRadius());
        //}
    }
}
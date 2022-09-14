// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMath.h"

namespace SPP
{
    template<typename T>
    struct AxisAlignedBoundingBox
    {
    private:
        using DataType = T;

        T _min;
        T _max;
        bool _bIsValid = false;

    public:
        AxisAlignedBoundingBox() = default;
        AxisAlignedBoundingBox(const T& InMin, const T& InMax) : _min(InMin), _max(InMax), _bIsValid(true) {}
        AxisAlignedBoundingBox(T&& InMin, T&& InMax) : _min(std::move(InMin)), _max(std::move(InMax)), _bIsValid(true) {}
        AxisAlignedBoundingBox(const AxisAlignedBoundingBox<T>& MoveBox)
        {
            *this = MoveBox;
        }
        AxisAlignedBoundingBox(AxisAlignedBoundingBox<T>&& MoveBox) 
        {
            *this = MoveBox;
        }
        AxisAlignedBoundingBox<T>& operator=(const AxisAlignedBoundingBox<T>& MoveBox)
        {
            if (MoveBox._bIsValid)
            {
                _min = MoveBox._min;
                _max = MoveBox._max;
                _bIsValid = true;
            }
            return *this;
        }
		AxisAlignedBoundingBox<T>& operator=(AxisAlignedBoundingBox<T>&& MoveBox) noexcept
		{
            if (MoveBox._bIsValid)
            {
                _min = std::move(MoveBox._min);
                _max = std::move(MoveBox._max);
                _bIsValid = true;
            }
			return *this;
		}

        AxisAlignedBoundingBox<T> Translate(const T& InValue) const
        {
            return AxisAlignedBoundingBox<T>(_min + InValue, _max + InValue);
        }

        void PopulateCorners(T corners[8]) const
        {
            corners[0] = _min;
            corners[1] = T(_min[0], _min[1], _max[2]);
            corners[2] = T(_min[0], _max[1], _min[2]);
            corners[3] = T(_max[0], _min[1], _min[2]);
            corners[4] = T(_min[0], _max[1], _max[2]);
            corners[5] = T(_max[0], _min[1], _max[2]);
            corners[6] = T(_max[0], _max[1], _min[2]);
            corners[7] = _max;
        }

        AxisAlignedBoundingBox<T> Transform(const Matrix4x4 &transformation) const
        {
            T corners[8];
            PopulateCorners(corners);
            AxisAlignedBoundingBox<T> oAABB;
            for (int i = 0; i < 8; i++)
            {
                Vector4 expanded(corners[i][0], corners[i][1], corners[i][2], 1);
                Vector4 transformed = expanded * transformation;
                oAABB += Vector3(transformed[0], transformed[1], transformed[2]);
            }
            return oAABB;
        }

        void operator += (const T& InValue) 
        {
            if (_bIsValid)
            {
                _min = eigenmin(_min, InValue);
                _max = eigenmax(_max, InValue);
            }
            else
            {
                _min = InValue;
                _max = InValue;
                _bIsValid = true;
            }            
        }

        void operator += (const AxisAlignedBoundingBox<T>& InValue)
        {
            if (InValue.IsValid() == false)
            {
                return;
            }

            if (_bIsValid)
            {
                _min = eigenmin(_min, InValue.GetMin());
                _max = eigenmax(_max, InValue.GetMax());
            }
            else
            {
                _min = InValue.GetMin();
                _max = InValue.GetMax();
                _bIsValid = true;
            }
        }

        bool IsValid() const
        {
            return _bIsValid;
        }

        const T& GetMin() const
        {
            return _min;
        }

        const T& GetMax() const
        {
            return _max;
        }


        inline T Extent() const
        {
            return ((_max - _min) / 2);
        }

        inline T Center() const
        {
            return Extent() + _min;
        }

        inline void GetData(T& outExtent, T& outCenter) const
        {
            outExtent = Extent();
            outCenter = outExtent + _min;
        }

        inline bool Encapsulates(const AxisAlignedBoundingBox<T>& InCompare)
        {
            return (InCompare.GetMin()[0] >= _min[0] ||
                InCompare.GetMin()[1] >= _min[1] ||
                InCompare.GetMin()[2] >= _min[2] ||
                InCompare.GetMax()[0] <= _max[0] ||
                InCompare.GetMax()[1] <= _max[1] ||
                InCompare.GetMax()[2] <= _max[2]);
        }

        inline bool Intersects(const AxisAlignedBoundingBox<T>& InCompare)
        {
            return !(InCompare.GetMin()[0] > _max[0] ||
                InCompare.GetMin()[1] > _max[1] ||
                InCompare.GetMin()[2] > _max[2] ||
                InCompare.GetMax()[0] < _min[0] ||
                InCompare.GetMax()[1] < _min[1] ||
                InCompare.GetMax()[2] < _min[2]);
        }
    };

    using AABB = AxisAlignedBoundingBox<Vector3>;
    using AABBd = AxisAlignedBoundingBox<Vector3d>;
    using AABBi = AxisAlignedBoundingBox<Vector3i>;

    inline bool Intersects_AAABBi_to_UniformCenteredAABB(const AABBi& InAB, int32_t CenteredUniformExts)
    {
        return !(InAB.GetMin()[0] > CenteredUniformExts ||
            InAB.GetMin()[1] > CenteredUniformExts ||
            InAB.GetMin()[2] > CenteredUniformExts ||
            InAB.GetMax()[0] < -CenteredUniformExts ||
            InAB.GetMax()[1] < -CenteredUniformExts ||
            InAB.GetMax()[2] < -CenteredUniformExts);
    }

    inline AABBi Convert(const AABB& InAB)
    {
        return AABBi{
            Vector3i((int32_t)std::floor(InAB.GetMin()[0]),
                     (int32_t)std::floor(InAB.GetMin()[1]),
                     (int32_t)std::floor(InAB.GetMin()[2])),
            Vector3i((int32_t)std::ceil(InAB.GetMax()[0]),
                     (int32_t)std::ceil(InAB.GetMax()[1]),
                     (int32_t)std::ceil(InAB.GetMax()[2])) };
    }

    struct Spherei
    {
        Vector3i center = { 0,0,0 };
        int32_t extent = 1;
    };

    struct Sphere;

    template<typename MeshVertexType, typename VectorType>
    Sphere MinimumBoundingSphere(MeshVertexType* points, uint32_t count);

  
    struct SPP_MATH_API Ray
    {
    private:        
        Vector3d _origin;
        Vector3 _direction;

    public:
        Ray() { }
        Ray(const Vector3d &InOrigin, const Vector3 &Indirection) : _origin(InOrigin), _direction(Indirection){ }

        const Vector3d& GetOrigin() const
        {
            return _origin;
        }

        const Vector3& GetDirection() const
        {
            return _direction;
        }
    };

    struct SPP_MATH_API Sphere
    {
    protected:
        bool _bValid = false;
        Vector3d _center = { 0,0,0 };
        float _radius = 1.0f;

    public:
        Sphere() {}
        Sphere(const Vector3d& InCenter, float InRadius) : _center(InCenter), _radius(InRadius), _bValid(true) {}

        operator bool() const
        {
            return _bValid;
        }

        bool IsValid() const
        {
            return _bValid;
        }

		const Vector3d& GetCenter() const
		{
			return _center;
		}

		const float& GetRadius() const
		{
			return _radius;
		}

        void operator+=(const Sphere& Other)
        {
            if (Other._bValid == false)
            {
                return;
            }
            else if (!_bValid)
            {
                *this = Other;
                _bValid = true;
                return;
            }

            Vector3d CenterDir = Other._center - _center;
            auto dirSizeSq = CenterDir.squaredNorm();
            if (dirSizeSq < 0.01)
            {
                _radius = std::max(_radius, Other._radius);
            }
            else
            {
                //furthest point on each...
                Vector3d points[4];
                CenterDir.normalize();
                points[0] = Other._center + CenterDir * _radius;
                points[1] = Other._center + CenterDir * Other._radius;

                points[2] = _center - CenterDir * _radius;
                points[3] = _center - CenterDir * Other._radius;

                *this = MinimumBoundingSphere< Vector3d, Vector3d >(points, 4);
            }
        }

        Sphere Transform(const Matrix4x4& transformation) const;

        Sphere Transform(const Vector3d& Translate, float Scale) const;
    };

    struct Sphered
    {
        Vector3d center;
        float extent;
    };

    inline Spherei Convert(const Sphered& InValue)
    {
        return Spherei{
            Vector3i((int32_t)std::round(InValue.center[0]),
                     (int32_t)std::round(InValue.center[1]),
                     (int32_t)std::round(InValue.center[2])),
                     //to compensate for rounding
            (int32_t)std::ceil(InValue.extent + 0.5) };
    }  

    inline Spherei Convert(const Sphere& InValue)
    {
        auto& center = InValue.GetCenter();
        return Spherei{
            Vector3i((int32_t)std::round(center[0]),
                     (int32_t)std::round(center[1]),
                     (int32_t)std::round(center[2])),
                     //to compensate for rounding
            (int32_t)std::ceil(InValue.GetRadius() + 0.5)};
    }

    inline Spherei Convert(const Vector3d &center, float extent)
    {
        return Spherei{
            Vector3i((int32_t)std::round(center[0]),
                     (int32_t)std::round(center[1]),
                     (int32_t)std::round(center[2])),
                     //to compensate for rounding
            (int32_t)std::ceil(extent + 0.5) };
    }

    template<typename MeshVertexType>
    AABB GetBoundingBox(MeshVertexType* points, uint32_t count)
    {
        AABB oBounds;
        for (uint32_t i = 1; i < count; ++i)
        {
            oBounds += GetPosition(*(points + i));
        }
        return oBounds;
    }

    template<typename MeshVertexType, typename VectorType>
    Sphere MinimumBoundingSphere(MeshVertexType* points, uint32_t count)
    {
        assert(points != nullptr && count != 0);

        using Scalar = VectorType::Scalar;

        // Find the min & max points indices along each axis.
        uint32_t minAxis[3] = { 0, 0, 0 };
        uint32_t maxAxis[3] = { 0, 0, 0 };

        for (uint32_t i = 1; i < count; ++i)
        {
            VectorType &point = GetPosition(*(points + i));

            for (uint32_t j = 0; j < 3; ++j)
            {
                VectorType &min = GetPosition(points[minAxis[j]]);
                VectorType &max = GetPosition(points[maxAxis[j]]);

                minAxis[j] = point[j] < min[j] ? i : minAxis[j];
                maxAxis[j] = point[j] > max[j] ? i : maxAxis[j];
            }
        }

        // Find axis with maximum span.
        Scalar distSqMax = 0.0f;
        uint32_t axis = 0;

        for (uint32_t i = 0; i < 3u; ++i)
        {
            VectorType& min = GetPosition(points[minAxis[i]]);
            VectorType& max = GetPosition(points[maxAxis[i]]);

            auto distSq = (max - min).squaredNorm();
            if (distSq > distSqMax)
            {
                distSqMax = distSq;
                axis = i;
            }
        }

        // Calculate an initial starting center point & radius.
        VectorType p1 = GetPosition(points[minAxis[axis]]);
        VectorType p2 = GetPosition(points[maxAxis[axis]]);

        Vector3d center = (p1 + p2).cast<double>() * 0.5;
        Scalar radius = (p2 - p1).norm() * 0.5;
        auto radiusSq = radius * radius;

        // Add all our points to bounding sphere expanding radius & recalculating center point as necessary.
        for (uint32_t i = 0; i < count; ++i)
        {
            Vector3d point = GetPosition(points[i]).cast<double>();
            Scalar distSq = (point - center).squaredNorm();

            if (distSq > radiusSq)
            {
                Scalar dist = std::sqrt(distSq);
                Scalar k = (radius / dist) * 0.5 + 0.5;

                center = center * k + point * (1.0 - k);
                radius = (radius + dist) * 0.5;
            }
        }

        // Populate a sphere
        return Sphere( center, (float)radius );
    }

    template<typename T, typename D, size_t N>
    inline bool BoxInFrustum(  const PlaneT<T> InPlanes[N], const AxisAlignedBoundingBox<D> &InBox)
    {
        using BoxVector3 = Eigen::Matrix< T, 1, 3, Eigen::RowMajor >;

        BoxVector3 minB = InBox.GetMin().template cast<double>();
        BoxVector3 maxB = InBox.GetMax().template cast<double>();
        BoxVector3 boxCorners[8] = {
            BoxVector3(minB[0], minB[1], minB[2]),
            BoxVector3(maxB[0], minB[1], minB[2]),
            BoxVector3(minB[0], maxB[1], minB[2]),
            BoxVector3(maxB[0], maxB[1], minB[2]),
            BoxVector3(minB[0], minB[1], maxB[2]),
            BoxVector3(maxB[0], minB[1], maxB[2]),
            BoxVector3(minB[0], maxB[1], maxB[2]),
            BoxVector3(maxB[0], maxB[1], maxB[2])
        };

        // check box outside/inside of frustum
        for (int i = 0; i < N; i++)
        {
            int out = 0;
            for (int j = 0; j < 8; j++)
            {
                out += (InPlanes[i].signedDistance(boxCorners[j]) < 0.0) ? 1 : 0;
            }
            if (out == 8) return false;
        }

        //less conservative
        // check frustum outside/inside box
        //int out;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].x > box.mMaxX) ? 1 : 0); if (out == 8) return false;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].x < box.mMinX) ? 1 : 0); if (out == 8) return false;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].y > box.mMaxY) ? 1 : 0); if (out == 8) return false;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].y < box.mMinY) ? 1 : 0); if (out == 8) return false;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].z > box.mMaxZ) ? 1 : 0); if (out == 8) return false;
        //out = 0; for (int i = 0; i < 8; i++) out += ((InPlanes[i].z < box.mMinZ) ? 1 : 0); if (out == 8) return false;

        return true;
    }

	
    namespace Intersection
    {
        // Intersects ray r = p + td, |d| = 1, with sphere s and, if intersecting, 
        // returns t value of intersection and intersection point q 
        SPP_MATH_API bool Intersect_RaySphere(const Ray &InRay, const Sphere &InSphere, Vector3d& intersectionPoint, float *timeToHit=nullptr);

        SPP_MATH_API bool Intersect_RayTriangle(const Ray& InRay, const Vector3& v0, const Vector3& v1, const Vector3& v2, float& t, float& u, float& v, float kEpsilon = 1e-8);

        SPP_MATH_API bool Intersect_SphereSphere(const Sphere& s1, const Sphere& s2);

        SPP_MATH_API bool Intersect_AABBSphere(const Sphere& InSphere, const AABB& InBox);
    }


    template<>
    inline BinarySerializer& operator<< <AABB> (BinarySerializer& Storage, const AABB& Value)
    {
        bool IsValid = Value.IsValid();
        Storage << IsValid;
        if (IsValid)
        {
            Storage << Value.GetMin();
            Storage << Value.GetMax();
        }
        return Storage;
    }
    template<>
    inline BinarySerializer& operator>> <AABB> (BinarySerializer& Storage, AABB& Value)
    {
        bool IsValid = false;
        Storage >> IsValid;
        if (IsValid)
        {
            Vector3 min, max;
            Storage >> min;
            Storage >> max;
            Value = AABB(min, max);
        }
        else
        {
            Value = AABB();
        }
        return Storage;
    }


    template<>
    inline BinarySerializer& operator<< <Sphere> (BinarySerializer& Storage, const Sphere& Value)
    {
        bool IsValid = Value.IsValid();
        Storage << IsValid;
        if (IsValid)
        {
            Storage << Value.GetCenter();
            Storage << Value.GetRadius();
        }
        return Storage;
    }
    template<>
    inline BinarySerializer& operator>> <Sphere> (BinarySerializer& Storage, Sphere& Value)
    {
        bool IsValid = false;
        Storage >> IsValid;
        if (IsValid)
        {
            Vector3d center;
            float radius;
            Storage >> center;
            Storage >> radius;
            Value = Sphere(center, radius);
        }
        else
        {
            Value = Sphere();
        }
        return Storage;
    }
}

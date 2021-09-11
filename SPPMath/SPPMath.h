// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSerialization.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <Eigen/Geometry> 

#if _WIN32 && !defined(SPP_MATH_STATIC)

	#ifdef SPP_MATH_EXPORT
		#define SPP_MATH_API __declspec(dllexport)
	#else
		#define SPP_MATH_API __declspec(dllimport)
	#endif

#else

	#define SPP_MATH_API 

#endif

namespace SPP
{
	//matrix storage
	using Matrix2x2 = Eigen::Matrix< float, 2, 2, Eigen::RowMajor >;
	using Matrix3x3 = Eigen::Matrix< float, 3, 3, Eigen::RowMajor >;
	using Matrix4x4 = Eigen::Matrix< float, 4, 4, Eigen::RowMajor >;

    using Matrix4x4d = Eigen::Matrix< double, 4, 4, Eigen::RowMajor >;

	using Vector2 = Eigen::Matrix< float, 1, 2, Eigen::RowMajor >;
	using _AsEigenVector3 = Eigen::Matrix< float, 1, 3, Eigen::RowMajor >;

    class Vector3 : public _AsEigenVector3
    {
    public:
        Vector3() : _AsEigenVector3() {}
        typedef _AsEigenVector3 Base;
        typedef float Scalar;

        // This constructor allows you to construct MyVectorType from Eigen expressions
        template<typename OtherDerived>
        Vector3(const Eigen::MatrixBase<OtherDerived>& other)
            : Base(other)
        { }
        // This method allows you to assign Eigen expressions to MyVectorType
        template<typename OtherDerived>
        Vector3& operator= (const Eigen::MatrixBase <OtherDerived>& other)
        {
            this->Base::operator=(other);
            return *this;
        }

        //Vector3(Vector3&& other) 
        //    : Base(std::move(other))
        //{
        //    Base::_check_template_params();
        //}         
        //Vector3& operator=(Vector3&& other)
        //{
        //    other.swap(*this);
        //    return *this;
        //}

        Vector3(const Scalar& x, const Scalar& y, const Scalar& z)
        {
            Base::_check_template_params();
            EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 3)
            m_storage.data()[0] = x;
            m_storage.data()[1] = y;
            m_storage.data()[2] = z;
        }

        void SetX(float InValue)
        {
            (*this)(0) = InValue;
        }
        void SetY(float InValue)
        {
            (*this)(1) = InValue;
        }
        void SetZ(float InValue)
        {
            (*this)(2) = InValue;
        }

        float GetX() const
        {
            return (*this)(0);
        }
        float GetY() const
        {
            return (*this)(1);
        }
        float GetZ() const
        {
            return (*this)(2);
        }
    };

	using Vector4 = Eigen::Matrix< float, 1, 4, Eigen::RowMajor >;

    using Vector2i = Eigen::Matrix< int32_t, 1, 2, Eigen::RowMajor >;
	using Vector3i = Eigen::Matrix< int32_t, 1, 3, Eigen::RowMajor >;
    using Vector4i = Eigen::Matrix< int32_t, 1, 4, Eigen::RowMajor >;
        
    using Vector2ui = Eigen::Matrix< uint32_t, 1, 2, Eigen::RowMajor >;
    using Vector3ui = Eigen::Matrix< uint32_t, 1, 3, Eigen::RowMajor >;
    using Vector4ui = Eigen::Matrix< uint32_t, 1, 4, Eigen::RowMajor >;

	using Vector2d = Eigen::Matrix< double, 1, 2, Eigen::RowMajor >;
	using Vector3d = Eigen::Matrix< double, 1, 3, Eigen::RowMajor >;
	using Vector4d = Eigen::Matrix< double, 1, 4, Eigen::RowMajor >;

    using Quarternion = Eigen::Quaternion< float >;
    using AxisAngle = Eigen::AngleAxis< float >;

    using Color3 = Eigen::Matrix< uint8_t, 1, 3, Eigen::RowMajor >;
    using Color4 = Eigen::Matrix< uint8_t, 1, 4, Eigen::RowMajor >;

    using Plane = Eigen::Hyperplane< float, 3 >;
    using Planed = Eigen::Hyperplane< double, 3 >;

    template<typename T>
    using PlaneT = Eigen::Hyperplane< T, 3 >;

    template<typename T>
    T RandomFloat(T a, T b) {
        T random = ((T)rand()) / (T)RAND_MAX;
        T diff = b - a;
        T r = random * diff;
        return a + r;
    }

    // An integer version of ceil(value / divisor)
    template <typename T, typename U>
    T DivRoundUp(T num, U denom)
    {
        return (num + denom - 1) / denom;
    }

	//matrix operations
	


	//radians = ( degrees * pi ) / 180 ;
	
	template<typename T>
	T DegToRad(const T& InValue)
	{
		return InValue * (M_PI / 180.0);
	}

	template<typename T>
	T RadToDeg(const T & InValue)
	{
		return InValue * (180.0 / M_PI);
	}

    //template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
    //Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > eigenMatPerOp(
    //    const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InA, 
    //    const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InB,
    //    std::function<void(const _Scalar&, const _Scalar&, _Scalar&)> Func)
    //{
    //    Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > outMat;
    //    for (int32_t IterY = 0; IterY < _Rows; IterY++)
    //    {
    //        for (int32_t IterX = 0; IterX < _Cols; IterX++)
    //        {
    //            Func(InA(IterX, IterY), InB(IterX, IterY), outMat(IterX, IterY));
    //        }
    //    }    
    //    return outMat;
    //}

    template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
    Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > eigenmin(  const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InA,
                                                                                const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InB)
    {
        Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > outMat;
        for (int32_t IterY = 0; IterY < _Rows; IterY++)
        {
            for (int32_t IterX = 0; IterX < _Cols; IterX++)
            {
                outMat(IterY, IterX) = std::min(InA(IterY, IterX), InB(IterY, IterX));
            }
        }
        return outMat;
    };

    template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
    Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > eigenmax(const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InA,
        const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& InB)
    {
        Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols > outMat;
        for (int32_t IterY = 0; IterY < _Rows; IterY++)
        {
            for (int32_t IterX = 0; IterX < _Cols; IterX++)
            {
                outMat(IterY, IterX) = std::max(InA(IterY, IterX), InB(IterY, IterX));
            }
        }
        return outMat;
    };

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
        AxisAlignedBoundingBox(const T& InMin, const T& InMax) : _min(InMin), _max(InMax), _bIsValid(true)
        {}

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

    template<typename MeshVertexType>
    Sphere MinimumBoundingSphere(MeshVertexType* points, uint32_t count);

    struct Sphere
    {
        bool bValid = false;
        Vector3 center = { 0,0,0 };
        float extent = 1.0f;

        Sphere()
        {

        }
        Sphere(const Vector3& InCenter, float InExtent) : center(InCenter), extent(InExtent), bValid(true) {}

        operator bool() const
        {
            return bValid;
        }

        void operator+=(const Sphere& Other)
        {
            if (Other.bValid == false)
            {
                return;
            }
            else if (!bValid)
            {
                *this = Other;
                bValid = true;
                return;
            }

            Vector3 CenterDir = Other.center - center;
            auto dirSizeSq = CenterDir.squaredNorm();
            if (dirSizeSq < 0.01)
            {
                extent = std::max(extent, Other.extent);
            }
            else
            {
                //furthest point on each...
                Vector3 points[4];
                CenterDir.normalize();
                points[0] = Other.center + CenterDir * extent;
                points[1] = Other.center + CenterDir * Other.extent;

                points[2] = center - CenterDir * extent;
                points[3] = center - CenterDir * Other.extent;

                *this = MinimumBoundingSphere(points, 4);
            }
        }
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

    inline Spherei Convert(const Vector3d &center, float extent)
    {
        return Spherei{
            Vector3i((int32_t)std::round(center[0]),
                     (int32_t)std::round(center[1]),
                     (int32_t)std::round(center[2])),
                     //to compensate for rounding
            (int32_t)std::ceil(extent + 0.5) };
    }

    template<typename T>
    T roundUpToPow2(const T& InValue)
    {
        return (T)pow(2, ceil(log(InValue) / log(2)));
    }

    template<typename T>
    T powerOf2(const T& InValue)
    {
        return (T)ceil(log(InValue) / log(2));
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

   

    inline Vector3& GetPosition(Vector3& InVertex)
    {
        return InVertex;
    }

    template<typename MeshVertexType>
    Sphere MinimumBoundingSphere(MeshVertexType* points, uint32_t count)
    {
        assert(points != nullptr && count != 0);

        // Find the min & max points indices along each axis.
        uint32_t minAxis[3] = { 0, 0, 0 };
        uint32_t maxAxis[3] = { 0, 0, 0 };

        for (uint32_t i = 1; i < count; ++i)
        {
            float* point = (float*)&GetPosition(*(points + i));

            for (uint32_t j = 0; j < 3; ++j)
            {
                float* min = (float*)(&GetPosition(points[minAxis[j]]));
                float* max = (float*)(&GetPosition(points[maxAxis[j]]));

                minAxis[j] = point[j] < min[j] ? i : minAxis[j];
                maxAxis[j] = point[j] > max[j] ? i : maxAxis[j];
            }
        }

        // Find axis with maximum span.
        float distSqMax = 0.0f;
        uint32_t axis = 0;

        for (uint32_t i = 0; i < 3u; ++i)
        {
            Vector3& min = GetPosition(points[minAxis[i]]);
            Vector3& max = GetPosition(points[maxAxis[i]]);

            auto distSq = (max - min).squaredNorm();
            if (distSq > distSqMax)
            {
                distSqMax = distSq;
                axis = i;
            }
        }

        // Calculate an initial starting center point & radius.
        auto p1 = GetPosition(points[minAxis[axis]]);
        auto p2 = GetPosition(points[maxAxis[axis]]);

        Vector3 center = (p1 + p2) * 0.5f;
        auto radius = (p2 - p1).norm() * 0.5f;
        auto radiusSq = radius * radius;

        // Add all our points to bounding sphere expanding radius & recalculating center point as necessary.
        for (uint32_t i = 0; i < count; ++i)
        {
            auto point = GetPosition(points[i]);
            float distSq = (point - center).squaredNorm();

            if (distSq > radiusSq)
            {
                float dist = std::sqrtf(distSq);
                float k = (radius / dist) * 0.5f + 0.5f;

                center = center * k + point * (1.0f - k);
                radius = (radius + dist) * 0.5f;
            }
        }

        // Populate a sphere
        return Sphere( center, radius );
    }

    template<typename T, typename D>
    bool boxInFrustum(  const PlaneT<T> InPlanes[6], const AxisAlignedBoundingBox<D> &InBox)
    {
        using BoxVector3 = Eigen::Matrix< T, 1, 3, Eigen::RowMajor >;

        // check box outside/inside of frustum
        for (int i = 0; i < 6; i++)
        {
            int out = 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMin()[0], InBox.GetMin()[1], InBox.GetMin()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMax()[0], InBox.GetMin()[1], InBox.GetMin()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMin()[0], InBox.GetMax()[1], InBox.GetMin()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMax()[0], InBox.GetMax()[1], InBox.GetMin()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMin()[0], InBox.GetMin()[1], InBox.GetMax()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMax()[0], InBox.GetMin()[1], InBox.GetMax()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMin()[0], InBox.GetMax()[1], InBox.GetMax()[2])) < 0.0) ? 1 : 0;
            out += (InPlanes[i].signedDistance(BoxVector3(InBox.GetMax()[0], InBox.GetMax()[1], InBox.GetMax()[2])) < 0.0) ? 1 : 0;
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

    inline size_t CRCHash(const uint32_t* dwords, uint32_t dwordCount)
    {
        size_t h = 0;

        for (uint32_t i = 0; i < dwordCount; ++i)
        {
            uint32_t highOrd = h & 0xf8000000;
            h = h << 5;
            h = h ^ (highOrd >> 27);
            h = h ^ size_t(dwords[i]);
        }

        return h;
    }

    template <typename T>
    inline size_t Hash(const T& val)
    {
        return std::hash<T>()(val);
    }

    
    template<>
    inline BinarySerializer& operator<< <Vector2> (BinarySerializer& Storage, const Vector2& Value)
    {
        Storage << Value[0];
        Storage << Value[1];
        return Storage;
    }

    template<>
    inline BinarySerializer& operator<< <Vector3> (BinarySerializer& Storage, const Vector3& Value)
    {
        Storage << Value[0];
        Storage << Value[1];
        Storage << Value[2];
        return Storage;
    }

    template<>
    inline BinarySerializer& operator>> <Vector2> (BinarySerializer& Storage, Vector2& Value)
    {
        Storage >> Value[0];
        Storage >> Value[1];
        return Storage;
    }

    template<>
    inline BinarySerializer& operator>> <Vector3> (BinarySerializer& Storage, Vector3& Value)
    {
        Storage >> Value[0];
        Storage >> Value[1];
        Storage >> Value[2];
        return Storage;
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
		
    SPP_MATH_API uint32_t GetMathVersion();
}

namespace std
{
    template <> struct hash<SPP::Vector3> { size_t operator()(const SPP::Vector3& v) const { return SPP::CRCHash(reinterpret_cast<const uint32_t*>(&v), sizeof(v) / 4); } };
}
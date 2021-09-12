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
    T roundUpToPow2(const T& InValue)
    {
        return (T)pow(2, ceil(log(InValue) / log(2)));
    }

    template<typename T>
    T powerOf2(const T& InValue)
    {
        return (T)ceil(log(InValue) / log(2));
    }      

    inline Vector3& GetPosition(Vector3& InVertex)
    {
        return InVertex;
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

    
		
    SPP_MATH_API uint32_t GetMathVersion();
}

namespace std
{
    template <> struct hash<SPP::Vector3> { size_t operator()(const SPP::Vector3& v) const { return SPP::CRCHash(reinterpret_cast<const uint32_t*>(&v), sizeof(v) / 4); } };
}
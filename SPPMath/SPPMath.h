// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSerialization.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <cmath>

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
    using Vector4 = Eigen::Matrix< float, 1, 4, Eigen::RowMajor >;

    using Vector2i = Eigen::Matrix< int32_t, 1, 2, Eigen::RowMajor >;
    using Vector3i = Eigen::Matrix< int32_t, 1, 3, Eigen::RowMajor >;
    using Vector4i = Eigen::Matrix< int32_t, 1, 4, Eigen::RowMajor >;

    using Vector2ui = Eigen::Matrix< uint32_t, 1, 2, Eigen::RowMajor >;
    using Vector3ui = Eigen::Matrix< uint32_t, 1, 3, Eigen::RowMajor >;
    using Vector4ui = Eigen::Matrix< uint32_t, 1, 4, Eigen::RowMajor >;

    using Vector2d = Eigen::Matrix< double, 1, 2, Eigen::RowMajor >;    
    using _AsEigenVector3d = Eigen::Matrix< double, 1, 3, Eigen::RowMajor >;
    using _AsEigenVector4d = Eigen::Matrix< double, 1, 4, Eigen::RowMajor >;

    namespace EAxis
    {
        enum Value
        {
            None = 0,
            X = (1 << 0),
            Y = (1 << 1),
            Z = (1 << 2)
        };
    };

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

        void SetX(float InValue) { (*this)(0) = InValue; }
        void SetY(float InValue) { (*this)(1) = InValue; }
        void SetZ(float InValue) { (*this)(2) = InValue; }

        float GetX() const { return (*this)(0); }
        float GetY() const { return (*this)(1); }
        float GetZ() const { return (*this)(2); }
    };

    class Vector3d : public _AsEigenVector3d
    {
    public:
        Vector3d() : _AsEigenVector3d() {}
        typedef _AsEigenVector3d Base;
        typedef double Scalar;

        // This constructor allows you to construct MyVectorType from Eigen expressions
        template<typename OtherDerived>
        Vector3d(const Eigen::MatrixBase<OtherDerived>& other)
            : Base(other)
        { }
        // This method allows you to assign Eigen expressions to MyVectorType
        template<typename OtherDerived>
        Vector3d& operator= (const Eigen::MatrixBase <OtherDerived>& other)
        {
            this->Base::operator=(other);
            return *this;
        }

        //Vector4d(Vector4d&& other) 
        //    : Base(std::move(other))
        //{
        //    Base::_check_template_params();
        //}         
        //Vector4d& operator=(Vector4d&& other)
        //{
        //    other.swap(*this);
        //    return *this;
        //}

        Vector3d(const Scalar& x, const Scalar& y, const Scalar& z)
        {
            Base::_check_template_params();
            EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 3)

            m_storage.data()[0] = x;
            m_storage.data()[1] = y;
            m_storage.data()[2] = z;
        }

        void SetX(double InValue) { (*this)(0) = InValue; }
        void SetY(double InValue) { (*this)(1) = InValue; }
        void SetZ(double InValue) { (*this)(2) = InValue; }

        double GetX() const { return (*this)(0); }
        double GetY() const { return (*this)(1); }
        double GetZ() const { return (*this)(2); }
    };

    class Vector4d : public _AsEigenVector4d
    {
    public:
        Vector4d() : _AsEigenVector4d() {}
        typedef _AsEigenVector4d Base;
        typedef double Scalar;

        // This constructor allows you to construct MyVectorType from Eigen expressions
        template<typename OtherDerived>
        Vector4d(const Eigen::MatrixBase<OtherDerived>& other)
            : Base(other)
        { }
        // This method allows you to assign Eigen expressions to MyVectorType
        template<typename OtherDerived>
        Vector4d& operator= (const Eigen::MatrixBase <OtherDerived>& other)
        {
            this->Base::operator=(other);
            return *this;
        }

        //Vector4d(Vector4d&& other) 
        //    : Base(std::move(other))
        //{
        //    Base::_check_template_params();
        //}         
        //Vector4d& operator=(Vector4d&& other)
        //{
        //    other.swap(*this);
        //    return *this;
        //}

        Vector4d(const Scalar& x, const Scalar& y, const Scalar& z, const Scalar& w)
        {
            Base::_check_template_params();
            EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 4)
            
            m_storage.data()[0] = x;
            m_storage.data()[1] = y;
            m_storage.data()[2] = z;
            m_storage.data()[3] = w;
        }

        void SetX(double InValue) { (*this)(0) = InValue; }
        void SetY(double InValue) { (*this)(1) = InValue; }
        void SetZ(double InValue) { (*this)(2) = InValue; }
        void SetW(double InValue) { (*this)(3) = InValue; }

        double GetX() const { return (*this)(0); }
        double GetY() const { return (*this)(1); }
        double GetZ() const { return (*this)(2); }
        double GetW() const { return (*this)(3); }
    };

	

    using Quarternion = Eigen::Quaternion< float >;
    using AxisAngle = Eigen::AngleAxis< float >;

    using Color3 = Eigen::Matrix< uint8_t, 1, 3, Eigen::RowMajor >;
    using Color4 = Eigen::Matrix< uint8_t, 1, 4, Eigen::RowMajor >;


    template<typename T>
    using PlaneT = Eigen::Hyperplane< T, 3 >;

    using Plane = PlaneT< float >;
    using Planed = PlaneT< double >;

    using RayD = Eigen::ParametrizedLine< double, 3 >;

    inline Vector4 ToVector4(const Vector3& InVector)
    {
        return Vector4(InVector[0], InVector[1], InVector[2], 1.0f);
    }

    inline Vector3 ToVector3(const Vector4& InVector)
    {
        return InVector.head<3>();
    }

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

    template <typename T, typename U>
    T RoundUp(T num, U round)
    {
        return ((num + round - 1) & ~(round - 1));
    }

    //radians = ( degrees * pi ) / 180 ;
	
	template<typename T>
	T DegToRad(const T& InValue)
	{
		return (T)(InValue * (M_PI / 180.0));
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
    T roundDownToPow2(const T& InValue)
    {
        return (T)pow(2, floor(log(InValue) / log(2)));
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

    inline Vector3d& GetPosition(Vector3d& InVertex)
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
    inline BinarySerializer& operator<< <Vector3d> (BinarySerializer& Storage, const Vector3d& Value)
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
    inline BinarySerializer& operator>> <Vector3d> (BinarySerializer& Storage, Vector3d& Value)
    {
        Storage >> Value[0];
        Storage >> Value[1];
        Storage >> Value[2];
        return Storage;
    }

    inline Eigen::Quaternionf EulerAnglesToQuaternion(const Vector3 &InEulerAngles)
    {
        const float degToRad = 0.0174533f;
        Eigen::AngleAxisf xAngle(InEulerAngles[0] * degToRad, Vector3::UnitX());
        Eigen::AngleAxisf yAngle(InEulerAngles[1] * degToRad, Vector3::UnitY());
        Eigen::AngleAxisf zAngle(InEulerAngles[2] * degToRad, Vector3::UnitZ());

        return (zAngle * xAngle) * yAngle;
    }

    inline Vector3 ToEulerAngles(const Eigen::Quaternion<float>& q)
    {
        Vector3 angles;
        const auto x = q.x();
        const auto y = q.y();
        const auto z = q.z();
        const auto w = q.w();

        // pitch
        double sinr_cosp = 2 * (w * x + y * z);
        double cosr_cosp = 1 - 2 * (x * x + y * y);
        angles[0] = (float)std::atan2(sinr_cosp, cosr_cosp);

        // yaw
        double sinp = 2 * (w * y - z * x);
        if (std::abs(sinp) >= 1)
            angles[1] = (float)std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
        else
            angles[1] = (float)std::asin(sinp);

        // roll
        double siny_cosp = 2 * (w * z + x * y);
        double cosy_cosp = 1 - 2 * (y * y + z * z);
        angles[2] = (float)std::atan2(siny_cosp, cosy_cosp);

        return angles;
    }

    inline Vector3 ToEulerAngles(const Matrix3x3& RotMat)
    {
        return ToEulerAngles(Eigen::Quaternion<float>(RotMat));
    }

    template <typename T> 
    int8_t sgn(T val) 
    {
        return (T(0) < val) - (val < T(0));
    }
		
    SPP_MATH_API uint32_t GetMathVersion();
}

namespace std
{
    template <> struct hash<SPP::Vector3> { size_t operator()(const SPP::Vector3& v) const { return SPP::CRCHash(reinterpret_cast<const uint32_t*>(&v), sizeof(v) / 4); } };
}
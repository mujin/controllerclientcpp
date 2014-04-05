// -*- coding: utf-8 -*-
// Copyright (C) 2012-2014 MUJIN Inc. <rosen.diankov@mujin.co.jp>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file   geometry.h
    \brief  Basic gemoetric primitives and affine math functions on them.

 */
#ifndef MUJIN_GEOMETRY_H
#define MUJIN_GEOMETRY_H

#include <cmath>
#include <vector>
#include <string>
#include <limits>
#include <utility> // for std::pair
#include <cstdlib>

#ifdef BOOST_ASSERT
#define MATH_ASSERT BOOST_ASSERT
#else
#include <cassert>
#define MATH_ASSERT assert
#endif

/// Templated math and geometric functions
namespace geometry {

template <typename T> class Transform;
template <typename T> class TransformMatrix;

inline float MATH_SQRT(float f) {
    return sqrtf(f);
}
inline double MATH_SQRT(double f) {
    return sqrt(f);
}
inline float MATH_SIN(float f) {
    return sinf(f);
}
inline double MATH_SIN(double f) {
    return sin(f);
}
inline float MATH_COS(float f) {
    return cosf(f);
}
inline double MATH_COS(double f) {
    return cos(f);
}
inline float MATH_FABS(float f) {
    return fabsf(f);
}
inline double MATH_FABS(double f) {
    return fabs(f);
}
inline float MATH_ACOS(float f) {
    return acosf(f);
}
inline double MATH_ACOS(double f) {
    return acos(f);
}
inline float MATH_ASIN(float f) {
    return asinf(f);
}
inline double MATH_ASIN(double f) {
    return asin(f);
}
inline float MATH_ATAN2(float fy, float fx) {
    return atan2f(fy,fx);
}
inline double MATH_ATAN2(double fy, double fx) {
    return atan2(fy,fx);
}

/** \brief Vector class containing 4 dimensions.

    \ingroup affine_math
     It is better to use this for a 3 dim vector because it is 16byte aligned and SIMD instructions can be used
 */
template <typename T>
class Vector
{
public:
    T x, y, z, w;

    Vector() : x(0), y(0), z(0), w(0) {
    }

    Vector(T x, T y, T z) : x(x), y(y), z(z), w(0) {
    }
    Vector(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {
    }
    template<typename U> Vector(const Vector<U> &vec) : x((T)vec.x), y((T)vec.y), z((T)vec.z), w((T)vec.w) {
    }

    /// note, it only copes 3 values!
    template<typename U> Vector(const U* pf) {
        MATH_ASSERT(pf != NULL); x = (T)pf[0]; y = (T)pf[1]; z = (T)pf[2]; w = 0;
    }

    T operator[] (int i) const {
        return (&x)[i];
    }
    T& operator[] (int i) {
        return (&x)[i];
    }

    template <typename U>
    Vector<T>& operator=(const Vector<U>&r) {
        x = (T)r.x; y = (T)r.y; z = (T)r.z; w = (T)r.w; return *this;
    }

    // SCALAR FUNCTIONS
    template <typename U> inline T dot(const Vector<U> &v) const {
        return x*v.x + y*v.y + z*v.z + w*v.w;
    }
    template <typename U> inline T dot3(const Vector<U> &v) const {
        return x*v.x + y*v.y + z*v.z;
    }
    inline Vector<T>& normalize() {
        return normalize4();
    }
    inline Vector<T>& normalize4() {
        T f = x*x+y*y+z*z+w*w;
        if(( f < T(1)-std::numeric_limits<T>::epsilon()) ||( f > T(1)+std::numeric_limits<T>::epsilon()) ) {
            MATH_ASSERT( f > 0 );
            // yes it is faster to multiply by (1/f), but with 4 divides we gain precision (which is more important in robotics)
            f = MATH_SQRT(f);
            x /= f; y /= f; z /= f; w /= f;
        }
        return *this;
    }
    inline Vector<T>& normalize3() {
        T f = x*x+y*y+z*z;
        if(( f < T(1)-std::numeric_limits<T>::epsilon()) ||( f > T(1)+std::numeric_limits<T>::epsilon()) ) {
            MATH_ASSERT( f > 0 );
            f = MATH_SQRT(f);
            x /= f; y /= f; z /= f;
        }
        return *this;
    }

    inline T lengthsqr2() const {
        return x*x + y*y;
    }
    inline T lengthsqr3() const {
        return x*x + y*y + z*z;
    }
    inline T lengthsqr4() const {
        return x*x + y*y + z*z + w*w;
    }

    inline void Set3(const T* pvals) {
        x = pvals[0]; y = pvals[1]; z = pvals[2];
    }
    inline void Set3(T val1, T val2, T val3) {
        x = val1; y = val2; z = val3;
    }
    inline void Set4(const T* pvals) {
        x = pvals[0]; y = pvals[1]; z = pvals[2]; w = pvals[3];
    }
    inline void Set4(T val1, T val2, T val3, T val4) {
        x = val1; y = val2; z = val3; w = val4;
    }
    /// 3 dim cross product, w is not touched
    inline Vector<T> cross(const Vector<T> &v) const {
        Vector<T> ucrossv;
        ucrossv[0] = y * v[2] - z * v[1];
        ucrossv[1] = z * v[0] - x * v[2];
        ucrossv[2] = x * v[1] - y * v[0];
        return ucrossv;
    }

    inline Vector<T> operator-() const {
        Vector<T> v; v.x = -x; v.y = -y; v.z = -z; v.w = -w; return v;
    }
    template <typename U> inline Vector<T> operator+(const Vector<U> &r) const {
        Vector<T> v; v.x = x+T(r.x); v.y = y+T(r.y); v.z = z+T(r.z); v.w = w+T(r.w); return v;
    }
    template <typename U> inline Vector<T> operator-(const Vector<U> &r) const {
        Vector<T> v; v.x = x-T(r.x); v.y = y-T(r.y); v.z = z-T(r.z); v.w = w-T(r.w); return v;
    }
    template <typename U> inline Vector<T> operator*(const Vector<U> &r) const {
        Vector<T> v; v.x = T(r.x)*x; v.y = T(r.y)*y; v.z = T(r.z)*z; v.w = T(r.w)*w; return v;
    }
    inline Vector<T> operator*(T k) const {
        Vector<T> v; v.x = k*x; v.y = k*y; v.z = k*z; v.w = k*w; return v;
    }

    template <typename U> inline Vector<T>& operator += (const Vector<U>&r) {
        x += T(r.x); y += T(r.y); z += T(r.z); w += T(r.w); return *this;
    }
    template <typename U> inline Vector<T>& operator -= (const Vector<U>&r) {
        x -= T(r.x); y -= T(r.y); z -= T(r.z); w -= T(r.w); return *this;
    }
    template <typename U> inline Vector<T>& operator *= (const Vector<U>&r) {
        x *= T(r.x); y *= T(r.y); z *= T(r.z); w *= T(r.w); return *this;
    }

    inline Vector<T>& operator *= (const T k) {
        x *= k; y *= k; z *= k; w *= k; return *this;
    }
    inline Vector<T>& operator /= (const T _k) {
        T k=1/_k; x *= k; y *= k; z *= k; w *= k; return *this;
    }

    template <typename U> friend Vector<U> operator* (float f, const Vector<U>&v);
    template <typename U> friend Vector<U> operator* (double f, const Vector<U>&v);

    template <typename U> friend std::ostream& operator<<(std::ostream& O, const Vector<U>&v);
    template <typename U> friend std::istream& operator>>(std::istream& I, Vector<U>&v);

    /// cross product operator
    template <typename U> inline Vector<T> operator^(const Vector<U> &v) const {
        Vector<T> ucrossv;
        ucrossv[0] = y * v[2] - z * v[1];
        ucrossv[1] = z * v[0] - x * v[2];
        ucrossv[2] = x * v[1] - y * v[0];
        return ucrossv;
    }
};

template <typename T>
inline Vector<T> operator* (float f, const Vector<T>&left)
{
    Vector<T> v;
    v.x = (T)f * left.x;
    v.y = (T)f * left.y;
    v.z = (T)f * left.z;
    v.w = (T)f * left.w;
    return v;
}

template <typename T>
inline Vector<T> operator* (double f, const Vector<T>&left)
{
    Vector<T> v;
    v.x = (T)f * left.x;
    v.y = (T)f * left.y;
    v.z = (T)f * left.z;
    v.w = (T)f * left.w;
    return v;
}

/** \brief Affine transformation parameterized with quaterions.

    \ingroup affine_math
 */
template <typename T>
class Transform
{
public:
    Transform() {
        rot.x = 1;
    }
    template <typename U> Transform(const Transform<U>& t) {
        rot = t.rot;
        trans = t.trans;
        T fnorm = rot.lengthsqr4();
        MATH_ASSERT( fnorm > 0.99f && fnorm < 1.01f );
    }
    template <typename U> Transform(const Vector<U>& rot, const Vector<U>& trans) : rot(rot), trans(trans) {
        T fnorm = rot.lengthsqr4();
        MATH_ASSERT( fnorm > 0.99f && fnorm < 1.01f );
    }
    inline Transform(const TransformMatrix<T>&t);

    void identity() {
        rot.x = 1; rot.y = rot.z = rot.w = 0;
        trans.x = trans.y = trans.z = 0;
    }

    /// transform a 3 dim vector
    inline Vector<T> operator* (const Vector<T>&r) const {
        return trans + rotate(r);
    }

    /// transform a vector by the rotation component only
    inline Vector<T> rotate(const Vector<T>& r) const {
        T xx = 2 * rot.y * rot.y;
        T xy = 2 * rot.y * rot.z;
        T xz = 2 * rot.y * rot.w;
        T xw = 2 * rot.y * rot.x;
        T yy = 2 * rot.z * rot.z;
        T yz = 2 * rot.z * rot.w;
        T yw = 2 * rot.z * rot.x;
        T zz = 2 * rot.w * rot.w;
        T zw = 2 * rot.w * rot.x;

        Vector<T> v;
        v.x = (1-yy-zz) * r.x + (xy-zw) * r.y + (xz+yw)*r.z;
        v.y = (xy+zw) * r.x + (1-xx-zz) * r.y + (yz-xw)*r.z;
        v.z = (xz-yw) * r.x + (yz+xw) * r.y + (1-xx-yy)*r.z;
        return v;
    }

    /// transform a transform by the rotation component only
    inline Transform<T> rotate(const Transform<T>& r) const {
        Transform<T> t;
        t.trans = rotate(r.trans);
        t.rot.x = rot.x*r.rot.x - rot.y*r.rot.y - rot.z*r.rot.z - rot.w*r.rot.w;
        t.rot.y = rot.x*r.rot.y + rot.y*r.rot.x + rot.z*r.rot.w - rot.w*r.rot.z;
        t.rot.z = rot.x*r.rot.z + rot.z*r.rot.x + rot.w*r.rot.y - rot.y*r.rot.w;
        t.rot.w = rot.x*r.rot.w + rot.w*r.rot.x + rot.y*r.rot.z - rot.z*r.rot.y;
        // normalize the transformation
        T fnorm = t.rot.lengthsqr4();
        MATH_ASSERT( fnorm > 0.99f && fnorm < 1.01f );
        t.rot.normalize4();
        return t;
    }

    /// t = this * r
    inline Transform<T> operator* (const Transform<T>&r) const {
        Transform<T> t;
        t.trans = operator*(r.trans);
        t.rot.x = rot.x*r.rot.x - rot.y*r.rot.y - rot.z*r.rot.z - rot.w*r.rot.w;
        t.rot.y = rot.x*r.rot.y + rot.y*r.rot.x + rot.z*r.rot.w - rot.w*r.rot.z;
        t.rot.z = rot.x*r.rot.z + rot.z*r.rot.x + rot.w*r.rot.y - rot.y*r.rot.w;
        t.rot.w = rot.x*r.rot.w + rot.w*r.rot.x + rot.y*r.rot.z - rot.z*r.rot.y;
        // normalize the transformation
        T fnorm = t.rot.lengthsqr4();
        MATH_ASSERT( fnorm > 0.99f && fnorm < 1.01f );
        t.rot.normalize4();
        return t;
    }

    inline Transform<T>& operator*= (const Transform<T>&right) {
        *this = operator*(right);
        return *this;
    }

    inline Transform<T> inverse() const {
        Transform<T> inv;
        inv.rot.x = rot.x;
        inv.rot.y = -rot.y;
        inv.rot.z = -rot.z;
        inv.rot.w = -rot.w;

        inv.trans = -inv.rotate(trans);
        return inv;
    }

    template <typename U> inline Transform<T>& operator= (const Transform<U>&r) {
        trans = r.trans;
        rot = r.rot;
        T fnorm = rot.lengthsqr4();
        MATH_ASSERT( fnorm > 0.99f && fnorm < 1.01f );
        return *this;
    }

    template <typename U> friend std::ostream& operator<<(std::ostream& O, const Transform<U>&v);
    template <typename U> friend std::istream& operator>>(std::istream& I, Transform<U>&v);

    Vector<T> rot, trans;     ///< rot is a quaternion=(cos(ang/2),axisx*sin(ang/2),axisy*sin(ang/2),axisz*sin(ang/2))
};

/** \brief Affine transformation parameterized with rotation matrices. Scales and shears are not supported.

    \ingroup affine_math
 */
template <typename T>
class TransformMatrix
{
public:
    inline TransformMatrix() {
        identity(); m[3] = m[7] = m[11] = 0;
    }
    template <typename U> TransformMatrix(const TransformMatrix<U>& t) {
        // don't memcpy!
        m[0] = T(t.m[0]); m[1] = T(t.m[1]); m[2] = T(t.m[2]); m[3] = T(t.m[3]);
        m[4] = T(t.m[4]); m[5] = T(t.m[5]); m[6] = T(t.m[6]); m[7] = T(t.m[7]);
        m[8] = T(t.m[8]); m[9] = T(t.m[9]); m[10] = T(t.m[10]); m[11] = T(t.m[11]);
        trans = t.trans;
    }
    inline TransformMatrix(const Transform<T>&t);

    inline void identity() {
        m[0] = 1; m[1] = 0; m[2] = 0;
        m[4] = 0; m[5] = 1; m[6] = 0;
        m[8] = 0; m[9] = 0; m[10] = 1;
        trans.x = trans.y = trans.z = 0;
    }

    inline void rotfrommat(T m_00, T m_01, T m_02, T m_10, T m_11, T m_12, T m_20, T m_21, T m_22) {
        m[0] = m_00; m[1] = m_01; m[2] = m_02; m[3] = 0;
        m[4] = m_10; m[5] = m_11; m[6] = m_12; m[7] = 0;
        m[8] = m_20; m[9] = m_21; m[10] = m_22; m[11] = 0;
    }

    inline T rot(int i, int j) const {
        MATH_ASSERT( i >= 0 && j >= 0 && i < 3 && j < 3);
        return m[4*i+j];
    }
    inline T& rot(int i, int j) {
        MATH_ASSERT( i >= 0 && j >= 0 && i < 3 && j < 3);
        return m[4*i+j];
    }

    template <typename U>
    inline Vector<T> operator* (const Vector<U>&r) const {
        Vector<T> v;
        v[0] = r[0] * m[0] + r[1] * m[1] + r[2] * m[2] + trans.x;
        v[1] = r[0] * m[4] + r[1] * m[5] + r[2] * m[6] + trans.y;
        v[2] = r[0] * m[8] + r[1] * m[9] + r[2] * m[10] + trans.z;
        return v;
    }

    /// t = this * r
    inline TransformMatrix<T> operator* (const TransformMatrix<T>&r) const {
        TransformMatrix<T> t;
        t.m[0*4+0] = m[0*4+0]*r.m[0*4+0]+m[0*4+1]*r.m[1*4+0]+m[0*4+2]*r.m[2*4+0];
        t.m[0*4+1] = m[0*4+0]*r.m[0*4+1]+m[0*4+1]*r.m[1*4+1]+m[0*4+2]*r.m[2*4+1];
        t.m[0*4+2] = m[0*4+0]*r.m[0*4+2]+m[0*4+1]*r.m[1*4+2]+m[0*4+2]*r.m[2*4+2];
        t.m[1*4+0] = m[1*4+0]*r.m[0*4+0]+m[1*4+1]*r.m[1*4+0]+m[1*4+2]*r.m[2*4+0];
        t.m[1*4+1] = m[1*4+0]*r.m[0*4+1]+m[1*4+1]*r.m[1*4+1]+m[1*4+2]*r.m[2*4+1];
        t.m[1*4+2] = m[1*4+0]*r.m[0*4+2]+m[1*4+1]*r.m[1*4+2]+m[1*4+2]*r.m[2*4+2];
        t.m[2*4+0] = m[2*4+0]*r.m[0*4+0]+m[2*4+1]*r.m[1*4+0]+m[2*4+2]*r.m[2*4+0];
        t.m[2*4+1] = m[2*4+0]*r.m[0*4+1]+m[2*4+1]*r.m[1*4+1]+m[2*4+2]*r.m[2*4+1];
        t.m[2*4+2] = m[2*4+0]*r.m[0*4+2]+m[2*4+1]*r.m[1*4+2]+m[2*4+2]*r.m[2*4+2];
        t.trans[0] = r.trans[0] * m[0] + r.trans[1] * m[1] + r.trans[2] * m[2] + trans.x;
        t.trans[1] = r.trans[0] * m[4] + r.trans[1] * m[5] + r.trans[2] * m[6] + trans.y;
        t.trans[2] = r.trans[0] * m[8] + r.trans[1] * m[9] + r.trans[2] * m[10] + trans.z;
        return t;
    }

    inline TransformMatrix<T> operator*= (const TransformMatrix<T>&r) const {
        *this = *this * r;
        return *this;
    }

    template <typename U>
    inline Vector<U> rotate(const Vector<U>& r) const {
        Vector<U> v;
        v.x = r.x * m[0] + r.y * m[1] + r.z * m[2];
        v.y = r.x * m[4] + r.y * m[5] + r.z * m[6];
        v.z = r.x * m[8] + r.y * m[9] + r.z * m[10];
        return v;
    }

    inline TransformMatrix<T> rotate(const TransformMatrix<T>& r) const {
        TransformMatrix<T> t;
        t.m[0*4+0] = m[0*4+0]*r.m[0*4+0]+m[0*4+1]*r.m[1*4+0]+m[0*4+2]*r.m[2*4+0];
        t.m[0*4+1] = m[0*4+0]*r.m[0*4+1]+m[0*4+1]*r.m[1*4+1]+m[0*4+2]*r.m[2*4+1];
        t.m[0*4+2] = m[0*4+0]*r.m[0*4+2]+m[0*4+1]*r.m[1*4+2]+m[0*4+2]*r.m[2*4+2];
        t.m[1*4+0] = m[1*4+0]*r.m[0*4+0]+m[1*4+1]*r.m[1*4+0]+m[1*4+2]*r.m[2*4+0];
        t.m[1*4+1] = m[1*4+0]*r.m[0*4+1]+m[1*4+1]*r.m[1*4+1]+m[1*4+2]*r.m[2*4+1];
        t.m[1*4+2] = m[1*4+0]*r.m[0*4+2]+m[1*4+1]*r.m[1*4+2]+m[1*4+2]*r.m[2*4+2];
        t.m[2*4+0] = m[2*4+0]*r.m[0*4+0]+m[2*4+1]*r.m[1*4+0]+m[2*4+2]*r.m[2*4+0];
        t.m[2*4+1] = m[2*4+0]*r.m[0*4+1]+m[2*4+1]*r.m[1*4+1]+m[2*4+2]*r.m[2*4+1];
        t.m[2*4+2] = m[2*4+0]*r.m[0*4+2]+m[2*4+1]*r.m[1*4+2]+m[2*4+2]*r.m[2*4+2];
        t.trans[0] = r.trans[0] * m[0] + r.trans[1] * m[1] + r.trans[2] * m[2];
        t.trans[1] = r.trans[0] * m[4] + r.trans[1] * m[5] + r.trans[2] * m[6];
        t.trans[2] = r.trans[0] * m[8] + r.trans[1] * m[9] + r.trans[2] * m[10];
        return t;
    }

    /// being on the safe side, do the full inverse incase someone uses scaling.
    inline TransformMatrix<T> inverse() const {
        // inverse = C^t / det(pf) where C is the matrix of coefficients
        // calc C^t
        TransformMatrix<T> inv;
        inv.m[0*4+0] = m[1*4 + 1] * m[2*4 + 2] - m[1*4 + 2] * m[2*4 + 1];
        inv.m[0*4+1] = m[0*4 + 2] * m[2*4 + 1] - m[0*4 + 1] * m[2*4 + 2];
        inv.m[0*4+2] = m[0*4 + 1] * m[1*4 + 2] - m[0*4 + 2] * m[1*4 + 1];
        inv.m[1*4+0] = m[1*4 + 2] * m[2*4 + 0] - m[1*4 + 0] * m[2*4 + 2];
        inv.m[1*4+1] = m[0*4 + 0] * m[2*4 + 2] - m[0*4 + 2] * m[2*4 + 0];
        inv.m[1*4+2] = m[0*4 + 2] * m[1*4 + 0] - m[0*4 + 0] * m[1*4 + 2];
        inv.m[2*4+0] = m[1*4 + 0] * m[2*4 + 1] - m[1*4 + 1] * m[2*4 + 0];
        inv.m[2*4+1] = m[0*4 + 1] * m[2*4 + 0] - m[0*4 + 0] * m[2*4 + 1];
        inv.m[2*4+2] = m[0*4 + 0] * m[1*4 + 1] - m[0*4 + 1] * m[1*4 + 0];
        T fdet = m[0*4 + 2] * inv.m[2*4+0] + m[1*4 + 2] * inv.m[2*4+1] + m[2*4 + 2] * inv.m[2*4+2];
        MATH_ASSERT(fdet>=0);
        fdet = 1 / fdet;
        inv.m[0*4+0] *= fdet;           inv.m[0*4+1] *= fdet;           inv.m[0*4+2] *= fdet;
        inv.m[1*4+0] *= fdet;           inv.m[1*4+1] *= fdet;           inv.m[1*4+2] *= fdet;
        inv.m[2*4+0] *= fdet;           inv.m[2*4+1] *= fdet;           inv.m[2*4+2] *= fdet;
        inv.trans = -inv.rotate(trans);
        return inv;
    }

    template <typename U>
    inline void Extract(Vector<U>& right, Vector<U>& up, Vector<U>& dir, Vector<U>& pos) const {
        pos = trans;
        right.x = m[0]; up.x = m[1]; dir.x = m[2];
        right.y = m[4]; up.y = m[5]; dir.y = m[6];
        right.z = m[8]; up.z = m[9]; dir.z = m[10];
    }

    template <typename U> friend std::ostream& operator<<(std::ostream& O, const TransformMatrix<U>&v);
    template <typename U> friend std::istream& operator>>(std::istream& I, TransformMatrix<U>&v);

    /// 3x3 rotation matrix. Note that each row is 4 elements long! So row 1 starts at m[4], row 2 at m[8]
    /// The reason is to maintain 16 byte alignment when sizeof(T) is 4 bytes
    T m[12];
    Vector<T> trans;     ///< translation component
};

/// \brief A ray defined by an origin and a direction.
/// \ingroup geometric_primitives
template <typename T>
class ray
{
public:
    ray() {
    }
    ray(const Vector<T>&_pos, const Vector<T>&_dir) : pos(_pos), dir(_dir) {
    }
    Vector<T> pos, dir;
};

/// \brief An axis aligned bounding box.
/// \ingroup geometric_primitives
template <typename T>
class aabb
{
public:
    aabb() {
    }
    aabb(const Vector<T>&vpos, const Vector<T>&vextents) : pos(vpos), extents(vextents) {
    }
    Vector<T> pos, extents;
};

/// \brief An oriented bounding box.
/// \ingroup geometric_primitives
template <typename T>
class OrientedBox
{
public:
    Transform<T> transform;
    Vector<T> extents;
};

/// \brief An oriented bounding box.
/// \ingroup geometric_primitives
template <typename T>
class obb
{
public:
    Vector<T> right, up, dir, pos, extents;
};

/// \brief A triangle defined by 3 points.
/// \ingroup geometric_primitives
template <typename T>
class triangle
{
public:
    triangle() {
    }
    triangle(const Vector<T>&v1, const Vector<T>&v2, const Vector<T>&v3) : v1(v1), v2(v2), v3(v3) {
    }
    ~triangle() {
    }

    Vector<T> v1, v2, v3;          //!< the vertices of the triangle

    const Vector<T>& operator[] (int i) const {
        return (&v1)[i];
    }
    Vector<T>& operator[] (int i)       {
        return (&v1)[i];
    }

    /// assumes CCW ordering of vertices
    inline Vector<T> normal() {
        return (v2-v1).cross(v3-v1);
    }
};

/// \brief A pyramid with its vertex clipped.
/// \ingroup geometric_primitives
template <typename T>
class frustum
{
public:
    Vector<T> right, up, dir, pos;
    T fnear, ffar;
    T ffovx,ffovy;
    T fcosfovx,fsinfovx,fcosfovy,fsinfovy;
};

/// \brief intrinsic parameters for a camera.
template <typename T>
class CameraIntrinsics
{
public:
    CameraIntrinsics() : fx(0),fy(0),cx(0),cy(0), focal_length(0.01) {
    }
    CameraIntrinsics(T fx, T fy, T cx, T cy) : fx(fx), fy(fy), cx(cx), cy(cy), focal_length(0.01) {
    }

    template <typename U>
    CameraIntrinsics<T>& operator=(const CameraIntrinsics<U>&r)
    {
        distortion_model = r.distortion_model;
        distortion_coeffs.resize(r.distortion_coeffs.size());
        std::copy(r.distortion_coeffs.begin(),r.distortion_coeffs.end(),distortion_coeffs.begin());
        focal_length = r.focal_length;
        fx = r.fx;
        fy = r.fy;
        cx = r.cx;
        cy = r.cy;
    }

    T fx,fy, cx,cy;

    /** \brief distortion model of the camera. if left empty, no distortion model is used.

        Possible values are:
        - "plumb_bob" - Brown. "Decentering Distortion of Lenses", Photometric Engineering, pages 444-462, Vol. 32, No. 3, 1966
     */
    std::string distortion_model;
    std::vector<T> distortion_coeffs;     ///< coefficients of the distortion model
    T focal_length;     ///< physical focal length distance since focal length cannot be recovered from the intrinsic matrix, but is necessary for determining the lens plane.
};

/// Don't add new lines to the output << operators. Some applications use it to serialize the data
/// to send across the network.
/// \name Primitive Serialization functions.
//@{
template <typename U>
std::ostream& operator<<(std::ostream& O, const Vector<U>&v)
{
    return O << v.x << " " << v.y << " " << v.z << " " << v.w << " ";
}

template <typename U>
std::istream& operator>>(std::istream& I, Vector<U>&v)
{
    return I >> v.x >> v.y >> v.z >> v.w;
}

template <typename U>
std::ostream& operator<<(std::ostream& O, const Transform<U>&v)
{
    return O << v.rot.x << " " << v.rot.y << " " << v.rot.z << " " << v.rot.w << " " << v.trans.x << " " << v.trans.y << " " << v.trans.z << " ";
}

template <typename U>
std::istream& operator>>(std::istream& I, Transform<U>&v)
{
    return I >> v.rot.x >> v.rot.y >> v.rot.z >> v.rot.w >> v.trans.x >> v.trans.y >> v.trans.z;
}

template <typename U>
std::ostream& operator<<(std::ostream& O, const ray<U>&r)
{
    return O << r.pos.x << " " << r.pos.y << " " << r.pos.z << " " << r.dir.x << " " << r.dir.y << " " << r.dir.z << " ";
}

template <typename U>
std::istream& operator>>(std::istream& I, ray<U>&r)
{
    return I >> r.pos.x >> r.pos.y >> r.pos.z >> r.dir.x >> r.dir.y >> r.dir.z;
}


/// \brief serialize in column order! This is the format transformations are passed across the network
template <typename U>
std::ostream& operator<<(std::ostream& O, const TransformMatrix<U>&v)
{
    return O << v.m[0] << " " << v.m[4] << " " << v.m[8] << " " << v.m[1] << " " << v.m[5] << " " << v.m[9] << " " << v.m[2] << " " << v.m[6] << " " << v.m[10] << " " << v.trans.x << " " << v.trans.y << " " << v.trans.z << " ";
}

/// \brief de-serialize in column order! This is the format transformations are passed across the network
template <typename U>
std::istream& operator>>(std::istream& I, TransformMatrix<U>&v)
{
    return I >> v.m[0] >> v.m[4] >> v.m[8] >> v.m[1] >> v.m[5] >> v.m[9] >> v.m[2] >> v.m[6] >> v.m[10] >> v.trans.x >> v.trans.y >> v.trans.z;
}

//@}

/// \brief Converts an axis-angle rotation into a quaternion.
///
/// \ingroup affine_math
/// \param axis unit axis, 3 values
/// \param angle rotation angle (radians)
template <typename T> inline Vector<T> quatFromAxisAngle(const Vector<T>& axis, T angle)
{
    T axislen = MATH_SQRT(axis.lengthsqr3());
    if( axislen == 0 ) {
        return Vector<T>(T(1),T(0),T(0),T(0));
    }
    angle *= T(0.5);
    T sang = MATH_SIN(angle)/axislen;
    return Vector<T>(MATH_COS(angle),axis.x*sang,axis.y*sang,axis.z*sang);
}

/// \brief Converts an axis-angle rotation into a quaternion.
///
/// \ingroup affine_math
/// \param axisangle unit axis * rotation angle (radians), 3 values
template <typename T> inline Vector<T> quatFromAxisAngle(const Vector<T>& axisangle)
{
    T axislen = MATH_SQRT(axisangle.lengthsqr3());
    if( axislen == 0 ) {
        return Vector<T>(T(1),T(0),T(0),T(0));
    }
    T sang = MATH_SIN(axislen*T(0.5))/axislen;
    return Vector<T>(MATH_COS(axislen*T(0.5)),axisangle.x*sang,axisangle.y*sang,axisangle.z*sang);
}

/// \brief Converts the rotation of a matrix into a quaternion.
///
/// \ingroup affine_math
/// \param t transform for extracting the 3x3 rotation.
template <typename T> inline Vector<T> quatFromMatrix(const TransformMatrix<T>& rotation)
{
    Vector<T> rot;
    T tr = rotation.m[4*0+0] + rotation.m[4*1+1] + rotation.m[4*2+2];
    if (tr >= 0) {
        rot[0] = tr + 1;
        rot[1] = (rotation.m[4*2+1] - rotation.m[4*1+2]);
        rot[2] = (rotation.m[4*0+2] - rotation.m[4*2+0]);
        rot[3] = (rotation.m[4*1+0] - rotation.m[4*0+1]);
    }
    else {
        // find the largest diagonal element and jump to the appropriate case
        if (rotation.m[4*1+1] > rotation.m[4*0+0]) {
            if (rotation.m[4*2+2] > rotation.m[4*1+1]) {
                rot[3] = (rotation.m[4*2+2] - (rotation.m[4*0+0] + rotation.m[4*1+1])) + 1;
                rot[1] = (rotation.m[4*2+0] + rotation.m[4*0+2]);
                rot[2] = (rotation.m[4*1+2] + rotation.m[4*2+1]);
                rot[0] = (rotation.m[4*1+0] - rotation.m[4*0+1]);
            }
            else {
                rot[2] = (rotation.m[4*1+1] - (rotation.m[4*2+2] + rotation.m[4*0+0])) + 1;
                rot[3] = (rotation.m[4*1+2] + rotation.m[4*2+1]);
                rot[1] = (rotation.m[4*0+1] + rotation.m[4*1+0]);
                rot[0] = (rotation.m[4*0+2] - rotation.m[4*2+0]);
            }
        }
        else if (rotation.m[4*2+2] > rotation.m[4*0+0]) {
            rot[3] = (rotation.m[4*2+2] - (rotation.m[4*0+0] + rotation.m[4*1+1])) + 1;
            rot[1] = (rotation.m[4*2+0] + rotation.m[4*0+2]);
            rot[2] = (rotation.m[4*1+2] + rotation.m[4*2+1]);
            rot[0] = (rotation.m[4*1+0] - rotation.m[4*0+1]);
        }
        else {
            rot[1] = (rotation.m[4*0+0] - (rotation.m[4*1+1] + rotation.m[4*2+2])) + 1;
            rot[2] = (rotation.m[4*0+1] + rotation.m[4*1+0]);
            rot[3] = (rotation.m[4*2+0] + rotation.m[4*0+2]);
            rot[0] = (rotation.m[4*2+1] - rotation.m[4*1+2]);
        }
    }
    rot.normalize4();
    return rot;
}

/// \brief Converts a quaternion to a 3x3 matrix
///
/// \ingroup affine_math
/// \param[in] quat quaternion, (s,vx,vy,vz)
template <typename T> inline TransformMatrix<T> matrixFromQuat(const Vector<T>& quat)
{
    // should normalize the quaternion first
    T length2 = quat.lengthsqr4();
    MATH_ASSERT(length2 > 0.99 && length2 < 1.01); // make sure it is at least close
    T ilength2 = 2/length2;
    TransformMatrix<T> t;
    T qq1 = ilength2*quat[1]*quat[1];
    T qq2 = ilength2*quat[2]*quat[2];
    T qq3 = ilength2*quat[3]*quat[3];
    t.m[4*0+0] = 1 - qq2 - qq3;
    t.m[4*0+1] = ilength2*(quat[1]*quat[2] - quat[0]*quat[3]);
    t.m[4*0+2] = ilength2*(quat[1]*quat[3] + quat[0]*quat[2]);
    t.m[4*0+3] = 0;
    t.m[4*1+0] = ilength2*(quat[1]*quat[2] + quat[0]*quat[3]);
    t.m[4*1+1] = 1 - qq1 - qq3;
    t.m[4*1+2] = ilength2*(quat[2]*quat[3] - quat[0]*quat[1]);
    t.m[4*1+3] = 0;
    t.m[4*2+0] = ilength2*(quat[1]*quat[3] - quat[0]*quat[2]);
    t.m[4*2+1] = ilength2*(quat[2]*quat[3] + quat[0]*quat[1]);
    t.m[4*2+2] = 1 - qq1 - qq2;
    t.m[4*2+3] = 0;
    return t;
}

/// \brief Converts a quaternion to a 3x3 matrix
///
/// \ingroup affine_math
/// \param[out] rotation
/// \param[in] quat quaternion, (s,vx,vy,vz)
template <typename T> void matrixFromQuat(TransformMatrix<T>& rotation, const Vector<T>& quat)
{
    // should normalize the quaternion first
    T length2 = quat.lengthsqr4();
    MATH_ASSERT(length2 > 0.99 && length2 < 1.01); // make sure it is at least close
    T ilength2 = 2/length2;
    T qq1 = ilength2*quat[1]*quat[1];
    T qq2 = ilength2*quat[2]*quat[2];
    T qq3 = ilength2*quat[3]*quat[3];
    rotation.m[4*0+0] = 1 - qq2 - qq3;
    rotation.m[4*0+1] = ilength2*(quat[1]*quat[2] - quat[0]*quat[3]);
    rotation.m[4*0+2] = ilength2*(quat[1]*quat[3] + quat[0]*quat[2]);
    rotation.m[4*0+3] = 0;
    rotation.m[4*1+0] = ilength2*(quat[1]*quat[2] + quat[0]*quat[3]);
    rotation.m[4*1+1] = 1 - qq1 - qq3;
    rotation.m[4*1+2] = ilength2*(quat[2]*quat[3] - quat[0]*quat[1]);
    rotation.m[4*1+3] = 0;
    rotation.m[4*2+0] = ilength2*(quat[1]*quat[3] - quat[0]*quat[2]);
    rotation.m[4*2+1] = ilength2*(quat[2]*quat[3] + quat[0]*quat[1]);
    rotation.m[4*2+2] = 1 - qq1 - qq2;
    rotation.m[4*2+3] = 0;
}

/// \brief Converts an axis-angle rotation to a 3x3 matrix.
///
/// \ingroup affine_math
/// \param axis unit axis, 3 values
/// \param angle rotation angle (radians)
template <typename T> inline TransformMatrix<T> matrixFromAxisAngle(const Vector<T>& axis, T angle)
{
    return matrixFromQuat(quatFromAxisAngle(axis,angle));
}

/// \brief Converts an axis-angle rotation to a 3x3 matrix.
///
/// \ingroup affine_math
/// \param axis unit axis * rotation angle (radians), 3 values
template <typename T> inline TransformMatrix<T> matrixFromAxisAngle(const Vector<T>& axisangle)
{
    return matrixFromQuat(quatFromAxisAngle(axisangle));
}

/// \brief Multiply two quaternions
///
/// \ingroup affine_math
/// \param quat0 quaternion, (s,vx,vy,vz)
/// \param quat1 quaternion, (s,vx,vy,vz)
template <typename T>
inline Vector<T> quatMultiply(const Vector<T>& quat0, const Vector<T>& quat1)
{
    Vector<T> q(quat0.x*quat1.x - quat0.y*quat1.y - quat0.z*quat1.z - quat0.w*quat1.w,
                    quat0.x*quat1.y + quat0.y*quat1.x + quat0.z*quat1.w - quat0.w*quat1.z,
                    quat0.x*quat1.z + quat0.z*quat1.x + quat0.w*quat1.y - quat0.y*quat1.w,
                    quat0.x*quat1.w + quat0.w*quat1.x + quat0.y*quat1.z - quat0.z*quat1.y);
    // do not normalize since some quaternion math (like derivatives) do not correspond to unit quaternions
    return q;
}

/// \brief Inverted a quaternion rotation.
///
/// \ingroup affine_math
/// \param quat quaternion, (s,vx,vy,vz)
template <typename T>
inline Vector<T> quatInverse(const Vector<T>& quat)
{
    return Vector<T>(quat.x,-quat.y,-quat.z,-quat.w);
}

/// \brief Sphereical linear interpolation between two quaternions.
///
/// \ingroup affine_math
/// \param quat0 quaternion, (s,vx,vy,vz)
/// \param quat1 quaternion, (s,vx,vy,vz)
/// \param t real value in [0,1]. 0 returns quat1, 1 returns quat2
template <typename T>
inline Vector<T> quatSlerp(const Vector<T>& quat0, const Vector<T>& quat1, T t)
{
    // quaternion to return
    Vector<T> qb, qm;
    if( quat0.dot(quat1) < 0 ) {
        qb = -quat1;
    }
    else {
        qb = quat1;
    }
    // Calculate angle between them.
    T cosHalfTheta = quat0.w * qb.w + quat0.x * qb.x + quat0.y * qb.y + quat0.z * qb.z;
    // if quat0=qb or quat0=-qb then theta = 0 and we can return quat0
    if (MATH_FABS(cosHalfTheta) >= 1.0) {
        qm.w = quat0.w; qm.x = quat0.x; qm.y = quat0.y; qm.z = quat0.z;
        return qm;
    }
    // Calculate temporary values.
    T halfTheta = MATH_ACOS(cosHalfTheta);
    T sinHalfTheta = MATH_SQRT(1 - cosHalfTheta*cosHalfTheta);
    // if theta = 180 degrees then result is not fully defined
    // we could rotate around any axis normal to quat0 or qb
    if (MATH_FABS(sinHalfTheta) < 1e-7f) { // fabs is floating point absolute
        qm.w = (quat0.w * 0.5f + qb.w * 0.5f);
        qm.x = (quat0.x * 0.5f + qb.x * 0.5f);
        qm.y = (quat0.y * 0.5f + qb.y * 0.5f);
        qm.z = (quat0.z * 0.5f + qb.z * 0.5f);
        return qm;
    }
    T ratioA = MATH_SIN((1 - t) * halfTheta) / sinHalfTheta;
    T ratioB = MATH_SIN(t * halfTheta) / sinHalfTheta;
    //calculate Quaternion.
    qm.w = (quat0.w * ratioA + qb.w * ratioB);
    qm.x = (quat0.x * ratioA + qb.x * ratioB);
    qm.y = (quat0.y * ratioA + qb.y * ratioB);
    qm.z = (quat0.z * ratioA + qb.z * ratioB);
    return qm;
}

template <typename T>
inline Vector<T> dQSlerp(const Vector<T>& qa, const Vector<T>& _qb, T t)
{
    return quatSlerp<T>(qa,_qb,t);
}

/// \brief transform a vector by a quaternion
///
/// \ingroup affine_math
/// \param
template <typename T>
Vector<T> quatRotate(const Vector<T>& q, const Vector<T>& t)
{
    T xx = 2 * q.y * q.y;
    T xy = 2 * q.y * q.z;
    T xz = 2 * q.y * q.w;
    T xw = 2 * q.y * q.x;
    T yy = 2 * q.z * q.z;
    T yz = 2 * q.z * q.w;
    T yw = 2 * q.z * q.x;
    T zz = 2 * q.w * q.w;
    T zw = 2 * q.w * q.x;
    Vector<T> v;
    v.x = (1-yy-zz) * t.x + (xy-zw) * t.y + (xz+yw)*t.z;
    v.y = (xy+zw) * t.x + (1-xx-zz) * t.y + (yz-xw)*t.z;
    v.z = (xz-yw) * t.x + (yz+xw) * t.y + (1-xx-yy)*t.z;
    return v;
}

/// \brief extracts the x, y, or z axes (column vectors of the rotation matrix) from a quaterion
///
/// \param axis the axis to extract. 0 for x, 1 for y, and 2 for z.
/// \ingroup affine_math
/// \param
template <typename T>
Vector<T> ExtractAxisFromQuat(const Vector<T>& q, int axis)
{
    if( axis == 0 ) {
        return Vector<T>(1 - 2 * (q.z * q.z + q.w * q.w), 2 * (q.y * q.z + q.w * q.x), 2 * (q.y * q.w - q.z * q.x));
    }
    else if( axis == 1 ) {
        return Vector<T>(2 * (q.y * q.z - q.w * q.x), 1 - 2 * (q.y * q.y + q.w * q.w), 2 * (q.z * q.w + q.y * q.x));
    }
    else if( axis == 2 ) {
        return Vector<T>(2 * (q.y * q.w + q.z * q.x), 2 * (q.z * q.w - q.y * q.x), 1 - 2 * (q.y * q.y + q.z * q.z));
    }
    else {
        MATH_ASSERT(0);
    }
    return Vector<T>();
}

/// \brief Return the minimal quaternion that orients sourcedir to targetdir
///
/// \ingroup affine_math
/// \param sourcedir direction of the original vector, 3 values
/// \param targetdir new direction, 3 values
template<typename T>
Vector<T> quatRotateDirection(const Vector<T>& sourcedir, const Vector<T>& targetdir)
{
    Vector<T> rottodirection = sourcedir.cross(targetdir);
    T fsin = MATH_SQRT(rottodirection.lengthsqr3());
    T fcos = sourcedir.dot3(targetdir);
    Transform<T> torient;
    if( fsin > 0 ) {
        return quatFromAxisAngle(rottodirection*(1/fsin), MATH_ATAN2(fsin, fcos));
    }
    if( fcos < 0 ) {
        // hand is flipped 180, rotate around x axis
        rottodirection = Vector<T>(1,0,0);
        rottodirection -= sourcedir * sourcedir.dot3(rottodirection);
        if( rottodirection.lengthsqr3()<1e-8 ) {
            rottodirection = Vector<T>(0,0,1);
            rottodirection -= sourcedir * sourcedir.dot3(rottodirection);
        }
        rottodirection.normalize3();
        return quatFromAxisAngle(rottodirection, MATH_ATAN2(fsin, fcos));
    }
    return Vector<T>(T(1),T(0),T(0),T(0));
}

/// \brief Find the rotation theta around axis such that rot(axis,theta) * q is closest to the identity rotation
///
/// \ingroup affine_math
/// \param[in] axis axis to minimize rotation about
/// \param[in] quat input
/// \return The angle that minimizes the rotation along with the normalized rotation rot(axis,theta)*q
template<typename T>
std::pair<T, Vector<T> > normalizeAxisRotation(const Vector<T>& axis, const Vector<T>& quat)
{
    T axislen = MATH_SQRT(axis.lengthsqr3());
    T angle = MATH_ATAN2(-quat.w*axis.z-quat.z*axis.y-quat.y*axis.x,quat.x*axislen);
    T sinangle2 = MATH_SIN(angle)/axislen, cosangle2 = MATH_COS(angle);
    Vector<T> normalizingquat = Vector<T>(cosangle2,axis.x*sinangle2,axis.y*sinangle2,axis.z*sinangle2);
    return std::make_pair(2*angle,quatMultiply(normalizingquat,quat));
}

/// \brief Converts a quaternion into the axis-angle representation.
///
/// \ingroup affine_math
/// \param quat quaternion, (s,vx,vy,vz)
template<typename T>
Vector<T> axisAngleFromQuat(const Vector<T>& quat)
{
    T sinang = quat.y*quat.y+quat.z*quat.z+quat.w*quat.w;
    if( sinang == 0 ) {
        return Vector<T>(0,0,0);
    }
    Vector<T> _quat;
    if( quat.x < 0 ) {
        _quat = -quat;
    }
    else {
        _quat = quat;
    }
    sinang = MATH_SQRT(sinang);
    T f = 2.0*MATH_ATAN2(sinang,_quat.x)/sinang;
    return Vector<T>(_quat.y*f,_quat.z*f,_quat.w*f);
}

/// \brief Converts the rotation of a matrix into axis-angle representation.
///
/// \ingroup affine_math
/// \param rotation 3x3 rotation matrix
template<typename T>
Vector<T> axisAngleFromMatrix(const TransformMatrix<T>& rotation)
{
    return axisAngleFromQuat(quatFromMatrix(rotation));
}

template <typename T>
Transform<T>::Transform(const TransformMatrix<T>& t)
{
    trans = t.trans;
    rot = quatFromMatrix(t);
}

template <typename T>
TransformMatrix<T>::TransformMatrix(const Transform<T>& t)
{
    matrixFromQuat(*this, t.rot);
    trans = t.trans;

}

/// \brief Returns a camera matrix that looks along a ray with a desired up vector.
///
/// \ingroup affine_math
/// \param[in] vlookat the point space to look at, the camera will rotation and zoom around this point
/// \param[in] vcampos the position of the camera in space
/// \param[in] vcamup vector from the camera
template<typename T>
TransformMatrix<T> transformLookat(const Vector<T>& vlookat, const Vector<T>& vcamerapos, const Vector<T>& vcameraup)
{
    Vector<T> dir = vlookat - vcamerapos;
    T len = MATH_SQRT(dir.lengthsqr3());
    if( len > 1e-6 ) {
        dir *= 1/len;
    }
    else {
        dir = Vector<T>(0,0,1);
    }
    Vector<T> up = vcameraup - dir * dir.dot3(vcameraup);
    len = up.lengthsqr3();
    if( len < 1e-8 ) {
        up = Vector<T>(0,1,0);
        up -= dir * dir.dot3(up);
        len = up.lengthsqr3();
        if( len < 1e-8 ) {
            up = Vector<T>(1,0,0);
            up -= dir * dir.dot3(up);
            len = up.lengthsqr3();
        }
    }
    up *= 1/MATH_SQRT(len);
    Vector<T> right = up.cross(dir);
    TransformMatrix<T> t;
    t.m[0] = right.x; t.m[1] = up.x; t.m[2] = dir.x;
    t.m[4] = right.y; t.m[5] = up.y; t.m[6] = dir.y;
    t.m[8] = right.z; t.m[9] = up.z; t.m[10] = dir.z;
    t.trans = vcamerapos;
    return t;
}

/// \brief Tests a point inside a 3D quadrilateral.
/// \ingroup geometric_primitives
template <typename T>
inline int insideQuadrilateral(const Vector<T>& v, const Vector<T>& verts)
{
    Vector<T> v4,v5;
    T m1,m2;
    T anglesum=0,costheta;
    for (int i=0; i<4; i++) {
        v4.x = verts[i].x - v->x;
        v4.y = verts[i].y - v->y;
        v4.z = verts[i].z - v->z;
        v5.x = verts[(i+1)%4].x - v->x;
        v5.y = verts[(i+1)%4].y - v->y;
        v5.z = verts[(i+1)%4].z - v->z;
        m1 = v4.lengthsqr3();
        m2 = v5.lengthsqr3();
        if (m1*m2 <= std::numeric_limits<T>::epsilon()*std::numeric_limits<T>::epsilon()) {
            return(1); // on a vertex, consider this inside
        }
        else {
            costheta = v4.dot3(v5)/MATH_SQRT(m1*m2);
        }
        anglesum += MATH_ACOS(costheta);
    }
    T diff = anglesum - (T)2.0 * M_PI;
    return Fabs(diff) <= std::numeric_limits<T>::epsilon();
}

/// \brief Tests a point insdie a 3D triangle.
/// \ingroup geometric_primitives
template <typename T>
inline int insideTriangle(const Vector<T> v, const triangle<T>& tri)
{
    Vector<T> v4,v5;
    T m1,m2;
    T anglesum=0.0;
    T costheta;
    for (int i=0; i<3; i++) {
        v4.x = tri[i].x - v->x;
        v4.y = tri[i].y - v->y;
        v4.z = tri[i].z - v->z;
        v5.x = tri[(i+1)%3].x - v->x;
        v5.y = tri[(i+1)%3].y - v->y;
        v5.z = tri[(i+1)%3].z - v->z;
        m1 = v4.lengthsqr3();
        m2 = v5.lengthsqr3();
        if (m1*m2 <= std::numeric_limits<T>::epsilon()*std::numeric_limits<T>::epsilon()) {
            return(1); // on a vertex, consider this inside
        }
        else {
            costheta = v4.dot3(v5)/MATH_SQRT(m1*m2);
        }
        anglesum += acos(costheta);
    }
    T diff = anglesum - (T)2.0 * M_PI;
    return Fabs(diff) <= std::numeric_limits<T>::epsilon();
}

/// \brief Test collision of a ray with an axis aligned bounding box.
/// \ingroup geometric_primitives
template <typename T>
inline bool RayAABBTest(const ray<T>& r, const aabb<T>& ab)
{
    Vector<T> vd, vpos = r.pos - ab.pos;
    if((MATH_FABS(vpos.x) > ab.extents.x)&&(r.dir.x* vpos.x > 0.0f))
        return false;
    if((MATH_FABS(vpos.y) > ab.extents.y)&&(r.dir.y * vpos.y > 0.0f))
        return false;
    if((MATH_FABS(vpos.z) > ab.extents.z)&&(r.dir.z * vpos.z > 0.0f))
        return false;
    vd = r.dir.cross(vpos);
    if( MATH_FABS(vd.x) > ab.extents.y * MATH_FABS(r.dir.z) + ab.extents.z * MATH_FABS(r.dir.y) )
        return false;
    if( MATH_FABS(vd.y) > ab.extents.x * MATH_FABS(r.dir.z) + ab.extents.z * MATH_FABS(r.dir.x) )
        return false;
    if( MATH_FABS(vd.z) > ab.extents.x * MATH_FABS(r.dir.y) + ab.extents.y * MATH_FABS(r.dir.x) )
        return false;
    return true;
}

/// \brief Test collision of a ray and an oriented bounding box.
/// \ingroup geometric_primitives
template <typename T>
inline bool RayOBBTest(const ray<T>& r, const obb<T>& o)
{
    Vector<T> vpos, vdir, vd;
    vd = r.pos - o.pos;
    vpos.x = vd.dot3(o.right);
    vdir.x = r.dir.dot3(o.right);
    if((MATH_FABS(vpos.x) > o.extents.x)&&(vdir.x* vpos.x > 0.0f)) {
        return false;
    }
    vpos.y = vd.dot3(o.up);
    vdir.y = r.dir.dot3(o.up);
    if((MATH_FABS(vpos.y) > o.extents.y)&&(vdir.y * vpos.y > 0.0f)) {
        return false;
    }
    vpos.z = vd.dot3(o.dir);
    vdir.z = r.dir.dot3(o.dir);
    if((MATH_FABS(vpos.z) > o.extents.z)&&(vdir.z * vpos.z > 0.0f)) {
        return false;
    }
    cross3(vd, vdir, vpos);
    if((MATH_FABS(vd.x) > o.extents.y * MATH_FABS(vdir.z) + o.extents.z * MATH_FABS(vdir.y))||
       ( MATH_FABS(vd.y) > o.extents.x * MATH_FABS(vdir.z) + o.extents.z * MATH_FABS(vdir.x)) ||
       ( MATH_FABS(vd.z) > o.extents.x * MATH_FABS(vdir.y) + o.extents.y * MATH_FABS(vdir.x)) ) {
        return false;
    }
    return true;
}

/// \brief Test collision of an oriented bounding box and a frustum.
/// \ingroup geometric_primitives
template <typename T>
inline bool IsOBBinFrustum(const obb<T>& o, const frustum<T>& fr)
{
    // check OBB against all 6 planes
    Vector<T> v = o.pos - fr.pos;
    // if v lies on the left or bottom sides of the frustrum
    // then freflect about the planes to get it on the right and
    // top sides
    // side planes
    Vector<T> vNorm = fr.fcosfovx * fr.right - fr.fsinfovx * fr.dir;
    if( v.dot3(vNorm) > -o.extents.x * MATH_FABS(vNorm.dot3(o.right)) -  o.extents.y * MATH_FABS(vNorm.dot3(o.up)) -  o.extents.z * MATH_FABS(vNorm.dot3(o.dir))) {
        return false;
    }
    vNorm = -fr.fcosfovx * fr.right - fr.fsinfovx * fr.dir;
    if(dot3(v, vNorm) > -o.extents.x * MATH_FABS(vNorm.dot3(o.right)) - o.extents.y * MATH_FABS(vNorm.dot3(o.up)) - o.extents.z * MATH_FABS(vNorm.dot3(o.dir))) {
        return false;
    }
    vNorm = fr.fcosfovy * fr.up - fr.fsinfovy * fr.dir;
    if(dot3(v, vNorm) > -o.extents.x * MATH_FABS(vNorm.dot3(o.right)) - o.extents.y * MATH_FABS(vNorm.dot3(o.up)) - o.extents.z * MATH_FABS(vNorm.dot3(o.dir))) {
        return false;
    }
    vNorm = -fr.fcosfovy * fr.up - fr.fsinfovy * fr.dir;
    if(dot3(v, vNorm) > -o.extents.x * MATH_FABS(vNorm.dot3(o.right)) - o.extents.y * MATH_FABS(vNorm.dot3(o.up)) - o.extents.z * MATH_FABS(vNorm.dot3(o.dir))) {
        return false;
    }
    vNorm.x = v.dot3(fr.dir);
    vNorm.y = o.extents.x * MATH_FABS(fr.dir.dot3(o.right)) +  o.extents.y * MATH_FABS(fr.dir.dot3(o.up)) +  o.extents.z * MATH_FABS(fr.dir.dot3(o.dir));
    if( (vNorm.x < fr.fnear + vNorm.y) || (vNorm.x > fr.ffar - vNorm.y) ) {
        return false;
    }
    return true;
}

/// \brief Tests if an oriented bounding box is inside a 3D convex hull.
///
/// \ingroup geometric_primitives
/// \param vplanes the plane normals of the convex hull, normals should be facing inside.
template <typename T, typename U>
inline bool IsOBBinConvexHull(const obb<T>& o, const U& vplanes)
{
    for(size_t i = 0; i < vplanes.size(); ++i) {
        Vector<T> vplane = vplanes[i];
        if( o.pos.dot3(vplane)+vplane.w < o.extents.x * MATH_FABS(vplane.dot3(o.right)) + o.extents.y * MATH_FABS(vplane.dot3(o.up)) + o.extents.z * MATH_FABS(vplane.dot3(o.dir))) {
            return false;
        }
    }
    return true;
}

/// \brief Test collision if two 3D triangles.
/// \ingroup geometric_primitives
///
/// Assuming triangle vertices are declared counter-clockwise!!
/// \param[out] contactnorm if triangles collide, then filled with the normal of the second triangle
/// \return true if triangles collide.
template <typename T>
inline bool TriTriCollision(const Vector<T>& u1, const Vector<T>& u2, const Vector<T>& u3, const Vector<T>& v1, const Vector<T>& v2, const Vector<T>& v3, Vector<T>& contactpos, Vector<T>& contactnorm)
{
    // triangle triangle collision test - by Rosen Diankov

    // first see if the faces intersect the planes
    // for the face to be intersecting the plane, one of its
    // vertices must be on the opposite side of the plane
    char b = 0;
    Vector<T> u12 = u2 - u1,                u31 = u1 - u3;
    Vector<T> v12 = v2 - v1, v23 = v3 - v2, v31 = v1 - v3;
    Vector<T> vedges[3] = { v12, v23, v31};
    Vector<T> unorm, vnorm;
    unorm = u31.cross(u12);
    unorm.w = -unorm.dot3(u1);
    vnorm = v31.cross(v12);
    vnorm.w = -vnorm.dot3(v1);
    if( vnorm.dot3(u1) + vnorm.w > 0 ) {
        b |= 1;
    }
    if( vnorm.dot3(u2) + vnorm.w > 0 ) {
        b |= 2;
    }
    if( vnorm.dot3(u3) + vnorm.w > 0 ) {
        b |= 4;
    }
    if((b == 7)||(b == 0)) {
        return false;
    }
    // now get segment from f1 when it crosses f2's plane
    // note that b gives us information on which edges actually intersected
    // so figure out the point that is alone on one side of the plane
    // then get the segment
    Vector<T> p1, p2;
    const Vector<T>* pu=NULL;
    switch(b) {
    case 1:
    case 6:
        pu = &u1;
        p1 = u2 - u1;
        p2 = u3 - u1;
        break;
    case 2:
    case 5:
        pu = &u2;
        p1 = u1 - u2;
        p2 = u3 - u2;
        break;
    case 4:
    case 3:
        pu = &u3;
        p1 = u1 - u3;
        p2 = u2 - u3;
        break;
    }

    T t = vnorm.dot3(*pu)+vnorm.w;
    p1 = *pu - p1 * (t / vnorm.dot3(p1));
    p2 = *pu - p2 * (t / vnorm.dot3(p2));

    // go through each of the segments in v2 and clip
    Vector<T> vcross;
    const Vector<T>* pv[] = { &v1, &v2, &v3, &v1};

    for(int i = 0; i < 3; ++i) {
        const Vector<T>* pprev = pv[i];
        Vector<T> q1 = p1 - *pprev;
        Vector<T> q2 = p2 - *pprev;
        vcross = vedges[i].cross(vnorm);
        T t1 = q1.dot3(vcross);
        T t2 = q2.dot3(vcross);

        // line segment is out of face
        if((t1 >= 0)&&(t2 >= 0)) {
            return false;
        }
        if((t1 > 0)&&(t2 < 0)) {
            // keep second point, clip first
            Vector<T> dq = q2-q1;
            p1 -= dq*(t1/dq.dot3(vcross));
        }
        else if((t1 < 0)&&(t2 > 0)) {
            // keep first point, clip second
            Vector<T> dq = q1-q2;
            p2 -= dq*(t2/dq.dot3(vcross));
        }
    }

    contactpos = 0.5f * (p1 + p2);
    contactnorm = vnorm.normalize3();
    return true;
}

/// \brief Transform an axis aligned bounding box to an oriented bounding box.
///
/// \ingroup geometric_primitives
/// \param[in] t transformation used to set the coordinate system of ab.
template <typename T>
inline obb<T> OBBFromAABB(const aabb<T>& ab, const TransformMatrix<T>& t)
{
    obb<T> o;
    o.right = Vector<T>(t.m[0],t.m[4],t.m[8]);
    o.up = Vector<T>(t.m[1],t.m[5],t.m[9]);
    o.dir = Vector<T>(t.m[2],t.m[6],t.m[10]);
    o.pos = t*ab.pos;
    o.extents = ab.extents;
    return o;
}

/// \brief Transform an axis aligned bounding box to an oriented bounding box.
///
/// \ingroup geometric_primitives
/// \param[in] t transformation used to set the coordinate system of ab.
template <typename T>
inline obb<T> OBBFromAABB(const aabb<T>& ab, const Transform<T>& t)
{
    return OBBFromAABB(ab,TransformMatrix<T>(t));
}

/// \brief Transforms an oriented bounding box.
///
/// \ingroup geometric_primitives
/// \param[in] t transformation used to set the coordinate system of o.
template <typename T>
inline obb<T> TransformOBB(const Transform<T>& t, const obb<T>& o)
{
    obb<T> newobb;
    newobb.extents = o.extents;
    newobb.pos = t*o.pos;
    newobb.right = t.rotate(o.right);
    newobb.up = t.rotate(o.up);
    newobb.dir = t.rotate(o.dir);
    return newobb;
}

/// \brief Transforms an oriented bounding box.
///
/// \ingroup geometric_primitives
/// \param[in] t transformation used to set the coordinate system of o.
template <typename T>
inline obb<T> TransformOBB(const TransformMatrix<T>& t, const obb<T>& o)
{
    obb<T> newobb;
    newobb.extents = o.extents;
    newobb.pos = t*o.pos;
    newobb.right = t.rotate(o.right);
    newobb.up = t.rotate(o.up);
    newobb.dir = t.rotate(o.dir);
    return newobb;
}

/// \brief Test collision between two axis-aligned bounding boxes.
///
/// \ingroup geometric_primitives
template <typename T>
inline bool AABBCollision(const aabb<T>& ab1, const aabb<T>& ab2)
{
    Vector<T> v = ab1.pos-ab2.pos;
    return MATH_FABS(v.x) <= ab1.extents.x+ab2.extents.x && MATH_FABS(v.y) <= ab1.extents.y+ab2.extents.y && MATH_FABS(v.z) <= ab1.extents.z+ab2.extents.z;
}

/// \brief The minimum distance form the vertex to the oriented bounding box.
/// \ingroup geometric_primitives
template <typename T>
T DistVertexOBBSq(const Vector<T>& v, const obb<T>& o)
{
    Vector<T> vn = v - o.pos;
    vn.x = MATH_FABS(vn.dot3(o.right)) - o.extents.x;
    vn.y = MATH_FABS(vn.dot3(o.up)) - o.extents.y;
    vn.z = MATH_FABS(vn.dot3(o.dir)) - o.extents.z;
    // now we have the vertex in OBB's frame
    T fDist = 0;
    if( vn.x > 0.0f ) {
        fDist += vn.x * vn.x;
    }
    if( vn.y > 0.0f ) {
        fDist += vn.y * vn.y;
    }
    if( vn.z > 0.0f ) {
        fDist += vn.z * vn.z;
    }
    return fDist;
}
} // end namespace geometry

#endif

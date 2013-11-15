#ifndef VEXCL_TYPES_HPP
#define VEXCL_TYPES_HPP

/*
The MIT License

Copyright (c) 2012-2013 Denis Demidov <ddemidov@ksu.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   vexcl/types.hpp
 * \author Pascal Germroth <pascal@ensieve.org>
 * \brief  Support for using native C++ and OpenCL types in expressions.
 */

#include <string>
#include <type_traits>
#include <stdexcept>
#include <iostream>
#include <sstream>

#ifdef VEXCL_BACKEND_OPENCL
#  ifndef __CL_ENABLE_EXCEPTIONS
#    define __CL_ENABLE_EXCEPTIONS
#  endif
#  include <CL/cl.hpp>
#else
#  include <CL/cl_platform.h>
#endif

/// \cond INTERNAL
typedef unsigned int  uint;
typedef unsigned char uchar;

namespace vex {

    /// Get the corresponding scalar type for a CL vector (or scalar) type.
    /** \code cl_scalar_of<cl_float4>::type == cl_float \endcode */
    template <class T>
    struct cl_scalar_of {};

    /// Get the corresponding vector type for a CL scalar type.
    /** \code cl_vector_of<cl_float, 4>::type == cl_float4 \endcode */
    template <class T, int dim>
    struct cl_vector_of {};

    /// Get the number of values in a CL vector (or scalar) type.
    /** \code cl_vector_length<cl_float4>::value == 4 \endcode */
    template <class T>
    struct cl_vector_length {};

} // namespace vex

#define BIN_OP(name, len, op)                                                  \
  inline cl_##name##len &operator op## =(cl_##name##len & a,                   \
                                         const cl_##name##len & b) {           \
    for (size_t i = 0; i < len; i++)                                           \
      a.s[i] op## = b.s[i];                                                    \
    return a;                                                                  \
  }                                                                            \
  inline cl_##name##len operator op(const cl_##name##len & a,                  \
                                    const cl_##name##len & b) {                \
    cl_##name##len res = a;                                                    \
    return res op## = b;                                                       \
  }

// `scalar OP vector` acts like `(vector_t)(scalar) OP vector` in OpenCl:
// all components are set to the scalar value.
#define BIN_SCALAR_OP(name, len, op)                                           \
  inline cl_##name##len &operator op## =(cl_##name##len & a,                   \
                                         const cl_##name & b) {                \
    for (size_t i = 0; i < len; i++)                                           \
      a.s[i] op## = b;                                                         \
    return a;                                                                  \
  }                                                                            \
  inline cl_##name##len operator op(const cl_##name##len & a,                  \
                                    const cl_##name & b) {                     \
    cl_##name##len res = a;                                                    \
    return res op## = b;                                                       \
  }                                                                            \
  inline cl_##name##len operator op(const cl_##name & a,                       \
                                    const cl_##name##len & b) {                \
    cl_##name##len res = b;                                                    \
    return res op## = a;                                                       \
  }

#define CL_VEC_TYPE(name, len)                                                 \
  BIN_OP(name, len, +)                                                         \
  BIN_OP(name, len, -)                                                         \
  BIN_OP(name, len, *)                                                         \
  BIN_OP(name, len, /)                                                         \
  BIN_SCALAR_OP(name, len, +)                                                  \
  BIN_SCALAR_OP(name, len, -)                                                  \
  BIN_SCALAR_OP(name, len, *)                                                  \
  BIN_SCALAR_OP(name, len, /)                                                  \
  inline cl_##name##len operator-(const cl_##name##len & a) {                  \
    cl_##name##len res;                                                        \
    for (size_t i = 0; i < len; i++)                                           \
      res.s[i] = -a.s[i];                                                      \
    return res;                                                                \
  }                                                                            \
  inline std::ostream &operator<<(std::ostream & os,                           \
                                  const cl_##name##len & value) {              \
    os << "(" #name #len ")(";                                                 \
    for (std::size_t i = 0; i < len; i++) {                                    \
      if (i != 0)                                                              \
        os << ',';                                                             \
      os << value.s[i];                                                        \
    }                                                                          \
    return os << ')';                                                          \
  }                                                                            \
  namespace vex {                                                              \
  template <> struct cl_scalar_of<cl_##name##len> {                            \
    typedef cl_##name type;                                                    \
  };                                                                           \
  template <> struct cl_vector_of<cl_##name, len> {                            \
    typedef cl_##name##len type;                                               \
  };                                                                           \
  template <>                                                                  \
  struct cl_vector_length<cl_##name##len>                                      \
      : std::integral_constant<unsigned, len> { };                             \
  }

#define CL_TYPES(name)                                                         \
  CL_VEC_TYPE(name, 2)                                                         \
  CL_VEC_TYPE(name, 4)                                                         \
  CL_VEC_TYPE(name, 8)                                                         \
  CL_VEC_TYPE(name, 16)                                                        \
  namespace vex {                                                              \
  template <> struct cl_scalar_of<cl_##name> {                                 \
    typedef cl_##name type;                                                    \
  };                                                                           \
  template <> struct cl_vector_of<cl_##name, 1> {                              \
    typedef cl_##name type;                                                    \
  };                                                                           \
  template <>                                                                  \
  struct cl_vector_length<cl_##name> : std::integral_constant<unsigned, 1> {}; \
  }

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4146)
#endif
CL_TYPES(float)
CL_TYPES(double)
CL_TYPES(char)
CL_TYPES(uchar)
CL_TYPES(short)
CL_TYPES(ushort)
CL_TYPES(int)
CL_TYPES(uint)
CL_TYPES(long)
CL_TYPES(ulong)
#ifdef _MSC_VER
#  pragma warning(pop)
#endif


#undef BIN_OP
#undef CL_VEC_TYPE
#undef CL_TYPES


namespace vex {

/// Convert each element of the vector to another type.
template<class To, class From>
inline To cl_convert(const From &val) {
    const size_t n = cl_vector_length<To>::value;
    static_assert(n == cl_vector_length<From>::value, "Vectors must be same length.");
    To out;
    for(size_t i = 0 ; i != n ; i++)
        out.s[i] = val.s[i];
    return out;
}

/// Declares a type as CL native, allows using it as a literal.
template <class T> struct is_cl_native : std::false_type {};

/// Convert typename to string.
template <class T, class Enable = void>
struct type_name_impl;

template <class T>
inline std::string type_name() {
    return type_name_impl<T>::get();
}

#define STRINGIFY(name)                                                        \
  template<> struct type_name_impl<cl_##name> {                                \
    static std::string get() { return #name; }                                 \
  };                                                                           \
  template<> struct is_cl_native<cl_##name> : std::true_type { };

// enable use of OpenCL vector types as literals
#define CL_VEC_TYPE(name, len)                                                 \
  template <> struct type_name_impl<cl_##name##len> {                          \
    static std::string get() { return #name #len; }                            \
  };                                                                           \
  template <> struct is_cl_native<cl_##name##len> : std::true_type { };

#define CL_TYPES(name)                                                         \
  STRINGIFY(name)                                                              \
  CL_VEC_TYPE(name, 2)                                                         \
  CL_VEC_TYPE(name, 4)                                                         \
  CL_VEC_TYPE(name, 8)                                                         \
  CL_VEC_TYPE(name, 16)                                                        \

CL_TYPES(float)
CL_TYPES(double)
CL_TYPES(char)
CL_TYPES(uchar)
CL_TYPES(short)
CL_TYPES(ushort)
CL_TYPES(int)
CL_TYPES(uint)
CL_TYPES(long)
CL_TYPES(ulong)

#undef CL_TYPES
#undef CL_VEC_TYPE
#undef STRINGIFY

// One can not pass bool to the kernel, but the overload is needed for type
// deduction:
template <> struct type_name_impl<bool> {
    static std::string get() { return "bool"; }
};

// char and cl_char are different types. Hence, special handling is required:
template <> struct type_name_impl<char> {
    static std::string get() { return "char"; }
};
template <> struct is_cl_native<char> : std::true_type {};
template <> struct cl_vector_length<char> : std::integral_constant<unsigned, 1> {};
template <> struct cl_scalar_of<char> { typedef char type; };

#if defined(__APPLE__)
template <> struct type_name_impl<size_t>
    : public type_name_impl<
        boost::if_c<
            sizeof(std::size_t) == sizeof(uint),
            cl_uint, cl_ulong
        >::type
    >
{};

template <> struct type_name_impl<ptrdiff_t>
    : public type_name_impl<
        boost::if_c<
            sizeof(std::size_t) == sizeof(uint),
            cl_int, cl_long
        >::type
    >
{};

template <> struct is_cl_native<size_t>    : std::true_type {};
template <> struct is_cl_native<ptrdiff_t> : std::true_type {};

template <> struct cl_vector_length<size_t>    : std::integral_constant<unsigned, 1> {};
template <> struct cl_vector_length<ptrdiff_t> : std::integral_constant<unsigned, 1> {};

template <> struct cl_scalar_of<size_t>       { typedef size_t    type; };
template <> struct cl_vector_of<size_t, 1>    { typedef size_t    type; };
template <> struct cl_scalar_of<ptrdiff_t>    { typedef ptrdiff_t type; };
template <> struct cl_vector_of<ptrdiff_t, 1> { typedef ptrdiff_t type; };
#endif

template <class T>
struct is_cl_scalar :
    std::integral_constant<
        bool,
        is_cl_native<T>::value && (cl_vector_length<T>::value == 1)
        >
{};

template <class T>
struct is_cl_vector :
    std::integral_constant<
        bool,
        is_cl_native<T>::value && (cl_vector_length<T>::value > 1)
        >
{};

}

/// \endcond

#endif

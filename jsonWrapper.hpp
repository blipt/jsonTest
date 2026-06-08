#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4710)
#pragma warning(disable : 4711)
#pragma warning(disable : 4464) // relative include path contains '..'
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#pragma warning(disable : 4820) // bytes padding added after data member
#pragma warning(disable : 4626) // assignment operator was implicitly defined as deleted
#pragma warning(disable : 5027) // move assignment operator was implicitly defined as deleted
#pragma warning(disable : 4623) // default constructor was implicitly defined as deleted
#pragma warning(disable : 4365) // conversion from 'int' to 'uint32_t', signed/unsigned mismatch
#pragma warning(disable : 5045) // Compiler will insert Spectre mitigation
#pragma warning(disable : 4061) // enumerator in switch not explicitly handled
#pragma warning(disable : 4371) // layout of class may have changed
#pragma warning(disable : 4625) // copy constructor was implicitly defined as deleted
#pragma warning(disable : 4866) // compiler may not enforce left-to-right evaluation order
#pragma warning(disable : 4800) // Implicit conversion from 'UBool' to bool
#pragma warning(disable : 5219) // implicit conversion, possible loss of data
#pragma warning(disable : 4388) // signed/unsigned mismatch
#pragma warning(disable : 5054) // operator '|': deprecated between enumerations
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable : 5204) // class has virtual functions, but its trivial destructor is not virtual
#pragma warning(disable : 4668) // macro is not defined as a preprocessor macro
#pragma warning(disable : 5039) // pointer or reference to potentially throwing function passed to 'extern "C"' function
#pragma warning(disable : 5262) // implicit conversion, possible loss of data
#endif // _MSC_VER

#include "json.hpp"

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

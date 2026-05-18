#pragma once
#ifndef INCLUDE_NLOHMANN_JSON_HPP_
#define INCLUDE_NLOHMANN_JSON_HPP_
#include <algorithm> 
#include <cstddef> 
#include <functional> 
#include <initializer_list> 
#ifndef JSON_NO_IO
#include <iosfwd> 
#endif  
#include <iterator> 
#include <memory> 
#include <string> 
#include <utility> 
#include <vector> 
#include <utility>
#ifndef JSON_SKIP_LIBRARY_VERSION_CHECK
#if defined(NLOHMANN_JSON_VERSION_MAJOR) && defined(NLOHMANN_JSON_VERSION_MINOR) && defined(NLOHMANN_JSON_VERSION_PATCH)
#if NLOHMANN_JSON_VERSION_MAJOR != 3 || NLOHMANN_JSON_VERSION_MINOR != 11 || NLOHMANN_JSON_VERSION_PATCH != 3
#warning "Already included a different version of the library!"
#endif
#endif
#endif
#define NLOHMANN_JSON_VERSION_MAJOR 3   
#define NLOHMANN_JSON_VERSION_MINOR 11  
#define NLOHMANN_JSON_VERSION_PATCH 3   
#ifndef JSON_DIAGNOSTICS
#define JSON_DIAGNOSTICS 0
#endif
#ifndef JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
#define JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON 0
#endif
#if JSON_DIAGNOSTICS
#define NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS _diag
#else
#define NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS
#endif
#if JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
#define NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON _ldvcmp
#else
#define NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON
#endif
#ifndef NLOHMANN_JSON_NAMESPACE_NO_VERSION
#define NLOHMANN_JSON_NAMESPACE_NO_VERSION 0
#endif
#define NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b) json_abi ## a ## b
#define NLOHMANN_JSON_ABI_TAGS_CONCAT(a, b) \
    NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b)
#define NLOHMANN_JSON_ABI_TAGS                                       \
    NLOHMANN_JSON_ABI_TAGS_CONCAT(                                   \
            NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS,                       \
            NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON)
#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(major, minor, patch) \
    _v ## major ## _ ## minor ## _ ## patch
#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(major, minor, patch) \
    NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(major, minor, patch)
#if NLOHMANN_JSON_NAMESPACE_NO_VERSION
#define NLOHMANN_JSON_NAMESPACE_VERSION
#else
#define NLOHMANN_JSON_NAMESPACE_VERSION                                 \
    NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(NLOHMANN_JSON_VERSION_MAJOR, \
                                           NLOHMANN_JSON_VERSION_MINOR, \
                                           NLOHMANN_JSON_VERSION_PATCH)
#endif
#define NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b) a ## b
#define NLOHMANN_JSON_NAMESPACE_CONCAT(a, b) \
    NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b)
#ifndef NLOHMANN_JSON_NAMESPACE
#define NLOHMANN_JSON_NAMESPACE               \
    JsonLib::NLOHMANN_JSON_NAMESPACE_CONCAT( \
            NLOHMANN_JSON_ABI_TAGS,           \
            NLOHMANN_JSON_NAMESPACE_VERSION)
#endif
#ifndef NLOHMANN_JSON_NAMESPACE_BEGIN
#define NLOHMANN_JSON_NAMESPACE_BEGIN                \
    namespace JsonLib                               \
    {                                                \
    inline namespace NLOHMANN_JSON_NAMESPACE_CONCAT( \
                NLOHMANN_JSON_ABI_TAGS,              \
                NLOHMANN_JSON_NAMESPACE_VERSION)     \
    {
#endif
#ifndef NLOHMANN_JSON_NAMESPACE_END
#define NLOHMANN_JSON_NAMESPACE_END                                     \
    }   \
    }  
#endif
#include <algorithm> 
#include <array> 
#include <forward_list> 
#include <iterator> 
#include <map> 
#include <string> 
#include <tuple> 
#include <type_traits> 
#include <unordered_map> 
#include <utility> 
#include <valarray> 
#include <cstddef> 
#include <exception> 
#if JSON_DIAGNOSTICS
#include <numeric> 
#endif
#include <stdexcept> 
#include <string> 
#include <vector> 
#include <array> 
#include <cstddef> 
#include <cstdint> 
#include <string> 
#include <utility> 
#include <type_traits>
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename ...Ts> struct make_void
	{
		using type = void;
	};
	template<typename ...Ts> using void_t = typename make_void<Ts...>::type;
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	struct nonesuch
	{
		nonesuch() = delete;
		~nonesuch() = delete;
		nonesuch(nonesuch const&) = delete;
		nonesuch(nonesuch const&&) = delete;
		void operator=(nonesuch const&) = delete;
		void operator=(nonesuch&&) = delete;
	};
	template<class Default,
		class AlwaysVoid,
		template<class...> class Op,
		class... Args>
	struct detector
	{
		using value_t = std::false_type;
		using type = Default;
	};
	template<class Default, template<class...> class Op, class... Args>
	struct detector<Default, void_t<Op<Args...>>, Op, Args...>
	{
		using value_t = std::true_type;
		using type = Op<Args...>;
	};
	template<template<class...> class Op, class... Args>
	using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;
	template<template<class...> class Op, class... Args>
	struct is_detected_lazy : is_detected<Op, Args...> {};
	template<template<class...> class Op, class... Args>
	using detected_t = typename detector<nonesuch, void, Op, Args...>::type;
	template<class Default, template<class...> class Op, class... Args>
	using detected_or = detector<Default, void, Op, Args...>;
	template<class Default, template<class...> class Op, class... Args>
	using detected_or_t = typename detected_or<Default, Op, Args...>::type;
	template<class Expected, template<class...> class Op, class... Args>
	using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;
	template<class To, template<class...> class Op, class... Args>
	using is_detected_convertible =
		std::is_convertible<detected_t<Op, Args...>, To>;
}
NLOHMANN_JSON_NAMESPACE_END
#if !defined(JSON_HEDLEY_VERSION) || (JSON_HEDLEY_VERSION < 15)
#if defined(JSON_HEDLEY_VERSION)
#undef JSON_HEDLEY_VERSION
#endif
#define JSON_HEDLEY_VERSION 15
#if defined(JSON_HEDLEY_STRINGIFY_EX)
#undef JSON_HEDLEY_STRINGIFY_EX
#endif
#define JSON_HEDLEY_STRINGIFY_EX(x) #x
#if defined(JSON_HEDLEY_STRINGIFY)
#undef JSON_HEDLEY_STRINGIFY
#endif
#define JSON_HEDLEY_STRINGIFY(x) JSON_HEDLEY_STRINGIFY_EX(x)
#if defined(JSON_HEDLEY_CONCAT_EX)
#undef JSON_HEDLEY_CONCAT_EX
#endif
#define JSON_HEDLEY_CONCAT_EX(a,b) a##b
#if defined(JSON_HEDLEY_CONCAT)
#undef JSON_HEDLEY_CONCAT
#endif
#define JSON_HEDLEY_CONCAT(a,b) JSON_HEDLEY_CONCAT_EX(a,b)
#if defined(JSON_HEDLEY_CONCAT3_EX)
#undef JSON_HEDLEY_CONCAT3_EX
#endif
#define JSON_HEDLEY_CONCAT3_EX(a,b,c) a##b##c
#if defined(JSON_HEDLEY_CONCAT3)
#undef JSON_HEDLEY_CONCAT3
#endif
#define JSON_HEDLEY_CONCAT3(a,b,c) JSON_HEDLEY_CONCAT3_EX(a,b,c)
#if defined(JSON_HEDLEY_VERSION_ENCODE)
#undef JSON_HEDLEY_VERSION_ENCODE
#endif
#define JSON_HEDLEY_VERSION_ENCODE(major,minor,revision) (((major) * 1000000) + ((minor) * 1000) + (revision))
#if defined(JSON_HEDLEY_VERSION_DECODE_MAJOR)
#undef JSON_HEDLEY_VERSION_DECODE_MAJOR
#endif
#define JSON_HEDLEY_VERSION_DECODE_MAJOR(version) ((version) / 1000000)
#if defined(JSON_HEDLEY_VERSION_DECODE_MINOR)
#undef JSON_HEDLEY_VERSION_DECODE_MINOR
#endif
#define JSON_HEDLEY_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)
#if defined(JSON_HEDLEY_VERSION_DECODE_REVISION)
#undef JSON_HEDLEY_VERSION_DECODE_REVISION
#endif
#define JSON_HEDLEY_VERSION_DECODE_REVISION(version) ((version) % 1000)
#if defined(JSON_HEDLEY_GNUC_VERSION)
#undef JSON_HEDLEY_GNUC_VERSION
#endif
#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#define JSON_HEDLEY_GNUC_VERSION JSON_HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
#define JSON_HEDLEY_GNUC_VERSION JSON_HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)
#endif
#if defined(JSON_HEDLEY_GNUC_VERSION_CHECK)
#undef JSON_HEDLEY_GNUC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_GNUC_VERSION)
#define JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_GNUC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_MSVC_VERSION)
#undef JSON_HEDLEY_MSVC_VERSION
#endif
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
#define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 10000000, (_MSC_FULL_VER % 10000000) / 100000, (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER) && !defined(__ICL)
#define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER) && !defined(__ICL)
#define JSON_HEDLEY_MSVC_VERSION JSON_HEDLEY_VERSION_ENCODE(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif
#if defined(JSON_HEDLEY_MSVC_VERSION_CHECK)
#undef JSON_HEDLEY_MSVC_VERSION_CHECK
#endif
#if !defined(JSON_HEDLEY_MSVC_VERSION)
#define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#define JSON_HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_VER >= ((major * 100) + (minor)))
#endif
#if defined(JSON_HEDLEY_INTEL_VERSION)
#undef JSON_HEDLEY_INTEL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && !defined(__ICL)
#define JSON_HEDLEY_INTEL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, __INTEL_COMPILER_UPDATE)
#elif defined(__INTEL_COMPILER) && !defined(__ICL)
#define JSON_HEDLEY_INTEL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, 0)
#endif
#if defined(JSON_HEDLEY_INTEL_VERSION_CHECK)
#undef JSON_HEDLEY_INTEL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_INTEL_VERSION)
#define JSON_HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_INTEL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_INTEL_CL_VERSION)
#undef JSON_HEDLEY_INTEL_CL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && defined(__ICL)
#define JSON_HEDLEY_INTEL_CL_VERSION JSON_HEDLEY_VERSION_ENCODE(__INTEL_COMPILER, __INTEL_COMPILER_UPDATE, 0)
#endif
#if defined(JSON_HEDLEY_INTEL_CL_VERSION_CHECK)
#undef JSON_HEDLEY_INTEL_CL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_INTEL_CL_VERSION)
#define JSON_HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_INTEL_CL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_PGI_VERSION)
#undef JSON_HEDLEY_PGI_VERSION
#endif
#if defined(__PGI) && defined(__PGIC__) && defined(__PGIC_MINOR__) && defined(__PGIC_PATCHLEVEL__)
#define JSON_HEDLEY_PGI_VERSION JSON_HEDLEY_VERSION_ENCODE(__PGIC__, __PGIC_MINOR__, __PGIC_PATCHLEVEL__)
#endif
#if defined(JSON_HEDLEY_PGI_VERSION_CHECK)
#undef JSON_HEDLEY_PGI_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_PGI_VERSION)
#define JSON_HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_PGI_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_SUNPRO_VERSION)
#undef JSON_HEDLEY_SUNPRO_VERSION
#endif
#if defined(__SUNPRO_C) && (__SUNPRO_C > 0x1000)
#define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((((__SUNPRO_C >> 16) & 0xf) * 10) + ((__SUNPRO_C >> 12) & 0xf), (((__SUNPRO_C >> 8) & 0xf) * 10) + ((__SUNPRO_C >> 4) & 0xf), (__SUNPRO_C & 0xf) * 10)
#elif defined(__SUNPRO_C)
#define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((__SUNPRO_C >> 8) & 0xf, (__SUNPRO_C >> 4) & 0xf, (__SUNPRO_C) & 0xf)
#elif defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x1000)
#define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((((__SUNPRO_CC >> 16) & 0xf) * 10) + ((__SUNPRO_CC >> 12) & 0xf), (((__SUNPRO_CC >> 8) & 0xf) * 10) + ((__SUNPRO_CC >> 4) & 0xf), (__SUNPRO_CC & 0xf) * 10)
#elif defined(__SUNPRO_CC)
#define JSON_HEDLEY_SUNPRO_VERSION JSON_HEDLEY_VERSION_ENCODE((__SUNPRO_CC >> 8) & 0xf, (__SUNPRO_CC >> 4) & 0xf, (__SUNPRO_CC) & 0xf)
#endif
#if defined(JSON_HEDLEY_SUNPRO_VERSION_CHECK)
#undef JSON_HEDLEY_SUNPRO_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_SUNPRO_VERSION)
#define JSON_HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_SUNPRO_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION)
#undef JSON_HEDLEY_EMSCRIPTEN_VERSION
#endif
#if defined(__EMSCRIPTEN__)
#define JSON_HEDLEY_EMSCRIPTEN_VERSION JSON_HEDLEY_VERSION_ENCODE(__EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, __EMSCRIPTEN_tiny__)
#endif
#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK)
#undef JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_EMSCRIPTEN_VERSION)
#define JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_EMSCRIPTEN_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_ARM_VERSION)
#undef JSON_HEDLEY_ARM_VERSION
#endif
#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
#define JSON_HEDLEY_ARM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ARMCOMPILER_VERSION / 1000000, (__ARMCOMPILER_VERSION % 1000000) / 10000, (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
#define JSON_HEDLEY_ARM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ARMCC_VERSION / 1000000, (__ARMCC_VERSION % 1000000) / 10000, (__ARMCC_VERSION % 10000) / 100)
#endif
#if defined(JSON_HEDLEY_ARM_VERSION_CHECK)
#undef JSON_HEDLEY_ARM_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_ARM_VERSION)
#define JSON_HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_ARM_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_IBM_VERSION)
#undef JSON_HEDLEY_IBM_VERSION
#endif
#if defined(__ibmxl__)
#define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__ibmxl_version__, __ibmxl_release__, __ibmxl_modification__)
#elif defined(__xlC__) && defined(__xlC_ver__)
#define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, (__xlC_ver__ >> 8) & 0xff)
#elif defined(__xlC__)
#define JSON_HEDLEY_IBM_VERSION JSON_HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, 0)
#endif
#if defined(JSON_HEDLEY_IBM_VERSION_CHECK)
#undef JSON_HEDLEY_IBM_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_IBM_VERSION)
#define JSON_HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_IBM_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_VERSION)
#undef JSON_HEDLEY_TI_VERSION
#endif
#if \
    defined(__TI_COMPILER_VERSION__) && \
    ( \
      defined(__TMS470__) || defined(__TI_ARM__) || \
      defined(__MSP430__) || \
      defined(__TMS320C2000__) \
    )
#if (__TI_COMPILER_VERSION__ >= 16000000)
#define JSON_HEDLEY_TI_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#endif
#if defined(JSON_HEDLEY_TI_VERSION_CHECK)
#undef JSON_HEDLEY_TI_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_VERSION)
#define JSON_HEDLEY_TI_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_CL2000_VERSION)
#undef JSON_HEDLEY_TI_CL2000_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C2000__)
#define JSON_HEDLEY_TI_CL2000_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_CL2000_VERSION_CHECK)
#undef JSON_HEDLEY_TI_CL2000_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL2000_VERSION)
#define JSON_HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL2000_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_CL430_VERSION)
#undef JSON_HEDLEY_TI_CL430_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__MSP430__)
#define JSON_HEDLEY_TI_CL430_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_CL430_VERSION_CHECK)
#undef JSON_HEDLEY_TI_CL430_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL430_VERSION)
#define JSON_HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL430_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_ARMCL_VERSION)
#undef JSON_HEDLEY_TI_ARMCL_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && (defined(__TMS470__) || defined(__TI_ARM__))
#define JSON_HEDLEY_TI_ARMCL_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_ARMCL_VERSION_CHECK)
#undef JSON_HEDLEY_TI_ARMCL_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_ARMCL_VERSION)
#define JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_ARMCL_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_CL6X_VERSION)
#undef JSON_HEDLEY_TI_CL6X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C6X__)
#define JSON_HEDLEY_TI_CL6X_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_CL6X_VERSION_CHECK)
#undef JSON_HEDLEY_TI_CL6X_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL6X_VERSION)
#define JSON_HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL6X_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_CL7X_VERSION)
#undef JSON_HEDLEY_TI_CL7X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__C7000__)
#define JSON_HEDLEY_TI_CL7X_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_CL7X_VERSION_CHECK)
#undef JSON_HEDLEY_TI_CL7X_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CL7X_VERSION)
#define JSON_HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CL7X_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TI_CLPRU_VERSION)
#undef JSON_HEDLEY_TI_CLPRU_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__PRU__)
#define JSON_HEDLEY_TI_CLPRU_VERSION JSON_HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif
#if defined(JSON_HEDLEY_TI_CLPRU_VERSION_CHECK)
#undef JSON_HEDLEY_TI_CLPRU_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TI_CLPRU_VERSION)
#define JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TI_CLPRU_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_CRAY_VERSION)
#undef JSON_HEDLEY_CRAY_VERSION
#endif
#if defined(_CRAYC)
#if defined(_RELEASE_PATCHLEVEL)
#define JSON_HEDLEY_CRAY_VERSION JSON_HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, _RELEASE_PATCHLEVEL)
#else
#define JSON_HEDLEY_CRAY_VERSION JSON_HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, 0)
#endif
#endif
#if defined(JSON_HEDLEY_CRAY_VERSION_CHECK)
#undef JSON_HEDLEY_CRAY_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_CRAY_VERSION)
#define JSON_HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_CRAY_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_IAR_VERSION)
#undef JSON_HEDLEY_IAR_VERSION
#endif
#if defined(__IAR_SYSTEMS_ICC__)
#if __VER__ > 1000
#define JSON_HEDLEY_IAR_VERSION JSON_HEDLEY_VERSION_ENCODE((__VER__ / 1000000), ((__VER__ / 1000) % 1000), (__VER__ % 1000))
#else
#define JSON_HEDLEY_IAR_VERSION JSON_HEDLEY_VERSION_ENCODE(__VER__ / 100, __VER__ % 100, 0)
#endif
#endif
#if defined(JSON_HEDLEY_IAR_VERSION_CHECK)
#undef JSON_HEDLEY_IAR_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_IAR_VERSION)
#define JSON_HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_IAR_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_TINYC_VERSION)
#undef JSON_HEDLEY_TINYC_VERSION
#endif
#if defined(__TINYC__)
#define JSON_HEDLEY_TINYC_VERSION JSON_HEDLEY_VERSION_ENCODE(__TINYC__ / 1000, (__TINYC__ / 100) % 10, __TINYC__ % 100)
#endif
#if defined(JSON_HEDLEY_TINYC_VERSION_CHECK)
#undef JSON_HEDLEY_TINYC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_TINYC_VERSION)
#define JSON_HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_TINYC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_DMC_VERSION)
#undef JSON_HEDLEY_DMC_VERSION
#endif
#if defined(__DMC__)
#define JSON_HEDLEY_DMC_VERSION JSON_HEDLEY_VERSION_ENCODE(__DMC__ >> 8, (__DMC__ >> 4) & 0xf, __DMC__ & 0xf)
#endif
#if defined(JSON_HEDLEY_DMC_VERSION_CHECK)
#undef JSON_HEDLEY_DMC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_DMC_VERSION)
#define JSON_HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_DMC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_COMPCERT_VERSION)
#undef JSON_HEDLEY_COMPCERT_VERSION
#endif
#if defined(__COMPCERT_VERSION__)
#define JSON_HEDLEY_COMPCERT_VERSION JSON_HEDLEY_VERSION_ENCODE(__COMPCERT_VERSION__ / 10000, (__COMPCERT_VERSION__ / 100) % 100, __COMPCERT_VERSION__ % 100)
#endif
#if defined(JSON_HEDLEY_COMPCERT_VERSION_CHECK)
#undef JSON_HEDLEY_COMPCERT_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_COMPCERT_VERSION)
#define JSON_HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_COMPCERT_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_PELLES_VERSION)
#undef JSON_HEDLEY_PELLES_VERSION
#endif
#if defined(__POCC__)
#define JSON_HEDLEY_PELLES_VERSION JSON_HEDLEY_VERSION_ENCODE(__POCC__ / 100, __POCC__ % 100, 0)
#endif
#if defined(JSON_HEDLEY_PELLES_VERSION_CHECK)
#undef JSON_HEDLEY_PELLES_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_PELLES_VERSION)
#define JSON_HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_PELLES_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_MCST_LCC_VERSION)
#undef JSON_HEDLEY_MCST_LCC_VERSION
#endif
#if defined(__LCC__) && defined(__LCC_MINOR__)
#define JSON_HEDLEY_MCST_LCC_VERSION JSON_HEDLEY_VERSION_ENCODE(__LCC__ / 100, __LCC__ % 100, __LCC_MINOR__)
#endif
#if defined(JSON_HEDLEY_MCST_LCC_VERSION_CHECK)
#undef JSON_HEDLEY_MCST_LCC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_MCST_LCC_VERSION)
#define JSON_HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_MCST_LCC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_GCC_VERSION)
#undef JSON_HEDLEY_GCC_VERSION
#endif
#if \
    defined(JSON_HEDLEY_GNUC_VERSION) && \
    !defined(__clang__) && \
    !defined(JSON_HEDLEY_INTEL_VERSION) && \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_ARM_VERSION) && \
    !defined(JSON_HEDLEY_CRAY_VERSION) && \
    !defined(JSON_HEDLEY_TI_VERSION) && \
    !defined(JSON_HEDLEY_TI_ARMCL_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL430_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL2000_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL6X_VERSION) && \
    !defined(JSON_HEDLEY_TI_CL7X_VERSION) && \
    !defined(JSON_HEDLEY_TI_CLPRU_VERSION) && \
    !defined(__COMPCERT__) && \
    !defined(JSON_HEDLEY_MCST_LCC_VERSION)
#define JSON_HEDLEY_GCC_VERSION JSON_HEDLEY_GNUC_VERSION
#endif
#if defined(JSON_HEDLEY_GCC_VERSION_CHECK)
#undef JSON_HEDLEY_GCC_VERSION_CHECK
#endif
#if defined(JSON_HEDLEY_GCC_VERSION)
#define JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (JSON_HEDLEY_GCC_VERSION >= JSON_HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (0)
#endif
#if defined(JSON_HEDLEY_HAS_ATTRIBUTE)
#undef JSON_HEDLEY_HAS_ATTRIBUTE
#endif
#if \
  defined(__has_attribute) && \
  ( \
    (!defined(JSON_HEDLEY_IAR_VERSION) || JSON_HEDLEY_IAR_VERSION_CHECK(8,5,9)) \
  )
#  define JSON_HEDLEY_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#  define JSON_HEDLEY_HAS_ATTRIBUTE(attribute) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_ATTRIBUTE)
#undef JSON_HEDLEY_GNUC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#define JSON_HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)
#else
#define JSON_HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_ATTRIBUTE)
#undef JSON_HEDLEY_GCC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#define JSON_HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)
#else
#define JSON_HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_CPP_ATTRIBUTE)
#undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE
#endif
#if \
    defined(__has_cpp_attribute) && \
    defined(__cplusplus) && \
    (!defined(JSON_HEDLEY_SUNPRO_VERSION) || JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0))
#define JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)
#else
#define JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute) (0)
#endif
#if defined(JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS)
#undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS
#endif
#if !defined(__cplusplus) || !defined(__has_cpp_attribute)
#define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#elif \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_IAR_VERSION) && \
    (!defined(JSON_HEDLEY_SUNPRO_VERSION) || JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0)) && \
    (!defined(JSON_HEDLEY_MSVC_VERSION) || JSON_HEDLEY_MSVC_VERSION_CHECK(19,20,0))
#define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) JSON_HEDLEY_HAS_CPP_ATTRIBUTE(ns::attribute)
#else
#define JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE)
#undef JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#define JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
#define JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE)
#undef JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#define JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
#define JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_BUILTIN)
#undef JSON_HEDLEY_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define JSON_HEDLEY_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else
#define JSON_HEDLEY_HAS_BUILTIN(builtin) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_BUILTIN)
#undef JSON_HEDLEY_GNUC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define JSON_HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
#define JSON_HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_BUILTIN)
#undef JSON_HEDLEY_GCC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define JSON_HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
#define JSON_HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_FEATURE)
#undef JSON_HEDLEY_HAS_FEATURE
#endif
#if defined(__has_feature)
#define JSON_HEDLEY_HAS_FEATURE(feature) __has_feature(feature)
#else
#define JSON_HEDLEY_HAS_FEATURE(feature) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_FEATURE)
#undef JSON_HEDLEY_GNUC_HAS_FEATURE
#endif
#if defined(__has_feature)
#define JSON_HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
#define JSON_HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_FEATURE)
#undef JSON_HEDLEY_GCC_HAS_FEATURE
#endif
#if defined(__has_feature)
#define JSON_HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
#define JSON_HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_EXTENSION)
#undef JSON_HEDLEY_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define JSON_HEDLEY_HAS_EXTENSION(extension) __has_extension(extension)
#else
#define JSON_HEDLEY_HAS_EXTENSION(extension) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_EXTENSION)
#undef JSON_HEDLEY_GNUC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define JSON_HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
#define JSON_HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_EXTENSION)
#undef JSON_HEDLEY_GCC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define JSON_HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
#define JSON_HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE)
#undef JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) __has_declspec_attribute(attribute)
#else
#define JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE)
#undef JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
#define JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE)
#undef JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
#define JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_HAS_WARNING)
#undef JSON_HEDLEY_HAS_WARNING
#endif
#if defined(__has_warning)
#define JSON_HEDLEY_HAS_WARNING(warning) __has_warning(warning)
#else
#define JSON_HEDLEY_HAS_WARNING(warning) (0)
#endif
#if defined(JSON_HEDLEY_GNUC_HAS_WARNING)
#undef JSON_HEDLEY_GNUC_HAS_WARNING
#endif
#if defined(__has_warning)
#define JSON_HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
#define JSON_HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) JSON_HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_GCC_HAS_WARNING)
#undef JSON_HEDLEY_GCC_HAS_WARNING
#endif
#if defined(__has_warning)
#define JSON_HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
#define JSON_HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
    defined(__clang__) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(5,0,0) || \
    JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,17) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(8,0,0) || \
    (JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) && defined(__C99_PRAGMA_OPERATOR))
#define JSON_HEDLEY_PRAGMA(value) _Pragma(#value)
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#define JSON_HEDLEY_PRAGMA(value) __pragma(value)
#else
#define JSON_HEDLEY_PRAGMA(value)
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_PUSH)
#undef JSON_HEDLEY_DIAGNOSTIC_PUSH
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_POP)
#undef JSON_HEDLEY_DIAGNOSTIC_POP
#endif
#if defined(__clang__)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(push))
#define JSON_HEDLEY_DIAGNOSTIC_POP __pragma(warning(pop))
#elif JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("push")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("pop")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,4,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("diag_push")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("diag_pop")
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,90,0)
#define JSON_HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#define JSON_HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#else
#define JSON_HEDLEY_DIAGNOSTIC_PUSH
#define JSON_HEDLEY_DIAGNOSTIC_POP
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_
#endif
#if defined(__cplusplus)
#  if JSON_HEDLEY_HAS_WARNING("-Wc++98-compat")
#    if JSON_HEDLEY_HAS_WARNING("-Wc++17-extensions")
#      if JSON_HEDLEY_HAS_WARNING("-Wc++1z-extensions")
#        define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
    _Pragma("clang diagnostic ignored \"-Wc++1z-extensions\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#      else
#        define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#      endif
#    else
#      define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
    xpr \
    JSON_HEDLEY_DIAGNOSTIC_POP
#    endif
#  endif
#endif
#if !defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(x) x
#endif
#if defined(JSON_HEDLEY_CONST_CAST)
#undef JSON_HEDLEY_CONST_CAST
#endif
#if defined(__cplusplus)
#  define JSON_HEDLEY_CONST_CAST(T, expr) (const_cast<T>(expr))
#elif \
  JSON_HEDLEY_HAS_WARNING("-Wcast-qual") || \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_CONST_CAST(T, expr) (__extension__ ({ \
        JSON_HEDLEY_DIAGNOSTIC_PUSH \
        JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
        ((T) (expr)); \
        JSON_HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define JSON_HEDLEY_CONST_CAST(T, expr) ((T) (expr))
#endif
#if defined(JSON_HEDLEY_REINTERPRET_CAST)
#undef JSON_HEDLEY_REINTERPRET_CAST
#endif
#if defined(__cplusplus)
#define JSON_HEDLEY_REINTERPRET_CAST(T, expr) (reinterpret_cast<T>(expr))
#else
#define JSON_HEDLEY_REINTERPRET_CAST(T, expr) ((T) (expr))
#endif
#if defined(JSON_HEDLEY_STATIC_CAST)
#undef JSON_HEDLEY_STATIC_CAST
#endif
#if defined(__cplusplus)
#define JSON_HEDLEY_STATIC_CAST(T, expr) (static_cast<T>(expr))
#else
#define JSON_HEDLEY_STATIC_CAST(T, expr) ((T) (expr))
#endif
#if defined(JSON_HEDLEY_CPP_CAST)
#undef JSON_HEDLEY_CPP_CAST
#endif
#if defined(__cplusplus)
#  if JSON_HEDLEY_HAS_WARNING("-Wold-style-cast")
#    define JSON_HEDLEY_CPP_CAST(T, expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wold-style-cast\"") \
    ((T) (expr)) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  elif JSON_HEDLEY_IAR_VERSION_CHECK(8,3,0)
#    define JSON_HEDLEY_CPP_CAST(T, expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("diag_suppress=Pe137") \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  else
#    define JSON_HEDLEY_CPP_CAST(T, expr) ((T) (expr))
#  endif
#else
#  define JSON_HEDLEY_CPP_CAST(T, expr) (expr)
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wdeprecated-declarations")
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warning(disable:1478 1786)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:1478 1786))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(20,7,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1216,1444,1445")
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:4996))
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1291,1718")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && !defined(__cplusplus)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,E_DEPRECATED_ATT,E_DEPRECATED_ATT_MESS)")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && defined(__cplusplus)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,symdeprecated,symdeprecated2)")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress=Pe1444,Pe1215")
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,90,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warn(disable:2241)")
#else
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("warning(disable:161)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:161))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 1675")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:4068))
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(16,9,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress=Pe161")
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 161")
#else
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-attributes")
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("clang diagnostic ignored \"-Wunknown-attributes\"")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(4,6,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(17,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("warning(disable:1292)")
#elif JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:1292))
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(19,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:5030))
#elif JSON_HEDLEY_PGI_VERSION_CHECK(20,7,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097,1098")
#elif JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("error_messages(off,attrskipunsup)")
#elif \
    JSON_HEDLEY_TI_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1173")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress=Pe1097")
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#else
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wcast-qual")
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("warning(disable:2203 2331)")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif
#if defined(JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION)
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunused-function")
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("clang diagnostic ignored \"-Wunused-function\"")
#elif JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#elif JSON_HEDLEY_MSVC_VERSION_CHECK(1,0,0)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION __pragma(warning(disable:4505))
#elif JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("diag_suppress 3142")
#else
#define JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif
#if defined(JSON_HEDLEY_DEPRECATED)
#undef JSON_HEDLEY_DEPRECATED
#endif
#if defined(JSON_HEDLEY_DEPRECATED_FOR)
#undef JSON_HEDLEY_DEPRECATED_FOR
#endif
#if \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DEPRECATED(since) __declspec(deprecated("Since " # since))
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated("Since " #since "; use " #replacement))
#elif \
    (JSON_HEDLEY_HAS_EXTENSION(attribute_deprecated_with_message) && !defined(JSON_HEDLEY_IAR_VERSION)) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(18,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_DEPRECATED(since) __attribute__((__deprecated__("Since " #since)))
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__("Since " #since "; use " #replacement)))
#elif defined(__cplusplus) && (__cplusplus >= 201402L)
#define JSON_HEDLEY_DEPRECATED(since) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since)]])
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since "; use " #replacement)]])
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(deprecated) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
#define JSON_HEDLEY_DEPRECATED(since) __attribute__((__deprecated__))
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_PELLES_VERSION_CHECK(6,50,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_DEPRECATED(since) __declspec(deprecated)
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated)
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_DEPRECATED(since) _Pragma("deprecated")
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement) _Pragma("deprecated")
#else
#define JSON_HEDLEY_DEPRECATED(since)
#define JSON_HEDLEY_DEPRECATED_FOR(since, replacement)
#endif
#if defined(JSON_HEDLEY_UNAVAILABLE)
#undef JSON_HEDLEY_UNAVAILABLE
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(warning) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_UNAVAILABLE(available_since) __attribute__((__warning__("Not available until " #available_since)))
#else
#define JSON_HEDLEY_UNAVAILABLE(available_since)
#endif
#if defined(JSON_HEDLEY_WARN_UNUSED_RESULT)
#undef JSON_HEDLEY_WARN_UNUSED_RESULT
#endif
#if defined(JSON_HEDLEY_WARN_UNUSED_RESULT_MSG)
#undef JSON_HEDLEY_WARN_UNUSED_RESULT_MSG
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(warn_unused_result) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) __attribute__((__warn_unused_result__))
#elif (JSON_HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L)
#define JSON_HEDLEY_WARN_UNUSED_RESULT JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard(msg)]])
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard)
#define JSON_HEDLEY_WARN_UNUSED_RESULT JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#elif defined(_Check_return_) 
#define JSON_HEDLEY_WARN_UNUSED_RESULT _Check_return_
#define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg) _Check_return_
#else
#define JSON_HEDLEY_WARN_UNUSED_RESULT
#define JSON_HEDLEY_WARN_UNUSED_RESULT_MSG(msg)
#endif
#if defined(JSON_HEDLEY_SENTINEL)
#undef JSON_HEDLEY_SENTINEL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(sentinel) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_SENTINEL(position) __attribute__((__sentinel__(position)))
#else
#define JSON_HEDLEY_SENTINEL(position)
#endif
#if defined(JSON_HEDLEY_NO_RETURN)
#undef JSON_HEDLEY_NO_RETURN
#endif
#if JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_NO_RETURN __noreturn
#elif \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define JSON_HEDLEY_NO_RETURN _Noreturn
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#define JSON_HEDLEY_NO_RETURN JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[noreturn]])
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(noreturn) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,2,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
#define JSON_HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#define JSON_HEDLEY_NO_RETURN _Pragma("does_not_return")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_NO_RETURN __declspec(noreturn)
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
#define JSON_HEDLEY_NO_RETURN _Pragma("FUNC_NEVER_RETURNS;")
#elif JSON_HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
#define JSON_HEDLEY_NO_RETURN __attribute((noreturn))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(9,0,0)
#define JSON_HEDLEY_NO_RETURN __declspec(noreturn)
#else
#define JSON_HEDLEY_NO_RETURN
#endif
#if defined(JSON_HEDLEY_NO_ESCAPE)
#undef JSON_HEDLEY_NO_ESCAPE
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(noescape)
#define JSON_HEDLEY_NO_ESCAPE __attribute__((__noescape__))
#else
#define JSON_HEDLEY_NO_ESCAPE
#endif
#if defined(JSON_HEDLEY_UNREACHABLE)
#undef JSON_HEDLEY_UNREACHABLE
#endif
#if defined(JSON_HEDLEY_UNREACHABLE_RETURN)
#undef JSON_HEDLEY_UNREACHABLE_RETURN
#endif
#if defined(JSON_HEDLEY_ASSUME)
#undef JSON_HEDLEY_ASSUME
#endif
#if \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_ASSUME(expr) __assume(expr)
#elif JSON_HEDLEY_HAS_BUILTIN(__builtin_assume)
#define JSON_HEDLEY_ASSUME(expr) __builtin_assume(expr)
#elif \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
#if defined(__cplusplus)
#define JSON_HEDLEY_ASSUME(expr) std::_nassert(expr)
#else
#define JSON_HEDLEY_ASSUME(expr) _nassert(expr)
#endif
#endif
#if \
    (JSON_HEDLEY_HAS_BUILTIN(__builtin_unreachable) && (!defined(JSON_HEDLEY_ARM_VERSION))) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(18,10,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,5) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(10,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_UNREACHABLE() __builtin_unreachable()
#elif defined(JSON_HEDLEY_ASSUME)
#define JSON_HEDLEY_UNREACHABLE() JSON_HEDLEY_ASSUME(0)
#endif
#if !defined(JSON_HEDLEY_ASSUME)
#if defined(JSON_HEDLEY_UNREACHABLE)
#define JSON_HEDLEY_ASSUME(expr) JSON_HEDLEY_STATIC_CAST(void, ((expr) ? 1 : (JSON_HEDLEY_UNREACHABLE(), 1)))
#else
#define JSON_HEDLEY_ASSUME(expr) JSON_HEDLEY_STATIC_CAST(void, expr)
#endif
#endif
#if defined(JSON_HEDLEY_UNREACHABLE)
#if  \
        JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
        JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
#define JSON_HEDLEY_UNREACHABLE_RETURN(value) return (JSON_HEDLEY_STATIC_CAST(void, JSON_HEDLEY_ASSUME(0)), (value))
#else
#define JSON_HEDLEY_UNREACHABLE_RETURN(value) JSON_HEDLEY_UNREACHABLE()
#endif
#else
#define JSON_HEDLEY_UNREACHABLE_RETURN(value) return (value)
#endif
#if !defined(JSON_HEDLEY_UNREACHABLE)
#define JSON_HEDLEY_UNREACHABLE() JSON_HEDLEY_ASSUME(0)
#endif
JSON_HEDLEY_DIAGNOSTIC_PUSH
#if JSON_HEDLEY_HAS_WARNING("-Wpedantic")
#pragma clang diagnostic ignored "-Wpedantic"
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic") && defined(__cplusplus)
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#if JSON_HEDLEY_GCC_HAS_WARNING("-Wvariadic-macros",4,0,0)
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wvariadic-macros"
#elif defined(JSON_HEDLEY_GCC_VERSION)
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif
#endif
#if defined(JSON_HEDLEY_NON_NULL)
#undef JSON_HEDLEY_NON_NULL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(nonnull) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0)
#define JSON_HEDLEY_NON_NULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
#define JSON_HEDLEY_NON_NULL(...)
#endif
JSON_HEDLEY_DIAGNOSTIC_POP
#if defined(JSON_HEDLEY_PRINTF_FORMAT)
#undef JSON_HEDLEY_PRINTF_FORMAT
#endif
#if defined(__MINGW32__) && JSON_HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && !defined(__USE_MINGW_ANSI_STDIO)
#define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(ms_printf, string_idx, first_to_check)))
#elif defined(__MINGW32__) && JSON_HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && defined(__USE_MINGW_ANSI_STDIO)
#define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(gnu_printf, string_idx, first_to_check)))
#elif \
    JSON_HEDLEY_HAS_ATTRIBUTE(format) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(__printf__, string_idx, first_to_check)))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(6,0,0)
#define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __declspec(vaformat(printf,string_idx,first_to_check))
#else
#define JSON_HEDLEY_PRINTF_FORMAT(string_idx,first_to_check)
#endif
#if defined(JSON_HEDLEY_CONSTEXPR)
#undef JSON_HEDLEY_CONSTEXPR
#endif
#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define JSON_HEDLEY_CONSTEXPR JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(constexpr)
#endif
#endif
#if !defined(JSON_HEDLEY_CONSTEXPR)
#define JSON_HEDLEY_CONSTEXPR
#endif
#if defined(JSON_HEDLEY_PREDICT)
#undef JSON_HEDLEY_PREDICT
#endif
#if defined(JSON_HEDLEY_LIKELY)
#undef JSON_HEDLEY_LIKELY
#endif
#if defined(JSON_HEDLEY_UNLIKELY)
#undef JSON_HEDLEY_UNLIKELY
#endif
#if defined(JSON_HEDLEY_UNPREDICTABLE)
#undef JSON_HEDLEY_UNPREDICTABLE
#endif
#if JSON_HEDLEY_HAS_BUILTIN(__builtin_unpredictable)
#define JSON_HEDLEY_UNPREDICTABLE(expr) __builtin_unpredictable((expr))
#endif
#if \
  (JSON_HEDLEY_HAS_BUILTIN(__builtin_expect_with_probability) && !defined(JSON_HEDLEY_PGI_VERSION)) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(9,0,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PREDICT(expr, value, probability) __builtin_expect_with_probability(  (expr), (value), (probability))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability)   __builtin_expect_with_probability(!!(expr),    1   , (probability))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability)  __builtin_expect_with_probability(!!(expr),    0   , (probability))
#  define JSON_HEDLEY_LIKELY(expr)                      __builtin_expect                 (!!(expr),    1                  )
#  define JSON_HEDLEY_UNLIKELY(expr)                    __builtin_expect                 (!!(expr),    0                  )
#elif \
  (JSON_HEDLEY_HAS_BUILTIN(__builtin_expect) && !defined(JSON_HEDLEY_INTEL_CL_VERSION)) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,27) || \
  JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PREDICT(expr, expected, probability) \
    (((probability) >= 0.9) ? __builtin_expect((expr), (expected)) : (JSON_HEDLEY_STATIC_CAST(void, expected), (expr)))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability) \
    (__extension__ ({ \
        double hedley_probability_ = (probability); \
        ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 1) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 0) : !!(expr))); \
    }))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability) \
    (__extension__ ({ \
        double hedley_probability_ = (probability); \
        ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 0) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 1) : !!(expr))); \
    }))
#  define JSON_HEDLEY_LIKELY(expr)   __builtin_expect(!!(expr), 1)
#  define JSON_HEDLEY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#  define JSON_HEDLEY_PREDICT(expr, expected, probability) (JSON_HEDLEY_STATIC_CAST(void, expected), (expr))
#  define JSON_HEDLEY_PREDICT_TRUE(expr, probability) (!!(expr))
#  define JSON_HEDLEY_PREDICT_FALSE(expr, probability) (!!(expr))
#  define JSON_HEDLEY_LIKELY(expr) (!!(expr))
#  define JSON_HEDLEY_UNLIKELY(expr) (!!(expr))
#endif
#if !defined(JSON_HEDLEY_UNPREDICTABLE)
#define JSON_HEDLEY_UNPREDICTABLE(expr) JSON_HEDLEY_PREDICT(expr, 1, 0.5)
#endif
#if defined(JSON_HEDLEY_MALLOC)
#undef JSON_HEDLEY_MALLOC
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(malloc) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_MALLOC __attribute__((__malloc__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#define JSON_HEDLEY_MALLOC _Pragma("returns_new_memory")
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_MALLOC __declspec(restrict)
#else
#define JSON_HEDLEY_MALLOC
#endif
#if defined(JSON_HEDLEY_PURE)
#undef JSON_HEDLEY_PURE
#endif
#if \
  JSON_HEDLEY_HAS_ATTRIBUTE(pure) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(2,96,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define JSON_HEDLEY_PURE __attribute__((__pure__))
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define JSON_HEDLEY_PURE _Pragma("does_not_write_global_data")
#elif defined(__cplusplus) && \
    ( \
      JSON_HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
      JSON_HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0) || \
      JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) \
    )
#  define JSON_HEDLEY_PURE _Pragma("FUNC_IS_PURE;")
#else
#  define JSON_HEDLEY_PURE
#endif
#if defined(JSON_HEDLEY_CONST)
#undef JSON_HEDLEY_CONST
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(const) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(2,5,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_CONST __attribute__((__const__))
#elif \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#define JSON_HEDLEY_CONST _Pragma("no_side_effect")
#else
#define JSON_HEDLEY_CONST JSON_HEDLEY_PURE
#endif
#if defined(JSON_HEDLEY_RESTRICT)
#undef JSON_HEDLEY_RESTRICT
#endif
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && !defined(__cplusplus)
#define JSON_HEDLEY_RESTRICT restrict
#elif \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,4) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
    defined(__clang__) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_RESTRICT __restrict
#elif JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,3,0) && !defined(__cplusplus)
#define JSON_HEDLEY_RESTRICT _Restrict
#else
#define JSON_HEDLEY_RESTRICT
#endif
#if defined(JSON_HEDLEY_INLINE)
#undef JSON_HEDLEY_INLINE
#endif
#if \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
    (defined(__cplusplus) && (__cplusplus >= 199711L))
#define JSON_HEDLEY_INLINE inline
#elif \
    defined(JSON_HEDLEY_GCC_VERSION) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(6,2,0)
#define JSON_HEDLEY_INLINE __inline__
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,1,0) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_INLINE __inline
#else
#define JSON_HEDLEY_INLINE
#endif
#if defined(JSON_HEDLEY_ALWAYS_INLINE)
#undef JSON_HEDLEY_ALWAYS_INLINE
#endif
#if \
  JSON_HEDLEY_HAS_ATTRIBUTE(always_inline) || \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
  JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define JSON_HEDLEY_ALWAYS_INLINE __attribute__((__always_inline__)) JSON_HEDLEY_INLINE
#elif \
  JSON_HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_ALWAYS_INLINE __forceinline
#elif defined(__cplusplus) && \
    ( \
      JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
      JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
      JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
      JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
      JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
      JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) \
    )
#  define JSON_HEDLEY_ALWAYS_INLINE _Pragma("FUNC_ALWAYS_INLINE;")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define JSON_HEDLEY_ALWAYS_INLINE _Pragma("inline=forced")
#else
#  define JSON_HEDLEY_ALWAYS_INLINE JSON_HEDLEY_INLINE
#endif
#if defined(JSON_HEDLEY_NEVER_INLINE)
#undef JSON_HEDLEY_NEVER_INLINE
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(noinline) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
    JSON_HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    JSON_HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    JSON_HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
    JSON_HEDLEY_IAR_VERSION_CHECK(8,10,0)
#define JSON_HEDLEY_NEVER_INLINE __attribute__((__noinline__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_NEVER_INLINE __declspec(noinline)
#elif JSON_HEDLEY_PGI_VERSION_CHECK(10,2,0)
#define JSON_HEDLEY_NEVER_INLINE _Pragma("noinline")
#elif JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
#define JSON_HEDLEY_NEVER_INLINE _Pragma("FUNC_CANNOT_INLINE;")
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#define JSON_HEDLEY_NEVER_INLINE _Pragma("inline=never")
#elif JSON_HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
#define JSON_HEDLEY_NEVER_INLINE __attribute((noinline))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(9,0,0)
#define JSON_HEDLEY_NEVER_INLINE __declspec(noinline)
#else
#define JSON_HEDLEY_NEVER_INLINE
#endif
#if defined(JSON_HEDLEY_PRIVATE)
#undef JSON_HEDLEY_PRIVATE
#endif
#if defined(JSON_HEDLEY_PUBLIC)
#undef JSON_HEDLEY_PUBLIC
#endif
#if defined(JSON_HEDLEY_IMPORT)
#undef JSON_HEDLEY_IMPORT
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#  define JSON_HEDLEY_PRIVATE
#  define JSON_HEDLEY_PUBLIC   __declspec(dllexport)
#  define JSON_HEDLEY_IMPORT   __declspec(dllimport)
#else
#  if \
    JSON_HEDLEY_HAS_ATTRIBUTE(visibility) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    ( \
      defined(__TI_EABI__) && \
      ( \
        (JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
        JSON_HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) \
      ) \
    ) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#    define JSON_HEDLEY_PRIVATE __attribute__((__visibility__("hidden")))
#    define JSON_HEDLEY_PUBLIC  __attribute__((__visibility__("default")))
#  else
#    define JSON_HEDLEY_PRIVATE
#    define JSON_HEDLEY_PUBLIC
#  endif
#  define JSON_HEDLEY_IMPORT    extern
#endif
#if defined(JSON_HEDLEY_NO_THROW)
#undef JSON_HEDLEY_NO_THROW
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(nothrow) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_NO_THROW __attribute__((__nothrow__))
#elif \
    JSON_HEDLEY_MSVC_VERSION_CHECK(13,1,0) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0)
#define JSON_HEDLEY_NO_THROW __declspec(nothrow)
#else
#define JSON_HEDLEY_NO_THROW
#endif
#if defined(JSON_HEDLEY_FALL_THROUGH)
#undef JSON_HEDLEY_FALL_THROUGH
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(fallthrough) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(7,0,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_FALL_THROUGH __attribute__((__fallthrough__))
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS(clang,fallthrough)
#define JSON_HEDLEY_FALL_THROUGH JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[clang::fallthrough]])
#elif JSON_HEDLEY_HAS_CPP_ATTRIBUTE(fallthrough)
#define JSON_HEDLEY_FALL_THROUGH JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[fallthrough]])
#elif defined(__fallthrough) 
#define JSON_HEDLEY_FALL_THROUGH __fallthrough
#else
#define JSON_HEDLEY_FALL_THROUGH
#endif
#if defined(JSON_HEDLEY_RETURNS_NON_NULL)
#undef JSON_HEDLEY_RETURNS_NON_NULL
#endif
#if \
    JSON_HEDLEY_HAS_ATTRIBUTE(returns_nonnull) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_RETURNS_NON_NULL __attribute__((__returns_nonnull__))
#elif defined(_Ret_notnull_) 
#define JSON_HEDLEY_RETURNS_NON_NULL _Ret_notnull_
#else
#define JSON_HEDLEY_RETURNS_NON_NULL
#endif
#if defined(JSON_HEDLEY_ARRAY_PARAM)
#undef JSON_HEDLEY_ARRAY_PARAM
#endif
#if \
    defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
    !defined(__STDC_NO_VLA__) && \
    !defined(__cplusplus) && \
    !defined(JSON_HEDLEY_PGI_VERSION) && \
    !defined(JSON_HEDLEY_TINYC_VERSION)
#define JSON_HEDLEY_ARRAY_PARAM(name) (name)
#else
#define JSON_HEDLEY_ARRAY_PARAM(name)
#endif
#if defined(JSON_HEDLEY_IS_CONSTANT)
#undef JSON_HEDLEY_IS_CONSTANT
#endif
#if defined(JSON_HEDLEY_REQUIRE_CONSTEXPR)
#undef JSON_HEDLEY_REQUIRE_CONSTEXPR
#endif
#if defined(JSON_HEDLEY_IS_CONSTEXPR_)
#undef JSON_HEDLEY_IS_CONSTEXPR_
#endif
#if \
    JSON_HEDLEY_HAS_BUILTIN(__builtin_constant_p) || \
    JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
    JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,19) || \
    JSON_HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    JSON_HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
    (JSON_HEDLEY_SUNPRO_VERSION_CHECK(5,10,0) && !defined(__cplusplus)) || \
    JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
    JSON_HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#define JSON_HEDLEY_IS_CONSTANT(expr) __builtin_constant_p(expr)
#endif
#if !defined(__cplusplus)
#  if \
       JSON_HEDLEY_HAS_BUILTIN(__builtin_types_compatible_p) || \
       JSON_HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
       JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
       JSON_HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
       JSON_HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
       JSON_HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
       JSON_HEDLEY_TINYC_VERSION_CHECK(0,9,24)
#if defined(__INTPTR_TYPE__)
#define JSON_HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0)), int*)
#else
#include <stdint.h>
#define JSON_HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((intptr_t) ((expr) * 0)) : (int*) 0)), int*)
#endif
#  elif \
       ( \
          defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
          !defined(JSON_HEDLEY_SUNPRO_VERSION) && \
          !defined(JSON_HEDLEY_PGI_VERSION) && \
          !defined(JSON_HEDLEY_IAR_VERSION)) || \
       (JSON_HEDLEY_HAS_EXTENSION(c_generic_selections) && !defined(JSON_HEDLEY_IAR_VERSION)) || \
       JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
       JSON_HEDLEY_INTEL_VERSION_CHECK(17,0,0) || \
       JSON_HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
       JSON_HEDLEY_ARM_VERSION_CHECK(5,3,0)
#if defined(__INTPTR_TYPE__)
#define JSON_HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0), int*: 1, void*: 0)
#else
#include <stdint.h>
#define JSON_HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((intptr_t) * 0) : (int*) 0), int*: 1, void*: 0)
#endif
#  elif \
       defined(JSON_HEDLEY_GCC_VERSION) || \
       defined(JSON_HEDLEY_INTEL_VERSION) || \
       defined(JSON_HEDLEY_TINYC_VERSION) || \
       defined(JSON_HEDLEY_TI_ARMCL_VERSION) || \
       JSON_HEDLEY_TI_CL430_VERSION_CHECK(18,12,0) || \
       defined(JSON_HEDLEY_TI_CL2000_VERSION) || \
       defined(JSON_HEDLEY_TI_CL6X_VERSION) || \
       defined(JSON_HEDLEY_TI_CL7X_VERSION) || \
       defined(JSON_HEDLEY_TI_CLPRU_VERSION) || \
       defined(__clang__)
#    define JSON_HEDLEY_IS_CONSTEXPR_(expr) ( \
        sizeof(void) != \
        sizeof(*( \
                  1 ? \
                  ((void*) ((expr) * 0L) ) : \
((struct { char v[sizeof(void) * 2]; } *) 1) \
                ) \
              ) \
                                            )
#  endif
#endif
#if defined(JSON_HEDLEY_IS_CONSTEXPR_)
#if !defined(JSON_HEDLEY_IS_CONSTANT)
#define JSON_HEDLEY_IS_CONSTANT(expr) JSON_HEDLEY_IS_CONSTEXPR_(expr)
#endif
#define JSON_HEDLEY_REQUIRE_CONSTEXPR(expr) (JSON_HEDLEY_IS_CONSTEXPR_(expr) ? (expr) : (-1))
#else
#if !defined(JSON_HEDLEY_IS_CONSTANT)
#define JSON_HEDLEY_IS_CONSTANT(expr) (0)
#endif
#define JSON_HEDLEY_REQUIRE_CONSTEXPR(expr) (expr)
#endif
#if defined(JSON_HEDLEY_BEGIN_C_DECLS)
#undef JSON_HEDLEY_BEGIN_C_DECLS
#endif
#if defined(JSON_HEDLEY_END_C_DECLS)
#undef JSON_HEDLEY_END_C_DECLS
#endif
#if defined(JSON_HEDLEY_C_DECL)
#undef JSON_HEDLEY_C_DECL
#endif
#if defined(__cplusplus)
#define JSON_HEDLEY_BEGIN_C_DECLS extern "C" {
#define JSON_HEDLEY_END_C_DECLS }
#define JSON_HEDLEY_C_DECL extern "C"
#else
#define JSON_HEDLEY_BEGIN_C_DECLS
#define JSON_HEDLEY_END_C_DECLS
#define JSON_HEDLEY_C_DECL
#endif
#if defined(JSON_HEDLEY_STATIC_ASSERT)
#undef JSON_HEDLEY_STATIC_ASSERT
#endif
#if \
  !defined(__cplusplus) && ( \
      (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
      (JSON_HEDLEY_HAS_FEATURE(c_static_assert) && !defined(JSON_HEDLEY_INTEL_CL_VERSION)) || \
      JSON_HEDLEY_GCC_VERSION_CHECK(6,0,0) || \
      JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      defined(_Static_assert) \
    )
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif \
  (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
  JSON_HEDLEY_MSVC_VERSION_CHECK(16,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message) JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(static_assert(expr, message))
#else
#  define JSON_HEDLEY_STATIC_ASSERT(expr, message)
#endif
#if defined(JSON_HEDLEY_NULL)
#undef JSON_HEDLEY_NULL
#endif
#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define JSON_HEDLEY_NULL JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(nullptr)
#elif defined(NULL)
#define JSON_HEDLEY_NULL NULL
#else
#define JSON_HEDLEY_NULL JSON_HEDLEY_STATIC_CAST(void*, 0)
#endif
#elif defined(NULL)
#define JSON_HEDLEY_NULL NULL
#else
#define JSON_HEDLEY_NULL ((void*) 0)
#endif
#if defined(JSON_HEDLEY_MESSAGE)
#undef JSON_HEDLEY_MESSAGE
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define JSON_HEDLEY_MESSAGE(msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
    JSON_HEDLEY_PRAGMA(message msg) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#elif \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,4,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message msg)
#elif JSON_HEDLEY_CRAY_VERSION_CHECK(5,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(_CRI message msg)
#elif JSON_HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message(msg))
#elif JSON_HEDLEY_PELLES_VERSION_CHECK(2,0,0)
#  define JSON_HEDLEY_MESSAGE(msg) JSON_HEDLEY_PRAGMA(message(msg))
#else
#  define JSON_HEDLEY_MESSAGE(msg)
#endif
#if defined(JSON_HEDLEY_WARNING)
#undef JSON_HEDLEY_WARNING
#endif
#if JSON_HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define JSON_HEDLEY_WARNING(msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
    JSON_HEDLEY_PRAGMA(clang warning msg) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#elif \
  JSON_HEDLEY_GCC_VERSION_CHECK(4,8,0) || \
  JSON_HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
  JSON_HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_PRAGMA(GCC warning msg)
#elif \
  JSON_HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
  JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_PRAGMA(message(msg))
#else
#  define JSON_HEDLEY_WARNING(msg) JSON_HEDLEY_MESSAGE(msg)
#endif
#if defined(JSON_HEDLEY_REQUIRE)
#undef JSON_HEDLEY_REQUIRE
#endif
#if defined(JSON_HEDLEY_REQUIRE_MSG)
#undef JSON_HEDLEY_REQUIRE_MSG
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(diagnose_if)
#  if JSON_HEDLEY_HAS_WARNING("-Wgcc-compat")
#    define JSON_HEDLEY_REQUIRE(expr) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
    __attribute__((diagnose_if(!(expr), #expr, "error"))) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#    define JSON_HEDLEY_REQUIRE_MSG(expr,msg) \
    JSON_HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
    __attribute__((diagnose_if(!(expr), msg, "error"))) \
    JSON_HEDLEY_DIAGNOSTIC_POP
#  else
#    define JSON_HEDLEY_REQUIRE(expr) __attribute__((diagnose_if(!(expr), #expr, "error")))
#    define JSON_HEDLEY_REQUIRE_MSG(expr,msg) __attribute__((diagnose_if(!(expr), msg, "error")))
#  endif
#else
#  define JSON_HEDLEY_REQUIRE(expr)
#  define JSON_HEDLEY_REQUIRE_MSG(expr,msg)
#endif
#if defined(JSON_HEDLEY_FLAGS)
#undef JSON_HEDLEY_FLAGS
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(flag_enum) && (!defined(__cplusplus) || JSON_HEDLEY_HAS_WARNING("-Wbitfield-enum-conversion"))
#define JSON_HEDLEY_FLAGS __attribute__((__flag_enum__))
#else
#define JSON_HEDLEY_FLAGS
#endif
#if defined(JSON_HEDLEY_FLAGS_CAST)
#undef JSON_HEDLEY_FLAGS_CAST
#endif
#if JSON_HEDLEY_INTEL_VERSION_CHECK(19,0,0)
#  define JSON_HEDLEY_FLAGS_CAST(T, expr) (__extension__ ({ \
        JSON_HEDLEY_DIAGNOSTIC_PUSH \
        _Pragma("warning(disable:188)") \
        ((T) (expr)); \
        JSON_HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define JSON_HEDLEY_FLAGS_CAST(T, expr) JSON_HEDLEY_STATIC_CAST(T, expr)
#endif
#if defined(JSON_HEDLEY_EMPTY_BASES)
#undef JSON_HEDLEY_EMPTY_BASES
#endif
#if \
    (JSON_HEDLEY_MSVC_VERSION_CHECK(19,0,23918) && !JSON_HEDLEY_MSVC_VERSION_CHECK(20,0,0)) || \
    JSON_HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#define JSON_HEDLEY_EMPTY_BASES __declspec(empty_bases)
#else
#define JSON_HEDLEY_EMPTY_BASES
#endif
#if defined(JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK)
#undef JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK
#endif
#if defined(__clang__)
#define JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) (0)
#else
#define JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) JSON_HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif
#if defined(JSON_HEDLEY_CLANG_HAS_ATTRIBUTE)
#undef JSON_HEDLEY_CLANG_HAS_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_ATTRIBUTE(attribute)
#if defined(JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE)
#undef JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_CPP_ATTRIBUTE(attribute)
#if defined(JSON_HEDLEY_CLANG_HAS_BUILTIN)
#undef JSON_HEDLEY_CLANG_HAS_BUILTIN
#endif
#define JSON_HEDLEY_CLANG_HAS_BUILTIN(builtin) JSON_HEDLEY_HAS_BUILTIN(builtin)
#if defined(JSON_HEDLEY_CLANG_HAS_FEATURE)
#undef JSON_HEDLEY_CLANG_HAS_FEATURE
#endif
#define JSON_HEDLEY_CLANG_HAS_FEATURE(feature) JSON_HEDLEY_HAS_FEATURE(feature)
#if defined(JSON_HEDLEY_CLANG_HAS_EXTENSION)
#undef JSON_HEDLEY_CLANG_HAS_EXTENSION
#endif
#define JSON_HEDLEY_CLANG_HAS_EXTENSION(extension) JSON_HEDLEY_HAS_EXTENSION(extension)
#if defined(JSON_HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE)
#undef JSON_HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE
#endif
#define JSON_HEDLEY_CLANG_HAS_DECLSPEC_ATTRIBUTE(attribute) JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute)
#if defined(JSON_HEDLEY_CLANG_HAS_WARNING)
#undef JSON_HEDLEY_CLANG_HAS_WARNING
#endif
#define JSON_HEDLEY_CLANG_HAS_WARNING(warning) JSON_HEDLEY_HAS_WARNING(warning)
#endif 
#if !defined(JSON_SKIP_UNSUPPORTED_COMPILER_CHECK)
#if defined(__clang__)
#if (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__) < 30400
#error "unsupported Clang version - see https:
#endif
#elif defined(__GNUC__) && !(defined(__ICC) || defined(__INTEL_COMPILER))
#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40800
#error "unsupported GCC version - see https:
#endif
#endif
#endif
#if !defined(JSON_HAS_CPP_20) && !defined(JSON_HAS_CPP_17) && !defined(JSON_HAS_CPP_14) && !defined(JSON_HAS_CPP_11)
#if (defined(__cplusplus) && __cplusplus >= 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define JSON_HAS_CPP_20
#define JSON_HAS_CPP_17
#define JSON_HAS_CPP_14
#elif (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_HAS_CXX17) && _HAS_CXX17 == 1) 
#define JSON_HAS_CPP_17
#define JSON_HAS_CPP_14
#elif (defined(__cplusplus) && __cplusplus >= 201402L) || (defined(_HAS_CXX14) && _HAS_CXX14 == 1)
#define JSON_HAS_CPP_14
#endif
#define JSON_HAS_CPP_11
#endif
#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif
#if !defined(JSON_HAS_FILESYSTEM) && !defined(JSON_HAS_EXPERIMENTAL_FILESYSTEM)
#ifdef JSON_HAS_CPP_17
#if defined(__cpp_lib_filesystem)
#define JSON_HAS_FILESYSTEM 1
#elif defined(__cpp_lib_experimental_filesystem)
#define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
#elif !defined(__has_include)
#define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
#elif __has_include(<filesystem>)
#define JSON_HAS_FILESYSTEM 1
#elif __has_include(<experimental/filesystem>)
#define JSON_HAS_EXPERIMENTAL_FILESYSTEM 1
#endif
#if defined(__MINGW32__) && defined(__GNUC__) && __GNUC__ == 8
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#if defined(__clang_major__) && __clang_major__ < 7
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#if defined(_MSC_VER) && _MSC_VER < 1914
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#endif
#endif
#endif
#ifndef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#define JSON_HAS_EXPERIMENTAL_FILESYSTEM 0
#endif
#ifndef JSON_HAS_FILESYSTEM
#define JSON_HAS_FILESYSTEM 0
#endif
#ifndef JSON_HAS_THREE_WAY_COMPARISON
#if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907L \
        && defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
#define JSON_HAS_THREE_WAY_COMPARISON 1
#else
#define JSON_HAS_THREE_WAY_COMPARISON 0
#endif
#endif
#ifndef JSON_HAS_RANGES
#if defined(__GLIBCXX__) && __GLIBCXX__ == 20210427
#define JSON_HAS_RANGES 0
#elif defined(__cpp_lib_ranges)
#define JSON_HAS_RANGES 1
#else
#define JSON_HAS_RANGES 0
#endif
#endif
#ifndef JSON_HAS_STATIC_RTTI
#if !defined(_HAS_STATIC_RTTI) || _HAS_STATIC_RTTI != 0
#define JSON_HAS_STATIC_RTTI 1
#else
#define JSON_HAS_STATIC_RTTI 0
#endif
#endif
#ifdef JSON_HAS_CPP_17
#define JSON_INLINE_VARIABLE inline
#else
#define JSON_INLINE_VARIABLE
#endif
#if JSON_HEDLEY_HAS_ATTRIBUTE(no_unique_address)
#define JSON_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define JSON_NO_UNIQUE_ADDRESS
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#endif
#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(JSON_NOEXCEPTION)
#define JSON_THROW(exception) throw exception
#define JSON_TRY try
#define JSON_CATCH(exception) catch(exception)
#define JSON_INTERNAL_CATCH(exception) catch(exception)
#else
#include <cstdlib>
#define JSON_THROW(exception) std::abort()
#define JSON_TRY if(true)
#define JSON_CATCH(exception) if(false)
#define JSON_INTERNAL_CATCH(exception) if(false)
#endif
#if defined(JSON_THROW_USER)
#undef JSON_THROW
#define JSON_THROW JSON_THROW_USER
#endif
#if defined(JSON_TRY_USER)
#undef JSON_TRY
#define JSON_TRY JSON_TRY_USER
#endif
#if defined(JSON_CATCH_USER)
#undef JSON_CATCH
#define JSON_CATCH JSON_CATCH_USER
#undef JSON_INTERNAL_CATCH
#define JSON_INTERNAL_CATCH JSON_CATCH_USER
#endif
#if defined(JSON_INTERNAL_CATCH_USER)
#undef JSON_INTERNAL_CATCH
#define JSON_INTERNAL_CATCH JSON_INTERNAL_CATCH_USER
#endif
#if !defined(JSON_ASSERT)
#include <cassert> 
#define JSON_ASSERT(x) assert(x)
#endif
#if defined(JSON_TESTS_PRIVATE)
#define JSON_PRIVATE_UNLESS_TESTED public
#else
#define JSON_PRIVATE_UNLESS_TESTED private
#endif
#define NLOHMANN_JSON_SERIALIZE_ENUM(ENUM_TYPE, ...)                                            \
    template<typename BasicJsonType>                                                            \
    inline void to_json(BasicJsonType& j, const ENUM_TYPE& e)                                   \
    {                                                                                           \
        static_assert(std::is_enum<ENUM_TYPE>::value, #ENUM_TYPE " must be an enum!");          \
        static const std::pair<ENUM_TYPE, BasicJsonType> m[] = __VA_ARGS__;                     \
        auto it = std::find_if(std::begin(m), std::end(m),                                      \
                               [e](const std::pair<ENUM_TYPE, BasicJsonType>& ej_pair) -> bool  \
        {                                                                                       \
            return ej_pair.first == e;                                                          \
        });                                                                                     \
        j = ((it != std::end(m)) ? it : std::begin(m))->second;                                 \
    }                                                                                           \
    template<typename BasicJsonType>                                                            \
    inline void from_json(const BasicJsonType& j, ENUM_TYPE& e)                                 \
    {                                                                                           \
        static_assert(std::is_enum<ENUM_TYPE>::value, #ENUM_TYPE " must be an enum!");          \
        static const std::pair<ENUM_TYPE, BasicJsonType> m[] = __VA_ARGS__;                     \
        auto it = std::find_if(std::begin(m), std::end(m),                                      \
                               [&j](const std::pair<ENUM_TYPE, BasicJsonType>& ej_pair) -> bool \
        {                                                                                       \
            return ej_pair.second == j;                                                         \
        });                                                                                     \
        e = ((it != std::end(m)) ? it : std::begin(m))->first;                                  \
    }
#define NLOHMANN_BASIC_JSON_TPL_DECLARATION                                \
    template<template<typename, typename, typename...> class ObjectType,   \
             template<typename, typename...> class ArrayType,              \
             class StringType, class BooleanType, class NumberIntegerType, \
             class NumberUnsignedType, class NumberFloatType,              \
             template<typename> class AllocatorType,                       \
             template<typename, typename = void> class JSONSerializer,     \
             class BinaryType,                                             \
             class CustomBaseClass>
#define NLOHMANN_BASIC_JSON_TPL                                            \
    basic_json<ObjectType, ArrayType, StringType, BooleanType,             \
    NumberIntegerType, NumberUnsignedType, NumberFloatType,                \
    AllocatorType, JSONSerializer, BinaryType, CustomBaseClass>
#define NLOHMANN_JSON_EXPAND( x ) x
#define NLOHMANN_JSON_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME
#define NLOHMANN_JSON_PASTE(...) NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_GET_MACRO(__VA_ARGS__, \
        NLOHMANN_JSON_PASTE64, \
        NLOHMANN_JSON_PASTE63, \
        NLOHMANN_JSON_PASTE62, \
        NLOHMANN_JSON_PASTE61, \
        NLOHMANN_JSON_PASTE60, \
        NLOHMANN_JSON_PASTE59, \
        NLOHMANN_JSON_PASTE58, \
        NLOHMANN_JSON_PASTE57, \
        NLOHMANN_JSON_PASTE56, \
        NLOHMANN_JSON_PASTE55, \
        NLOHMANN_JSON_PASTE54, \
        NLOHMANN_JSON_PASTE53, \
        NLOHMANN_JSON_PASTE52, \
        NLOHMANN_JSON_PASTE51, \
        NLOHMANN_JSON_PASTE50, \
        NLOHMANN_JSON_PASTE49, \
        NLOHMANN_JSON_PASTE48, \
        NLOHMANN_JSON_PASTE47, \
        NLOHMANN_JSON_PASTE46, \
        NLOHMANN_JSON_PASTE45, \
        NLOHMANN_JSON_PASTE44, \
        NLOHMANN_JSON_PASTE43, \
        NLOHMANN_JSON_PASTE42, \
        NLOHMANN_JSON_PASTE41, \
        NLOHMANN_JSON_PASTE40, \
        NLOHMANN_JSON_PASTE39, \
        NLOHMANN_JSON_PASTE38, \
        NLOHMANN_JSON_PASTE37, \
        NLOHMANN_JSON_PASTE36, \
        NLOHMANN_JSON_PASTE35, \
        NLOHMANN_JSON_PASTE34, \
        NLOHMANN_JSON_PASTE33, \
        NLOHMANN_JSON_PASTE32, \
        NLOHMANN_JSON_PASTE31, \
        NLOHMANN_JSON_PASTE30, \
        NLOHMANN_JSON_PASTE29, \
        NLOHMANN_JSON_PASTE28, \
        NLOHMANN_JSON_PASTE27, \
        NLOHMANN_JSON_PASTE26, \
        NLOHMANN_JSON_PASTE25, \
        NLOHMANN_JSON_PASTE24, \
        NLOHMANN_JSON_PASTE23, \
        NLOHMANN_JSON_PASTE22, \
        NLOHMANN_JSON_PASTE21, \
        NLOHMANN_JSON_PASTE20, \
        NLOHMANN_JSON_PASTE19, \
        NLOHMANN_JSON_PASTE18, \
        NLOHMANN_JSON_PASTE17, \
        NLOHMANN_JSON_PASTE16, \
        NLOHMANN_JSON_PASTE15, \
        NLOHMANN_JSON_PASTE14, \
        NLOHMANN_JSON_PASTE13, \
        NLOHMANN_JSON_PASTE12, \
        NLOHMANN_JSON_PASTE11, \
        NLOHMANN_JSON_PASTE10, \
        NLOHMANN_JSON_PASTE9, \
        NLOHMANN_JSON_PASTE8, \
        NLOHMANN_JSON_PASTE7, \
        NLOHMANN_JSON_PASTE6, \
        NLOHMANN_JSON_PASTE5, \
        NLOHMANN_JSON_PASTE4, \
        NLOHMANN_JSON_PASTE3, \
        NLOHMANN_JSON_PASTE2, \
        NLOHMANN_JSON_PASTE1)(__VA_ARGS__))
#define NLOHMANN_JSON_PASTE2(func, v1) func(v1)
#define NLOHMANN_JSON_PASTE3(func, v1, v2) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE2(func, v2)
#define NLOHMANN_JSON_PASTE4(func, v1, v2, v3) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE3(func, v2, v3)
#define NLOHMANN_JSON_PASTE5(func, v1, v2, v3, v4) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE4(func, v2, v3, v4)
#define NLOHMANN_JSON_PASTE6(func, v1, v2, v3, v4, v5) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE5(func, v2, v3, v4, v5)
#define NLOHMANN_JSON_PASTE7(func, v1, v2, v3, v4, v5, v6) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE6(func, v2, v3, v4, v5, v6)
#define NLOHMANN_JSON_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define NLOHMANN_JSON_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define NLOHMANN_JSON_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define NLOHMANN_JSON_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define NLOHMANN_JSON_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define NLOHMANN_JSON_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define NLOHMANN_JSON_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define NLOHMANN_JSON_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define NLOHMANN_JSON_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define NLOHMANN_JSON_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define NLOHMANN_JSON_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define NLOHMANN_JSON_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define NLOHMANN_JSON_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define NLOHMANN_JSON_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define NLOHMANN_JSON_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define NLOHMANN_JSON_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define NLOHMANN_JSON_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define NLOHMANN_JSON_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define NLOHMANN_JSON_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define NLOHMANN_JSON_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define NLOHMANN_JSON_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define NLOHMANN_JSON_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define NLOHMANN_JSON_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define NLOHMANN_JSON_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define NLOHMANN_JSON_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define NLOHMANN_JSON_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define NLOHMANN_JSON_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define NLOHMANN_JSON_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define NLOHMANN_JSON_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define NLOHMANN_JSON_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define NLOHMANN_JSON_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define NLOHMANN_JSON_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define NLOHMANN_JSON_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define NLOHMANN_JSON_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define NLOHMANN_JSON_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define NLOHMANN_JSON_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define NLOHMANN_JSON_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define NLOHMANN_JSON_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define NLOHMANN_JSON_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define NLOHMANN_JSON_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define NLOHMANN_JSON_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define NLOHMANN_JSON_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define NLOHMANN_JSON_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define NLOHMANN_JSON_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define NLOHMANN_JSON_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define NLOHMANN_JSON_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define NLOHMANN_JSON_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define NLOHMANN_JSON_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define NLOHMANN_JSON_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define NLOHMANN_JSON_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define NLOHMANN_JSON_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define NLOHMANN_JSON_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define NLOHMANN_JSON_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define NLOHMANN_JSON_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define NLOHMANN_JSON_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define NLOHMANN_JSON_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define NLOHMANN_JSON_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)
#define NLOHMANN_JSON_TO(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#define NLOHMANN_JSON_FROM(v1) nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1);
#define NLOHMANN_JSON_FROM_WITH_DEFAULT(v1) nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1);
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, ...)  \
    friend void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    friend void from_json(const JsonLib::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
    friend void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    friend void from_json(const JsonLib::json& nlohmann_json_j, Type& nlohmann_json_t) { const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, ...)  \
    friend void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)  \
    inline void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    inline void from_json(const JsonLib::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, ...)  \
    inline void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
    inline void to_json(JsonLib::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    inline void from_json(const JsonLib::json& nlohmann_json_j, Type& nlohmann_json_t) { const Type nlohmann_json_default_obj{}; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }
#define NLOHMANN_CAN_CALL_STD_FUNC_IMPL(std_name)                                 \
    namespace detail {                                                            \
    using std::std_name;                                                          \
    \
    template<typename... T>                                                       \
    using result_of_##std_name = decltype(std_name(std::declval<T>()...));        \
    }                                                                             \
    \
    namespace detail2 {                                                           \
    struct std_name##_tag                                                         \
    {                                                                             \
    };                                                                            \
    \
    template<typename... T>                                                       \
    std_name##_tag std_name(T&&...);                                              \
    \
    template<typename... T>                                                       \
    using result_of_##std_name = decltype(std_name(std::declval<T>()...));        \
    \
    template<typename... T>                                                       \
    struct would_call_std_##std_name                                              \
    {                                                                             \
        static constexpr auto const value = ::JsonLib::detail::                  \
                                            is_detected_exact<std_name##_tag, result_of_##std_name, T...>::value; \
    };                                                                            \
    }  \
    \
    template<typename... T>                                                       \
    struct would_call_std_##std_name : detail2::would_call_std_##std_name<T...>   \
    {                                                                             \
    }
#ifndef JSON_USE_IMPLICIT_CONVERSIONS
#define JSON_USE_IMPLICIT_CONVERSIONS 1
#endif
#if JSON_USE_IMPLICIT_CONVERSIONS
#define JSON_EXPLICIT
#else
#define JSON_EXPLICIT explicit
#endif
#ifndef JSON_DISABLE_ENUM_SERIALIZATION
#define JSON_DISABLE_ENUM_SERIALIZATION 0
#endif
#ifndef JSON_USE_GLOBAL_UDLS
#define JSON_USE_GLOBAL_UDLS 1
#endif
#if JSON_HAS_THREE_WAY_COMPARISON
#include <compare> 
#endif
	NLOHMANN_JSON_NAMESPACE_BEGIN
	namespace detail
{
	enum class value_t : std::uint8_t
	{
		null,
		object,
		array,
		string,
		boolean,
		number_integer,
		number_unsigned,
		number_float,
		binary,
		discarded
	};
#if JSON_HAS_THREE_WAY_COMPARISON
	inline std::partial_ordering operator<=>(const value_t lhs, const value_t rhs) noexcept
#else
	inline bool operator<(const value_t lhs, const value_t rhs) noexcept
#endif
	{
		static constexpr std::array<std::uint8_t, 9> order = { {
				0 , 3 , 4 , 5 ,
				1 , 2 , 2 , 2 ,
				6
			}
		};
		const auto l_index = static_cast<std::size_t>(lhs);
		const auto r_index = static_cast<std::size_t>(rhs);
#if JSON_HAS_THREE_WAY_COMPARISON
		if (l_index < order.size() && r_index < order.size())
		{
			return order[l_index] <=> order[r_index];
		}
		return std::partial_ordering::unordered;
#else
		return l_index < order.size() && r_index < order.size() && order[l_index] < order[r_index];
#endif
	}
#if JSON_HAS_THREE_WAY_COMPARISON && defined(__GNUC__)
	inline bool operator<(const value_t lhs, const value_t rhs) noexcept
	{
		return std::is_lt(lhs <=> rhs);
	}
#endif
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename StringType>
	inline void replace_substring(StringType& s, const StringType& f,
		const StringType& t)
	{
		JSON_ASSERT(!f.empty());
		for (auto pos = s.find(f);
			pos != StringType::npos;
			s.replace(pos, f.size(), t),
			pos = s.find(f, pos + t.size()))
		{
		}
	}
	template<typename StringType>
	inline StringType escape(StringType s)
	{
		replace_substring(s, StringType{ "~" }, StringType{ "~0" });
		replace_substring(s, StringType{ "/" }, StringType{ "~1" });
		return s;
	}
	template<typename StringType>
	static void unescape(StringType& s)
	{
		replace_substring(s, StringType{ "~1" }, StringType{ "/" });
		replace_substring(s, StringType{ "~0" }, StringType{ "~" });
	}
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstddef> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	struct position_t
	{
		std::size_t chars_read_total = 0;
		std::size_t chars_read_current_line = 0;
		std::size_t lines_read = 0;
		constexpr operator size_t() const
		{
			return chars_read_total;
		}
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <array> 
#include <cstddef> 
#include <type_traits> 
#include <utility> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename T>
	using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
#ifdef JSON_HAS_CPP_14
	using std::enable_if_t;
	using std::index_sequence;
	using std::make_index_sequence;
	using std::index_sequence_for;
#else
	template<bool B, typename T = void>
	using enable_if_t = typename std::enable_if<B, T>::type;
	template <typename T, T... Ints>
	struct integer_sequence
	{
		using value_type = T;
		static constexpr std::size_t size() noexcept
		{
			return sizeof...(Ints);
		}
	};
	template <size_t... Ints>
	using index_sequence = integer_sequence<size_t, Ints...>;
	namespace utility_internal
	{
		template <typename Seq, size_t SeqSize, size_t Rem>
		struct Extend;
		template <typename T, T... Ints, size_t SeqSize>
		struct Extend<integer_sequence<T, Ints...>, SeqSize, 0>
		{
			using type = integer_sequence < T, Ints..., (Ints + SeqSize)... >;
		};
		template <typename T, T... Ints, size_t SeqSize>
		struct Extend<integer_sequence<T, Ints...>, SeqSize, 1>
		{
			using type = integer_sequence < T, Ints..., (Ints + SeqSize)..., 2 * SeqSize >;
		};
		template <typename T, size_t N>
		struct Gen
		{
			using type =
				typename Extend < typename Gen < T, N / 2 >::type, N / 2, N % 2 >::type;
		};
		template <typename T>
		struct Gen<T, 0>
		{
			using type = integer_sequence<T>;
		};
	}
	template <typename T, T N>
	using make_integer_sequence = typename utility_internal::Gen<T, N>::type;
	template <size_t N>
	using make_index_sequence = make_integer_sequence<size_t, N>;
	template <typename... Ts>
	using index_sequence_for = make_index_sequence<sizeof...(Ts)>;
#endif
	template<unsigned N> struct priority_tag : priority_tag < N - 1 > {};
	template<> struct priority_tag<0> {};
	template<typename T>
	struct static_const
	{
		static JSON_INLINE_VARIABLE constexpr T value{};
	};
#ifndef JSON_HAS_CPP_17
	template<typename T>
	constexpr T static_const<T>::value;
#endif
	template<typename T, typename... Args>
	inline constexpr std::array<T, sizeof...(Args)> make_array(Args&& ... args)
	{
		return std::array<T, sizeof...(Args)> {{static_cast<T>(std::forward<Args>(args))...}};
	}
}
NLOHMANN_JSON_NAMESPACE_END
#include <limits> 
#include <type_traits> 
#include <utility> 
#include <tuple> 
#include <string> 
#include <iterator> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename It, typename = void>
	struct iterator_types {};
	template<typename It>
	struct iterator_types <
		It,
		void_t<typename It::difference_type, typename It::value_type, typename It::pointer,
		typename It::reference, typename It::iterator_category >>
	{
		using difference_type = typename It::difference_type;
		using value_type = typename It::value_type;
		using pointer = typename It::pointer;
		using reference = typename It::reference;
		using iterator_category = typename It::iterator_category;
	};
	template<typename T, typename = void>
	struct iterator_traits
	{
	};
	template<typename T>
	struct iterator_traits < T, enable_if_t < !std::is_pointer<T>::value >>
		: iterator_types<T>
	{
	};
	template<typename T>
	struct iterator_traits<T*, enable_if_t<std::is_object<T>::value>>
	{
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = ptrdiff_t;
		using pointer = T*;
		using reference = T&;
	};
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
NLOHMANN_CAN_CALL_STD_FUNC_IMPL(begin);
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
NLOHMANN_CAN_CALL_STD_FUNC_IMPL(end);
NLOHMANN_JSON_NAMESPACE_END
#ifndef INCLUDE_NLOHMANN_JSON_FWD_HPP_
#define INCLUDE_NLOHMANN_JSON_FWD_HPP_
#include <cstdint> 
#include <map> 
#include <memory> 
#include <string> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename T = void, typename SFINAE = void>
struct adl_serializer;
template<template<typename U, typename V, typename... Args> class ObjectType =
	std::map,
	template<typename U, typename... Args> class ArrayType = std::vector,
	class StringType = std::string, class BooleanType = bool,
	class NumberIntegerType = std::int64_t,
	class NumberUnsignedType = std::uint64_t,
	class NumberFloatType = double,
	template<typename U> class AllocatorType = std::allocator,
	template<typename T, typename SFINAE = void> class JSONSerializer =
	adl_serializer,
	class BinaryType = std::vector<std::uint8_t>,
	class CustomBaseClass = void>
class basic_json;
template<typename RefStringType>
class json_pointer;
using json = basic_json<>;
template<class Key, class T, class IgnoredLess, class Allocator>
struct ordered_map;
using ordered_json = basic_json<JsonLib::ordered_map>;
NLOHMANN_JSON_NAMESPACE_END
#endif  
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename> struct is_basic_json : std::false_type {};
	NLOHMANN_BASIC_JSON_TPL_DECLARATION
		struct is_basic_json<NLOHMANN_BASIC_JSON_TPL> : std::true_type {};
	template<typename BasicJsonContext>
	struct is_basic_json_context :
		std::integral_constant < bool,
		is_basic_json<typename std::remove_cv<typename std::remove_pointer<BasicJsonContext>::type>::type>::value
		|| std::is_same<BasicJsonContext, std::nullptr_t>::value >
	{
	};
	template<typename>
	class json_ref;
	template<typename>
	struct is_json_ref : std::false_type {};
	template<typename T>
	struct is_json_ref<json_ref<T>> : std::true_type {};
	template<typename T>
	using mapped_type_t = typename T::mapped_type;
	template<typename T>
	using key_type_t = typename T::key_type;
	template<typename T>
	using value_type_t = typename T::value_type;
	template<typename T>
	using difference_type_t = typename T::difference_type;
	template<typename T>
	using pointer_t = typename T::pointer;
	template<typename T>
	using reference_t = typename T::reference;
	template<typename T>
	using iterator_category_t = typename T::iterator_category;
	template<typename T, typename... Args>
	using to_json_function = decltype(T::to_json(std::declval<Args>()...));
	template<typename T, typename... Args>
	using from_json_function = decltype(T::from_json(std::declval<Args>()...));
	template<typename T, typename U>
	using get_template_function = decltype(std::declval<T>().template get<U>());
	template<typename BasicJsonType, typename T, typename = void>
	struct has_from_json : std::false_type {};
	template <typename BasicJsonType, typename T>
	struct is_getable
	{
		static constexpr bool value = is_detected<get_template_function, const BasicJsonType&, T>::value;
	};
	template<typename BasicJsonType, typename T>
	struct has_from_json < BasicJsonType, T, enable_if_t < !is_basic_json<T>::value >>
	{
		using serializer = typename BasicJsonType::template json_serializer<T, void>;
		static constexpr bool value =
			is_detected_exact<void, from_json_function, serializer,
			const BasicJsonType&, T&>::value;
	};
	template<typename BasicJsonType, typename T, typename = void>
	struct has_non_default_from_json : std::false_type {};
	template<typename BasicJsonType, typename T>
	struct has_non_default_from_json < BasicJsonType, T, enable_if_t < !is_basic_json<T>::value >>
	{
		using serializer = typename BasicJsonType::template json_serializer<T, void>;
		static constexpr bool value =
			is_detected_exact<T, from_json_function, serializer,
			const BasicJsonType&>::value;
	};
	template<typename BasicJsonType, typename T, typename = void>
	struct has_to_json : std::false_type {};
	template<typename BasicJsonType, typename T>
	struct has_to_json < BasicJsonType, T, enable_if_t < !is_basic_json<T>::value >>
	{
		using serializer = typename BasicJsonType::template json_serializer<T, void>;
		static constexpr bool value =
			is_detected_exact<void, to_json_function, serializer, BasicJsonType&,
			T>::value;
	};
	template<typename T>
	using detect_key_compare = typename T::key_compare;
	template<typename T>
	struct has_key_compare : std::integral_constant<bool, is_detected<detect_key_compare, T>::value> {};
	template<typename BasicJsonType>
	struct actual_object_comparator
	{
		using object_t = typename BasicJsonType::object_t;
		using object_comparator_t = typename BasicJsonType::default_object_comparator_t;
		using type = typename std::conditional < has_key_compare<object_t>::value,
			typename object_t::key_compare, object_comparator_t>::type;
	};
	template<typename BasicJsonType>
	using actual_object_comparator_t = typename actual_object_comparator<BasicJsonType>::type;
	template<typename T>
	struct char_traits : std::char_traits<T>
	{
	};
	template<>
	struct char_traits<unsigned char> : std::char_traits<char>
	{
		using char_type = unsigned char;
		using int_type = uint64_t;
		static int_type to_int_type(char_type c) noexcept
		{
			return static_cast<int_type>(c);
		}
		static char_type to_char_type(int_type i) noexcept
		{
			return static_cast<char_type>(i);
		}
		static constexpr int_type eof() noexcept
		{
			return static_cast<int_type>(EOF);
		}
	};
	template<>
	struct char_traits<signed char> : std::char_traits<char>
	{
		using char_type = signed char;
		using int_type = uint64_t;
		static int_type to_int_type(char_type c) noexcept
		{
			return static_cast<int_type>(c);
		}
		static char_type to_char_type(int_type i) noexcept
		{
			return static_cast<char_type>(i);
		}
		static constexpr int_type eof() noexcept
		{
			return static_cast<int_type>(EOF);
		}
	};
	template<class...> struct conjunction : std::true_type {};
	template<class B> struct conjunction<B> : B {};
	template<class B, class... Bn>
	struct conjunction<B, Bn...>
		: std::conditional<static_cast<bool>(B::value), conjunction<Bn...>, B>::type {
	};
	template<class B> struct negation : std::integral_constant < bool, !B::value > {};
	template <typename T>
	struct is_default_constructible : std::is_default_constructible<T> {};
	template <typename T1, typename T2>
	struct is_default_constructible<std::pair<T1, T2>>
		: conjunction<is_default_constructible<T1>, is_default_constructible<T2>> {
	};
	template <typename T1, typename T2>
	struct is_default_constructible<const std::pair<T1, T2>>
		: conjunction<is_default_constructible<T1>, is_default_constructible<T2>> {
	};
	template <typename... Ts>
	struct is_default_constructible<std::tuple<Ts...>>
		: conjunction<is_default_constructible<Ts>...> {
	};
	template <typename... Ts>
	struct is_default_constructible<const std::tuple<Ts...>>
		: conjunction<is_default_constructible<Ts>...> {
	};
	template <typename T, typename... Args>
	struct is_constructible : std::is_constructible<T, Args...> {};
	template <typename T1, typename T2>
	struct is_constructible<std::pair<T1, T2>> : is_default_constructible<std::pair<T1, T2>> {};
	template <typename T1, typename T2>
	struct is_constructible<const std::pair<T1, T2>> : is_default_constructible<const std::pair<T1, T2>> {};
	template <typename... Ts>
	struct is_constructible<std::tuple<Ts...>> : is_default_constructible<std::tuple<Ts...>> {};
	template <typename... Ts>
	struct is_constructible<const std::tuple<Ts...>> : is_default_constructible<const std::tuple<Ts...>> {};
	template<typename T, typename = void>
	struct is_iterator_traits : std::false_type {};
	template<typename T>
	struct is_iterator_traits<iterator_traits<T>>
	{
	private:
		using traits = iterator_traits<T>;
	public:
		static constexpr auto value =
			is_detected<value_type_t, traits>::value&&
			is_detected<difference_type_t, traits>::value&&
			is_detected<pointer_t, traits>::value&&
			is_detected<iterator_category_t, traits>::value&&
			is_detected<reference_t, traits>::value;
	};
	template<typename T>
	struct is_range
	{
	private:
		using t_ref = typename std::add_lvalue_reference<T>::type;
		using iterator = detected_t<result_of_begin, t_ref>;
		using sentinel = detected_t<result_of_end, t_ref>;
		static constexpr auto is_iterator_begin =
			is_iterator_traits<iterator_traits<iterator>>::value;
	public:
		static constexpr bool value = !std::is_same<iterator, nonesuch>::value && !std::is_same<sentinel, nonesuch>::value&& is_iterator_begin;
	};
	template<typename R>
	using iterator_t = enable_if_t<is_range<R>::value, result_of_begin<decltype(std::declval<R&>())>>;
	template<typename T>
	using range_value_t = value_type_t<iterator_traits<iterator_t<T>>>;
	template<typename T, typename = void>
	struct is_complete_type : std::false_type {};
	template<typename T>
	struct is_complete_type<T, decltype(void(sizeof(T)))> : std::true_type {};
	template<typename BasicJsonType, typename CompatibleObjectType,
		typename = void>
	struct is_compatible_object_type_impl : std::false_type {};
	template<typename BasicJsonType, typename CompatibleObjectType>
	struct is_compatible_object_type_impl <
		BasicJsonType, CompatibleObjectType,
		enable_if_t < is_detected<mapped_type_t, CompatibleObjectType>::value&&
		is_detected<key_type_t, CompatibleObjectType>::value >>
	{
		using object_t = typename BasicJsonType::object_t;
		static constexpr bool value =
			is_constructible<typename object_t::key_type,
			typename CompatibleObjectType::key_type>::value&&
			is_constructible<typename object_t::mapped_type,
			typename CompatibleObjectType::mapped_type>::value;
	};
	template<typename BasicJsonType, typename CompatibleObjectType>
	struct is_compatible_object_type
		: is_compatible_object_type_impl<BasicJsonType, CompatibleObjectType> {
	};
	template<typename BasicJsonType, typename ConstructibleObjectType,
		typename = void>
	struct is_constructible_object_type_impl : std::false_type {};
	template<typename BasicJsonType, typename ConstructibleObjectType>
	struct is_constructible_object_type_impl <
		BasicJsonType, ConstructibleObjectType,
		enable_if_t < is_detected<mapped_type_t, ConstructibleObjectType>::value&&
		is_detected<key_type_t, ConstructibleObjectType>::value >>
	{
		using object_t = typename BasicJsonType::object_t;
		static constexpr bool value =
			(is_default_constructible<ConstructibleObjectType>::value &&
				(std::is_move_assignable<ConstructibleObjectType>::value ||
					std::is_copy_assignable<ConstructibleObjectType>::value) &&
				(is_constructible<typename ConstructibleObjectType::key_type,
					typename object_t::key_type>::value &&
					std::is_same <
					typename object_t::mapped_type,
					typename ConstructibleObjectType::mapped_type >::value)) ||
			(has_from_json<BasicJsonType,
				typename ConstructibleObjectType::mapped_type>::value ||
				has_non_default_from_json <
				BasicJsonType,
				typename ConstructibleObjectType::mapped_type >::value);
	};
	template<typename BasicJsonType, typename ConstructibleObjectType>
	struct is_constructible_object_type
		: is_constructible_object_type_impl<BasicJsonType,
		ConstructibleObjectType> {
	};
	template<typename BasicJsonType, typename CompatibleStringType>
	struct is_compatible_string_type
	{
		static constexpr auto value =
			is_constructible<typename BasicJsonType::string_t, CompatibleStringType>::value;
	};
	template<typename BasicJsonType, typename ConstructibleStringType>
	struct is_constructible_string_type
	{
#ifdef __INTEL_COMPILER
		using laundered_type = decltype(std::declval<ConstructibleStringType>());
#else
		using laundered_type = ConstructibleStringType;
#endif
		static constexpr auto value =
			conjunction <
			is_constructible<laundered_type, typename BasicJsonType::string_t>,
			is_detected_exact<typename BasicJsonType::string_t::value_type,
			value_type_t, laundered_type >>::value;
	};
	template<typename BasicJsonType, typename CompatibleArrayType, typename = void>
	struct is_compatible_array_type_impl : std::false_type {};
	template<typename BasicJsonType, typename CompatibleArrayType>
	struct is_compatible_array_type_impl <
		BasicJsonType, CompatibleArrayType,
		enable_if_t <
		is_detected<iterator_t, CompatibleArrayType>::value&&
		is_iterator_traits<iterator_traits<detected_t<iterator_t, CompatibleArrayType>>>::value &&
		!std::is_same<CompatibleArrayType, detected_t<range_value_t, CompatibleArrayType>>::value >>
	{
		static constexpr bool value =
			is_constructible<BasicJsonType,
			range_value_t<CompatibleArrayType>>::value;
	};
	template<typename BasicJsonType, typename CompatibleArrayType>
	struct is_compatible_array_type
		: is_compatible_array_type_impl<BasicJsonType, CompatibleArrayType> {
	};
	template<typename BasicJsonType, typename ConstructibleArrayType, typename = void>
	struct is_constructible_array_type_impl : std::false_type {};
	template<typename BasicJsonType, typename ConstructibleArrayType>
	struct is_constructible_array_type_impl <
		BasicJsonType, ConstructibleArrayType,
		enable_if_t<std::is_same<ConstructibleArrayType,
		typename BasicJsonType::value_type>::value >>
		: std::true_type {};
	template<typename BasicJsonType, typename ConstructibleArrayType>
	struct is_constructible_array_type_impl <
		BasicJsonType, ConstructibleArrayType,
		enable_if_t < !std::is_same<ConstructibleArrayType,
		typename BasicJsonType::value_type>::value &&
		!is_compatible_string_type<BasicJsonType, ConstructibleArrayType>::value&&
		is_default_constructible<ConstructibleArrayType>::value &&
		(std::is_move_assignable<ConstructibleArrayType>::value ||
			std::is_copy_assignable<ConstructibleArrayType>::value) &&
		is_detected<iterator_t, ConstructibleArrayType>::value&&
		is_iterator_traits<iterator_traits<detected_t<iterator_t, ConstructibleArrayType>>>::value&&
		is_detected<range_value_t, ConstructibleArrayType>::value &&
		!std::is_same<ConstructibleArrayType, detected_t<range_value_t, ConstructibleArrayType>>::value&&
		is_complete_type <
		detected_t<range_value_t, ConstructibleArrayType >>::value >>
	{
		using value_type = range_value_t<ConstructibleArrayType>;
		static constexpr bool value =
			std::is_same<value_type,
			typename BasicJsonType::array_t::value_type>::value ||
			has_from_json<BasicJsonType,
			value_type>::value ||
			has_non_default_from_json <
			BasicJsonType,
			value_type >::value;
	};
	template<typename BasicJsonType, typename ConstructibleArrayType>
	struct is_constructible_array_type
		: is_constructible_array_type_impl<BasicJsonType, ConstructibleArrayType> {
	};
	template<typename RealIntegerType, typename CompatibleNumberIntegerType,
		typename = void>
	struct is_compatible_integer_type_impl : std::false_type {};
	template<typename RealIntegerType, typename CompatibleNumberIntegerType>
	struct is_compatible_integer_type_impl <
		RealIntegerType, CompatibleNumberIntegerType,
		enable_if_t < std::is_integral<RealIntegerType>::value&&
		std::is_integral<CompatibleNumberIntegerType>::value &&
		!std::is_same<bool, CompatibleNumberIntegerType>::value >>
	{
		using RealLimits = std::numeric_limits<RealIntegerType>;
		using CompatibleLimits = std::numeric_limits<CompatibleNumberIntegerType>;
		static constexpr auto value =
			is_constructible<RealIntegerType,
			CompatibleNumberIntegerType>::value&&
			CompatibleLimits::is_integer&&
			RealLimits::is_signed == CompatibleLimits::is_signed;
	};
	template<typename RealIntegerType, typename CompatibleNumberIntegerType>
	struct is_compatible_integer_type
		: is_compatible_integer_type_impl<RealIntegerType,
		CompatibleNumberIntegerType> {
	};
	template<typename BasicJsonType, typename CompatibleType, typename = void>
	struct is_compatible_type_impl : std::false_type {};
	template<typename BasicJsonType, typename CompatibleType>
	struct is_compatible_type_impl <
		BasicJsonType, CompatibleType,
		enable_if_t<is_complete_type<CompatibleType>::value >>
	{
		static constexpr bool value =
			has_to_json<BasicJsonType, CompatibleType>::value;
	};
	template<typename BasicJsonType, typename CompatibleType>
	struct is_compatible_type
		: is_compatible_type_impl<BasicJsonType, CompatibleType> {
	};
	template<typename T1, typename T2>
	struct is_constructible_tuple : std::false_type {};
	template<typename T1, typename... Args>
	struct is_constructible_tuple<T1, std::tuple<Args...>> : conjunction<is_constructible<T1, Args>...> {};
	template<typename BasicJsonType, typename T>
	struct is_json_iterator_of : std::false_type {};
	template<typename BasicJsonType>
	struct is_json_iterator_of<BasicJsonType, typename BasicJsonType::iterator> : std::true_type {};
	template<typename BasicJsonType>
	struct is_json_iterator_of<BasicJsonType, typename BasicJsonType::const_iterator> : std::true_type
	{
	};
	template<template <typename...> class Primary, typename T>
	struct is_specialization_of : std::false_type {};
	template<template <typename...> class Primary, typename... Args>
	struct is_specialization_of<Primary, Primary<Args...>> : std::true_type {};
	template<typename T>
	using is_json_pointer = is_specialization_of<::JsonLib::json_pointer, uncvref_t<T>>;
	template<typename Compare, typename A, typename B, typename = void>
	struct is_comparable : std::false_type {};
	template<typename Compare, typename A, typename B>
	struct is_comparable<Compare, A, B, void_t<
		decltype(std::declval<Compare>()(std::declval<A>(), std::declval<B>())),
		decltype(std::declval<Compare>()(std::declval<B>(), std::declval<A>()))
		>> : std::true_type {};
	template<typename T>
	using detect_is_transparent = typename T::is_transparent;
	template<typename Comparator, typename ObjectKeyType, typename KeyTypeCVRef, bool RequireTransparentComparator = true,
		bool ExcludeObjectKeyType = RequireTransparentComparator, typename KeyType = uncvref_t<KeyTypeCVRef>>
		using is_usable_as_key_type = typename std::conditional <
		is_comparable<Comparator, ObjectKeyType, KeyTypeCVRef>::value
		&& !(ExcludeObjectKeyType&& std::is_same<KeyType,
			ObjectKeyType>::value)
		&& (!RequireTransparentComparator
			|| is_detected <detect_is_transparent, Comparator>::value)
		&& !is_json_pointer<KeyType>::value,
		std::true_type,
		std::false_type >::type;
	template<typename BasicJsonType, typename KeyTypeCVRef, bool RequireTransparentComparator = true,
		bool ExcludeObjectKeyType = RequireTransparentComparator, typename KeyType = uncvref_t<KeyTypeCVRef>>
		using is_usable_as_basic_json_key_type = typename std::conditional <
		is_usable_as_key_type<typename BasicJsonType::object_comparator_t,
		typename BasicJsonType::object_t::key_type, KeyTypeCVRef,
		RequireTransparentComparator, ExcludeObjectKeyType>::value
		&& !is_json_iterator_of<BasicJsonType, KeyType>::value,
		std::true_type,
		std::false_type >::type;
	template<typename ObjectType, typename KeyType>
	using detect_erase_with_key_type = decltype(std::declval<ObjectType&>().erase(std::declval<KeyType>()));
	template<typename BasicJsonType, typename KeyType>
	using has_erase_with_key_type = typename std::conditional <
		is_detected <
		detect_erase_with_key_type,
		typename BasicJsonType::object_t, KeyType >::value,
		std::true_type,
		std::false_type >::type;
	template <typename T>
	struct is_ordered_map
	{
		using one = char;
		struct two
		{
			char x[2];
		};
		template <typename C> static one test(decltype(&C::capacity));
		template <typename C> static two test(...);
		enum { value = sizeof(test<T>(nullptr)) == sizeof(char) };
	};
	template < typename T, typename U, enable_if_t < !std::is_same<T, U>::value, int > = 0 >
	T conditional_static_cast(U value)
	{
		return static_cast<T>(value);
	}
	template<typename T, typename U, enable_if_t<std::is_same<T, U>::value, int> = 0>
	T conditional_static_cast(U value)
	{
		return value;
	}
	template<typename... Types>
	using all_integral = conjunction<std::is_integral<Types>...>;
	template<typename... Types>
	using all_signed = conjunction<std::is_signed<Types>...>;
	template<typename... Types>
	using all_unsigned = conjunction<std::is_unsigned<Types>...>;
	template<typename... Types>
	using same_sign = std::integral_constant < bool,
		all_signed<Types...>::value || all_unsigned<Types...>::value >;
	template<typename OfType, typename T>
	using never_out_of_range = std::integral_constant < bool,
		(std::is_signed<OfType>::value && (sizeof(T) < sizeof(OfType)))
		|| (same_sign<OfType, T>::value && sizeof(OfType) == sizeof(T)) >;
	template<typename OfType, typename T,
		bool OfTypeSigned = std::is_signed<OfType>::value,
		bool TSigned = std::is_signed<T>::value>
	struct value_in_range_of_impl2;
	template<typename OfType, typename T>
	struct value_in_range_of_impl2<OfType, T, false, false>
	{
		static constexpr bool test(T val)
		{
			using CommonType = typename std::common_type<OfType, T>::type;
			return static_cast<CommonType>(val) <= static_cast<CommonType>((std::numeric_limits<OfType>::max)());
		}
	};
	template<typename OfType, typename T>
	struct value_in_range_of_impl2<OfType, T, true, false>
	{
		static constexpr bool test(T val)
		{
			using CommonType = typename std::common_type<OfType, T>::type;
			return static_cast<CommonType>(val) <= static_cast<CommonType>((std::numeric_limits<OfType>::max)());
		}
	};
	template<typename OfType, typename T>
	struct value_in_range_of_impl2<OfType, T, false, true>
	{
		static constexpr bool test(T val)
		{
			using CommonType = typename std::common_type<OfType, T>::type;
			return val >= 0 && static_cast<CommonType>(val) <= static_cast<CommonType>((std::numeric_limits<OfType>::max)());
		}
	};
	template<typename OfType, typename T>
	struct value_in_range_of_impl2<OfType, T, true, true>
	{
		static constexpr bool test(T val)
		{
			using CommonType = typename std::common_type<OfType, T>::type;
			return static_cast<CommonType>(val) >= static_cast<CommonType>((std::numeric_limits<OfType>::min)())
				&& static_cast<CommonType>(val) <= static_cast<CommonType>((std::numeric_limits<OfType>::max)());
		}
	};
	template<typename OfType, typename T,
		bool NeverOutOfRange = never_out_of_range<OfType, T>::value,
		typename = detail::enable_if_t<all_integral<OfType, T>::value>>
		struct value_in_range_of_impl1;
	template<typename OfType, typename T>
	struct value_in_range_of_impl1<OfType, T, false>
	{
		static constexpr bool test(T val)
		{
			return value_in_range_of_impl2<OfType, T>::test(val);
		}
	};
	template<typename OfType, typename T>
	struct value_in_range_of_impl1<OfType, T, true>
	{
		static constexpr bool test(T)
		{
			return true;
		}
	};
	template<typename OfType, typename T>
	inline constexpr bool value_in_range_of(T val)
	{
		return value_in_range_of_impl1<OfType, T>::test(val);
	}
	template<bool Value>
	using bool_constant = std::integral_constant<bool, Value>;
	namespace impl
	{
		template<typename T>
		inline constexpr bool is_c_string()
		{
			using TUnExt = typename std::remove_extent<T>::type;
			using TUnCVExt = typename std::remove_cv<TUnExt>::type;
			using TUnPtr = typename std::remove_pointer<T>::type;
			using TUnCVPtr = typename std::remove_cv<TUnPtr>::type;
			return
				(std::is_array<T>::value && std::is_same<TUnCVExt, char>::value)
				|| (std::is_pointer<T>::value && std::is_same<TUnCVPtr, char>::value);
		}
	}
	template<typename T>
	struct is_c_string : bool_constant<impl::is_c_string<T>()> {};
	template<typename T>
	using is_c_string_uncvref = is_c_string<uncvref_t<T>>;
	namespace impl
	{
		template<typename T>
		inline constexpr bool is_transparent()
		{
			return is_detected<detect_is_transparent, T>::value;
		}
	}
	template<typename T>
	struct is_transparent : bool_constant<impl::is_transparent<T>()> {};
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstring> 
#include <string> 
#include <utility> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	inline std::size_t concat_length()
	{
		return 0;
	}
	template<typename... Args>
	inline std::size_t concat_length(const char* cstr, const Args& ... rest);
	template<typename StringType, typename... Args>
	inline std::size_t concat_length(const StringType& str, const Args& ... rest);
	template<typename... Args>
	inline std::size_t concat_length(const char, const Args& ... rest)
	{
		return 1 + concat_length(rest...);
	}
	template<typename... Args>
	inline std::size_t concat_length(const char* cstr, const Args& ... rest)
	{
		return ::strlen(cstr) + concat_length(rest...);
	}
	template<typename StringType, typename... Args>
	inline std::size_t concat_length(const StringType& str, const Args& ... rest)
	{
		return str.size() + concat_length(rest...);
	}
	template<typename OutStringType>
	inline void concat_into(OutStringType&)
	{
	}
	template<typename StringType, typename Arg>
	using string_can_append = decltype(std::declval<StringType&>().append(std::declval < Arg&& >()));
	template<typename StringType, typename Arg>
	using detect_string_can_append = is_detected<string_can_append, StringType, Arg>;
	template<typename StringType, typename Arg>
	using string_can_append_op = decltype(std::declval<StringType&>() += std::declval < Arg&& >());
	template<typename StringType, typename Arg>
	using detect_string_can_append_op = is_detected<string_can_append_op, StringType, Arg>;
	template<typename StringType, typename Arg>
	using string_can_append_iter = decltype(std::declval<StringType&>().append(std::declval<const Arg&>().begin(), std::declval<const Arg&>().end()));
	template<typename StringType, typename Arg>
	using detect_string_can_append_iter = is_detected<string_can_append_iter, StringType, Arg>;
	template<typename StringType, typename Arg>
	using string_can_append_data = decltype(std::declval<StringType&>().append(std::declval<const Arg&>().data(), std::declval<const Arg&>().size()));
	template<typename StringType, typename Arg>
	using detect_string_can_append_data = is_detected<string_can_append_data, StringType, Arg>;
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& detect_string_can_append_op<OutStringType, Arg>::value, int > = 0 >
	inline void concat_into(OutStringType& out, Arg&& arg, Args && ... rest);
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& !detect_string_can_append_op<OutStringType, Arg>::value
		&& detect_string_can_append_iter<OutStringType, Arg>::value, int > = 0 >
	inline void concat_into(OutStringType& out, const Arg& arg, Args && ... rest);
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& !detect_string_can_append_op<OutStringType, Arg>::value
		&& !detect_string_can_append_iter<OutStringType, Arg>::value
		&& detect_string_can_append_data<OutStringType, Arg>::value, int > = 0 >
	inline void concat_into(OutStringType& out, const Arg& arg, Args && ... rest);
	template<typename OutStringType, typename Arg, typename... Args,
		enable_if_t<detect_string_can_append<OutStringType, Arg>::value, int> = 0>
	inline void concat_into(OutStringType& out, Arg&& arg, Args && ... rest)
	{
		out.append(std::forward<Arg>(arg));
		concat_into(out, std::forward<Args>(rest)...);
	}
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& detect_string_can_append_op<OutStringType, Arg>::value, int > >
	inline void concat_into(OutStringType& out, Arg&& arg, Args&& ... rest)
	{
		out += std::forward<Arg>(arg);
		concat_into(out, std::forward<Args>(rest)...);
	}
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& !detect_string_can_append_op<OutStringType, Arg>::value
		&& detect_string_can_append_iter<OutStringType, Arg>::value, int > >
	inline void concat_into(OutStringType& out, const Arg& arg, Args&& ... rest)
	{
		out.append(arg.begin(), arg.end());
		concat_into(out, std::forward<Args>(rest)...);
	}
	template < typename OutStringType, typename Arg, typename... Args,
		enable_if_t < !detect_string_can_append<OutStringType, Arg>::value
		&& !detect_string_can_append_op<OutStringType, Arg>::value
		&& !detect_string_can_append_iter<OutStringType, Arg>::value
		&& detect_string_can_append_data<OutStringType, Arg>::value, int > >
	inline void concat_into(OutStringType& out, const Arg& arg, Args&& ... rest)
	{
		out.append(arg.data(), arg.size());
		concat_into(out, std::forward<Args>(rest)...);
	}
	template<typename OutStringType = std::string, typename... Args>
	inline OutStringType concat(Args && ... args)
	{
		OutStringType str;
		str.reserve(concat_length(args...));
		concat_into(str, std::forward<Args>(args)...);
		return str;
	}
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	class exception : public std::exception
	{
	public:
		const char* what() const noexcept override
		{
			return m.what();
		}
		const int id;
	protected:
		JSON_HEDLEY_NON_NULL(3)
			exception(int id_, const char* what_arg) : id(id_), m(what_arg) {}
		static std::string name(const std::string& ename, int id_)
		{
			return concat("[json.exception.", ename, '.', std::to_string(id_), "] ");
		}
		static std::string diagnostics(std::nullptr_t)
		{
			return "";
		}
		template<typename BasicJsonType>
		static std::string diagnostics(const BasicJsonType* leaf_element)
		{
#if JSON_DIAGNOSTICS
			std::vector<std::string> tokens;
			for (const auto* current = leaf_element; current != nullptr && current->m_parent != nullptr; current = current->m_parent)
			{
				switch (current->m_parent->type())
				{
				case value_t::array:
				{
					for (std::size_t i = 0; i < current->m_parent->m_data.m_value.array->size(); ++i)
					{
						if (&current->m_parent->m_data.m_value.array->operator[](i) == current)
						{
							tokens.emplace_back(std::to_string(i));
							break;
						}
					}
					break;
				}
				case value_t::object:
				{
					for (const auto& element : *current->m_parent->m_data.m_value.object)
					{
						if (&element.second == current)
						{
							tokens.emplace_back(element.first.c_str());
							break;
						}
					}
					break;
				}
				case value_t::null:
				case value_t::string:
				case value_t::boolean:
				case value_t::number_integer:
				case value_t::number_unsigned:
				case value_t::number_float:
				case value_t::binary:
				case value_t::discarded:
				default:
					break;
				}
			}
			if (tokens.empty())
			{
				return "";
			}
			auto str = std::accumulate(tokens.rbegin(), tokens.rend(), std::string{},
				[](const std::string& a, const std::string& b)
				{
					return concat(a, '/', detail::escape(b));
				});
			return concat('(', str, ") ");
#else
			static_cast<void>(leaf_element);
			return "";
#endif
		}
	private:
		std::runtime_error m;
	};
	class parse_error : public exception
	{
	public:
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static parse_error create(int id_, const position_t& pos, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("parse_error", id_), "parse error",
				position_string(pos), ": ", exception::diagnostics(context), what_arg);
			return { id_, pos.chars_read_total, w.c_str() };
		}
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static parse_error create(int id_, std::size_t byte_, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("parse_error", id_), "parse error",
				(byte_ != 0 ? (concat(" at byte ", std::to_string(byte_))) : ""),
				": ", exception::diagnostics(context), what_arg);
			return { id_, byte_, w.c_str() };
		}
		const std::size_t byte;
	private:
		parse_error(int id_, std::size_t byte_, const char* what_arg)
			: exception(id_, what_arg), byte(byte_) {
		}
		static std::string position_string(const position_t& pos)
		{
			return concat(" at line ", std::to_string(pos.lines_read + 1),
				", column ", std::to_string(pos.chars_read_current_line));
		}
	};
	class invalid_iterator : public exception
	{
	public:
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static invalid_iterator create(int id_, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("invalid_iterator", id_), exception::diagnostics(context), what_arg);
			return { id_, w.c_str() };
		}
	private:
		JSON_HEDLEY_NON_NULL(3)
			invalid_iterator(int id_, const char* what_arg)
			: exception(id_, what_arg) {
		}
	};
	class type_error : public exception
	{
	public:
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static type_error create(int id_, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("type_error", id_), exception::diagnostics(context), what_arg);
			return { id_, w.c_str() };
		}
	private:
		JSON_HEDLEY_NON_NULL(3)
			type_error(int id_, const char* what_arg) : exception(id_, what_arg) {}
	};
	class out_of_range : public exception
	{
	public:
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static out_of_range create(int id_, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("out_of_range", id_), exception::diagnostics(context), what_arg);
			return { id_, w.c_str() };
		}
	private:
		JSON_HEDLEY_NON_NULL(3)
			out_of_range(int id_, const char* what_arg) : exception(id_, what_arg) {}
	};
	class other_error : public exception
	{
	public:
		template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
		static other_error create(int id_, const std::string& what_arg, BasicJsonContext context)
		{
			const std::string w = concat(exception::name("other_error", id_), exception::diagnostics(context), what_arg);
			return { id_, w.c_str() };
		}
	private:
		JSON_HEDLEY_NON_NULL(3)
			other_error(int id_, const char* what_arg) : exception(id_, what_arg) {}
	};
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template <class T> struct identity_tag {};
}
NLOHMANN_JSON_NAMESPACE_END
#if JSON_HAS_EXPERIMENTAL_FILESYSTEM
#include <experimental/filesystem>
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	namespace std_fs = std::experimental::filesystem;
}
NLOHMANN_JSON_NAMESPACE_END
#elif JSON_HAS_FILESYSTEM
#include <filesystem>
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	namespace std_fs = std::filesystem;
}
NLOHMANN_JSON_NAMESPACE_END
#endif
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename std::nullptr_t& n)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_null()))
		{
			JSON_THROW(type_error::create(302, concat("type must be null, but is ", j.type_name()), &j));
		}
		n = nullptr;
	}
	template < typename BasicJsonType, typename ArithmeticType,
		enable_if_t < std::is_arithmetic<ArithmeticType>::value &&
		!std::is_same<ArithmeticType, typename BasicJsonType::boolean_t>::value,
		int > = 0 >
	void get_arithmetic_value(const BasicJsonType& j, ArithmeticType& val)
	{
		switch (static_cast<value_t>(j))
		{
		case value_t::number_unsigned:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_unsigned_t*>());
			break;
		}
		case value_t::number_integer:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_integer_t*>());
			break;
		}
		case value_t::number_float:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_float_t*>());
			break;
		}
		case value_t::null:
		case value_t::object:
		case value_t::array:
		case value_t::string:
		case value_t::boolean:
		case value_t::binary:
		case value_t::discarded:
		default:
			JSON_THROW(type_error::create(302, concat("type must be number, but is ", j.type_name()), &j));
		}
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::boolean_t& b)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_boolean()))
		{
			JSON_THROW(type_error::create(302, concat("type must be boolean, but is ", j.type_name()), &j));
		}
		b = *j.template get_ptr<const typename BasicJsonType::boolean_t*>();
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::string_t& s)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_string()))
		{
			JSON_THROW(type_error::create(302, concat("type must be string, but is ", j.type_name()), &j));
		}
		s = *j.template get_ptr<const typename BasicJsonType::string_t*>();
	}
	template <
		typename BasicJsonType, typename StringType,
		enable_if_t <
		std::is_assignable<StringType&, const typename BasicJsonType::string_t>::value
		&& is_detected_exact<typename BasicJsonType::string_t::value_type, value_type_t, StringType>::value
		&& !std::is_same<typename BasicJsonType::string_t, StringType>::value
		&& !is_json_ref<StringType>::value, int > = 0 >
	inline void from_json(const BasicJsonType& j, StringType& s)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_string()))
		{
			JSON_THROW(type_error::create(302, concat("type must be string, but is ", j.type_name()), &j));
		}
		s = *j.template get_ptr<const typename BasicJsonType::string_t*>();
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::number_float_t& val)
	{
		get_arithmetic_value(j, val);
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::number_unsigned_t& val)
	{
		get_arithmetic_value(j, val);
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::number_integer_t& val)
	{
		get_arithmetic_value(j, val);
	}
#if !JSON_DISABLE_ENUM_SERIALIZATION
	template<typename BasicJsonType, typename EnumType,
		enable_if_t<std::is_enum<EnumType>::value, int> = 0>
	inline void from_json(const BasicJsonType& j, EnumType& e)
	{
		typename std::underlying_type<EnumType>::type val;
		get_arithmetic_value(j, val);
		e = static_cast<EnumType>(val);
	}
#endif  
	template<typename BasicJsonType, typename T, typename Allocator,
		enable_if_t<is_getable<BasicJsonType, T>::value, int> = 0>
	inline void from_json(const BasicJsonType& j, std::forward_list<T, Allocator>& l)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		l.clear();
		std::transform(j.rbegin(), j.rend(),
			std::front_inserter(l), [](const BasicJsonType& i)
			{
				return i.template get<T>();
			});
	}
	template<typename BasicJsonType, typename T,
		enable_if_t<is_getable<BasicJsonType, T>::value, int> = 0>
	inline void from_json(const BasicJsonType& j, std::valarray<T>& l)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		l.resize(j.size());
		std::transform(j.begin(), j.end(), std::begin(l),
			[](const BasicJsonType& elem)
			{
				return elem.template get<T>();
			});
	}
	template<typename BasicJsonType, typename T, std::size_t N>
	auto from_json(const BasicJsonType& j, T(&arr)[N])
		-> decltype(j.template get<T>(), void())
	{
		for (std::size_t i = 0; i < N; ++i)
		{
			arr[i] = j.at(i).template get<T>();
		}
	}
	template<typename BasicJsonType>
	inline void from_json_array_impl(const BasicJsonType& j, typename BasicJsonType::array_t& arr, priority_tag<3>)
	{
		arr = *j.template get_ptr<const typename BasicJsonType::array_t*>();
	}
	template<typename BasicJsonType, typename T, std::size_t N>
	auto from_json_array_impl(const BasicJsonType& j, std::array<T, N>& arr,
		priority_tag<2>)
		-> decltype(j.template get<T>(), void())
	{
		for (std::size_t i = 0; i < N; ++i)
		{
			arr[i] = j.at(i).template get<T>();
		}
	}
	template<typename BasicJsonType, typename ConstructibleArrayType,
		enable_if_t<
		std::is_assignable<ConstructibleArrayType&, ConstructibleArrayType>::value,
		int> = 0>
	auto from_json_array_impl(const BasicJsonType& j, ConstructibleArrayType& arr, priority_tag<1>)
		-> decltype(
			arr.reserve(std::declval<typename ConstructibleArrayType::size_type>()),
			j.template get<typename ConstructibleArrayType::value_type>(),
			void())
	{
		using std::end;
		ConstructibleArrayType ret;
		ret.reserve(j.size());
		std::transform(j.begin(), j.end(),
			std::inserter(ret, end(ret)), [](const BasicJsonType& i)
			{
				return i.template get<typename ConstructibleArrayType::value_type>();
			});
		arr = std::move(ret);
	}
	template<typename BasicJsonType, typename ConstructibleArrayType,
		enable_if_t<
		std::is_assignable<ConstructibleArrayType&, ConstructibleArrayType>::value,
		int> = 0>
	inline void from_json_array_impl(const BasicJsonType& j, ConstructibleArrayType& arr,
		priority_tag<0>)
	{
		using std::end;
		ConstructibleArrayType ret;
		std::transform(
			j.begin(), j.end(), std::inserter(ret, end(ret)),
			[](const BasicJsonType& i)
			{
				return i.template get<typename ConstructibleArrayType::value_type>();
			});
		arr = std::move(ret);
	}
	template < typename BasicJsonType, typename ConstructibleArrayType,
		enable_if_t <
		is_constructible_array_type<BasicJsonType, ConstructibleArrayType>::value &&
		!is_constructible_object_type<BasicJsonType, ConstructibleArrayType>::value &&
		!is_constructible_string_type<BasicJsonType, ConstructibleArrayType>::value &&
		!std::is_same<ConstructibleArrayType, typename BasicJsonType::binary_t>::value &&
		!is_basic_json<ConstructibleArrayType>::value,
		int > = 0 >
	auto from_json(const BasicJsonType& j, ConstructibleArrayType& arr)
		-> decltype(from_json_array_impl(j, arr, priority_tag<3> {}),
			j.template get<typename ConstructibleArrayType::value_type>(),
			void())
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		from_json_array_impl(j, arr, priority_tag<3> {});
	}
	template < typename BasicJsonType, typename T, std::size_t... Idx >
	std::array<T, sizeof...(Idx)> from_json_inplace_array_impl(BasicJsonType&& j,
		identity_tag<std::array<T, sizeof...(Idx)>>, index_sequence<Idx...>)
	{
		return { { std::forward<BasicJsonType>(j).at(Idx).template get<T>()... } };
	}
	template < typename BasicJsonType, typename T, std::size_t N >
	auto from_json(BasicJsonType&& j, identity_tag<std::array<T, N>> tag)
		-> decltype(from_json_inplace_array_impl(std::forward<BasicJsonType>(j), tag, make_index_sequence<N> {}))
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		return from_json_inplace_array_impl(std::forward<BasicJsonType>(j), tag, make_index_sequence<N> {});
	}
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, typename BasicJsonType::binary_t& bin)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_binary()))
		{
			JSON_THROW(type_error::create(302, concat("type must be binary, but is ", j.type_name()), &j));
		}
		bin = *j.template get_ptr<const typename BasicJsonType::binary_t*>();
	}
	template<typename BasicJsonType, typename ConstructibleObjectType,
		enable_if_t<is_constructible_object_type<BasicJsonType, ConstructibleObjectType>::value, int> = 0>
	inline void from_json(const BasicJsonType& j, ConstructibleObjectType& obj)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_object()))
		{
			JSON_THROW(type_error::create(302, concat("type must be object, but is ", j.type_name()), &j));
		}
		ConstructibleObjectType ret;
		const auto* inner_object = j.template get_ptr<const typename BasicJsonType::object_t*>();
		using value_type = typename ConstructibleObjectType::value_type;
		std::transform(
			inner_object->begin(), inner_object->end(),
			std::inserter(ret, ret.begin()),
			[](typename BasicJsonType::object_t::value_type const& p)
			{
				return value_type(p.first, p.second.template get<typename ConstructibleObjectType::mapped_type>());
			});
		obj = std::move(ret);
	}
	template < typename BasicJsonType, typename ArithmeticType,
		enable_if_t <
		std::is_arithmetic<ArithmeticType>::value &&
		!std::is_same<ArithmeticType, typename BasicJsonType::number_unsigned_t>::value &&
		!std::is_same<ArithmeticType, typename BasicJsonType::number_integer_t>::value &&
		!std::is_same<ArithmeticType, typename BasicJsonType::number_float_t>::value &&
		!std::is_same<ArithmeticType, typename BasicJsonType::boolean_t>::value,
		int > = 0 >
	inline void from_json(const BasicJsonType& j, ArithmeticType& val)
	{
		switch (static_cast<value_t>(j))
		{
		case value_t::number_unsigned:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_unsigned_t*>());
			break;
		}
		case value_t::number_integer:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_integer_t*>());
			break;
		}
		case value_t::number_float:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::number_float_t*>());
			break;
		}
		case value_t::boolean:
		{
			val = static_cast<ArithmeticType>(*j.template get_ptr<const typename BasicJsonType::boolean_t*>());
			break;
		}
		case value_t::null:
		case value_t::object:
		case value_t::array:
		case value_t::string:
		case value_t::binary:
		case value_t::discarded:
		default:
			JSON_THROW(type_error::create(302, concat("type must be number, but is ", j.type_name()), &j));
		}
	}
	template<typename BasicJsonType, typename... Args, std::size_t... Idx>
	std::tuple<Args...> from_json_tuple_impl_base(BasicJsonType&& j, index_sequence<Idx...>)
	{
		return std::make_tuple(std::forward<BasicJsonType>(j).at(Idx).template get<Args>()...);
	}
	template < typename BasicJsonType, class A1, class A2 >
	std::pair<A1, A2> from_json_tuple_impl(BasicJsonType&& j, identity_tag<std::pair<A1, A2>>, priority_tag<0>)
	{
		return { std::forward<BasicJsonType>(j).at(0).template get<A1>(),
				std::forward<BasicJsonType>(j).at(1).template get<A2>() };
	}
	template<typename BasicJsonType, typename A1, typename A2>
	inline void from_json_tuple_impl(BasicJsonType&& j, std::pair<A1, A2>& p, priority_tag<1>)
	{
		p = from_json_tuple_impl(std::forward<BasicJsonType>(j), identity_tag<std::pair<A1, A2>> {}, priority_tag<0> {});
	}
	template<typename BasicJsonType, typename... Args>
	std::tuple<Args...> from_json_tuple_impl(BasicJsonType&& j, identity_tag<std::tuple<Args...>>, priority_tag<2>)
	{
		return from_json_tuple_impl_base<BasicJsonType, Args...>(std::forward<BasicJsonType>(j), index_sequence_for<Args...> {});
	}
	template<typename BasicJsonType, typename... Args>
	inline void from_json_tuple_impl(BasicJsonType&& j, std::tuple<Args...>& t, priority_tag<3>)
	{
		t = from_json_tuple_impl_base<BasicJsonType, Args...>(std::forward<BasicJsonType>(j), index_sequence_for<Args...> {});
	}
	template<typename BasicJsonType, typename TupleRelated>
	auto from_json(BasicJsonType&& j, TupleRelated&& t)
		-> decltype(from_json_tuple_impl(std::forward<BasicJsonType>(j), std::forward<TupleRelated>(t), priority_tag<3> {}))
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		return from_json_tuple_impl(std::forward<BasicJsonType>(j), std::forward<TupleRelated>(t), priority_tag<3> {});
	}
	template < typename BasicJsonType, typename Key, typename Value, typename Compare, typename Allocator,
		typename = enable_if_t < !std::is_constructible <
		typename BasicJsonType::string_t, Key >::value >>
		inline void from_json(const BasicJsonType& j, std::map<Key, Value, Compare, Allocator>& m)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		m.clear();
		for (const auto& p : j)
		{
			if (JSON_HEDLEY_UNLIKELY(!p.is_array()))
			{
				JSON_THROW(type_error::create(302, concat("type must be array, but is ", p.type_name()), &j));
			}
			m.emplace(p.at(0).template get<Key>(), p.at(1).template get<Value>());
		}
	}
	template < typename BasicJsonType, typename Key, typename Value, typename Hash, typename KeyEqual, typename Allocator,
		typename = enable_if_t < !std::is_constructible <
		typename BasicJsonType::string_t, Key >::value >>
		inline void from_json(const BasicJsonType& j, std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>& m)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_array()))
		{
			JSON_THROW(type_error::create(302, concat("type must be array, but is ", j.type_name()), &j));
		}
		m.clear();
		for (const auto& p : j)
		{
			if (JSON_HEDLEY_UNLIKELY(!p.is_array()))
			{
				JSON_THROW(type_error::create(302, concat("type must be array, but is ", p.type_name()), &j));
			}
			m.emplace(p.at(0).template get<Key>(), p.at(1).template get<Value>());
		}
	}
#if JSON_HAS_FILESYSTEM || JSON_HAS_EXPERIMENTAL_FILESYSTEM
	template<typename BasicJsonType>
	inline void from_json(const BasicJsonType& j, std_fs::path& p)
	{
		if (JSON_HEDLEY_UNLIKELY(!j.is_string()))
		{
			JSON_THROW(type_error::create(302, concat("type must be string, but is ", j.type_name()), &j));
		}
		p = *j.template get_ptr<const typename BasicJsonType::string_t*>();
	}
#endif
	struct from_json_fn
	{
		template<typename BasicJsonType, typename T>
		auto operator()(const BasicJsonType& j, T&& val) const
			noexcept(noexcept(from_json(j, std::forward<T>(val))))
			-> decltype(from_json(j, std::forward<T>(val)))
		{
			return from_json(j, std::forward<T>(val));
		}
	};
}
#ifndef JSON_HAS_CPP_17
namespace
{
#endif
	JSON_INLINE_VARIABLE constexpr const auto& from_json =
		detail::static_const<detail::from_json_fn>::value;
#ifndef JSON_HAS_CPP_17
}
#endif
NLOHMANN_JSON_NAMESPACE_END
#include <algorithm> 
#include <iterator> 
#include <string> 
#include <tuple> 
#include <type_traits> 
#include <utility> 
#include <valarray> 
#include <vector> 
#include <cstddef> 
#include <iterator> 
#include <string> 
#include <tuple> 
#include <utility> 
#if JSON_HAS_RANGES
#include <ranges> 
#endif
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename string_type>
	void int_to_string(string_type& target, std::size_t value)
	{
		using std::to_string;
		target = to_string(value);
	}
	template<typename IteratorType> class iteration_proxy_value
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = iteration_proxy_value;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::input_iterator_tag;
		using string_type = typename std::remove_cv< typename std::remove_reference<decltype(std::declval<IteratorType>().key()) >::type >::type;
	private:
		IteratorType anchor{};
		std::size_t array_index = 0;
		mutable std::size_t array_index_last = 0;
		mutable string_type array_index_str = "0";
		string_type empty_str{};
	public:
		explicit iteration_proxy_value() = default;
		explicit iteration_proxy_value(IteratorType it, std::size_t array_index_ = 0)
			noexcept(std::is_nothrow_move_constructible<IteratorType>::value
				&& std::is_nothrow_default_constructible<string_type>::value)
			: anchor(std::move(it))
			, array_index(array_index_)
		{
		}
		iteration_proxy_value(iteration_proxy_value const&) = default;
		iteration_proxy_value& operator=(iteration_proxy_value const&) = default;
		iteration_proxy_value(iteration_proxy_value&&)
			noexcept(std::is_nothrow_move_constructible<IteratorType>::value
				&& std::is_nothrow_move_constructible<string_type>::value) = default;
		iteration_proxy_value& operator=(iteration_proxy_value&&)
			noexcept(std::is_nothrow_move_assignable<IteratorType>::value
				&& std::is_nothrow_move_assignable<string_type>::value) = default;
		~iteration_proxy_value() = default;
		const iteration_proxy_value& operator*() const
		{
			return *this;
		}
		iteration_proxy_value& operator++()
		{
			++anchor;
			++array_index;
			return *this;
		}
		iteration_proxy_value operator++(int)&
		{
			auto tmp = iteration_proxy_value(anchor, array_index);
			++anchor;
			++array_index;
			return tmp;
		}
		bool operator==(const iteration_proxy_value& o) const
		{
			return anchor == o.anchor;
		}
		bool operator!=(const iteration_proxy_value& o) const
		{
			return anchor != o.anchor;
		}
		const string_type& key() const
		{
			JSON_ASSERT(anchor.m_object != nullptr);
			switch (anchor.m_object->type())
			{
			case value_t::array:
			{
				if (array_index != array_index_last)
				{
					int_to_string(array_index_str, array_index);
					array_index_last = array_index;
				}
				return array_index_str;
			}
			case value_t::object:
				return anchor.key();
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
				return empty_str;
			}
		}
		typename IteratorType::reference value() const
		{
			return anchor.value();
		}
	};
	template<typename IteratorType> class iteration_proxy
	{
	private:
		typename IteratorType::pointer container = nullptr;
	public:
		explicit iteration_proxy() = default;
		explicit iteration_proxy(typename IteratorType::reference cont) noexcept
			: container(&cont) {
		}
		iteration_proxy(iteration_proxy const&) = default;
		iteration_proxy& operator=(iteration_proxy const&) = default;
		iteration_proxy(iteration_proxy&&) noexcept = default;
		iteration_proxy& operator=(iteration_proxy&&) noexcept = default;
		~iteration_proxy() = default;
		iteration_proxy_value<IteratorType> begin() const noexcept
		{
			return iteration_proxy_value<IteratorType>(container->begin());
		}
		iteration_proxy_value<IteratorType> end() const noexcept
		{
			return iteration_proxy_value<IteratorType>(container->end());
		}
	};
	template<std::size_t N, typename IteratorType, enable_if_t<N == 0, int> = 0>
	auto get(const JsonLib::detail::iteration_proxy_value<IteratorType>& i) -> decltype(i.key())
	{
		return i.key();
	}
	template<std::size_t N, typename IteratorType, enable_if_t<N == 1, int> = 0>
	auto get(const JsonLib::detail::iteration_proxy_value<IteratorType>& i) -> decltype(i.value())
	{
		return i.value();
	}
}
NLOHMANN_JSON_NAMESPACE_END
namespace std
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
	template<typename IteratorType>
	class tuple_size<::JsonLib::detail::iteration_proxy_value<IteratorType>>
		: public std::integral_constant<std::size_t, 2> {
	};
	template<std::size_t N, typename IteratorType>
	class tuple_element<N, ::JsonLib::detail::iteration_proxy_value<IteratorType >>
	{
	public:
		using type = decltype(
			get<N>(std::declval <
				::JsonLib::detail::iteration_proxy_value<IteratorType >> ()));
	};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}
#if JSON_HAS_RANGES
template <typename IteratorType>
inline constexpr bool ::std::ranges::enable_borrowed_range<::JsonLib::detail::iteration_proxy<IteratorType>> = true;
#endif
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<value_t> struct external_constructor;
	template<>
	struct external_constructor<value_t::boolean>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::boolean_t b) noexcept
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::boolean;
			j.m_data.m_value = b;
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::string>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, const typename BasicJsonType::string_t& s)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::string;
			j.m_data.m_value = s;
			j.assert_invariant();
		}
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::string_t&& s)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::string;
			j.m_data.m_value = std::move(s);
			j.assert_invariant();
		}
		template < typename BasicJsonType, typename CompatibleStringType,
			enable_if_t < !std::is_same<CompatibleStringType, typename BasicJsonType::string_t>::value,
			int > = 0 >
		static void construct(BasicJsonType& j, const CompatibleStringType& str)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::string;
			j.m_data.m_value.string = j.template create<typename BasicJsonType::string_t>(str);
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::binary>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, const typename BasicJsonType::binary_t& b)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::binary;
			j.m_data.m_value = typename BasicJsonType::binary_t(b);
			j.assert_invariant();
		}
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::binary_t&& b)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::binary;
			j.m_data.m_value = typename BasicJsonType::binary_t(std::move(b));
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::number_float>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::number_float_t val) noexcept
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::number_float;
			j.m_data.m_value = val;
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::number_unsigned>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::number_unsigned_t val) noexcept
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::number_unsigned;
			j.m_data.m_value = val;
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::number_integer>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::number_integer_t val) noexcept
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::number_integer;
			j.m_data.m_value = val;
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::array>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, const typename BasicJsonType::array_t& arr)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::array;
			j.m_data.m_value = arr;
			j.set_parents();
			j.assert_invariant();
		}
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::array_t&& arr)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::array;
			j.m_data.m_value = std::move(arr);
			j.set_parents();
			j.assert_invariant();
		}
		template < typename BasicJsonType, typename CompatibleArrayType,
			enable_if_t < !std::is_same<CompatibleArrayType, typename BasicJsonType::array_t>::value,
			int > = 0 >
		static void construct(BasicJsonType& j, const CompatibleArrayType& arr)
		{
			using std::begin;
			using std::end;
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::array;
			j.m_data.m_value.array = j.template create<typename BasicJsonType::array_t>(begin(arr), end(arr));
			j.set_parents();
			j.assert_invariant();
		}
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, const std::vector<bool>& arr)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::array;
			j.m_data.m_value = value_t::array;
			j.m_data.m_value.array->reserve(arr.size());
			for (const bool x : arr)
			{
				j.m_data.m_value.array->push_back(x);
				j.set_parent(j.m_data.m_value.array->back());
			}
			j.assert_invariant();
		}
		template<typename BasicJsonType, typename T,
			enable_if_t<std::is_convertible<T, BasicJsonType>::value, int> = 0>
		static void construct(BasicJsonType& j, const std::valarray<T>& arr)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::array;
			j.m_data.m_value = value_t::array;
			j.m_data.m_value.array->resize(arr.size());
			if (arr.size() > 0)
			{
				std::copy(std::begin(arr), std::end(arr), j.m_data.m_value.array->begin());
			}
			j.set_parents();
			j.assert_invariant();
		}
	};
	template<>
	struct external_constructor<value_t::object>
	{
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, const typename BasicJsonType::object_t& obj)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::object;
			j.m_data.m_value = obj;
			j.set_parents();
			j.assert_invariant();
		}
		template<typename BasicJsonType>
		static void construct(BasicJsonType& j, typename BasicJsonType::object_t&& obj)
		{
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::object;
			j.m_data.m_value = std::move(obj);
			j.set_parents();
			j.assert_invariant();
		}
		template < typename BasicJsonType, typename CompatibleObjectType,
			enable_if_t < !std::is_same<CompatibleObjectType, typename BasicJsonType::object_t>::value, int > = 0 >
		static void construct(BasicJsonType& j, const CompatibleObjectType& obj)
		{
			using std::begin;
			using std::end;
			j.m_data.m_value.destroy(j.m_data.m_type);
			j.m_data.m_type = value_t::object;
			j.m_data.m_value.object = j.template create<typename BasicJsonType::object_t>(begin(obj), end(obj));
			j.set_parents();
			j.assert_invariant();
		}
	};
	template<typename BasicJsonType, typename T,
		enable_if_t<std::is_same<T, typename BasicJsonType::boolean_t>::value, int> = 0>
	inline void to_json(BasicJsonType& j, T b) noexcept
	{
		external_constructor<value_t::boolean>::construct(j, b);
	}
	template < typename BasicJsonType, typename BoolRef,
		enable_if_t <
		((std::is_same<std::vector<bool>::reference, BoolRef>::value
			&& !std::is_same <std::vector<bool>::reference, typename BasicJsonType::boolean_t&>::value)
			|| (std::is_same<std::vector<bool>::const_reference, BoolRef>::value
				&& !std::is_same <detail::uncvref_t<std::vector<bool>::const_reference>,
				typename BasicJsonType::boolean_t >::value))
		&& std::is_convertible<const BoolRef&, typename BasicJsonType::boolean_t>::value, int > = 0 >
	inline void to_json(BasicJsonType& j, const BoolRef& b) noexcept
	{
		external_constructor<value_t::boolean>::construct(j, static_cast<typename BasicJsonType::boolean_t>(b));
	}
	template<typename BasicJsonType, typename CompatibleString,
		enable_if_t<std::is_constructible<typename BasicJsonType::string_t, CompatibleString>::value, int> = 0>
	inline void to_json(BasicJsonType& j, const CompatibleString& s)
	{
		external_constructor<value_t::string>::construct(j, s);
	}
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, typename BasicJsonType::string_t&& s)
	{
		external_constructor<value_t::string>::construct(j, std::move(s));
	}
	template<typename BasicJsonType, typename FloatType,
		enable_if_t<std::is_floating_point<FloatType>::value, int> = 0>
	inline void to_json(BasicJsonType& j, FloatType val) noexcept
	{
		external_constructor<value_t::number_float>::construct(j, static_cast<typename BasicJsonType::number_float_t>(val));
	}
	template<typename BasicJsonType, typename CompatibleNumberUnsignedType,
		enable_if_t<is_compatible_integer_type<typename BasicJsonType::number_unsigned_t, CompatibleNumberUnsignedType>::value, int> = 0>
	inline void to_json(BasicJsonType& j, CompatibleNumberUnsignedType val) noexcept
	{
		external_constructor<value_t::number_unsigned>::construct(j, static_cast<typename BasicJsonType::number_unsigned_t>(val));
	}
	template<typename BasicJsonType, typename CompatibleNumberIntegerType,
		enable_if_t<is_compatible_integer_type<typename BasicJsonType::number_integer_t, CompatibleNumberIntegerType>::value, int> = 0>
	inline void to_json(BasicJsonType& j, CompatibleNumberIntegerType val) noexcept
	{
		external_constructor<value_t::number_integer>::construct(j, static_cast<typename BasicJsonType::number_integer_t>(val));
	}
#if !JSON_DISABLE_ENUM_SERIALIZATION
	template<typename BasicJsonType, typename EnumType,
		enable_if_t<std::is_enum<EnumType>::value, int> = 0>
	inline void to_json(BasicJsonType& j, EnumType e) noexcept
	{
		using underlying_type = typename std::underlying_type<EnumType>::type;
		static constexpr value_t integral_value_t = std::is_unsigned<underlying_type>::value ? value_t::number_unsigned : value_t::number_integer;
		external_constructor<integral_value_t>::construct(j, static_cast<underlying_type>(e));
	}
#endif  
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, const std::vector<bool>& e)
	{
		external_constructor<value_t::array>::construct(j, e);
	}
	template < typename BasicJsonType, typename CompatibleArrayType,
		enable_if_t < is_compatible_array_type<BasicJsonType,
		CompatibleArrayType>::value &&
		!is_compatible_object_type<BasicJsonType, CompatibleArrayType>::value &&
		!is_compatible_string_type<BasicJsonType, CompatibleArrayType>::value &&
		!std::is_same<typename BasicJsonType::binary_t, CompatibleArrayType>::value &&
		!is_basic_json<CompatibleArrayType>::value,
		int > = 0 >
	inline void to_json(BasicJsonType& j, const CompatibleArrayType& arr)
	{
		external_constructor<value_t::array>::construct(j, arr);
	}
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, const typename BasicJsonType::binary_t& bin)
	{
		external_constructor<value_t::binary>::construct(j, bin);
	}
	template<typename BasicJsonType, typename T,
		enable_if_t<std::is_convertible<T, BasicJsonType>::value, int> = 0>
	inline void to_json(BasicJsonType& j, const std::valarray<T>& arr)
	{
		external_constructor<value_t::array>::construct(j, std::move(arr));
	}
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, typename BasicJsonType::array_t&& arr)
	{
		external_constructor<value_t::array>::construct(j, std::move(arr));
	}
	template < typename BasicJsonType, typename CompatibleObjectType,
		enable_if_t < is_compatible_object_type<BasicJsonType, CompatibleObjectType>::value && !is_basic_json<CompatibleObjectType>::value, int > = 0 >
	inline void to_json(BasicJsonType& j, const CompatibleObjectType& obj)
	{
		external_constructor<value_t::object>::construct(j, obj);
	}
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, typename BasicJsonType::object_t&& obj)
	{
		external_constructor<value_t::object>::construct(j, std::move(obj));
	}
	template <
		typename BasicJsonType, typename T, std::size_t N,
		enable_if_t < !std::is_constructible<typename BasicJsonType::string_t,
		const T(&)[N]>::value,
		int > = 0 >
	inline void to_json(BasicJsonType& j, const T(&arr)[N])
	{
		external_constructor<value_t::array>::construct(j, arr);
	}
	template < typename BasicJsonType, typename T1, typename T2, enable_if_t < std::is_constructible<BasicJsonType, T1>::value&& std::is_constructible<BasicJsonType, T2>::value, int > = 0 >
	inline void to_json(BasicJsonType& j, const std::pair<T1, T2>& p)
	{
		j = { p.first, p.second };
	}
	template<typename BasicJsonType, typename T,
		enable_if_t<std::is_same<T, iteration_proxy_value<typename BasicJsonType::iterator>>::value, int> = 0>
	inline void to_json(BasicJsonType& j, const T& b)
	{
		j = { {b.key(), b.value()} };
	}
	template<typename BasicJsonType, typename Tuple, std::size_t... Idx>
	inline void to_json_tuple_impl(BasicJsonType& j, const Tuple& t, index_sequence<Idx...>)
	{
		j = { std::get<Idx>(t)... };
	}
	template<typename BasicJsonType, typename T, enable_if_t<is_constructible_tuple<BasicJsonType, T>::value, int > = 0>
	inline void to_json(BasicJsonType& j, const T& t)
	{
		to_json_tuple_impl(j, t, make_index_sequence<std::tuple_size<T>::value> {});
	}
#if JSON_HAS_FILESYSTEM || JSON_HAS_EXPERIMENTAL_FILESYSTEM
	template<typename BasicJsonType>
	inline void to_json(BasicJsonType& j, const std_fs::path& p)
	{
		j = p.string();
	}
#endif
	struct to_json_fn
	{
		template<typename BasicJsonType, typename T>
		auto operator()(BasicJsonType& j, T&& val) const noexcept(noexcept(to_json(j, std::forward<T>(val))))
			-> decltype(to_json(j, std::forward<T>(val)), void())
		{
			return to_json(j, std::forward<T>(val));
		}
	};
}
#ifndef JSON_HAS_CPP_17
namespace
{
#endif
	JSON_INLINE_VARIABLE constexpr const auto& to_json =
		detail::static_const<detail::to_json_fn>::value;
#ifndef JSON_HAS_CPP_17
}
#endif
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename ValueType, typename>
struct adl_serializer
{
	template<typename BasicJsonType, typename TargetType = ValueType>
	static auto from_json(BasicJsonType&& j, TargetType& val) noexcept(
		noexcept(::JsonLib::from_json(std::forward<BasicJsonType>(j), val)))
		-> decltype(::JsonLib::from_json(std::forward<BasicJsonType>(j), val), void())
	{
		::JsonLib::from_json(std::forward<BasicJsonType>(j), val);
	}
	template<typename BasicJsonType, typename TargetType = ValueType>
	static auto from_json(BasicJsonType&& j) noexcept(
		noexcept(::JsonLib::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType> {})))
		-> decltype(::JsonLib::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType> {}))
	{
		return ::JsonLib::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType> {});
	}
	template<typename BasicJsonType, typename TargetType = ValueType>
	static auto to_json(BasicJsonType& j, TargetType&& val) noexcept(
		noexcept(::JsonLib::to_json(j, std::forward<TargetType>(val))))
		-> decltype(::JsonLib::to_json(j, std::forward<TargetType>(val)), void())
	{
		::JsonLib::to_json(j, std::forward<TargetType>(val));
	}
};
NLOHMANN_JSON_NAMESPACE_END
#include <cstdint> 
#include <tuple> 
#include <utility> 
NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename BinaryType>
class byte_container_with_subtype : public BinaryType
{
public:
	using container_type = BinaryType;
	using subtype_type = std::uint64_t;
	byte_container_with_subtype() noexcept(noexcept(container_type()))
		: container_type()
	{
	}
	byte_container_with_subtype(const container_type& b) noexcept(noexcept(container_type(b)))
		: container_type(b)
	{
	}
	byte_container_with_subtype(container_type&& b) noexcept(noexcept(container_type(std::move(b))))
		: container_type(std::move(b))
	{
	}
	byte_container_with_subtype(const container_type& b, subtype_type subtype_) noexcept(noexcept(container_type(b)))
		: container_type(b)
		, m_subtype(subtype_)
		, m_has_subtype(true)
	{
	}
	byte_container_with_subtype(container_type&& b, subtype_type subtype_) noexcept(noexcept(container_type(std::move(b))))
		: container_type(std::move(b))
		, m_subtype(subtype_)
		, m_has_subtype(true)
	{
	}
	bool operator==(const byte_container_with_subtype& rhs) const
	{
		return std::tie(static_cast<const BinaryType&>(*this), m_subtype, m_has_subtype) ==
			std::tie(static_cast<const BinaryType&>(rhs), rhs.m_subtype, rhs.m_has_subtype);
	}
	bool operator!=(const byte_container_with_subtype& rhs) const
	{
		return !(rhs == *this);
	}
	void set_subtype(subtype_type subtype_) noexcept
	{
		m_subtype = subtype_;
		m_has_subtype = true;
	}
	constexpr subtype_type subtype() const noexcept
	{
		return m_has_subtype ? m_subtype : static_cast<subtype_type>(-1);
	}
	constexpr bool has_subtype() const noexcept
	{
		return m_has_subtype;
	}
	void clear_subtype() noexcept
	{
		m_subtype = 0;
		m_has_subtype = false;
	}
private:
	subtype_type m_subtype = 0;
	bool m_has_subtype = false;
};
NLOHMANN_JSON_NAMESPACE_END
#include <cstdint> 
#include <cstddef> 
#include <functional> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	inline std::size_t combine(std::size_t seed, std::size_t h) noexcept
	{
		seed ^= h + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
		return seed;
	}
	template<typename BasicJsonType>
	std::size_t hash(const BasicJsonType& j)
	{
		using string_t = typename BasicJsonType::string_t;
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		const auto type = static_cast<std::size_t>(j.type());
		switch (j.type())
		{
		case BasicJsonType::value_t::null:
		case BasicJsonType::value_t::discarded:
		{
			return combine(type, 0);
		}
		case BasicJsonType::value_t::object:
		{
			auto seed = combine(type, j.size());
			for (const auto& element : j.items())
			{
				const auto h = std::hash<string_t>{}(element.key());
				seed = combine(seed, h);
				seed = combine(seed, hash(element.value()));
			}
			return seed;
		}
		case BasicJsonType::value_t::array:
		{
			auto seed = combine(type, j.size());
			for (const auto& element : j)
			{
				seed = combine(seed, hash(element));
			}
			return seed;
		}
		case BasicJsonType::value_t::string:
		{
			const auto h = std::hash<string_t>{}(j.template get_ref<const string_t&>());
			return combine(type, h);
		}
		case BasicJsonType::value_t::boolean:
		{
			const auto h = std::hash<bool>{}(j.template get<bool>());
			return combine(type, h);
		}
		case BasicJsonType::value_t::number_integer:
		{
			const auto h = std::hash<number_integer_t>{}(j.template get<number_integer_t>());
			return combine(type, h);
		}
		case BasicJsonType::value_t::number_unsigned:
		{
			const auto h = std::hash<number_unsigned_t>{}(j.template get<number_unsigned_t>());
			return combine(type, h);
		}
		case BasicJsonType::value_t::number_float:
		{
			const auto h = std::hash<number_float_t>{}(j.template get<number_float_t>());
			return combine(type, h);
		}
		case BasicJsonType::value_t::binary:
		{
			auto seed = combine(type, j.get_binary().size());
			const auto h = std::hash<bool>{}(j.get_binary().has_subtype());
			seed = combine(seed, h);
			seed = combine(seed, static_cast<std::size_t>(j.get_binary().subtype()));
			for (const auto byte : j.get_binary())
			{
				seed = combine(seed, std::hash<std::uint8_t> {}(byte));
			}
			return seed;
		}
		default:
			JSON_ASSERT(false);
			return 0;
		}
	}
}
NLOHMANN_JSON_NAMESPACE_END
#include <algorithm> 
#include <array> 
#include <cmath> 
#include <cstddef> 
#include <cstdint> 
#include <cstdio> 
#include <cstring> 
#include <iterator> 
#include <limits> 
#include <string> 
#include <utility> 
#include <vector> 
#include <array> 
#include <cstddef> 
#include <cstring> 
#include <iterator> 
#include <memory> 
#include <numeric> 
#include <string> 
#include <type_traits> 
#include <utility> 
#ifndef JSON_NO_IO
#include <cstdio>   
#include <istream>  
#endif                  
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	enum class input_format_t { json, cbor, msgpack, ubjson, bson, bjdata };
#ifndef JSON_NO_IO
	class file_input_adapter
	{
	public:
		using char_type = char;
		JSON_HEDLEY_NON_NULL(2)
			explicit file_input_adapter(std::FILE* f) noexcept
			: m_file(f)
		{
			JSON_ASSERT(m_file != nullptr);
		}
		file_input_adapter(const file_input_adapter&) = delete;
		file_input_adapter(file_input_adapter&&) noexcept = default;
		file_input_adapter& operator=(const file_input_adapter&) = delete;
		file_input_adapter& operator=(file_input_adapter&&) = delete;
		~file_input_adapter() = default;
		std::char_traits<char>::int_type get_character() noexcept
		{
			return std::fgetc(m_file);
		}
	private:
		std::FILE* m_file;
	};
	class input_stream_adapter
	{
	public:
		using char_type = char;
		~input_stream_adapter()
		{
			if (is != nullptr)
			{
				is->clear(is->rdstate() & std::ios::eofbit);
			}
		}
		explicit input_stream_adapter(std::istream& i)
			: is(&i), sb(i.rdbuf())
		{
		}
		input_stream_adapter(const input_stream_adapter&) = delete;
		input_stream_adapter& operator=(input_stream_adapter&) = delete;
		input_stream_adapter& operator=(input_stream_adapter&&) = delete;
		input_stream_adapter(input_stream_adapter&& rhs) noexcept
			: is(rhs.is), sb(rhs.sb)
		{
			rhs.is = nullptr;
			rhs.sb = nullptr;
		}
		std::char_traits<char>::int_type get_character()
		{
			auto res = sb->sbumpc();
			if (JSON_HEDLEY_UNLIKELY(res == std::char_traits<char>::eof()))
			{
				is->clear(is->rdstate() | std::ios::eofbit);
			}
			return res;
		}
	private:
		std::istream* is = nullptr;
		std::streambuf* sb = nullptr;
	};
#endif  
	template<typename IteratorType>
	class iterator_input_adapter
	{
	public:
		using char_type = typename std::iterator_traits<IteratorType>::value_type;
		iterator_input_adapter(IteratorType first, IteratorType last)
			: current(std::move(first)), end(std::move(last))
		{
		}
		typename char_traits<char_type>::int_type get_character()
		{
			if (JSON_HEDLEY_LIKELY(current != end))
			{
				auto result = char_traits<char_type>::to_int_type(*current);
				std::advance(current, 1);
				return result;
			}
			return char_traits<char_type>::eof();
		}
	private:
		IteratorType current;
		IteratorType end;
		template<typename BaseInputAdapter, size_t T>
		friend struct wide_string_input_helper;
		bool empty() const
		{
			return current == end;
		}
	};
	template<typename BaseInputAdapter, size_t T>
	struct wide_string_input_helper;
	template<typename BaseInputAdapter>
	struct wide_string_input_helper<BaseInputAdapter, 4>
	{
		static void fill_buffer(BaseInputAdapter& input,
			std::array<std::char_traits<char>::int_type, 4>& utf8_bytes,
			size_t& utf8_bytes_index,
			size_t& utf8_bytes_filled)
		{
			utf8_bytes_index = 0;
			if (JSON_HEDLEY_UNLIKELY(input.empty()))
			{
				utf8_bytes[0] = std::char_traits<char>::eof();
				utf8_bytes_filled = 1;
			}
			else
			{
				const auto wc = input.get_character();
				if (wc < 0x80)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(wc);
					utf8_bytes_filled = 1;
				}
				else if (wc <= 0x7FF)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xC0u | ((static_cast<unsigned int>(wc) >> 6u) & 0x1Fu));
					utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | (static_cast<unsigned int>(wc) & 0x3Fu));
					utf8_bytes_filled = 2;
				}
				else if (wc <= 0xFFFF)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xE0u | ((static_cast<unsigned int>(wc) >> 12u) & 0x0Fu));
					utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | ((static_cast<unsigned int>(wc) >> 6u) & 0x3Fu));
					utf8_bytes[2] = static_cast<std::char_traits<char>::int_type>(0x80u | (static_cast<unsigned int>(wc) & 0x3Fu));
					utf8_bytes_filled = 3;
				}
				else if (wc <= 0x10FFFF)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xF0u | ((static_cast<unsigned int>(wc) >> 18u) & 0x07u));
					utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | ((static_cast<unsigned int>(wc) >> 12u) & 0x3Fu));
					utf8_bytes[2] = static_cast<std::char_traits<char>::int_type>(0x80u | ((static_cast<unsigned int>(wc) >> 6u) & 0x3Fu));
					utf8_bytes[3] = static_cast<std::char_traits<char>::int_type>(0x80u | (static_cast<unsigned int>(wc) & 0x3Fu));
					utf8_bytes_filled = 4;
				}
				else
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(wc);
					utf8_bytes_filled = 1;
				}
			}
		}
	};
	template<typename BaseInputAdapter>
	struct wide_string_input_helper<BaseInputAdapter, 2>
	{
		static void fill_buffer(BaseInputAdapter& input,
			std::array<std::char_traits<char>::int_type, 4>& utf8_bytes,
			size_t& utf8_bytes_index,
			size_t& utf8_bytes_filled)
		{
			utf8_bytes_index = 0;
			if (JSON_HEDLEY_UNLIKELY(input.empty()))
			{
				utf8_bytes[0] = std::char_traits<char>::eof();
				utf8_bytes_filled = 1;
			}
			else
			{
				const auto wc = input.get_character();
				if (wc < 0x80)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(wc);
					utf8_bytes_filled = 1;
				}
				else if (wc <= 0x7FF)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xC0u | ((static_cast<unsigned int>(wc) >> 6u)));
					utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | (static_cast<unsigned int>(wc) & 0x3Fu));
					utf8_bytes_filled = 2;
				}
				else if (0xD800 > wc || wc >= 0xE000)
				{
					utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xE0u | ((static_cast<unsigned int>(wc) >> 12u)));
					utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | ((static_cast<unsigned int>(wc) >> 6u) & 0x3Fu));
					utf8_bytes[2] = static_cast<std::char_traits<char>::int_type>(0x80u | (static_cast<unsigned int>(wc) & 0x3Fu));
					utf8_bytes_filled = 3;
				}
				else
				{
					if (JSON_HEDLEY_UNLIKELY(!input.empty()))
					{
						const auto wc2 = static_cast<unsigned int>(input.get_character());
						const auto charcode = 0x10000u + (((static_cast<unsigned int>(wc) & 0x3FFu) << 10u) | (wc2 & 0x3FFu));
						utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(0xF0u | (charcode >> 18u));
						utf8_bytes[1] = static_cast<std::char_traits<char>::int_type>(0x80u | ((charcode >> 12u) & 0x3Fu));
						utf8_bytes[2] = static_cast<std::char_traits<char>::int_type>(0x80u | ((charcode >> 6u) & 0x3Fu));
						utf8_bytes[3] = static_cast<std::char_traits<char>::int_type>(0x80u | (charcode & 0x3Fu));
						utf8_bytes_filled = 4;
					}
					else
					{
						utf8_bytes[0] = static_cast<std::char_traits<char>::int_type>(wc);
						utf8_bytes_filled = 1;
					}
				}
			}
		}
	};
	template<typename BaseInputAdapter, typename WideCharType>
	class wide_string_input_adapter
	{
	public:
		using char_type = char;
		wide_string_input_adapter(BaseInputAdapter base)
			: base_adapter(base) {
		}
		typename std::char_traits<char>::int_type get_character() noexcept
		{
			if (utf8_bytes_index == utf8_bytes_filled)
			{
				fill_buffer<sizeof(WideCharType)>();
				JSON_ASSERT(utf8_bytes_filled > 0);
				JSON_ASSERT(utf8_bytes_index == 0);
			}
			JSON_ASSERT(utf8_bytes_filled > 0);
			JSON_ASSERT(utf8_bytes_index < utf8_bytes_filled);
			return utf8_bytes[utf8_bytes_index++];
		}
	private:
		BaseInputAdapter base_adapter;
		template<size_t T>
		void fill_buffer()
		{
			wide_string_input_helper<BaseInputAdapter, T>::fill_buffer(base_adapter, utf8_bytes, utf8_bytes_index, utf8_bytes_filled);
		}
		std::array<std::char_traits<char>::int_type, 4> utf8_bytes = { {0, 0, 0, 0} };
		std::size_t utf8_bytes_index = 0;
		std::size_t utf8_bytes_filled = 0;
	};
	template<typename IteratorType, typename Enable = void>
	struct iterator_input_adapter_factory
	{
		using iterator_type = IteratorType;
		using char_type = typename std::iterator_traits<iterator_type>::value_type;
		using adapter_type = iterator_input_adapter<iterator_type>;
		static adapter_type create(IteratorType first, IteratorType last)
		{
			return adapter_type(std::move(first), std::move(last));
		}
	};
	template<typename T>
	struct is_iterator_of_multibyte
	{
		using value_type = typename std::iterator_traits<T>::value_type;
		enum
		{
			value = sizeof(value_type) > 1
		};
	};
	template<typename IteratorType>
	struct iterator_input_adapter_factory<IteratorType, enable_if_t<is_iterator_of_multibyte<IteratorType>::value>>
	{
		using iterator_type = IteratorType;
		using char_type = typename std::iterator_traits<iterator_type>::value_type;
		using base_adapter_type = iterator_input_adapter<iterator_type>;
		using adapter_type = wide_string_input_adapter<base_adapter_type, char_type>;
		static adapter_type create(IteratorType first, IteratorType last)
		{
			return adapter_type(base_adapter_type(std::move(first), std::move(last)));
		}
	};
	template<typename IteratorType>
	typename iterator_input_adapter_factory<IteratorType>::adapter_type input_adapter(IteratorType first, IteratorType last)
	{
		using factory_type = iterator_input_adapter_factory<IteratorType>;
		return factory_type::create(first, last);
	}
	namespace container_input_adapter_factory_impl
	{
		using std::begin;
		using std::end;
		template<typename ContainerType, typename Enable = void>
		struct container_input_adapter_factory {};
		template<typename ContainerType>
		struct container_input_adapter_factory< ContainerType,
			void_t<decltype(begin(std::declval<ContainerType>()), end(std::declval<ContainerType>()))>>
		{
			using adapter_type = decltype(input_adapter(begin(std::declval<ContainerType>()), end(std::declval<ContainerType>())));
			static adapter_type create(const ContainerType& container)
			{
				return input_adapter(begin(container), end(container));
			}
		};
	}
	template<typename ContainerType>
	typename container_input_adapter_factory_impl::container_input_adapter_factory<ContainerType>::adapter_type input_adapter(const ContainerType& container)
	{
		return container_input_adapter_factory_impl::container_input_adapter_factory<ContainerType>::create(container);
	}
#ifndef JSON_NO_IO
	inline file_input_adapter input_adapter(std::FILE* file)
	{
		return file_input_adapter(file);
	}
	inline input_stream_adapter input_adapter(std::istream& stream)
	{
		return input_stream_adapter(stream);
	}
	inline input_stream_adapter input_adapter(std::istream&& stream)
	{
		return input_stream_adapter(stream);
	}
#endif  
	using contiguous_bytes_input_adapter = decltype(input_adapter(std::declval<const char*>(), std::declval<const char*>()));
	template < typename CharT,
		typename std::enable_if <
		std::is_pointer<CharT>::value &&
		!std::is_array<CharT>::value&&
		std::is_integral<typename std::remove_pointer<CharT>::type>::value &&
		sizeof(typename std::remove_pointer<CharT>::type) == 1,
		int >::type = 0 >
	contiguous_bytes_input_adapter input_adapter(CharT b)
	{
		auto length = std::strlen(reinterpret_cast<const char*>(b));
		const auto* ptr = reinterpret_cast<const char*>(b);
		return input_adapter(ptr, ptr + length);
	}
	template<typename T, std::size_t N>
	auto input_adapter(T(&array)[N]) -> decltype(input_adapter(array, array + N))
	{
		return input_adapter(array, array + N);
	}
	class span_input_adapter
	{
	public:
		template < typename CharT,
			typename std::enable_if <
			std::is_pointer<CharT>::value&&
			std::is_integral<typename std::remove_pointer<CharT>::type>::value &&
			sizeof(typename std::remove_pointer<CharT>::type) == 1,
			int >::type = 0 >
		span_input_adapter(CharT b, std::size_t l)
			: ia(reinterpret_cast<const char*>(b), reinterpret_cast<const char*>(b) + l) {
		}
		template<class IteratorType,
			typename std::enable_if<
			std::is_same<typename iterator_traits<IteratorType>::iterator_category, std::random_access_iterator_tag>::value,
			int>::type = 0>
		span_input_adapter(IteratorType first, IteratorType last)
			: ia(input_adapter(first, last)) {
		}
		contiguous_bytes_input_adapter&& get()
		{
			return std::move(ia);
		}
	private:
		contiguous_bytes_input_adapter ia;
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstddef>
#include <string> 
#include <utility> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename BasicJsonType>
struct json_sax
{
	using number_integer_t = typename BasicJsonType::number_integer_t;
	using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
	using number_float_t = typename BasicJsonType::number_float_t;
	using string_t = typename BasicJsonType::string_t;
	using binary_t = typename BasicJsonType::binary_t;
	virtual bool null() = 0;
	virtual bool boolean(bool val) = 0;
	virtual bool number_integer(number_integer_t val) = 0;
	virtual bool number_unsigned(number_unsigned_t val) = 0;
	virtual bool number_float(number_float_t val, const string_t& s) = 0;
	virtual bool string(string_t& val) = 0;
	virtual bool binary(binary_t& val) = 0;
	virtual bool start_object(std::size_t elements) = 0;
	virtual bool key(string_t& val) = 0;
	virtual bool end_object() = 0;
	virtual bool start_array(std::size_t elements) = 0;
	virtual bool end_array() = 0;
	virtual bool parse_error(std::size_t position,
		const std::string& last_token,
		const detail::exception& ex) = 0;
	json_sax() = default;
	json_sax(const json_sax&) = default;
	json_sax(json_sax&&) noexcept = default;
	json_sax& operator=(const json_sax&) = default;
	json_sax& operator=(json_sax&&) noexcept = default;
	virtual ~json_sax() = default;
};
namespace detail
{
	template<typename BasicJsonType>
	class json_sax_dom_parser
	{
	public:
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		explicit json_sax_dom_parser(BasicJsonType& r, const bool allow_exceptions_ = true)
			: root(r), allow_exceptions(allow_exceptions_)
		{
		}
		json_sax_dom_parser(const json_sax_dom_parser&) = delete;
		json_sax_dom_parser(json_sax_dom_parser&&) = default;
		json_sax_dom_parser& operator=(const json_sax_dom_parser&) = delete;
		json_sax_dom_parser& operator=(json_sax_dom_parser&&) = default;
		~json_sax_dom_parser() = default;
		bool null()
		{
			handle_value(nullptr);
			return true;
		}
		bool boolean(bool val)
		{
			handle_value(val);
			return true;
		}
		bool number_integer(number_integer_t val)
		{
			handle_value(val);
			return true;
		}
		bool number_unsigned(number_unsigned_t val)
		{
			handle_value(val);
			return true;
		}
		bool number_float(number_float_t val, const string_t&)
		{
			handle_value(val);
			return true;
		}
		bool string(string_t& val)
		{
			handle_value(val);
			return true;
		}
		bool binary(binary_t& val)
		{
			handle_value(std::move(val));
			return true;
		}
		bool start_object(std::size_t len)
		{
			ref_stack.push_back(handle_value(BasicJsonType::value_t::object));
			if (JSON_HEDLEY_UNLIKELY(len != static_cast<std::size_t>(-1) && len > ref_stack.back()->max_size()))
			{
				JSON_THROW(out_of_range::create(408, concat("excessive object size: ", std::to_string(len)), ref_stack.back()));
			}
			return true;
		}
		bool key(string_t& val)
		{
			JSON_ASSERT(!ref_stack.empty());
			JSON_ASSERT(ref_stack.back()->is_object());
			object_element = &(ref_stack.back()->m_data.m_value.object->operator[](val));
			return true;
		}
		bool end_object()
		{
			JSON_ASSERT(!ref_stack.empty());
			JSON_ASSERT(ref_stack.back()->is_object());
			ref_stack.back()->set_parents();
			ref_stack.pop_back();
			return true;
		}
		bool start_array(std::size_t len)
		{
			ref_stack.push_back(handle_value(BasicJsonType::value_t::array));
			if (JSON_HEDLEY_UNLIKELY(len != static_cast<std::size_t>(-1) && len > ref_stack.back()->max_size()))
			{
				JSON_THROW(out_of_range::create(408, concat("excessive array size: ", std::to_string(len)), ref_stack.back()));
			}
			return true;
		}
		bool end_array()
		{
			JSON_ASSERT(!ref_stack.empty());
			JSON_ASSERT(ref_stack.back()->is_array());
			ref_stack.back()->set_parents();
			ref_stack.pop_back();
			return true;
		}
		template<class Exception>
		bool parse_error(std::size_t, const std::string&,
			const Exception& ex)
		{
			errored = true;
			static_cast<void>(ex);
			if (allow_exceptions)
			{
				JSON_THROW(ex);
			}
			return false;
		}
		constexpr bool is_errored() const
		{
			return errored;
		}
	private:
		template<typename Value>
		JSON_HEDLEY_RETURNS_NON_NULL
			BasicJsonType* handle_value(Value&& v)
		{
			if (ref_stack.empty())
			{
				root = BasicJsonType(std::forward<Value>(v));
				return &root;
			}
			JSON_ASSERT(ref_stack.back()->is_array() || ref_stack.back()->is_object());
			if (ref_stack.back()->is_array())
			{
				ref_stack.back()->m_data.m_value.array->emplace_back(std::forward<Value>(v));
				return &(ref_stack.back()->m_data.m_value.array->back());
			}
			JSON_ASSERT(ref_stack.back()->is_object());
			JSON_ASSERT(object_element);
			*object_element = BasicJsonType(std::forward<Value>(v));
			return object_element;
		}
		BasicJsonType& root;
		std::vector<BasicJsonType*> ref_stack{};
		BasicJsonType* object_element = nullptr;
		bool errored = false;
		const bool allow_exceptions = true;
	};
	template<typename BasicJsonType>
	class json_sax_dom_callback_parser
	{
	public:
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		using parser_callback_t = typename BasicJsonType::parser_callback_t;
		using parse_event_t = typename BasicJsonType::parse_event_t;
		json_sax_dom_callback_parser(BasicJsonType& r,
			const parser_callback_t cb,
			const bool allow_exceptions_ = true)
			: root(r), callback(cb), allow_exceptions(allow_exceptions_)
		{
			keep_stack.push_back(true);
		}
		json_sax_dom_callback_parser(const json_sax_dom_callback_parser&) = delete;
		json_sax_dom_callback_parser(json_sax_dom_callback_parser&&) = default;
		json_sax_dom_callback_parser& operator=(const json_sax_dom_callback_parser&) = delete;
		json_sax_dom_callback_parser& operator=(json_sax_dom_callback_parser&&) = default;
		~json_sax_dom_callback_parser() = default;
		bool null()
		{
			handle_value(nullptr);
			return true;
		}
		bool boolean(bool val)
		{
			handle_value(val);
			return true;
		}
		bool number_integer(number_integer_t val)
		{
			handle_value(val);
			return true;
		}
		bool number_unsigned(number_unsigned_t val)
		{
			handle_value(val);
			return true;
		}
		bool number_float(number_float_t val, const string_t&)
		{
			handle_value(val);
			return true;
		}
		bool string(string_t& val)
		{
			handle_value(val);
			return true;
		}
		bool binary(binary_t& val)
		{
			handle_value(std::move(val));
			return true;
		}
		bool start_object(std::size_t len)
		{
			const bool keep = callback(static_cast<int>(ref_stack.size()), parse_event_t::object_start, discarded);
			keep_stack.push_back(keep);
			auto val = handle_value(BasicJsonType::value_t::object, true);
			ref_stack.push_back(val.second);
			if (ref_stack.back() && JSON_HEDLEY_UNLIKELY(len != static_cast<std::size_t>(-1) && len > ref_stack.back()->max_size()))
			{
				JSON_THROW(out_of_range::create(408, concat("excessive object size: ", std::to_string(len)), ref_stack.back()));
			}
			return true;
		}
		bool key(string_t& val)
		{
			BasicJsonType k = BasicJsonType(val);
			const bool keep = callback(static_cast<int>(ref_stack.size()), parse_event_t::key, k);
			key_keep_stack.push_back(keep);
			if (keep && ref_stack.back())
			{
				object_element = &(ref_stack.back()->m_data.m_value.object->operator[](val) = discarded);
			}
			return true;
		}
		bool end_object()
		{
			if (ref_stack.back())
			{
				if (!callback(static_cast<int>(ref_stack.size()) - 1, parse_event_t::object_end, *ref_stack.back()))
				{
					*ref_stack.back() = discarded;
				}
				else
				{
					ref_stack.back()->set_parents();
				}
			}
			JSON_ASSERT(!ref_stack.empty());
			JSON_ASSERT(!keep_stack.empty());
			ref_stack.pop_back();
			keep_stack.pop_back();
			if (!ref_stack.empty() && ref_stack.back() && ref_stack.back()->is_structured())
			{
				for (auto it = ref_stack.back()->begin(); it != ref_stack.back()->end(); ++it)
				{
					if (it->is_discarded())
					{
						ref_stack.back()->erase(it);
						break;
					}
				}
			}
			return true;
		}
		bool start_array(std::size_t len)
		{
			const bool keep = callback(static_cast<int>(ref_stack.size()), parse_event_t::array_start, discarded);
			keep_stack.push_back(keep);
			auto val = handle_value(BasicJsonType::value_t::array, true);
			ref_stack.push_back(val.second);
			if (ref_stack.back() && JSON_HEDLEY_UNLIKELY(len != static_cast<std::size_t>(-1) && len > ref_stack.back()->max_size()))
			{
				JSON_THROW(out_of_range::create(408, concat("excessive array size: ", std::to_string(len)), ref_stack.back()));
			}
			return true;
		}
		bool end_array()
		{
			bool keep = true;
			if (ref_stack.back())
			{
				keep = callback(static_cast<int>(ref_stack.size()) - 1, parse_event_t::array_end, *ref_stack.back());
				if (keep)
				{
					ref_stack.back()->set_parents();
				}
				else
				{
					*ref_stack.back() = discarded;
				}
			}
			JSON_ASSERT(!ref_stack.empty());
			JSON_ASSERT(!keep_stack.empty());
			ref_stack.pop_back();
			keep_stack.pop_back();
			if (!keep && !ref_stack.empty() && ref_stack.back()->is_array())
			{
				ref_stack.back()->m_data.m_value.array->pop_back();
			}
			return true;
		}
		template<class Exception>
		bool parse_error(std::size_t, const std::string&,
			const Exception& ex)
		{
			errored = true;
			static_cast<void>(ex);
			if (allow_exceptions)
			{
				JSON_THROW(ex);
			}
			return false;
		}
		constexpr bool is_errored() const
		{
			return errored;
		}
	private:
		template<typename Value>
		std::pair<bool, BasicJsonType*> handle_value(Value&& v, const bool skip_callback = false)
		{
			JSON_ASSERT(!keep_stack.empty());
			if (!keep_stack.back())
			{
				return { false, nullptr };
			}
			auto value = BasicJsonType(std::forward<Value>(v));
			const bool keep = skip_callback || callback(static_cast<int>(ref_stack.size()), parse_event_t::value, value);
			if (!keep)
			{
				return { false, nullptr };
			}
			if (ref_stack.empty())
			{
				root = std::move(value);
				return { true, &root };
			}
			if (!ref_stack.back())
			{
				return { false, nullptr };
			}
			JSON_ASSERT(ref_stack.back()->is_array() || ref_stack.back()->is_object());
			if (ref_stack.back()->is_array())
			{
				ref_stack.back()->m_data.m_value.array->emplace_back(std::move(value));
				return { true, &(ref_stack.back()->m_data.m_value.array->back()) };
			}
			JSON_ASSERT(ref_stack.back()->is_object());
			JSON_ASSERT(!key_keep_stack.empty());
			const bool store_element = key_keep_stack.back();
			key_keep_stack.pop_back();
			if (!store_element)
			{
				return { false, nullptr };
			}
			JSON_ASSERT(object_element);
			*object_element = std::move(value);
			return { true, object_element };
		}
		BasicJsonType& root;
		std::vector<BasicJsonType*> ref_stack{};
		std::vector<bool> keep_stack{};
		std::vector<bool> key_keep_stack{};
		BasicJsonType* object_element = nullptr;
		bool errored = false;
		const parser_callback_t callback = nullptr;
		const bool allow_exceptions = true;
		BasicJsonType discarded = BasicJsonType::value_t::discarded;
	};
	template<typename BasicJsonType>
	class json_sax_acceptor
	{
	public:
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		bool null()
		{
			return true;
		}
		bool boolean(bool)
		{
			return true;
		}
		bool number_integer(number_integer_t)
		{
			return true;
		}
		bool number_unsigned(number_unsigned_t)
		{
			return true;
		}
		bool number_float(number_float_t, const string_t&)
		{
			return true;
		}
		bool string(string_t&)
		{
			return true;
		}
		bool binary(binary_t&)
		{
			return true;
		}
		bool start_object(std::size_t = static_cast<std::size_t>(-1))
		{
			return true;
		}
		bool key(string_t&)
		{
			return true;
		}
		bool end_object()
		{
			return true;
		}
		bool start_array(std::size_t = static_cast<std::size_t>(-1))
		{
			return true;
		}
		bool end_array()
		{
			return true;
		}
		bool parse_error(std::size_t, const std::string&, const detail::exception&)
		{
			return false;
		}
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <array> 
#include <clocale> 
#include <cstddef> 
#include <cstdio> 
#include <cstdlib> 
#include <initializer_list> 
#include <string> 
#include <utility> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename BasicJsonType>
	class lexer_base
	{
	public:
		enum class token_type
		{
			uninitialized,
			literal_true,
			literal_false,
			literal_null,
			value_string,
			value_unsigned,
			value_integer,
			value_float,
			begin_array,
			begin_object,
			end_array,
			end_object,
			name_separator,
			value_separator,
			parse_error,
			end_of_input,
			literal_or_value
		};
		JSON_HEDLEY_RETURNS_NON_NULL
			JSON_HEDLEY_CONST
			static const char* token_type_name(const token_type t) noexcept
		{
			switch (t)
			{
			case token_type::uninitialized:
				return "<uninitialized>";
			case token_type::literal_true:
				return "true literal";
			case token_type::literal_false:
				return "false literal";
			case token_type::literal_null:
				return "null literal";
			case token_type::value_string:
				return "string literal";
			case token_type::value_unsigned:
			case token_type::value_integer:
			case token_type::value_float:
				return "number literal";
			case token_type::begin_array:
				return "'['";
			case token_type::begin_object:
				return "'{'";
			case token_type::end_array:
				return "']'";
			case token_type::end_object:
				return "'}'";
			case token_type::name_separator:
				return "':'";
			case token_type::value_separator:
				return "','";
			case token_type::parse_error:
				return "<parse error>";
			case token_type::end_of_input:
				return "end of input";
			case token_type::literal_or_value:
				return "'[', '{', or a literal";
			default:
				return "unknown token";
			}
		}
	};
	template<typename BasicJsonType, typename InputAdapterType>
	class lexer : public lexer_base<BasicJsonType>
	{
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using char_type = typename InputAdapterType::char_type;
		using char_int_type = typename char_traits<char_type>::int_type;
	public:
		using token_type = typename lexer_base<BasicJsonType>::token_type;
		explicit lexer(InputAdapterType&& adapter, bool ignore_comments_ = false) noexcept
			: ia(std::move(adapter))
			, ignore_comments(ignore_comments_)
			, decimal_point_char(static_cast<char_int_type>(get_decimal_point()))
		{
		}
		lexer(const lexer&) = delete;
		lexer(lexer&&) = default;
		lexer& operator=(lexer&) = delete;
		lexer& operator=(lexer&&) = default;
		~lexer() = default;
	private:
		JSON_HEDLEY_PURE
			static char get_decimal_point() noexcept
		{
			const auto* loc = localeconv();
			JSON_ASSERT(loc != nullptr);
			return (loc->decimal_point == nullptr) ? '.' : *(loc->decimal_point);
		}
		int get_codepoint()
		{
			JSON_ASSERT(current == 'u');
			int codepoint = 0;
			const auto factors = { 12u, 8u, 4u, 0u };
			for (const auto factor : factors)
			{
				get();
				if (current >= '0' && current <= '9')
				{
					codepoint += static_cast<int>((static_cast<unsigned int>(current) - 0x30u) << factor);
				}
				else if (current >= 'A' && current <= 'F')
				{
					codepoint += static_cast<int>((static_cast<unsigned int>(current) - 0x37u) << factor);
				}
				else if (current >= 'a' && current <= 'f')
				{
					codepoint += static_cast<int>((static_cast<unsigned int>(current) - 0x57u) << factor);
				}
				else
				{
					return -1;
				}
			}
			JSON_ASSERT(0x0000 <= codepoint && codepoint <= 0xFFFF);
			return codepoint;
		}
		bool next_byte_in_range(std::initializer_list<char_int_type> ranges)
		{
			JSON_ASSERT(ranges.size() == 2 || ranges.size() == 4 || ranges.size() == 6);
			add(current);
			for (auto range = ranges.begin(); range != ranges.end(); ++range)
			{
				get();
				if (JSON_HEDLEY_LIKELY(*range <= current && current <= *(++range)))
				{
					add(current);
				}
				else
				{
					error_message = "invalid string: ill-formed UTF-8 byte";
					return false;
				}
			}
			return true;
		}
		token_type scan_string()
		{
			reset();
			JSON_ASSERT(current == '\"');
			while (true)
			{
				switch (get())
				{
				case char_traits<char_type>::eof():
				{
					error_message = "invalid string: missing closing quote";
					return token_type::parse_error;
				}
				case '\"':
				{
					return token_type::value_string;
				}
				case '\\':
				{
					switch (get())
					{
					case '\"':
						add('\"');
						break;
					case '\\':
						add('\\');
						break;
					case '/':
						add('/');
						break;
					case 'b':
						add('\b');
						break;
					case 'f':
						add('\f');
						break;
					case 'n':
						add('\n');
						break;
					case 'r':
						add('\r');
						break;
					case 't':
						add('\t');
						break;
					case 'u':
					{
						const int codepoint1 = get_codepoint();
						int codepoint = codepoint1;
						if (JSON_HEDLEY_UNLIKELY(codepoint1 == -1))
						{
							error_message = "invalid string: '\\u' must be followed by 4 hex digits";
							return token_type::parse_error;
						}
						if (0xD800 <= codepoint1 && codepoint1 <= 0xDBFF)
						{
							if (JSON_HEDLEY_LIKELY(get() == '\\' && get() == 'u'))
							{
								const int codepoint2 = get_codepoint();
								if (JSON_HEDLEY_UNLIKELY(codepoint2 == -1))
								{
									error_message = "invalid string: '\\u' must be followed by 4 hex digits";
									return token_type::parse_error;
								}
								if (JSON_HEDLEY_LIKELY(0xDC00 <= codepoint2 && codepoint2 <= 0xDFFF))
								{
									codepoint = static_cast<int>(
										(static_cast<unsigned int>(codepoint1) << 10u)
										+ static_cast<unsigned int>(codepoint2)
										- 0x35FDC00u);
								}
								else
								{
									error_message = "invalid string: surrogate U+D800..U+DBFF must be followed by U+DC00..U+DFFF";
									return token_type::parse_error;
								}
							}
							else
							{
								error_message = "invalid string: surrogate U+D800..U+DBFF must be followed by U+DC00..U+DFFF";
								return token_type::parse_error;
							}
						}
						else
						{
							if (JSON_HEDLEY_UNLIKELY(0xDC00 <= codepoint1 && codepoint1 <= 0xDFFF))
							{
								error_message = "invalid string: surrogate U+DC00..U+DFFF must follow U+D800..U+DBFF";
								return token_type::parse_error;
							}
						}
						JSON_ASSERT(0x00 <= codepoint && codepoint <= 0x10FFFF);
						if (codepoint < 0x80)
						{
							add(static_cast<char_int_type>(codepoint));
						}
						else if (codepoint <= 0x7FF)
						{
							add(static_cast<char_int_type>(0xC0u | (static_cast<unsigned int>(codepoint) >> 6u)));
							add(static_cast<char_int_type>(0x80u | (static_cast<unsigned int>(codepoint) & 0x3Fu)));
						}
						else if (codepoint <= 0xFFFF)
						{
							add(static_cast<char_int_type>(0xE0u | (static_cast<unsigned int>(codepoint) >> 12u)));
							add(static_cast<char_int_type>(0x80u | ((static_cast<unsigned int>(codepoint) >> 6u) & 0x3Fu)));
							add(static_cast<char_int_type>(0x80u | (static_cast<unsigned int>(codepoint) & 0x3Fu)));
						}
						else
						{
							add(static_cast<char_int_type>(0xF0u | (static_cast<unsigned int>(codepoint) >> 18u)));
							add(static_cast<char_int_type>(0x80u | ((static_cast<unsigned int>(codepoint) >> 12u) & 0x3Fu)));
							add(static_cast<char_int_type>(0x80u | ((static_cast<unsigned int>(codepoint) >> 6u) & 0x3Fu)));
							add(static_cast<char_int_type>(0x80u | (static_cast<unsigned int>(codepoint) & 0x3Fu)));
						}
						break;
					}
					default:
						error_message = "invalid string: forbidden character after backslash";
						return token_type::parse_error;
					}
					break;
				}
				case 0x00:
				{
					error_message = "invalid string: control character U+0000 (NUL) must be escaped to \\u0000";
					return token_type::parse_error;
				}
				case 0x01:
				{
					error_message = "invalid string: control character U+0001 (SOH) must be escaped to \\u0001";
					return token_type::parse_error;
				}
				case 0x02:
				{
					error_message = "invalid string: control character U+0002 (STX) must be escaped to \\u0002";
					return token_type::parse_error;
				}
				case 0x03:
				{
					error_message = "invalid string: control character U+0003 (ETX) must be escaped to \\u0003";
					return token_type::parse_error;
				}
				case 0x04:
				{
					error_message = "invalid string: control character U+0004 (EOT) must be escaped to \\u0004";
					return token_type::parse_error;
				}
				case 0x05:
				{
					error_message = "invalid string: control character U+0005 (ENQ) must be escaped to \\u0005";
					return token_type::parse_error;
				}
				case 0x06:
				{
					error_message = "invalid string: control character U+0006 (ACK) must be escaped to \\u0006";
					return token_type::parse_error;
				}
				case 0x07:
				{
					error_message = "invalid string: control character U+0007 (BEL) must be escaped to \\u0007";
					return token_type::parse_error;
				}
				case 0x08:
				{
					error_message = "invalid string: control character U+0008 (BS) must be escaped to \\u0008 or \\b";
					return token_type::parse_error;
				}
				case 0x09:
				{
					error_message = "invalid string: control character U+0009 (HT) must be escaped to \\u0009 or \\t";
					return token_type::parse_error;
				}
				case 0x0A:
				{
					error_message = "invalid string: control character U+000A (LF) must be escaped to \\u000A or \\n";
					return token_type::parse_error;
				}
				case 0x0B:
				{
					error_message = "invalid string: control character U+000B (VT) must be escaped to \\u000B";
					return token_type::parse_error;
				}
				case 0x0C:
				{
					error_message = "invalid string: control character U+000C (FF) must be escaped to \\u000C or \\f";
					return token_type::parse_error;
				}
				case 0x0D:
				{
					error_message = "invalid string: control character U+000D (CR) must be escaped to \\u000D or \\r";
					return token_type::parse_error;
				}
				case 0x0E:
				{
					error_message = "invalid string: control character U+000E (SO) must be escaped to \\u000E";
					return token_type::parse_error;
				}
				case 0x0F:
				{
					error_message = "invalid string: control character U+000F (SI) must be escaped to \\u000F";
					return token_type::parse_error;
				}
				case 0x10:
				{
					error_message = "invalid string: control character U+0010 (DLE) must be escaped to \\u0010";
					return token_type::parse_error;
				}
				case 0x11:
				{
					error_message = "invalid string: control character U+0011 (DC1) must be escaped to \\u0011";
					return token_type::parse_error;
				}
				case 0x12:
				{
					error_message = "invalid string: control character U+0012 (DC2) must be escaped to \\u0012";
					return token_type::parse_error;
				}
				case 0x13:
				{
					error_message = "invalid string: control character U+0013 (DC3) must be escaped to \\u0013";
					return token_type::parse_error;
				}
				case 0x14:
				{
					error_message = "invalid string: control character U+0014 (DC4) must be escaped to \\u0014";
					return token_type::parse_error;
				}
				case 0x15:
				{
					error_message = "invalid string: control character U+0015 (NAK) must be escaped to \\u0015";
					return token_type::parse_error;
				}
				case 0x16:
				{
					error_message = "invalid string: control character U+0016 (SYN) must be escaped to \\u0016";
					return token_type::parse_error;
				}
				case 0x17:
				{
					error_message = "invalid string: control character U+0017 (ETB) must be escaped to \\u0017";
					return token_type::parse_error;
				}
				case 0x18:
				{
					error_message = "invalid string: control character U+0018 (CAN) must be escaped to \\u0018";
					return token_type::parse_error;
				}
				case 0x19:
				{
					error_message = "invalid string: control character U+0019 (EM) must be escaped to \\u0019";
					return token_type::parse_error;
				}
				case 0x1A:
				{
					error_message = "invalid string: control character U+001A (SUB) must be escaped to \\u001A";
					return token_type::parse_error;
				}
				case 0x1B:
				{
					error_message = "invalid string: control character U+001B (ESC) must be escaped to \\u001B";
					return token_type::parse_error;
				}
				case 0x1C:
				{
					error_message = "invalid string: control character U+001C (FS) must be escaped to \\u001C";
					return token_type::parse_error;
				}
				case 0x1D:
				{
					error_message = "invalid string: control character U+001D (GS) must be escaped to \\u001D";
					return token_type::parse_error;
				}
				case 0x1E:
				{
					error_message = "invalid string: control character U+001E (RS) must be escaped to \\u001E";
					return token_type::parse_error;
				}
				case 0x1F:
				{
					error_message = "invalid string: control character U+001F (US) must be escaped to \\u001F";
					return token_type::parse_error;
				}
				case 0x20:
				case 0x21:
				case 0x23:
				case 0x24:
				case 0x25:
				case 0x26:
				case 0x27:
				case 0x28:
				case 0x29:
				case 0x2A:
				case 0x2B:
				case 0x2C:
				case 0x2D:
				case 0x2E:
				case 0x2F:
				case 0x30:
				case 0x31:
				case 0x32:
				case 0x33:
				case 0x34:
				case 0x35:
				case 0x36:
				case 0x37:
				case 0x38:
				case 0x39:
				case 0x3A:
				case 0x3B:
				case 0x3C:
				case 0x3D:
				case 0x3E:
				case 0x3F:
				case 0x40:
				case 0x41:
				case 0x42:
				case 0x43:
				case 0x44:
				case 0x45:
				case 0x46:
				case 0x47:
				case 0x48:
				case 0x49:
				case 0x4A:
				case 0x4B:
				case 0x4C:
				case 0x4D:
				case 0x4E:
				case 0x4F:
				case 0x50:
				case 0x51:
				case 0x52:
				case 0x53:
				case 0x54:
				case 0x55:
				case 0x56:
				case 0x57:
				case 0x58:
				case 0x59:
				case 0x5A:
				case 0x5B:
				case 0x5D:
				case 0x5E:
				case 0x5F:
				case 0x60:
				case 0x61:
				case 0x62:
				case 0x63:
				case 0x64:
				case 0x65:
				case 0x66:
				case 0x67:
				case 0x68:
				case 0x69:
				case 0x6A:
				case 0x6B:
				case 0x6C:
				case 0x6D:
				case 0x6E:
				case 0x6F:
				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x76:
				case 0x77:
				case 0x78:
				case 0x79:
				case 0x7A:
				case 0x7B:
				case 0x7C:
				case 0x7D:
				case 0x7E:
				case 0x7F:
				{
					add(current);
					break;
				}
				case 0xC2:
				case 0xC3:
				case 0xC4:
				case 0xC5:
				case 0xC6:
				case 0xC7:
				case 0xC8:
				case 0xC9:
				case 0xCA:
				case 0xCB:
				case 0xCC:
				case 0xCD:
				case 0xCE:
				case 0xCF:
				case 0xD0:
				case 0xD1:
				case 0xD2:
				case 0xD3:
				case 0xD4:
				case 0xD5:
				case 0xD6:
				case 0xD7:
				case 0xD8:
				case 0xD9:
				case 0xDA:
				case 0xDB:
				case 0xDC:
				case 0xDD:
				case 0xDE:
				case 0xDF:
				{
					if (JSON_HEDLEY_UNLIKELY(!next_byte_in_range({ 0x80, 0xBF })))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xE0:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0xA0, 0xBF, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xE1:
				case 0xE2:
				case 0xE3:
				case 0xE4:
				case 0xE5:
				case 0xE6:
				case 0xE7:
				case 0xE8:
				case 0xE9:
				case 0xEA:
				case 0xEB:
				case 0xEC:
				case 0xEE:
				case 0xEF:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0x80, 0xBF, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xED:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0x80, 0x9F, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xF0:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0x90, 0xBF, 0x80, 0xBF, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xF1:
				case 0xF2:
				case 0xF3:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				case 0xF4:
				{
					if (JSON_HEDLEY_UNLIKELY(!(next_byte_in_range({ 0x80, 0x8F, 0x80, 0xBF, 0x80, 0xBF }))))
					{
						return token_type::parse_error;
					}
					break;
				}
				default:
				{
					error_message = "invalid string: ill-formed UTF-8 byte";
					return token_type::parse_error;
				}
				}
			}
		}
		bool scan_comment()
		{
			switch (get())
			{
			case '/':
			{
				while (true)
				{
					switch (get())
					{
					case '\n':
					case '\r':
					case char_traits<char_type>::eof():
					case '\0':
						return true;
					default:
						break;
					}
				}
			}
			case '*':
			{
				while (true)
				{
					switch (get())
					{
					case char_traits<char_type>::eof():
					case '\0':
					{
						error_message = "invalid comment; missing closing '*/'";
						return false;
					}
					case '*':
					{
						switch (get())
						{
						case '/':
							return true;
						default:
						{
							unget();
							continue;
						}
						}
					}
					default:
						continue;
					}
				}
			}
			default:
			{
				error_message = "invalid comment; expecting '/' or '*' after '/'";
				return false;
			}
			}
		}
		JSON_HEDLEY_NON_NULL(2)
			static void strtof(float& f, const char* str, char** endptr) noexcept
		{
			f = std::strtof(str, endptr);
		}
		JSON_HEDLEY_NON_NULL(2)
			static void strtof(double& f, const char* str, char** endptr) noexcept
		{
			f = std::strtod(str, endptr);
		}
		JSON_HEDLEY_NON_NULL(2)
			static void strtof(long double& f, const char* str, char** endptr) noexcept
		{
			f = std::strtold(str, endptr);
		}
		token_type scan_number()
		{
			reset();
			token_type number_type = token_type::value_unsigned;
			switch (current)
			{
			case '-':
			{
				add(current);
				goto scan_number_minus;
			}
			case '0':
			{
				add(current);
				goto scan_number_zero;
			}
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any1;
			}
			default:
				JSON_ASSERT(false);
			}
		scan_number_minus:
			number_type = token_type::value_integer;
			switch (get())
			{
			case '0':
			{
				add(current);
				goto scan_number_zero;
			}
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any1;
			}
			default:
			{
				error_message = "invalid number; expected digit after '-'";
				return token_type::parse_error;
			}
			}
		scan_number_zero:
			switch (get())
			{
			case '.':
			{
				add(decimal_point_char);
				goto scan_number_decimal1;
			}
			case 'e':
			case 'E':
			{
				add(current);
				goto scan_number_exponent;
			}
			default:
				goto scan_number_done;
			}
		scan_number_any1:
			switch (get())
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any1;
			}
			case '.':
			{
				add(decimal_point_char);
				goto scan_number_decimal1;
			}
			case 'e':
			case 'E':
			{
				add(current);
				goto scan_number_exponent;
			}
			default:
				goto scan_number_done;
			}
		scan_number_decimal1:
			number_type = token_type::value_float;
			switch (get())
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_decimal2;
			}
			default:
			{
				error_message = "invalid number; expected digit after '.'";
				return token_type::parse_error;
			}
			}
		scan_number_decimal2:
			switch (get())
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_decimal2;
			}
			case 'e':
			case 'E':
			{
				add(current);
				goto scan_number_exponent;
			}
			default:
				goto scan_number_done;
			}
		scan_number_exponent:
			number_type = token_type::value_float;
			switch (get())
			{
			case '+':
			case '-':
			{
				add(current);
				goto scan_number_sign;
			}
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any2;
			}
			default:
			{
				error_message =
					"invalid number; expected '+', '-', or digit after exponent";
				return token_type::parse_error;
			}
			}
		scan_number_sign:
			switch (get())
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any2;
			}
			default:
			{
				error_message = "invalid number; expected digit after exponent sign";
				return token_type::parse_error;
			}
			}
		scan_number_any2:
			switch (get())
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				add(current);
				goto scan_number_any2;
			}
			default:
				goto scan_number_done;
			}
		scan_number_done:
			unget();
			char* endptr = nullptr;
			errno = 0;
			if (number_type == token_type::value_unsigned)
			{
				const auto x = std::strtoull(token_buffer.data(), &endptr, 10);
				JSON_ASSERT(endptr == token_buffer.data() + token_buffer.size());
				if (errno == 0)
				{
					value_unsigned = static_cast<number_unsigned_t>(x);
					if (value_unsigned == x)
					{
						return token_type::value_unsigned;
					}
				}
			}
			else if (number_type == token_type::value_integer)
			{
				const auto x = std::strtoll(token_buffer.data(), &endptr, 10);
				JSON_ASSERT(endptr == token_buffer.data() + token_buffer.size());
				if (errno == 0)
				{
					value_integer = static_cast<number_integer_t>(x);
					if (value_integer == x)
					{
						return token_type::value_integer;
					}
				}
			}
			strtof(value_float, token_buffer.data(), &endptr);
			JSON_ASSERT(endptr == token_buffer.data() + token_buffer.size());
			return token_type::value_float;
		}
		JSON_HEDLEY_NON_NULL(2)
			token_type scan_literal(const char_type* literal_text, const std::size_t length,
				token_type return_type)
		{
			JSON_ASSERT(char_traits<char_type>::to_char_type(current) == literal_text[0]);
			for (std::size_t i = 1; i < length; ++i)
			{
				if (JSON_HEDLEY_UNLIKELY(char_traits<char_type>::to_char_type(get()) != literal_text[i]))
				{
					error_message = "invalid literal";
					return token_type::parse_error;
				}
			}
			return return_type;
		}
		void reset() noexcept
		{
			token_buffer.clear();
			token_string.clear();
			token_string.push_back(char_traits<char_type>::to_char_type(current));
		}
		char_int_type get()
		{
			++position.chars_read_total;
			++position.chars_read_current_line;
			if (next_unget)
			{
				next_unget = false;
			}
			else
			{
				current = ia.get_character();
			}
			if (JSON_HEDLEY_LIKELY(current != char_traits<char_type>::eof()))
			{
				token_string.push_back(char_traits<char_type>::to_char_type(current));
			}
			if (current == '\n')
			{
				++position.lines_read;
				position.chars_read_current_line = 0;
			}
			return current;
		}
		void unget()
		{
			next_unget = true;
			--position.chars_read_total;
			if (position.chars_read_current_line == 0)
			{
				if (position.lines_read > 0)
				{
					--position.lines_read;
				}
			}
			else
			{
				--position.chars_read_current_line;
			}
			if (JSON_HEDLEY_LIKELY(current != char_traits<char_type>::eof()))
			{
				JSON_ASSERT(!token_string.empty());
				token_string.pop_back();
			}
		}
		void add(char_int_type c)
		{
			token_buffer.push_back(static_cast<typename string_t::value_type>(c));
		}
	public:
		constexpr number_integer_t get_number_integer() const noexcept
		{
			return value_integer;
		}
		constexpr number_unsigned_t get_number_unsigned() const noexcept
		{
			return value_unsigned;
		}
		constexpr number_float_t get_number_float() const noexcept
		{
			return value_float;
		}
		string_t& get_string()
		{
			return token_buffer;
		}
		constexpr position_t get_position() const noexcept
		{
			return position;
		}
		std::string get_token_string() const
		{
			std::string result;
			for (const auto c : token_string)
			{
				if (static_cast<unsigned char>(c) <= '\x1F')
				{
					std::array<char, 9> cs{ {} };
					static_cast<void>((std::snprintf)(cs.data(), cs.size(), "<U+%.4X>", static_cast<unsigned char>(c)));
					result += cs.data();
				}
				else
				{
					result.push_back(static_cast<std::string::value_type>(c));
				}
			}
			return result;
		}
		JSON_HEDLEY_RETURNS_NON_NULL
			constexpr const char* get_error_message() const noexcept
		{
			return error_message;
		}
		bool skip_bom()
		{
			if (get() == 0xEF)
			{
				return get() == 0xBB && get() == 0xBF;
			}
			unget();
			return true;
		}
		void skip_whitespace()
		{
			do
			{
				get();
			} while (current == ' ' || current == '\t' || current == '\n' || current == '\r');
		}
		token_type scan()
		{
			if (position.chars_read_total == 0 && !skip_bom())
			{
				error_message = "invalid BOM; must be 0xEF 0xBB 0xBF if given";
				return token_type::parse_error;
			}
			skip_whitespace();
			while (ignore_comments && current == '/')
			{
				if (!scan_comment())
				{
					return token_type::parse_error;
				}
				skip_whitespace();
			}
			switch (current)
			{
			case '[':
				return token_type::begin_array;
			case ']':
				return token_type::end_array;
			case '{':
				return token_type::begin_object;
			case '}':
				return token_type::end_object;
			case ':':
				return token_type::name_separator;
			case ',':
				return token_type::value_separator;
			case 't':
			{
				std::array<char_type, 4> true_literal = { {static_cast<char_type>('t'), static_cast<char_type>('r'), static_cast<char_type>('u'), static_cast<char_type>('e')} };
				return scan_literal(true_literal.data(), true_literal.size(), token_type::literal_true);
			}
			case 'f':
			{
				std::array<char_type, 5> false_literal = { {static_cast<char_type>('f'), static_cast<char_type>('a'), static_cast<char_type>('l'), static_cast<char_type>('s'), static_cast<char_type>('e')} };
				return scan_literal(false_literal.data(), false_literal.size(), token_type::literal_false);
			}
			case 'n':
			{
				std::array<char_type, 4> null_literal = { {static_cast<char_type>('n'), static_cast<char_type>('u'), static_cast<char_type>('l'), static_cast<char_type>('l')} };
				return scan_literal(null_literal.data(), null_literal.size(), token_type::literal_null);
			}
			case '\"':
				return scan_string();
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				return scan_number();
			case '\0':
			case char_traits<char_type>::eof():
				return token_type::end_of_input;
			default:
				error_message = "invalid literal";
				return token_type::parse_error;
			}
		}
	private:
		InputAdapterType ia;
		const bool ignore_comments = false;
		char_int_type current = char_traits<char_type>::eof();
		bool next_unget = false;
		position_t position{};
		std::vector<char_type> token_string{};
		string_t token_buffer{};
		const char* error_message = "";
		number_integer_t value_integer = 0;
		number_unsigned_t value_unsigned = 0;
		number_float_t value_float = 0;
		const char_int_type decimal_point_char = '.';
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstdint> 
#include <utility> 
#include <string> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename T>
	using null_function_t = decltype(std::declval<T&>().null());
	template<typename T>
	using boolean_function_t =
		decltype(std::declval<T&>().boolean(std::declval<bool>()));
	template<typename T, typename Integer>
	using number_integer_function_t =
		decltype(std::declval<T&>().number_integer(std::declval<Integer>()));
	template<typename T, typename Unsigned>
	using number_unsigned_function_t =
		decltype(std::declval<T&>().number_unsigned(std::declval<Unsigned>()));
	template<typename T, typename Float, typename String>
	using number_float_function_t = decltype(std::declval<T&>().number_float(
		std::declval<Float>(), std::declval<const String&>()));
	template<typename T, typename String>
	using string_function_t =
		decltype(std::declval<T&>().string(std::declval<String&>()));
	template<typename T, typename Binary>
	using binary_function_t =
		decltype(std::declval<T&>().binary(std::declval<Binary&>()));
	template<typename T>
	using start_object_function_t =
		decltype(std::declval<T&>().start_object(std::declval<std::size_t>()));
	template<typename T, typename String>
	using key_function_t =
		decltype(std::declval<T&>().key(std::declval<String&>()));
	template<typename T>
	using end_object_function_t = decltype(std::declval<T&>().end_object());
	template<typename T>
	using start_array_function_t =
		decltype(std::declval<T&>().start_array(std::declval<std::size_t>()));
	template<typename T>
	using end_array_function_t = decltype(std::declval<T&>().end_array());
	template<typename T, typename Exception>
	using parse_error_function_t = decltype(std::declval<T&>().parse_error(
		std::declval<std::size_t>(), std::declval<const std::string&>(),
		std::declval<const Exception&>()));
	template<typename SAX, typename BasicJsonType>
	struct is_sax
	{
	private:
		static_assert(is_basic_json<BasicJsonType>::value,
			"BasicJsonType must be of type basic_json<...>");
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		using exception_t = typename BasicJsonType::exception;
	public:
		static constexpr bool value =
			is_detected_exact<bool, null_function_t, SAX>::value&&
			is_detected_exact<bool, boolean_function_t, SAX>::value&&
			is_detected_exact<bool, number_integer_function_t, SAX, number_integer_t>::value&&
			is_detected_exact<bool, number_unsigned_function_t, SAX, number_unsigned_t>::value&&
			is_detected_exact<bool, number_float_function_t, SAX, number_float_t, string_t>::value&&
			is_detected_exact<bool, string_function_t, SAX, string_t>::value&&
			is_detected_exact<bool, binary_function_t, SAX, binary_t>::value&&
			is_detected_exact<bool, start_object_function_t, SAX>::value&&
			is_detected_exact<bool, key_function_t, SAX, string_t>::value&&
			is_detected_exact<bool, end_object_function_t, SAX>::value&&
			is_detected_exact<bool, start_array_function_t, SAX>::value&&
			is_detected_exact<bool, end_array_function_t, SAX>::value&&
			is_detected_exact<bool, parse_error_function_t, SAX, exception_t>::value;
	};
	template<typename SAX, typename BasicJsonType>
	struct is_sax_static_asserts
	{
	private:
		static_assert(is_basic_json<BasicJsonType>::value,
			"BasicJsonType must be of type basic_json<...>");
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		using exception_t = typename BasicJsonType::exception;
	public:
		static_assert(is_detected_exact<bool, null_function_t, SAX>::value,
			"Missing/invalid function: bool null()");
		static_assert(is_detected_exact<bool, boolean_function_t, SAX>::value,
			"Missing/invalid function: bool boolean(bool)");
		static_assert(is_detected_exact<bool, boolean_function_t, SAX>::value,
			"Missing/invalid function: bool boolean(bool)");
		static_assert(
			is_detected_exact<bool, number_integer_function_t, SAX,
			number_integer_t>::value,
			"Missing/invalid function: bool number_integer(number_integer_t)");
		static_assert(
			is_detected_exact<bool, number_unsigned_function_t, SAX,
			number_unsigned_t>::value,
			"Missing/invalid function: bool number_unsigned(number_unsigned_t)");
		static_assert(is_detected_exact<bool, number_float_function_t, SAX,
			number_float_t, string_t>::value,
			"Missing/invalid function: bool number_float(number_float_t, const string_t&)");
		static_assert(
			is_detected_exact<bool, string_function_t, SAX, string_t>::value,
			"Missing/invalid function: bool string(string_t&)");
		static_assert(
			is_detected_exact<bool, binary_function_t, SAX, binary_t>::value,
			"Missing/invalid function: bool binary(binary_t&)");
		static_assert(is_detected_exact<bool, start_object_function_t, SAX>::value,
			"Missing/invalid function: bool start_object(std::size_t)");
		static_assert(is_detected_exact<bool, key_function_t, SAX, string_t>::value,
			"Missing/invalid function: bool key(string_t&)");
		static_assert(is_detected_exact<bool, end_object_function_t, SAX>::value,
			"Missing/invalid function: bool end_object()");
		static_assert(is_detected_exact<bool, start_array_function_t, SAX>::value,
			"Missing/invalid function: bool start_array(std::size_t)");
		static_assert(is_detected_exact<bool, end_array_function_t, SAX>::value,
			"Missing/invalid function: bool end_array()");
		static_assert(
			is_detected_exact<bool, parse_error_function_t, SAX, exception_t>::value,
			"Missing/invalid function: bool parse_error(std::size_t, const "
			"std::string&, const exception&)");
	};
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	enum class cbor_tag_handler_t
	{
		error,
		ignore,
		store
	};
	static inline bool little_endianness(int num = 1) noexcept
	{
		return *reinterpret_cast<char*>(&num) == 1;
	}
	template<typename BasicJsonType, typename InputAdapterType, typename SAX = json_sax_dom_parser<BasicJsonType>>
	class binary_reader
	{
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		using json_sax_t = SAX;
		using char_type = typename InputAdapterType::char_type;
		using char_int_type = typename char_traits<char_type>::int_type;
	public:
		explicit binary_reader(InputAdapterType&& adapter, const input_format_t format = input_format_t::json) noexcept : ia(std::move(adapter)), input_format(format)
		{
			(void)detail::is_sax_static_asserts<SAX, BasicJsonType> {};
		}
		binary_reader(const binary_reader&) = delete;
		binary_reader(binary_reader&&) = default;
		binary_reader& operator=(const binary_reader&) = delete;
		binary_reader& operator=(binary_reader&&) = default;
		~binary_reader() = default;
		JSON_HEDLEY_NON_NULL(3)
			bool sax_parse(const input_format_t format,
				json_sax_t* sax_,
				const bool strict = true,
				const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error)
		{
			sax = sax_;
			bool result = false;
			switch (format)
			{
			case input_format_t::bson:
				result = parse_bson_internal();
				break;
			case input_format_t::cbor:
				result = parse_cbor_internal(true, tag_handler);
				break;
			case input_format_t::msgpack:
				result = parse_msgpack_internal();
				break;
			case input_format_t::ubjson:
			case input_format_t::bjdata:
				result = parse_ubjson_internal();
				break;
			case input_format_t::json:
			default:
				JSON_ASSERT(false);
			}
			if (result && strict)
			{
				if (input_format == input_format_t::ubjson || input_format == input_format_t::bjdata)
				{
					get_ignore_noop();
				}
				else
				{
					get();
				}
				if (JSON_HEDLEY_UNLIKELY(current != char_traits<char_type>::eof()))
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(110, chars_read,
						exception_message(input_format, concat("expected end of input; last byte: 0x", get_token_string()), "value"), nullptr));
				}
			}
			return result;
		}
	private:
		bool parse_bson_internal()
		{
			std::int32_t document_size{};
			get_number<std::int32_t, true>(input_format_t::bson, document_size);
			if (JSON_HEDLEY_UNLIKELY(!sax->start_object(static_cast<std::size_t>(-1))))
			{
				return false;
			}
			if (JSON_HEDLEY_UNLIKELY(!parse_bson_element_list(false)))
			{
				return false;
			}
			return sax->end_object();
		}
		bool get_bson_cstr(string_t& result)
		{
			auto out = std::back_inserter(result);
			while (true)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::bson, "cstring")))
				{
					return false;
				}
				if (current == 0x00)
				{
					return true;
				}
				*out++ = static_cast<typename string_t::value_type>(current);
			}
		}
		template<typename NumberType>
		bool get_bson_string(const NumberType len, string_t& result)
		{
			if (JSON_HEDLEY_UNLIKELY(len < 1))
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
					exception_message(input_format_t::bson, concat("string length must be at least 1, is ", std::to_string(len)), "string"), nullptr));
			}
			return get_string(input_format_t::bson, len - static_cast<NumberType>(1), result) && get() != char_traits<char_type>::eof();
		}
		template<typename NumberType>
		bool get_bson_binary(const NumberType len, binary_t& result)
		{
			if (JSON_HEDLEY_UNLIKELY(len < 0))
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
					exception_message(input_format_t::bson, concat("byte array length cannot be negative, is ", std::to_string(len)), "binary"), nullptr));
			}
			std::uint8_t subtype{};
			get_number<std::uint8_t>(input_format_t::bson, subtype);
			result.set_subtype(subtype);
			return get_binary(input_format_t::bson, len, result);
		}
		bool parse_bson_element_internal(const char_int_type element_type,
			const std::size_t element_type_parse_position)
		{
			switch (element_type)
			{
			case 0x01:
			{
				double number{};
				return get_number<double, true>(input_format_t::bson, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 0x02:
			{
				std::int32_t len{};
				string_t value;
				return get_number<std::int32_t, true>(input_format_t::bson, len) && get_bson_string(len, value) && sax->string(value);
			}
			case 0x03:
			{
				return parse_bson_internal();
			}
			case 0x04:
			{
				return parse_bson_array();
			}
			case 0x05:
			{
				std::int32_t len{};
				binary_t value;
				return get_number<std::int32_t, true>(input_format_t::bson, len) && get_bson_binary(len, value) && sax->binary(value);
			}
			case 0x08:
			{
				return sax->boolean(get() != 0);
			}
			case 0x0A:
			{
				return sax->null();
			}
			case 0x10:
			{
				std::int32_t value{};
				return get_number<std::int32_t, true>(input_format_t::bson, value) && sax->number_integer(value);
			}
			case 0x12:
			{
				std::int64_t value{};
				return get_number<std::int64_t, true>(input_format_t::bson, value) && sax->number_integer(value);
			}
			default:
			{
				std::array<char, 3> cr{ {} };
				static_cast<void>((std::snprintf)(cr.data(), cr.size(), "%.2hhX", static_cast<unsigned char>(element_type)));
				const std::string cr_str{ cr.data() };
				return sax->parse_error(element_type_parse_position, cr_str,
					parse_error::create(114, element_type_parse_position, concat("Unsupported BSON record type 0x", cr_str), nullptr));
			}
			}
		}
		bool parse_bson_element_list(const bool is_array)
		{
			string_t key;
			while (auto element_type = get())
			{
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::bson, "element list")))
				{
					return false;
				}
				const std::size_t element_type_parse_position = chars_read;
				if (JSON_HEDLEY_UNLIKELY(!get_bson_cstr(key)))
				{
					return false;
				}
				if (!is_array && !sax->key(key))
				{
					return false;
				}
				if (JSON_HEDLEY_UNLIKELY(!parse_bson_element_internal(element_type, element_type_parse_position)))
				{
					return false;
				}
				key.clear();
			}
			return true;
		}
		bool parse_bson_array()
		{
			std::int32_t document_size{};
			get_number<std::int32_t, true>(input_format_t::bson, document_size);
			if (JSON_HEDLEY_UNLIKELY(!sax->start_array(static_cast<std::size_t>(-1))))
			{
				return false;
			}
			if (JSON_HEDLEY_UNLIKELY(!parse_bson_element_list(true)))
			{
				return false;
			}
			return sax->end_array();
		}
		bool parse_cbor_internal(const bool get_char,
			const cbor_tag_handler_t tag_handler)
		{
			switch (get_char ? get() : current)
			{
			case char_traits<char_type>::eof():
				return unexpect_eof(input_format_t::cbor, "value");
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
			case 0x0E:
			case 0x0F:
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
				return sax->number_unsigned(static_cast<number_unsigned_t>(current));
			case 0x18:
			{
				std::uint8_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_unsigned(number);
			}
			case 0x19:
			{
				std::uint16_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_unsigned(number);
			}
			case 0x1A:
			{
				std::uint32_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_unsigned(number);
			}
			case 0x1B:
			{
				std::uint64_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_unsigned(number);
			}
			case 0x20:
			case 0x21:
			case 0x22:
			case 0x23:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x28:
			case 0x29:
			case 0x2A:
			case 0x2B:
			case 0x2C:
			case 0x2D:
			case 0x2E:
			case 0x2F:
			case 0x30:
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37:
				return sax->number_integer(static_cast<std::int8_t>(0x20 - 1 - current));
			case 0x38:
			{
				std::uint8_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_integer(static_cast<number_integer_t>(-1) - number);
			}
			case 0x39:
			{
				std::uint16_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_integer(static_cast<number_integer_t>(-1) - number);
			}
			case 0x3A:
			{
				std::uint32_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_integer(static_cast<number_integer_t>(-1) - number);
			}
			case 0x3B:
			{
				std::uint64_t number{};
				return get_number(input_format_t::cbor, number) && sax->number_integer(static_cast<number_integer_t>(-1)
					- static_cast<number_integer_t>(number));
			}
			case 0x40:
			case 0x41:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4A:
			case 0x4B:
			case 0x4C:
			case 0x4D:
			case 0x4E:
			case 0x4F:
			case 0x50:
			case 0x51:
			case 0x52:
			case 0x53:
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			case 0x58:
			case 0x59:
			case 0x5A:
			case 0x5B:
			case 0x5F:
			{
				binary_t b;
				return get_cbor_binary(b) && sax->binary(b);
			}
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x69:
			case 0x6A:
			case 0x6B:
			case 0x6C:
			case 0x6D:
			case 0x6E:
			case 0x6F:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x76:
			case 0x77:
			case 0x78:
			case 0x79:
			case 0x7A:
			case 0x7B:
			case 0x7F:
			{
				string_t s;
				return get_cbor_string(s) && sax->string(s);
			}
			case 0x80:
			case 0x81:
			case 0x82:
			case 0x83:
			case 0x84:
			case 0x85:
			case 0x86:
			case 0x87:
			case 0x88:
			case 0x89:
			case 0x8A:
			case 0x8B:
			case 0x8C:
			case 0x8D:
			case 0x8E:
			case 0x8F:
			case 0x90:
			case 0x91:
			case 0x92:
			case 0x93:
			case 0x94:
			case 0x95:
			case 0x96:
			case 0x97:
				return get_cbor_array(
					conditional_static_cast<std::size_t>(static_cast<unsigned int>(current) & 0x1Fu), tag_handler);
			case 0x98:
			{
				std::uint8_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_array(static_cast<std::size_t>(len), tag_handler);
			}
			case 0x99:
			{
				std::uint16_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_array(static_cast<std::size_t>(len), tag_handler);
			}
			case 0x9A:
			{
				std::uint32_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_array(conditional_static_cast<std::size_t>(len), tag_handler);
			}
			case 0x9B:
			{
				std::uint64_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_array(conditional_static_cast<std::size_t>(len), tag_handler);
			}
			case 0x9F:
				return get_cbor_array(static_cast<std::size_t>(-1), tag_handler);
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA4:
			case 0xA5:
			case 0xA6:
			case 0xA7:
			case 0xA8:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
			case 0xAF:
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
				return get_cbor_object(conditional_static_cast<std::size_t>(static_cast<unsigned int>(current) & 0x1Fu), tag_handler);
			case 0xB8:
			{
				std::uint8_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_object(static_cast<std::size_t>(len), tag_handler);
			}
			case 0xB9:
			{
				std::uint16_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_object(static_cast<std::size_t>(len), tag_handler);
			}
			case 0xBA:
			{
				std::uint32_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_object(conditional_static_cast<std::size_t>(len), tag_handler);
			}
			case 0xBB:
			{
				std::uint64_t len{};
				return get_number(input_format_t::cbor, len) && get_cbor_object(conditional_static_cast<std::size_t>(len), tag_handler);
			}
			case 0xBF:
				return get_cbor_object(static_cast<std::size_t>(-1), tag_handler);
			case 0xC6:
			case 0xC7:
			case 0xC8:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCC:
			case 0xCD:
			case 0xCE:
			case 0xCF:
			case 0xD0:
			case 0xD1:
			case 0xD2:
			case 0xD3:
			case 0xD4:
			case 0xD8:
			case 0xD9:
			case 0xDA:
			case 0xDB:
			{
				switch (tag_handler)
				{
				case cbor_tag_handler_t::error:
				{
					auto last_token = get_token_string();
					return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
						exception_message(input_format_t::cbor, concat("invalid byte: 0x", last_token), "value"), nullptr));
				}
				case cbor_tag_handler_t::ignore:
				{
					switch (current)
					{
					case 0xD8:
					{
						std::uint8_t subtype_to_ignore{};
						get_number(input_format_t::cbor, subtype_to_ignore);
						break;
					}
					case 0xD9:
					{
						std::uint16_t subtype_to_ignore{};
						get_number(input_format_t::cbor, subtype_to_ignore);
						break;
					}
					case 0xDA:
					{
						std::uint32_t subtype_to_ignore{};
						get_number(input_format_t::cbor, subtype_to_ignore);
						break;
					}
					case 0xDB:
					{
						std::uint64_t subtype_to_ignore{};
						get_number(input_format_t::cbor, subtype_to_ignore);
						break;
					}
					default:
						break;
					}
					return parse_cbor_internal(true, tag_handler);
				}
				case cbor_tag_handler_t::store:
				{
					binary_t b;
					switch (current)
					{
					case 0xD8:
					{
						std::uint8_t subtype{};
						get_number(input_format_t::cbor, subtype);
						b.set_subtype(detail::conditional_static_cast<typename binary_t::subtype_type>(subtype));
						break;
					}
					case 0xD9:
					{
						std::uint16_t subtype{};
						get_number(input_format_t::cbor, subtype);
						b.set_subtype(detail::conditional_static_cast<typename binary_t::subtype_type>(subtype));
						break;
					}
					case 0xDA:
					{
						std::uint32_t subtype{};
						get_number(input_format_t::cbor, subtype);
						b.set_subtype(detail::conditional_static_cast<typename binary_t::subtype_type>(subtype));
						break;
					}
					case 0xDB:
					{
						std::uint64_t subtype{};
						get_number(input_format_t::cbor, subtype);
						b.set_subtype(detail::conditional_static_cast<typename binary_t::subtype_type>(subtype));
						break;
					}
					default:
						return parse_cbor_internal(true, tag_handler);
					}
					get();
					return get_cbor_binary(b) && sax->binary(b);
				}
				default:
					JSON_ASSERT(false);
					return false;
				}
			}
			case 0xF4:
				return sax->boolean(false);
			case 0xF5:
				return sax->boolean(true);
			case 0xF6:
				return sax->null();
			case 0xF9:
			{
				const auto byte1_raw = get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::cbor, "number")))
				{
					return false;
				}
				const auto byte2_raw = get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::cbor, "number")))
				{
					return false;
				}
				const auto byte1 = static_cast<unsigned char>(byte1_raw);
				const auto byte2 = static_cast<unsigned char>(byte2_raw);
				const auto half = static_cast<unsigned int>((byte1 << 8u) + byte2);
				const double val = [&half]
					{
						const int exp = (half >> 10u) & 0x1Fu;
						const unsigned int mant = half & 0x3FFu;
						JSON_ASSERT(0 <= exp && exp <= 32);
						JSON_ASSERT(mant <= 1024);
						switch (exp)
						{
						case 0:
							return std::ldexp(mant, -24);
						case 31:
							return (mant == 0)
								? std::numeric_limits<double>::infinity()
								: std::numeric_limits<double>::quiet_NaN();
						default:
							return std::ldexp(mant + 1024, exp - 25);
						}
					}();
				return sax->number_float((half & 0x8000u) != 0
					? static_cast<number_float_t>(-val)
					: static_cast<number_float_t>(val), "");
			}
			case 0xFA:
			{
				float number{};
				return get_number(input_format_t::cbor, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 0xFB:
			{
				double number{};
				return get_number(input_format_t::cbor, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			default:
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
					exception_message(input_format_t::cbor, concat("invalid byte: 0x", last_token), "value"), nullptr));
			}
			}
		}
		bool get_cbor_string(string_t& result)
		{
			if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::cbor, "string")))
			{
				return false;
			}
			switch (current)
			{
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x69:
			case 0x6A:
			case 0x6B:
			case 0x6C:
			case 0x6D:
			case 0x6E:
			case 0x6F:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x76:
			case 0x77:
			{
				return get_string(input_format_t::cbor, static_cast<unsigned int>(current) & 0x1Fu, result);
			}
			case 0x78:
			{
				std::uint8_t len{};
				return get_number(input_format_t::cbor, len) && get_string(input_format_t::cbor, len, result);
			}
			case 0x79:
			{
				std::uint16_t len{};
				return get_number(input_format_t::cbor, len) && get_string(input_format_t::cbor, len, result);
			}
			case 0x7A:
			{
				std::uint32_t len{};
				return get_number(input_format_t::cbor, len) && get_string(input_format_t::cbor, len, result);
			}
			case 0x7B:
			{
				std::uint64_t len{};
				return get_number(input_format_t::cbor, len) && get_string(input_format_t::cbor, len, result);
			}
			case 0x7F:
			{
				while (get() != 0xFF)
				{
					string_t chunk;
					if (!get_cbor_string(chunk))
					{
						return false;
					}
					result.append(chunk);
				}
				return true;
			}
			default:
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read,
					exception_message(input_format_t::cbor, concat("expected length specification (0x60-0x7B) or indefinite string type (0x7F); last byte: 0x", last_token), "string"), nullptr));
			}
			}
		}
		bool get_cbor_binary(binary_t& result)
		{
			if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::cbor, "binary")))
			{
				return false;
			}
			switch (current)
			{
			case 0x40:
			case 0x41:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4A:
			case 0x4B:
			case 0x4C:
			case 0x4D:
			case 0x4E:
			case 0x4F:
			case 0x50:
			case 0x51:
			case 0x52:
			case 0x53:
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			{
				return get_binary(input_format_t::cbor, static_cast<unsigned int>(current) & 0x1Fu, result);
			}
			case 0x58:
			{
				std::uint8_t len{};
				return get_number(input_format_t::cbor, len) &&
					get_binary(input_format_t::cbor, len, result);
			}
			case 0x59:
			{
				std::uint16_t len{};
				return get_number(input_format_t::cbor, len) &&
					get_binary(input_format_t::cbor, len, result);
			}
			case 0x5A:
			{
				std::uint32_t len{};
				return get_number(input_format_t::cbor, len) &&
					get_binary(input_format_t::cbor, len, result);
			}
			case 0x5B:
			{
				std::uint64_t len{};
				return get_number(input_format_t::cbor, len) &&
					get_binary(input_format_t::cbor, len, result);
			}
			case 0x5F:
			{
				while (get() != 0xFF)
				{
					binary_t chunk;
					if (!get_cbor_binary(chunk))
					{
						return false;
					}
					result.insert(result.end(), chunk.begin(), chunk.end());
				}
				return true;
			}
			default:
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read,
					exception_message(input_format_t::cbor, concat("expected length specification (0x40-0x5B) or indefinite binary array type (0x5F); last byte: 0x", last_token), "binary"), nullptr));
			}
			}
		}
		bool get_cbor_array(const std::size_t len,
			const cbor_tag_handler_t tag_handler)
		{
			if (JSON_HEDLEY_UNLIKELY(!sax->start_array(len)))
			{
				return false;
			}
			if (len != static_cast<std::size_t>(-1))
			{
				for (std::size_t i = 0; i < len; ++i)
				{
					if (JSON_HEDLEY_UNLIKELY(!parse_cbor_internal(true, tag_handler)))
					{
						return false;
					}
				}
			}
			else
			{
				while (get() != 0xFF)
				{
					if (JSON_HEDLEY_UNLIKELY(!parse_cbor_internal(false, tag_handler)))
					{
						return false;
					}
				}
			}
			return sax->end_array();
		}
		bool get_cbor_object(const std::size_t len,
			const cbor_tag_handler_t tag_handler)
		{
			if (JSON_HEDLEY_UNLIKELY(!sax->start_object(len)))
			{
				return false;
			}
			if (len != 0)
			{
				string_t key;
				if (len != static_cast<std::size_t>(-1))
				{
					for (std::size_t i = 0; i < len; ++i)
					{
						get();
						if (JSON_HEDLEY_UNLIKELY(!get_cbor_string(key) || !sax->key(key)))
						{
							return false;
						}
						if (JSON_HEDLEY_UNLIKELY(!parse_cbor_internal(true, tag_handler)))
						{
							return false;
						}
						key.clear();
					}
				}
				else
				{
					while (get() != 0xFF)
					{
						if (JSON_HEDLEY_UNLIKELY(!get_cbor_string(key) || !sax->key(key)))
						{
							return false;
						}
						if (JSON_HEDLEY_UNLIKELY(!parse_cbor_internal(true, tag_handler)))
						{
							return false;
						}
						key.clear();
					}
				}
			}
			return sax->end_object();
		}
		bool parse_msgpack_internal()
		{
			switch (get())
			{
			case char_traits<char_type>::eof():
				return unexpect_eof(input_format_t::msgpack, "value");
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0D:
			case 0x0E:
			case 0x0F:
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
			case 0x1A:
			case 0x1B:
			case 0x1C:
			case 0x1D:
			case 0x1E:
			case 0x1F:
			case 0x20:
			case 0x21:
			case 0x22:
			case 0x23:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x28:
			case 0x29:
			case 0x2A:
			case 0x2B:
			case 0x2C:
			case 0x2D:
			case 0x2E:
			case 0x2F:
			case 0x30:
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37:
			case 0x38:
			case 0x39:
			case 0x3A:
			case 0x3B:
			case 0x3C:
			case 0x3D:
			case 0x3E:
			case 0x3F:
			case 0x40:
			case 0x41:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4A:
			case 0x4B:
			case 0x4C:
			case 0x4D:
			case 0x4E:
			case 0x4F:
			case 0x50:
			case 0x51:
			case 0x52:
			case 0x53:
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			case 0x58:
			case 0x59:
			case 0x5A:
			case 0x5B:
			case 0x5C:
			case 0x5D:
			case 0x5E:
			case 0x5F:
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x69:
			case 0x6A:
			case 0x6B:
			case 0x6C:
			case 0x6D:
			case 0x6E:
			case 0x6F:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x76:
			case 0x77:
			case 0x78:
			case 0x79:
			case 0x7A:
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E:
			case 0x7F:
				return sax->number_unsigned(static_cast<number_unsigned_t>(current));
			case 0x80:
			case 0x81:
			case 0x82:
			case 0x83:
			case 0x84:
			case 0x85:
			case 0x86:
			case 0x87:
			case 0x88:
			case 0x89:
			case 0x8A:
			case 0x8B:
			case 0x8C:
			case 0x8D:
			case 0x8E:
			case 0x8F:
				return get_msgpack_object(conditional_static_cast<std::size_t>(static_cast<unsigned int>(current) & 0x0Fu));
			case 0x90:
			case 0x91:
			case 0x92:
			case 0x93:
			case 0x94:
			case 0x95:
			case 0x96:
			case 0x97:
			case 0x98:
			case 0x99:
			case 0x9A:
			case 0x9B:
			case 0x9C:
			case 0x9D:
			case 0x9E:
			case 0x9F:
				return get_msgpack_array(conditional_static_cast<std::size_t>(static_cast<unsigned int>(current) & 0x0Fu));
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA4:
			case 0xA5:
			case 0xA6:
			case 0xA7:
			case 0xA8:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
			case 0xAF:
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
			case 0xB8:
			case 0xB9:
			case 0xBA:
			case 0xBB:
			case 0xBC:
			case 0xBD:
			case 0xBE:
			case 0xBF:
			case 0xD9:
			case 0xDA:
			case 0xDB:
			{
				string_t s;
				return get_msgpack_string(s) && sax->string(s);
			}
			case 0xC0:
				return sax->null();
			case 0xC2:
				return sax->boolean(false);
			case 0xC3:
				return sax->boolean(true);
			case 0xC4:
			case 0xC5:
			case 0xC6:
			case 0xC7:
			case 0xC8:
			case 0xC9:
			case 0xD4:
			case 0xD5:
			case 0xD6:
			case 0xD7:
			case 0xD8:
			{
				binary_t b;
				return get_msgpack_binary(b) && sax->binary(b);
			}
			case 0xCA:
			{
				float number{};
				return get_number(input_format_t::msgpack, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 0xCB:
			{
				double number{};
				return get_number(input_format_t::msgpack, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 0xCC:
			{
				std::uint8_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_unsigned(number);
			}
			case 0xCD:
			{
				std::uint16_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_unsigned(number);
			}
			case 0xCE:
			{
				std::uint32_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_unsigned(number);
			}
			case 0xCF:
			{
				std::uint64_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_unsigned(number);
			}
			case 0xD0:
			{
				std::int8_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_integer(number);
			}
			case 0xD1:
			{
				std::int16_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_integer(number);
			}
			case 0xD2:
			{
				std::int32_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_integer(number);
			}
			case 0xD3:
			{
				std::int64_t number{};
				return get_number(input_format_t::msgpack, number) && sax->number_integer(number);
			}
			case 0xDC:
			{
				std::uint16_t len{};
				return get_number(input_format_t::msgpack, len) && get_msgpack_array(static_cast<std::size_t>(len));
			}
			case 0xDD:
			{
				std::uint32_t len{};
				return get_number(input_format_t::msgpack, len) && get_msgpack_array(conditional_static_cast<std::size_t>(len));
			}
			case 0xDE:
			{
				std::uint16_t len{};
				return get_number(input_format_t::msgpack, len) && get_msgpack_object(static_cast<std::size_t>(len));
			}
			case 0xDF:
			{
				std::uint32_t len{};
				return get_number(input_format_t::msgpack, len) && get_msgpack_object(conditional_static_cast<std::size_t>(len));
			}
			case 0xE0:
			case 0xE1:
			case 0xE2:
			case 0xE3:
			case 0xE4:
			case 0xE5:
			case 0xE6:
			case 0xE7:
			case 0xE8:
			case 0xE9:
			case 0xEA:
			case 0xEB:
			case 0xEC:
			case 0xED:
			case 0xEE:
			case 0xEF:
			case 0xF0:
			case 0xF1:
			case 0xF2:
			case 0xF3:
			case 0xF4:
			case 0xF5:
			case 0xF6:
			case 0xF7:
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
			case 0xFC:
			case 0xFD:
			case 0xFE:
			case 0xFF:
				return sax->number_integer(static_cast<std::int8_t>(current));
			default:
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
					exception_message(input_format_t::msgpack, concat("invalid byte: 0x", last_token), "value"), nullptr));
			}
			}
		}
		bool get_msgpack_string(string_t& result)
		{
			if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format_t::msgpack, "string")))
			{
				return false;
			}
			switch (current)
			{
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA4:
			case 0xA5:
			case 0xA6:
			case 0xA7:
			case 0xA8:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
			case 0xAF:
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
			case 0xB8:
			case 0xB9:
			case 0xBA:
			case 0xBB:
			case 0xBC:
			case 0xBD:
			case 0xBE:
			case 0xBF:
			{
				return get_string(input_format_t::msgpack, static_cast<unsigned int>(current) & 0x1Fu, result);
			}
			case 0xD9:
			{
				std::uint8_t len{};
				return get_number(input_format_t::msgpack, len) && get_string(input_format_t::msgpack, len, result);
			}
			case 0xDA:
			{
				std::uint16_t len{};
				return get_number(input_format_t::msgpack, len) && get_string(input_format_t::msgpack, len, result);
			}
			case 0xDB:
			{
				std::uint32_t len{};
				return get_number(input_format_t::msgpack, len) && get_string(input_format_t::msgpack, len, result);
			}
			default:
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read,
					exception_message(input_format_t::msgpack, concat("expected length specification (0xA0-0xBF, 0xD9-0xDB); last byte: 0x", last_token), "string"), nullptr));
			}
			}
		}
		bool get_msgpack_binary(binary_t& result)
		{
			auto assign_and_return_true = [&result](std::int8_t subtype)
				{
					result.set_subtype(static_cast<std::uint8_t>(subtype));
					return true;
				};
			switch (current)
			{
			case 0xC4:
			{
				std::uint8_t len{};
				return get_number(input_format_t::msgpack, len) &&
					get_binary(input_format_t::msgpack, len, result);
			}
			case 0xC5:
			{
				std::uint16_t len{};
				return get_number(input_format_t::msgpack, len) &&
					get_binary(input_format_t::msgpack, len, result);
			}
			case 0xC6:
			{
				std::uint32_t len{};
				return get_number(input_format_t::msgpack, len) &&
					get_binary(input_format_t::msgpack, len, result);
			}
			case 0xC7:
			{
				std::uint8_t len{};
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, len) &&
					get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, len, result) &&
					assign_and_return_true(subtype);
			}
			case 0xC8:
			{
				std::uint16_t len{};
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, len) &&
					get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, len, result) &&
					assign_and_return_true(subtype);
			}
			case 0xC9:
			{
				std::uint32_t len{};
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, len) &&
					get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, len, result) &&
					assign_and_return_true(subtype);
			}
			case 0xD4:
			{
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, 1, result) &&
					assign_and_return_true(subtype);
			}
			case 0xD5:
			{
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, 2, result) &&
					assign_and_return_true(subtype);
			}
			case 0xD6:
			{
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, 4, result) &&
					assign_and_return_true(subtype);
			}
			case 0xD7:
			{
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, 8, result) &&
					assign_and_return_true(subtype);
			}
			case 0xD8:
			{
				std::int8_t subtype{};
				return get_number(input_format_t::msgpack, subtype) &&
					get_binary(input_format_t::msgpack, 16, result) &&
					assign_and_return_true(subtype);
			}
			default:
				return false;
			}
		}
		bool get_msgpack_array(const std::size_t len)
		{
			if (JSON_HEDLEY_UNLIKELY(!sax->start_array(len)))
			{
				return false;
			}
			for (std::size_t i = 0; i < len; ++i)
			{
				if (JSON_HEDLEY_UNLIKELY(!parse_msgpack_internal()))
				{
					return false;
				}
			}
			return sax->end_array();
		}
		bool get_msgpack_object(const std::size_t len)
		{
			if (JSON_HEDLEY_UNLIKELY(!sax->start_object(len)))
			{
				return false;
			}
			string_t key;
			for (std::size_t i = 0; i < len; ++i)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!get_msgpack_string(key) || !sax->key(key)))
				{
					return false;
				}
				if (JSON_HEDLEY_UNLIKELY(!parse_msgpack_internal()))
				{
					return false;
				}
				key.clear();
			}
			return sax->end_object();
		}
		bool parse_ubjson_internal(const bool get_char = true)
		{
			return get_ubjson_value(get_char ? get_ignore_noop() : current);
		}
		bool get_ubjson_string(string_t& result, const bool get_char = true)
		{
			if (get_char)
			{
				get();
			}
			if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "value")))
			{
				return false;
			}
			switch (current)
			{
			case 'U':
			{
				std::uint8_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'i':
			{
				std::int8_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'I':
			{
				std::int16_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'l':
			{
				std::int32_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'L':
			{
				std::int64_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'u':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint16_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'm':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint32_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			case 'M':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint64_t len{};
				return get_number(input_format, len) && get_string(input_format, len, result);
			}
			default:
				break;
			}
			auto last_token = get_token_string();
			std::string message;
			if (input_format != input_format_t::bjdata)
			{
				message = "expected length type specification (U, i, I, l, L); last byte: 0x" + last_token;
			}
			else
			{
				message = "expected length type specification (U, i, u, I, m, l, M, L); last byte: 0x" + last_token;
			}
			return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read, exception_message(input_format, message, "string"), nullptr));
		}
		bool get_ubjson_ndarray_size(std::vector<size_t>& dim)
		{
			std::pair<std::size_t, char_int_type> size_and_type;
			size_t dimlen = 0;
			bool no_ndarray = true;
			if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_type(size_and_type, no_ndarray)))
			{
				return false;
			}
			if (size_and_type.first != npos)
			{
				if (size_and_type.second != 0)
				{
					if (size_and_type.second != 'N')
					{
						for (std::size_t i = 0; i < size_and_type.first; ++i)
						{
							if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_value(dimlen, no_ndarray, size_and_type.second)))
							{
								return false;
							}
							dim.push_back(dimlen);
						}
					}
				}
				else
				{
					for (std::size_t i = 0; i < size_and_type.first; ++i)
					{
						if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_value(dimlen, no_ndarray)))
						{
							return false;
						}
						dim.push_back(dimlen);
					}
				}
			}
			else
			{
				while (current != ']')
				{
					if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_value(dimlen, no_ndarray, current)))
					{
						return false;
					}
					dim.push_back(dimlen);
					get_ignore_noop();
				}
			}
			return true;
		}
		bool get_ubjson_size_value(std::size_t& result, bool& is_ndarray, char_int_type prefix = 0)
		{
			if (prefix == 0)
			{
				prefix = get_ignore_noop();
			}
			switch (prefix)
			{
			case 'U':
			{
				std::uint8_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'i':
			{
				std::int8_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				if (number < 0)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(113, chars_read,
						exception_message(input_format, "count in an optimized container must be positive", "size"), nullptr));
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'I':
			{
				std::int16_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				if (number < 0)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(113, chars_read,
						exception_message(input_format, "count in an optimized container must be positive", "size"), nullptr));
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'l':
			{
				std::int32_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				if (number < 0)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(113, chars_read,
						exception_message(input_format, "count in an optimized container must be positive", "size"), nullptr));
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'L':
			{
				std::int64_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				if (number < 0)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(113, chars_read,
						exception_message(input_format, "count in an optimized container must be positive", "size"), nullptr));
				}
				if (!value_in_range_of<std::size_t>(number))
				{
					return sax->parse_error(chars_read, get_token_string(), out_of_range::create(408,
						exception_message(input_format, "integer value overflow", "size"), nullptr));
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'u':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint16_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				result = static_cast<std::size_t>(number);
				return true;
			}
			case 'm':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint32_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				result = conditional_static_cast<std::size_t>(number);
				return true;
			}
			case 'M':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint64_t number{};
				if (JSON_HEDLEY_UNLIKELY(!get_number(input_format, number)))
				{
					return false;
				}
				if (!value_in_range_of<std::size_t>(number))
				{
					return sax->parse_error(chars_read, get_token_string(), out_of_range::create(408,
						exception_message(input_format, "integer value overflow", "size"), nullptr));
				}
				result = detail::conditional_static_cast<std::size_t>(number);
				return true;
			}
			case '[':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				if (is_ndarray)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(113, chars_read, exception_message(input_format, "ndarray dimensional vector is not allowed", "size"), nullptr));
				}
				std::vector<size_t> dim;
				if (JSON_HEDLEY_UNLIKELY(!get_ubjson_ndarray_size(dim)))
				{
					return false;
				}
				if (dim.size() == 1 || (dim.size() == 2 && dim.at(0) == 1))
				{
					result = dim.at(dim.size() - 1);
					return true;
				}
				if (!dim.empty())
				{
					for (auto i : dim)
					{
						if (i == 0)
						{
							result = 0;
							return true;
						}
					}
					string_t key = "_ArraySize_";
					if (JSON_HEDLEY_UNLIKELY(!sax->start_object(3) || !sax->key(key) || !sax->start_array(dim.size())))
					{
						return false;
					}
					result = 1;
					for (auto i : dim)
					{
						result *= i;
						if (result == 0 || result == npos)
						{
							return sax->parse_error(chars_read, get_token_string(), out_of_range::create(408, exception_message(input_format, "excessive ndarray size caused overflow", "size"), nullptr));
						}
						if (JSON_HEDLEY_UNLIKELY(!sax->number_unsigned(static_cast<number_unsigned_t>(i))))
						{
							return false;
						}
					}
					is_ndarray = true;
					return sax->end_array();
				}
				result = 0;
				return true;
			}
			default:
				break;
			}
			auto last_token = get_token_string();
			std::string message;
			if (input_format != input_format_t::bjdata)
			{
				message = "expected length type specification (U, i, I, l, L) after '#'; last byte: 0x" + last_token;
			}
			else
			{
				message = "expected length type specification (U, i, u, I, m, l, M, L) after '#'; last byte: 0x" + last_token;
			}
			return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read, exception_message(input_format, message, "size"), nullptr));
		}
		bool get_ubjson_size_type(std::pair<std::size_t, char_int_type>& result, bool inside_ndarray = false)
		{
			result.first = npos;
			result.second = 0;
			bool is_ndarray = false;
			get_ignore_noop();
			if (current == '$')
			{
				result.second = get();
				if (input_format == input_format_t::bjdata
					&& JSON_HEDLEY_UNLIKELY(std::binary_search(bjd_optimized_type_markers.begin(), bjd_optimized_type_markers.end(), result.second)))
				{
					auto last_token = get_token_string();
					return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
						exception_message(input_format, concat("marker 0x", last_token, " is not a permitted optimized array type"), "type"), nullptr));
				}
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "type")))
				{
					return false;
				}
				get_ignore_noop();
				if (JSON_HEDLEY_UNLIKELY(current != '#'))
				{
					if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "value")))
					{
						return false;
					}
					auto last_token = get_token_string();
					return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
						exception_message(input_format, concat("expected '#' after type information; last byte: 0x", last_token), "size"), nullptr));
				}
				const bool is_error = get_ubjson_size_value(result.first, is_ndarray);
				if (input_format == input_format_t::bjdata && is_ndarray)
				{
					if (inside_ndarray)
					{
						return sax->parse_error(chars_read, get_token_string(), parse_error::create(112, chars_read,
							exception_message(input_format, "ndarray can not be recursive", "size"), nullptr));
					}
					result.second |= (1 << 8);
				}
				return is_error;
			}
			if (current == '#')
			{
				const bool is_error = get_ubjson_size_value(result.first, is_ndarray);
				if (input_format == input_format_t::bjdata && is_ndarray)
				{
					return sax->parse_error(chars_read, get_token_string(), parse_error::create(112, chars_read,
						exception_message(input_format, "ndarray requires both type and size", "size"), nullptr));
				}
				return is_error;
			}
			return true;
		}
		bool get_ubjson_value(const char_int_type prefix)
		{
			switch (prefix)
			{
			case char_traits<char_type>::eof():
				return unexpect_eof(input_format, "value");
			case 'T':
				return sax->boolean(true);
			case 'F':
				return sax->boolean(false);
			case 'Z':
				return sax->null();
			case 'U':
			{
				std::uint8_t number{};
				return get_number(input_format, number) && sax->number_unsigned(number);
			}
			case 'i':
			{
				std::int8_t number{};
				return get_number(input_format, number) && sax->number_integer(number);
			}
			case 'I':
			{
				std::int16_t number{};
				return get_number(input_format, number) && sax->number_integer(number);
			}
			case 'l':
			{
				std::int32_t number{};
				return get_number(input_format, number) && sax->number_integer(number);
			}
			case 'L':
			{
				std::int64_t number{};
				return get_number(input_format, number) && sax->number_integer(number);
			}
			case 'u':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint16_t number{};
				return get_number(input_format, number) && sax->number_unsigned(number);
			}
			case 'm':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint32_t number{};
				return get_number(input_format, number) && sax->number_unsigned(number);
			}
			case 'M':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				std::uint64_t number{};
				return get_number(input_format, number) && sax->number_unsigned(number);
			}
			case 'h':
			{
				if (input_format != input_format_t::bjdata)
				{
					break;
				}
				const auto byte1_raw = get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "number")))
				{
					return false;
				}
				const auto byte2_raw = get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "number")))
				{
					return false;
				}
				const auto byte1 = static_cast<unsigned char>(byte1_raw);
				const auto byte2 = static_cast<unsigned char>(byte2_raw);
				const auto half = static_cast<unsigned int>((byte2 << 8u) + byte1);
				const double val = [&half]
					{
						const int exp = (half >> 10u) & 0x1Fu;
						const unsigned int mant = half & 0x3FFu;
						JSON_ASSERT(0 <= exp && exp <= 32);
						JSON_ASSERT(mant <= 1024);
						switch (exp)
						{
						case 0:
							return std::ldexp(mant, -24);
						case 31:
							return (mant == 0)
								? std::numeric_limits<double>::infinity()
								: std::numeric_limits<double>::quiet_NaN();
						default:
							return std::ldexp(mant + 1024, exp - 25);
						}
					}();
				return sax->number_float((half & 0x8000u) != 0
					? static_cast<number_float_t>(-val)
					: static_cast<number_float_t>(val), "");
			}
			case 'd':
			{
				float number{};
				return get_number(input_format, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 'D':
			{
				double number{};
				return get_number(input_format, number) && sax->number_float(static_cast<number_float_t>(number), "");
			}
			case 'H':
			{
				return get_ubjson_high_precision_number();
			}
			case 'C':
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "char")))
				{
					return false;
				}
				if (JSON_HEDLEY_UNLIKELY(current > 127))
				{
					auto last_token = get_token_string();
					return sax->parse_error(chars_read, last_token, parse_error::create(113, chars_read,
						exception_message(input_format, concat("byte after 'C' must be in range 0x00..0x7F; last byte: 0x", last_token), "char"), nullptr));
				}
				string_t s(1, static_cast<typename string_t::value_type>(current));
				return sax->string(s);
			}
			case 'S':
			{
				string_t s;
				return get_ubjson_string(s) && sax->string(s);
			}
			case '[':
				return get_ubjson_array();
			case '{':
				return get_ubjson_object();
			default:
				break;
			}
			auto last_token = get_token_string();
			return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read, exception_message(input_format, "invalid byte: 0x" + last_token, "value"), nullptr));
		}
		bool get_ubjson_array()
		{
			std::pair<std::size_t, char_int_type> size_and_type;
			if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_type(size_and_type)))
			{
				return false;
			}
			if (input_format == input_format_t::bjdata && size_and_type.first != npos && (size_and_type.second & (1 << 8)) != 0)
			{
				size_and_type.second &= ~(static_cast<char_int_type>(1) << 8);
				auto it = std::lower_bound(bjd_types_map.begin(), bjd_types_map.end(), size_and_type.second, [](const bjd_type& p, char_int_type t)
					{
						return p.first < t;
					});
				string_t key = "_ArrayType_";
				if (JSON_HEDLEY_UNLIKELY(it == bjd_types_map.end() || it->first != size_and_type.second))
				{
					auto last_token = get_token_string();
					return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
						exception_message(input_format, "invalid byte: 0x" + last_token, "type"), nullptr));
				}
				string_t type = it->second;
				if (JSON_HEDLEY_UNLIKELY(!sax->key(key) || !sax->string(type)))
				{
					return false;
				}
				if (size_and_type.second == 'C')
				{
					size_and_type.second = 'U';
				}
				key = "_ArrayData_";
				if (JSON_HEDLEY_UNLIKELY(!sax->key(key) || !sax->start_array(size_and_type.first)))
				{
					return false;
				}
				for (std::size_t i = 0; i < size_and_type.first; ++i)
				{
					if (JSON_HEDLEY_UNLIKELY(!get_ubjson_value(size_and_type.second)))
					{
						return false;
					}
				}
				return (sax->end_array() && sax->end_object());
			}
			if (size_and_type.first != npos)
			{
				if (JSON_HEDLEY_UNLIKELY(!sax->start_array(size_and_type.first)))
				{
					return false;
				}
				if (size_and_type.second != 0)
				{
					if (size_and_type.second != 'N')
					{
						for (std::size_t i = 0; i < size_and_type.first; ++i)
						{
							if (JSON_HEDLEY_UNLIKELY(!get_ubjson_value(size_and_type.second)))
							{
								return false;
							}
						}
					}
				}
				else
				{
					for (std::size_t i = 0; i < size_and_type.first; ++i)
					{
						if (JSON_HEDLEY_UNLIKELY(!parse_ubjson_internal()))
						{
							return false;
						}
					}
				}
			}
			else
			{
				if (JSON_HEDLEY_UNLIKELY(!sax->start_array(static_cast<std::size_t>(-1))))
				{
					return false;
				}
				while (current != ']')
				{
					if (JSON_HEDLEY_UNLIKELY(!parse_ubjson_internal(false)))
					{
						return false;
					}
					get_ignore_noop();
				}
			}
			return sax->end_array();
		}
		bool get_ubjson_object()
		{
			std::pair<std::size_t, char_int_type> size_and_type;
			if (JSON_HEDLEY_UNLIKELY(!get_ubjson_size_type(size_and_type)))
			{
				return false;
			}
			if (input_format == input_format_t::bjdata && size_and_type.first != npos && (size_and_type.second & (1 << 8)) != 0)
			{
				auto last_token = get_token_string();
				return sax->parse_error(chars_read, last_token, parse_error::create(112, chars_read,
					exception_message(input_format, "BJData object does not support ND-array size in optimized format", "object"), nullptr));
			}
			string_t key;
			if (size_and_type.first != npos)
			{
				if (JSON_HEDLEY_UNLIKELY(!sax->start_object(size_and_type.first)))
				{
					return false;
				}
				if (size_and_type.second != 0)
				{
					for (std::size_t i = 0; i < size_and_type.first; ++i)
					{
						if (JSON_HEDLEY_UNLIKELY(!get_ubjson_string(key) || !sax->key(key)))
						{
							return false;
						}
						if (JSON_HEDLEY_UNLIKELY(!get_ubjson_value(size_and_type.second)))
						{
							return false;
						}
						key.clear();
					}
				}
				else
				{
					for (std::size_t i = 0; i < size_and_type.first; ++i)
					{
						if (JSON_HEDLEY_UNLIKELY(!get_ubjson_string(key) || !sax->key(key)))
						{
							return false;
						}
						if (JSON_HEDLEY_UNLIKELY(!parse_ubjson_internal()))
						{
							return false;
						}
						key.clear();
					}
				}
			}
			else
			{
				if (JSON_HEDLEY_UNLIKELY(!sax->start_object(static_cast<std::size_t>(-1))))
				{
					return false;
				}
				while (current != '}')
				{
					if (JSON_HEDLEY_UNLIKELY(!get_ubjson_string(key, false) || !sax->key(key)))
					{
						return false;
					}
					if (JSON_HEDLEY_UNLIKELY(!parse_ubjson_internal()))
					{
						return false;
					}
					get_ignore_noop();
					key.clear();
				}
			}
			return sax->end_object();
		}
		bool get_ubjson_high_precision_number()
		{
			std::size_t size{};
			bool no_ndarray = true;
			auto res = get_ubjson_size_value(size, no_ndarray);
			if (JSON_HEDLEY_UNLIKELY(!res))
			{
				return res;
			}
			std::vector<char> number_vector;
			for (std::size_t i = 0; i < size; ++i)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(input_format, "number")))
				{
					return false;
				}
				number_vector.push_back(static_cast<char>(current));
			}
			using ia_type = decltype(detail::input_adapter(number_vector));
			auto number_lexer = detail::lexer<BasicJsonType, ia_type>(detail::input_adapter(number_vector), false);
			const auto result_number = number_lexer.scan();
			const auto number_string = number_lexer.get_token_string();
			const auto result_remainder = number_lexer.scan();
			using token_type = typename detail::lexer_base<BasicJsonType>::token_type;
			if (JSON_HEDLEY_UNLIKELY(result_remainder != token_type::end_of_input))
			{
				return sax->parse_error(chars_read, number_string, parse_error::create(115, chars_read,
					exception_message(input_format, concat("invalid number text: ", number_lexer.get_token_string()), "high-precision number"), nullptr));
			}
			switch (result_number)
			{
			case token_type::value_integer:
				return sax->number_integer(number_lexer.get_number_integer());
			case token_type::value_unsigned:
				return sax->number_unsigned(number_lexer.get_number_unsigned());
			case token_type::value_float:
				return sax->number_float(number_lexer.get_number_float(), std::move(number_string));
			case token_type::uninitialized:
			case token_type::literal_true:
			case token_type::literal_false:
			case token_type::literal_null:
			case token_type::value_string:
			case token_type::begin_array:
			case token_type::begin_object:
			case token_type::end_array:
			case token_type::end_object:
			case token_type::name_separator:
			case token_type::value_separator:
			case token_type::parse_error:
			case token_type::end_of_input:
			case token_type::literal_or_value:
			default:
				return sax->parse_error(chars_read, number_string, parse_error::create(115, chars_read,
					exception_message(input_format, concat("invalid number text: ", number_lexer.get_token_string()), "high-precision number"), nullptr));
			}
		}
		char_int_type get()
		{
			++chars_read;
			return current = ia.get_character();
		}
		char_int_type get_ignore_noop()
		{
			do
			{
				get();
			} while (current == 'N');
			return current;
		}
		template<typename NumberType, bool InputIsLittleEndian = false>
		bool get_number(const input_format_t format, NumberType& result)
		{
			std::array<std::uint8_t, sizeof(NumberType)> vec{};
			for (std::size_t i = 0; i < sizeof(NumberType); ++i)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(format, "number")))
				{
					return false;
				}
				if (is_little_endian != (InputIsLittleEndian || format == input_format_t::bjdata))
				{
					vec[sizeof(NumberType) - i - 1] = static_cast<std::uint8_t>(current);
				}
				else
				{
					vec[i] = static_cast<std::uint8_t>(current);
				}
			}
			std::memcpy(&result, vec.data(), sizeof(NumberType));
			return true;
		}
		template<typename NumberType>
		bool get_string(const input_format_t format,
			const NumberType len,
			string_t& result)
		{
			bool success = true;
			for (NumberType i = 0; i < len; i++)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(format, "string")))
				{
					success = false;
					break;
				}
				result.push_back(static_cast<typename string_t::value_type>(current));
			}
			return success;
		}
		template<typename NumberType>
		bool get_binary(const input_format_t format,
			const NumberType len,
			binary_t& result)
		{
			bool success = true;
			for (NumberType i = 0; i < len; i++)
			{
				get();
				if (JSON_HEDLEY_UNLIKELY(!unexpect_eof(format, "binary")))
				{
					success = false;
					break;
				}
				result.push_back(static_cast<std::uint8_t>(current));
			}
			return success;
		}
		JSON_HEDLEY_NON_NULL(3)
			bool unexpect_eof(const input_format_t format, const char* context) const
		{
			if (JSON_HEDLEY_UNLIKELY(current == char_traits<char_type>::eof()))
			{
				return sax->parse_error(chars_read, "<end of file>",
					parse_error::create(110, chars_read, exception_message(format, "unexpected end of input", context), nullptr));
			}
			return true;
		}
		std::string get_token_string() const
		{
			std::array<char, 3> cr{ {} };
			static_cast<void>((std::snprintf)(cr.data(), cr.size(), "%.2hhX", static_cast<unsigned char>(current)));
			return std::string{ cr.data() };
		}
		std::string exception_message(const input_format_t format,
			const std::string& detail,
			const std::string& context) const
		{
			std::string error_msg = "syntax error while parsing ";
			switch (format)
			{
			case input_format_t::cbor:
				error_msg += "CBOR";
				break;
			case input_format_t::msgpack:
				error_msg += "MessagePack";
				break;
			case input_format_t::ubjson:
				error_msg += "UBJSON";
				break;
			case input_format_t::bson:
				error_msg += "BSON";
				break;
			case input_format_t::bjdata:
				error_msg += "BJData";
				break;
			case input_format_t::json:
			default:
				JSON_ASSERT(false);
			}
			return concat(error_msg, ' ', context, ": ", detail);
		}
	private:
		static JSON_INLINE_VARIABLE constexpr std::size_t npos = static_cast<std::size_t>(-1);
		InputAdapterType ia;
		char_int_type current = char_traits<char_type>::eof();
		std::size_t chars_read = 0;
		const bool is_little_endian = little_endianness();
		const input_format_t input_format = input_format_t::json;
		json_sax_t* sax = nullptr;
#define JSON_BINARY_READER_MAKE_BJD_OPTIMIZED_TYPE_MARKERS_ \
    make_array<char_int_type>('F', 'H', 'N', 'S', 'T', 'Z', '[', '{')
#define JSON_BINARY_READER_MAKE_BJD_TYPES_MAP_ \
    make_array<bjd_type>(                      \
    bjd_type{'C', "char"},                     \
    bjd_type{'D', "double"},                   \
    bjd_type{'I', "int16"},                    \
    bjd_type{'L', "int64"},                    \
    bjd_type{'M', "uint64"},                   \
    bjd_type{'U', "uint8"},                    \
    bjd_type{'d', "single"},                   \
    bjd_type{'i', "int8"},                     \
    bjd_type{'l', "int32"},                    \
    bjd_type{'m', "uint32"},                   \
    bjd_type{'u', "uint16"})
		JSON_PRIVATE_UNLESS_TESTED:
		const decltype(JSON_BINARY_READER_MAKE_BJD_OPTIMIZED_TYPE_MARKERS_) bjd_optimized_type_markers =
			JSON_BINARY_READER_MAKE_BJD_OPTIMIZED_TYPE_MARKERS_;
		using bjd_type = std::pair<char_int_type, string_t>;
		const decltype(JSON_BINARY_READER_MAKE_BJD_TYPES_MAP_) bjd_types_map =
			JSON_BINARY_READER_MAKE_BJD_TYPES_MAP_;
#undef JSON_BINARY_READER_MAKE_BJD_OPTIMIZED_TYPE_MARKERS_
#undef JSON_BINARY_READER_MAKE_BJD_TYPES_MAP_
	};
#ifndef JSON_HAS_CPP_17
	template<typename BasicJsonType, typename InputAdapterType, typename SAX>
	constexpr std::size_t binary_reader<BasicJsonType, InputAdapterType, SAX>::npos;
#endif
}
NLOHMANN_JSON_NAMESPACE_END
#include <cmath> 
#include <cstdint> 
#include <functional> 
#include <string> 
#include <utility> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	enum class parse_event_t : std::uint8_t
	{
		object_start,
		object_end,
		array_start,
		array_end,
		key,
		value
	};
	template<typename BasicJsonType>
	using parser_callback_t =
		std::function<bool(int, parse_event_t, BasicJsonType&)>;
	template<typename BasicJsonType, typename InputAdapterType>
	class parser
	{
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using string_t = typename BasicJsonType::string_t;
		using lexer_t = lexer<BasicJsonType, InputAdapterType>;
		using token_type = typename lexer_t::token_type;
	public:
		explicit parser(InputAdapterType&& adapter,
			const parser_callback_t<BasicJsonType> cb = nullptr,
			const bool allow_exceptions_ = true,
			const bool skip_comments = false)
			: callback(cb)
			, m_lexer(std::move(adapter), skip_comments)
			, allow_exceptions(allow_exceptions_)
		{
			get_token();
		}
		void parse(const bool strict, BasicJsonType& result)
		{
			if (callback)
			{
				json_sax_dom_callback_parser<BasicJsonType> sdp(result, callback, allow_exceptions);
				sax_parse_internal(&sdp);
				if (strict && (get_token() != token_type::end_of_input))
				{
					sdp.parse_error(m_lexer.get_position(),
						m_lexer.get_token_string(),
						parse_error::create(101, m_lexer.get_position(),
							exception_message(token_type::end_of_input, "value"), nullptr));
				}
				if (sdp.is_errored())
				{
					result = value_t::discarded;
					return;
				}
				if (result.is_discarded())
				{
					result = nullptr;
				}
			}
			else
			{
				json_sax_dom_parser<BasicJsonType> sdp(result, allow_exceptions);
				sax_parse_internal(&sdp);
				if (strict && (get_token() != token_type::end_of_input))
				{
					sdp.parse_error(m_lexer.get_position(),
						m_lexer.get_token_string(),
						parse_error::create(101, m_lexer.get_position(), exception_message(token_type::end_of_input, "value"), nullptr));
				}
				if (sdp.is_errored())
				{
					result = value_t::discarded;
					return;
				}
			}
			result.assert_invariant();
		}
		bool accept(const bool strict = true)
		{
			json_sax_acceptor<BasicJsonType> sax_acceptor;
			return sax_parse(&sax_acceptor, strict);
		}
		template<typename SAX>
		JSON_HEDLEY_NON_NULL(2)
			bool sax_parse(SAX* sax, const bool strict = true)
		{
			(void)detail::is_sax_static_asserts<SAX, BasicJsonType> {};
			const bool result = sax_parse_internal(sax);
			if (result && strict && (get_token() != token_type::end_of_input))
			{
				return sax->parse_error(m_lexer.get_position(),
					m_lexer.get_token_string(),
					parse_error::create(101, m_lexer.get_position(), exception_message(token_type::end_of_input, "value"), nullptr));
			}
			return result;
		}
	private:
		template<typename SAX>
		JSON_HEDLEY_NON_NULL(2)
			bool sax_parse_internal(SAX* sax)
		{
			std::vector<bool> states;
			bool skip_to_state_evaluation = false;
			while (true)
			{
				if (!skip_to_state_evaluation)
				{
					switch (last_token)
					{
					case token_type::begin_object:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->start_object(static_cast<std::size_t>(-1))))
						{
							return false;
						}
						if (get_token() == token_type::end_object)
						{
							if (JSON_HEDLEY_UNLIKELY(!sax->end_object()))
							{
								return false;
							}
							break;
						}
						if (JSON_HEDLEY_UNLIKELY(last_token != token_type::value_string))
						{
							return sax->parse_error(m_lexer.get_position(),
								m_lexer.get_token_string(),
								parse_error::create(101, m_lexer.get_position(), exception_message(token_type::value_string, "object key"), nullptr));
						}
						if (JSON_HEDLEY_UNLIKELY(!sax->key(m_lexer.get_string())))
						{
							return false;
						}
						if (JSON_HEDLEY_UNLIKELY(get_token() != token_type::name_separator))
						{
							return sax->parse_error(m_lexer.get_position(),
								m_lexer.get_token_string(),
								parse_error::create(101, m_lexer.get_position(), exception_message(token_type::name_separator, "object separator"), nullptr));
						}
						states.push_back(false);
						get_token();
						continue;
					}
					case token_type::begin_array:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->start_array(static_cast<std::size_t>(-1))))
						{
							return false;
						}
						if (get_token() == token_type::end_array)
						{
							if (JSON_HEDLEY_UNLIKELY(!sax->end_array()))
							{
								return false;
							}
							break;
						}
						states.push_back(true);
						continue;
					}
					case token_type::value_float:
					{
						const auto res = m_lexer.get_number_float();
						if (JSON_HEDLEY_UNLIKELY(!std::isfinite(res)))
						{
							return sax->parse_error(m_lexer.get_position(),
								m_lexer.get_token_string(),
								out_of_range::create(406, concat("number overflow parsing '", m_lexer.get_token_string(), '\''), nullptr));
						}
						if (JSON_HEDLEY_UNLIKELY(!sax->number_float(res, m_lexer.get_string())))
						{
							return false;
						}
						break;
					}
					case token_type::literal_false:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->boolean(false)))
						{
							return false;
						}
						break;
					}
					case token_type::literal_null:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->null()))
						{
							return false;
						}
						break;
					}
					case token_type::literal_true:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->boolean(true)))
						{
							return false;
						}
						break;
					}
					case token_type::value_integer:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->number_integer(m_lexer.get_number_integer())))
						{
							return false;
						}
						break;
					}
					case token_type::value_string:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->string(m_lexer.get_string())))
						{
							return false;
						}
						break;
					}
					case token_type::value_unsigned:
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->number_unsigned(m_lexer.get_number_unsigned())))
						{
							return false;
						}
						break;
					}
					case token_type::parse_error:
					{
						return sax->parse_error(m_lexer.get_position(),
							m_lexer.get_token_string(),
							parse_error::create(101, m_lexer.get_position(), exception_message(token_type::uninitialized, "value"), nullptr));
					}
					case token_type::end_of_input:
					{
						if (JSON_HEDLEY_UNLIKELY(m_lexer.get_position().chars_read_total == 1))
						{
							return sax->parse_error(m_lexer.get_position(),
								m_lexer.get_token_string(),
								parse_error::create(101, m_lexer.get_position(),
									"attempting to parse an empty input; check that your input string or stream contains the expected JSON", nullptr));
						}
						return sax->parse_error(m_lexer.get_position(),
							m_lexer.get_token_string(),
							parse_error::create(101, m_lexer.get_position(), exception_message(token_type::literal_or_value, "value"), nullptr));
					}
					case token_type::uninitialized:
					case token_type::end_array:
					case token_type::end_object:
					case token_type::name_separator:
					case token_type::value_separator:
					case token_type::literal_or_value:
					default:
					{
						return sax->parse_error(m_lexer.get_position(),
							m_lexer.get_token_string(),
							parse_error::create(101, m_lexer.get_position(), exception_message(token_type::literal_or_value, "value"), nullptr));
					}
					}
				}
				else
				{
					skip_to_state_evaluation = false;
				}
				if (states.empty())
				{
					return true;
				}
				if (states.back())
				{
					if (get_token() == token_type::value_separator)
					{
						get_token();
						continue;
					}
					if (JSON_HEDLEY_LIKELY(last_token == token_type::end_array))
					{
						if (JSON_HEDLEY_UNLIKELY(!sax->end_array()))
						{
							return false;
						}
						JSON_ASSERT(!states.empty());
						states.pop_back();
						skip_to_state_evaluation = true;
						continue;
					}
					return sax->parse_error(m_lexer.get_position(),
						m_lexer.get_token_string(),
						parse_error::create(101, m_lexer.get_position(), exception_message(token_type::end_array, "array"), nullptr));
				}
				if (get_token() == token_type::value_separator)
				{
					if (JSON_HEDLEY_UNLIKELY(get_token() != token_type::value_string))
					{
						return sax->parse_error(m_lexer.get_position(),
							m_lexer.get_token_string(),
							parse_error::create(101, m_lexer.get_position(), exception_message(token_type::value_string, "object key"), nullptr));
					}
					if (JSON_HEDLEY_UNLIKELY(!sax->key(m_lexer.get_string())))
					{
						return false;
					}
					if (JSON_HEDLEY_UNLIKELY(get_token() != token_type::name_separator))
					{
						return sax->parse_error(m_lexer.get_position(),
							m_lexer.get_token_string(),
							parse_error::create(101, m_lexer.get_position(), exception_message(token_type::name_separator, "object separator"), nullptr));
					}
					get_token();
					continue;
				}
				if (JSON_HEDLEY_LIKELY(last_token == token_type::end_object))
				{
					if (JSON_HEDLEY_UNLIKELY(!sax->end_object()))
					{
						return false;
					}
					JSON_ASSERT(!states.empty());
					states.pop_back();
					skip_to_state_evaluation = true;
					continue;
				}
				return sax->parse_error(m_lexer.get_position(),
					m_lexer.get_token_string(),
					parse_error::create(101, m_lexer.get_position(), exception_message(token_type::end_object, "object"), nullptr));
			}
		}
		token_type get_token()
		{
			return last_token = m_lexer.scan();
		}
		std::string exception_message(const token_type expected, const std::string& context)
		{
			std::string error_msg = "syntax error ";
			if (!context.empty())
			{
				error_msg += concat("while parsing ", context, ' ');
			}
			error_msg += "- ";
			if (last_token == token_type::parse_error)
			{
				error_msg += concat(m_lexer.get_error_message(), "; last read: '",
					m_lexer.get_token_string(), '\'');
			}
			else
			{
				error_msg += concat("unexpected ", lexer_t::token_type_name(last_token));
			}
			if (expected != token_type::uninitialized)
			{
				error_msg += concat("; expected ", lexer_t::token_type_name(expected));
			}
			return error_msg;
		}
	private:
		const parser_callback_t<BasicJsonType> callback = nullptr;
		token_type last_token = token_type::uninitialized;
		lexer_t m_lexer;
		const bool allow_exceptions = true;
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstddef> 
#include <limits>  
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	class primitive_iterator_t
	{
	private:
		using difference_type = std::ptrdiff_t;
		static constexpr difference_type begin_value = 0;
		static constexpr difference_type end_value = begin_value + 1;
	JSON_PRIVATE_UNLESS_TESTED:
		difference_type m_it = (std::numeric_limits<std::ptrdiff_t>::min)();
	public:
		constexpr difference_type get_value() const noexcept
		{
			return m_it;
		}
		void set_begin() noexcept
		{
			m_it = begin_value;
		}
		void set_end() noexcept
		{
			m_it = end_value;
		}
		constexpr bool is_begin() const noexcept
		{
			return m_it == begin_value;
		}
		constexpr bool is_end() const noexcept
		{
			return m_it == end_value;
		}
		friend constexpr bool operator==(primitive_iterator_t lhs, primitive_iterator_t rhs) noexcept
		{
			return lhs.m_it == rhs.m_it;
		}
		friend constexpr bool operator<(primitive_iterator_t lhs, primitive_iterator_t rhs) noexcept
		{
			return lhs.m_it < rhs.m_it;
		}
		primitive_iterator_t operator+(difference_type n) noexcept
		{
			auto result = *this;
			result += n;
			return result;
		}
		friend constexpr difference_type operator-(primitive_iterator_t lhs, primitive_iterator_t rhs) noexcept
		{
			return lhs.m_it - rhs.m_it;
		}
		primitive_iterator_t& operator++() noexcept
		{
			++m_it;
			return *this;
		}
		primitive_iterator_t operator++(int)&noexcept
		{
			auto result = *this;
			++m_it;
			return result;
		}
		primitive_iterator_t& operator--() noexcept
		{
			--m_it;
			return *this;
		}
		primitive_iterator_t operator--(int)&noexcept
		{
			auto result = *this;
			--m_it;
			return result;
		}
		primitive_iterator_t& operator+=(difference_type n) noexcept
		{
			m_it += n;
			return *this;
		}
		primitive_iterator_t& operator-=(difference_type n) noexcept
		{
			m_it -= n;
			return *this;
		}
	};
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename BasicJsonType> struct internal_iterator
	{
		typename BasicJsonType::object_t::iterator object_iterator{};
		typename BasicJsonType::array_t::iterator array_iterator{};
		primitive_iterator_t primitive_iterator{};
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <iterator> 
#include <type_traits> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename IteratorType> class iteration_proxy;
	template<typename IteratorType> class iteration_proxy_value;
	template<typename BasicJsonType>
	class iter_impl
	{
		using other_iter_impl = iter_impl<typename std::conditional<std::is_const<BasicJsonType>::value, typename std::remove_const<BasicJsonType>::type, const BasicJsonType>::type>;
		friend other_iter_impl;
		friend BasicJsonType;
		friend iteration_proxy<iter_impl>;
		friend iteration_proxy_value<iter_impl>;
		using object_t = typename BasicJsonType::object_t;
		using array_t = typename BasicJsonType::array_t;
		static_assert(is_basic_json<typename std::remove_const<BasicJsonType>::type>::value,
			"iter_impl only accepts (const) basic_json");
		static_assert(std::is_base_of<std::bidirectional_iterator_tag, std::bidirectional_iterator_tag>::value
			&& std::is_base_of<std::bidirectional_iterator_tag, typename std::iterator_traits<typename array_t::iterator>::iterator_category>::value,
			"basic_json iterator assumes array and object type iterators satisfy the LegacyBidirectionalIterator named requirement.");
	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = typename BasicJsonType::value_type;
		using difference_type = typename BasicJsonType::difference_type;
		using pointer = typename std::conditional<std::is_const<BasicJsonType>::value,
			typename BasicJsonType::const_pointer,
			typename BasicJsonType::pointer>::type;
		using reference =
			typename std::conditional<std::is_const<BasicJsonType>::value,
			typename BasicJsonType::const_reference,
			typename BasicJsonType::reference>::type;
		iter_impl() = default;
		~iter_impl() = default;
		iter_impl(iter_impl&&) noexcept = default;
		iter_impl& operator=(iter_impl&&) noexcept = default;
		explicit iter_impl(pointer object) noexcept : m_object(object)
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				m_it.object_iterator = typename object_t::iterator();
				break;
			}
			case value_t::array:
			{
				m_it.array_iterator = typename array_t::iterator();
				break;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				m_it.primitive_iterator = primitive_iterator_t();
				break;
			}
			}
		}
		iter_impl(const iter_impl<const BasicJsonType>& other) noexcept
			: m_object(other.m_object), m_it(other.m_it)
		{
		}
		iter_impl& operator=(const iter_impl<const BasicJsonType>& other) noexcept
		{
			if (&other != this)
			{
				m_object = other.m_object;
				m_it = other.m_it;
			}
			return *this;
		}
		iter_impl(const iter_impl<typename std::remove_const<BasicJsonType>::type>& other) noexcept
			: m_object(other.m_object), m_it(other.m_it)
		{
		}
		iter_impl& operator=(const iter_impl<typename std::remove_const<BasicJsonType>::type>& other) noexcept
		{
			m_object = other.m_object;
			m_it = other.m_it;
			return *this;
		}
	JSON_PRIVATE_UNLESS_TESTED:
		void set_begin() noexcept
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				m_it.object_iterator = m_object->m_data.m_value.object->begin();
				break;
			}
			case value_t::array:
			{
				m_it.array_iterator = m_object->m_data.m_value.array->begin();
				break;
			}
			case value_t::null:
			{
				m_it.primitive_iterator.set_end();
				break;
			}
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				m_it.primitive_iterator.set_begin();
				break;
			}
			}
		}
		void set_end() noexcept
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				m_it.object_iterator = m_object->m_data.m_value.object->end();
				break;
			}
			case value_t::array:
			{
				m_it.array_iterator = m_object->m_data.m_value.array->end();
				break;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				m_it.primitive_iterator.set_end();
				break;
			}
			}
		}
	public:
		reference operator*() const
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				JSON_ASSERT(m_it.object_iterator != m_object->m_data.m_value.object->end());
				return m_it.object_iterator->second;
			}
			case value_t::array:
			{
				JSON_ASSERT(m_it.array_iterator != m_object->m_data.m_value.array->end());
				return *m_it.array_iterator;
			}
			case value_t::null:
				JSON_THROW(invalid_iterator::create(214, "cannot get value", m_object));
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				if (JSON_HEDLEY_LIKELY(m_it.primitive_iterator.is_begin()))
				{
					return *m_object;
				}
				JSON_THROW(invalid_iterator::create(214, "cannot get value", m_object));
			}
			}
		}
		pointer operator->() const
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				JSON_ASSERT(m_it.object_iterator != m_object->m_data.m_value.object->end());
				return &(m_it.object_iterator->second);
			}
			case value_t::array:
			{
				JSON_ASSERT(m_it.array_iterator != m_object->m_data.m_value.array->end());
				return &*m_it.array_iterator;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				if (JSON_HEDLEY_LIKELY(m_it.primitive_iterator.is_begin()))
				{
					return m_object;
				}
				JSON_THROW(invalid_iterator::create(214, "cannot get value", m_object));
			}
			}
		}
		iter_impl operator++(int)&
		{
			auto result = *this;
			++(*this);
			return result;
		}
		iter_impl& operator++()
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				std::advance(m_it.object_iterator, 1);
				break;
			}
			case value_t::array:
			{
				std::advance(m_it.array_iterator, 1);
				break;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				++m_it.primitive_iterator;
				break;
			}
			}
			return *this;
		}
		iter_impl operator--(int)&
		{
			auto result = *this;
			--(*this);
			return result;
		}
		iter_impl& operator--()
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
			{
				std::advance(m_it.object_iterator, -1);
				break;
			}
			case value_t::array:
			{
				std::advance(m_it.array_iterator, -1);
				break;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				--m_it.primitive_iterator;
				break;
			}
			}
			return *this;
		}
		template < typename IterImpl, detail::enable_if_t < (std::is_same<IterImpl, iter_impl>::value || std::is_same<IterImpl, other_iter_impl>::value), std::nullptr_t > = nullptr >
		bool operator==(const IterImpl& other) const
		{
			if (JSON_HEDLEY_UNLIKELY(m_object != other.m_object))
			{
				JSON_THROW(invalid_iterator::create(212, "cannot compare iterators of different containers", m_object));
			}
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
				return (m_it.object_iterator == other.m_it.object_iterator);
			case value_t::array:
				return (m_it.array_iterator == other.m_it.array_iterator);
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
				return (m_it.primitive_iterator == other.m_it.primitive_iterator);
			}
		}
		template < typename IterImpl, detail::enable_if_t < (std::is_same<IterImpl, iter_impl>::value || std::is_same<IterImpl, other_iter_impl>::value), std::nullptr_t > = nullptr >
		bool operator!=(const IterImpl& other) const
		{
			return !operator==(other);
		}
		bool operator<(const iter_impl& other) const
		{
			if (JSON_HEDLEY_UNLIKELY(m_object != other.m_object))
			{
				JSON_THROW(invalid_iterator::create(212, "cannot compare iterators of different containers", m_object));
			}
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
				JSON_THROW(invalid_iterator::create(213, "cannot compare order of object iterators", m_object));
			case value_t::array:
				return (m_it.array_iterator < other.m_it.array_iterator);
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
				return (m_it.primitive_iterator < other.m_it.primitive_iterator);
			}
		}
		bool operator<=(const iter_impl& other) const
		{
			return !other.operator < (*this);
		}
		bool operator>(const iter_impl& other) const
		{
			return !operator<=(other);
		}
		bool operator>=(const iter_impl& other) const
		{
			return !operator<(other);
		}
		iter_impl& operator+=(difference_type i)
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
				JSON_THROW(invalid_iterator::create(209, "cannot use offsets with object iterators", m_object));
			case value_t::array:
			{
				std::advance(m_it.array_iterator, i);
				break;
			}
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				m_it.primitive_iterator += i;
				break;
			}
			}
			return *this;
		}
		iter_impl& operator-=(difference_type i)
		{
			return operator+=(-i);
		}
		iter_impl operator+(difference_type i) const
		{
			auto result = *this;
			result += i;
			return result;
		}
		friend iter_impl operator+(difference_type i, const iter_impl& it)
		{
			auto result = it;
			result += i;
			return result;
		}
		iter_impl operator-(difference_type i) const
		{
			auto result = *this;
			result -= i;
			return result;
		}
		difference_type operator-(const iter_impl& other) const
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
				JSON_THROW(invalid_iterator::create(209, "cannot use offsets with object iterators", m_object));
			case value_t::array:
				return m_it.array_iterator - other.m_it.array_iterator;
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
				return m_it.primitive_iterator - other.m_it.primitive_iterator;
			}
		}
		reference operator[](difference_type n) const
		{
			JSON_ASSERT(m_object != nullptr);
			switch (m_object->m_data.m_type)
			{
			case value_t::object:
				JSON_THROW(invalid_iterator::create(208, "cannot use operator[] for object iterators", m_object));
			case value_t::array:
				return *std::next(m_it.array_iterator, n);
			case value_t::null:
				JSON_THROW(invalid_iterator::create(214, "cannot get value", m_object));
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				if (JSON_HEDLEY_LIKELY(m_it.primitive_iterator.get_value() == -n))
				{
					return *m_object;
				}
				JSON_THROW(invalid_iterator::create(214, "cannot get value", m_object));
			}
			}
		}
		const typename object_t::key_type& key() const
		{
			JSON_ASSERT(m_object != nullptr);
			if (JSON_HEDLEY_LIKELY(m_object->is_object()))
			{
				return m_it.object_iterator->first;
			}
			JSON_THROW(invalid_iterator::create(207, "cannot use key() for non-object iterators", m_object));
		}
		reference value() const
		{
			return operator*();
		}
	JSON_PRIVATE_UNLESS_TESTED:
		pointer m_object = nullptr;
		internal_iterator<typename std::remove_const<BasicJsonType>::type> m_it{};
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <cstddef> 
#include <iterator> 
#include <utility> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename Base>
	class json_reverse_iterator : public std::reverse_iterator<Base>
	{
	public:
		using difference_type = std::ptrdiff_t;
		using base_iterator = std::reverse_iterator<Base>;
		using reference = typename Base::reference;
		explicit json_reverse_iterator(const typename base_iterator::iterator_type& it) noexcept
			: base_iterator(it) {
		}
		explicit json_reverse_iterator(const base_iterator& it) noexcept : base_iterator(it) {}
		json_reverse_iterator operator++(int)&
		{
			return static_cast<json_reverse_iterator>(base_iterator::operator++(1));
		}
		json_reverse_iterator& operator++()
		{
			return static_cast<json_reverse_iterator&>(base_iterator::operator++());
		}
		json_reverse_iterator operator--(int)&
		{
			return static_cast<json_reverse_iterator>(base_iterator::operator--(1));
		}
		json_reverse_iterator& operator--()
		{
			return static_cast<json_reverse_iterator&>(base_iterator::operator--());
		}
		json_reverse_iterator& operator+=(difference_type i)
		{
			return static_cast<json_reverse_iterator&>(base_iterator::operator+=(i));
		}
		json_reverse_iterator operator+(difference_type i) const
		{
			return static_cast<json_reverse_iterator>(base_iterator::operator+(i));
		}
		json_reverse_iterator operator-(difference_type i) const
		{
			return static_cast<json_reverse_iterator>(base_iterator::operator-(i));
		}
		difference_type operator-(const json_reverse_iterator& other) const
		{
			return base_iterator(*this) - base_iterator(other);
		}
		reference operator[](difference_type n) const
		{
			return *(this->operator+(n));
		}
		auto key() const -> decltype(std::declval<Base>().key())
		{
			auto it = --this->base();
			return it.key();
		}
		reference value() const
		{
			auto it = --this->base();
			return it.operator * ();
		}
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <type_traits> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	struct json_default_base {};
	template<class T>
	using json_base_class = typename std::conditional <
		std::is_same<T, void>::value,
		json_default_base,
		T
	>::type;
}
NLOHMANN_JSON_NAMESPACE_END
#include <algorithm> 
#include <cctype> 
#include <cerrno> 
#include <cstdlib> 
#ifndef JSON_NO_IO
#include <iosfwd> 
#endif  
#include <limits> 
#include <numeric> 
#include <string> 
#include <utility> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename RefStringType>
class json_pointer
{
	NLOHMANN_BASIC_JSON_TPL_DECLARATION
		friend class basic_json;
	template<typename>
	friend class json_pointer;
	template<typename T>
	struct string_t_helper
	{
		using type = T;
	};
	NLOHMANN_BASIC_JSON_TPL_DECLARATION
		struct string_t_helper<NLOHMANN_BASIC_JSON_TPL>
	{
		using type = StringType;
	};
public:
	using string_t = typename string_t_helper<RefStringType>::type;
	explicit json_pointer(const string_t& s = "")
		: reference_tokens(split(s))
	{
	}
	string_t to_string() const
	{
		return std::accumulate(reference_tokens.begin(), reference_tokens.end(),
			string_t{},
			[](const string_t& a, const string_t& b)
			{
				return detail::concat(a, '/', detail::escape(b));
			});
	}
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, to_string())
		operator string_t() const
	{
		return to_string();
	}
#ifndef JSON_NO_IO
	friend std::ostream& operator<<(std::ostream& o, const json_pointer& ptr)
	{
		o << ptr.to_string();
		return o;
	}
#endif
	json_pointer& operator/=(const json_pointer& ptr)
	{
		reference_tokens.insert(reference_tokens.end(),
			ptr.reference_tokens.begin(),
			ptr.reference_tokens.end());
		return *this;
	}
	json_pointer& operator/=(string_t token)
	{
		push_back(std::move(token));
		return *this;
	}
	json_pointer& operator/=(std::size_t array_idx)
	{
		return *this /= std::to_string(array_idx);
	}
	friend json_pointer operator/(const json_pointer& lhs,
		const json_pointer& rhs)
	{
		return json_pointer(lhs) /= rhs;
	}
	friend json_pointer operator/(const json_pointer& lhs, string_t token)
	{
		return json_pointer(lhs) /= std::move(token);
	}
	friend json_pointer operator/(const json_pointer& lhs, std::size_t array_idx)
	{
		return json_pointer(lhs) /= array_idx;
	}
	json_pointer parent_pointer() const
	{
		if (empty())
		{
			return *this;
		}
		json_pointer res = *this;
		res.pop_back();
		return res;
	}
	void pop_back()
	{
		if (JSON_HEDLEY_UNLIKELY(empty()))
		{
			JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
		}
		reference_tokens.pop_back();
	}
	const string_t& back() const
	{
		if (JSON_HEDLEY_UNLIKELY(empty()))
		{
			JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
		}
		return reference_tokens.back();
	}
	void push_back(const string_t& token)
	{
		reference_tokens.push_back(token);
	}
	void push_back(string_t&& token)
	{
		reference_tokens.push_back(std::move(token));
	}
	bool empty() const noexcept
	{
		return reference_tokens.empty();
	}
private:
	template<typename BasicJsonType>
	static typename BasicJsonType::size_type array_index(const string_t& s)
	{
		using size_type = typename BasicJsonType::size_type;
		if (JSON_HEDLEY_UNLIKELY(s.size() > 1 && s[0] == '0'))
		{
			JSON_THROW(detail::parse_error::create(106, 0, detail::concat("array index '", s, "' must not begin with '0'"), nullptr));
		}
		if (JSON_HEDLEY_UNLIKELY(s.size() > 1 && !(s[0] >= '1' && s[0] <= '9')))
		{
			JSON_THROW(detail::parse_error::create(109, 0, detail::concat("array index '", s, "' is not a number"), nullptr));
		}
		const char* p = s.c_str();
		char* p_end = nullptr;
		errno = 0;
		const unsigned long long res = std::strtoull(p, &p_end, 10);
		if (p == p_end
			|| errno == ERANGE
			|| JSON_HEDLEY_UNLIKELY(static_cast<std::size_t>(p_end - p) != s.size()))
		{
			JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", s, "'"), nullptr));
		}
		if (res >= static_cast<unsigned long long>((std::numeric_limits<size_type>::max)()))
		{
			JSON_THROW(detail::out_of_range::create(410, detail::concat("array index ", s, " exceeds size_type"), nullptr));
		}
		return static_cast<size_type>(res);
	}
JSON_PRIVATE_UNLESS_TESTED:
	json_pointer top() const
	{
		if (JSON_HEDLEY_UNLIKELY(empty()))
		{
			JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
		}
		json_pointer result = *this;
		result.reference_tokens = { reference_tokens[0] };
		return result;
	}
private:
	template<typename BasicJsonType>
	BasicJsonType& get_and_create(BasicJsonType& j) const
	{
		auto* result = &j;
		for (const auto& reference_token : reference_tokens)
		{
			switch (result->type())
			{
			case detail::value_t::null:
			{
				if (reference_token == "0")
				{
					result = &result->operator[](0);
				}
				else
				{
					result = &result->operator[](reference_token);
				}
				break;
			}
			case detail::value_t::object:
			{
				result = &result->operator[](reference_token);
				break;
			}
			case detail::value_t::array:
			{
				result = &result->operator[](array_index<BasicJsonType>(reference_token));
				break;
			}
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
				JSON_THROW(detail::type_error::create(313, "invalid value to unflatten", &j));
			}
		}
		return *result;
	}
	template<typename BasicJsonType>
	BasicJsonType& get_unchecked(BasicJsonType* ptr) const
	{
		for (const auto& reference_token : reference_tokens)
		{
			if (ptr->is_null())
			{
				const bool nums =
					std::all_of(reference_token.begin(), reference_token.end(),
						[](const unsigned char x)
						{
							return std::isdigit(x);
						});
				*ptr = (nums || reference_token == "-")
					? detail::value_t::array
					: detail::value_t::object;
			}
			switch (ptr->type())
			{
			case detail::value_t::object:
			{
				ptr = &ptr->operator[](reference_token);
				break;
			}
			case detail::value_t::array:
			{
				if (reference_token == "-")
				{
					ptr = &ptr->operator[](ptr->m_data.m_value.array->size());
				}
				else
				{
					ptr = &ptr->operator[](array_index<BasicJsonType>(reference_token));
				}
				break;
			}
			case detail::value_t::null:
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
				JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
			}
		}
		return *ptr;
	}
	template<typename BasicJsonType>
	BasicJsonType& get_checked(BasicJsonType* ptr) const
	{
		for (const auto& reference_token : reference_tokens)
		{
			switch (ptr->type())
			{
			case detail::value_t::object:
			{
				ptr = &ptr->at(reference_token);
				break;
			}
			case detail::value_t::array:
			{
				if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
				{
					JSON_THROW(detail::out_of_range::create(402, detail::concat(
						"array index '-' (", std::to_string(ptr->m_data.m_value.array->size()),
						") is out of range"), ptr));
				}
				ptr = &ptr->at(array_index<BasicJsonType>(reference_token));
				break;
			}
			case detail::value_t::null:
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
				JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
			}
		}
		return *ptr;
	}
	template<typename BasicJsonType>
	const BasicJsonType& get_unchecked(const BasicJsonType* ptr) const
	{
		for (const auto& reference_token : reference_tokens)
		{
			switch (ptr->type())
			{
			case detail::value_t::object:
			{
				ptr = &ptr->operator[](reference_token);
				break;
			}
			case detail::value_t::array:
			{
				if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
				{
					JSON_THROW(detail::out_of_range::create(402, detail::concat("array index '-' (", std::to_string(ptr->m_data.m_value.array->size()), ") is out of range"), ptr));
				}
				ptr = &ptr->operator[](array_index<BasicJsonType>(reference_token));
				break;
			}
			case detail::value_t::null:
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
				JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
			}
		}
		return *ptr;
	}
	template<typename BasicJsonType>
	const BasicJsonType& get_checked(const BasicJsonType* ptr) const
	{
		for (const auto& reference_token : reference_tokens)
		{
			switch (ptr->type())
			{
			case detail::value_t::object:
			{
				ptr = &ptr->at(reference_token);
				break;
			}
			case detail::value_t::array:
			{
				if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
				{
					JSON_THROW(detail::out_of_range::create(402, detail::concat(
						"array index '-' (", std::to_string(ptr->m_data.m_value.array->size()),
						") is out of range"), ptr));
				}
				ptr = &ptr->at(array_index<BasicJsonType>(reference_token));
				break;
			}
			case detail::value_t::null:
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
				JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
			}
		}
		return *ptr;
	}
	template<typename BasicJsonType>
	bool contains(const BasicJsonType* ptr) const
	{
		for (const auto& reference_token : reference_tokens)
		{
			switch (ptr->type())
			{
			case detail::value_t::object:
			{
				if (!ptr->contains(reference_token))
				{
					return false;
				}
				ptr = &ptr->operator[](reference_token);
				break;
			}
			case detail::value_t::array:
			{
				if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
				{
					return false;
				}
				if (JSON_HEDLEY_UNLIKELY(reference_token.size() == 1 && !("0" <= reference_token && reference_token <= "9")))
				{
					return false;
				}
				if (JSON_HEDLEY_UNLIKELY(reference_token.size() > 1))
				{
					if (JSON_HEDLEY_UNLIKELY(!('1' <= reference_token[0] && reference_token[0] <= '9')))
					{
						return false;
					}
					for (std::size_t i = 1; i < reference_token.size(); i++)
					{
						if (JSON_HEDLEY_UNLIKELY(!('0' <= reference_token[i] && reference_token[i] <= '9')))
						{
							return false;
						}
					}
				}
				const auto idx = array_index<BasicJsonType>(reference_token);
				if (idx >= ptr->size())
				{
					return false;
				}
				ptr = &ptr->operator[](idx);
				break;
			}
			case detail::value_t::null:
			case detail::value_t::string:
			case detail::value_t::boolean:
			case detail::value_t::number_integer:
			case detail::value_t::number_unsigned:
			case detail::value_t::number_float:
			case detail::value_t::binary:
			case detail::value_t::discarded:
			default:
			{
				return false;
			}
			}
		}
		return true;
	}
	static std::vector<string_t> split(const string_t& reference_string)
	{
		std::vector<string_t> result;
		if (reference_string.empty())
		{
			return result;
		}
		if (JSON_HEDLEY_UNLIKELY(reference_string[0] != '/'))
		{
			JSON_THROW(detail::parse_error::create(107, 1, detail::concat("JSON pointer must be empty or begin with '/' - was: '", reference_string, "'"), nullptr));
		}
		for (
			std::size_t slash = reference_string.find_first_of('/', 1),
			start = 1;
			start != 0;
			start = (slash == string_t::npos) ? 0 : slash + 1,
			slash = reference_string.find_first_of('/', start))
		{
			auto reference_token = reference_string.substr(start, slash - start);
			for (std::size_t pos = reference_token.find_first_of('~');
				pos != string_t::npos;
				pos = reference_token.find_first_of('~', pos + 1))
			{
				JSON_ASSERT(reference_token[pos] == '~');
				if (JSON_HEDLEY_UNLIKELY(pos == reference_token.size() - 1 ||
					(reference_token[pos + 1] != '0' &&
						reference_token[pos + 1] != '1')))
				{
					JSON_THROW(detail::parse_error::create(108, 0, "escape character '~' must be followed with '0' or '1'", nullptr));
				}
			}
			detail::unescape(reference_token);
			result.push_back(reference_token);
		}
		return result;
	}
private:
	template<typename BasicJsonType>
	static void flatten(const string_t& reference_string,
		const BasicJsonType& value,
		BasicJsonType& result)
	{
		switch (value.type())
		{
		case detail::value_t::array:
		{
			if (value.m_data.m_value.array->empty())
			{
				result[reference_string] = nullptr;
			}
			else
			{
				for (std::size_t i = 0; i < value.m_data.m_value.array->size(); ++i)
				{
					flatten(detail::concat(reference_string, '/', std::to_string(i)),
						value.m_data.m_value.array->operator[](i), result);
				}
			}
			break;
		}
		case detail::value_t::object:
		{
			if (value.m_data.m_value.object->empty())
			{
				result[reference_string] = nullptr;
			}
			else
			{
				for (const auto& element : *value.m_data.m_value.object)
				{
					flatten(detail::concat(reference_string, '/', detail::escape(element.first)), element.second, result);
				}
			}
			break;
		}
		case detail::value_t::null:
		case detail::value_t::string:
		case detail::value_t::boolean:
		case detail::value_t::number_integer:
		case detail::value_t::number_unsigned:
		case detail::value_t::number_float:
		case detail::value_t::binary:
		case detail::value_t::discarded:
		default:
		{
			result[reference_string] = value;
			break;
		}
		}
	}
	template<typename BasicJsonType>
	static BasicJsonType
		unflatten(const BasicJsonType& value)
	{
		if (JSON_HEDLEY_UNLIKELY(!value.is_object()))
		{
			JSON_THROW(detail::type_error::create(314, "only objects can be unflattened", &value));
		}
		BasicJsonType result;
		for (const auto& element : *value.m_data.m_value.object)
		{
			if (JSON_HEDLEY_UNLIKELY(!element.second.is_primitive()))
			{
				JSON_THROW(detail::type_error::create(315, "values in object must be primitive", &element.second));
			}
			json_pointer(element.first).get_and_create(result) = element.second;
		}
		return result;
	}
	json_pointer<string_t> convert() const&
	{
		json_pointer<string_t> result;
		result.reference_tokens = reference_tokens;
		return result;
	}
	json_pointer<string_t> convert()&&
	{
		json_pointer<string_t> result;
		result.reference_tokens = std::move(reference_tokens);
		return result;
	}
public:
#if JSON_HAS_THREE_WAY_COMPARISON
	template<typename RefStringTypeRhs>
	bool operator==(const json_pointer<RefStringTypeRhs>& rhs) const noexcept
	{
		return reference_tokens == rhs.reference_tokens;
	}
	JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer))
		bool operator==(const string_t& rhs) const
	{
		return *this == json_pointer(rhs);
	}
	template<typename RefStringTypeRhs>
	std::strong_ordering operator<=>(const json_pointer<RefStringTypeRhs>& rhs) const noexcept
	{
		return  reference_tokens <=> rhs.reference_tokens;
	}
#else
	template<typename RefStringTypeLhs, typename RefStringTypeRhs>
	friend bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
		const json_pointer<RefStringTypeRhs>& rhs) noexcept;
	template<typename RefStringTypeLhs, typename StringType>
	friend bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
		const StringType& rhs);
	template<typename RefStringTypeRhs, typename StringType>
	friend bool operator==(const StringType& lhs,
		const json_pointer<RefStringTypeRhs>& rhs);
	template<typename RefStringTypeLhs, typename RefStringTypeRhs>
	friend bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
		const json_pointer<RefStringTypeRhs>& rhs) noexcept;
	template<typename RefStringTypeLhs, typename StringType>
	friend bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
		const StringType& rhs);
	template<typename RefStringTypeRhs, typename StringType>
	friend bool operator!=(const StringType& lhs,
		const json_pointer<RefStringTypeRhs>& rhs);
	template<typename RefStringTypeLhs, typename RefStringTypeRhs>
	friend bool operator<(const json_pointer<RefStringTypeLhs>& lhs,
		const json_pointer<RefStringTypeRhs>& rhs) noexcept;
#endif
private:
	std::vector<string_t> reference_tokens;
};
#if !JSON_HAS_THREE_WAY_COMPARISON
template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
	const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
	return lhs.reference_tokens == rhs.reference_tokens;
}
template<typename RefStringTypeLhs,
	typename StringType = typename json_pointer<RefStringTypeLhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer, json_pointer))
inline bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
	const StringType& rhs)
{
	return lhs == json_pointer<RefStringTypeLhs>(rhs);
}
template<typename RefStringTypeRhs,
	typename StringType = typename json_pointer<RefStringTypeRhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer, json_pointer))
inline bool operator==(const StringType& lhs,
	const json_pointer<RefStringTypeRhs>& rhs)
{
	return json_pointer<RefStringTypeRhs>(lhs) == rhs;
}
template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
	const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
	return !(lhs == rhs);
}
template<typename RefStringTypeLhs,
	typename StringType = typename json_pointer<RefStringTypeLhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator!=(json_pointer, json_pointer))
inline bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
	const StringType& rhs)
{
	return !(lhs == rhs);
}
template<typename RefStringTypeRhs,
	typename StringType = typename json_pointer<RefStringTypeRhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator!=(json_pointer, json_pointer))
inline bool operator!=(const StringType& lhs,
	const json_pointer<RefStringTypeRhs>& rhs)
{
	return !(lhs == rhs);
}
template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator<(const json_pointer<RefStringTypeLhs>& lhs,
	const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
	return lhs.reference_tokens < rhs.reference_tokens;
}
#endif
NLOHMANN_JSON_NAMESPACE_END
#include <initializer_list>
#include <utility>
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename BasicJsonType>
	class json_ref
	{
	public:
		using value_type = BasicJsonType;
		json_ref(value_type&& value)
			: owned_value(std::move(value))
		{
		}
		json_ref(const value_type& value)
			: value_ref(&value)
		{
		}
		json_ref(std::initializer_list<json_ref> init)
			: owned_value(init)
		{
		}
		template <
			class... Args,
			enable_if_t<std::is_constructible<value_type, Args...>::value, int> = 0 >
		json_ref(Args && ... args)
			: owned_value(std::forward<Args>(args)...)
		{
		}
		json_ref(json_ref&&) noexcept = default;
		json_ref(const json_ref&) = delete;
		json_ref& operator=(const json_ref&) = delete;
		json_ref& operator=(json_ref&&) = delete;
		~json_ref() = default;
		value_type moved_or_copied() const
		{
			if (value_ref == nullptr)
			{
				return std::move(owned_value);
			}
			return *value_ref;
		}
		value_type const& operator*() const
		{
			return value_ref ? *value_ref : owned_value;
		}
		value_type const* operator->() const
		{
			return &**this;
		}
	private:
		mutable value_type owned_value = nullptr;
		value_type const* value_ref = nullptr;
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <algorithm> 
#include <array> 
#include <map> 
#include <cmath> 
#include <cstdint> 
#include <cstring> 
#include <limits> 
#include <string> 
#include <utility> 
#include <vector> 
#include <algorithm> 
#include <cstddef> 
#include <iterator> 
#include <memory> 
#include <string> 
#include <vector> 
#ifndef JSON_NO_IO
#include <ios>      
#include <ostream>  
#endif  
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename CharType> struct output_adapter_protocol
	{
		virtual void write_character(CharType c) = 0;
		virtual void write_characters(const CharType* s, std::size_t length) = 0;
		virtual ~output_adapter_protocol() = default;
		output_adapter_protocol() = default;
		output_adapter_protocol(const output_adapter_protocol&) = default;
		output_adapter_protocol(output_adapter_protocol&&) noexcept = default;
		output_adapter_protocol& operator=(const output_adapter_protocol&) = default;
		output_adapter_protocol& operator=(output_adapter_protocol&&) noexcept = default;
	};
	template<typename CharType>
	using output_adapter_t = std::shared_ptr<output_adapter_protocol<CharType>>;
	template<typename CharType, typename AllocatorType = std::allocator<CharType>>
	class output_vector_adapter : public output_adapter_protocol<CharType>
	{
	public:
		explicit output_vector_adapter(std::vector<CharType, AllocatorType>& vec) noexcept
			: v(vec)
		{
		}
		void write_character(CharType c) override
		{
			v.push_back(c);
		}
		JSON_HEDLEY_NON_NULL(2)
			void write_characters(const CharType* s, std::size_t length) override
		{
			v.insert(v.end(), s, s + length);
		}
	private:
		std::vector<CharType, AllocatorType>& v;
	};
#ifndef JSON_NO_IO
	template<typename CharType>
	class output_stream_adapter : public output_adapter_protocol<CharType>
	{
	public:
		explicit output_stream_adapter(std::basic_ostream<CharType>& s) noexcept
			: stream(s)
		{
		}
		void write_character(CharType c) override
		{
			stream.put(c);
		}
		JSON_HEDLEY_NON_NULL(2)
			void write_characters(const CharType* s, std::size_t length) override
		{
			stream.write(s, static_cast<std::streamsize>(length));
		}
	private:
		std::basic_ostream<CharType>& stream;
	};
#endif  
	template<typename CharType, typename StringType = std::basic_string<CharType>>
	class output_string_adapter : public output_adapter_protocol<CharType>
	{
	public:
		explicit output_string_adapter(StringType& s) noexcept
			: str(s)
		{
		}
		void write_character(CharType c) override
		{
			str.push_back(c);
		}
		JSON_HEDLEY_NON_NULL(2)
			void write_characters(const CharType* s, std::size_t length) override
		{
			str.append(s, length);
		}
	private:
		StringType& str;
	};
	template<typename CharType, typename StringType = std::basic_string<CharType>>
	class output_adapter
	{
	public:
		template<typename AllocatorType = std::allocator<CharType>>
		output_adapter(std::vector<CharType, AllocatorType>& vec)
			: oa(std::make_shared<output_vector_adapter<CharType, AllocatorType>>(vec)) {
		}
#ifndef JSON_NO_IO
		output_adapter(std::basic_ostream<CharType>& s)
			: oa(std::make_shared<output_stream_adapter<CharType>>(s)) {
		}
#endif  
		output_adapter(StringType& s)
			: oa(std::make_shared<output_string_adapter<CharType, StringType>>(s)) {
		}
		operator output_adapter_t<CharType>()
		{
			return oa;
		}
	private:
		output_adapter_t<CharType> oa = nullptr;
	};
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	template<typename BasicJsonType, typename CharType>
	class binary_writer
	{
		using string_t = typename BasicJsonType::string_t;
		using binary_t = typename BasicJsonType::binary_t;
		using number_float_t = typename BasicJsonType::number_float_t;
	public:
		explicit binary_writer(output_adapter_t<CharType> adapter) : oa(std::move(adapter))
		{
			JSON_ASSERT(oa);
		}
		void write_bson(const BasicJsonType& j)
		{
			switch (j.type())
			{
			case value_t::object:
			{
				write_bson_object(*j.m_data.m_value.object);
				break;
			}
			case value_t::null:
			case value_t::array:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
			{
				JSON_THROW(type_error::create(317, concat("to serialize to BSON, top-level type must be object, but is ", j.type_name()), &j));
			}
			}
		}
		void write_cbor(const BasicJsonType& j)
		{
			switch (j.type())
			{
			case value_t::null:
			{
				oa->write_character(to_char_type(0xF6));
				break;
			}
			case value_t::boolean:
			{
				oa->write_character(j.m_data.m_value.boolean
					? to_char_type(0xF5)
					: to_char_type(0xF4));
				break;
			}
			case value_t::number_integer:
			{
				if (j.m_data.m_value.number_integer >= 0)
				{
					if (j.m_data.m_value.number_integer <= 0x17)
					{
						write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint8_t>::max)())
					{
						oa->write_character(to_char_type(0x18));
						write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint16_t>::max)())
					{
						oa->write_character(to_char_type(0x19));
						write_number(static_cast<std::uint16_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint32_t>::max)())
					{
						oa->write_character(to_char_type(0x1A));
						write_number(static_cast<std::uint32_t>(j.m_data.m_value.number_integer));
					}
					else
					{
						oa->write_character(to_char_type(0x1B));
						write_number(static_cast<std::uint64_t>(j.m_data.m_value.number_integer));
					}
				}
				else
				{
					const auto positive_number = -1 - j.m_data.m_value.number_integer;
					if (j.m_data.m_value.number_integer >= -24)
					{
						write_number(static_cast<std::uint8_t>(0x20 + positive_number));
					}
					else if (positive_number <= (std::numeric_limits<std::uint8_t>::max)())
					{
						oa->write_character(to_char_type(0x38));
						write_number(static_cast<std::uint8_t>(positive_number));
					}
					else if (positive_number <= (std::numeric_limits<std::uint16_t>::max)())
					{
						oa->write_character(to_char_type(0x39));
						write_number(static_cast<std::uint16_t>(positive_number));
					}
					else if (positive_number <= (std::numeric_limits<std::uint32_t>::max)())
					{
						oa->write_character(to_char_type(0x3A));
						write_number(static_cast<std::uint32_t>(positive_number));
					}
					else
					{
						oa->write_character(to_char_type(0x3B));
						write_number(static_cast<std::uint64_t>(positive_number));
					}
				}
				break;
			}
			case value_t::number_unsigned:
			{
				if (j.m_data.m_value.number_unsigned <= 0x17)
				{
					write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_unsigned));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0x18));
					write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_unsigned));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0x19));
					write_number(static_cast<std::uint16_t>(j.m_data.m_value.number_unsigned));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0x1A));
					write_number(static_cast<std::uint32_t>(j.m_data.m_value.number_unsigned));
				}
				else
				{
					oa->write_character(to_char_type(0x1B));
					write_number(static_cast<std::uint64_t>(j.m_data.m_value.number_unsigned));
				}
				break;
			}
			case value_t::number_float:
			{
				if (std::isnan(j.m_data.m_value.number_float))
				{
					oa->write_character(to_char_type(0xF9));
					oa->write_character(to_char_type(0x7E));
					oa->write_character(to_char_type(0x00));
				}
				else if (std::isinf(j.m_data.m_value.number_float))
				{
					oa->write_character(to_char_type(0xf9));
					oa->write_character(j.m_data.m_value.number_float > 0 ? to_char_type(0x7C) : to_char_type(0xFC));
					oa->write_character(to_char_type(0x00));
				}
				else
				{
					write_compact_float(j.m_data.m_value.number_float, detail::input_format_t::cbor);
				}
				break;
			}
			case value_t::string:
			{
				const auto N = j.m_data.m_value.string->size();
				if (N <= 0x17)
				{
					write_number(static_cast<std::uint8_t>(0x60 + N));
				}
				else if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0x78));
					write_number(static_cast<std::uint8_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0x79));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0x7A));
					write_number(static_cast<std::uint32_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint64_t>::max)())
				{
					oa->write_character(to_char_type(0x7B));
					write_number(static_cast<std::uint64_t>(N));
				}
				oa->write_characters(
					reinterpret_cast<const CharType*>(j.m_data.m_value.string->c_str()),
					j.m_data.m_value.string->size());
				break;
			}
			case value_t::array:
			{
				const auto N = j.m_data.m_value.array->size();
				if (N <= 0x17)
				{
					write_number(static_cast<std::uint8_t>(0x80 + N));
				}
				else if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0x98));
					write_number(static_cast<std::uint8_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0x99));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0x9A));
					write_number(static_cast<std::uint32_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint64_t>::max)())
				{
					oa->write_character(to_char_type(0x9B));
					write_number(static_cast<std::uint64_t>(N));
				}
				for (const auto& el : *j.m_data.m_value.array)
				{
					write_cbor(el);
				}
				break;
			}
			case value_t::binary:
			{
				if (j.m_data.m_value.binary->has_subtype())
				{
					if (j.m_data.m_value.binary->subtype() <= (std::numeric_limits<std::uint8_t>::max)())
					{
						write_number(static_cast<std::uint8_t>(0xd8));
						write_number(static_cast<std::uint8_t>(j.m_data.m_value.binary->subtype()));
					}
					else if (j.m_data.m_value.binary->subtype() <= (std::numeric_limits<std::uint16_t>::max)())
					{
						write_number(static_cast<std::uint8_t>(0xd9));
						write_number(static_cast<std::uint16_t>(j.m_data.m_value.binary->subtype()));
					}
					else if (j.m_data.m_value.binary->subtype() <= (std::numeric_limits<std::uint32_t>::max)())
					{
						write_number(static_cast<std::uint8_t>(0xda));
						write_number(static_cast<std::uint32_t>(j.m_data.m_value.binary->subtype()));
					}
					else if (j.m_data.m_value.binary->subtype() <= (std::numeric_limits<std::uint64_t>::max)())
					{
						write_number(static_cast<std::uint8_t>(0xdb));
						write_number(static_cast<std::uint64_t>(j.m_data.m_value.binary->subtype()));
					}
				}
				const auto N = j.m_data.m_value.binary->size();
				if (N <= 0x17)
				{
					write_number(static_cast<std::uint8_t>(0x40 + N));
				}
				else if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0x58));
					write_number(static_cast<std::uint8_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0x59));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0x5A));
					write_number(static_cast<std::uint32_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint64_t>::max)())
				{
					oa->write_character(to_char_type(0x5B));
					write_number(static_cast<std::uint64_t>(N));
				}
				oa->write_characters(
					reinterpret_cast<const CharType*>(j.m_data.m_value.binary->data()),
					N);
				break;
			}
			case value_t::object:
			{
				const auto N = j.m_data.m_value.object->size();
				if (N <= 0x17)
				{
					write_number(static_cast<std::uint8_t>(0xA0 + N));
				}
				else if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0xB8));
					write_number(static_cast<std::uint8_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0xB9));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0xBA));
					write_number(static_cast<std::uint32_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint64_t>::max)())
				{
					oa->write_character(to_char_type(0xBB));
					write_number(static_cast<std::uint64_t>(N));
				}
				for (const auto& el : *j.m_data.m_value.object)
				{
					write_cbor(el.first);
					write_cbor(el.second);
				}
				break;
			}
			case value_t::discarded:
			default:
				break;
			}
		}
		void write_msgpack(const BasicJsonType& j)
		{
			switch (j.type())
			{
			case value_t::null:
			{
				oa->write_character(to_char_type(0xC0));
				break;
			}
			case value_t::boolean:
			{
				oa->write_character(j.m_data.m_value.boolean
					? to_char_type(0xC3)
					: to_char_type(0xC2));
				break;
			}
			case value_t::number_integer:
			{
				if (j.m_data.m_value.number_integer >= 0)
				{
					if (j.m_data.m_value.number_unsigned < 128)
					{
						write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint8_t>::max)())
					{
						oa->write_character(to_char_type(0xCC));
						write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint16_t>::max)())
					{
						oa->write_character(to_char_type(0xCD));
						write_number(static_cast<std::uint16_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint32_t>::max)())
					{
						oa->write_character(to_char_type(0xCE));
						write_number(static_cast<std::uint32_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint64_t>::max)())
					{
						oa->write_character(to_char_type(0xCF));
						write_number(static_cast<std::uint64_t>(j.m_data.m_value.number_integer));
					}
				}
				else
				{
					if (j.m_data.m_value.number_integer >= -32)
					{
						write_number(static_cast<std::int8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer >= (std::numeric_limits<std::int8_t>::min)() &&
						j.m_data.m_value.number_integer <= (std::numeric_limits<std::int8_t>::max)())
					{
						oa->write_character(to_char_type(0xD0));
						write_number(static_cast<std::int8_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer >= (std::numeric_limits<std::int16_t>::min)() &&
						j.m_data.m_value.number_integer <= (std::numeric_limits<std::int16_t>::max)())
					{
						oa->write_character(to_char_type(0xD1));
						write_number(static_cast<std::int16_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer >= (std::numeric_limits<std::int32_t>::min)() &&
						j.m_data.m_value.number_integer <= (std::numeric_limits<std::int32_t>::max)())
					{
						oa->write_character(to_char_type(0xD2));
						write_number(static_cast<std::int32_t>(j.m_data.m_value.number_integer));
					}
					else if (j.m_data.m_value.number_integer >= (std::numeric_limits<std::int64_t>::min)() &&
						j.m_data.m_value.number_integer <= (std::numeric_limits<std::int64_t>::max)())
					{
						oa->write_character(to_char_type(0xD3));
						write_number(static_cast<std::int64_t>(j.m_data.m_value.number_integer));
					}
				}
				break;
			}
			case value_t::number_unsigned:
			{
				if (j.m_data.m_value.number_unsigned < 128)
				{
					write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0xCC));
					write_number(static_cast<std::uint8_t>(j.m_data.m_value.number_integer));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0xCD));
					write_number(static_cast<std::uint16_t>(j.m_data.m_value.number_integer));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0xCE));
					write_number(static_cast<std::uint32_t>(j.m_data.m_value.number_integer));
				}
				else if (j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint64_t>::max)())
				{
					oa->write_character(to_char_type(0xCF));
					write_number(static_cast<std::uint64_t>(j.m_data.m_value.number_integer));
				}
				break;
			}
			case value_t::number_float:
			{
				write_compact_float(j.m_data.m_value.number_float, detail::input_format_t::msgpack);
				break;
			}
			case value_t::string:
			{
				const auto N = j.m_data.m_value.string->size();
				if (N <= 31)
				{
					write_number(static_cast<std::uint8_t>(0xA0 | N));
				}
				else if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					oa->write_character(to_char_type(0xD9));
					write_number(static_cast<std::uint8_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0xDA));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0xDB));
					write_number(static_cast<std::uint32_t>(N));
				}
				oa->write_characters(
					reinterpret_cast<const CharType*>(j.m_data.m_value.string->c_str()),
					j.m_data.m_value.string->size());
				break;
			}
			case value_t::array:
			{
				const auto N = j.m_data.m_value.array->size();
				if (N <= 15)
				{
					write_number(static_cast<std::uint8_t>(0x90 | N));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0xDC));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0xDD));
					write_number(static_cast<std::uint32_t>(N));
				}
				for (const auto& el : *j.m_data.m_value.array)
				{
					write_msgpack(el);
				}
				break;
			}
			case value_t::binary:
			{
				const bool use_ext = j.m_data.m_value.binary->has_subtype();
				const auto N = j.m_data.m_value.binary->size();
				if (N <= (std::numeric_limits<std::uint8_t>::max)())
				{
					std::uint8_t output_type{};
					bool fixed = true;
					if (use_ext)
					{
						switch (N)
						{
						case 1:
							output_type = 0xD4;
							break;
						case 2:
							output_type = 0xD5;
							break;
						case 4:
							output_type = 0xD6;
							break;
						case 8:
							output_type = 0xD7;
							break;
						case 16:
							output_type = 0xD8;
							break;
						default:
							output_type = 0xC7;
							fixed = false;
							break;
						}
					}
					else
					{
						output_type = 0xC4;
						fixed = false;
					}
					oa->write_character(to_char_type(output_type));
					if (!fixed)
					{
						write_number(static_cast<std::uint8_t>(N));
					}
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					const std::uint8_t output_type = use_ext
						? 0xC8
						: 0xC5;
					oa->write_character(to_char_type(output_type));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					const std::uint8_t output_type = use_ext
						? 0xC9
						: 0xC6;
					oa->write_character(to_char_type(output_type));
					write_number(static_cast<std::uint32_t>(N));
				}
				if (use_ext)
				{
					write_number(static_cast<std::int8_t>(j.m_data.m_value.binary->subtype()));
				}
				oa->write_characters(
					reinterpret_cast<const CharType*>(j.m_data.m_value.binary->data()),
					N);
				break;
			}
			case value_t::object:
			{
				const auto N = j.m_data.m_value.object->size();
				if (N <= 15)
				{
					write_number(static_cast<std::uint8_t>(0x80 | (N & 0xF)));
				}
				else if (N <= (std::numeric_limits<std::uint16_t>::max)())
				{
					oa->write_character(to_char_type(0xDE));
					write_number(static_cast<std::uint16_t>(N));
				}
				else if (N <= (std::numeric_limits<std::uint32_t>::max)())
				{
					oa->write_character(to_char_type(0xDF));
					write_number(static_cast<std::uint32_t>(N));
				}
				for (const auto& el : *j.m_data.m_value.object)
				{
					write_msgpack(el.first);
					write_msgpack(el.second);
				}
				break;
			}
			case value_t::discarded:
			default:
				break;
			}
		}
		void write_ubjson(const BasicJsonType& j, const bool use_count,
			const bool use_type, const bool add_prefix = true,
			const bool use_bjdata = false)
		{
			switch (j.type())
			{
			case value_t::null:
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('Z'));
				}
				break;
			}
			case value_t::boolean:
			{
				if (add_prefix)
				{
					oa->write_character(j.m_data.m_value.boolean
						? to_char_type('T')
						: to_char_type('F'));
				}
				break;
			}
			case value_t::number_integer:
			{
				write_number_with_ubjson_prefix(j.m_data.m_value.number_integer, add_prefix, use_bjdata);
				break;
			}
			case value_t::number_unsigned:
			{
				write_number_with_ubjson_prefix(j.m_data.m_value.number_unsigned, add_prefix, use_bjdata);
				break;
			}
			case value_t::number_float:
			{
				write_number_with_ubjson_prefix(j.m_data.m_value.number_float, add_prefix, use_bjdata);
				break;
			}
			case value_t::string:
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('S'));
				}
				write_number_with_ubjson_prefix(j.m_data.m_value.string->size(), true, use_bjdata);
				oa->write_characters(
					reinterpret_cast<const CharType*>(j.m_data.m_value.string->c_str()),
					j.m_data.m_value.string->size());
				break;
			}
			case value_t::array:
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('['));
				}
				bool prefix_required = true;
				if (use_type && !j.m_data.m_value.array->empty())
				{
					JSON_ASSERT(use_count);
					const CharType first_prefix = ubjson_prefix(j.front(), use_bjdata);
					const bool same_prefix = std::all_of(j.begin() + 1, j.end(),
						[this, first_prefix, use_bjdata](const BasicJsonType& v)
						{
							return ubjson_prefix(v, use_bjdata) == first_prefix;
						});
					std::vector<CharType> bjdx = { '[', '{', 'S', 'H', 'T', 'F', 'N', 'Z' };
					if (same_prefix && !(use_bjdata && std::find(bjdx.begin(), bjdx.end(), first_prefix) != bjdx.end()))
					{
						prefix_required = false;
						oa->write_character(to_char_type('$'));
						oa->write_character(first_prefix);
					}
				}
				if (use_count)
				{
					oa->write_character(to_char_type('#'));
					write_number_with_ubjson_prefix(j.m_data.m_value.array->size(), true, use_bjdata);
				}
				for (const auto& el : *j.m_data.m_value.array)
				{
					write_ubjson(el, use_count, use_type, prefix_required, use_bjdata);
				}
				if (!use_count)
				{
					oa->write_character(to_char_type(']'));
				}
				break;
			}
			case value_t::binary:
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('['));
				}
				if (use_type && !j.m_data.m_value.binary->empty())
				{
					JSON_ASSERT(use_count);
					oa->write_character(to_char_type('$'));
					oa->write_character('U');
				}
				if (use_count)
				{
					oa->write_character(to_char_type('#'));
					write_number_with_ubjson_prefix(j.m_data.m_value.binary->size(), true, use_bjdata);
				}
				if (use_type)
				{
					oa->write_characters(
						reinterpret_cast<const CharType*>(j.m_data.m_value.binary->data()),
						j.m_data.m_value.binary->size());
				}
				else
				{
					for (size_t i = 0; i < j.m_data.m_value.binary->size(); ++i)
					{
						oa->write_character(to_char_type('U'));
						oa->write_character(j.m_data.m_value.binary->data()[i]);
					}
				}
				if (!use_count)
				{
					oa->write_character(to_char_type(']'));
				}
				break;
			}
			case value_t::object:
			{
				if (use_bjdata && j.m_data.m_value.object->size() == 3 && j.m_data.m_value.object->find("_ArrayType_") != j.m_data.m_value.object->end() && j.m_data.m_value.object->find("_ArraySize_") != j.m_data.m_value.object->end() && j.m_data.m_value.object->find("_ArrayData_") != j.m_data.m_value.object->end())
				{
					if (!write_bjdata_ndarray(*j.m_data.m_value.object, use_count, use_type))
					{
						break;
					}
				}
				if (add_prefix)
				{
					oa->write_character(to_char_type('{'));
				}
				bool prefix_required = true;
				if (use_type && !j.m_data.m_value.object->empty())
				{
					JSON_ASSERT(use_count);
					const CharType first_prefix = ubjson_prefix(j.front(), use_bjdata);
					const bool same_prefix = std::all_of(j.begin(), j.end(),
						[this, first_prefix, use_bjdata](const BasicJsonType& v)
						{
							return ubjson_prefix(v, use_bjdata) == first_prefix;
						});
					std::vector<CharType> bjdx = { '[', '{', 'S', 'H', 'T', 'F', 'N', 'Z' };
					if (same_prefix && !(use_bjdata && std::find(bjdx.begin(), bjdx.end(), first_prefix) != bjdx.end()))
					{
						prefix_required = false;
						oa->write_character(to_char_type('$'));
						oa->write_character(first_prefix);
					}
				}
				if (use_count)
				{
					oa->write_character(to_char_type('#'));
					write_number_with_ubjson_prefix(j.m_data.m_value.object->size(), true, use_bjdata);
				}
				for (const auto& el : *j.m_data.m_value.object)
				{
					write_number_with_ubjson_prefix(el.first.size(), true, use_bjdata);
					oa->write_characters(
						reinterpret_cast<const CharType*>(el.first.c_str()),
						el.first.size());
					write_ubjson(el.second, use_count, use_type, prefix_required, use_bjdata);
				}
				if (!use_count)
				{
					oa->write_character(to_char_type('}'));
				}
				break;
			}
			case value_t::discarded:
			default:
				break;
			}
		}
	private:
		static std::size_t calc_bson_entry_header_size(const string_t& name, const BasicJsonType& j)
		{
			const auto it = name.find(static_cast<typename string_t::value_type>(0));
			if (JSON_HEDLEY_UNLIKELY(it != BasicJsonType::string_t::npos))
			{
				JSON_THROW(out_of_range::create(409, concat("BSON key cannot contain code point U+0000 (at byte ", std::to_string(it), ")"), &j));
				static_cast<void>(j);
			}
			return  1ul + name.size() + 1u;
		}
		void write_bson_entry_header(const string_t& name,
			const std::uint8_t element_type)
		{
			oa->write_character(to_char_type(element_type));
			oa->write_characters(
				reinterpret_cast<const CharType*>(name.c_str()),
				name.size() + 1u);
		}
		void write_bson_boolean(const string_t& name,
			const bool value)
		{
			write_bson_entry_header(name, 0x08);
			oa->write_character(value ? to_char_type(0x01) : to_char_type(0x00));
		}
		void write_bson_double(const string_t& name,
			const double value)
		{
			write_bson_entry_header(name, 0x01);
			write_number<double>(value, true);
		}
		static std::size_t calc_bson_string_size(const string_t& value)
		{
			return sizeof(std::int32_t) + value.size() + 1ul;
		}
		void write_bson_string(const string_t& name,
			const string_t& value)
		{
			write_bson_entry_header(name, 0x02);
			write_number<std::int32_t>(static_cast<std::int32_t>(value.size() + 1ul), true);
			oa->write_characters(
				reinterpret_cast<const CharType*>(value.c_str()),
				value.size() + 1);
		}
		void write_bson_null(const string_t& name)
		{
			write_bson_entry_header(name, 0x0A);
		}
		static std::size_t calc_bson_integer_size(const std::int64_t value)
		{
			return (std::numeric_limits<std::int32_t>::min)() <= value && value <= (std::numeric_limits<std::int32_t>::max)()
				? sizeof(std::int32_t)
				: sizeof(std::int64_t);
		}
		void write_bson_integer(const string_t& name,
			const std::int64_t value)
		{
			if ((std::numeric_limits<std::int32_t>::min)() <= value && value <= (std::numeric_limits<std::int32_t>::max)())
			{
				write_bson_entry_header(name, 0x10);
				write_number<std::int32_t>(static_cast<std::int32_t>(value), true);
			}
			else
			{
				write_bson_entry_header(name, 0x12);
				write_number<std::int64_t>(static_cast<std::int64_t>(value), true);
			}
		}
		static constexpr std::size_t calc_bson_unsigned_size(const std::uint64_t value) noexcept
		{
			return (value <= static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)()))
				? sizeof(std::int32_t)
				: sizeof(std::int64_t);
		}
		void write_bson_unsigned(const string_t& name,
			const BasicJsonType& j)
		{
			if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)()))
			{
				write_bson_entry_header(name, 0x10);
				write_number<std::int32_t>(static_cast<std::int32_t>(j.m_data.m_value.number_unsigned), true);
			}
			else if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
			{
				write_bson_entry_header(name, 0x12);
				write_number<std::int64_t>(static_cast<std::int64_t>(j.m_data.m_value.number_unsigned), true);
			}
			else
			{
				JSON_THROW(out_of_range::create(407, concat("integer number ", std::to_string(j.m_data.m_value.number_unsigned), " cannot be represented by BSON as it does not fit int64"), &j));
			}
		}
		void write_bson_object_entry(const string_t& name,
			const typename BasicJsonType::object_t& value)
		{
			write_bson_entry_header(name, 0x03);
			write_bson_object(value);
		}
		static std::size_t calc_bson_array_size(const typename BasicJsonType::array_t& value)
		{
			std::size_t array_index = 0ul;
			const std::size_t embedded_document_size = std::accumulate(std::begin(value), std::end(value), static_cast<std::size_t>(0), [&array_index](std::size_t result, const typename BasicJsonType::array_t::value_type& el)
				{
					return result + calc_bson_element_size(std::to_string(array_index++), el);
				});
			return sizeof(std::int32_t) + embedded_document_size + 1ul;
		}
		static std::size_t calc_bson_binary_size(const typename BasicJsonType::binary_t& value)
		{
			return sizeof(std::int32_t) + value.size() + 1ul;
		}
		void write_bson_array(const string_t& name,
			const typename BasicJsonType::array_t& value)
		{
			write_bson_entry_header(name, 0x04);
			write_number<std::int32_t>(static_cast<std::int32_t>(calc_bson_array_size(value)), true);
			std::size_t array_index = 0ul;
			for (const auto& el : value)
			{
				write_bson_element(std::to_string(array_index++), el);
			}
			oa->write_character(to_char_type(0x00));
		}
		void write_bson_binary(const string_t& name,
			const binary_t& value)
		{
			write_bson_entry_header(name, 0x05);
			write_number<std::int32_t>(static_cast<std::int32_t>(value.size()), true);
			write_number(value.has_subtype() ? static_cast<std::uint8_t>(value.subtype()) : static_cast<std::uint8_t>(0x00));
			oa->write_characters(reinterpret_cast<const CharType*>(value.data()), value.size());
		}
		static std::size_t calc_bson_element_size(const string_t& name,
			const BasicJsonType& j)
		{
			const auto header_size = calc_bson_entry_header_size(name, j);
			switch (j.type())
			{
			case value_t::object:
				return header_size + calc_bson_object_size(*j.m_data.m_value.object);
			case value_t::array:
				return header_size + calc_bson_array_size(*j.m_data.m_value.array);
			case value_t::binary:
				return header_size + calc_bson_binary_size(*j.m_data.m_value.binary);
			case value_t::boolean:
				return header_size + 1ul;
			case value_t::number_float:
				return header_size + 8ul;
			case value_t::number_integer:
				return header_size + calc_bson_integer_size(j.m_data.m_value.number_integer);
			case value_t::number_unsigned:
				return header_size + calc_bson_unsigned_size(j.m_data.m_value.number_unsigned);
			case value_t::string:
				return header_size + calc_bson_string_size(*j.m_data.m_value.string);
			case value_t::null:
				return header_size + 0ul;
			case value_t::discarded:
			default:
				JSON_ASSERT(false);
				return 0ul;
			}
		}
		void write_bson_element(const string_t& name,
			const BasicJsonType& j)
		{
			switch (j.type())
			{
			case value_t::object:
				return write_bson_object_entry(name, *j.m_data.m_value.object);
			case value_t::array:
				return write_bson_array(name, *j.m_data.m_value.array);
			case value_t::binary:
				return write_bson_binary(name, *j.m_data.m_value.binary);
			case value_t::boolean:
				return write_bson_boolean(name, j.m_data.m_value.boolean);
			case value_t::number_float:
				return write_bson_double(name, j.m_data.m_value.number_float);
			case value_t::number_integer:
				return write_bson_integer(name, j.m_data.m_value.number_integer);
			case value_t::number_unsigned:
				return write_bson_unsigned(name, j);
			case value_t::string:
				return write_bson_string(name, *j.m_data.m_value.string);
			case value_t::null:
				return write_bson_null(name);
			case value_t::discarded:
			default:
				JSON_ASSERT(false);
				return;
			}
		}
		static std::size_t calc_bson_object_size(const typename BasicJsonType::object_t& value)
		{
			const std::size_t document_size = std::accumulate(value.begin(), value.end(), static_cast<std::size_t>(0),
				[](size_t result, const typename BasicJsonType::object_t::value_type& el)
				{
					return result += calc_bson_element_size(el.first, el.second);
				});
			return sizeof(std::int32_t) + document_size + 1ul;
		}
		void write_bson_object(const typename BasicJsonType::object_t& value)
		{
			write_number<std::int32_t>(static_cast<std::int32_t>(calc_bson_object_size(value)), true);
			for (const auto& el : value)
			{
				write_bson_element(el.first, el.second);
			}
			oa->write_character(to_char_type(0x00));
		}
		static constexpr CharType get_cbor_float_prefix(float)
		{
			return to_char_type(0xFA);
		}
		static constexpr CharType get_cbor_float_prefix(double)
		{
			return to_char_type(0xFB);
		}
		static constexpr CharType get_msgpack_float_prefix(float)
		{
			return to_char_type(0xCA);
		}
		static constexpr CharType get_msgpack_float_prefix(double)
		{
			return to_char_type(0xCB);
		}
		template<typename NumberType, typename std::enable_if<
			std::is_floating_point<NumberType>::value, int>::type = 0>
		void write_number_with_ubjson_prefix(const NumberType n,
			const bool add_prefix,
			const bool use_bjdata)
		{
			if (add_prefix)
			{
				oa->write_character(get_ubjson_float_prefix(n));
			}
			write_number(n, use_bjdata);
		}
		template<typename NumberType, typename std::enable_if<
			std::is_unsigned<NumberType>::value, int>::type = 0>
		void write_number_with_ubjson_prefix(const NumberType n,
			const bool add_prefix,
			const bool use_bjdata)
		{
			if (n <= static_cast<std::uint64_t>((std::numeric_limits<std::int8_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('i'));
				}
				write_number(static_cast<std::uint8_t>(n), use_bjdata);
			}
			else if (n <= (std::numeric_limits<std::uint8_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('U'));
				}
				write_number(static_cast<std::uint8_t>(n), use_bjdata);
			}
			else if (n <= static_cast<std::uint64_t>((std::numeric_limits<std::int16_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('I'));
				}
				write_number(static_cast<std::int16_t>(n), use_bjdata);
			}
			else if (use_bjdata && n <= static_cast<uint64_t>((std::numeric_limits<uint16_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('u'));
				}
				write_number(static_cast<std::uint16_t>(n), use_bjdata);
			}
			else if (n <= static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('l'));
				}
				write_number(static_cast<std::int32_t>(n), use_bjdata);
			}
			else if (use_bjdata && n <= static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('m'));
				}
				write_number(static_cast<std::uint32_t>(n), use_bjdata);
			}
			else if (n <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('L'));
				}
				write_number(static_cast<std::int64_t>(n), use_bjdata);
			}
			else if (use_bjdata && n <= (std::numeric_limits<uint64_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('M'));
				}
				write_number(static_cast<std::uint64_t>(n), use_bjdata);
			}
			else
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('H'));
				}
				const auto number = BasicJsonType(n).dump();
				write_number_with_ubjson_prefix(number.size(), true, use_bjdata);
				for (std::size_t i = 0; i < number.size(); ++i)
				{
					oa->write_character(to_char_type(static_cast<std::uint8_t>(number[i])));
				}
			}
		}
		template < typename NumberType, typename std::enable_if <
			std::is_signed<NumberType>::value &&
			!std::is_floating_point<NumberType>::value, int >::type = 0 >
		void write_number_with_ubjson_prefix(const NumberType n,
			const bool add_prefix,
			const bool use_bjdata)
		{
			if ((std::numeric_limits<std::int8_t>::min)() <= n && n <= (std::numeric_limits<std::int8_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('i'));
				}
				write_number(static_cast<std::int8_t>(n), use_bjdata);
			}
			else if (static_cast<std::int64_t>((std::numeric_limits<std::uint8_t>::min)()) <= n && n <= static_cast<std::int64_t>((std::numeric_limits<std::uint8_t>::max)()))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('U'));
				}
				write_number(static_cast<std::uint8_t>(n), use_bjdata);
			}
			else if ((std::numeric_limits<std::int16_t>::min)() <= n && n <= (std::numeric_limits<std::int16_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('I'));
				}
				write_number(static_cast<std::int16_t>(n), use_bjdata);
			}
			else if (use_bjdata && (static_cast<std::int64_t>((std::numeric_limits<std::uint16_t>::min)()) <= n && n <= static_cast<std::int64_t>((std::numeric_limits<std::uint16_t>::max)())))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('u'));
				}
				write_number(static_cast<uint16_t>(n), use_bjdata);
			}
			else if ((std::numeric_limits<std::int32_t>::min)() <= n && n <= (std::numeric_limits<std::int32_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('l'));
				}
				write_number(static_cast<std::int32_t>(n), use_bjdata);
			}
			else if (use_bjdata && (static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::min)()) <= n && n <= static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::max)())))
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('m'));
				}
				write_number(static_cast<uint32_t>(n), use_bjdata);
			}
			else if ((std::numeric_limits<std::int64_t>::min)() <= n && n <= (std::numeric_limits<std::int64_t>::max)())
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('L'));
				}
				write_number(static_cast<std::int64_t>(n), use_bjdata);
			}
			else
			{
				if (add_prefix)
				{
					oa->write_character(to_char_type('H'));
				}
				const auto number = BasicJsonType(n).dump();
				write_number_with_ubjson_prefix(number.size(), true, use_bjdata);
				for (std::size_t i = 0; i < number.size(); ++i)
				{
					oa->write_character(to_char_type(static_cast<std::uint8_t>(number[i])));
				}
			}
		}
		CharType ubjson_prefix(const BasicJsonType& j, const bool use_bjdata) const noexcept
		{
			switch (j.type())
			{
			case value_t::null:
				return 'Z';
			case value_t::boolean:
				return j.m_data.m_value.boolean ? 'T' : 'F';
			case value_t::number_integer:
			{
				if ((std::numeric_limits<std::int8_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::int8_t>::max)())
				{
					return 'i';
				}
				if ((std::numeric_limits<std::uint8_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint8_t>::max)())
				{
					return 'U';
				}
				if ((std::numeric_limits<std::int16_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::int16_t>::max)())
				{
					return 'I';
				}
				if (use_bjdata && ((std::numeric_limits<std::uint16_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint16_t>::max)()))
				{
					return 'u';
				}
				if ((std::numeric_limits<std::int32_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::int32_t>::max)())
				{
					return 'l';
				}
				if (use_bjdata && ((std::numeric_limits<std::uint32_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::uint32_t>::max)()))
				{
					return 'm';
				}
				if ((std::numeric_limits<std::int64_t>::min)() <= j.m_data.m_value.number_integer && j.m_data.m_value.number_integer <= (std::numeric_limits<std::int64_t>::max)())
				{
					return 'L';
				}
				return 'H';
			}
			case value_t::number_unsigned:
			{
				if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int8_t>::max)()))
				{
					return 'i';
				}
				if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::uint8_t>::max)()))
				{
					return 'U';
				}
				if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int16_t>::max)()))
				{
					return 'I';
				}
				if (use_bjdata && j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::uint16_t>::max)()))
				{
					return 'u';
				}
				if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)()))
				{
					return 'l';
				}
				if (use_bjdata && j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()))
				{
					return 'm';
				}
				if (j.m_data.m_value.number_unsigned <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
				{
					return 'L';
				}
				if (use_bjdata && j.m_data.m_value.number_unsigned <= (std::numeric_limits<std::uint64_t>::max)())
				{
					return 'M';
				}
				return 'H';
			}
			case value_t::number_float:
				return get_ubjson_float_prefix(j.m_data.m_value.number_float);
			case value_t::string:
				return 'S';
			case value_t::array:
			case value_t::binary:
				return '[';
			case value_t::object:
				return '{';
			case value_t::discarded:
			default:
				return 'N';
			}
		}
		static constexpr CharType get_ubjson_float_prefix(float)
		{
			return 'd';
		}
		static constexpr CharType get_ubjson_float_prefix(double)
		{
			return 'D';
		}
		bool write_bjdata_ndarray(const typename BasicJsonType::object_t& value, const bool use_count, const bool use_type)
		{
			std::map<string_t, CharType> bjdtype = { {"uint8", 'U'},  {"int8", 'i'},  {"uint16", 'u'}, {"int16", 'I'},
				{"uint32", 'm'}, {"int32", 'l'}, {"uint64", 'M'}, {"int64", 'L'}, {"single", 'd'}, {"double", 'D'}, {"char", 'C'}
			};
			string_t key = "_ArrayType_";
			auto it = bjdtype.find(static_cast<string_t>(value.at(key)));
			if (it == bjdtype.end())
			{
				return true;
			}
			CharType dtype = it->second;
			key = "_ArraySize_";
			std::size_t len = (value.at(key).empty() ? 0 : 1);
			for (const auto& el : value.at(key))
			{
				len *= static_cast<std::size_t>(el.m_data.m_value.number_unsigned);
			}
			key = "_ArrayData_";
			if (value.at(key).size() != len)
			{
				return true;
			}
			oa->write_character('[');
			oa->write_character('$');
			oa->write_character(dtype);
			oa->write_character('#');
			key = "_ArraySize_";
			write_ubjson(value.at(key), use_count, use_type, true, true);
			key = "_ArrayData_";
			if (dtype == 'U' || dtype == 'C')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::uint8_t>(el.m_data.m_value.number_unsigned), true);
				}
			}
			else if (dtype == 'i')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::int8_t>(el.m_data.m_value.number_integer), true);
				}
			}
			else if (dtype == 'u')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::uint16_t>(el.m_data.m_value.number_unsigned), true);
				}
			}
			else if (dtype == 'I')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::int16_t>(el.m_data.m_value.number_integer), true);
				}
			}
			else if (dtype == 'm')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::uint32_t>(el.m_data.m_value.number_unsigned), true);
				}
			}
			else if (dtype == 'l')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::int32_t>(el.m_data.m_value.number_integer), true);
				}
			}
			else if (dtype == 'M')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::uint64_t>(el.m_data.m_value.number_unsigned), true);
				}
			}
			else if (dtype == 'L')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<std::int64_t>(el.m_data.m_value.number_integer), true);
				}
			}
			else if (dtype == 'd')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<float>(el.m_data.m_value.number_float), true);
				}
			}
			else if (dtype == 'D')
			{
				for (const auto& el : value.at(key))
				{
					write_number(static_cast<double>(el.m_data.m_value.number_float), true);
				}
			}
			return false;
		}
		template<typename NumberType>
		void write_number(const NumberType n, const bool OutputIsLittleEndian = false)
		{
			std::array<CharType, sizeof(NumberType)> vec{};
			std::memcpy(vec.data(), &n, sizeof(NumberType));
			if (is_little_endian != OutputIsLittleEndian)
			{
				std::reverse(vec.begin(), vec.end());
			}
			oa->write_characters(vec.data(), sizeof(NumberType));
		}
		void write_compact_float(const number_float_t n, detail::input_format_t format)
		{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
			if (static_cast<double>(n) >= static_cast<double>(std::numeric_limits<float>::lowest()) &&
				static_cast<double>(n) <= static_cast<double>((std::numeric_limits<float>::max)()) &&
				static_cast<double>(static_cast<float>(n)) == static_cast<double>(n))
			{
				oa->write_character(format == detail::input_format_t::cbor
					? get_cbor_float_prefix(static_cast<float>(n))
					: get_msgpack_float_prefix(static_cast<float>(n)));
				write_number(static_cast<float>(n));
			}
			else
			{
				oa->write_character(format == detail::input_format_t::cbor
					? get_cbor_float_prefix(n)
					: get_msgpack_float_prefix(n));
				write_number(n);
			}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
		}
	public:
		template < typename C = CharType,
			enable_if_t < std::is_signed<C>::value&& std::is_signed<char>::value >* = nullptr >
		static constexpr CharType to_char_type(std::uint8_t x) noexcept
		{
			return *reinterpret_cast<char*>(&x);
		}
		template < typename C = CharType,
			enable_if_t < std::is_signed<C>::value&& std::is_unsigned<char>::value >* = nullptr >
		static CharType to_char_type(std::uint8_t x) noexcept
		{
			static_assert(sizeof(std::uint8_t) == sizeof(CharType), "size of CharType must be equal to std::uint8_t");
			static_assert(std::is_trivial<CharType>::value, "CharType must be trivial");
			CharType result;
			std::memcpy(&result, &x, sizeof(x));
			return result;
		}
		template<typename C = CharType,
			enable_if_t<std::is_unsigned<C>::value>* = nullptr>
		static constexpr CharType to_char_type(std::uint8_t x) noexcept
		{
			return x;
		}
		template < typename InputCharType, typename C = CharType,
			enable_if_t <
			std::is_signed<C>::value&&
			std::is_signed<char>::value&&
			std::is_same<char, typename std::remove_cv<InputCharType>::type>::value
		>* = nullptr >
		static constexpr CharType to_char_type(InputCharType x) noexcept
		{
			return x;
		}
	private:
		const bool is_little_endian = little_endianness();
		output_adapter_t<CharType> oa = nullptr;
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <algorithm> 
#include <array> 
#include <clocale> 
#include <cmath> 
#include <cstddef> 
#include <cstdint> 
#include <cstdio> 
#include <limits> 
#include <string> 
#include <iomanip> 
#include <type_traits> 
#include <utility> 
#include <array> 
#include <cmath>   
#include <cstdint> 
#include <cstring> 
#include <limits> 
#include <type_traits> 
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	namespace dtoa_impl
	{
		template<typename Target, typename Source>
		Target reinterpret_bits(const Source source)
		{
			static_assert(sizeof(Target) == sizeof(Source), "size mismatch");
			Target target;
			std::memcpy(&target, &source, sizeof(Source));
			return target;
		}
		struct diyfp
		{
			static constexpr int kPrecision = 64;
			std::uint64_t f = 0;
			int e = 0;
			constexpr diyfp(std::uint64_t f_, int e_) noexcept : f(f_), e(e_) {}
			static diyfp sub(const diyfp& x, const diyfp& y) noexcept
			{
				JSON_ASSERT(x.e == y.e);
				JSON_ASSERT(x.f >= y.f);
				return { x.f - y.f, x.e };
			}
			static diyfp mul(const diyfp& x, const diyfp& y) noexcept
			{
				static_assert(kPrecision == 64, "internal error");
				const std::uint64_t u_lo = x.f & 0xFFFFFFFFu;
				const std::uint64_t u_hi = x.f >> 32u;
				const std::uint64_t v_lo = y.f & 0xFFFFFFFFu;
				const std::uint64_t v_hi = y.f >> 32u;
				const std::uint64_t p0 = u_lo * v_lo;
				const std::uint64_t p1 = u_lo * v_hi;
				const std::uint64_t p2 = u_hi * v_lo;
				const std::uint64_t p3 = u_hi * v_hi;
				const std::uint64_t p0_hi = p0 >> 32u;
				const std::uint64_t p1_lo = p1 & 0xFFFFFFFFu;
				const std::uint64_t p1_hi = p1 >> 32u;
				const std::uint64_t p2_lo = p2 & 0xFFFFFFFFu;
				const std::uint64_t p2_hi = p2 >> 32u;
				std::uint64_t Q = p0_hi + p1_lo + p2_lo;
				Q += std::uint64_t{ 1 } << (64u - 32u - 1u);
				const std::uint64_t h = p3 + p2_hi + p1_hi + (Q >> 32u);
				return { h, x.e + y.e + 64 };
			}
			static diyfp normalize(diyfp x) noexcept
			{
				JSON_ASSERT(x.f != 0);
				while ((x.f >> 63u) == 0)
				{
					x.f <<= 1u;
					x.e--;
				}
				return x;
			}
			static diyfp normalize_to(const diyfp& x, const int target_exponent) noexcept
			{
				const int delta = x.e - target_exponent;
				JSON_ASSERT(delta >= 0);
				JSON_ASSERT(((x.f << delta) >> delta) == x.f);
				return { x.f << delta, target_exponent };
			}
		};
		struct boundaries
		{
			diyfp w;
			diyfp minus;
			diyfp plus;
		};
		template<typename FloatType>
		boundaries compute_boundaries(FloatType value)
		{
			JSON_ASSERT(std::isfinite(value));
			JSON_ASSERT(value > 0);
			static_assert(std::numeric_limits<FloatType>::is_iec559,
				"internal error: dtoa_short requires an IEEE-754 floating-point implementation");
			constexpr int      kPrecision = std::numeric_limits<FloatType>::digits;
			constexpr int      kBias = std::numeric_limits<FloatType>::max_exponent - 1 + (kPrecision - 1);
			constexpr int      kMinExp = 1 - kBias;
			constexpr std::uint64_t kHiddenBit = std::uint64_t{ 1 } << (kPrecision - 1);
			using bits_type = typename std::conditional<kPrecision == 24, std::uint32_t, std::uint64_t >::type;
			const auto bits = static_cast<std::uint64_t>(reinterpret_bits<bits_type>(value));
			const std::uint64_t E = bits >> (kPrecision - 1);
			const std::uint64_t F = bits & (kHiddenBit - 1);
			const bool is_denormal = E == 0;
			const diyfp v = is_denormal
				? diyfp(F, kMinExp)
				: diyfp(F + kHiddenBit, static_cast<int>(E) - kBias);
			const bool lower_boundary_is_closer = F == 0 && E > 1;
			const diyfp m_plus = diyfp(2 * v.f + 1, v.e - 1);
			const diyfp m_minus = lower_boundary_is_closer
				? diyfp(4 * v.f - 1, v.e - 2)
				: diyfp(2 * v.f - 1, v.e - 1);
			const diyfp w_plus = diyfp::normalize(m_plus);
			const diyfp w_minus = diyfp::normalize_to(m_minus, w_plus.e);
			return { diyfp::normalize(v), w_minus, w_plus };
		}
		constexpr int kAlpha = -60;
		constexpr int kGamma = -32;
		struct cached_power
		{
			std::uint64_t f;
			int e;
			int k;
		};
		inline cached_power get_cached_power_for_binary_exponent(int e)
		{
			constexpr int kCachedPowersMinDecExp = -300;
			constexpr int kCachedPowersDecStep = 8;
			static constexpr std::array<cached_power, 79> kCachedPowers =
			{
				{
					{ 0xAB70FE17C79AC6CA, -1060, -300 },
					{ 0xFF77B1FCBEBCDC4F, -1034, -292 },
					{ 0xBE5691EF416BD60C, -1007, -284 },
					{ 0x8DD01FAD907FFC3C,  -980, -276 },
					{ 0xD3515C2831559A83,  -954, -268 },
					{ 0x9D71AC8FADA6C9B5,  -927, -260 },
					{ 0xEA9C227723EE8BCB,  -901, -252 },
					{ 0xAECC49914078536D,  -874, -244 },
					{ 0x823C12795DB6CE57,  -847, -236 },
					{ 0xC21094364DFB5637,  -821, -228 },
					{ 0x9096EA6F3848984F,  -794, -220 },
					{ 0xD77485CB25823AC7,  -768, -212 },
					{ 0xA086CFCD97BF97F4,  -741, -204 },
					{ 0xEF340A98172AACE5,  -715, -196 },
					{ 0xB23867FB2A35B28E,  -688, -188 },
					{ 0x84C8D4DFD2C63F3B,  -661, -180 },
					{ 0xC5DD44271AD3CDBA,  -635, -172 },
					{ 0x936B9FCEBB25C996,  -608, -164 },
					{ 0xDBAC6C247D62A584,  -582, -156 },
					{ 0xA3AB66580D5FDAF6,  -555, -148 },
					{ 0xF3E2F893DEC3F126,  -529, -140 },
					{ 0xB5B5ADA8AAFF80B8,  -502, -132 },
					{ 0x87625F056C7C4A8B,  -475, -124 },
					{ 0xC9BCFF6034C13053,  -449, -116 },
					{ 0x964E858C91BA2655,  -422, -108 },
					{ 0xDFF9772470297EBD,  -396, -100 },
					{ 0xA6DFBD9FB8E5B88F,  -369,  -92 },
					{ 0xF8A95FCF88747D94,  -343,  -84 },
					{ 0xB94470938FA89BCF,  -316,  -76 },
					{ 0x8A08F0F8BF0F156B,  -289,  -68 },
					{ 0xCDB02555653131B6,  -263,  -60 },
					{ 0x993FE2C6D07B7FAC,  -236,  -52 },
					{ 0xE45C10C42A2B3B06,  -210,  -44 },
					{ 0xAA242499697392D3,  -183,  -36 },
					{ 0xFD87B5F28300CA0E,  -157,  -28 },
					{ 0xBCE5086492111AEB,  -130,  -20 },
					{ 0x8CBCCC096F5088CC,  -103,  -12 },
					{ 0xD1B71758E219652C,   -77,   -4 },
					{ 0x9C40000000000000,   -50,    4 },
					{ 0xE8D4A51000000000,   -24,   12 },
					{ 0xAD78EBC5AC620000,     3,   20 },
					{ 0x813F3978F8940984,    30,   28 },
					{ 0xC097CE7BC90715B3,    56,   36 },
					{ 0x8F7E32CE7BEA5C70,    83,   44 },
					{ 0xD5D238A4ABE98068,   109,   52 },
					{ 0x9F4F2726179A2245,   136,   60 },
					{ 0xED63A231D4C4FB27,   162,   68 },
					{ 0xB0DE65388CC8ADA8,   189,   76 },
					{ 0x83C7088E1AAB65DB,   216,   84 },
					{ 0xC45D1DF942711D9A,   242,   92 },
					{ 0x924D692CA61BE758,   269,  100 },
					{ 0xDA01EE641A708DEA,   295,  108 },
					{ 0xA26DA3999AEF774A,   322,  116 },
					{ 0xF209787BB47D6B85,   348,  124 },
					{ 0xB454E4A179DD1877,   375,  132 },
					{ 0x865B86925B9BC5C2,   402,  140 },
					{ 0xC83553C5C8965D3D,   428,  148 },
					{ 0x952AB45CFA97A0B3,   455,  156 },
					{ 0xDE469FBD99A05FE3,   481,  164 },
					{ 0xA59BC234DB398C25,   508,  172 },
					{ 0xF6C69A72A3989F5C,   534,  180 },
					{ 0xB7DCBF5354E9BECE,   561,  188 },
					{ 0x88FCF317F22241E2,   588,  196 },
					{ 0xCC20CE9BD35C78A5,   614,  204 },
					{ 0x98165AF37B2153DF,   641,  212 },
					{ 0xE2A0B5DC971F303A,   667,  220 },
					{ 0xA8D9D1535CE3B396,   694,  228 },
					{ 0xFB9B7CD9A4A7443C,   720,  236 },
					{ 0xBB764C4CA7A44410,   747,  244 },
					{ 0x8BAB8EEFB6409C1A,   774,  252 },
					{ 0xD01FEF10A657842C,   800,  260 },
					{ 0x9B10A4E5E9913129,   827,  268 },
					{ 0xE7109BFBA19C0C9D,   853,  276 },
					{ 0xAC2820D9623BF429,   880,  284 },
					{ 0x80444B5E7AA7CF85,   907,  292 },
					{ 0xBF21E44003ACDD2D,   933,  300 },
					{ 0x8E679C2F5E44FF8F,   960,  308 },
					{ 0xD433179D9C8CB841,   986,  316 },
					{ 0x9E19DB92B4E31BA9,  1013,  324 },
				}
			};
			JSON_ASSERT(e >= -1500);
			JSON_ASSERT(e <= 1500);
			const int f = kAlpha - e - 1;
			const int k = (f * 78913) / (1 << 18) + static_cast<int>(f > 0);
			const int index = (-kCachedPowersMinDecExp + k + (kCachedPowersDecStep - 1)) / kCachedPowersDecStep;
			JSON_ASSERT(index >= 0);
			JSON_ASSERT(static_cast<std::size_t>(index) < kCachedPowers.size());
			const cached_power cached = kCachedPowers[static_cast<std::size_t>(index)];
			JSON_ASSERT(kAlpha <= cached.e + e + 64);
			JSON_ASSERT(kGamma >= cached.e + e + 64);
			return cached;
		}
		inline int find_largest_pow10(const std::uint32_t n, std::uint32_t& pow10)
		{
			if (n >= 1000000000)
			{
				pow10 = 1000000000;
				return 10;
			}
			if (n >= 100000000)
			{
				pow10 = 100000000;
				return  9;
			}
			if (n >= 10000000)
			{
				pow10 = 10000000;
				return  8;
			}
			if (n >= 1000000)
			{
				pow10 = 1000000;
				return  7;
			}
			if (n >= 100000)
			{
				pow10 = 100000;
				return  6;
			}
			if (n >= 10000)
			{
				pow10 = 10000;
				return  5;
			}
			if (n >= 1000)
			{
				pow10 = 1000;
				return  4;
			}
			if (n >= 100)
			{
				pow10 = 100;
				return  3;
			}
			if (n >= 10)
			{
				pow10 = 10;
				return  2;
			}
			pow10 = 1;
			return 1;
		}
		inline void grisu2_round(char* buf, int len, std::uint64_t dist, std::uint64_t delta,
			std::uint64_t rest, std::uint64_t ten_k)
		{
			JSON_ASSERT(len >= 1);
			JSON_ASSERT(dist <= delta);
			JSON_ASSERT(rest <= delta);
			JSON_ASSERT(ten_k > 0);
			while (rest < dist
				&& delta - rest >= ten_k
				&& (rest + ten_k < dist || dist - rest > rest + ten_k - dist))
			{
				JSON_ASSERT(buf[len - 1] != '0');
				buf[len - 1]--;
				rest += ten_k;
			}
		}
		inline void grisu2_digit_gen(char* buffer, int& length, int& decimal_exponent,
			diyfp M_minus, diyfp w, diyfp M_plus)
		{
			static_assert(kAlpha >= -60, "internal error");
			static_assert(kGamma <= -32, "internal error");
			JSON_ASSERT(M_plus.e >= kAlpha);
			JSON_ASSERT(M_plus.e <= kGamma);
			std::uint64_t delta = diyfp::sub(M_plus, M_minus).f;
			std::uint64_t dist = diyfp::sub(M_plus, w).f;
			const diyfp one(std::uint64_t{ 1 } << -M_plus.e, M_plus.e);
			auto p1 = static_cast<std::uint32_t>(M_plus.f >> -one.e);
			std::uint64_t p2 = M_plus.f & (one.f - 1);
			JSON_ASSERT(p1 > 0);
			std::uint32_t pow10{};
			const int k = find_largest_pow10(p1, pow10);
			int n = k;
			while (n > 0)
			{
				const std::uint32_t d = p1 / pow10;
				const std::uint32_t r = p1 % pow10;
				JSON_ASSERT(d <= 9);
				buffer[length++] = static_cast<char>('0' + d);
				p1 = r;
				n--;
				const std::uint64_t rest = (std::uint64_t{ p1 } << -one.e) + p2;
				if (rest <= delta)
				{
					decimal_exponent += n;
					const std::uint64_t ten_n = std::uint64_t{ pow10 } << -one.e;
					grisu2_round(buffer, length, dist, delta, rest, ten_n);
					return;
				}
				pow10 /= 10;
			}
			JSON_ASSERT(p2 > delta);
			int m = 0;
			for (;;)
			{
				JSON_ASSERT(p2 <= (std::numeric_limits<std::uint64_t>::max)() / 10);
				p2 *= 10;
				const std::uint64_t d = p2 >> -one.e;
				const std::uint64_t r = p2 & (one.f - 1);
				JSON_ASSERT(d <= 9);
				buffer[length++] = static_cast<char>('0' + d);
				p2 = r;
				m++;
				delta *= 10;
				dist *= 10;
				if (p2 <= delta)
				{
					break;
				}
			}
			decimal_exponent -= m;
			const std::uint64_t ten_m = one.f;
			grisu2_round(buffer, length, dist, delta, p2, ten_m);
		}
		JSON_HEDLEY_NON_NULL(1)
			inline void grisu2(char* buf, int& len, int& decimal_exponent,
				diyfp m_minus, diyfp v, diyfp m_plus)
		{
			JSON_ASSERT(m_plus.e == m_minus.e);
			JSON_ASSERT(m_plus.e == v.e);
			const cached_power cached = get_cached_power_for_binary_exponent(m_plus.e);
			const diyfp c_minus_k(cached.f, cached.e);
			const diyfp w = diyfp::mul(v, c_minus_k);
			const diyfp w_minus = diyfp::mul(m_minus, c_minus_k);
			const diyfp w_plus = diyfp::mul(m_plus, c_minus_k);
			const diyfp M_minus(w_minus.f + 1, w_minus.e);
			const diyfp M_plus(w_plus.f - 1, w_plus.e);
			decimal_exponent = -cached.k;
			grisu2_digit_gen(buf, len, decimal_exponent, M_minus, w, M_plus);
		}
		template<typename FloatType>
		JSON_HEDLEY_NON_NULL(1)
			void grisu2(char* buf, int& len, int& decimal_exponent, FloatType value)
		{
			static_assert(diyfp::kPrecision >= std::numeric_limits<FloatType>::digits + 3,
				"internal error: not enough precision");
			JSON_ASSERT(std::isfinite(value));
			JSON_ASSERT(value > 0);
#if 0 
			const boundaries w = compute_boundaries(static_cast<double>(value));
#else
			const boundaries w = compute_boundaries(value);
#endif
			grisu2(buf, len, decimal_exponent, w.minus, w.w, w.plus);
		}
		JSON_HEDLEY_NON_NULL(1)
			JSON_HEDLEY_RETURNS_NON_NULL
			inline char* append_exponent(char* buf, int e)
		{
			JSON_ASSERT(e > -1000);
			JSON_ASSERT(e < 1000);
			if (e < 0)
			{
				e = -e;
				*buf++ = '-';
			}
			else
			{
				*buf++ = '+';
			}
			auto k = static_cast<std::uint32_t>(e);
			if (k < 10)
			{
				*buf++ = '0';
				*buf++ = static_cast<char>('0' + k);
			}
			else if (k < 100)
			{
				*buf++ = static_cast<char>('0' + k / 10);
				k %= 10;
				*buf++ = static_cast<char>('0' + k);
			}
			else
			{
				*buf++ = static_cast<char>('0' + k / 100);
				k %= 100;
				*buf++ = static_cast<char>('0' + k / 10);
				k %= 10;
				*buf++ = static_cast<char>('0' + k);
			}
			return buf;
		}
		JSON_HEDLEY_NON_NULL(1)
			JSON_HEDLEY_RETURNS_NON_NULL
			inline char* format_buffer(char* buf, int len, int decimal_exponent,
				int min_exp, int max_exp)
		{
			JSON_ASSERT(min_exp < 0);
			JSON_ASSERT(max_exp > 0);
			const int k = len;
			const int n = len + decimal_exponent;
			if (k <= n && n <= max_exp)
			{
				std::memset(buf + k, '0', static_cast<size_t>(n) - static_cast<size_t>(k));
				buf[n + 0] = '.';
				buf[n + 1] = '0';
				return buf + (static_cast<size_t>(n) + 2);
			}
			if (0 < n && n <= max_exp)
			{
				JSON_ASSERT(k > n);
				std::memmove(buf + (static_cast<size_t>(n) + 1), buf + n, static_cast<size_t>(k) - static_cast<size_t>(n));
				buf[n] = '.';
				return buf + (static_cast<size_t>(k) + 1U);
			}
			if (min_exp < n && n <= 0)
			{
				std::memmove(buf + (2 + static_cast<size_t>(-n)), buf, static_cast<size_t>(k));
				buf[0] = '0';
				buf[1] = '.';
				std::memset(buf + 2, '0', static_cast<size_t>(-n));
				return buf + (2U + static_cast<size_t>(-n) + static_cast<size_t>(k));
			}
			if (k == 1)
			{
				buf += 1;
			}
			else
			{
				std::memmove(buf + 2, buf + 1, static_cast<size_t>(k) - 1);
				buf[1] = '.';
				buf += 1 + static_cast<size_t>(k);
			}
			*buf++ = 'e';
			return append_exponent(buf, n - 1);
		}
	}
	template<typename FloatType>
	JSON_HEDLEY_NON_NULL(1, 2)
		JSON_HEDLEY_RETURNS_NON_NULL
		char* to_chars(char* first, const char* last, FloatType value)
	{
		static_cast<void>(last);
		JSON_ASSERT(std::isfinite(value));
		if (std::signbit(value))
		{
			value = -value;
			*first++ = '-';
		}
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
		if (value == 0)
		{
			*first++ = '0';
			*first++ = '.';
			*first++ = '0';
			return first;
		}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
		JSON_ASSERT(last - first >= std::numeric_limits<FloatType>::max_digits10);
		int len = 0;
		int decimal_exponent = 0;
		dtoa_impl::grisu2(first, len, decimal_exponent, value);
		JSON_ASSERT(len <= std::numeric_limits<FloatType>::max_digits10);
		constexpr int kMinExp = -4;
		constexpr int kMaxExp = std::numeric_limits<FloatType>::digits10;
		JSON_ASSERT(last - first >= kMaxExp + 2);
		JSON_ASSERT(last - first >= 2 + (-kMinExp - 1) + std::numeric_limits<FloatType>::max_digits10);
		JSON_ASSERT(last - first >= std::numeric_limits<FloatType>::max_digits10 + 6);
		return dtoa_impl::format_buffer(first, len, decimal_exponent, kMinExp, kMaxExp);
	}
}
NLOHMANN_JSON_NAMESPACE_END
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
	enum class error_handler_t
	{
		strict,
		replace,
		ignore
	};
	template<typename BasicJsonType>
	class serializer
	{
		using string_t = typename BasicJsonType::string_t;
		using number_float_t = typename BasicJsonType::number_float_t;
		using number_integer_t = typename BasicJsonType::number_integer_t;
		using number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using binary_char_t = typename BasicJsonType::binary_t::value_type;
		static constexpr std::uint8_t UTF8_ACCEPT = 0;
		static constexpr std::uint8_t UTF8_REJECT = 1;
	public:
		serializer(output_adapter_t<char> s, const char ichar,
			error_handler_t error_handler_ = error_handler_t::strict)
			: o(std::move(s))
			, loc(std::localeconv())
			, thousands_sep(loc->thousands_sep == nullptr ? '\0' : std::char_traits<char>::to_char_type(*(loc->thousands_sep)))
			, decimal_point(loc->decimal_point == nullptr ? '\0' : std::char_traits<char>::to_char_type(*(loc->decimal_point)))
			, indent_char(ichar)
			, indent_string(512, indent_char)
			, error_handler(error_handler_)
		{
		}
		serializer(const serializer&) = delete;
		serializer& operator=(const serializer&) = delete;
		serializer(serializer&&) = delete;
		serializer& operator=(serializer&&) = delete;
		~serializer() = default;
		void dump(const BasicJsonType& val,
			const bool pretty_print,
			const bool ensure_ascii,
			const unsigned int indent_step,
			const unsigned int current_indent = 0)
		{
			switch (val.m_data.m_type)
			{
			case value_t::object:
			{
				if (val.m_data.m_value.object->empty())
				{
					o->write_characters("{}", 2);
					return;
				}
				if (pretty_print)
				{
					o->write_characters("{\n", 2);
					const auto new_indent = current_indent + indent_step;
					if (JSON_HEDLEY_UNLIKELY(indent_string.size() < new_indent))
					{
						indent_string.resize(indent_string.size() * 2, ' ');
					}
					auto i = val.m_data.m_value.object->cbegin();
					for (std::size_t cnt = 0; cnt < val.m_data.m_value.object->size() - 1; ++cnt, ++i)
					{
						o->write_characters(indent_string.c_str(), new_indent);
						o->write_character('\"');
						dump_escaped(i->first, ensure_ascii);
						o->write_characters("\": ", 3);
						dump(i->second, true, ensure_ascii, indent_step, new_indent);
						o->write_characters(",\n", 2);
					}
					JSON_ASSERT(i != val.m_data.m_value.object->cend());
					JSON_ASSERT(std::next(i) == val.m_data.m_value.object->cend());
					o->write_characters(indent_string.c_str(), new_indent);
					o->write_character('\"');
					dump_escaped(i->first, ensure_ascii);
					o->write_characters("\": ", 3);
					dump(i->second, true, ensure_ascii, indent_step, new_indent);
					o->write_character('\n');
					o->write_characters(indent_string.c_str(), current_indent);
					o->write_character('}');
				}
				else
				{
					o->write_character('{');
					auto i = val.m_data.m_value.object->cbegin();
					for (std::size_t cnt = 0; cnt < val.m_data.m_value.object->size() - 1; ++cnt, ++i)
					{
						o->write_character('\"');
						dump_escaped(i->first, ensure_ascii);
						o->write_characters("\":", 2);
						dump(i->second, false, ensure_ascii, indent_step, current_indent);
						o->write_character(',');
					}
					JSON_ASSERT(i != val.m_data.m_value.object->cend());
					JSON_ASSERT(std::next(i) == val.m_data.m_value.object->cend());
					o->write_character('\"');
					dump_escaped(i->first, ensure_ascii);
					o->write_characters("\":", 2);
					dump(i->second, false, ensure_ascii, indent_step, current_indent);
					o->write_character('}');
				}
				return;
			}
			case value_t::array:
			{
				if (val.m_data.m_value.array->empty())
				{
					o->write_characters("[]", 2);
					return;
				}
				if (pretty_print)
				{
					o->write_characters("[\n", 2);
					const auto new_indent = current_indent + indent_step;
					if (JSON_HEDLEY_UNLIKELY(indent_string.size() < new_indent))
					{
						indent_string.resize(indent_string.size() * 2, ' ');
					}
					for (auto i = val.m_data.m_value.array->cbegin();
						i != val.m_data.m_value.array->cend() - 1; ++i)
					{
						o->write_characters(indent_string.c_str(), new_indent);
						dump(*i, true, ensure_ascii, indent_step, new_indent);
						o->write_characters(",\n", 2);
					}
					JSON_ASSERT(!val.m_data.m_value.array->empty());
					o->write_characters(indent_string.c_str(), new_indent);
					dump(val.m_data.m_value.array->back(), true, ensure_ascii, indent_step, new_indent);
					o->write_character('\n');
					o->write_characters(indent_string.c_str(), current_indent);
					o->write_character(']');
				}
				else
				{
					o->write_character('[');
					for (auto i = val.m_data.m_value.array->cbegin();
						i != val.m_data.m_value.array->cend() - 1; ++i)
					{
						dump(*i, false, ensure_ascii, indent_step, current_indent);
						o->write_character(',');
					}
					JSON_ASSERT(!val.m_data.m_value.array->empty());
					dump(val.m_data.m_value.array->back(), false, ensure_ascii, indent_step, current_indent);
					o->write_character(']');
				}
				return;
			}
			case value_t::string:
			{
				o->write_character('\"');
				dump_escaped(*val.m_data.m_value.string, ensure_ascii);
				o->write_character('\"');
				return;
			}
			case value_t::binary:
			{
				if (pretty_print)
				{
					o->write_characters("{\n", 2);
					const auto new_indent = current_indent + indent_step;
					if (JSON_HEDLEY_UNLIKELY(indent_string.size() < new_indent))
					{
						indent_string.resize(indent_string.size() * 2, ' ');
					}
					o->write_characters(indent_string.c_str(), new_indent);
					o->write_characters("\"bytes\": [", 10);
					if (!val.m_data.m_value.binary->empty())
					{
						for (auto i = val.m_data.m_value.binary->cbegin();
							i != val.m_data.m_value.binary->cend() - 1; ++i)
						{
							dump_integer(*i);
							o->write_characters(", ", 2);
						}
						dump_integer(val.m_data.m_value.binary->back());
					}
					o->write_characters("],\n", 3);
					o->write_characters(indent_string.c_str(), new_indent);
					o->write_characters("\"subtype\": ", 11);
					if (val.m_data.m_value.binary->has_subtype())
					{
						dump_integer(val.m_data.m_value.binary->subtype());
					}
					else
					{
						o->write_characters("null", 4);
					}
					o->write_character('\n');
					o->write_characters(indent_string.c_str(), current_indent);
					o->write_character('}');
				}
				else
				{
					o->write_characters("{\"bytes\":[", 10);
					if (!val.m_data.m_value.binary->empty())
					{
						for (auto i = val.m_data.m_value.binary->cbegin();
							i != val.m_data.m_value.binary->cend() - 1; ++i)
						{
							dump_integer(*i);
							o->write_character(',');
						}
						dump_integer(val.m_data.m_value.binary->back());
					}
					o->write_characters("],\"subtype\":", 12);
					if (val.m_data.m_value.binary->has_subtype())
					{
						dump_integer(val.m_data.m_value.binary->subtype());
						o->write_character('}');
					}
					else
					{
						o->write_characters("null}", 5);
					}
				}
				return;
			}
			case value_t::boolean:
			{
				if (val.m_data.m_value.boolean)
				{
					o->write_characters("true", 4);
				}
				else
				{
					o->write_characters("false", 5);
				}
				return;
			}
			case value_t::number_integer:
			{
				dump_integer(val.m_data.m_value.number_integer);
				return;
			}
			case value_t::number_unsigned:
			{
				dump_integer(val.m_data.m_value.number_unsigned);
				return;
			}
			case value_t::number_float:
			{
				dump_float(val.m_data.m_value.number_float);
				return;
			}
			case value_t::discarded:
			{
				o->write_characters("<discarded>", 11);
				return;
			}
			case value_t::null:
			{
				o->write_characters("null", 4);
				return;
			}
			default:
				JSON_ASSERT(false);
			}
		}
	JSON_PRIVATE_UNLESS_TESTED:
		void dump_escaped(const string_t& s, const bool ensure_ascii)
		{
			std::uint32_t codepoint{};
			std::uint8_t state = UTF8_ACCEPT;
			std::size_t bytes = 0;
			std::size_t bytes_after_last_accept = 0;
			std::size_t undumped_chars = 0;
			for (std::size_t i = 0; i < s.size(); ++i)
			{
				const auto byte = static_cast<std::uint8_t>(s[i]);
				switch (decode(state, codepoint, byte))
				{
				case UTF8_ACCEPT:
				{
					switch (codepoint)
					{
					case 0x08:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = 'b';
						break;
					}
					case 0x09:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = 't';
						break;
					}
					case 0x0A:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = 'n';
						break;
					}
					case 0x0C:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = 'f';
						break;
					}
					case 0x0D:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = 'r';
						break;
					}
					case 0x22:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = '\"';
						break;
					}
					case 0x5C:
					{
						string_buffer[bytes++] = '\\';
						string_buffer[bytes++] = '\\';
						break;
					}
					default:
					{
						if ((codepoint <= 0x1F) || (ensure_ascii && (codepoint >= 0x7F)))
						{
							if (codepoint <= 0xFFFF)
							{
								static_cast<void>((std::snprintf)(string_buffer.data() + bytes, 7, "\\u%04x",
									static_cast<std::uint16_t>(codepoint)));
								bytes += 6;
							}
							else
							{
								static_cast<void>((std::snprintf)(string_buffer.data() + bytes, 13, "\\u%04x\\u%04x",
									static_cast<std::uint16_t>(0xD7C0u + (codepoint >> 10u)),
									static_cast<std::uint16_t>(0xDC00u + (codepoint & 0x3FFu))));
								bytes += 12;
							}
						}
						else
						{
							string_buffer[bytes++] = s[i];
						}
						break;
					}
					}
					if (string_buffer.size() - bytes < 13)
					{
						o->write_characters(string_buffer.data(), bytes);
						bytes = 0;
					}
					bytes_after_last_accept = bytes;
					undumped_chars = 0;
					break;
				}
				case UTF8_REJECT:
				{
					switch (error_handler)
					{
					case error_handler_t::strict:
					{
						JSON_THROW(type_error::create(316, concat("invalid UTF-8 byte at index ", std::to_string(i), ": 0x", hex_bytes(byte | 0)), nullptr));
					}
					case error_handler_t::ignore:
					case error_handler_t::replace:
					{
						if (undumped_chars > 0)
						{
							--i;
						}
						bytes = bytes_after_last_accept;
						if (error_handler == error_handler_t::replace)
						{
							if (ensure_ascii)
							{
								string_buffer[bytes++] = '\\';
								string_buffer[bytes++] = 'u';
								string_buffer[bytes++] = 'f';
								string_buffer[bytes++] = 'f';
								string_buffer[bytes++] = 'f';
								string_buffer[bytes++] = 'd';
							}
							else
							{
								string_buffer[bytes++] = detail::binary_writer<BasicJsonType, char>::to_char_type('\xEF');
								string_buffer[bytes++] = detail::binary_writer<BasicJsonType, char>::to_char_type('\xBF');
								string_buffer[bytes++] = detail::binary_writer<BasicJsonType, char>::to_char_type('\xBD');
							}
							if (string_buffer.size() - bytes < 13)
							{
								o->write_characters(string_buffer.data(), bytes);
								bytes = 0;
							}
							bytes_after_last_accept = bytes;
						}
						undumped_chars = 0;
						state = UTF8_ACCEPT;
						break;
					}
					default:
						JSON_ASSERT(false);
					}
					break;
				}
				default:
				{
					if (!ensure_ascii)
					{
						string_buffer[bytes++] = s[i];
					}
					++undumped_chars;
					break;
				}
				}
			}
			if (JSON_HEDLEY_LIKELY(state == UTF8_ACCEPT))
			{
				if (bytes > 0)
				{
					o->write_characters(string_buffer.data(), bytes);
				}
			}
			else
			{
				switch (error_handler)
				{
				case error_handler_t::strict:
				{
					JSON_THROW(type_error::create(316, concat("incomplete UTF-8 string; last byte: 0x", hex_bytes(static_cast<std::uint8_t>(s.back() | 0))), nullptr));
				}
				case error_handler_t::ignore:
				{
					o->write_characters(string_buffer.data(), bytes_after_last_accept);
					break;
				}
				case error_handler_t::replace:
				{
					o->write_characters(string_buffer.data(), bytes_after_last_accept);
					if (ensure_ascii)
					{
						o->write_characters("\\ufffd", 6);
					}
					else
					{
						o->write_characters("\xEF\xBF\xBD", 3);
					}
					break;
				}
				default:
					JSON_ASSERT(false);
				}
			}
		}
	private:
		inline unsigned int count_digits(number_unsigned_t x) noexcept
		{
			unsigned int n_digits = 1;
			for (;;)
			{
				if (x < 10)
				{
					return n_digits;
				}
				if (x < 100)
				{
					return n_digits + 1;
				}
				if (x < 1000)
				{
					return n_digits + 2;
				}
				if (x < 10000)
				{
					return n_digits + 3;
				}
				x = x / 10000u;
				n_digits += 4;
			}
		}
		static std::string hex_bytes(std::uint8_t byte)
		{
			std::string result = "FF";
			constexpr const char* nibble_to_hex = "0123456789ABCDEF";
			result[0] = nibble_to_hex[byte / 16];
			result[1] = nibble_to_hex[byte % 16];
			return result;
		}
		template <typename NumberType, enable_if_t<std::is_signed<NumberType>::value, int> = 0>
		bool is_negative_number(NumberType x)
		{
			return x < 0;
		}
		template < typename NumberType, enable_if_t <std::is_unsigned<NumberType>::value, int > = 0 >
		bool is_negative_number(NumberType)
		{
			return false;
		}
		template < typename NumberType, detail::enable_if_t <
			std::is_integral<NumberType>::value ||
			std::is_same<NumberType, number_unsigned_t>::value ||
			std::is_same<NumberType, number_integer_t>::value ||
			std::is_same<NumberType, binary_char_t>::value,
			int > = 0 >
		void dump_integer(NumberType x)
		{
			static constexpr std::array<std::array<char, 2>, 100> digits_to_99
			{
				{
					{{'0', '0'}}, {{'0', '1'}}, {{'0', '2'}}, {{'0', '3'}}, {{'0', '4'}}, {{'0', '5'}}, {{'0', '6'}}, {{'0', '7'}}, {{'0', '8'}}, {{'0', '9'}},
					{{'1', '0'}}, {{'1', '1'}}, {{'1', '2'}}, {{'1', '3'}}, {{'1', '4'}}, {{'1', '5'}}, {{'1', '6'}}, {{'1', '7'}}, {{'1', '8'}}, {{'1', '9'}},
					{{'2', '0'}}, {{'2', '1'}}, {{'2', '2'}}, {{'2', '3'}}, {{'2', '4'}}, {{'2', '5'}}, {{'2', '6'}}, {{'2', '7'}}, {{'2', '8'}}, {{'2', '9'}},
					{{'3', '0'}}, {{'3', '1'}}, {{'3', '2'}}, {{'3', '3'}}, {{'3', '4'}}, {{'3', '5'}}, {{'3', '6'}}, {{'3', '7'}}, {{'3', '8'}}, {{'3', '9'}},
					{{'4', '0'}}, {{'4', '1'}}, {{'4', '2'}}, {{'4', '3'}}, {{'4', '4'}}, {{'4', '5'}}, {{'4', '6'}}, {{'4', '7'}}, {{'4', '8'}}, {{'4', '9'}},
					{{'5', '0'}}, {{'5', '1'}}, {{'5', '2'}}, {{'5', '3'}}, {{'5', '4'}}, {{'5', '5'}}, {{'5', '6'}}, {{'5', '7'}}, {{'5', '8'}}, {{'5', '9'}},
					{{'6', '0'}}, {{'6', '1'}}, {{'6', '2'}}, {{'6', '3'}}, {{'6', '4'}}, {{'6', '5'}}, {{'6', '6'}}, {{'6', '7'}}, {{'6', '8'}}, {{'6', '9'}},
					{{'7', '0'}}, {{'7', '1'}}, {{'7', '2'}}, {{'7', '3'}}, {{'7', '4'}}, {{'7', '5'}}, {{'7', '6'}}, {{'7', '7'}}, {{'7', '8'}}, {{'7', '9'}},
					{{'8', '0'}}, {{'8', '1'}}, {{'8', '2'}}, {{'8', '3'}}, {{'8', '4'}}, {{'8', '5'}}, {{'8', '6'}}, {{'8', '7'}}, {{'8', '8'}}, {{'8', '9'}},
					{{'9', '0'}}, {{'9', '1'}}, {{'9', '2'}}, {{'9', '3'}}, {{'9', '4'}}, {{'9', '5'}}, {{'9', '6'}}, {{'9', '7'}}, {{'9', '8'}}, {{'9', '9'}},
				}
			};
			if (x == 0)
			{
				o->write_character('0');
				return;
			}
			auto buffer_ptr = number_buffer.begin();
			number_unsigned_t abs_value;
			unsigned int n_chars{};
			if (is_negative_number(x))
			{
				*buffer_ptr = '-';
				abs_value = remove_sign(static_cast<number_integer_t>(x));
				n_chars = 1 + count_digits(abs_value);
			}
			else
			{
				abs_value = static_cast<number_unsigned_t>(x);
				n_chars = count_digits(abs_value);
			}
			JSON_ASSERT(n_chars < number_buffer.size() - 1);
			buffer_ptr += n_chars;
			while (abs_value >= 100)
			{
				const auto digits_index = static_cast<unsigned>((abs_value % 100));
				abs_value /= 100;
				*(--buffer_ptr) = digits_to_99[digits_index][1];
				*(--buffer_ptr) = digits_to_99[digits_index][0];
			}
			if (abs_value >= 10)
			{
				const auto digits_index = static_cast<unsigned>(abs_value);
				*(--buffer_ptr) = digits_to_99[digits_index][1];
				*(--buffer_ptr) = digits_to_99[digits_index][0];
			}
			else
			{
				*(--buffer_ptr) = static_cast<char>('0' + abs_value);
			}
			o->write_characters(number_buffer.data(), n_chars);
		}
		void dump_float(number_float_t x)
		{
			if (!std::isfinite(x))
			{
				o->write_characters("null", 4);
				return;
			}
			static constexpr bool is_ieee_single_or_double
				= (std::numeric_limits<number_float_t>::is_iec559 && std::numeric_limits<number_float_t>::digits == 24 && std::numeric_limits<number_float_t>::max_exponent == 128) ||
				(std::numeric_limits<number_float_t>::is_iec559 && std::numeric_limits<number_float_t>::digits == 53 && std::numeric_limits<number_float_t>::max_exponent == 1024);
			dump_float(x, std::integral_constant<bool, is_ieee_single_or_double>());
		}
		void dump_float(number_float_t x, std::true_type)
		{
			auto* begin = number_buffer.data();
			auto* end = ::JsonLib::detail::to_chars(begin, begin + number_buffer.size(), x);
			o->write_characters(begin, static_cast<size_t>(end - begin));
		}
		void dump_float(number_float_t x, std::false_type)
		{
			static constexpr auto d = std::numeric_limits<number_float_t>::max_digits10;
			std::ptrdiff_t len = (std::snprintf)(number_buffer.data(), number_buffer.size(), "%.*g", d, x);
			JSON_ASSERT(len > 0);
			JSON_ASSERT(static_cast<std::size_t>(len) < number_buffer.size());
			if (thousands_sep != '\0')
			{
				const auto end = std::remove(number_buffer.begin(), number_buffer.begin() + len, thousands_sep);
				std::fill(end, number_buffer.end(), '\0');
				JSON_ASSERT((end - number_buffer.begin()) <= len);
				len = (end - number_buffer.begin());
			}
			if (decimal_point != '\0' && decimal_point != '.')
			{
				const auto dec_pos = std::find(number_buffer.begin(), number_buffer.end(), decimal_point);
				if (dec_pos != number_buffer.end())
				{
					*dec_pos = '.';
				}
			}
			o->write_characters(number_buffer.data(), static_cast<std::size_t>(len));
			const bool value_is_int_like =
				std::none_of(number_buffer.begin(), number_buffer.begin() + len + 1,
					[](char c)
					{
						return c == '.' || c == 'e';
					});
			if (value_is_int_like)
			{
				o->write_characters(".0", 2);
			}
		}
		static std::uint8_t decode(std::uint8_t& state, std::uint32_t& codep, const std::uint8_t byte) noexcept
		{
			static const std::array<std::uint8_t, 400> utf8d =
			{
				{
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
					7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
					8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
					0xA, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,
					0xB, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
					0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,
					1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,
					1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,
					1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1,
					1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
				}
			};
			JSON_ASSERT(byte < utf8d.size());
			const std::uint8_t type = utf8d[byte];
			codep = (state != UTF8_ACCEPT)
				? (byte & 0x3fu) | (codep << 6u)
				: (0xFFu >> type) & (byte);
			const std::size_t index = 256u + static_cast<size_t>(state) * 16u + static_cast<size_t>(type);
			JSON_ASSERT(index < utf8d.size());
			state = utf8d[index];
			return state;
		}
		number_unsigned_t remove_sign(number_unsigned_t x)
		{
			JSON_ASSERT(false);
			return x;
		}
		inline number_unsigned_t remove_sign(number_integer_t x) noexcept
		{
			JSON_ASSERT(x < 0 && x < (std::numeric_limits<number_integer_t>::max)());
			return static_cast<number_unsigned_t>(-(x + 1)) + 1;
		}
	private:
		output_adapter_t<char> o = nullptr;
		std::array<char, 64> number_buffer{ {} };
		const std::lconv* loc = nullptr;
		const char thousands_sep = '\0';
		const char decimal_point = '\0';
		std::array<char, 512> string_buffer{ {} };
		const char indent_char;
		string_t indent_string;
		const error_handler_t error_handler;
	};
}
NLOHMANN_JSON_NAMESPACE_END
#include <functional> 
#include <initializer_list> 
#include <iterator> 
#include <memory> 
#include <stdexcept> 
#include <type_traits> 
#include <utility> 
#include <vector> 
NLOHMANN_JSON_NAMESPACE_BEGIN
template <class Key, class T, class IgnoredLess = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, T>>>
struct ordered_map : std::vector<std::pair<const Key, T>, Allocator>
{
	using key_type = Key;
	using mapped_type = T;
	using Container = std::vector<std::pair<const Key, T>, Allocator>;
	using iterator = typename Container::iterator;
	using const_iterator = typename Container::const_iterator;
	using size_type = typename Container::size_type;
	using value_type = typename Container::value_type;
#ifdef JSON_HAS_CPP_14
	using key_compare = std::equal_to<>;
#else
	using key_compare = std::equal_to<Key>;
#endif
	ordered_map() noexcept(noexcept(Container())) : Container{} {}
	explicit ordered_map(const Allocator& alloc) noexcept(noexcept(Container(alloc))) : Container{ alloc } {}
	template <class It>
	ordered_map(It first, It last, const Allocator& alloc = Allocator())
		: Container{ first, last, alloc } {
	}
	ordered_map(std::initializer_list<value_type> init, const Allocator& alloc = Allocator())
		: Container{ init, alloc } {
	}
	std::pair<iterator, bool> emplace(const key_type& key, T&& t)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return { it, false };
			}
		}
		Container::emplace_back(key, std::forward<T>(t));
		return { std::prev(this->end()), true };
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	std::pair<iterator, bool> emplace(KeyType&& key, T&& t)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return { it, false };
			}
		}
		Container::emplace_back(std::forward<KeyType>(key), std::forward<T>(t));
		return { std::prev(this->end()), true };
	}
	T& operator[](const key_type& key)
	{
		return emplace(key, T{}).first->second;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	T& operator[](KeyType&& key)
	{
		return emplace(std::forward<KeyType>(key), T{}).first->second;
	}
	const T& operator[](const key_type& key) const
	{
		return at(key);
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	const T& operator[](KeyType&& key) const
	{
		return at(std::forward<KeyType>(key));
	}
	T& at(const key_type& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it->second;
			}
		}
		JSON_THROW(std::out_of_range("key not found"));
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	T& at(KeyType&& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it->second;
			}
		}
		JSON_THROW(std::out_of_range("key not found"));
	}
	const T& at(const key_type& key) const
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it->second;
			}
		}
		JSON_THROW(std::out_of_range("key not found"));
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	const T& at(KeyType&& key) const
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it->second;
			}
		}
		JSON_THROW(std::out_of_range("key not found"));
	}
	size_type erase(const key_type& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				for (auto next = it; ++next != this->end(); ++it)
				{
					it->~value_type();
					new (&*it) value_type{ std::move(*next) };
				}
				Container::pop_back();
				return 1;
			}
		}
		return 0;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	size_type erase(KeyType&& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				for (auto next = it; ++next != this->end(); ++it)
				{
					it->~value_type();
					new (&*it) value_type{ std::move(*next) };
				}
				Container::pop_back();
				return 1;
			}
		}
		return 0;
	}
	iterator erase(iterator pos)
	{
		return erase(pos, std::next(pos));
	}
	iterator erase(iterator first, iterator last)
	{
		if (first == last)
		{
			return first;
		}
		const auto elements_affected = std::distance(first, last);
		const auto offset = std::distance(Container::begin(), first);
		for (auto it = first; std::next(it, elements_affected) != Container::end(); ++it)
		{
			it->~value_type();
			new (&*it) value_type{ std::move(*std::next(it, elements_affected)) };
		}
		Container::resize(this->size() - static_cast<size_type>(elements_affected));
		return Container::begin() + offset;
	}
	size_type count(const key_type& key) const
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return 1;
			}
		}
		return 0;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	size_type count(KeyType&& key) const
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return 1;
			}
		}
		return 0;
	}
	iterator find(const key_type& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it;
			}
		}
		return Container::end();
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_key_type<key_compare, key_type, KeyType>::value, int> = 0>
	iterator find(KeyType&& key)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it;
			}
		}
		return Container::end();
	}
	const_iterator find(const key_type& key) const
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, key))
			{
				return it;
			}
		}
		return Container::end();
	}
	std::pair<iterator, bool> insert(value_type&& value)
	{
		return emplace(value.first, std::move(value.second));
	}
	std::pair<iterator, bool> insert(const value_type& value)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
		{
			if (m_compare(it->first, value.first))
			{
				return { it, false };
			}
		}
		Container::push_back(value);
		return { --this->end(), true };
	}
	template<typename InputIt>
	using require_input_iter = typename std::enable_if<std::is_convertible<typename std::iterator_traits<InputIt>::iterator_category,
		std::input_iterator_tag>::value>::type;
	template<typename InputIt, typename = require_input_iter<InputIt>>
	void insert(InputIt first, InputIt last)
	{
		for (auto it = first; it != last; ++it)
		{
			insert(*it);
		}
	}
private:
	JSON_NO_UNIQUE_ADDRESS key_compare m_compare = key_compare();
};
NLOHMANN_JSON_NAMESPACE_END
#if defined(JSON_HAS_CPP_17)
#if JSON_HAS_STATIC_RTTI
#include <any>
#endif
#include <string_view>
#endif
NLOHMANN_JSON_NAMESPACE_BEGIN
NLOHMANN_BASIC_JSON_TPL_DECLARATION
class basic_json
	: public ::JsonLib::detail::json_base_class<CustomBaseClass>
{
private:
	template<detail::value_t> friend struct detail::external_constructor;
	template<typename>
	friend class ::JsonLib::json_pointer;
	template<typename BasicJsonType, typename InputType>
	friend class ::JsonLib::detail::parser;
	friend ::JsonLib::detail::serializer<basic_json>;
	template<typename BasicJsonType>
	friend class ::JsonLib::detail::iter_impl;
	template<typename BasicJsonType, typename CharType>
	friend class ::JsonLib::detail::binary_writer;
	template<typename BasicJsonType, typename InputType, typename SAX>
	friend class ::JsonLib::detail::binary_reader;
	template<typename BasicJsonType>
	friend class ::JsonLib::detail::json_sax_dom_parser;
	template<typename BasicJsonType>
	friend class ::JsonLib::detail::json_sax_dom_callback_parser;
	friend class ::JsonLib::detail::exception;
	using basic_json_t = NLOHMANN_BASIC_JSON_TPL;
	using json_base_class_t = ::JsonLib::detail::json_base_class<CustomBaseClass>;
JSON_PRIVATE_UNLESS_TESTED:
	using lexer = ::JsonLib::detail::lexer_base<basic_json>;
	template<typename InputAdapterType>
	static ::JsonLib::detail::parser<basic_json, InputAdapterType> parser(
		InputAdapterType adapter,
		detail::parser_callback_t<basic_json>cb = nullptr,
		const bool allow_exceptions = true,
		const bool ignore_comments = false
	)
	{
		return ::JsonLib::detail::parser<basic_json, InputAdapterType>(std::move(adapter),
			std::move(cb), allow_exceptions, ignore_comments);
	}
private:
	using primitive_iterator_t = ::JsonLib::detail::primitive_iterator_t;
	template<typename BasicJsonType>
	using internal_iterator = ::JsonLib::detail::internal_iterator<BasicJsonType>;
	template<typename BasicJsonType>
	using iter_impl = ::JsonLib::detail::iter_impl<BasicJsonType>;
	template<typename Iterator>
	using iteration_proxy = ::JsonLib::detail::iteration_proxy<Iterator>;
	template<typename Base> using json_reverse_iterator = ::JsonLib::detail::json_reverse_iterator<Base>;
	template<typename CharType>
	using output_adapter_t = ::JsonLib::detail::output_adapter_t<CharType>;
	template<typename InputType>
	using binary_reader = ::JsonLib::detail::binary_reader<basic_json, InputType>;
	template<typename CharType> using binary_writer = ::JsonLib::detail::binary_writer<basic_json, CharType>;
JSON_PRIVATE_UNLESS_TESTED:
	using serializer = ::JsonLib::detail::serializer<basic_json>;
public:
	using value_t = detail::value_t;
	using json_pointer = ::JsonLib::json_pointer<StringType>;
	template<typename T, typename SFINAE>
	using json_serializer = JSONSerializer<T, SFINAE>;
	using error_handler_t = detail::error_handler_t;
	using cbor_tag_handler_t = detail::cbor_tag_handler_t;
	using initializer_list_t = std::initializer_list<detail::json_ref<basic_json>>;
	using input_format_t = detail::input_format_t;
	using json_sax_t = json_sax<basic_json>;
	using exception = detail::exception;
	using parse_error = detail::parse_error;
	using invalid_iterator = detail::invalid_iterator;
	using type_error = detail::type_error;
	using out_of_range = detail::out_of_range;
	using other_error = detail::other_error;
	using value_type = basic_json;
	using reference = value_type&;
	using const_reference = const value_type&;
	using difference_type = std::ptrdiff_t;
	using size_type = std::size_t;
	using allocator_type = AllocatorType<basic_json>;
	using pointer = typename std::allocator_traits<allocator_type>::pointer;
	using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
	using iterator = iter_impl<basic_json>;
	using const_iterator = iter_impl<const basic_json>;
	using reverse_iterator = json_reverse_iterator<typename basic_json::iterator>;
	using const_reverse_iterator = json_reverse_iterator<typename basic_json::const_iterator>;
	static allocator_type get_allocator()
	{
		return allocator_type();
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json meta()
	{
		basic_json result;
		result["copyright"] = "(C) 2013-2023 Niels Lohmann";
		result["name"] = "JSON for Modern C++";
		result["url"] = "https://github.com/nlohmann/json";
		result["version"]["string"] =
			detail::concat(std::to_string(NLOHMANN_JSON_VERSION_MAJOR), '.',
				std::to_string(NLOHMANN_JSON_VERSION_MINOR), '.',
				std::to_string(NLOHMANN_JSON_VERSION_PATCH));
		result["version"]["major"] = NLOHMANN_JSON_VERSION_MAJOR;
		result["version"]["minor"] = NLOHMANN_JSON_VERSION_MINOR;
		result["version"]["patch"] = NLOHMANN_JSON_VERSION_PATCH;
#ifdef _WIN32
		result["platform"] = "win32";
#elif defined __linux__
		result["platform"] = "linux";
#elif defined __APPLE__
		result["platform"] = "apple";
#elif defined __unix__
		result["platform"] = "unix";
#else
		result["platform"] = "unknown";
#endif
#if defined(__ICC) || defined(__INTEL_COMPILER)
		result["compiler"] = { {"family", "icc"}, {"version", __INTEL_COMPILER} };
#elif defined(__clang__)
		result["compiler"] = { {"family", "clang"}, {"version", __clang_version__} };
#elif defined(__GNUC__) || defined(__GNUG__)
		result["compiler"] = { {"family", "gcc"}, {"version", detail::concat(
					std::to_string(__GNUC__), '.',
					std::to_string(__GNUC_MINOR__), '.',
					std::to_string(__GNUC_PATCHLEVEL__))
			}
		};
#elif defined(__HP_cc) || defined(__HP_aCC)
		result["compiler"] = "hp"
#elif defined(__IBMCPP__)
		result["compiler"] = { {"family", "ilecpp"}, {"version", __IBMCPP__} };
#elif defined(_MSC_VER)
		result["compiler"] = { {"family", "msvc"}, {"version", _MSC_VER} };
#elif defined(__PGI)
		result["compiler"] = { {"family", "pgcpp"}, {"version", __PGI} };
#elif defined(__SUNPRO_CC)
		result["compiler"] = { {"family", "sunpro"}, {"version", __SUNPRO_CC} };
#else
		result["compiler"] = { {"family", "unknown"}, {"version", "unknown"} };
#endif
#if defined(_MSVC_LANG)
		result["compiler"]["c++"] = std::to_string(_MSVC_LANG);
#elif defined(__cplusplus)
		result["compiler"]["c++"] = std::to_string(__cplusplus);
#else
		result["compiler"]["c++"] = "unknown";
#endif
		return result;
	}
#if defined(JSON_HAS_CPP_14)
	using default_object_comparator_t = std::less<>;
#else
	using default_object_comparator_t = std::less<StringType>;
#endif
	using object_t = ObjectType<StringType,
		basic_json,
		default_object_comparator_t,
		AllocatorType<std::pair<const StringType,
		basic_json>>>;
	using array_t = ArrayType<basic_json, AllocatorType<basic_json>>;
	using string_t = StringType;
	using boolean_t = BooleanType;
	using number_integer_t = NumberIntegerType;
	using number_unsigned_t = NumberUnsignedType;
	using number_float_t = NumberFloatType;
	using binary_t = JsonLib::byte_container_with_subtype<BinaryType>;
	using object_comparator_t = detail::actual_object_comparator_t<basic_json>;
private:
	template<typename T, typename... Args>
	JSON_HEDLEY_RETURNS_NON_NULL
		static T* create(Args&& ... args)
	{
		AllocatorType<T> alloc;
		using AllocatorTraits = std::allocator_traits<AllocatorType<T>>;
		auto deleter = [&](T* obj)
			{
				AllocatorTraits::deallocate(alloc, obj, 1);
			};
		std::unique_ptr<T, decltype(deleter)> obj(AllocatorTraits::allocate(alloc, 1), deleter);
		AllocatorTraits::construct(alloc, obj.get(), std::forward<Args>(args)...);
		JSON_ASSERT(obj != nullptr);
		return obj.release();
	}
JSON_PRIVATE_UNLESS_TESTED:
	union json_value
	{
		object_t* object;
		array_t* array;
		string_t* string;
		binary_t* binary;
		boolean_t boolean;
		number_integer_t number_integer;
		number_unsigned_t number_unsigned;
		number_float_t number_float;
		json_value() = default;
		json_value(boolean_t v) noexcept : boolean(v) {}
		json_value(number_integer_t v) noexcept : number_integer(v) {}
		json_value(number_unsigned_t v) noexcept : number_unsigned(v) {}
		json_value(number_float_t v) noexcept : number_float(v) {}
		json_value(value_t t)
		{
			switch (t)
			{
			case value_t::object:
			{
				object = create<object_t>();
				break;
			}
			case value_t::array:
			{
				array = create<array_t>();
				break;
			}
			case value_t::string:
			{
				string = create<string_t>("");
				break;
			}
			case value_t::binary:
			{
				binary = create<binary_t>();
				break;
			}
			case value_t::boolean:
			{
				boolean = static_cast<boolean_t>(false);
				break;
			}
			case value_t::number_integer:
			{
				number_integer = static_cast<number_integer_t>(0);
				break;
			}
			case value_t::number_unsigned:
			{
				number_unsigned = static_cast<number_unsigned_t>(0);
				break;
			}
			case value_t::number_float:
			{
				number_float = static_cast<number_float_t>(0.0);
				break;
			}
			case value_t::null:
			{
				object = nullptr;
				break;
			}
			case value_t::discarded:
			default:
			{
				object = nullptr;
				if (JSON_HEDLEY_UNLIKELY(t == value_t::null))
				{
					JSON_THROW(other_error::create(500, "961c151d2e87f2686a955a9be24d316f1362bf21 3.11.3", nullptr));
				}
				break;
			}
			}
		}
		json_value(const string_t& value) : string(create<string_t>(value)) {}
		json_value(string_t&& value) : string(create<string_t>(std::move(value))) {}
		json_value(const object_t& value) : object(create<object_t>(value)) {}
		json_value(object_t&& value) : object(create<object_t>(std::move(value))) {}
		json_value(const array_t& value) : array(create<array_t>(value)) {}
		json_value(array_t&& value) : array(create<array_t>(std::move(value))) {}
		json_value(const typename binary_t::container_type& value) : binary(create<binary_t>(value)) {}
		json_value(typename binary_t::container_type&& value) : binary(create<binary_t>(std::move(value))) {}
		json_value(const binary_t& value) : binary(create<binary_t>(value)) {}
		json_value(binary_t&& value) : binary(create<binary_t>(std::move(value))) {}
		void destroy(value_t t)
		{
			if (
				(t == value_t::object && object == nullptr) ||
				(t == value_t::array && array == nullptr) ||
				(t == value_t::string && string == nullptr) ||
				(t == value_t::binary && binary == nullptr)
				)
			{
				return;
			}
			if (t == value_t::array || t == value_t::object)
			{
				std::vector<basic_json> stack;
				if (t == value_t::array)
				{
					stack.reserve(array->size());
					std::move(array->begin(), array->end(), std::back_inserter(stack));
				}
				else
				{
					stack.reserve(object->size());
					for (auto&& it : *object)
					{
						stack.push_back(std::move(it.second));
					}
				}
				while (!stack.empty())
				{
					basic_json current_item(std::move(stack.back()));
					stack.pop_back();
					if (current_item.is_array())
					{
						std::move(current_item.m_data.m_value.array->begin(), current_item.m_data.m_value.array->end(), std::back_inserter(stack));
						current_item.m_data.m_value.array->clear();
					}
					else if (current_item.is_object())
					{
						for (auto&& it : *current_item.m_data.m_value.object)
						{
							stack.push_back(std::move(it.second));
						}
						current_item.m_data.m_value.object->clear();
					}
				}
			}
			switch (t)
			{
			case value_t::object:
			{
				AllocatorType<object_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, object);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, object, 1);
				break;
			}
			case value_t::array:
			{
				AllocatorType<array_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, array);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, array, 1);
				break;
			}
			case value_t::string:
			{
				AllocatorType<string_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, string);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, string, 1);
				break;
			}
			case value_t::binary:
			{
				AllocatorType<binary_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, binary);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, binary, 1);
				break;
			}
			case value_t::null:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::discarded:
			default:
			{
				break;
			}
			}
		}
	};
private:
	void assert_invariant(bool check_parents = true) const noexcept
	{
		JSON_ASSERT(m_data.m_type != value_t::object || m_data.m_value.object != nullptr);
		JSON_ASSERT(m_data.m_type != value_t::array || m_data.m_value.array != nullptr);
		JSON_ASSERT(m_data.m_type != value_t::string || m_data.m_value.string != nullptr);
		JSON_ASSERT(m_data.m_type != value_t::binary || m_data.m_value.binary != nullptr);
#if JSON_DIAGNOSTICS
		JSON_TRY
		{
			JSON_ASSERT(!check_parents || !is_structured() || std::all_of(begin(), end(), [this](const basic_json& j)
			{
				return j.m_parent == this;
			}));
		}
			JSON_CATCH(...) {}
#endif
		static_cast<void>(check_parents);
	}
	void set_parents()
	{
#if JSON_DIAGNOSTICS
		switch (m_data.m_type)
		{
		case value_t::array:
		{
			for (auto& element : *m_data.m_value.array)
			{
				element.m_parent = this;
			}
			break;
		}
		case value_t::object:
		{
			for (auto& element : *m_data.m_value.object)
			{
				element.second.m_parent = this;
			}
			break;
		}
		case value_t::null:
		case value_t::string:
		case value_t::boolean:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		case value_t::binary:
		case value_t::discarded:
		default:
			break;
		}
#endif
	}
	iterator set_parents(iterator it, typename iterator::difference_type count_set_parents)
	{
#if JSON_DIAGNOSTICS
		for (typename iterator::difference_type i = 0; i < count_set_parents; ++i)
		{
			(it + i)->m_parent = this;
		}
#else
		static_cast<void>(count_set_parents);
#endif
		return it;
	}
	reference set_parent(reference j, std::size_t old_capacity = static_cast<std::size_t>(-1))
	{
#if JSON_DIAGNOSTICS
		if (old_capacity != static_cast<std::size_t>(-1))
		{
			JSON_ASSERT(type() == value_t::array);
			if (JSON_HEDLEY_UNLIKELY(m_data.m_value.array->capacity() != old_capacity))
			{
				set_parents();
				return j;
			}
		}
#ifdef JSON_HEDLEY_MSVC_VERSION
#pragma warning(push )
#pragma warning(disable : 4127) 
#endif
		if (detail::is_ordered_map<object_t>::value)
		{
			set_parents();
			return j;
		}
#ifdef JSON_HEDLEY_MSVC_VERSION
#pragma warning( pop )
#endif
		j.m_parent = this;
#else
		static_cast<void>(j);
		static_cast<void>(old_capacity);
#endif
		return j;
	}
public:
	using parse_event_t = detail::parse_event_t;
	using parser_callback_t = detail::parser_callback_t<basic_json>;
	basic_json(const value_t v)
		: m_data(v)
	{
		assert_invariant();
	}
	basic_json(std::nullptr_t = nullptr) noexcept
		: basic_json(value_t::null)
	{
		assert_invariant();
	}
	template < typename CompatibleType,
		typename U = detail::uncvref_t<CompatibleType>,
		detail::enable_if_t <
		!detail::is_basic_json<U>::value&& detail::is_compatible_type<basic_json_t, U>::value, int > = 0 >
	basic_json(CompatibleType&& val) noexcept(noexcept(
		JSONSerializer<U>::to_json(std::declval<basic_json_t&>(),
			std::forward<CompatibleType>(val))))
	{
		JSONSerializer<U>::to_json(*this, std::forward<CompatibleType>(val));
		set_parents();
		assert_invariant();
	}
	template < typename BasicJsonType,
		detail::enable_if_t <
		detail::is_basic_json<BasicJsonType>::value && !std::is_same<basic_json, BasicJsonType>::value, int > = 0 >
	basic_json(const BasicJsonType& val)
	{
		using other_boolean_t = typename BasicJsonType::boolean_t;
		using other_number_float_t = typename BasicJsonType::number_float_t;
		using other_number_integer_t = typename BasicJsonType::number_integer_t;
		using other_number_unsigned_t = typename BasicJsonType::number_unsigned_t;
		using other_string_t = typename BasicJsonType::string_t;
		using other_object_t = typename BasicJsonType::object_t;
		using other_array_t = typename BasicJsonType::array_t;
		using other_binary_t = typename BasicJsonType::binary_t;
		switch (val.type())
		{
		case value_t::boolean:
			JSONSerializer<other_boolean_t>::to_json(*this, val.template get<other_boolean_t>());
			break;
		case value_t::number_float:
			JSONSerializer<other_number_float_t>::to_json(*this, val.template get<other_number_float_t>());
			break;
		case value_t::number_integer:
			JSONSerializer<other_number_integer_t>::to_json(*this, val.template get<other_number_integer_t>());
			break;
		case value_t::number_unsigned:
			JSONSerializer<other_number_unsigned_t>::to_json(*this, val.template get<other_number_unsigned_t>());
			break;
		case value_t::string:
			JSONSerializer<other_string_t>::to_json(*this, val.template get_ref<const other_string_t&>());
			break;
		case value_t::object:
			JSONSerializer<other_object_t>::to_json(*this, val.template get_ref<const other_object_t&>());
			break;
		case value_t::array:
			JSONSerializer<other_array_t>::to_json(*this, val.template get_ref<const other_array_t&>());
			break;
		case value_t::binary:
			JSONSerializer<other_binary_t>::to_json(*this, val.template get_ref<const other_binary_t&>());
			break;
		case value_t::null:
			*this = nullptr;
			break;
		case value_t::discarded:
			m_data.m_type = value_t::discarded;
			break;
		default:
			JSON_ASSERT(false);
		}
		JSON_ASSERT(m_data.m_type == val.type());
		set_parents();
		assert_invariant();
	}
	basic_json(initializer_list_t init,
		bool type_deduction = true,
		value_t manual_type = value_t::array)
	{
		bool is_an_object = std::all_of(init.begin(), init.end(),
			[](const detail::json_ref<basic_json>& element_ref)
			{
				return element_ref->is_array() && element_ref->size() == 2 && (*element_ref)[static_cast<size_type>(0)].is_string();
			});
		if (!type_deduction)
		{
			if (manual_type == value_t::array)
			{
				is_an_object = false;
			}
			if (JSON_HEDLEY_UNLIKELY(manual_type == value_t::object && !is_an_object))
			{
				JSON_THROW(type_error::create(301, "cannot create object from initializer list", nullptr));
			}
		}
		if (is_an_object)
		{
			m_data.m_type = value_t::object;
			m_data.m_value = value_t::object;
			for (auto& element_ref : init)
			{
				auto element = element_ref.moved_or_copied();
				m_data.m_value.object->emplace(
					std::move(*((*element.m_data.m_value.array)[0].m_data.m_value.string)),
					std::move((*element.m_data.m_value.array)[1]));
			}
		}
		else
		{
			m_data.m_type = value_t::array;
			m_data.m_value.array = create<array_t>(init.begin(), init.end());
		}
		set_parents();
		assert_invariant();
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json binary(const typename binary_t::container_type& init)
	{
		auto res = basic_json();
		res.m_data.m_type = value_t::binary;
		res.m_data.m_value = init;
		return res;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json binary(const typename binary_t::container_type& init, typename binary_t::subtype_type subtype)
	{
		auto res = basic_json();
		res.m_data.m_type = value_t::binary;
		res.m_data.m_value = binary_t(init, subtype);
		return res;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json binary(typename binary_t::container_type&& init)
	{
		auto res = basic_json();
		res.m_data.m_type = value_t::binary;
		res.m_data.m_value = std::move(init);
		return res;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json binary(typename binary_t::container_type&& init, typename binary_t::subtype_type subtype)
	{
		auto res = basic_json();
		res.m_data.m_type = value_t::binary;
		res.m_data.m_value = binary_t(std::move(init), subtype);
		return res;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json array(initializer_list_t init = {})
	{
		return basic_json(init, false, value_t::array);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json object(initializer_list_t init = {})
	{
		return basic_json(init, false, value_t::object);
	}
	basic_json(size_type cnt, const basic_json& val) :
		m_data{ cnt, val }
	{
		set_parents();
		assert_invariant();
	}
	template < class InputIT, typename std::enable_if <
		std::is_same<InputIT, typename basic_json_t::iterator>::value ||
		std::is_same<InputIT, typename basic_json_t::const_iterator>::value, int >::type = 0 >
	basic_json(InputIT first, InputIT last)
	{
		JSON_ASSERT(first.m_object != nullptr);
		JSON_ASSERT(last.m_object != nullptr);
		if (JSON_HEDLEY_UNLIKELY(first.m_object != last.m_object))
		{
			JSON_THROW(invalid_iterator::create(201, "iterators are not compatible", nullptr));
		}
		m_data.m_type = first.m_object->m_data.m_type;
		switch (m_data.m_type)
		{
		case value_t::boolean:
		case value_t::number_float:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::string:
		{
			if (JSON_HEDLEY_UNLIKELY(!first.m_it.primitive_iterator.is_begin()
				|| !last.m_it.primitive_iterator.is_end()))
			{
				JSON_THROW(invalid_iterator::create(204, "iterators out of range", first.m_object));
			}
			break;
		}
		case value_t::null:
		case value_t::object:
		case value_t::array:
		case value_t::binary:
		case value_t::discarded:
		default:
			break;
		}
		switch (m_data.m_type)
		{
		case value_t::number_integer:
		{
			m_data.m_value.number_integer = first.m_object->m_data.m_value.number_integer;
			break;
		}
		case value_t::number_unsigned:
		{
			m_data.m_value.number_unsigned = first.m_object->m_data.m_value.number_unsigned;
			break;
		}
		case value_t::number_float:
		{
			m_data.m_value.number_float = first.m_object->m_data.m_value.number_float;
			break;
		}
		case value_t::boolean:
		{
			m_data.m_value.boolean = first.m_object->m_data.m_value.boolean;
			break;
		}
		case value_t::string:
		{
			m_data.m_value = *first.m_object->m_data.m_value.string;
			break;
		}
		case value_t::object:
		{
			m_data.m_value.object = create<object_t>(first.m_it.object_iterator,
				last.m_it.object_iterator);
			break;
		}
		case value_t::array:
		{
			m_data.m_value.array = create<array_t>(first.m_it.array_iterator,
				last.m_it.array_iterator);
			break;
		}
		case value_t::binary:
		{
			m_data.m_value = *first.m_object->m_data.m_value.binary;
			break;
		}
		case value_t::null:
		case value_t::discarded:
		default:
			JSON_THROW(invalid_iterator::create(206, detail::concat("cannot construct with iterators from ", first.m_object->type_name()), first.m_object));
		}
		set_parents();
		assert_invariant();
	}
	template<typename JsonRef,
		detail::enable_if_t<detail::conjunction<detail::is_json_ref<JsonRef>,
		std::is_same<typename JsonRef::value_type, basic_json>>::value, int> = 0 >
		basic_json(const JsonRef& ref) : basic_json(ref.moved_or_copied()) {}
	basic_json(const basic_json& other)
		: json_base_class_t(other)
	{
		m_data.m_type = other.m_data.m_type;
		other.assert_invariant();
		switch (m_data.m_type)
		{
		case value_t::object:
		{
			m_data.m_value = *other.m_data.m_value.object;
			break;
		}
		case value_t::array:
		{
			m_data.m_value = *other.m_data.m_value.array;
			break;
		}
		case value_t::string:
		{
			m_data.m_value = *other.m_data.m_value.string;
			break;
		}
		case value_t::boolean:
		{
			m_data.m_value = other.m_data.m_value.boolean;
			break;
		}
		case value_t::number_integer:
		{
			m_data.m_value = other.m_data.m_value.number_integer;
			break;
		}
		case value_t::number_unsigned:
		{
			m_data.m_value = other.m_data.m_value.number_unsigned;
			break;
		}
		case value_t::number_float:
		{
			m_data.m_value = other.m_data.m_value.number_float;
			break;
		}
		case value_t::binary:
		{
			m_data.m_value = *other.m_data.m_value.binary;
			break;
		}
		case value_t::null:
		case value_t::discarded:
		default:
			break;
		}
		set_parents();
		assert_invariant();
	}
	basic_json(basic_json&& other) noexcept
		: json_base_class_t(std::forward<json_base_class_t>(other)),
		m_data(std::move(other.m_data))
	{
		other.assert_invariant(false);
		other.m_data.m_type = value_t::null;
		other.m_data.m_value = {};
		set_parents();
		assert_invariant();
	}
	basic_json& operator=(basic_json other) noexcept (
		std::is_nothrow_move_constructible<value_t>::value&&
		std::is_nothrow_move_assignable<value_t>::value&&
		std::is_nothrow_move_constructible<json_value>::value&&
		std::is_nothrow_move_assignable<json_value>::value&&
		std::is_nothrow_move_assignable<json_base_class_t>::value
		)
	{
		other.assert_invariant();
		using std::swap;
		swap(m_data.m_type, other.m_data.m_type);
		swap(m_data.m_value, other.m_data.m_value);
		json_base_class_t::operator=(std::move(other));
		set_parents();
		assert_invariant();
		return *this;
	}
	~basic_json() noexcept
	{
		assert_invariant(false);
	}
public:
	string_t dump(const int indent = -1,
		const char indent_char = ' ',
		const bool ensure_ascii = false,
		const error_handler_t error_handler = error_handler_t::strict) const
	{
		string_t result;
		serializer s(detail::output_adapter<char, string_t>(result), indent_char, error_handler);
		if (indent >= 0)
		{
			s.dump(*this, true, ensure_ascii, static_cast<unsigned int>(indent));
		}
		else
		{
			s.dump(*this, false, ensure_ascii, 0);
		}
		return result;
	}
	constexpr value_t type() const noexcept
	{
		return m_data.m_type;
	}
	constexpr bool is_primitive() const noexcept
	{
		return is_null() || is_string() || is_boolean() || is_number() || is_binary();
	}
	constexpr bool is_structured() const noexcept
	{
		return is_array() || is_object();
	}
	constexpr bool is_null() const noexcept
	{
		return m_data.m_type == value_t::null;
	}
	constexpr bool is_boolean() const noexcept
	{
		return m_data.m_type == value_t::boolean;
	}
	constexpr bool is_number() const noexcept
	{
		return is_number_integer() || is_number_float();
	}
	constexpr bool is_number_integer() const noexcept
	{
		return m_data.m_type == value_t::number_integer || m_data.m_type == value_t::number_unsigned;
	}
	constexpr bool is_number_unsigned() const noexcept
	{
		return m_data.m_type == value_t::number_unsigned;
	}
	constexpr bool is_number_float() const noexcept
	{
		return m_data.m_type == value_t::number_float;
	}
	constexpr bool is_object() const noexcept
	{
		return m_data.m_type == value_t::object;
	}
	constexpr bool is_array() const noexcept
	{
		return m_data.m_type == value_t::array;
	}
	constexpr bool is_string() const noexcept
	{
		return m_data.m_type == value_t::string;
	}
	constexpr bool is_binary() const noexcept
	{
		return m_data.m_type == value_t::binary;
	}
	constexpr bool is_discarded() const noexcept
	{
		return m_data.m_type == value_t::discarded;
	}
	constexpr operator value_t() const noexcept
	{
		return m_data.m_type;
	}
private:
	boolean_t get_impl(boolean_t*) const
	{
		if (JSON_HEDLEY_LIKELY(is_boolean()))
		{
			return m_data.m_value.boolean;
		}
		JSON_THROW(type_error::create(302, detail::concat("type must be boolean, but is ", type_name()), this));
	}
	object_t* get_impl_ptr(object_t*) noexcept
	{
		return is_object() ? m_data.m_value.object : nullptr;
	}
	constexpr const object_t* get_impl_ptr(const object_t*) const noexcept
	{
		return is_object() ? m_data.m_value.object : nullptr;
	}
	array_t* get_impl_ptr(array_t*) noexcept
	{
		return is_array() ? m_data.m_value.array : nullptr;
	}
	constexpr const array_t* get_impl_ptr(const array_t*) const noexcept
	{
		return is_array() ? m_data.m_value.array : nullptr;
	}
	string_t* get_impl_ptr(string_t*) noexcept
	{
		return is_string() ? m_data.m_value.string : nullptr;
	}
	constexpr const string_t* get_impl_ptr(const string_t*) const noexcept
	{
		return is_string() ? m_data.m_value.string : nullptr;
	}
	boolean_t* get_impl_ptr(boolean_t*) noexcept
	{
		return is_boolean() ? &m_data.m_value.boolean : nullptr;
	}
	constexpr const boolean_t* get_impl_ptr(const boolean_t*) const noexcept
	{
		return is_boolean() ? &m_data.m_value.boolean : nullptr;
	}
	number_integer_t* get_impl_ptr(number_integer_t*) noexcept
	{
		return is_number_integer() ? &m_data.m_value.number_integer : nullptr;
	}
	constexpr const number_integer_t* get_impl_ptr(const number_integer_t*) const noexcept
	{
		return is_number_integer() ? &m_data.m_value.number_integer : nullptr;
	}
	number_unsigned_t* get_impl_ptr(number_unsigned_t*) noexcept
	{
		return is_number_unsigned() ? &m_data.m_value.number_unsigned : nullptr;
	}
	constexpr const number_unsigned_t* get_impl_ptr(const number_unsigned_t*) const noexcept
	{
		return is_number_unsigned() ? &m_data.m_value.number_unsigned : nullptr;
	}
	number_float_t* get_impl_ptr(number_float_t*) noexcept
	{
		return is_number_float() ? &m_data.m_value.number_float : nullptr;
	}
	constexpr const number_float_t* get_impl_ptr(const number_float_t*) const noexcept
	{
		return is_number_float() ? &m_data.m_value.number_float : nullptr;
	}
	binary_t* get_impl_ptr(binary_t*) noexcept
	{
		return is_binary() ? m_data.m_value.binary : nullptr;
	}
	constexpr const binary_t* get_impl_ptr(const binary_t*) const noexcept
	{
		return is_binary() ? m_data.m_value.binary : nullptr;
	}
	template<typename ReferenceType, typename ThisType>
	static ReferenceType get_ref_impl(ThisType& obj)
	{
		auto* ptr = obj.template get_ptr<typename std::add_pointer<ReferenceType>::type>();
		if (JSON_HEDLEY_LIKELY(ptr != nullptr))
		{
			return *ptr;
		}
		JSON_THROW(type_error::create(303, detail::concat("incompatible ReferenceType for get_ref, actual type is ", obj.type_name()), &obj));
	}
public:
	template<typename PointerType, typename std::enable_if<
		std::is_pointer<PointerType>::value, int>::type = 0>
	auto get_ptr() noexcept -> decltype(std::declval<basic_json_t&>().get_impl_ptr(std::declval<PointerType>()))
	{
		return get_impl_ptr(static_cast<PointerType>(nullptr));
	}
	template < typename PointerType, typename std::enable_if <
		std::is_pointer<PointerType>::value&&
		std::is_const<typename std::remove_pointer<PointerType>::type>::value, int >::type = 0 >
	constexpr auto get_ptr() const noexcept -> decltype(std::declval<const basic_json_t&>().get_impl_ptr(std::declval<PointerType>()))
	{
		return get_impl_ptr(static_cast<PointerType>(nullptr));
	}
private:
	template < typename ValueType,
		detail::enable_if_t <
		detail::is_default_constructible<ValueType>::value&&
		detail::has_from_json<basic_json_t, ValueType>::value,
		int > = 0 >
	ValueType get_impl(detail::priority_tag<0>) const noexcept(noexcept(
		JSONSerializer<ValueType>::from_json(std::declval<const basic_json_t&>(), std::declval<ValueType&>())))
	{
		auto ret = ValueType();
		JSONSerializer<ValueType>::from_json(*this, ret);
		return ret;
	}
	template < typename ValueType,
		detail::enable_if_t <
		detail::has_non_default_from_json<basic_json_t, ValueType>::value,
		int > = 0 >
	ValueType get_impl(detail::priority_tag<1>) const noexcept(noexcept(
		JSONSerializer<ValueType>::from_json(std::declval<const basic_json_t&>())))
	{
		return JSONSerializer<ValueType>::from_json(*this);
	}
	template < typename BasicJsonType,
		detail::enable_if_t <
		detail::is_basic_json<BasicJsonType>::value,
		int > = 0 >
	BasicJsonType get_impl(detail::priority_tag<2>) const
	{
		return *this;
	}
	template<typename BasicJsonType,
		detail::enable_if_t<
		std::is_same<BasicJsonType, basic_json_t>::value,
		int> = 0>
	basic_json get_impl(detail::priority_tag<3>) const
	{
		return *this;
	}
	template<typename PointerType,
		detail::enable_if_t<
		std::is_pointer<PointerType>::value,
		int> = 0>
	constexpr auto get_impl(detail::priority_tag<4>) const noexcept
		-> decltype(std::declval<const basic_json_t&>().template get_ptr<PointerType>())
	{
		return get_ptr<PointerType>();
	}
public:
	template < typename ValueTypeCV, typename ValueType = detail::uncvref_t<ValueTypeCV>>
#if defined(JSON_HAS_CPP_14)
	constexpr
#endif
		auto get() const noexcept(
			noexcept(std::declval<const basic_json_t&>().template get_impl<ValueType>(detail::priority_tag<4> {})))
		-> decltype(std::declval<const basic_json_t&>().template get_impl<ValueType>(detail::priority_tag<4> {}))
	{
		static_assert(!std::is_reference<ValueTypeCV>::value,
			"get() cannot be used with reference types, you might want to use get_ref()");
		return get_impl<ValueType>(detail::priority_tag<4> {});
	}
	template<typename PointerType, typename std::enable_if<
		std::is_pointer<PointerType>::value, int>::type = 0>
	auto get() noexcept -> decltype(std::declval<basic_json_t&>().template get_ptr<PointerType>())
	{
		return get_ptr<PointerType>();
	}
	template < typename ValueType,
		detail::enable_if_t <
		!detail::is_basic_json<ValueType>::value&&
		detail::has_from_json<basic_json_t, ValueType>::value,
		int > = 0 >
	ValueType& get_to(ValueType& v) const noexcept(noexcept(
		JSONSerializer<ValueType>::from_json(std::declval<const basic_json_t&>(), v)))
	{
		JSONSerializer<ValueType>::from_json(*this, v);
		return v;
	}
	template<typename ValueType,
		detail::enable_if_t <
		detail::is_basic_json<ValueType>::value,
		int> = 0>
	ValueType& get_to(ValueType& v) const
	{
		v = *this;
		return v;
	}
	template <
		typename T, std::size_t N,
		typename Array = T(&)[N],
		detail::enable_if_t <
		detail::has_from_json<basic_json_t, Array>::value, int > = 0 >
	Array get_to(T(&v)[N]) const
		noexcept(noexcept(JSONSerializer<Array>::from_json(
			std::declval<const basic_json_t&>(), v)))
	{
		JSONSerializer<Array>::from_json(*this, v);
		return v;
	}
	template<typename ReferenceType, typename std::enable_if<
		std::is_reference<ReferenceType>::value, int>::type = 0>
	ReferenceType get_ref()
	{
		return get_ref_impl<ReferenceType>(*this);
	}
	template < typename ReferenceType, typename std::enable_if <
		std::is_reference<ReferenceType>::value&&
		std::is_const<typename std::remove_reference<ReferenceType>::type>::value, int >::type = 0 >
	ReferenceType get_ref() const
	{
		return get_ref_impl<ReferenceType>(*this);
	}
	template < typename ValueType, typename std::enable_if <
		detail::conjunction <
		detail::negation<std::is_pointer<ValueType>>,
		detail::negation<std::is_same<ValueType, std::nullptr_t>>,
		detail::negation<std::is_same<ValueType, detail::json_ref<basic_json>>>,
		detail::negation<std::is_same<ValueType, typename string_t::value_type>>,
		detail::negation<detail::is_basic_json<ValueType>>,
		detail::negation<std::is_same<ValueType, std::initializer_list<typename string_t::value_type>>>,
#if defined(JSON_HAS_CPP_17) && (defined(__GNUC__) || (defined(_MSC_VER) && _MSC_VER >= 1910 && _MSC_VER <= 1914))
		detail::negation<std::is_same<ValueType, std::string_view>>,
#endif
#if defined(JSON_HAS_CPP_17) && JSON_HAS_STATIC_RTTI
		detail::negation<std::is_same<ValueType, std::any>>,
#endif
		detail::is_detected_lazy<detail::get_template_function, const basic_json_t&, ValueType>
	>::value, int >::type = 0 >
	JSON_EXPLICIT operator ValueType() const
	{
		return get<ValueType>();
	}
	binary_t& get_binary()
	{
		if (!is_binary())
		{
			JSON_THROW(type_error::create(302, detail::concat("type must be binary, but is ", type_name()), this));
		}
		return *get_ptr<binary_t*>();
	}
	const binary_t& get_binary() const
	{
		if (!is_binary())
		{
			JSON_THROW(type_error::create(302, detail::concat("type must be binary, but is ", type_name()), this));
		}
		return *get_ptr<const binary_t*>();
	}
	reference at(size_type idx)
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			JSON_TRY
			{
				return set_parent(m_data.m_value.array->at(idx));
			}
				JSON_CATCH(std::out_of_range&)
			{
				JSON_THROW(out_of_range::create(401, detail::concat("array index ", std::to_string(idx), " is out of range"), this));
			}
		}
		else
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
	}
	const_reference at(size_type idx) const
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			JSON_TRY
			{
				return m_data.m_value.array->at(idx);
			}
				JSON_CATCH(std::out_of_range&)
			{
				JSON_THROW(out_of_range::create(401, detail::concat("array index ", std::to_string(idx), " is out of range"), this));
			}
		}
		else
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
	}
	reference at(const typename object_t::key_type& key)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
		auto it = m_data.m_value.object->find(key);
		if (it == m_data.m_value.object->end())
		{
			JSON_THROW(out_of_range::create(403, detail::concat("key '", key, "' not found"), this));
		}
		return set_parent(it->second);
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	reference at(KeyType&& key)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
		auto it = m_data.m_value.object->find(std::forward<KeyType>(key));
		if (it == m_data.m_value.object->end())
		{
			JSON_THROW(out_of_range::create(403, detail::concat("key '", string_t(std::forward<KeyType>(key)), "' not found"), this));
		}
		return set_parent(it->second);
	}
	const_reference at(const typename object_t::key_type& key) const
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
		auto it = m_data.m_value.object->find(key);
		if (it == m_data.m_value.object->end())
		{
			JSON_THROW(out_of_range::create(403, detail::concat("key '", key, "' not found"), this));
		}
		return it->second;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	const_reference at(KeyType&& key) const
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(304, detail::concat("cannot use at() with ", type_name()), this));
		}
		auto it = m_data.m_value.object->find(std::forward<KeyType>(key));
		if (it == m_data.m_value.object->end())
		{
			JSON_THROW(out_of_range::create(403, detail::concat("key '", string_t(std::forward<KeyType>(key)), "' not found"), this));
		}
		return it->second;
	}
	reference operator[](size_type idx)
	{
		if (is_null())
		{
			m_data.m_type = value_t::array;
			m_data.m_value.array = create<array_t>();
			assert_invariant();
		}
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			if (idx >= m_data.m_value.array->size())
			{
#if JSON_DIAGNOSTICS
				const auto old_size = m_data.m_value.array->size();
				const auto old_capacity = m_data.m_value.array->capacity();
#endif
				m_data.m_value.array->resize(idx + 1);
#if JSON_DIAGNOSTICS
				if (JSON_HEDLEY_UNLIKELY(m_data.m_value.array->capacity() != old_capacity))
				{
					set_parents();
				}
				else
				{
					set_parents(begin() + static_cast<typename iterator::difference_type>(old_size), static_cast<typename iterator::difference_type>(idx + 1 - old_size));
				}
#endif
				assert_invariant();
			}
			return m_data.m_value.array->operator[](idx);
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a numeric argument with ", type_name()), this));
	}
	const_reference operator[](size_type idx) const
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			return m_data.m_value.array->operator[](idx);
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a numeric argument with ", type_name()), this));
	}
	reference operator[](typename object_t::key_type key)
	{
		if (is_null())
		{
			m_data.m_type = value_t::object;
			m_data.m_value.object = create<object_t>();
			assert_invariant();
		}
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			auto result = m_data.m_value.object->emplace(std::move(key), nullptr);
			return set_parent(result.first->second);
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a string argument with ", type_name()), this));
	}
	const_reference operator[](const typename object_t::key_type& key) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			auto it = m_data.m_value.object->find(key);
			JSON_ASSERT(it != m_data.m_value.object->end());
			return it->second;
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a string argument with ", type_name()), this));
	}
	template<typename T>
	reference operator[](T* key)
	{
		return operator[](typename object_t::key_type(key));
	}
	template<typename T>
	const_reference operator[](T* key) const
	{
		return operator[](typename object_t::key_type(key));
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int > = 0 >
	reference operator[](KeyType&& key)
	{
		if (is_null())
		{
			m_data.m_type = value_t::object;
			m_data.m_value.object = create<object_t>();
			assert_invariant();
		}
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			auto result = m_data.m_value.object->emplace(std::forward<KeyType>(key), nullptr);
			return set_parent(result.first->second);
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a string argument with ", type_name()), this));
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int > = 0 >
	const_reference operator[](KeyType&& key) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			auto it = m_data.m_value.object->find(std::forward<KeyType>(key));
			JSON_ASSERT(it != m_data.m_value.object->end());
			return it->second;
		}
		JSON_THROW(type_error::create(305, detail::concat("cannot use operator[] with a string argument with ", type_name()), this));
	}
private:
	template<typename KeyType>
	using is_comparable_with_object_key = detail::is_comparable <
		object_comparator_t, const typename object_t::key_type&, KeyType >;
	template<typename ValueType>
	using value_return_type = std::conditional <
		detail::is_c_string_uncvref<ValueType>::value,
		string_t, typename std::decay<ValueType>::type >;
public:
	template < class ValueType, detail::enable_if_t <
		!detail::is_transparent<object_comparator_t>::value
		&& detail::is_getable<basic_json_t, ValueType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ValueType value(const typename object_t::key_type& key, const ValueType& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			const auto it = find(key);
			if (it != end())
			{
				return it->template get<ValueType>();
			}
			return default_value;
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, class ReturnType = typename value_return_type<ValueType>::type,
		detail::enable_if_t <
		!detail::is_transparent<object_comparator_t>::value
		&& detail::is_getable<basic_json_t, ReturnType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ReturnType value(const typename object_t::key_type& key, ValueType&& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			const auto it = find(key);
			if (it != end())
			{
				return it->template get<ReturnType>();
			}
			return std::forward<ValueType>(default_value);
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, class KeyType, detail::enable_if_t <
		detail::is_transparent<object_comparator_t>::value
		&& !detail::is_json_pointer<KeyType>::value
		&& is_comparable_with_object_key<KeyType>::value
		&& detail::is_getable<basic_json_t, ValueType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ValueType value(KeyType&& key, const ValueType& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			const auto it = find(std::forward<KeyType>(key));
			if (it != end())
			{
				return it->template get<ValueType>();
			}
			return default_value;
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, class KeyType, class ReturnType = typename value_return_type<ValueType>::type,
		detail::enable_if_t <
		detail::is_transparent<object_comparator_t>::value
		&& !detail::is_json_pointer<KeyType>::value
		&& is_comparable_with_object_key<KeyType>::value
		&& detail::is_getable<basic_json_t, ReturnType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ReturnType value(KeyType&& key, ValueType&& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			const auto it = find(std::forward<KeyType>(key));
			if (it != end())
			{
				return it->template get<ReturnType>();
			}
			return std::forward<ValueType>(default_value);
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, detail::enable_if_t <
		detail::is_getable<basic_json_t, ValueType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ValueType value(const json_pointer& ptr, const ValueType& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			JSON_TRY
			{
				return ptr.get_checked(this).template get<ValueType>();
			}
				JSON_INTERNAL_CATCH(out_of_range&)
			{
				return default_value;
			}
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, class ReturnType = typename value_return_type<ValueType>::type,
		detail::enable_if_t <
		detail::is_getable<basic_json_t, ReturnType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	ReturnType value(const json_pointer& ptr, ValueType&& default_value) const
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			JSON_TRY
			{
				return ptr.get_checked(this).template get<ReturnType>();
			}
				JSON_INTERNAL_CATCH(out_of_range&)
			{
				return std::forward<ValueType>(default_value);
			}
		}
		JSON_THROW(type_error::create(306, detail::concat("cannot use value() with ", type_name()), this));
	}
	template < class ValueType, class BasicJsonType, detail::enable_if_t <
		detail::is_basic_json<BasicJsonType>::value
		&& detail::is_getable<basic_json_t, ValueType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		ValueType value(const ::JsonLib::json_pointer<BasicJsonType>& ptr, const ValueType& default_value) const
	{
		return value(ptr.convert(), default_value);
	}
	template < class ValueType, class BasicJsonType, class ReturnType = typename value_return_type<ValueType>::type,
		detail::enable_if_t <
		detail::is_basic_json<BasicJsonType>::value
		&& detail::is_getable<basic_json_t, ReturnType>::value
		&& !std::is_same<value_t, detail::uncvref_t<ValueType>>::value, int > = 0 >
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		ReturnType value(const ::JsonLib::json_pointer<BasicJsonType>& ptr, ValueType&& default_value) const
	{
		return value(ptr.convert(), std::forward<ValueType>(default_value));
	}
	reference front()
	{
		return *begin();
	}
	const_reference front() const
	{
		return *cbegin();
	}
	reference back()
	{
		auto tmp = end();
		--tmp;
		return *tmp;
	}
	const_reference back() const
	{
		auto tmp = cend();
		--tmp;
		return *tmp;
	}
	template < class IteratorType, detail::enable_if_t <
		std::is_same<IteratorType, typename basic_json_t::iterator>::value ||
		std::is_same<IteratorType, typename basic_json_t::const_iterator>::value, int > = 0 >
	IteratorType erase(IteratorType pos)
	{
		if (JSON_HEDLEY_UNLIKELY(this != pos.m_object))
		{
			JSON_THROW(invalid_iterator::create(202, "iterator does not fit current value", this));
		}
		IteratorType result = end();
		switch (m_data.m_type)
		{
		case value_t::boolean:
		case value_t::number_float:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::string:
		case value_t::binary:
		{
			if (JSON_HEDLEY_UNLIKELY(!pos.m_it.primitive_iterator.is_begin()))
			{
				JSON_THROW(invalid_iterator::create(205, "iterator out of range", this));
			}
			if (is_string())
			{
				AllocatorType<string_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, m_data.m_value.string);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, m_data.m_value.string, 1);
				m_data.m_value.string = nullptr;
			}
			else if (is_binary())
			{
				AllocatorType<binary_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, m_data.m_value.binary);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, m_data.m_value.binary, 1);
				m_data.m_value.binary = nullptr;
			}
			m_data.m_type = value_t::null;
			assert_invariant();
			break;
		}
		case value_t::object:
		{
			result.m_it.object_iterator = m_data.m_value.object->erase(pos.m_it.object_iterator);
			break;
		}
		case value_t::array:
		{
			result.m_it.array_iterator = m_data.m_value.array->erase(pos.m_it.array_iterator);
			break;
		}
		case value_t::null:
		case value_t::discarded:
		default:
			JSON_THROW(type_error::create(307, detail::concat("cannot use erase() with ", type_name()), this));
		}
		return result;
	}
	template < class IteratorType, detail::enable_if_t <
		std::is_same<IteratorType, typename basic_json_t::iterator>::value ||
		std::is_same<IteratorType, typename basic_json_t::const_iterator>::value, int > = 0 >
	IteratorType erase(IteratorType first, IteratorType last)
	{
		if (JSON_HEDLEY_UNLIKELY(this != first.m_object || this != last.m_object))
		{
			JSON_THROW(invalid_iterator::create(203, "iterators do not fit current value", this));
		}
		IteratorType result = end();
		switch (m_data.m_type)
		{
		case value_t::boolean:
		case value_t::number_float:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::string:
		case value_t::binary:
		{
			if (JSON_HEDLEY_LIKELY(!first.m_it.primitive_iterator.is_begin()
				|| !last.m_it.primitive_iterator.is_end()))
			{
				JSON_THROW(invalid_iterator::create(204, "iterators out of range", this));
			}
			if (is_string())
			{
				AllocatorType<string_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, m_data.m_value.string);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, m_data.m_value.string, 1);
				m_data.m_value.string = nullptr;
			}
			else if (is_binary())
			{
				AllocatorType<binary_t> alloc;
				std::allocator_traits<decltype(alloc)>::destroy(alloc, m_data.m_value.binary);
				std::allocator_traits<decltype(alloc)>::deallocate(alloc, m_data.m_value.binary, 1);
				m_data.m_value.binary = nullptr;
			}
			m_data.m_type = value_t::null;
			assert_invariant();
			break;
		}
		case value_t::object:
		{
			result.m_it.object_iterator = m_data.m_value.object->erase(first.m_it.object_iterator,
				last.m_it.object_iterator);
			break;
		}
		case value_t::array:
		{
			result.m_it.array_iterator = m_data.m_value.array->erase(first.m_it.array_iterator,
				last.m_it.array_iterator);
			break;
		}
		case value_t::null:
		case value_t::discarded:
		default:
			JSON_THROW(type_error::create(307, detail::concat("cannot use erase() with ", type_name()), this));
		}
		return result;
	}
private:
	template < typename KeyType, detail::enable_if_t <
		detail::has_erase_with_key_type<basic_json_t, KeyType>::value, int > = 0 >
	size_type erase_internal(KeyType&& key)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(307, detail::concat("cannot use erase() with ", type_name()), this));
		}
		return m_data.m_value.object->erase(std::forward<KeyType>(key));
	}
	template < typename KeyType, detail::enable_if_t <
		!detail::has_erase_with_key_type<basic_json_t, KeyType>::value, int > = 0 >
	size_type erase_internal(KeyType&& key)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(307, detail::concat("cannot use erase() with ", type_name()), this));
		}
		const auto it = m_data.m_value.object->find(std::forward<KeyType>(key));
		if (it != m_data.m_value.object->end())
		{
			m_data.m_value.object->erase(it);
			return 1;
		}
		return 0;
	}
public:
	size_type erase(const typename object_t::key_type& key)
	{
		return erase_internal(key);
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	size_type erase(KeyType&& key)
	{
		return erase_internal(std::forward<KeyType>(key));
	}
	void erase(const size_type idx)
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			if (JSON_HEDLEY_UNLIKELY(idx >= size()))
			{
				JSON_THROW(out_of_range::create(401, detail::concat("array index ", std::to_string(idx), " is out of range"), this));
			}
			m_data.m_value.array->erase(m_data.m_value.array->begin() + static_cast<difference_type>(idx));
		}
		else
		{
			JSON_THROW(type_error::create(307, detail::concat("cannot use erase() with ", type_name()), this));
		}
	}
	iterator find(const typename object_t::key_type& key)
	{
		auto result = end();
		if (is_object())
		{
			result.m_it.object_iterator = m_data.m_value.object->find(key);
		}
		return result;
	}
	const_iterator find(const typename object_t::key_type& key) const
	{
		auto result = cend();
		if (is_object())
		{
			result.m_it.object_iterator = m_data.m_value.object->find(key);
		}
		return result;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	iterator find(KeyType&& key)
	{
		auto result = end();
		if (is_object())
		{
			result.m_it.object_iterator = m_data.m_value.object->find(std::forward<KeyType>(key));
		}
		return result;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	const_iterator find(KeyType&& key) const
	{
		auto result = cend();
		if (is_object())
		{
			result.m_it.object_iterator = m_data.m_value.object->find(std::forward<KeyType>(key));
		}
		return result;
	}
	size_type count(const typename object_t::key_type& key) const
	{
		return is_object() ? m_data.m_value.object->count(key) : 0;
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	size_type count(KeyType&& key) const
	{
		return is_object() ? m_data.m_value.object->count(std::forward<KeyType>(key)) : 0;
	}
	bool contains(const typename object_t::key_type& key) const
	{
		return is_object() && m_data.m_value.object->find(key) != m_data.m_value.object->end();
	}
	template<class KeyType, detail::enable_if_t<
		detail::is_usable_as_basic_json_key_type<basic_json_t, KeyType>::value, int> = 0>
	bool contains(KeyType&& key) const
	{
		return is_object() && m_data.m_value.object->find(std::forward<KeyType>(key)) != m_data.m_value.object->end();
	}
	bool contains(const json_pointer& ptr) const
	{
		return ptr.contains(this);
	}
	template<typename BasicJsonType, detail::enable_if_t<detail::is_basic_json<BasicJsonType>::value, int> = 0>
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		bool contains(const typename ::JsonLib::json_pointer<BasicJsonType>& ptr) const
	{
		return ptr.contains(this);
	}
	iterator begin() noexcept
	{
		iterator result(this);
		result.set_begin();
		return result;
	}
	const_iterator begin() const noexcept
	{
		return cbegin();
	}
	const_iterator cbegin() const noexcept
	{
		const_iterator result(this);
		result.set_begin();
		return result;
	}
	iterator end() noexcept
	{
		iterator result(this);
		result.set_end();
		return result;
	}
	const_iterator end() const noexcept
	{
		return cend();
	}
	const_iterator cend() const noexcept
	{
		const_iterator result(this);
		result.set_end();
		return result;
	}
	reverse_iterator rbegin() noexcept
	{
		return reverse_iterator(end());
	}
	const_reverse_iterator rbegin() const noexcept
	{
		return crbegin();
	}
	reverse_iterator rend() noexcept
	{
		return reverse_iterator(begin());
	}
	const_reverse_iterator rend() const noexcept
	{
		return crend();
	}
	const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator(cend());
	}
	const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator(cbegin());
	}
public:
	JSON_HEDLEY_DEPRECATED_FOR(3.1.0, items())
		static iteration_proxy<iterator> iterator_wrapper(reference ref) noexcept
	{
		return ref.items();
	}
	JSON_HEDLEY_DEPRECATED_FOR(3.1.0, items())
		static iteration_proxy<const_iterator> iterator_wrapper(const_reference ref) noexcept
	{
		return ref.items();
	}
	iteration_proxy<iterator> items() noexcept
	{
		return iteration_proxy<iterator>(*this);
	}
	iteration_proxy<const_iterator> items() const noexcept
	{
		return iteration_proxy<const_iterator>(*this);
	}
	bool empty() const noexcept
	{
		switch (m_data.m_type)
		{
		case value_t::null:
		{
			return true;
		}
		case value_t::array:
		{
			return m_data.m_value.array->empty();
		}
		case value_t::object:
		{
			return m_data.m_value.object->empty();
		}
		case value_t::string:
		case value_t::boolean:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		case value_t::binary:
		case value_t::discarded:
		default:
		{
			return false;
		}
		}
	}
	size_type size() const noexcept
	{
		switch (m_data.m_type)
		{
		case value_t::null:
		{
			return 0;
		}
		case value_t::array:
		{
			return m_data.m_value.array->size();
		}
		case value_t::object:
		{
			return m_data.m_value.object->size();
		}
		case value_t::string:
		case value_t::boolean:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		case value_t::binary:
		case value_t::discarded:
		default:
		{
			return 1;
		}
		}
	}
	size_type max_size() const noexcept
	{
		switch (m_data.m_type)
		{
		case value_t::array:
		{
			return m_data.m_value.array->max_size();
		}
		case value_t::object:
		{
			return m_data.m_value.object->max_size();
		}
		case value_t::null:
		case value_t::string:
		case value_t::boolean:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		case value_t::binary:
		case value_t::discarded:
		default:
		{
			return size();
		}
		}
	}
	void clear() noexcept
	{
		switch (m_data.m_type)
		{
		case value_t::number_integer:
		{
			m_data.m_value.number_integer = 0;
			break;
		}
		case value_t::number_unsigned:
		{
			m_data.m_value.number_unsigned = 0;
			break;
		}
		case value_t::number_float:
		{
			m_data.m_value.number_float = 0.0;
			break;
		}
		case value_t::boolean:
		{
			m_data.m_value.boolean = false;
			break;
		}
		case value_t::string:
		{
			m_data.m_value.string->clear();
			break;
		}
		case value_t::binary:
		{
			m_data.m_value.binary->clear();
			break;
		}
		case value_t::array:
		{
			m_data.m_value.array->clear();
			break;
		}
		case value_t::object:
		{
			m_data.m_value.object->clear();
			break;
		}
		case value_t::null:
		case value_t::discarded:
		default:
			break;
		}
	}
	void push_back(basic_json&& val)
	{
		if (JSON_HEDLEY_UNLIKELY(!(is_null() || is_array())))
		{
			JSON_THROW(type_error::create(308, detail::concat("cannot use push_back() with ", type_name()), this));
		}
		if (is_null())
		{
			m_data.m_type = value_t::array;
			m_data.m_value = value_t::array;
			assert_invariant();
		}
		const auto old_capacity = m_data.m_value.array->capacity();
		m_data.m_value.array->push_back(std::move(val));
		set_parent(m_data.m_value.array->back(), old_capacity);
	}
	reference operator+=(basic_json&& val)
	{
		push_back(std::move(val));
		return *this;
	}
	void push_back(const basic_json& val)
	{
		if (JSON_HEDLEY_UNLIKELY(!(is_null() || is_array())))
		{
			JSON_THROW(type_error::create(308, detail::concat("cannot use push_back() with ", type_name()), this));
		}
		if (is_null())
		{
			m_data.m_type = value_t::array;
			m_data.m_value = value_t::array;
			assert_invariant();
		}
		const auto old_capacity = m_data.m_value.array->capacity();
		m_data.m_value.array->push_back(val);
		set_parent(m_data.m_value.array->back(), old_capacity);
	}
	reference operator+=(const basic_json& val)
	{
		push_back(val);
		return *this;
	}
	void push_back(const typename object_t::value_type& val)
	{
		if (JSON_HEDLEY_UNLIKELY(!(is_null() || is_object())))
		{
			JSON_THROW(type_error::create(308, detail::concat("cannot use push_back() with ", type_name()), this));
		}
		if (is_null())
		{
			m_data.m_type = value_t::object;
			m_data.m_value = value_t::object;
			assert_invariant();
		}
		auto res = m_data.m_value.object->insert(val);
		set_parent(res.first->second);
	}
	reference operator+=(const typename object_t::value_type& val)
	{
		push_back(val);
		return *this;
	}
	void push_back(initializer_list_t init)
	{
		if (is_object() && init.size() == 2 && (*init.begin())->is_string())
		{
			basic_json&& key = init.begin()->moved_or_copied();
			push_back(typename object_t::value_type(
				std::move(key.get_ref<string_t&>()), (init.begin() + 1)->moved_or_copied()));
		}
		else
		{
			push_back(basic_json(init));
		}
	}
	reference operator+=(initializer_list_t init)
	{
		push_back(init);
		return *this;
	}
	template<class... Args>
	reference emplace_back(Args&& ... args)
	{
		if (JSON_HEDLEY_UNLIKELY(!(is_null() || is_array())))
		{
			JSON_THROW(type_error::create(311, detail::concat("cannot use emplace_back() with ", type_name()), this));
		}
		if (is_null())
		{
			m_data.m_type = value_t::array;
			m_data.m_value = value_t::array;
			assert_invariant();
		}
		const auto old_capacity = m_data.m_value.array->capacity();
		m_data.m_value.array->emplace_back(std::forward<Args>(args)...);
		return set_parent(m_data.m_value.array->back(), old_capacity);
	}
	template<class... Args>
	std::pair<iterator, bool> emplace(Args&& ... args)
	{
		if (JSON_HEDLEY_UNLIKELY(!(is_null() || is_object())))
		{
			JSON_THROW(type_error::create(311, detail::concat("cannot use emplace() with ", type_name()), this));
		}
		if (is_null())
		{
			m_data.m_type = value_t::object;
			m_data.m_value = value_t::object;
			assert_invariant();
		}
		auto res = m_data.m_value.object->emplace(std::forward<Args>(args)...);
		set_parent(res.first->second);
		auto it = begin();
		it.m_it.object_iterator = res.first;
		return { it, res.second };
	}
	template<typename... Args>
	iterator insert_iterator(const_iterator pos, Args&& ... args)
	{
		iterator result(this);
		JSON_ASSERT(m_data.m_value.array != nullptr);
		auto insert_pos = std::distance(m_data.m_value.array->begin(), pos.m_it.array_iterator);
		m_data.m_value.array->insert(pos.m_it.array_iterator, std::forward<Args>(args)...);
		result.m_it.array_iterator = m_data.m_value.array->begin() + insert_pos;
		set_parents();
		return result;
	}
	iterator insert(const_iterator pos, const basic_json& val)
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			if (JSON_HEDLEY_UNLIKELY(pos.m_object != this))
			{
				JSON_THROW(invalid_iterator::create(202, "iterator does not fit current value", this));
			}
			return insert_iterator(pos, val);
		}
		JSON_THROW(type_error::create(309, detail::concat("cannot use insert() with ", type_name()), this));
	}
	iterator insert(const_iterator pos, basic_json&& val)
	{
		return insert(pos, val);
	}
	iterator insert(const_iterator pos, size_type cnt, const basic_json& val)
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			if (JSON_HEDLEY_UNLIKELY(pos.m_object != this))
			{
				JSON_THROW(invalid_iterator::create(202, "iterator does not fit current value", this));
			}
			return insert_iterator(pos, cnt, val);
		}
		JSON_THROW(type_error::create(309, detail::concat("cannot use insert() with ", type_name()), this));
	}
	iterator insert(const_iterator pos, const_iterator first, const_iterator last)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_array()))
		{
			JSON_THROW(type_error::create(309, detail::concat("cannot use insert() with ", type_name()), this));
		}
		if (JSON_HEDLEY_UNLIKELY(pos.m_object != this))
		{
			JSON_THROW(invalid_iterator::create(202, "iterator does not fit current value", this));
		}
		if (JSON_HEDLEY_UNLIKELY(first.m_object != last.m_object))
		{
			JSON_THROW(invalid_iterator::create(210, "iterators do not fit", this));
		}
		if (JSON_HEDLEY_UNLIKELY(first.m_object == this))
		{
			JSON_THROW(invalid_iterator::create(211, "passed iterators may not belong to container", this));
		}
		return insert_iterator(pos, first.m_it.array_iterator, last.m_it.array_iterator);
	}
	iterator insert(const_iterator pos, initializer_list_t ilist)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_array()))
		{
			JSON_THROW(type_error::create(309, detail::concat("cannot use insert() with ", type_name()), this));
		}
		if (JSON_HEDLEY_UNLIKELY(pos.m_object != this))
		{
			JSON_THROW(invalid_iterator::create(202, "iterator does not fit current value", this));
		}
		return insert_iterator(pos, ilist.begin(), ilist.end());
	}
	void insert(const_iterator first, const_iterator last)
	{
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(309, detail::concat("cannot use insert() with ", type_name()), this));
		}
		if (JSON_HEDLEY_UNLIKELY(first.m_object != last.m_object))
		{
			JSON_THROW(invalid_iterator::create(210, "iterators do not fit", this));
		}
		if (JSON_HEDLEY_UNLIKELY(!first.m_object->is_object()))
		{
			JSON_THROW(invalid_iterator::create(202, "iterators first and last must point to objects", this));
		}
		m_data.m_value.object->insert(first.m_it.object_iterator, last.m_it.object_iterator);
	}
	void update(const_reference j, bool merge_objects = false)
	{
		update(j.begin(), j.end(), merge_objects);
	}
	void update(const_iterator first, const_iterator last, bool merge_objects = false)
	{
		if (is_null())
		{
			m_data.m_type = value_t::object;
			m_data.m_value.object = create<object_t>();
			assert_invariant();
		}
		if (JSON_HEDLEY_UNLIKELY(!is_object()))
		{
			JSON_THROW(type_error::create(312, detail::concat("cannot use update() with ", type_name()), this));
		}
		if (JSON_HEDLEY_UNLIKELY(first.m_object != last.m_object))
		{
			JSON_THROW(invalid_iterator::create(210, "iterators do not fit", this));
		}
		if (JSON_HEDLEY_UNLIKELY(!first.m_object->is_object()))
		{
			JSON_THROW(type_error::create(312, detail::concat("cannot use update() with ", first.m_object->type_name()), first.m_object));
		}
		for (auto it = first; it != last; ++it)
		{
			if (merge_objects && it.value().is_object())
			{
				auto it2 = m_data.m_value.object->find(it.key());
				if (it2 != m_data.m_value.object->end())
				{
					it2->second.update(it.value(), true);
					continue;
				}
			}
			m_data.m_value.object->operator[](it.key()) = it.value();
#if JSON_DIAGNOSTICS
			m_data.m_value.object->operator[](it.key()).m_parent = this;
#endif
		}
	}
	void swap(reference other) noexcept (
		std::is_nothrow_move_constructible<value_t>::value&&
		std::is_nothrow_move_assignable<value_t>::value&&
		std::is_nothrow_move_constructible<json_value>::value&&
		std::is_nothrow_move_assignable<json_value>::value
		)
	{
		std::swap(m_data.m_type, other.m_data.m_type);
		std::swap(m_data.m_value, other.m_data.m_value);
		set_parents();
		other.set_parents();
		assert_invariant();
	}
	friend void swap(reference left, reference right) noexcept (
		std::is_nothrow_move_constructible<value_t>::value&&
		std::is_nothrow_move_assignable<value_t>::value&&
		std::is_nothrow_move_constructible<json_value>::value&&
		std::is_nothrow_move_assignable<json_value>::value
		)
	{
		left.swap(right);
	}
	void swap(array_t& other)
	{
		if (JSON_HEDLEY_LIKELY(is_array()))
		{
			using std::swap;
			swap(*(m_data.m_value.array), other);
		}
		else
		{
			JSON_THROW(type_error::create(310, detail::concat("cannot use swap(array_t&) with ", type_name()), this));
		}
	}
	void swap(object_t& other)
	{
		if (JSON_HEDLEY_LIKELY(is_object()))
		{
			using std::swap;
			swap(*(m_data.m_value.object), other);
		}
		else
		{
			JSON_THROW(type_error::create(310, detail::concat("cannot use swap(object_t&) with ", type_name()), this));
		}
	}
	void swap(string_t& other)
	{
		if (JSON_HEDLEY_LIKELY(is_string()))
		{
			using std::swap;
			swap(*(m_data.m_value.string), other);
		}
		else
		{
			JSON_THROW(type_error::create(310, detail::concat("cannot use swap(string_t&) with ", type_name()), this));
		}
	}
	void swap(binary_t& other)
	{
		if (JSON_HEDLEY_LIKELY(is_binary()))
		{
			using std::swap;
			swap(*(m_data.m_value.binary), other);
		}
		else
		{
			JSON_THROW(type_error::create(310, detail::concat("cannot use swap(binary_t&) with ", type_name()), this));
		}
	}
	void swap(typename binary_t::container_type& other)
	{
		if (JSON_HEDLEY_LIKELY(is_binary()))
		{
			using std::swap;
			swap(*(m_data.m_value.binary), other);
		}
		else
		{
			JSON_THROW(type_error::create(310, detail::concat("cannot use swap(binary_t::container_type&) with ", type_name()), this));
		}
	}
#define JSON_IMPLEMENT_OPERATOR(op, null_result, unordered_result, default_result)                       \
    const auto lhs_type = lhs.type();                                                                    \
    const auto rhs_type = rhs.type();                                                                    \
    \
    if (lhs_type == rhs_type)                                            \
    {                                                                                                    \
        switch (lhs_type)                                                                                \
        {                                                                                                \
            case value_t::array:                                                                         \
                return (*lhs.m_data.m_value.array) op (*rhs.m_data.m_value.array);                                     \
                \
            case value_t::object:                                                                        \
                return (*lhs.m_data.m_value.object) op (*rhs.m_data.m_value.object);                                   \
                \
            case value_t::null:                                                                          \
                return (null_result);                                                                    \
                \
            case value_t::string:                                                                        \
                return (*lhs.m_data.m_value.string) op (*rhs.m_data.m_value.string);                                   \
                \
            case value_t::boolean:                                                                       \
                return (lhs.m_data.m_value.boolean) op (rhs.m_data.m_value.boolean);                                   \
                \
            case value_t::number_integer:                                                                \
                return (lhs.m_data.m_value.number_integer) op (rhs.m_data.m_value.number_integer);                     \
                \
            case value_t::number_unsigned:                                                               \
                return (lhs.m_data.m_value.number_unsigned) op (rhs.m_data.m_value.number_unsigned);                   \
                \
            case value_t::number_float:                                                                  \
                return (lhs.m_data.m_value.number_float) op (rhs.m_data.m_value.number_float);                         \
                \
            case value_t::binary:                                                                        \
                return (*lhs.m_data.m_value.binary) op (*rhs.m_data.m_value.binary);                                   \
                \
            case value_t::discarded:                                                                     \
            default:                                                                                     \
                return (unordered_result);                                                               \
        }                                                                                                \
    }                                                                                                    \
    else if (lhs_type == value_t::number_integer && rhs_type == value_t::number_float)                   \
    {                                                                                                    \
        return static_cast<number_float_t>(lhs.m_data.m_value.number_integer) op rhs.m_data.m_value.number_float;      \
    }                                                                                                    \
    else if (lhs_type == value_t::number_float && rhs_type == value_t::number_integer)                   \
    {                                                                                                    \
        return lhs.m_data.m_value.number_float op static_cast<number_float_t>(rhs.m_data.m_value.number_integer);      \
    }                                                                                                    \
    else if (lhs_type == value_t::number_unsigned && rhs_type == value_t::number_float)                  \
    {                                                                                                    \
        return static_cast<number_float_t>(lhs.m_data.m_value.number_unsigned) op rhs.m_data.m_value.number_float;     \
    }                                                                                                    \
    else if (lhs_type == value_t::number_float && rhs_type == value_t::number_unsigned)                  \
    {                                                                                                    \
        return lhs.m_data.m_value.number_float op static_cast<number_float_t>(rhs.m_data.m_value.number_unsigned);     \
    }                                                                                                    \
    else if (lhs_type == value_t::number_unsigned && rhs_type == value_t::number_integer)                \
    {                                                                                                    \
        return static_cast<number_integer_t>(lhs.m_data.m_value.number_unsigned) op rhs.m_data.m_value.number_integer; \
    }                                                                                                    \
    else if (lhs_type == value_t::number_integer && rhs_type == value_t::number_unsigned)                \
    {                                                                                                    \
        return lhs.m_data.m_value.number_integer op static_cast<number_integer_t>(rhs.m_data.m_value.number_unsigned); \
    }                                                                                                    \
    else if(compares_unordered(lhs, rhs))\
    {\
        return (unordered_result);\
    }\
    \
    return (default_result);
JSON_PRIVATE_UNLESS_TESTED:
	static bool compares_unordered(const_reference lhs, const_reference rhs, bool inverse = false) noexcept
	{
		if ((lhs.is_number_float() && std::isnan(lhs.m_data.m_value.number_float) && rhs.is_number())
			|| (rhs.is_number_float() && std::isnan(rhs.m_data.m_value.number_float) && lhs.is_number()))
		{
			return true;
		}
#if JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
		return (lhs.is_discarded() || rhs.is_discarded()) && !inverse;
#else
		static_cast<void>(inverse);
		return lhs.is_discarded() || rhs.is_discarded();
#endif
	}
private:
	bool compares_unordered(const_reference rhs, bool inverse = false) const noexcept
	{
		return compares_unordered(*this, rhs, inverse);
	}
public:
#if JSON_HAS_THREE_WAY_COMPARISON
	bool operator==(const_reference rhs) const noexcept
	{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
		const_reference lhs = *this;
		JSON_IMPLEMENT_OPERATOR(== , true, false, false)
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	}
	template<typename ScalarType>
		requires std::is_scalar_v<ScalarType>
	bool operator==(ScalarType rhs) const noexcept
	{
		return *this == basic_json(rhs);
	}
	bool operator!=(const_reference rhs) const noexcept
	{
		if (compares_unordered(rhs, true))
		{
			return false;
		}
		return !operator==(rhs);
	}
	std::partial_ordering operator<=>(const_reference rhs) const noexcept
	{
		const_reference lhs = *this;
		JSON_IMPLEMENT_OPERATOR(<=> ,
			std::partial_ordering::equivalent,
			std::partial_ordering::unordered,
			lhs_type <=> rhs_type)
	}
	template<typename ScalarType>
		requires std::is_scalar_v<ScalarType>
	std::partial_ordering operator<=>(ScalarType rhs) const noexcept
	{
		return *this <=> basic_json(rhs);
	}
#if JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, undef JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON)
		bool operator<=(const_reference rhs) const noexcept
	{
		if (compares_unordered(rhs, true))
		{
			return false;
		}
		return !(rhs < *this);
	}
	template<typename ScalarType>
		requires std::is_scalar_v<ScalarType>
	bool operator<=(ScalarType rhs) const noexcept
	{
		return *this <= basic_json(rhs);
	}
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, undef JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON)
		bool operator>=(const_reference rhs) const noexcept
	{
		if (compares_unordered(rhs, true))
		{
			return false;
		}
		return !(*this < rhs);
	}
	template<typename ScalarType>
		requires std::is_scalar_v<ScalarType>
	bool operator>=(ScalarType rhs) const noexcept
	{
		return *this >= basic_json(rhs);
	}
#endif
#else
	friend bool operator==(const_reference lhs, const_reference rhs) noexcept
	{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
		JSON_IMPLEMENT_OPERATOR(== , true, false, false)
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator==(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs == basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator==(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) == rhs;
	}
	friend bool operator!=(const_reference lhs, const_reference rhs) noexcept
	{
		if (compares_unordered(lhs, rhs, true))
		{
			return false;
		}
		return !(lhs == rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator!=(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs != basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator!=(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) != rhs;
	}
	friend bool operator<(const_reference lhs, const_reference rhs) noexcept
	{
		JSON_IMPLEMENT_OPERATOR(< , false, false, operator<(lhs_type, rhs_type))
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator<(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs < basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator<(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) < rhs;
	}
	friend bool operator<=(const_reference lhs, const_reference rhs) noexcept
	{
		if (compares_unordered(lhs, rhs, true))
		{
			return false;
		}
		return !(rhs < lhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator<=(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs <= basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator<=(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) <= rhs;
	}
	friend bool operator>(const_reference lhs, const_reference rhs) noexcept
	{
		if (compares_unordered(lhs, rhs))
		{
			return false;
		}
		return !(lhs <= rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator>(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs > basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator>(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) > rhs;
	}
	friend bool operator>=(const_reference lhs, const_reference rhs) noexcept
	{
		if (compares_unordered(lhs, rhs, true))
		{
			return false;
		}
		return !(lhs < rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator>=(const_reference lhs, ScalarType rhs) noexcept
	{
		return lhs >= basic_json(rhs);
	}
	template<typename ScalarType, typename std::enable_if<
		std::is_scalar<ScalarType>::value, int>::type = 0>
	friend bool operator>=(ScalarType lhs, const_reference rhs) noexcept
	{
		return basic_json(lhs) >= rhs;
	}
#endif
#undef JSON_IMPLEMENT_OPERATOR
#ifndef JSON_NO_IO
	friend std::ostream& operator<<(std::ostream& o, const basic_json& j)
	{
		const bool pretty_print = o.width() > 0;
		const auto indentation = pretty_print ? o.width() : 0;
		o.width(0);
		serializer s(detail::output_adapter<char>(o), o.fill());
		s.dump(j, pretty_print, false, static_cast<unsigned int>(indentation));
		return o;
	}
	JSON_HEDLEY_DEPRECATED_FOR(3.0.0, operator<<(std::ostream&, const basic_json&))
		friend std::ostream& operator>>(const basic_json& j, std::ostream& o)
	{
		return o << j;
	}
#endif  
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json parse(InputType&& i,
			const parser_callback_t cb = nullptr,
			const bool allow_exceptions = true,
			const bool ignore_comments = false)
	{
		basic_json result;
		parser(detail::input_adapter(std::forward<InputType>(i)), cb, allow_exceptions, ignore_comments).parse(true, result);
		return result;
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json parse(IteratorType first,
			IteratorType last,
			const parser_callback_t cb = nullptr,
			const bool allow_exceptions = true,
			const bool ignore_comments = false)
	{
		basic_json result;
		parser(detail::input_adapter(std::move(first), std::move(last)), cb, allow_exceptions, ignore_comments).parse(true, result);
		return result;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, parse(ptr, ptr + len))
		static basic_json parse(detail::span_input_adapter&& i,
			const parser_callback_t cb = nullptr,
			const bool allow_exceptions = true,
			const bool ignore_comments = false)
	{
		basic_json result;
		parser(i.get(), cb, allow_exceptions, ignore_comments).parse(true, result);
		return result;
	}
	template<typename InputType>
	static bool accept(InputType&& i,
		const bool ignore_comments = false)
	{
		return parser(detail::input_adapter(std::forward<InputType>(i)), nullptr, false, ignore_comments).accept(true);
	}
	template<typename IteratorType>
	static bool accept(IteratorType first, IteratorType last,
		const bool ignore_comments = false)
	{
		return parser(detail::input_adapter(std::move(first), std::move(last)), nullptr, false, ignore_comments).accept(true);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, accept(ptr, ptr + len))
		static bool accept(detail::span_input_adapter&& i,
			const bool ignore_comments = false)
	{
		return parser(i.get(), nullptr, false, ignore_comments).accept(true);
	}
	template <typename InputType, typename SAX>
	JSON_HEDLEY_NON_NULL(2)
		static bool sax_parse(InputType&& i, SAX* sax,
			input_format_t format = input_format_t::json,
			const bool strict = true,
			const bool ignore_comments = false)
	{
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		return format == input_format_t::json
			? parser(std::move(ia), nullptr, true, ignore_comments).sax_parse(sax, strict)
			: detail::binary_reader<basic_json, decltype(ia), SAX>(std::move(ia), format).sax_parse(format, sax, strict);
	}
	template<class IteratorType, class SAX>
	JSON_HEDLEY_NON_NULL(3)
		static bool sax_parse(IteratorType first, IteratorType last, SAX* sax,
			input_format_t format = input_format_t::json,
			const bool strict = true,
			const bool ignore_comments = false)
	{
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		return format == input_format_t::json
			? parser(std::move(ia), nullptr, true, ignore_comments).sax_parse(sax, strict)
			: detail::binary_reader<basic_json, decltype(ia), SAX>(std::move(ia), format).sax_parse(format, sax, strict);
	}
	template <typename SAX>
	JSON_HEDLEY_DEPRECATED_FOR(3.8.0, sax_parse(ptr, ptr + len, ...))
		JSON_HEDLEY_NON_NULL(2)
		static bool sax_parse(detail::span_input_adapter&& i, SAX* sax,
			input_format_t format = input_format_t::json,
			const bool strict = true,
			const bool ignore_comments = false)
	{
		auto ia = i.get();
		return format == input_format_t::json
			? parser(std::move(ia), nullptr, true, ignore_comments).sax_parse(sax, strict)
			: detail::binary_reader<basic_json, decltype(ia), SAX>(std::move(ia), format).sax_parse(format, sax, strict);
	}
#ifndef JSON_NO_IO
	JSON_HEDLEY_DEPRECATED_FOR(3.0.0, operator>>(std::istream&, basic_json&))
		friend std::istream& operator<<(basic_json& j, std::istream& i)
	{
		return operator>>(i, j);
	}
	friend std::istream& operator>>(std::istream& i, basic_json& j)
	{
		parser(detail::input_adapter(i)).parse(false, j);
		return i;
	}
#endif  
	JSON_HEDLEY_RETURNS_NON_NULL
		const char* type_name() const noexcept
	{
		switch (m_data.m_type)
		{
		case value_t::null:
			return "null";
		case value_t::object:
			return "object";
		case value_t::array:
			return "array";
		case value_t::string:
			return "string";
		case value_t::boolean:
			return "boolean";
		case value_t::binary:
			return "binary";
		case value_t::discarded:
			return "discarded";
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		default:
			return "number";
		}
	}
JSON_PRIVATE_UNLESS_TESTED:
	struct data
	{
		value_t m_type = value_t::null;
		json_value m_value = {};
		data(const value_t v)
			: m_type(v), m_value(v)
		{
		}
		data(size_type cnt, const basic_json& val)
			: m_type(value_t::array)
		{
			m_value.array = create<array_t>(cnt, val);
		}
		data() noexcept = default;
		data(data&&) noexcept = default;
		data(const data&) noexcept = delete;
		data& operator=(data&&) noexcept = delete;
		data& operator=(const data&) noexcept = delete;
		~data() noexcept
		{
			m_value.destroy(m_type);
		}
	};
	data m_data = {};
#if JSON_DIAGNOSTICS
	basic_json* m_parent = nullptr;
#endif
public:
	static std::vector<std::uint8_t> to_cbor(const basic_json& j)
	{
		std::vector<std::uint8_t> result;
		to_cbor(j, result);
		return result;
	}
	static void to_cbor(const basic_json& j, detail::output_adapter<std::uint8_t> o)
	{
		binary_writer<std::uint8_t>(o).write_cbor(j);
	}
	static void to_cbor(const basic_json& j, detail::output_adapter<char> o)
	{
		binary_writer<char>(o).write_cbor(j);
	}
	static std::vector<std::uint8_t> to_msgpack(const basic_json& j)
	{
		std::vector<std::uint8_t> result;
		to_msgpack(j, result);
		return result;
	}
	static void to_msgpack(const basic_json& j, detail::output_adapter<std::uint8_t> o)
	{
		binary_writer<std::uint8_t>(o).write_msgpack(j);
	}
	static void to_msgpack(const basic_json& j, detail::output_adapter<char> o)
	{
		binary_writer<char>(o).write_msgpack(j);
	}
	static std::vector<std::uint8_t> to_ubjson(const basic_json& j,
		const bool use_size = false,
		const bool use_type = false)
	{
		std::vector<std::uint8_t> result;
		to_ubjson(j, result, use_size, use_type);
		return result;
	}
	static void to_ubjson(const basic_json& j, detail::output_adapter<std::uint8_t> o,
		const bool use_size = false, const bool use_type = false)
	{
		binary_writer<std::uint8_t>(o).write_ubjson(j, use_size, use_type);
	}
	static void to_ubjson(const basic_json& j, detail::output_adapter<char> o,
		const bool use_size = false, const bool use_type = false)
	{
		binary_writer<char>(o).write_ubjson(j, use_size, use_type);
	}
	static std::vector<std::uint8_t> to_bjdata(const basic_json& j,
		const bool use_size = false,
		const bool use_type = false)
	{
		std::vector<std::uint8_t> result;
		to_bjdata(j, result, use_size, use_type);
		return result;
	}
	static void to_bjdata(const basic_json& j, detail::output_adapter<std::uint8_t> o,
		const bool use_size = false, const bool use_type = false)
	{
		binary_writer<std::uint8_t>(o).write_ubjson(j, use_size, use_type, true, true);
	}
	static void to_bjdata(const basic_json& j, detail::output_adapter<char> o,
		const bool use_size = false, const bool use_type = false)
	{
		binary_writer<char>(o).write_ubjson(j, use_size, use_type, true, true);
	}
	static std::vector<std::uint8_t> to_bson(const basic_json& j)
	{
		std::vector<std::uint8_t> result;
		to_bson(j, result);
		return result;
	}
	static void to_bson(const basic_json& j, detail::output_adapter<std::uint8_t> o)
	{
		binary_writer<std::uint8_t>(o).write_bson(j);
	}
	static void to_bson(const basic_json& j, detail::output_adapter<char> o)
	{
		binary_writer<char>(o).write_bson(j);
	}
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_cbor(InputType&& i,
			const bool strict = true,
			const bool allow_exceptions = true,
			const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::cbor).sax_parse(input_format_t::cbor, &sdp, strict, tag_handler);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_cbor(IteratorType first, IteratorType last,
			const bool strict = true,
			const bool allow_exceptions = true,
			const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::cbor).sax_parse(input_format_t::cbor, &sdp, strict, tag_handler);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename T>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_cbor(ptr, ptr + len))
		static basic_json from_cbor(const T* ptr, std::size_t len,
			const bool strict = true,
			const bool allow_exceptions = true,
			const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error)
	{
		return from_cbor(ptr, ptr + len, strict, allow_exceptions, tag_handler);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_cbor(ptr, ptr + len))
		static basic_json from_cbor(detail::span_input_adapter&& i,
			const bool strict = true,
			const bool allow_exceptions = true,
			const cbor_tag_handler_t tag_handler = cbor_tag_handler_t::error)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = i.get();
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::cbor).sax_parse(input_format_t::cbor, &sdp, strict, tag_handler);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_msgpack(InputType&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::msgpack).sax_parse(input_format_t::msgpack, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_msgpack(IteratorType first, IteratorType last,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::msgpack).sax_parse(input_format_t::msgpack, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename T>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_msgpack(ptr, ptr + len))
		static basic_json from_msgpack(const T* ptr, std::size_t len,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		return from_msgpack(ptr, ptr + len, strict, allow_exceptions);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_msgpack(ptr, ptr + len))
		static basic_json from_msgpack(detail::span_input_adapter&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = i.get();
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::msgpack).sax_parse(input_format_t::msgpack, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_ubjson(InputType&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::ubjson).sax_parse(input_format_t::ubjson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_ubjson(IteratorType first, IteratorType last,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::ubjson).sax_parse(input_format_t::ubjson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename T>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_ubjson(ptr, ptr + len))
		static basic_json from_ubjson(const T* ptr, std::size_t len,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		return from_ubjson(ptr, ptr + len, strict, allow_exceptions);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_ubjson(ptr, ptr + len))
		static basic_json from_ubjson(detail::span_input_adapter&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = i.get();
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::ubjson).sax_parse(input_format_t::ubjson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_bjdata(InputType&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::bjdata).sax_parse(input_format_t::bjdata, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_bjdata(IteratorType first, IteratorType last,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::bjdata).sax_parse(input_format_t::bjdata, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename InputType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_bson(InputType&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::forward<InputType>(i));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::bson).sax_parse(input_format_t::bson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename IteratorType>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json from_bson(IteratorType first, IteratorType last,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = detail::input_adapter(std::move(first), std::move(last));
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::bson).sax_parse(input_format_t::bson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	template<typename T>
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_bson(ptr, ptr + len))
		static basic_json from_bson(const T* ptr, std::size_t len,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		return from_bson(ptr, ptr + len, strict, allow_exceptions);
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		JSON_HEDLEY_DEPRECATED_FOR(3.8.0, from_bson(ptr, ptr + len))
		static basic_json from_bson(detail::span_input_adapter&& i,
			const bool strict = true,
			const bool allow_exceptions = true)
	{
		basic_json result;
		detail::json_sax_dom_parser<basic_json> sdp(result, allow_exceptions);
		auto ia = i.get();
		const bool res = binary_reader<decltype(ia)>(std::move(ia), input_format_t::bson).sax_parse(input_format_t::bson, &sdp, strict);
		return res ? result : basic_json(value_t::discarded);
	}
	reference operator[](const json_pointer& ptr)
	{
		return ptr.get_unchecked(this);
	}
	template<typename BasicJsonType, detail::enable_if_t<detail::is_basic_json<BasicJsonType>::value, int> = 0>
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		reference operator[](const ::JsonLib::json_pointer<BasicJsonType>& ptr)
	{
		return ptr.get_unchecked(this);
	}
	const_reference operator[](const json_pointer& ptr) const
	{
		return ptr.get_unchecked(this);
	}
	template<typename BasicJsonType, detail::enable_if_t<detail::is_basic_json<BasicJsonType>::value, int> = 0>
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		const_reference operator[](const ::JsonLib::json_pointer<BasicJsonType>& ptr) const
	{
		return ptr.get_unchecked(this);
	}
	reference at(const json_pointer& ptr)
	{
		return ptr.get_checked(this);
	}
	template<typename BasicJsonType, detail::enable_if_t<detail::is_basic_json<BasicJsonType>::value, int> = 0>
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		reference at(const ::JsonLib::json_pointer<BasicJsonType>& ptr)
	{
		return ptr.get_checked(this);
	}
	const_reference at(const json_pointer& ptr) const
	{
		return ptr.get_checked(this);
	}
	template<typename BasicJsonType, detail::enable_if_t<detail::is_basic_json<BasicJsonType>::value, int> = 0>
	JSON_HEDLEY_DEPRECATED_FOR(3.11.0, basic_json::json_pointer or JsonLib::json_pointer<basic_json::string_t>)
		const_reference at(const ::JsonLib::json_pointer<BasicJsonType>& ptr) const
	{
		return ptr.get_checked(this);
	}
	basic_json flatten() const
	{
		basic_json result(value_t::object);
		json_pointer::flatten("", *this, result);
		return result;
	}
	basic_json unflatten() const
	{
		return json_pointer::unflatten(*this);
	}
	void patch_inplace(const basic_json& json_patch)
	{
		basic_json& result = *this;
		enum class patch_operations { add, remove, replace, move, copy, test, invalid };
		const auto get_op = [](const std::string& op)
			{
				if (op == "add")
				{
					return patch_operations::add;
				}
				if (op == "remove")
				{
					return patch_operations::remove;
				}
				if (op == "replace")
				{
					return patch_operations::replace;
				}
				if (op == "move")
				{
					return patch_operations::move;
				}
				if (op == "copy")
				{
					return patch_operations::copy;
				}
				if (op == "test")
				{
					return patch_operations::test;
				}
				return patch_operations::invalid;
			};
		const auto operation_add = [&result](json_pointer& ptr, basic_json val)
			{
				if (ptr.empty())
				{
					result = val;
					return;
				}
				json_pointer const top_pointer = ptr.top();
				if (top_pointer != ptr)
				{
					result.at(top_pointer);
				}
				const auto last_path = ptr.back();
				ptr.pop_back();
				basic_json& parent = result.at(ptr);
				switch (parent.m_data.m_type)
				{
				case value_t::null:
				case value_t::object:
				{
					parent[last_path] = val;
					break;
				}
				case value_t::array:
				{
					if (last_path == "-")
					{
						parent.push_back(val);
					}
					else
					{
						const auto idx = json_pointer::template array_index<basic_json_t>(last_path);
						if (JSON_HEDLEY_UNLIKELY(idx > parent.size()))
						{
							JSON_THROW(out_of_range::create(401, detail::concat("array index ", std::to_string(idx), " is out of range"), &parent));
						}
						parent.insert(parent.begin() + static_cast<difference_type>(idx), val);
					}
					break;
				}
				case value_t::string:
				case value_t::boolean:
				case value_t::number_integer:
				case value_t::number_unsigned:
				case value_t::number_float:
				case value_t::binary:
				case value_t::discarded:
				default:
					JSON_ASSERT(false);
				}
			};
		const auto operation_remove = [this, &result](json_pointer& ptr)
			{
				const auto last_path = ptr.back();
				ptr.pop_back();
				basic_json& parent = result.at(ptr);
				if (parent.is_object())
				{
					auto it = parent.find(last_path);
					if (JSON_HEDLEY_LIKELY(it != parent.end()))
					{
						parent.erase(it);
					}
					else
					{
						JSON_THROW(out_of_range::create(403, detail::concat("key '", last_path, "' not found"), this));
					}
				}
				else if (parent.is_array())
				{
					parent.erase(json_pointer::template array_index<basic_json_t>(last_path));
				}
			};
		if (JSON_HEDLEY_UNLIKELY(!json_patch.is_array()))
		{
			JSON_THROW(parse_error::create(104, 0, "JSON patch must be an array of objects", &json_patch));
		}
		for (const auto& val : json_patch)
		{
			const auto get_value = [&val](const std::string& op,
				const std::string& member,
				bool string_type) -> basic_json&
				{
					auto it = val.m_data.m_value.object->find(member);
					const auto error_msg = (op == "op") ? "operation" : detail::concat("operation '", op, '\'');
					if (JSON_HEDLEY_UNLIKELY(it == val.m_data.m_value.object->end()))
					{
						JSON_THROW(parse_error::create(105, 0, detail::concat(error_msg, " must have member '", member, "'"), &val));
					}
					if (JSON_HEDLEY_UNLIKELY(string_type && !it->second.is_string()))
					{
						JSON_THROW(parse_error::create(105, 0, detail::concat(error_msg, " must have string member '", member, "'"), &val));
					}
					return it->second;
				};
			if (JSON_HEDLEY_UNLIKELY(!val.is_object()))
			{
				JSON_THROW(parse_error::create(104, 0, "JSON patch must be an array of objects", &val));
			}
			const auto op = get_value("op", "op", true).template get<std::string>();
			const auto path = get_value(op, "path", true).template get<std::string>();
			json_pointer ptr(path);
			switch (get_op(op))
			{
			case patch_operations::add:
			{
				operation_add(ptr, get_value("add", "value", false));
				break;
			}
			case patch_operations::remove:
			{
				operation_remove(ptr);
				break;
			}
			case patch_operations::replace:
			{
				result.at(ptr) = get_value("replace", "value", false);
				break;
			}
			case patch_operations::move:
			{
				const auto from_path = get_value("move", "from", true).template get<std::string>();
				json_pointer from_ptr(from_path);
				basic_json const v = result.at(from_ptr);
				operation_remove(from_ptr);
				operation_add(ptr, v);
				break;
			}
			case patch_operations::copy:
			{
				const auto from_path = get_value("copy", "from", true).template get<std::string>();
				const json_pointer from_ptr(from_path);
				basic_json const v = result.at(from_ptr);
				operation_add(ptr, v);
				break;
			}
			case patch_operations::test:
			{
				bool success = false;
				JSON_TRY
				{
					success = (result.at(ptr) == get_value("test", "value", false));
				}
					JSON_INTERNAL_CATCH(out_of_range&)
				{
				}
				if (JSON_HEDLEY_UNLIKELY(!success))
				{
					JSON_THROW(other_error::create(501, detail::concat("unsuccessful: ", val.dump()), &val));
				}
				break;
			}
			case patch_operations::invalid:
			default:
			{
				JSON_THROW(parse_error::create(105, 0, detail::concat("operation value '", op, "' is invalid"), &val));
			}
			}
		}
	}
	basic_json patch(const basic_json& json_patch) const
	{
		basic_json result = *this;
		result.patch_inplace(json_patch);
		return result;
	}
	JSON_HEDLEY_WARN_UNUSED_RESULT
		static basic_json diff(const basic_json& source, const basic_json& target,
			const std::string& path = "")
	{
		basic_json result(value_t::array);
		if (source == target)
		{
			return result;
		}
		if (source.type() != target.type())
		{
			result.push_back(
				{
					{"op", "replace"}, {"path", path}, {"value", target}
				});
			return result;
		}
		switch (source.type())
		{
		case value_t::array:
		{
			std::size_t i = 0;
			while (i < source.size() && i < target.size())
			{
				auto temp_diff = diff(source[i], target[i], detail::concat(path, '/', std::to_string(i)));
				result.insert(result.end(), temp_diff.begin(), temp_diff.end());
				++i;
			}
			const auto end_index = static_cast<difference_type>(result.size());
			while (i < source.size())
			{
				result.insert(result.begin() + end_index, object(
					{
						{"op", "remove"},
						{"path", detail::concat(path, '/', std::to_string(i))}
					}));
				++i;
			}
			while (i < target.size())
			{
				result.push_back(
					{
						{"op", "add"},
						{"path", detail::concat(path, "/-")},
						{"value", target[i]}
					});
				++i;
			}
			break;
		}
		case value_t::object:
		{
			for (auto it = source.cbegin(); it != source.cend(); ++it)
			{
				const auto path_key = detail::concat(path, '/', detail::escape(it.key()));
				if (target.find(it.key()) != target.end())
				{
					auto temp_diff = diff(it.value(), target[it.key()], path_key);
					result.insert(result.end(), temp_diff.begin(), temp_diff.end());
				}
				else
				{
					result.push_back(object(
						{
							{"op", "remove"}, {"path", path_key}
						}));
				}
			}
			for (auto it = target.cbegin(); it != target.cend(); ++it)
			{
				if (source.find(it.key()) == source.end())
				{
					const auto path_key = detail::concat(path, '/', detail::escape(it.key()));
					result.push_back(
						{
							{"op", "add"}, {"path", path_key},
							{"value", it.value()}
						});
				}
			}
			break;
		}
		case value_t::null:
		case value_t::string:
		case value_t::boolean:
		case value_t::number_integer:
		case value_t::number_unsigned:
		case value_t::number_float:
		case value_t::binary:
		case value_t::discarded:
		default:
		{
			result.push_back(
				{
					{"op", "replace"}, {"path", path}, {"value", target}
				});
			break;
		}
		}
		return result;
	}
	void merge_patch(const basic_json& apply_patch)
	{
		if (apply_patch.is_object())
		{
			if (!is_object())
			{
				*this = object();
			}
			for (auto it = apply_patch.begin(); it != apply_patch.end(); ++it)
			{
				if (it.value().is_null())
				{
					erase(it.key());
				}
				else
				{
					operator[](it.key()).merge_patch(it.value());
				}
			}
		}
		else
		{
			*this = apply_patch;
		}
	}
};
NLOHMANN_BASIC_JSON_TPL_DECLARATION
std::string to_string(const NLOHMANN_BASIC_JSON_TPL& j)
{
	return j.dump();
}
inline namespace literals
{
	inline namespace json_literals
	{
		JSON_HEDLEY_NON_NULL(1)
#if !defined(JSON_HEDLEY_GCC_VERSION) || JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0)
			inline JsonLib::json operator ""_json(const char* s, std::size_t n)
#else
			inline JsonLib::json operator "" _json(const char* s, std::size_t n)
#endif
		{
			return JsonLib::json::parse(s, s + n);
		}
		JSON_HEDLEY_NON_NULL(1)
#if !defined(JSON_HEDLEY_GCC_VERSION) || JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0)
			inline JsonLib::json::json_pointer operator ""_json_pointer(const char* s, std::size_t n)
#else
			inline JsonLib::json::json_pointer operator "" _json_pointer(const char* s, std::size_t n)
#endif
		{
			return JsonLib::json::json_pointer(std::string(s, n));
		}
	}
}
NLOHMANN_JSON_NAMESPACE_END
namespace std
{
	NLOHMANN_BASIC_JSON_TPL_DECLARATION
		struct hash<JsonLib::NLOHMANN_BASIC_JSON_TPL>
	{
		std::size_t operator()(const JsonLib::NLOHMANN_BASIC_JSON_TPL& j) const
		{
			return JsonLib::detail::hash(j);
		}
	};
	template<>
	struct less< ::JsonLib::detail::value_t>
	{
		bool operator()(::JsonLib::detail::value_t lhs,
			::JsonLib::detail::value_t rhs) const noexcept
		{
#if JSON_HAS_THREE_WAY_COMPARISON
			return std::is_lt(lhs <=> rhs);
#else
			return ::JsonLib::detail::operator<(lhs, rhs);
#endif
		}
	};
#ifndef JSON_HAS_CPP_20
	NLOHMANN_BASIC_JSON_TPL_DECLARATION
		inline void swap(JsonLib::NLOHMANN_BASIC_JSON_TPL& j1, JsonLib::NLOHMANN_BASIC_JSON_TPL& j2) noexcept(
			is_nothrow_move_constructible<JsonLib::NLOHMANN_BASIC_JSON_TPL>::value&&
			is_nothrow_move_assignable<JsonLib::NLOHMANN_BASIC_JSON_TPL>::value)
	{
		j1.swap(j2);
	}
#endif
}
#if JSON_USE_GLOBAL_UDLS
#if !defined(JSON_HEDLEY_GCC_VERSION) || JSON_HEDLEY_GCC_VERSION_CHECK(4,9,0)
using JsonLib::literals::json_literals::operator ""_json;
using JsonLib::literals::json_literals::operator ""_json_pointer;
#else
using JsonLib::literals::json_literals::operator "" _json;
using JsonLib::literals::json_literals::operator "" _json_pointer;
#endif
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#undef JSON_ASSERT
#undef JSON_INTERNAL_CATCH
#undef JSON_THROW
#undef JSON_PRIVATE_UNLESS_TESTED
#undef NLOHMANN_BASIC_JSON_TPL_DECLARATION
#undef NLOHMANN_BASIC_JSON_TPL
#undef JSON_EXPLICIT
#undef NLOHMANN_CAN_CALL_STD_FUNC_IMPL
#undef JSON_INLINE_VARIABLE
#undef JSON_NO_UNIQUE_ADDRESS
#undef JSON_DISABLE_ENUM_SERIALIZATION
#undef JSON_USE_GLOBAL_UDLS
#ifndef JSON_TEST_KEEP_MACROS
#undef JSON_CATCH
#undef JSON_TRY
#undef JSON_HAS_CPP_11
#undef JSON_HAS_CPP_14
#undef JSON_HAS_CPP_17
#undef JSON_HAS_CPP_20
#undef JSON_HAS_FILESYSTEM
#undef JSON_HAS_EXPERIMENTAL_FILESYSTEM
#undef JSON_HAS_THREE_WAY_COMPARISON
#undef JSON_HAS_RANGES
#undef JSON_HAS_STATIC_RTTI
#undef JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
#endif
#undef JSON_HEDLEY_ALWAYS_INLINE
#undef JSON_HEDLEY_ARM_VERSION
#undef JSON_HEDLEY_ARM_VERSION_CHECK
#undef JSON_HEDLEY_ARRAY_PARAM
#undef JSON_HEDLEY_ASSUME
#undef JSON_HEDLEY_BEGIN_C_DECLS
#undef JSON_HEDLEY_CLANG_HAS_ATTRIBUTE
#undef JSON_HEDLEY_CLANG_HAS_BUILTIN
#undef JSON_HEDLEY_CLANG_HAS_CPP_ATTRIBUTE
#undef JSON_HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE
#undef JSON_HEDLEY_CLANG_HAS_EXTENSION
#undef JSON_HEDLEY_CLANG_HAS_FEATURE
#undef JSON_HEDLEY_CLANG_HAS_WARNING
#undef JSON_HEDLEY_COMPCERT_VERSION
#undef JSON_HEDLEY_COMPCERT_VERSION_CHECK
#undef JSON_HEDLEY_CONCAT
#undef JSON_HEDLEY_CONCAT3
#undef JSON_HEDLEY_CONCAT3_EX
#undef JSON_HEDLEY_CONCAT_EX
#undef JSON_HEDLEY_CONST
#undef JSON_HEDLEY_CONSTEXPR
#undef JSON_HEDLEY_CONST_CAST
#undef JSON_HEDLEY_CPP_CAST
#undef JSON_HEDLEY_CRAY_VERSION
#undef JSON_HEDLEY_CRAY_VERSION_CHECK
#undef JSON_HEDLEY_C_DECL
#undef JSON_HEDLEY_DEPRECATED
#undef JSON_HEDLEY_DEPRECATED_FOR
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#undef JSON_HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#undef JSON_HEDLEY_DIAGNOSTIC_POP
#undef JSON_HEDLEY_DIAGNOSTIC_PUSH
#undef JSON_HEDLEY_DMC_VERSION
#undef JSON_HEDLEY_DMC_VERSION_CHECK
#undef JSON_HEDLEY_EMPTY_BASES
#undef JSON_HEDLEY_EMSCRIPTEN_VERSION
#undef JSON_HEDLEY_EMSCRIPTEN_VERSION_CHECK
#undef JSON_HEDLEY_END_C_DECLS
#undef JSON_HEDLEY_FLAGS
#undef JSON_HEDLEY_FLAGS_CAST
#undef JSON_HEDLEY_GCC_HAS_ATTRIBUTE
#undef JSON_HEDLEY_GCC_HAS_BUILTIN
#undef JSON_HEDLEY_GCC_HAS_CPP_ATTRIBUTE
#undef JSON_HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE
#undef JSON_HEDLEY_GCC_HAS_EXTENSION
#undef JSON_HEDLEY_GCC_HAS_FEATURE
#undef JSON_HEDLEY_GCC_HAS_WARNING
#undef JSON_HEDLEY_GCC_NOT_CLANG_VERSION_CHECK
#undef JSON_HEDLEY_GCC_VERSION
#undef JSON_HEDLEY_GCC_VERSION_CHECK
#undef JSON_HEDLEY_GNUC_HAS_ATTRIBUTE
#undef JSON_HEDLEY_GNUC_HAS_BUILTIN
#undef JSON_HEDLEY_GNUC_HAS_CPP_ATTRIBUTE
#undef JSON_HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE
#undef JSON_HEDLEY_GNUC_HAS_EXTENSION
#undef JSON_HEDLEY_GNUC_HAS_FEATURE
#undef JSON_HEDLEY_GNUC_HAS_WARNING
#undef JSON_HEDLEY_GNUC_VERSION
#undef JSON_HEDLEY_GNUC_VERSION_CHECK
#undef JSON_HEDLEY_HAS_ATTRIBUTE
#undef JSON_HEDLEY_HAS_BUILTIN
#undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE
#undef JSON_HEDLEY_HAS_CPP_ATTRIBUTE_NS
#undef JSON_HEDLEY_HAS_DECLSPEC_ATTRIBUTE
#undef JSON_HEDLEY_HAS_EXTENSION
#undef JSON_HEDLEY_HAS_FEATURE
#undef JSON_HEDLEY_HAS_WARNING
#undef JSON_HEDLEY_IAR_VERSION
#undef JSON_HEDLEY_IAR_VERSION_CHECK
#undef JSON_HEDLEY_IBM_VERSION
#undef JSON_HEDLEY_IBM_VERSION_CHECK
#undef JSON_HEDLEY_IMPORT
#undef JSON_HEDLEY_INLINE
#undef JSON_HEDLEY_INTEL_CL_VERSION
#undef JSON_HEDLEY_INTEL_CL_VERSION_CHECK
#undef JSON_HEDLEY_INTEL_VERSION
#undef JSON_HEDLEY_INTEL_VERSION_CHECK
#undef JSON_HEDLEY_IS_CONSTANT
#undef JSON_HEDLEY_IS_CONSTEXPR_
#undef JSON_HEDLEY_LIKELY
#undef JSON_HEDLEY_MALLOC
#undef JSON_HEDLEY_MCST_LCC_VERSION
#undef JSON_HEDLEY_MCST_LCC_VERSION_CHECK
#undef JSON_HEDLEY_MESSAGE
#undef JSON_HEDLEY_MSVC_VERSION
#undef JSON_HEDLEY_MSVC_VERSION_CHECK
#undef JSON_HEDLEY_NEVER_INLINE
#undef JSON_HEDLEY_NON_NULL
#undef JSON_HEDLEY_NO_ESCAPE
#undef JSON_HEDLEY_NO_RETURN
#undef JSON_HEDLEY_NO_THROW
#undef JSON_HEDLEY_NULL
#undef JSON_HEDLEY_PELLES_VERSION
#undef JSON_HEDLEY_PELLES_VERSION_CHECK
#undef JSON_HEDLEY_PGI_VERSION
#undef JSON_HEDLEY_PGI_VERSION_CHECK
#undef JSON_HEDLEY_PREDICT
#undef JSON_HEDLEY_PRINTF_FORMAT
#undef JSON_HEDLEY_PRIVATE
#undef JSON_HEDLEY_PUBLIC
#undef JSON_HEDLEY_PURE
#undef JSON_HEDLEY_REINTERPRET_CAST
#undef JSON_HEDLEY_REQUIRE
#undef JSON_HEDLEY_REQUIRE_CONSTEXPR
#undef JSON_HEDLEY_REQUIRE_MSG
#undef JSON_HEDLEY_RESTRICT
#undef JSON_HEDLEY_RETURNS_NON_NULL
#undef JSON_HEDLEY_SENTINEL
#undef JSON_HEDLEY_STATIC_ASSERT
#undef JSON_HEDLEY_STATIC_CAST
#undef JSON_HEDLEY_STRINGIFY
#undef JSON_HEDLEY_STRINGIFY_EX
#undef JSON_HEDLEY_SUNPRO_VERSION
#undef JSON_HEDLEY_SUNPRO_VERSION_CHECK
#undef JSON_HEDLEY_TINYC_VERSION
#undef JSON_HEDLEY_TINYC_VERSION_CHECK
#undef JSON_HEDLEY_TI_ARMCL_VERSION
#undef JSON_HEDLEY_TI_ARMCL_VERSION_CHECK
#undef JSON_HEDLEY_TI_CL2000_VERSION
#undef JSON_HEDLEY_TI_CL2000_VERSION_CHECK
#undef JSON_HEDLEY_TI_CL430_VERSION
#undef JSON_HEDLEY_TI_CL430_VERSION_CHECK
#undef JSON_HEDLEY_TI_CL6X_VERSION
#undef JSON_HEDLEY_TI_CL6X_VERSION_CHECK
#undef JSON_HEDLEY_TI_CL7X_VERSION
#undef JSON_HEDLEY_TI_CL7X_VERSION_CHECK
#undef JSON_HEDLEY_TI_CLPRU_VERSION
#undef JSON_HEDLEY_TI_CLPRU_VERSION_CHECK
#undef JSON_HEDLEY_TI_VERSION
#undef JSON_HEDLEY_TI_VERSION_CHECK
#undef JSON_HEDLEY_UNAVAILABLE
#undef JSON_HEDLEY_UNLIKELY
#undef JSON_HEDLEY_UNPREDICTABLE
#undef JSON_HEDLEY_UNREACHABLE
#undef JSON_HEDLEY_UNREACHABLE_RETURN
#undef JSON_HEDLEY_VERSION
#undef JSON_HEDLEY_VERSION_DECODE_MAJOR
#undef JSON_HEDLEY_VERSION_DECODE_MINOR
#undef JSON_HEDLEY_VERSION_DECODE_REVISION
#undef JSON_HEDLEY_VERSION_ENCODE
#undef JSON_HEDLEY_WARNING
#undef JSON_HEDLEY_WARN_UNUSED_RESULT
#undef JSON_HEDLEY_WARN_UNUSED_RESULT_MSG
#undef JSON_HEDLEY_FALL_THROUGH
#endif  

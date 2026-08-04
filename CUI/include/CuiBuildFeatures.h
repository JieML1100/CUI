#pragma once

// Production is the default. Design-time hosts opt in explicitly and must
// link the matching CUIDesignCore library so Control's ABI stays consistent.
// CUI_ENABLE_DYNAMIC_XAML is derived from the runtime flavor so callers cannot
// independently select an ABI-affecting feature set.
#ifndef CUI_RUNTIME_FLAVOR_DESIGN
#define CUI_RUNTIME_FLAVOR_DESIGN 0
#endif

#if CUI_RUNTIME_FLAVOR_DESIGN != 0 && CUI_RUNTIME_FLAVOR_DESIGN != 1
#error CUI_RUNTIME_FLAVOR_DESIGN must be 0 or 1
#endif

#ifdef CUI_ENABLE_DYNAMIC_XAML
#if CUI_ENABLE_DYNAMIC_XAML != CUI_RUNTIME_FLAVOR_DESIGN
#error CUI_ENABLE_DYNAMIC_XAML cannot differ from CUI_RUNTIME_FLAVOR_DESIGN
#endif
#else
#define CUI_ENABLE_DYNAMIC_XAML CUI_RUNTIME_FLAVOR_DESIGN
#endif

// Dependency-property presentation metadata belongs to the designer runtime.
// Keep it tied to the same ABI flavor as dynamic XAML so a consumer cannot
// independently select a public DependencyPropertyMetadata layout.
#ifdef CUI_ENABLE_DESIGN_METADATA
#if CUI_ENABLE_DESIGN_METADATA != CUI_RUNTIME_FLAVOR_DESIGN
#error CUI_ENABLE_DESIGN_METADATA cannot differ from CUI_RUNTIME_FLAVOR_DESIGN
#endif
#else
#define CUI_ENABLE_DESIGN_METADATA CUI_RUNTIME_FLAVOR_DESIGN
#endif

// Design registration blocks can keep their declarations next to the runtime
// property while disappearing before Production C++ parsing/code generation.
#if CUI_ENABLE_DESIGN_METADATA
#define CUI_DESIGN_METADATA_ONLY(...) __VA_ARGS__
#define CUI_DESIGN_METADATA_ARGUMENTS(...) , __VA_ARGS__
#else
#define CUI_DESIGN_METADATA_ONLY(...)
#define CUI_DESIGN_METADATA_ARGUMENTS(...)
#endif

// Control has a different object layout in the two flavors. Make an accidental
// header/library mismatch a deterministic linker error instead of silent ABI
// corruption. MSVC carries this record through both objects and static libs.
#if defined(_MSC_VER)
#if CUI_RUNTIME_FLAVOR_DESIGN
#pragma detect_mismatch("CUI_RUNTIME_FLAVOR", "Design")
#else
#pragma detect_mismatch("CUI_RUNTIME_FLAVOR", "Production")
#endif
#endif

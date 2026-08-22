#pragma once

#include "Control.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if CUI_ENABLE_DYNAMIC_XAML
/** Design-runtime builder value; production consumes CompiledStyleProgramView. */
struct ControlStyleValue
{
	BindingValue Literal;
	std::wstring ResourceKey;
	bool IsDynamicResource = false;

	ControlStyleValue() = default;
	explicit ControlStyleValue(BindingValue value)
		: Literal(std::move(value)) {}

	static ControlStyleValue Resource(std::wstring key);
	static ControlStyleValue DynamicResource(std::wstring key);

	bool IsResource() const noexcept { return !ResourceKey.empty(); }
};

/** Assigns one metadata property when its containing Setter or Trigger is active. */
struct ControlStyleSetter
{
	DependencyPropertyReference Property;
	ControlStyleValue Value;

	ControlStyleSetter() = default;
#if CUI_ENABLE_DYNAMIC_XAML
	ControlStyleSetter(std::wstring propertyName, BindingValue value);
	ControlStyleSetter(std::wstring propertyName, ControlStyleValue value);
#endif
	ControlStyleSetter(
		const DependencyProperty& property, BindingValue value)
		: Property(property), Value(std::move(value)) {}
	ControlStyleSetter(
		const DependencyProperty& property, ControlStyleValue value)
		: Property(property), Value(std::move(value)) {}

#if CUI_ENABLE_DYNAMIC_XAML
	static ControlStyleSetter Resource(
		std::wstring propertyName,
		std::wstring resourceKey);
#endif
	static ControlStyleSetter Resource(
		const DependencyProperty& property,
		std::wstring resourceKey)
	{
		return ControlStyleSetter(
			property,
			ControlStyleValue::Resource(std::move(resourceKey)));
	}

#if CUI_ENABLE_DYNAMIC_XAML
	static ControlStyleSetter DynamicResource(
		std::wstring propertyName,
		std::wstring resourceKey);
#endif
	static ControlStyleSetter DynamicResource(
		const DependencyProperty& property,
		std::wstring resourceKey)
	{
		return ControlStyleSetter(
			property,
			ControlStyleValue::DynamicResource(std::move(resourceKey)));
	}
};

/** One observable DataContext path/value predicate used by DataTrigger. */
struct ControlStyleDataCondition
{
	std::wstring SourceProperty;
	BindingValue Value;
};

/** One target-property/value predicate used by Trigger or MultiTrigger. */
struct ControlStylePropertyCondition
{
	DependencyPropertyReference Property;
	BindingValue Value;

	ControlStylePropertyCondition() = default;
#if CUI_ENABLE_DYNAMIC_XAML
	ControlStylePropertyCondition(
		std::wstring propertyName, BindingValue value);
#endif
	ControlStylePropertyCondition(
		const DependencyProperty& property, BindingValue value)
		: Property(property), Value(std::move(value)) {}
};

/**
 * One lowered WPF Style rule. The resource key and exact target type select a
 * single effective Style; conditions only select Setter/Trigger entries inside
 * that Style and never participate in cross-style cascading.
 */
struct ControlStyleSelector
{
	std::optional<UIClass> Type;
	/** Exact XAML component identity; empty means no component-type constraint. */
	std::wstring DeclarativeTypeNamespace;
	std::wstring DeclarativeTypeName;
	std::wstring StyleResourceKey;
	std::vector<ControlStylePropertyCondition> PropertyConditions;
	std::vector<ControlStyleDataCondition> DataConditions;

	bool MatchesTargetType(Control& target) const;
	bool MatchesConditions(Control& target) const;
	bool IsConditional() const noexcept;
};

struct ControlStyleRule
{
	size_t Id = 0;
	ControlStyleSelector Selector;
	std::vector<ControlStyleSetter> Setters;
	/** WPF TriggerBase edge actions; clocks are instantiated per target control. */
	std::vector<DeclarativeEventTriggerActionDefinition> EnterActions;
	std::vector<DeclarativeEventTriggerActionDefinition> ExitActions;
};
#endif

inline constexpr uint32_t CompiledStyleInvalidIndex = UINT32_MAX;

/**
 * Compiled value references retain the historical zero-based instance-pool
 * representation when the high bit is clear.  AOT output sets the high bit
 * and packs a typed-pool slot plus an element index into the remaining bits.
 * This keeps the Design builder source-compatible while allowing Production
 * to leave scalar/POD values in process-lifetime read-only storage.
 */
inline constexpr uint32_t CompiledStyleStaticValueFlag = 0x80000000u;
inline constexpr uint32_t CompiledStyleStaticValuePoolShift = 24u;
inline constexpr uint32_t CompiledStyleStaticValuePoolLimit = 0x7fu;
inline constexpr uint32_t CompiledStyleStaticValueElementLimit = 0x01000000u;
inline constexpr uint32_t CompiledStyleStaticValueElementMask = 0x00ffffffu;

[[nodiscard]] constexpr uint32_t MakeCompiledStyleStaticValueReference(
	uint32_t poolIndex,
	uint32_t elementIndex) noexcept
{
	return poolIndex < CompiledStyleStaticValuePoolLimit
		&& elementIndex < CompiledStyleStaticValueElementLimit
		? CompiledStyleStaticValueFlag
			| (poolIndex << CompiledStyleStaticValuePoolShift)
			| elementIndex
		: CompiledStyleInvalidIndex;
}

[[nodiscard]] constexpr bool IsCompiledStyleStaticValueReference(
	uint32_t value) noexcept
{
	return value != CompiledStyleInvalidIndex
		&& (value & CompiledStyleStaticValueFlag) != 0;
}

[[nodiscard]] constexpr uint32_t CompiledStyleStaticValuePoolIndex(
	uint32_t value) noexcept
{
	return (value >> CompiledStyleStaticValuePoolShift)
		& CompiledStyleStaticValuePoolLimit;
}

[[nodiscard]] constexpr uint32_t CompiledStyleStaticValueElementIndex(
	uint32_t value) noexcept
{
	return value & CompiledStyleStaticValueElementMask;
}

enum class CompiledStyleValuePoolKind : uint8_t
{
	Bool,
	NullableBool,
	Int,
	Int64,
	Float,
	Double,
	String,
	Color,
	Thickness,
	CornerRadius,
	Point,
	Vector,
	Rect,
	Size,
	Matrix,
	Length
};

/** One generated typed array; Data has the element type selected by Kind. */
struct CompiledStyleValuePoolView
{
	const void* Data = nullptr;
	uint32_t Count = 0;
	CompiledStyleValuePoolKind Kind = CompiledStyleValuePoolKind::Bool;
};

template<typename T>
struct CompiledStyleValuePoolKindOf;

#define CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(Type, KindName) \
	template<> struct CompiledStyleValuePoolKindOf<Type> final \
	{ \
		static constexpr CompiledStyleValuePoolKind Value = \
			CompiledStyleValuePoolKind::KindName; \
	}

CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(bool, Bool);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(NullableBool, NullableBool);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(int, Int);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(long long, Int64);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(float, Float);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(double, Double);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(std::wstring_view, String);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(D2D1_COLOR_F, Color);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(Thickness, Thickness);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(CornerRadius, CornerRadius);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(cui::core::Point, Point);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(cui::core::Vector, Vector);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(cui::core::Rect, Rect);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(cui::core::Size, Size);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(D2D1_MATRIX_3X2_F, Matrix);
CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND(cui::layout::Length, Length);

#undef CUI_DECLARE_COMPILED_STYLE_VALUE_POOL_KIND

template<typename T, size_t N>
[[nodiscard]] constexpr CompiledStyleValuePoolView
MakeCompiledStyleValuePoolView(const T (&values)[N]) noexcept
{
	using Value = std::remove_cv_t<T>;
	static_assert(N <= CompiledStyleStaticValueElementLimit,
		"compiled Style typed pool exceeds its 24-bit element range");
	return {
		values,
		static_cast<uint32_t>(N),
		CompiledStyleValuePoolKindOf<Value>::Value };
}

struct CompiledStyleRange
{
	uint32_t Offset = 0;
	uint32_t Count = 0;
};

enum class CompiledStyleOperandKind : uint8_t
{
	Literal,
	StaticResource,
	DynamicResource
};

struct CompiledStyleOperand
{
	CompiledStyleOperandKind Kind = CompiledStyleOperandKind::Literal;
	uint32_t Index = CompiledStyleInvalidIndex;
};

struct CompiledStylePropertyConditionOp
{
	DependencyPropertyReference Property;
	uint32_t ValueIndex = CompiledStyleInvalidIndex;
};

struct CompiledStyleDataConditionOp
{
	/**
	 * AOT programs index CompiledStyleProgramView::DataPaths. The Design-only
	 * owned adapter sets CompiledStyleDynamicDataPathFlag and indexes Strings so
	 * authored paths can keep their name-based, incrementally editable form.
	 */
	uint32_t PathReference = CompiledStyleInvalidIndex;
	uint32_t ValueIndex = CompiledStyleInvalidIndex;
};

inline constexpr uint32_t CompiledStyleDynamicDataPathFlag = 0x80000000u;
inline constexpr uint32_t CompiledStyleDataPathIndexMask = 0x7fffffffu;

[[nodiscard]] constexpr bool IsCompiledStyleDynamicDataPathReference(
	uint32_t reference) noexcept
{
	return reference != CompiledStyleInvalidIndex
		&& (reference & CompiledStyleDynamicDataPathFlag) != 0;
}

[[nodiscard]] constexpr uint32_t CompiledStyleDataPathIndex(
	uint32_t reference) noexcept
{
	return reference & CompiledStyleDataPathIndexMask;
}

struct CompiledStyleSetterOp
{
	DependencyPropertyReference Property;
	CompiledStyleOperand Value;
};

struct CompiledStyleRuleOp
{
	uint32_t RuleId = 0;
	uint32_t SourceOrder = 0;
	CompiledStyleRange PropertyConditions;
	CompiledStyleRange DataConditions;
	CompiledStyleRange Setters;
	CompiledStyleRange EnterActions;
	CompiledStyleRange ExitActions;
};

struct CompiledStyleGroupOp
{
	bool HasType = false;
	UIClass Type = UIClass::UI_Base;
	/** Exact ComponentDefinition selector; zero means native/no component. */
	ComponentTypeToken ComponentType;
	uint32_t StyleResourceKey = CompiledStyleInvalidIndex;
	CompiledStyleRange RuleIndexes;
	CompiledStyleRange PropertyWatchers;
	CompiledStyleRange DataPathWatchers;
};

struct CompiledStyleResourceOp
{
	uint32_t KeyStringIndex = CompiledStyleInvalidIndex;
	uint32_t ValueIndex = CompiledStyleInvalidIndex;
};

/** Private generated-code contract version; not a stable plugin ABI. */
inline constexpr uint32_t CompiledStyleProgramViewVersion = 18;

/**
 * Non-owning process-lifetime layout emitted by the AOT compiler. Generated
 * backing arrays must outlive every ControlStyleSheet created from this view.
 * Per-document BindingValue objects deliberately live in the sheet instance;
 * Style trigger/storyboard structure stays flat and read-only here.
 */
struct CompiledStyleProgramView
{
	uint32_t Version = CompiledStyleProgramViewVersion;
	std::span<const std::wstring_view> Strings;
	/** Typed scalar/POD arrays addressed by packed compiled-value references. */
	std::span<const CompiledStyleValuePoolView> ValuePools;
	std::span<const CompiledStyleResourceOp> Resources;
	/** Resource indexes sorted by key for allocation-free binary lookup. */
	std::span<const uint32_t> ResourceLookup;
	std::span<const CompiledStylePropertyConditionOp> PropertyConditions;
	std::span<const CompiledStyleDataConditionOp> DataConditions;
	std::span<const CompiledStyleSetterOp> Setters;
	/** Style storyboard records reuse the string-free interaction ABI. */
	std::span<const CompiledInteractionPropertyOperand> PropertyOperands;
	std::span<const uint32_t> ObjectPathChildIndices;
	std::span<const CompiledStoryboardObjectPathOp> ObjectPaths;
	std::span<const CompiledInteractionKeyFrameOp> KeyFrames;
	std::span<const DeclarativePathAnimationSegment> PathSegments;
	std::span<const CompiledInteractionAnimationOp> Animations;
	std::span<const CompiledInteractionTimelineGroupOp> TimelineGroups;
	std::span<const CompiledInteractionStoryboardOp> Storyboards;
	std::span<const CompiledInteractionActionOp> Actions;
	/** Source-ordered rules. Groups are consecutive selector-identity runs. */
	std::span<const CompiledStyleRuleOp> Rules;
	std::span<const uint32_t> RuleIndexes;
	std::span<const DependencyPropertyReference> PropertyWatchers;
	std::span<const uint32_t> DataPathWatchers;
	std::span<const CompiledStyleGroupOp> Groups;
	std::span<const DependencyPropertyReference> GlobalPropertyWatchers;
	std::span<const uint32_t> GlobalDataPathWatchers;
	/** Immutable name-free paths referenced by DataConditions and watchers. */
	std::span<const CompiledBindingPathView> DataPaths;
};

#if CUI_ENABLE_DYNAMIC_XAML
/**
 * Design/test adapter for incrementally assembling a compiled program. The
 * immutable runtime converts this owned builder to CompiledStyleProgramView;
 * Production does not expose or carry these structural vectors.
 */
struct CompiledStyleProgram
{
	std::vector<std::wstring> Strings;
	std::vector<BindingValue> Values;
	std::vector<CompiledStyleResourceOp> Resources;
	std::vector<uint32_t> ResourceLookup;
	std::vector<CompiledStylePropertyConditionOp> PropertyConditions;
	std::vector<CompiledStyleDataConditionOp> DataConditions;
	std::vector<CompiledStyleSetterOp> Setters;
	std::vector<DeclarativeEventTriggerActionDefinition> Actions;
	std::vector<CompiledStyleRuleOp> Rules;
	std::vector<uint32_t> RuleIndexes;
	std::vector<DependencyPropertyReference> PropertyWatchers;
	std::vector<uint32_t> DataPathWatchers;
	std::vector<CompiledStyleGroupOp> Groups;
	std::vector<DependencyPropertyReference> GlobalPropertyWatchers;
	/** Design adapter entries are untagged indexes into Strings on input. */
	std::vector<uint32_t> GlobalDataPathWatchers;
};

namespace cui::style::design
{
	/** Narrow hooks implemented only by the Design runtime compatibility TU. */
	bool ValidateDynamicDataPathReference(
		const CompiledStyleProgramView& program,
		uint32_t reference,
		bool requireObserve);
	const DependencyPropertyMetadata* FindNamedPropertyMetadata(
		Control& target,
		const std::wstring& propertyName);
	bool TryReadDynamicDataPath(
		const CompiledStyleProgramView& program,
		uint32_t reference,
		const IBindingSource& source,
		BindingValue& value);
	bool TryParseDataPathSegments(
		std::wstring_view value,
		std::vector<std::wstring>& segments);
}
#endif

enum class ControlStyleResolutionIssueCode
{
	MissingResource,
	PropertyNotFound,
	PropertyNotWritable,
	InvalidValue
};

struct ControlStyleResolutionIssue
{
	ControlStyleResolutionIssueCode Code =
		ControlStyleResolutionIssueCode::MissingResource;
	size_t RuleId = 0;
	std::wstring PropertyName;
	std::wstring ResourceKey;
};

struct ResolvedControlStyleSetter
{
	DependencyPropertyReference Property;
	BindingValue Value;
	std::wstring ResourceKey;
	bool IsDynamicResource = false;
	size_t RuleId = 0;
	bool IsConditional = false;
};

struct ResolvedControlStyleTrigger
{
	size_t RuleId = 0;
	bool IsActive = false;
	/** Non-null selects the immutable, allocation-free Production action path. */
	const CompiledStyleProgramView* CompiledProgram = nullptr;
	std::span<const BindingValue> CompiledValues;
	CompiledStyleRange CompiledEnterActions;
	CompiledStyleRange CompiledExitActions;
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design-only compatibility graph used by mutable/live-XAML sheets. */
	std::vector<DeclarativeEventTriggerActionDefinition> EnterActions;
	std::vector<DeclarativeEventTriggerActionDefinition> ExitActions;
#endif
};

struct ControlStyleResolution
{
	bool HasStyle = false;
	std::vector<ResolvedControlStyleSetter> Setters;
	/** Includes active and inactive action rules so targets can detect edges. */
	std::vector<ResolvedControlStyleTrigger> Triggers;
	std::vector<ControlStyleResolutionIssue> Issues;

	bool Success() const noexcept { return Issues.empty(); }
};

/**
 * Runtime view of lowered WPF Style resources. Production instances bind an
 * immutable CompiledStyleProgramView; the Design flavor additionally
 * exposes an observable mutable builder backend for live XAML editing.
 * Data conditions always resolve through each target's effective DataContext;
 * the shared sheet never owns or supplies a context.
 */
class ControlStyleSheet final
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	ControlStyleSheet();
#endif
	~ControlStyleSheet();
	ControlStyleSheet(const ControlStyleSheet&) = delete;
	ControlStyleSheet& operator=(const ControlStyleSheet&) = delete;

	/**
	 * Binds one process-lifetime structural view to its instance value pool
	 * without copying or allocating any structural pool.
	 */
	static std::shared_ptr<const ControlStyleSheet> CreateCompiled(
		CompiledStyleProgramView program,
		std::vector<BindingValue> values);
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design/test adapter retaining the owned structural builder. */
	static std::shared_ptr<const ControlStyleSheet> CreateCompiled(
		CompiledStyleProgram program);
#endif
	bool IsImmutable() const noexcept { return _compiledProgram != nullptr; }

#if CUI_ENABLE_DYNAMIC_XAML
	size_t AddRule(
		ControlStyleSelector selector,
		std::vector<ControlStyleSetter> setters,
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions = {},
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions = {});
	bool RemoveRule(size_t ruleId);
	void ClearRules();
#endif
	size_t RuleCount() const noexcept;
	bool HasRules() const noexcept { return RuleCount() != 0; }
#if CUI_ENABLE_DYNAMIC_XAML
	/** Mutable/designer backend view. Compiled sheets use RuleCount/Resolve. */
	const std::vector<ControlStyleRule>& Rules() const noexcept;

	bool SetResource(std::wstring key, BindingValue value);
	bool RemoveResource(const std::wstring& key);
	void ClearResources();
#endif
	bool TryGetResource(const std::wstring& key, BindingValue& value) const;

	ControlStyleResolution Resolve(
		Control& target,
		bool themeStyle = false) const;
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design compatibility query for dynamically named trigger properties. */
	bool UsesPropertyCondition(const std::wstring& propertyName) const;
#endif
	/**
	 * Returns whether the effective Style selected for this target contains
	 * any target-property Trigger/MultiTrigger conditions.
	 */
	bool HasPropertyConditionsFor(
		Control& target,
		bool themeStyle = false) const;
	/**
	 * Target-scoped condition query. Unlike the sheet-wide overload, this
	 * observes the target identity, explicit Style key, and Theme/author slot.
	 */
#if CUI_ENABLE_DYNAMIC_XAML
	bool UsesPropertyCondition(
		Control& target,
		const std::wstring& propertyName,
		bool themeStyle = false) const;
#endif
	bool UsesPropertyCondition(
		Control& target,
		const DependencyPropertyChangedEventArgs& args,
		bool themeStyle = false) const;
	/**
	 * Returns whether the effective Style selected for this target contains
	 * at least one DataTrigger/MultiDataTrigger condition.
	 */
	bool HasDataConditionsFor(
		Control& target,
		bool themeStyle = false) const;
#if CUI_ENABLE_DYNAMIC_XAML
	/**
	 * Unique DataTrigger paths for the effective Style selected for this
	 * target. This name-based view belongs to the mutable Design backend.
	 */
	std::vector<std::wstring> DataConditionPathsFor(
		Control& target,
		bool themeStyle = false) const;
	/** Unique DataTrigger paths used to build target-local DataContext observers. */
	const std::vector<std::wstring>& DataConditionPaths() const;
#endif
	/** Name-free DataTrigger paths used by the Production observer fast path. */
	std::vector<CompiledBindingPathView> CompiledDataConditionPaths() const;
#if CUI_ENABLE_DYNAMIC_XAML
	uint64_t Revision() const noexcept;
	EventConnection SubscribeChanged(std::function<void()> handler) const;
#endif

private:
#if !CUI_ENABLE_DYNAMIC_XAML
	ControlStyleSheet();
#endif
#if CUI_ENABLE_DYNAMIC_XAML
	struct RuleIdentityCacheKey
	{
		UIClass NativeType = UIClass::UI_Base;
		std::wstring DeclarativeTypeNamespace;
		std::wstring DeclarativeTypeName;
		std::wstring StyleResourceKey;
		bool ThemeStyle = false;

		bool operator==(const RuleIdentityCacheKey&) const = default;
	};

	struct RuleIdentityCacheKeyHash
	{
		size_t operator()(const RuleIdentityCacheKey& value) const noexcept
		{
			size_t result = std::hash<int>{}(
				static_cast<int>(value.NativeType));
			auto combine = [&](size_t next)
			{
				result ^= next + 0x9e3779b9u
					+ (result << 6) + (result >> 2);
			};
			combine(std::hash<std::wstring>{}(
				value.DeclarativeTypeNamespace));
			combine(std::hash<std::wstring>{}(
				value.DeclarativeTypeName));
			combine(std::hash<std::wstring>{}(
				value.StyleResourceKey));
			combine(std::hash<bool>{}(value.ThemeStyle));
			return result;
		}
	};

	struct ResourceEntry
	{
		std::wstring Key;
		BindingValue Value;
	};

	struct RuleIdentityCacheEntry
	{
		std::vector<size_t> RuleIndexes;
		std::vector<DependencyPropertyReference> PropertyConditions;
		std::vector<std::wstring> DataConditionPaths;
	};

	std::vector<ControlStyleRule> _rules;
	std::vector<ResourceEntry> _resources;
	size_t _nextRuleId = 1;
	mutable uint64_t _revision = 0;
	mutable Event<void()> _changed;
	mutable std::unordered_map<
		RuleIdentityCacheKey,
		RuleIdentityCacheEntry,
		RuleIdentityCacheKeyHash> _ruleIdentityCache;
	mutable bool _conditionCachesBuilt = false;
	mutable std::vector<DependencyPropertyReference>
		_propertyConditions;
	mutable bool _resourceIndexBuilt = false;
	mutable std::unordered_map<std::wstring, size_t>
		_resourceIndex;

	bool MatchesDataConditions(
		const ControlStyleSelector& selector,
		Control& target) const;
	const RuleIdentityCacheEntry& CandidateRuleIdentity(
		Control& target,
		bool themeStyle) const;
	void EnsureConditionCaches() const;
	void NotifyChanged() const;
#endif
	struct CompiledStyleInstance final : CompiledStyleProgramView
	{
		std::vector<BindingValue> Values;
#if CUI_ENABLE_DYNAMIC_XAML
		std::vector<DeclarativeEventTriggerActionDefinition> DesignActions;
		bool UsesDesignActions = false;
		std::unique_ptr<CompiledStyleProgram> OwnedProgram;
		std::vector<std::wstring_view> OwnedStringViews;
#endif
	};
	std::unique_ptr<CompiledStyleInstance> _compiledProgram;
#if CUI_ENABLE_DYNAMIC_XAML
	mutable std::vector<std::wstring> _dataConditionPaths;
#endif
	ControlStyleResolution ResolveCompiled(
		Control& target,
		bool themeStyle) const;
	static std::shared_ptr<ControlStyleSheet> CreateCompiledCore(
		CompiledStyleProgramView program,
		std::vector<BindingValue> values,
		size_t actionCount,
		bool usesFlatActions);
	bool TryGetCompiledResource(
		const std::wstring& key,
		BindingValue& value) const;
	bool CompiledHasPropertyConditionsFor(
		Control& target,
		bool themeStyle) const;
	bool CompiledUsesPropertyCondition(
		Control& target,
		const DependencyPropertyChangedEventArgs& args,
		bool themeStyle) const;
	bool CompiledHasDataConditionsFor(
		Control& target,
		bool themeStyle) const;
#if CUI_ENABLE_DYNAMIC_XAML
	bool TryPopulateDesignTriggerActions(
		const CompiledStyleRuleOp& rule,
		ResolvedControlStyleTrigger& trigger) const;
#endif
	bool CompiledGroupMatches(
		const CompiledStyleGroupOp& group,
		Control& target,
		bool themeStyle) const;
};

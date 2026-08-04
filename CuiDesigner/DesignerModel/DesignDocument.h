#pragma once

#include "../DesignerTypes.h"
#include "../DesignerStyleSheet.h"
#include "DesignValue.h"
#include "../../CUI/include/CollectionViewSource.h"
#include "../../CUI/include/ItemsPanelTemplate.h"
#include "../../CUI/include/Resource.h"
#include <map>
#include <memory>
#include <limits>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace DesignerModel
{
/** UTF-16 source range retained by the normalized document frontend. */
struct XamlSourceSpan
{
	static constexpr std::size_t UnknownOffset = static_cast<std::size_t>(-1);

	std::size_t Utf16Offset = UnknownOffset;
	std::size_t Utf16Length = 0;
	/** 1-based Unicode-scalar line/column coordinates. */
	std::size_t Line = 0;
	std::size_t Column = 0;
	std::size_t EndLine = 0;
	std::size_t EndColumn = 0;

	bool Valid() const noexcept
	{
		return Utf16Offset != UnknownOffset && Line != 0 && Column != 0;
	}
	bool operator==(const XamlSourceSpan&) const = default;
};

enum class XamlDiagnosticStage : unsigned char
{
	Unknown,
	Parse,
	Normalize,
	Materialize,
	Reload,
	CodeGeneration
};

/** Shared syntax/semantic diagnostic used by every normalized XAML consumer. */
struct XamlDocumentDiagnostic
{
	static constexpr std::size_t UnknownOffset = XamlSourceSpan::UnknownOffset;

	std::wstring Message;
	std::wstring QName;
	std::wstring Member;
	XamlDiagnosticStage Stage = XamlDiagnosticStage::Unknown;
	std::size_t Line = 0;
	std::size_t Column = 0;
	std::size_t EndLine = 0;
	std::size_t EndColumn = 0;
	std::size_t Utf16Offset = UnknownOffset;
	std::size_t Utf16Length = 0;

	bool HasLocation() const noexcept { return Line != 0 && Column != 0; }
	bool HasSourceOffset() const noexcept { return Utf16Offset != UnknownOffset; }
	void Apply(const XamlSourceSpan& span) noexcept;
};

/**
 * Portable association between a design document and its generated/user C++
 * pair. RelativeBasePath has no extension and is resolved from the design
 * document directory by the Designer shell; runtime loading does not depend on
 * it.
 */
struct DesignCodeBehindModel
{
	std::wstring ClassName;
	std::wstring RelativeBasePath;

	bool Empty() const noexcept
	{
		return ClassName.empty() && RelativeBasePath.empty();
	}
	bool Validate(std::wstring* outError = nullptr) const;
	/** Accepts C++ `::` or XAML-style `.` separators and emits canonical `::`. */
	static bool TryNormalizeClassName(
		const std::wstring& value,
		std::wstring& normalized,
		std::wstring* outError = nullptr);
	static bool TryNormalizeRelativeBasePath(
		const std::wstring& value,
		std::wstring& normalized,
		std::wstring* outError = nullptr);
	bool operator==(const DesignCodeBehindModel& other) const;
};

struct DesignComponentDefinition;
struct DesignControlTemplate;
struct DesignDataTemplate;
struct DesignItemsPanelTemplate;
struct DesignGroupStyle;

/** Structural resources owned by one XAML Resources scope. */
struct DesignObjectResourceDictionary
{
	std::vector<DesignComponentDefinition> Components;
	std::vector<DesignControlTemplate> ControlTemplates;
	std::vector<DesignDataTemplate> DataTemplates;
	std::vector<DesignItemsPanelTemplate> ItemsPanelTemplates;
	std::vector<DesignGroupStyle> GroupStyles;

	DesignObjectResourceDictionary();
	~DesignObjectResourceDictionary();
	DesignObjectResourceDictionary(const DesignObjectResourceDictionary&);
	DesignObjectResourceDictionary(DesignObjectResourceDictionary&&) noexcept;
	DesignObjectResourceDictionary& operator=(const DesignObjectResourceDictionary&);
	DesignObjectResourceDictionary& operator=(DesignObjectResourceDictionary&&) noexcept;
	bool Empty() const noexcept;
	bool operator==(const DesignObjectResourceDictionary& other) const;
};

enum class DesignGridLengthUnit
{
	Auto,
	Pixel,
	Star
};

struct DesignGridLength
{
	double Value = 1.0;
	DesignGridLengthUnit Unit = DesignGridLengthUnit::Auto;
	bool operator==(const DesignGridLength&) const = default;
};

struct DesignGridTrack
{
	DesignGridLength Length;
	double Minimum = 0.0;
	double Maximum = (std::numeric_limits<float>::max)();
	bool operator==(const DesignGridTrack&) const = default;
};

struct DesignColor
{
	double R = 0.0;
	double G = 0.0;
	double B = 0.0;
	double A = 1.0;
	bool operator==(const DesignColor&) const = default;
};

struct DesignChartPoint
{
	std::wstring Label;
	double Value = 0.0;
	unsigned long long Tag = 0;
	std::optional<DesignColor> Color;
	bool operator==(const DesignChartPoint&) const = default;
};

struct DesignChartSeries
{
	std::wstring Name;
	bool Visible = true;
	std::optional<DesignColor> Color;
	std::vector<DesignChartPoint> Points;
	bool operator==(const DesignChartSeries&) const = default;
};

struct DesignRelativePanelConstraints
{
	std::optional<bool> CenterHorizontal;
	std::optional<bool> CenterVertical;
	std::optional<bool> AlignLeftWithPanel;
	std::optional<bool> AlignTopWithPanel;
	std::optional<bool> AlignRightWithPanel;
	std::optional<bool> AlignBottomWithPanel;
	std::optional<std::wstring> Above;
	std::optional<std::wstring> Below;
	std::optional<std::wstring> LeftOf;
	std::optional<std::wstring> RightOf;
	std::optional<std::wstring> AlignLeftWith;
	std::optional<std::wstring> AlignRightWith;
	std::optional<std::wstring> AlignTopWith;
	std::optional<std::wstring> AlignBottomWith;
	bool Empty() const noexcept;
	bool operator==(const DesignRelativePanelConstraints&) const = default;
};

enum class DesignNodeChildRole
{
	Default,
	Header
};

/** Authored, strongly typed structural content of one XAML element. */
struct DesignNodeStructure
{
	/** Authored Button/MenuItem ICommandSource target x:Name. */
	std::wstring CommandTarget;
	std::wstring ItemsSourceResource;
	std::wstring ItemTemplate;
	std::wstring ContentTemplate;
	std::wstring HeaderTemplate;
	std::wstring ControlTemplate;
	std::wstring GroupStyle;
	std::wstring ItemsPanel;
	std::wstring ItemContainerStyle;
	std::wstring MediaFile;
	DesignNodeChildRole ChildRole = DesignNodeChildRole::Default;
	std::optional<DesignRelativePanelConstraints> RelativePanel;

	std::optional<std::vector<DesignGridTrack>> GridRows;
	std::optional<std::vector<DesignGridTrack>> GridColumns;
	std::optional<std::vector<DesignChartSeries>> ChartSeries;

	bool Empty() const noexcept;
	bool operator==(const DesignNodeStructure&) const = default;
};

/** Ephemeral state created only while expanding component/control templates. */
struct DesignNodeTemplateState
{
	bool ComponentExpanded = false;
	bool ControlTemplateExpanded = false;
	bool Generated = false;
	bool ResourceScopeFromTheme = false;
	bool ControlTemplateRoot = false;
	std::wstring Owner;
	std::wstring ContentOwner;
	std::wstring PartName;
	std::wstring AppliedControlTemplate;
	std::wstring AppliedControlTemplateResource;
	bool AppliedControlTemplateFromTheme = false;
	bool AppliedControlTemplateFromStyle = false;
	std::wstring ControlTemplateChain;
	bool operator==(const DesignNodeTemplateState&) const = default;
};

/** One authored XAML member value, before styles and bindings are resolved. */
struct DesignPropertyAssignment
{
	DesignerStyleValue Value;
	std::wstring ResourceKey;
	std::wstring DynamicResourceKey;

	bool operator==(const DesignPropertyAssignment&) const = default;
};

/** Transient spans attached to one normalized element and its authored members. */
struct DesignNodeSourceInfo
{
	XamlSourceSpan Element;
	std::map<std::wstring, XamlSourceSpan, DesignPropertyNameLess> Members;

	void RecordMember(std::wstring name, XamlSourceSpan span);
	const XamlSourceSpan* FindMember(const std::wstring& name) const noexcept;
};

/** Transient document-level spans for resources, styles and templates. */
struct XamlDocumentSourceMap
{
	XamlSourceSpan Root;
	std::map<std::wstring, XamlSourceSpan, DesignPropertyNameLess> Symbols;

	void RecordSymbol(std::wstring symbol, XamlSourceSpan span);
	const XamlSourceSpan* FindSymbol(const std::wstring& symbol) const noexcept;
	const XamlSourceSpan* FindMentionedSymbol(
		const std::wstring& message,
		std::wstring* matchedSymbol = nullptr) const noexcept;
};

/** Authored target-member expressions, keyed by canonical Schema member name. */
using DesignBindingMap = std::map<
	std::wstring, DesignerDataBinding, DesignPropertyNameLess>;

/**
 * Schema-facing authored properties of one XAML element. Property names remain
 * data because XAML may define new component members; values and resource
 * expressions are nevertheless strongly typed and cannot carry unrelated
 * designer state.
 */
struct DesignNodeProperties
{
	std::wstring StyleResourceKey;
	std::map<std::wstring, DesignPropertyAssignment, DesignPropertyNameLess>
		Values;

	bool Empty() const noexcept;
	const DesignPropertyAssignment* Find(const std::wstring& name) const noexcept;
	DesignPropertyAssignment* Find(const std::wstring& name) noexcept;
	void Set(std::wstring name, DesignPropertyAssignment assignment);
	bool Remove(const std::wstring& name) noexcept;
	bool operator==(const DesignNodeProperties&) const = default;
};

/** Canonical serialization adapter; the document itself never stores this bag. */
DesignValue EncodeDesignNodeStructure(
	UIClass type,
	const DesignNodeStructure& structure);
bool DecodeDesignNodeStructure(
	UIClass type,
	const DesignValue& value,
	DesignNodeStructure& structure,
	std::wstring* outError = nullptr);

/** Versioned snapshot adapter; the document itself never stores a value bag. */
DesignValue EncodeDesignNodeProperties(const DesignNodeProperties& properties);
bool DecodeDesignNodeProperties(
	const DesignValue& value,
	DesignNodeProperties& properties,
	std::wstring* outError = nullptr);

/** Versioned snapshot adapters; the document stores only the typed maps. */
DesignValue EncodeDesignNodeBindings(const DesignBindingMap& bindings);
bool DecodeDesignNodeBindings(
	const DesignValue& value,
	DesignBindingMap& bindings,
	std::wstring* outError = nullptr);
DesignValue EncodeDesignNodeEvents(const DesignEventHandlerMap& events);
bool DecodeDesignNodeEvents(
	const DesignValue& value,
	DesignEventHandlerMap& events,
	std::wstring* outError = nullptr);
DesignValue EncodeDesignCommandBindings(
	const std::vector<DesignCommandBinding>& bindings);
bool DecodeDesignCommandBindings(
	const DesignValue& value,
	std::vector<DesignCommandBinding>& bindings,
	std::wstring* outError = nullptr);
DesignValue EncodeDesignInputBindings(
	const std::vector<DesignInputBinding>& bindings);
bool DecodeDesignInputBindings(
	const DesignValue& value,
	std::vector<DesignInputBinding>& bindings,
	std::wstring* outError = nullptr);

struct DesignNode
{
	int Id = 0;
	int ParentId = 0;
	std::wstring ParentRef;
	std::wstring Name;
	UIClass Type = UIClass::UI_Base;
	/** Authoritative built-in XAML identity; Native Type selects behavior only. */
	RuntimeTypeId XamlType;
	/** Non-empty when the element type comes from ComponentDefinition. */
	DesignerComponentType ComponentType;
	/** Component content property used by this public visual child. */
	std::wstring ComponentContentProperty;
	/** Template-local container receiving one component content property. */
	std::wstring PresentedComponentContent;
	/** WPF ContentPresenter alias used only inside a ControlTemplate. */
	std::wstring TemplateContentSource;
	int Order = -1;
	// Pure design-time placement protection; it is never projected to runtime.
	bool Locked = false;
	DesignNodeProperties Properties;
	DesignNodeStructure Structure;
	DesignNodeTemplateState TemplateState;
	DesignEventHandlerMap Events;
	DesignBindingMap Bindings;
	std::vector<DesignCommandBinding> CommandBindings;
	std::vector<DesignInputBinding> InputBindings;
	/** Frontend-only spans; excluded from snapshots and semantic equality. */
	DesignNodeSourceInfo Source;
	/** Resources declared by <Control.Resources>; values are lexically scoped. */
	DesignerStyleSheet LocalResources;
	/** Structural resources owned by this lexical Resources scope. */
	DesignObjectResourceDictionary LocalObjectResources;
	/** Target property -> owning component property for template-local nodes. */
	std::map<std::wstring, std::wstring> TemplateBindings;
	/** Template-local source event -> event declared by the owning component. */
	std::map<std::wstring, std::wstring> TemplateEventBindings;

	bool operator==(const DesignNode& other) const;
};

/**
 * A XAML-owned control type. BaseType supplies the current native layout and
 * rendering host; Properties are installed as instance metadata before any
 * attributes, styles or bindings are applied.
 */
struct DesignComponentDefinition
{
	DesignerComponentType Type;
	UIClass BaseType = UIClass::UI_Canvas;
	std::wstring DisplayName;
	std::wstring Category = L"Components";
	std::vector<DesignerComponentPropertyDescriptor> Properties;
	std::vector<DesignerComponentContentPropertyDescriptor> ContentProperties;
	std::vector<DesignerComponentEventDescriptor> Events;
	/** One independently named visual tree; ids and names are local to the template. */
	std::vector<DesignNode> Template;
	/** WPF/WinUI-like mutually exclusive state groups applied to template parts. */
	std::vector<DesignerVisualStateGroup> VisualStateGroups;
	/** WPF-style template-root event triggers and controllable storyboards. */
	std::vector<DesignerEventTrigger> EventTriggers;
	/** Empty for local definitions; set when loaded from a merged dictionary. */
	std::wstring SourceDictionary;

	bool operator==(const DesignComponentDefinition&) const = default;
};

/** XAML-declared record metadata used to validate item-scoped bindings. */
struct DesignDataTypeDefinition
{
	std::wstring Name;
	DesignerDataContextSchema Properties;
	std::wstring SourceDictionary;

	bool operator==(const DesignDataTypeDefinition&) const = default;
};

/** One keyed visual tree instantiated once per ItemsSource record. */
struct DesignDataTemplate
{
	std::wstring Key;
	std::wstring DataType;
	/** True when authored as HierarchicalDataTemplate. */
	bool Hierarchical = false;
	/** Child-list Binding evaluated against the current data item. */
	std::optional<DesignerDataBinding> ItemsSourceBinding;
	std::vector<DesignNode> Template;
	std::wstring SourceDictionary;
	/** WPF-style implicit templates omit x:Key and are keyed by DataType. */
	bool IsImplicit() const noexcept { return Key.empty(); }
	bool HasSameResourceIdentity(const DesignDataTemplate& other) const noexcept;
	std::wstring DisplayName() const;

	bool operator==(const DesignDataTemplate&) const = default;
};

/**
 * A reusable visual tree for either a built-in control or an exact declarative
 * component QName. Keyless definitions are implicit and match the actual XAML
 * type identity; keyed definitions are selected through the Template property.
 */
struct DesignControlTemplate
{
	std::wstring Key;
	/** Built-in target, or the declarative component's runtime BaseType. */
	UIClass TargetType = UIClass::UI_Base;
	/** Non-empty when TargetType names an exact ComponentDefinition QName. */
	DesignerComponentType TargetComponentType;
	std::vector<DesignNode> Template;
	std::vector<DesignerVisualStateGroup> VisualStateGroups;
	std::vector<DesignerEventTrigger> EventTriggers;
	std::wstring SourceDictionary;
	bool IsImplicit() const noexcept { return Key.empty(); }
	bool HasSameResourceIdentity(const DesignControlTemplate& other) const noexcept;
	std::wstring DisplayName() const;

	bool operator==(const DesignControlTemplate&) const = default;
};

/** One keyed ItemsControl layout policy. It contains no authored visual nodes. */
struct DesignItemsPanelTemplate
{
	std::wstring Key;
	ItemsPanelTemplate Value;
	std::wstring SourceDictionary;

	bool operator==(const DesignItemsPanelTemplate&) const = default;
};

/** One declarative record in a strongly typed DataList resource. */
struct DesignDataRecord
{
	/** Canonical DataType property path -> editable literal text. */
	std::map<std::wstring, std::wstring> Fields;

	bool operator==(const DesignDataRecord&) const = default;
};

/** Embedded observable item source usable at runtime and in Designer preview. */
struct DesignDataList
{
	std::wstring Key;
	std::wstring ItemType;
	std::vector<DesignDataRecord> Records;
	std::wstring SourceDictionary;

	bool operator==(const DesignDataList&) const = default;
};

/** One declarative sort key applied in authored order. */
struct DesignCollectionSortDescription
{
	std::wstring PropertyName;
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;

	bool operator==(const DesignCollectionSortDescription&) const = default;
};

/** One grouping key. Authored order defines the hierarchy depth. */
struct DesignCollectionGroupDescription
{
	std::wstring PropertyName;
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;

	bool operator==(const DesignCollectionGroupDescription&) const = default;
};

/** One named per-group aggregate exposed under CollectionViewGroup.Aggregates. */
struct DesignCollectionAggregateDescription
{
	std::wstring Name;
	std::wstring PropertyName;
	CollectionAggregateFunction Function = CollectionAggregateFunction::Count;

	bool operator==(const DesignCollectionAggregateDescription&) const = default;
};

/** One typed predicate; all predicates in a view are combined with AND. */
struct DesignCollectionFilterDescription
{
	std::wstring PropertyName;
	CollectionFilterOperator Operator = CollectionFilterOperator::Equals;
	/** Authored literal converted through the source DataType metadata. */
	std::wstring Value;
	bool IgnoreCase = true;

	bool operator==(const DesignCollectionFilterDescription&) const = default;
};

/** Reusable filtered/sorted/current-item projection over a binding list. */
struct DesignCollectionViewSource
{
	std::wstring Key;
	/** Exactly one source form is used: a resource key or DataContext path. */
	std::wstring SourceResource;
	std::wstring SourceBindingPath;
	std::vector<DesignCollectionGroupDescription> GroupDescriptions;
	std::vector<DesignCollectionAggregateDescription> AggregateDescriptions;
	std::vector<DesignCollectionSortDescription> SortDescriptions;
	std::vector<DesignCollectionFilterDescription> FilterDescriptions;
	std::wstring SourceDictionary;

	bool operator==(const DesignCollectionViewSource&) const = default;
};

/** Keyed presentation policy for CollectionViewSource group headers. */
struct DesignGroupStyle
{
	std::wstring Key;
	std::wstring HeaderTemplate;
	std::wstring SourceDictionary;

	bool operator==(const DesignGroupStyle&) const = default;
};

struct DesignDocument
{
	static constexpr int CurrentSchemaVersion = 43;
	DesignDocument();
	std::string Schema = "cui.designer";
	int SchemaVersion = CurrentSchemaVersion;
	int NextStableId = 1;
	/** The XAML root is an ordinary schema node; Content remains in Nodes. */
	DesignNode Window;
	DesignCodeBehindModel CodeBehind;
	DesignerDataContextSchema DataContextSchema;
	DesignerStyleSheet StyleSheet;
	std::vector<DesignComponentDefinition> Components;
	std::vector<DesignControlTemplate> ControlTemplates;
	std::vector<DesignDataTypeDefinition> DataTypes;
	std::vector<DesignDataTemplate> DataTemplates;
	std::vector<DesignItemsPanelTemplate> ItemsPanelTemplates;
	std::vector<DesignGroupStyle> GroupStyles;
	std::vector<DesignDataList> DataLists;
	std::vector<DesignCollectionViewSource> CollectionViews;
	std::vector<DesignNode> Nodes;
	/** Frontend-only source map; excluded from snapshots and semantic equality. */
	XamlDocumentSourceMap Sources;
	const DesignComponentDefinition* FindComponent(
		const DesignerComponentType& type) const;
	const DesignComponentDefinition* FindComponent(
		const std::wstring& xamlNamespace,
		const std::wstring& xamlName) const;
	const DesignComponentDefinition* FindComponent(
		const DesignNode& origin,
		const DesignerComponentType& type) const;
	const DesignComponentDefinition* FindComponent(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin,
		const DesignerComponentType& type) const;
	/** True when a component VisualState resolves at least one StaticResource. */
	bool HasResourceBackedVisualStates() const noexcept;
	const DesignDataTypeDefinition* FindDataType(
		const std::wstring& name) const;
	const DesignDataTemplate* FindDataTemplate(
		const std::wstring& key) const;
	const DesignDataTemplate* FindDataTemplate(
		const DesignNode& origin,
		const std::wstring& key) const;
	const DesignControlTemplate* FindControlTemplate(
		const std::wstring& key) const;
	const DesignControlTemplate* FindControlTemplate(
		const DesignNode& origin, const std::wstring& key) const;
	const DesignControlTemplate* FindControlTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, const std::wstring& key) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		UIClass targetType) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		const DesignerComponentType& targetType) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		const DesignNode& origin, UIClass targetType) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		const DesignNode& origin,
		const DesignerComponentType& targetType) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, UIClass targetType) const;
	const DesignControlTemplate* FindImplicitControlTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin,
		const DesignerComponentType& targetType) const;
	const DesignDataTemplate* FindDataTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin,
		const std::wstring& key) const;
	/** Resolves a keyless DataTemplate by DataType. */
	const DesignDataTemplate* FindImplicitDataTemplate(
		const std::wstring& dataType) const;
	const DesignDataTemplate* FindImplicitDataTemplate(
		const DesignNode& origin,
		const std::wstring& dataType) const;
	const DesignDataTemplate* FindImplicitDataTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin,
		const std::wstring& dataType) const;
	/** Returns document plus root-to-origin structural resources with shadowing. */
	DesignObjectResourceDictionary VisibleObjectResources(
		const DesignNode& origin) const;
	DesignObjectResourceDictionary VisibleObjectResources(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin) const;
	const DesignItemsPanelTemplate* FindItemsPanelTemplate(
		const std::wstring& key) const;
	const DesignItemsPanelTemplate* FindItemsPanelTemplate(
		const DesignNode& origin, const std::wstring& key) const;
	const DesignItemsPanelTemplate* FindItemsPanelTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, const std::wstring& key) const;
	const DesignGroupStyle* FindGroupStyle(const std::wstring& key) const;
	const DesignGroupStyle* FindGroupStyle(
		const DesignNode& origin, const std::wstring& key) const;
	const DesignGroupStyle* FindGroupStyle(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, const std::wstring& key) const;
	/** Returns the declaring node for a lexically local GroupStyle, else null. */
	const DesignNode* FindLocalGroupStyleOwner(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, const std::wstring& key) const;
	/** Resolves explicit or implicit HeaderTemplate at the declaration scope. */
	const DesignDataTemplate* FindGroupStyleHeaderTemplate(
		const std::vector<DesignNode>& scopeNodes,
		const DesignNode& origin, const std::wstring& groupStyleKey) const;
	const DesignDataList* FindDataList(const std::wstring& key) const;
	const DesignCollectionViewSource* FindCollectionView(
		const std::wstring& key) const;
	/**
	 * Non-persisted directory used to resolve relative XAML resource URIs.
	 * File frontends set it to the source document directory; equality ignores it.
	 */
	std::wstring ResourceBasePath;
	/** Non-persisted resolver snapshot and dependency collector for this load. */
	std::shared_ptr<ResourceLoadContext> Resources;
	std::vector<ResourceDependency> ResourceDependencies() const
	{
		return Resources ? Resources->Dependencies()
			: std::vector<ResourceDependency>{};
	}

	int AllocateNodeId();
	void RecalculateNextStableId();
	/** Validates every authored CommandTarget in every independent namescope. */
	bool ValidateCommandTargetReferences(
		std::wstring* outError = nullptr) const;
	void Clear();
	bool operator==(const DesignDocument& other) const;
};
}

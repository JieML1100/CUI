#pragma once

#include "../DesignerTypes.h"
#include "../DesignerStyleSheet.h"
#include "DesignValue.h"
#include "../../CUI/include/CollectionViewSource.h"
#include "../../CUI/include/ItemsPanelTemplate.h"
#include "../../CUI/include/Resource.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace DesignerModel
{
struct DesignFormModel
{
	std::wstring Name = L"MainForm";
	std::wstring Text = L"Form";
	std::wstring FontName;
	float FontSize = 18.0f;
	SIZE Size{ 800, 600 };
	POINT Location{ 100, 100 };
	D2D1_COLOR_F BackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ForeColor = Colors::Black;
	bool ShowInTaskBar = true;
	bool TopMost = false;
	bool Enable = true;
	bool Visible = true;
	bool VisibleHead = true;
	int HeadHeight = 24;
	bool MinBox = true;
	bool MaxBox = true;
	bool CloseBox = true;
	bool CenterTitle = true;
	bool AllowResize = true;
	std::map<std::wstring, std::wstring> EventHandlers;

	bool operator==(const DesignFormModel& other) const;
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

struct DesignNode
{
	int Id = 0;
	int ParentId = 0;
	std::wstring ParentRef;
	std::wstring Name;
	UIClass Type = UIClass::UI_Base;
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
	DesignValue Props = DesignValue::object();
	DesignValue Extra = DesignValue::object();
	DesignValue Events = DesignValue::object();
	DesignValue Bindings = DesignValue::object();
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
	UIClass BaseType = UIClass::UI_Panel;
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
	float HeaderIndent = 16.0f;
	float HeaderSpacing = 4.0f;
	float HeaderHeight = 24.0f;
	std::wstring SourceDictionary;

	bool operator==(const DesignGroupStyle&) const = default;
};

struct DesignDocument
{
	static constexpr int CurrentSchemaVersion = 29;
	std::string Schema = "cui.designer";
	int SchemaVersion = CurrentSchemaVersion;
	int NextStableId = 1;
	DesignFormModel Form;
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
	void Clear();
	bool operator==(const DesignDocument& other) const;
};
}

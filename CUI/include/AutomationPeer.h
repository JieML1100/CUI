#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Control;
struct AccessibilityScrollInfo;
struct AccessibilityVirtualContainerInfo;
struct AccessibilityVirtualNode;
enum class AccessibilityScrollAmount : uint8_t;
enum class AccessibilitySelectionAction : uint8_t;

/**
 * Platform-independent automation control types.
 *
 * These names intentionally follow WPF/UI Automation rather than MSAA roles.
 * Native bridges translate them to the platform constants at their boundary.
 */
enum class AutomationControlType : uint8_t
{
	Custom,
	Window,
	Pane,
	Group,
	Text,
	Hyperlink,
	Button,
	CheckBox,
	RadioButton,
	Edit,
	ComboBox,
	List,
	ListItem,
	DataGrid,
	Tree,
	TreeItem,
	DataItem,
	HeaderItem,
	Tab,
	TabItem,
	Menu,
	MenuItem,
	ToolBar,
	StatusBar,
	Slider,
	Spinner,
	ProgressBar,
	ScrollBar,
	Image,
	Document,
	Separator,
	Calendar
};

/** Capability set returned by an AutomationPeer. */
enum class AutomationPattern : uint32_t
{
	None = 0,
	Invoke = 1u << 0,
	Toggle = 1u << 1,
	Value = 1u << 2,
	RangeValue = 1u << 3,
	ExpandCollapse = 1u << 4,
	SelectionItem = 1u << 5,
	Selection = 1u << 6,
	Grid = 1u << 7,
	Table = 1u << 8,
	Scroll = 1u << 9,
	ScrollItem = 1u << 10,
	VirtualizedItem = 1u << 11,
	GridItem = 1u << 12,
	TableItem = 1u << 13
};

inline AutomationPattern operator|(
	AutomationPattern left, AutomationPattern right) noexcept
{
	return static_cast<AutomationPattern>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

inline AutomationPattern& operator|=(
	AutomationPattern& left, AutomationPattern right) noexcept
{
	left = left | right;
	return left;
}

inline bool HasAutomationPattern(
	AutomationPattern value, AutomationPattern pattern) noexcept
{
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(pattern))
		== static_cast<uint32_t>(pattern);
}

/** Platform-neutral result for an automation operation. */
enum class AutomationOperationResult : uint8_t
{
	Succeeded,
	NotSupported,
	InvalidOperation,
	ElementNotEnabled,
	InvalidArgument,
	ElementNotAvailable
};

/** Platform-neutral WPF/UIA TogglePattern state. */
enum class AutomationToggleState : uint8_t
{
	Off,
	On,
	Indeterminate
};

struct AutomationRangeValue
{
	double Value = 0.0;
	double Minimum = 0.0;
	double Maximum = 0.0;
	double SmallChange = 0.0;
	double LargeChange = 0.0;
	bool IsReadOnly = true;
};

/**
 * WPF-style behavior boundary between one Control and native automation APIs.
 *
 * A peer is created lazily by Control::OnCreateAutomationPeer. It owns no
 * platform objects and never registers a XAML type; it only exposes the
 * semantic behavior supplied by the native C++ control implementation.
 */
class AutomationPeer
{
public:
	AutomationPeer(Control& owner, AutomationControlType controlType,
		std::wstring fallbackClassName);
	virtual ~AutomationPeer() = default;

	Control& Owner() const noexcept { return _owner; }
	virtual AutomationControlType GetAutomationControlType() const noexcept;
	virtual std::wstring GetAutomationClassName() const;
	virtual AutomationPattern GetPatternSet() const noexcept;
	bool SupportsPattern(AutomationPattern pattern) const noexcept
	{
		return HasAutomationPattern(GetPatternSet(), pattern);
	}

	virtual bool IsPassword() const noexcept { return false; }
	virtual std::wstring GetValue() const;
	virtual bool IsReadOnly() const;

	virtual AutomationOperationResult Invoke();
	virtual AutomationOperationResult Toggle();
	virtual bool TryGetToggleState(AutomationToggleState& value) const;
	virtual AutomationOperationResult SetValue(const std::wstring& value);
	virtual bool TryGetRangeValue(AutomationRangeValue& value) const;
	virtual AutomationOperationResult SetRangeValue(double value);
	virtual bool TryGetExpanded(bool& value) const;
	virtual AutomationOperationResult SetExpanded(bool value);
	virtual bool TryGetSelectionItemSelected(bool& value) const;
	virtual AutomationOperationResult Select();
	virtual Control* GetSelectionContainer() const;
	virtual Control* GetSelectedItem() const;
	virtual bool CanSelectMultiple() const noexcept { return false; }
	virtual bool IsSelectionRequired() const noexcept { return false; }

	/**
	 * Logical automation children that do not have Control instances of their
	 * own. Native bridges ask the peer exclusively; a Control never implements
	 * a parallel platform-provider interface.
	 */
	virtual bool SupportsVirtualizedChildren() const noexcept { return false; }
	virtual bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result);
	virtual size_t GetAccessibilityVirtualChildCount(uint32_t parentId);
	virtual bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result);
	virtual bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result);
	virtual bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result);
	void GetAccessibilityVirtualChildren(
		uint32_t parentId, std::vector<uint32_t>& result);
	virtual AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept;
	virtual void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result);
	virtual bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& result);
	virtual void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result);
	virtual void GetAccessibilityVirtualRowHeaders(
		std::vector<uint32_t>& result);
	virtual AutomationOperationResult FocusAccessibilityVirtualNode(uint32_t id);
	virtual bool TryGetAccessibilityVirtualFocusedNode(uint32_t& result);
	virtual bool InvokeAccessibilityVirtualNode(uint32_t id);
	virtual bool ToggleAccessibilityVirtualNode(uint32_t id);
	virtual bool SetAccessibilityVirtualNodeValue(
		uint32_t id, const std::wstring& value);
	virtual bool SetAccessibilityVirtualNodeExpanded(
		uint32_t id, bool expanded);
	virtual bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action);
	virtual bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id);
	virtual bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept;
	virtual bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical);
	virtual bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent);

private:
	Control& _owner;
	AutomationControlType _controlType = AutomationControlType::Custom;
	std::wstring _fallbackClassName;
};

class InvokeAutomationPeer : public AutomationPeer
{
public:
	InvokeAutomationPeer(Control& owner, AutomationControlType controlType,
		std::wstring fallbackClassName);
	AutomationPattern GetPatternSet() const noexcept override;
	AutomationOperationResult Invoke() override;
};

/** WPF ScrollViewerAutomationPeer projection of the UIA Scroll pattern. */
class ScrollViewerAutomationPeer final : public AutomationPeer
{
public:
	explicit ScrollViewerAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
};

/** WPF CalendarAutomationPeer projection for CalendarView's native date grid. */
class CalendarViewAutomationPeer final : public AutomationPeer
{
public:
	explicit CalendarViewAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool SupportsVirtualizedChildren() const noexcept override { return true; }
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& result) override;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) override;
	bool InvokeAccessibilityVirtualNode(uint32_t id) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
};

/**
 * WPF DataGridAutomationPeer projection for DataGrid's logical row/cell tree.
 *
 * Rows and cells remain available to UI Automation when their visual
 * containers are virtualized.  DataGrid owns their stable identities and all
 * mutations; this peer only publishes that semantic surface to native bridges.
 */
class DataGridAutomationPeer final : public AutomationPeer
{
public:
	explicit DataGridAutomationPeer(Control& owner);
	std::wstring GetAutomationClassName() const override;
	AutomationPattern GetPatternSet() const noexcept override;
	bool CanSelectMultiple() const noexcept override;
	bool SupportsVirtualizedChildren() const noexcept override { return true; }
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& result) override;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) override;
	void GetAccessibilityVirtualRowHeaders(
		std::vector<uint32_t>& result) override;
	AutomationOperationResult FocusAccessibilityVirtualNode(uint32_t id) override;
	bool TryGetAccessibilityVirtualFocusedNode(uint32_t& result) override;
	bool InvokeAccessibilityVirtualNode(uint32_t id) override;
	bool SetAccessibilityVirtualNodeValue(
		uint32_t id, const std::wstring& value) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
};

/** Accessible projection for ChartView's retained data-point renderer. */
class ChartViewAutomationPeer final : public AutomationPeer
{
public:
	explicit ChartViewAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool SupportsVirtualizedChildren() const noexcept override { return true; }
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool InvokeAccessibilityVirtualNode(uint32_t id) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
};

class ToggleAutomationPeer : public AutomationPeer
{
public:
	ToggleAutomationPeer(Control& owner, AutomationControlType controlType,
		std::wstring fallbackClassName);
	AutomationPattern GetPatternSet() const noexcept override;
	AutomationOperationResult Toggle() override;
	bool TryGetToggleState(AutomationToggleState& value) const override;
};

/** WPF MenuItem peer: leaf invocation, check toggling, or submenu expansion. */
class MenuItemAutomationPeer final : public AutomationPeer
{
public:
	explicit MenuItemAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	AutomationOperationResult Invoke() override;
	AutomationOperationResult Toggle() override;
	bool TryGetToggleState(AutomationToggleState& value) const override;
	bool TryGetExpanded(bool& value) const override;
	AutomationOperationResult SetExpanded(bool value) override;
};

class RadioButtonAutomationPeer final : public AutomationPeer
{
public:
	explicit RadioButtonAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool TryGetSelectionItemSelected(bool& value) const override;
	AutomationOperationResult Select() override;
	Control* GetSelectionContainer() const override;
};

class TextBoxAutomationPeer final : public AutomationPeer
{
public:
	TextBoxAutomationPeer(Control& owner, std::wstring fallbackClassName,
		bool password = false);
	AutomationPattern GetPatternSet() const noexcept override;
	bool IsPassword() const noexcept override { return _password; }
	std::wstring GetValue() const override;
	bool IsReadOnly() const override;
	AutomationOperationResult SetValue(const std::wstring& value) override;

private:
	bool _password = false;
};

class RangeBaseAutomationPeer final : public AutomationPeer
{
public:
	RangeBaseAutomationPeer(Control& owner, AutomationControlType controlType,
		std::wstring fallbackClassName, bool readOnly = false);
	AutomationPattern GetPatternSet() const noexcept override;
	std::wstring GetValue() const override;
	bool IsReadOnly() const override { return _readOnly; }
	bool TryGetRangeValue(AutomationRangeValue& value) const override;
	AutomationOperationResult SetRangeValue(double value) override;

private:
	bool _readOnly = false;
};

class ListBoxAutomationPeer final : public AutomationPeer
{
public:
	explicit ListBoxAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	Control* GetSelectedItem() const override;
	bool CanSelectMultiple() const noexcept override;
};

class ListBoxItemAutomationPeer final : public AutomationPeer
{
public:
	explicit ListBoxItemAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool TryGetSelectionItemSelected(bool& value) const override;
	AutomationOperationResult Select() override;
	Control* GetSelectionContainer() const override;
};

class ComboBoxAutomationPeer final : public AutomationPeer
{
public:
	explicit ComboBoxAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	std::wstring GetValue() const override;
	bool IsReadOnly() const override;
	AutomationOperationResult SetValue(
		const std::wstring& value) override;
	bool TryGetExpanded(bool& value) const override;
	AutomationOperationResult SetExpanded(bool value) override;
	Control* GetSelectedItem() const override;
	bool SupportsVirtualizedChildren() const noexcept override { return true; }
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
};

class ExpanderAutomationPeer final : public AutomationPeer
{
public:
	explicit ExpanderAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool TryGetExpanded(bool& value) const override;
	AutomationOperationResult SetExpanded(bool value) override;
};

class TreeViewItemAutomationPeer final : public AutomationPeer
{
public:
	explicit TreeViewItemAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool TryGetExpanded(bool& value) const override;
	AutomationOperationResult SetExpanded(bool value) override;
	bool TryGetSelectionItemSelected(bool& value) const override;
	AutomationOperationResult Select() override;
	Control* GetSelectionContainer() const override;
};

class TreeViewAutomationPeer final : public AutomationPeer
{
public:
	explicit TreeViewAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	Control* GetSelectedItem() const override;
};

class TabItemAutomationPeer final : public AutomationPeer
{
public:
	explicit TabItemAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	bool TryGetSelectionItemSelected(bool& value) const override;
	AutomationOperationResult Select() override;
	Control* GetSelectionContainer() const override;
};

class TabControlAutomationPeer final : public AutomationPeer
{
public:
	explicit TabControlAutomationPeer(Control& owner);
	AutomationPattern GetPatternSet() const noexcept override;
	Control* GetSelectedItem() const override;
	bool IsSelectionRequired() const noexcept override { return true; }
};

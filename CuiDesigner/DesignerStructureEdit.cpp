#include "DesignerStructureEdit.h"

#include "../CUI/include/ComboBox.h"
#include "../CUI/include/GridView.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/Layout/GridPanel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace
{
	constexpr size_t MaxTreeNodes = 1'000'000;
	constexpr size_t MaxTreeDepth = 256;
	constexpr size_t MaxMenuItems = 1'000'000;
	constexpr size_t MaxMenuDepth = 256;

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(value)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t TreeMemory(const DesignerTreeNodeSnapshot& node) noexcept
	{
		size_t result = sizeof(node) + StringMemory(node.Text)
			+ node.Children.capacity() * sizeof(DesignerTreeNodeSnapshot);
		for (const auto& child : node.Children) result += TreeMemory(child);
		return result;
	}

	size_t MenuMemory(const DesignerMenuItemSnapshot& item) noexcept
	{
		size_t result = sizeof(item) + StringMemory(item.Text)
			+ StringMemory(item.Shortcut)
			+ item.Children.capacity() * sizeof(DesignerMenuItemSnapshot);
		for (const auto& child : item.Children) result += MenuMemory(child);
		return result;
	}

	DesignerTreeNodeSnapshot CaptureTreeNode(const TreeNode& node)
	{
		DesignerTreeNodeSnapshot result;
		result.Text = node.Text;
		result.Expand = node.Expand;
		result.Children.reserve(node.Children.size());
		for (const auto* child : node.Children)
			if (child) result.Children.push_back(CaptureTreeNode(*child));
		return result;
	}

	std::unique_ptr<TreeNode> BuildTreeNode(
		const DesignerTreeNodeSnapshot& snapshot,
		size_t depth,
		size_t& count,
		std::wstring* outError)
	{
		if (depth > MaxTreeDepth || ++count > MaxTreeNodes)
		{
			Fail(L"TreeView 节点快照超过深度或数量限制。", outError);
			return nullptr;
		}
		auto node = std::make_unique<TreeNode>(snapshot.Text);
		node->Expand = snapshot.Expand;
		for (const auto& childSnapshot : snapshot.Children)
		{
			auto child = BuildTreeNode(
				childSnapshot, depth + 1, count, outError);
			if (!child || !node->AddChild(std::move(child)))
			{
				if (outError && outError->empty())
					*outError = L"TreeView 节点快照包含无效所有权关系。";
				return nullptr;
			}
		}
		return node;
	}

	bool CaptureMenuItem(
		MenuItem& item,
		DesignerMenuItemSnapshot& output,
		size_t depth,
		size_t& count,
		std::wstring* outError)
	{
		if (depth > MaxMenuDepth || ++count > MaxMenuItems)
			return Fail(L"Menu Items 快照超过深度或数量限制。", outError);
		DesignerMenuItemSnapshot candidate;
		candidate.Text = item.Text;
		candidate.Id = item.Id;
		candidate.Shortcut = item.Shortcut;
		candidate.Separator = item.Separator;
		candidate.Enable = item.Enable;
		candidate.Children.reserve(item.SubItems.size());
		for (auto* child : item.SubItems)
		{
			if (!child)
				return Fail(L"Menu Items 包含空子项。", outError);
			DesignerMenuItemSnapshot saved;
			if (!CaptureMenuItem(
				*child, saved, depth + 1, count, outError)) return false;
			candidate.Children.push_back(std::move(saved));
		}
		output = std::move(candidate);
		return true;
	}

	std::unique_ptr<MenuItem> BuildMenuItem(
		const DesignerMenuItemSnapshot& snapshot,
		size_t depth,
		size_t& count,
		std::wstring* outError)
	{
		if (depth > MaxMenuDepth || ++count > MaxMenuItems)
		{
			Fail(L"Menu Items 快照超过深度或数量限制。", outError);
			return nullptr;
		}
		std::unique_ptr<MenuItem> item(snapshot.Separator
			? MenuItem::CreateSeparator()
			: new MenuItem(snapshot.Text, snapshot.Id));
		if (!item)
		{
			Fail(L"无法创建 MenuItem。", outError);
			return nullptr;
		}
		item->Text = snapshot.Text;
		item->Id = snapshot.Id;
		item->Shortcut = snapshot.Shortcut;
		item->Separator = snapshot.Separator;
		item->Enable = snapshot.Enable;
		for (const auto& childSnapshot : snapshot.Children)
		{
			auto child = BuildMenuItem(
				childSnapshot, depth + 1, count, outError);
			if (!child || !item->AddSubItem(std::move(child)))
			{
				if (outError && outError->empty())
					*outError = L"MenuItem 拒绝恢复子项所有权。";
				return nullptr;
			}
		}
		return item;
	}

	bool ValidTrack(const DesignerGridTrackSnapshot& track) noexcept
	{
		return track.Unit >= static_cast<int>(SizeUnit::Pixel)
			&& track.Unit <= static_cast<int>(SizeUnit::Star)
			&& std::isfinite(track.Value)
			&& std::isfinite(track.Minimum)
			&& std::isfinite(track.Maximum)
			&& track.Minimum >= 0.0f
			&& track.Maximum >= track.Minimum;
	}

	const DesignerDataBinding* FindSelectedIndexBinding(
		const DesignerControl& control) noexcept
	{
		const auto found = std::find_if(
			control.DataBindings.begin(), control.DataBindings.end(),
			[](const auto& entry)
			{
				return _wcsicmp(
					entry.first.c_str(), L"SelectedIndex") == 0;
			});
		return found == control.DataBindings.end()
			? nullptr : &found->second;
	}

	bool CaptureComboBox(
		const DesignerControl& control,
		DesignerComboBoxSnapshot& output,
		std::wstring* outError)
	{
		auto* combo = dynamic_cast<ComboBox*>(control.ControlInstance);
		if (!combo || control.Type != UIClass::UI_ComboBox)
			return Fail(L"ComboBox Items 差量与目标控件类型不匹配。", outError);

		DesignerComboBoxSnapshot candidate;
		candidate.Items.assign(combo->Items.begin(), combo->Items.end());
		BindingValue value;
		if (!combo->TryGetPropertyValue(L"SelectedIndex", value)
			|| !value.TryGetInt(candidate.EffectiveSelectedIndex))
			return Fail(L"无法捕获 ComboBox.SelectedIndex 有效值。", outError);

		candidate.HasLocalSelectedIndex = combo->TryGetPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Local, value);
		if (candidate.HasLocalSelectedIndex
			&& !value.TryGetInt(candidate.LocalSelectedIndex))
			return Fail(L"ComboBox.SelectedIndex Local 值类型无效。", outError);

		candidate.HasBindingSelectedIndex = combo->TryGetPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Binding, value);
		if (candidate.HasBindingSelectedIndex
			&& !value.TryGetInt(candidate.BindingSelectedIndex))
			return Fail(L"ComboBox.SelectedIndex Binding 值类型无效。", outError);

		if (const auto* binding = FindSelectedIndexBinding(control))
		{
			candidate.HasConfiguredBinding = true;
			candidate.ConfiguredBinding = *binding;
		}
		const auto tracked = control.MetadataProperties.find(L"SelectedIndex");
		if (tracked != control.MetadataProperties.end())
		{
			candidate.HasTrackedSelectedIndex = true;
			candidate.TrackedSelectedIndex = tracked->second;
		}
		output = std::move(candidate);
		return true;
	}

	bool RestoreComboBox(
		DesignerControl& control,
		const DesignerComboBoxSnapshot& snapshot,
		std::wstring* outError)
	{
		auto* combo = dynamic_cast<ComboBox*>(control.ControlInstance);
		if (!combo || control.Type != UIClass::UI_ComboBox)
			return Fail(L"ComboBox Items 差量与目标控件类型不匹配。", outError);

		const auto* configured = FindSelectedIndexBinding(control);
		if ((configured != nullptr) != snapshot.HasConfiguredBinding
			|| (configured && *configured != snapshot.ConfiguredBinding))
			return Fail(L"ComboBox.SelectedIndex Binding 配置已经变化。", outError);

		combo->Items = snapshot.Items;
		if (snapshot.HasBindingSelectedIndex)
		{
			BindingValue currentBinding;
			if (!combo->TryGetPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Binding,
				currentBinding))
				return Fail(L"ComboBox.SelectedIndex Binding 值来源已经消失。", outError);
			if (combo->HasPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Local)
				&& !combo->ClearPropertyValue(
					L"SelectedIndex", ControlPropertyValueSource::Local))
				return Fail(L"无法暂时移除 ComboBox.SelectedIndex Local 值。", outError);
			if (!combo->TrySetCurrentPropertyValue(
				L"SelectedIndex", snapshot.BindingSelectedIndex))
				return Fail(L"无法恢复 ComboBox.SelectedIndex Binding 值。", outError);
		}
		else if (combo->HasPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Binding))
		{
			return Fail(L"ComboBox.SelectedIndex 出现了意外的 Binding 值来源。", outError);
		}

		if (snapshot.HasLocalSelectedIndex)
		{
			if (!combo->TrySetPropertyValue(
				L"SelectedIndex", snapshot.LocalSelectedIndex,
				ControlPropertyValueSource::Local))
				return Fail(L"无法恢复 ComboBox.SelectedIndex Local 值。", outError);
		}
		else if (combo->HasPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Local)
			&& !combo->ClearPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Local))
		{
			return Fail(L"无法清除 ComboBox.SelectedIndex Local 值。", outError);
		}

		if (snapshot.HasTrackedSelectedIndex)
			control.MetadataProperties[L"SelectedIndex"] =
				snapshot.TrackedSelectedIndex;
		else
			control.MetadataProperties.erase(L"SelectedIndex");

		DesignerComboBoxSnapshot actual;
		std::wstring verifyError;
		if (!CaptureComboBox(control, actual, &verifyError))
			return Fail(L"无法验证 ComboBox Items 恢复结果：" + verifyError, outError);
		if (actual != snapshot)
			return Fail(L"ComboBox Items setter 未恢复到请求的精确状态。", outError);
		return true;
	}
}

size_t DesignerComboBoxSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this)
		+ Items.capacity() * sizeof(std::wstring)
		+ ConfiguredBinding.SourceProperty.capacity() * sizeof(wchar_t)
		+ ConfiguredBinding.Converter.capacity() * sizeof(wchar_t)
		+ TrackedSelectedIndex.Text.capacity() * sizeof(wchar_t);
	for (const auto& item : Items) result += StringMemory(item);
	return result;
}

size_t DesignerStructureSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this) + StringMemory(TargetName)
		+ ComboBox.GetEstimatedMemoryUsage()
		+ GridViewColumns.capacity()
			* sizeof(DesignerGridViewColumnSnapshot)
		+ TreeNodes.capacity() * sizeof(DesignerTreeNodeSnapshot)
		+ MenuItems.capacity() * sizeof(DesignerMenuItemSnapshot)
		+ GridRows.capacity() * sizeof(DesignerGridTrackSnapshot)
		+ GridColumns.capacity() * sizeof(DesignerGridTrackSnapshot)
		+ StatusBarParts.capacity()
			* sizeof(DesignerStatusBarPartSnapshot);
	for (const auto& column : GridViewColumns)
	{
		result += StringMemory(column.Name) + StringMemory(column.ButtonText)
			+ column.ComboBoxItems.capacity() * sizeof(std::wstring);
		for (const auto& item : column.ComboBoxItems)
			result += StringMemory(item);
	}
	for (const auto& node : TreeNodes) result += TreeMemory(node);
	for (const auto& item : MenuItems) result += MenuMemory(item);
	for (const auto& part : StatusBarParts) result += StringMemory(part.Text);
	return result;
}

bool DesignerStructureEdit::SupportsDelta(
	DesignerCustomEditorKind kind) noexcept
{
	return kind == DesignerCustomEditorKind::GridViewColumns
		|| kind == DesignerCustomEditorKind::ComboBoxItems
		|| kind == DesignerCustomEditorKind::TreeViewNodes
		|| kind == DesignerCustomEditorKind::MenuItems
		|| kind == DesignerCustomEditorKind::GridPanelDefinitions
		|| kind == DesignerCustomEditorKind::StatusBarParts;
}

bool DesignerStructureEdit::Capture(
	const DesignerControl& control,
	DesignerCustomEditorKind kind,
	DesignerStructureSnapshot& output,
	std::wstring* outError)
{
	if (!SupportsDelta(kind))
		return Fail(L"该结构编辑器需要完整文档事务。", outError);
	if (!control.ControlInstance || control.StableId < 1
		|| control.Name.empty())
		return Fail(L"结构差量的目标控件身份无效。", outError);

	try
	{
		DesignerStructureSnapshot candidate;
		candidate.StableId = control.StableId;
		candidate.TargetName = control.Name;
		candidate.TargetType = control.Type;
		candidate.Kind = kind;
		switch (kind)
		{
		case DesignerCustomEditorKind::ComboBoxItems:
			if (!CaptureComboBox(control, candidate.ComboBox, outError))
				return false;
			break;
		case DesignerCustomEditorKind::GridViewColumns:
		{
			auto* grid = dynamic_cast<GridView*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_GridView)
				return Fail(L"GridView 列差量与目标控件类型不匹配。", outError);
			candidate.GridViewColumns.reserve(grid->ColumnCount());
			for (size_t index = 0; index < grid->ColumnCount(); ++index)
			{
				const auto& column = grid->ColumnAt(static_cast<int>(index));
				candidate.GridViewColumns.push_back({
					column.Name, column.Width, static_cast<int>(column.Type),
					column.CanEdit, column.ComboBoxItems, column.ButtonText });
			}
			break;
		}
		case DesignerCustomEditorKind::TreeViewNodes:
		{
			auto* tree = dynamic_cast<TreeView*>(control.ControlInstance);
			if (!tree || !tree->Root || control.Type != UIClass::UI_TreeView)
				return Fail(L"TreeView 节点差量与目标控件类型不匹配。", outError);
			candidate.TreeNodes.reserve(tree->Root->Children.size());
			for (const auto* node : tree->Root->Children)
				if (node) candidate.TreeNodes.push_back(CaptureTreeNode(*node));
			break;
		}
		case DesignerCustomEditorKind::MenuItems:
		{
			auto* menu = dynamic_cast<Menu*>(control.ControlInstance);
			if (!menu || control.Type != UIClass::UI_Menu)
				return Fail(L"Menu Items 差量与目标控件类型不匹配。", outError);
			candidate.MenuItems.reserve(
				static_cast<size_t>((std::max)(0, menu->Count)));
			size_t count = 0;
			for (int index = 0; index < menu->Count; ++index)
			{
				auto* item = dynamic_cast<MenuItem*>(menu->operator[](index));
				if (!item) return Fail(L"Menu 包含非 MenuItem 子控件。", outError);
				DesignerMenuItemSnapshot saved;
				if (!CaptureMenuItem(*item, saved, 0, count, outError))
					return false;
				candidate.MenuItems.push_back(std::move(saved));
			}
			break;
		}
		case DesignerCustomEditorKind::GridPanelDefinitions:
		{
			auto* grid = dynamic_cast<GridPanel*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_GridPanel)
				return Fail(L"GridPanel 定义差量与目标控件类型不匹配。", outError);
			candidate.GridRows.reserve(grid->GetRows().size());
			for (const auto& row : grid->GetRows())
				candidate.GridRows.push_back({ row.Height.Value,
					static_cast<int>(row.Height.Unit), row.MinHeight, row.MaxHeight });
			candidate.GridColumns.reserve(grid->GetColumns().size());
			for (const auto& column : grid->GetColumns())
				candidate.GridColumns.push_back({ column.Width.Value,
					static_cast<int>(column.Width.Unit), column.MinWidth, column.MaxWidth });
			break;
		}
		case DesignerCustomEditorKind::StatusBarParts:
		{
			auto* status = dynamic_cast<StatusBar*>(control.ControlInstance);
			if (!status || control.Type != UIClass::UI_StatusBar)
				return Fail(L"StatusBar 分段差量与目标控件类型不匹配。", outError);
			candidate.StatusBarParts.reserve(
				static_cast<size_t>((std::max)(0, status->PartCount())));
			for (int index = 0; index < status->PartCount(); ++index)
				candidate.StatusBarParts.push_back({
					status->GetPartText(index), status->GetPartWidth(index) });
			break;
		}
		default:
			return Fail(L"该结构编辑器需要完整文档事务。", outError);
		}
		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		return Fail(L"捕获结构差量时资源分配失败。", outError);
	}
}

bool DesignerStructureEdit::Restore(
	DesignerControl& control,
	const DesignerStructureSnapshot& snapshot,
	std::wstring* outError)
{
	if (!control.ControlInstance || control.StableId != snapshot.StableId
		|| control.Name != snapshot.TargetName
		|| control.Type != snapshot.TargetType)
		return Fail(L"结构差量的目标控件身份已经变化。", outError);
	try
	{
		switch (snapshot.Kind)
		{
		case DesignerCustomEditorKind::ComboBoxItems:
			if (!RestoreComboBox(control, snapshot.ComboBox, outError))
				return false;
			break;
		case DesignerCustomEditorKind::GridViewColumns:
		{
			auto* grid = dynamic_cast<GridView*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_GridView)
				return Fail(L"GridView 列差量与目标控件类型不匹配。", outError);
			std::vector<GridViewColumn> columns;
			columns.reserve(snapshot.GridViewColumns.size());
			for (const auto& saved : snapshot.GridViewColumns)
			{
				if (!std::isfinite(saved.Width) || saved.Width <= 0.0f
					|| saved.Type < static_cast<int>(ColumnType::Text)
					|| saved.Type > static_cast<int>(ColumnType::LinkedText))
					return Fail(L"GridView 列快照包含无效值。", outError);
				GridViewColumn column(saved.Name, saved.Width,
					static_cast<ColumnType>(saved.Type), saved.CanEdit);
				column.ComboBoxItems = saved.ComboBoxItems;
				column.ButtonText = saved.ButtonText;
				columns.push_back(std::move(column));
			}
			auto update = grid->DeferUpdates();
			grid->ClearColumns();
			for (const auto& column : columns) grid->AddColumn(column);
			break;
		}
		case DesignerCustomEditorKind::TreeViewNodes:
		{
			auto* tree = dynamic_cast<TreeView*>(control.ControlInstance);
			if (!tree || !tree->Root || control.Type != UIClass::UI_TreeView)
				return Fail(L"TreeView 节点差量与目标控件类型不匹配。", outError);
			std::vector<std::unique_ptr<TreeNode>> nodes;
			nodes.reserve(snapshot.TreeNodes.size());
			size_t count = 0;
			for (const auto& saved : snapshot.TreeNodes)
			{
				auto node = BuildTreeNode(saved, 0, count, outError);
				if (!node) return false;
				nodes.push_back(std::move(node));
			}
			tree->SelectedNode = nullptr;
			tree->HoveredNode = nullptr;
			tree->Root->ClearChildren();
			for (auto& node : nodes)
				if (!tree->Root->AddChild(std::move(node)))
					return Fail(L"TreeView 拒绝恢复节点所有权。", outError);
			break;
		}
		case DesignerCustomEditorKind::MenuItems:
		{
			auto* menu = dynamic_cast<Menu*>(control.ControlInstance);
			if (!menu || control.Type != UIClass::UI_Menu)
				return Fail(L"Menu Items 差量与目标控件类型不匹配。", outError);
			std::vector<std::unique_ptr<MenuItem>> items;
			items.reserve(snapshot.MenuItems.size());
			size_t count = 0;
			for (const auto& saved : snapshot.MenuItems)
			{
				auto item = BuildMenuItem(saved, 0, count, outError);
				if (!item) return false;
				items.push_back(std::move(item));
			}
			menu->ClearItems();
			for (auto& item : items)
				if (!menu->AddItem(std::move(item)))
					return Fail(L"Menu 拒绝恢复顶层项所有权。", outError);
			break;
		}
		case DesignerCustomEditorKind::GridPanelDefinitions:
		{
			auto* grid = dynamic_cast<GridPanel*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_GridPanel)
				return Fail(L"GridPanel 定义差量与目标控件类型不匹配。", outError);
			if (!std::all_of(snapshot.GridRows.begin(), snapshot.GridRows.end(), ValidTrack)
				|| !std::all_of(snapshot.GridColumns.begin(), snapshot.GridColumns.end(), ValidTrack))
				return Fail(L"GridPanel 定义快照包含无效值。", outError);
			grid->ClearRows();
			grid->ClearColumns();
			for (const auto& row : snapshot.GridRows)
				grid->AddRow(GridLength(row.Value, static_cast<SizeUnit>(row.Unit)),
					row.Minimum, row.Maximum);
			for (const auto& column : snapshot.GridColumns)
				grid->AddColumn(GridLength(column.Value, static_cast<SizeUnit>(column.Unit)),
					column.Minimum, column.Maximum);
			break;
		}
		case DesignerCustomEditorKind::StatusBarParts:
		{
			auto* status = dynamic_cast<StatusBar*>(control.ControlInstance);
			if (!status || control.Type != UIClass::UI_StatusBar)
				return Fail(L"StatusBar 分段差量与目标控件类型不匹配。", outError);
			status->ClearParts();
			for (const auto& part : snapshot.StatusBarParts)
				status->AddPart(part.Text, part.Width);
			status->LayoutItems();
			break;
		}
		default:
			return Fail(L"该结构编辑器需要完整文档事务。", outError);
		}
		control.ControlInstance->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		return Fail(L"恢复结构差量时资源分配失败。", outError);
	}
}

#include "ControlOwnedCollectionCommand.h"

#include "../../DesignerCanvas.h"
#include "../PropertyGridBinder.h"
#include "../../../CUI/include/Button.h"
#include "../../../CUI/include/TabControl.h"
#include "../../../CUI/include/ToolBar.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
	constexpr size_t MissingIndex = (std::numeric_limits<size_t>::max)();

	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(value)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	bool Fail(const std::wstring& message, std::wstring* outError)
	{
		if (outError) *outError = message;
		return false;
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

	std::wstring PrimarySelectionName(DesignerCanvas* canvas)
	{
		const auto selected = canvas ? canvas->GetSelectedControl() : nullptr;
		return selected ? selected->Name : std::wstring{};
	}
}

struct ControlOwnedCollectionCommand::Impl
{
	enum class Kind { TabPages, ToolBarButtons };

	struct SelectedIndexState
	{
		int Effective = 0;
		bool HasLocal = false;
		int Local = 0;
		bool HasBinding = false;
		int Binding = 0;
		bool HasConfiguredBinding = false;
		DesignerDataBinding ConfiguredBinding;
		bool HasTracked = false;
		DesignerStyleValue Tracked;

		bool operator==(const SelectedIndexState&) const = default;
	};

	struct Record
	{
		Control* Raw = nullptr;
		std::unique_ptr<Control> Owner;
		std::vector<size_t> Wrappers;
	};

	struct Wrapper
	{
		std::shared_ptr<DesignerControl> Value;
		size_t RecordIndex = MissingIndex;
	};

	struct Entry
	{
		size_t RecordIndex = MissingIndex;
		std::wstring Text;
		int DesignId = 0;
		bool HasControlSize = false;
		SIZE ControlSize{ 0, 0 };
		bool HasToolBarSizeOverride = false;
		SIZE ToolBarSizeOverride{ -1, -1 };

		bool operator==(const Entry& other) const noexcept
		{
			return RecordIndex == other.RecordIndex
				&& Text == other.Text
				&& DesignId == other.DesignId
				&& HasControlSize == other.HasControlSize
				&& ControlSize.cx == other.ControlSize.cx
				&& ControlSize.cy == other.ControlSize.cy
				&& HasToolBarSizeOverride == other.HasToolBarSizeOverride
				&& ToolBarSizeOverride.cx == other.ToolBarSizeOverride.cx
				&& ToolBarSizeOverride.cy == other.ToolBarSizeOverride.cy;
		}
	};

	struct State
	{
		std::vector<Entry> Entries;
		std::vector<size_t> WrapperFlatIndices;
		SelectedIndexState SelectedIndex;

		bool operator==(const State&) const = default;
	};

	DesignerCanvas* Canvas = nullptr;
	std::weak_ptr<DesignerControl> Target;
	Control* TargetInstance = nullptr;
	int TargetStableId = 0;
	std::wstring TargetName;
	UIClass TargetType = UIClass::UI_Base;
	Kind CollectionKind = Kind::TabPages;
	std::vector<Record> Records;
	std::vector<Wrapper> Wrappers;
	State Before;
	State After;
	std::vector<std::wstring> BeforeSelectionNames;
	std::vector<std::wstring> AfterSelectionNames;
	std::wstring BeforePrimarySelectionName;
	std::wstring AfterPrimarySelectionName;
	std::wstring Label;
	size_t EstimatedMemoryUsage = 0;

	std::shared_ptr<DesignerControl> ResolveTarget() const
	{
		if (!Canvas) return {};
		auto retained = Target.lock();
		if (retained && retained->ControlInstance == TargetInstance
			&& retained->StableId == TargetStableId
			&& retained->Name == TargetName
			&& retained->Type == TargetType)
			return retained;
		for (const auto& candidate : Canvas->_designerControls)
		{
			if (candidate && candidate->ControlInstance == TargetInstance
				&& candidate->StableId == TargetStableId
				&& candidate->Name == TargetName
				&& candidate->Type == TargetType)
				return candidate;
		}
		return {};
	}

	bool CaptureSelectedIndex(
		const DesignerControl& target,
		SelectedIndexState& output,
		std::wstring* outError) const
	{
		auto* tab = dynamic_cast<TabControl*>(target.ControlInstance);
		if (!tab) return Fail(L"TabControl 目标已经失效。", outError);
		SelectedIndexState candidate;
		BindingValue value;
		if (!tab->TryGetPropertyValue(L"SelectedIndex", value)
			|| !value.TryGetInt(candidate.Effective))
			return Fail(L"无法捕获 TabControl.SelectedIndex 有效值。", outError);
		candidate.HasLocal = tab->TryGetPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Local, value);
		if (candidate.HasLocal && !value.TryGetInt(candidate.Local))
			return Fail(L"TabControl.SelectedIndex Local 值无效。", outError);
		candidate.HasBinding = tab->TryGetPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Binding, value);
		if (candidate.HasBinding && !value.TryGetInt(candidate.Binding))
			return Fail(L"TabControl.SelectedIndex Binding 值无效。", outError);
		if (const auto* binding = FindSelectedIndexBinding(target))
		{
			candidate.HasConfiguredBinding = true;
			candidate.ConfiguredBinding = *binding;
		}
		const auto tracked = target.MetadataProperties.find(L"SelectedIndex");
		if (tracked != target.MetadataProperties.end())
		{
			candidate.HasTracked = true;
			candidate.Tracked = tracked->second;
		}
		output = std::move(candidate);
		return true;
	}

	bool RestoreSelectedIndex(
		DesignerControl& target,
		const SelectedIndexState& desired,
		std::wstring* outError) const
	{
		auto* tab = dynamic_cast<TabControl*>(target.ControlInstance);
		if (!tab) return Fail(L"TabControl 目标已经失效。", outError);
		const auto* configured = FindSelectedIndexBinding(target);
		if ((configured != nullptr) != desired.HasConfiguredBinding
			|| (configured && *configured != desired.ConfiguredBinding))
			return Fail(L"TabControl.SelectedIndex Binding 配置已经变化。", outError);

		if (desired.HasBinding)
		{
			BindingValue current;
			if (!tab->TryGetPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Binding, current))
				return Fail(L"TabControl.SelectedIndex Binding 值来源已经消失。", outError);
			if (tab->HasPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Local)
				&& !tab->ClearPropertyValue(
					L"SelectedIndex", ControlPropertyValueSource::Local))
				return Fail(L"无法暂时移除 TabControl.SelectedIndex Local 值。", outError);
			if (!tab->TrySetCurrentPropertyValue(
				L"SelectedIndex", desired.Binding))
				return Fail(L"无法恢复 TabControl.SelectedIndex Binding 值。", outError);
		}
		else if (tab->HasPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Binding))
		{
			return Fail(L"TabControl.SelectedIndex 出现了意外的 Binding 值来源。", outError);
		}

		if (desired.HasLocal)
		{
			if (!tab->TrySetPropertyValue(
				L"SelectedIndex", desired.Local,
				ControlPropertyValueSource::Local))
				return Fail(L"无法恢复 TabControl.SelectedIndex Local 值。", outError);
		}
		else if (tab->HasPropertyValue(
			L"SelectedIndex", ControlPropertyValueSource::Local)
			&& !tab->ClearPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Local))
		{
			return Fail(L"无法清除 TabControl.SelectedIndex Local 值。", outError);
		}

		if (desired.HasTracked)
			target.MetadataProperties[L"SelectedIndex"] = desired.Tracked;
		else
			target.MetadataProperties.erase(L"SelectedIndex");

		SelectedIndexState actual;
		if (!CaptureSelectedIndex(target, actual, outError)) return false;
		if (actual != desired)
			return Fail(L"TabControl.SelectedIndex 未恢复到精确状态。", outError);
		return true;
	}

	bool Capture(State& output, std::wstring* outError) const
	{
		auto target = ResolveTarget();
		if (!target) return Fail(L"集合编辑目标控件不存在。", outError);
		State candidate;
		candidate.WrapperFlatIndices.assign(Wrappers.size(), MissingIndex);
		std::unordered_map<Control*, size_t> recordByControl;
		recordByControl.reserve(Records.size());
		for (size_t index = 0; index < Records.size(); ++index)
			recordByControl.emplace(Records[index].Raw, index);

		std::unordered_set<size_t> attached;
		attached.reserve(Records.size());
		for (int index = 0; index < TargetInstance->Count; ++index)
		{
			auto* child = TargetInstance->operator[](index);
			const auto found = recordByControl.find(child);
			if (found == recordByControl.end())
				return Fail(L"集合目标包含命令范围外的直接子控件。", outError);
			if (!attached.insert(found->second).second)
				return Fail(L"集合目标包含重复子控件。", outError);
			const auto& record = Records[found->second];
			if (record.Owner)
				return Fail(L"已挂载子控件同时被命令持有。", outError);
			Entry entry;
			entry.RecordIndex = found->second;
			entry.Text = child->Text;
			entry.DesignId = child->DesignId;
			if (CollectionKind == Kind::ToolBarButtons)
			{
				auto* toolBar = static_cast<ToolBar*>(TargetInstance);
				entry.HasControlSize = true;
				entry.ControlSize = child->Size;
				entry.HasToolBarSizeOverride =
					toolBar->TryGetToolItemSizeOverride(
						child, entry.ToolBarSizeOverride);
			}
			candidate.Entries.push_back(std::move(entry));
		}
		for (size_t index = 0; index < Records.size(); ++index)
		{
			if (attached.contains(index)) continue;
			const auto& record = Records[index];
			if (!record.Owner || record.Owner.get() != record.Raw
				|| record.Raw->Parent)
				return Fail(L"未挂载子控件没有唯一的命令所有者。", outError);
		}

		for (size_t flat = 0; flat < Canvas->_designerControls.size(); ++flat)
		{
			const auto& current = Canvas->_designerControls[flat];
			for (size_t wrapper = 0; wrapper < Wrappers.size(); ++wrapper)
			{
				if (Wrappers[wrapper].Value != current) continue;
				if (candidate.WrapperFlatIndices[wrapper] != MissingIndex)
					return Fail(L"设计器包装在集合中重复出现。", outError);
				candidate.WrapperFlatIndices[wrapper] = flat;
			}
		}
		if (CollectionKind == Kind::TabPages
			&& !CaptureSelectedIndex(*target, candidate.SelectedIndex, outError))
			return false;
		output = std::move(candidate);
		return true;
	}

	bool RemoveWrappers(std::wstring* outError)
	{
		for (const auto& wrapper : Wrappers)
		{
			const auto found = std::find(
				Canvas->_designerControls.begin(),
				Canvas->_designerControls.end(), wrapper.Value);
			if (found == Canvas->_designerControls.end()) continue;
			if (wrapper.Value)
				Canvas->DetachDesignBindingPreview(*wrapper.Value);
			Canvas->_designerControls.erase(found);
		}
		if (outError) outError->clear();
		return true;
	}

	bool Materialize(const State& desired, std::wstring* outError)
	{
		auto target = ResolveTarget();
		if (!target) return Fail(L"集合编辑目标控件不存在。", outError);
		Canvas->ClearSelection();

		while (TargetInstance->Count > 0)
		{
			auto* child = TargetInstance->operator[](TargetInstance->Count - 1);
			auto found = std::find_if(
				Records.begin(), Records.end(),
				[child](const Record& record) { return record.Raw == child; });
			if (found == Records.end())
				return Fail(L"无法分离命令范围外的集合子控件。", outError);
			if (CollectionKind == Kind::ToolBarButtons)
				static_cast<ToolBar*>(TargetInstance)
					->ClearToolItemSizeOverride(child);
			auto owner = TargetInstance->DetachControl(child);
			if (!owner)
				return Fail(L"无法取得集合子控件所有权。", outError);
			found->Owner = std::move(owner);
		}
		if (!RemoveWrappers(outError)) return false;

		for (size_t index = 0; index < desired.Entries.size(); ++index)
		{
			const auto& entry = desired.Entries[index];
			if (entry.RecordIndex >= Records.size())
				return Fail(L"集合状态引用了无效子控件。", outError);
			auto& record = Records[entry.RecordIndex];
			if (!record.Owner || record.Owner.get() != record.Raw)
				return Fail(L"待挂载子控件没有唯一所有者。", outError);
			record.Raw->Text = entry.Text;
			record.Raw->DesignId = entry.DesignId;
			if (entry.HasControlSize)
				record.Raw->Size = entry.ControlSize;
			Control* raw = record.Owner.get();
			try
			{
				TargetInstance->InsertControl(
					static_cast<int>(index), raw);
			}
			catch (...)
			{
				if (raw->Parent == TargetInstance) record.Owner.release();
				throw;
			}
			record.Owner.release();
			if (CollectionKind == Kind::ToolBarButtons)
			{
				auto* toolBar = static_cast<ToolBar*>(TargetInstance);
				if (entry.HasToolBarSizeOverride)
					toolBar->SetToolItemSizeOverride(
						record.Raw, entry.ToolBarSizeOverride);
				else
					toolBar->ClearToolItemSizeOverride(record.Raw);
			}
		}

		std::vector<std::pair<size_t, size_t>> insertions;
		for (size_t wrapper = 0;
			wrapper < desired.WrapperFlatIndices.size(); ++wrapper)
		{
			if (desired.WrapperFlatIndices[wrapper] != MissingIndex)
				insertions.emplace_back(
					desired.WrapperFlatIndices[wrapper], wrapper);
		}
		std::sort(insertions.begin(), insertions.end());
		for (const auto& [flatIndex, wrapperIndex] : insertions)
		{
			if (flatIndex > Canvas->_designerControls.size())
				return Fail(L"设计器包装顺序超出有效范围。", outError);
			const auto& wrapper = Wrappers[wrapperIndex].Value;
			Canvas->_designerControls.insert(
				Canvas->_designerControls.begin() + flatIndex, wrapper);
			std::wstring bindingError;
			if (wrapper && !Canvas->RefreshDesignBindings(
				*wrapper, &bindingError))
				return Fail(L"无法恢复集合子树绑定：" + bindingError, outError);
		}

		if (CollectionKind == Kind::TabPages
			&& !RestoreSelectedIndex(*target, desired.SelectedIndex, outError))
			return false;
		if (CollectionKind == Kind::ToolBarButtons)
			static_cast<ToolBar*>(TargetInstance)->LayoutItems();
		TargetInstance->InvalidateVisual();
		return true;
	}

	void NotifyAndRestoreSelection(
		const std::vector<std::wstring>& names,
		const std::wstring& primary)
	{
		PropertyGridBinder binder;
		binder.SetCanvas(Canvas);
		binder.NotifyControlChanged(TargetInstance);
		Canvas->RestoreSelectionByNames(names, primary, true);
	}
};

namespace
{
	using Impl = ControlOwnedCollectionCommand::Impl;

	bool Initialize(
		std::unique_ptr<Impl>& impl,
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		Impl::Kind kind,
		std::wstring label,
		std::wstring* outError)
	{
		if (!canvas || !target || !target->ControlInstance)
			return Fail(L"集合编辑目标无效。", outError);
		if ((kind == Impl::Kind::TabPages
				&& (!dynamic_cast<TabControl*>(target->ControlInstance)
					|| target->Type != UIClass::UI_TabControl))
			|| (kind == Impl::Kind::ToolBarButtons
				&& (!dynamic_cast<ToolBar*>(target->ControlInstance)
					|| target->Type != UIClass::UI_ToolBar)))
			return Fail(L"集合编辑目标类型不匹配。", outError);

		auto candidate = std::make_unique<Impl>();
		candidate->Canvas = canvas;
		candidate->Target = target;
		candidate->TargetInstance = target->ControlInstance;
		candidate->TargetStableId = target->StableId;
		candidate->TargetName = target->Name;
		candidate->TargetType = target->Type;
		candidate->CollectionKind = kind;
		candidate->Label = std::move(label);
		for (const auto& selected : canvas->GetSelectedControls())
			if (selected && !selected->Name.empty())
				candidate->BeforeSelectionNames.push_back(selected->Name);
		candidate->BeforePrimarySelectionName = PrimarySelectionName(canvas);

		for (int index = 0; index < target->ControlInstance->Count; ++index)
		{
			Impl::Record record;
			record.Raw = target->ControlInstance->operator[](index);
			candidate->Records.push_back(std::move(record));
		}
		std::unordered_map<Control*, size_t> recordByRoot;
		for (size_t index = 0; index < candidate->Records.size(); ++index)
			recordByRoot.emplace(candidate->Records[index].Raw, index);

		for (const auto& wrapper : canvas->GetAllControls())
		{
			if (!wrapper || !wrapper->ControlInstance) continue;
			Control* root = wrapper->ControlInstance;
			while (root && root->Parent != target->ControlInstance)
				root = root->Parent;
			if (!root) continue;
			const auto found = recordByRoot.find(root);
			if (found == recordByRoot.end()) continue;
			const size_t wrapperIndex = candidate->Wrappers.size();
			candidate->Wrappers.push_back({ wrapper, found->second });
			candidate->Records[found->second].Wrappers.push_back(wrapperIndex);
		}
		if (!candidate->Capture(candidate->Before, outError)) return false;
		impl = std::move(candidate);
		return true;
	}

	void BuildAfterWrapperOrder(Impl& impl)
	{
		size_t first = MissingIndex;
		for (const auto index : impl.Before.WrapperFlatIndices)
			if (index != MissingIndex) first = (std::min)(first, index);
		if (first == MissingIndex) first = impl.Canvas->GetAllControls().size();

		size_t insertionIndex = 0;
		for (size_t index = 0;
			index < (std::min)(first, impl.Canvas->GetAllControls().size()); ++index)
		{
			const auto& current = impl.Canvas->GetAllControls()[index];
			const bool owned = std::any_of(
				impl.Wrappers.begin(), impl.Wrappers.end(),
				[&](const Impl::Wrapper& wrapper)
				{
					return wrapper.Value == current;
				});
			if (!owned) ++insertionIndex;
		}

		impl.After.WrapperFlatIndices.assign(
			impl.Wrappers.size(), MissingIndex);
		size_t next = insertionIndex;
		for (const auto& entry : impl.After.Entries)
		{
			auto wrappers = impl.Records[entry.RecordIndex].Wrappers;
			std::sort(wrappers.begin(), wrappers.end(),
				[&](size_t left, size_t right)
				{
					const size_t leftIndex = left < impl.Before.WrapperFlatIndices.size()
						? impl.Before.WrapperFlatIndices[left] : MissingIndex;
					const size_t rightIndex = right < impl.Before.WrapperFlatIndices.size()
						? impl.Before.WrapperFlatIndices[right] : MissingIndex;
					return leftIndex < rightIndex;
				});
			for (const auto wrapper : wrappers)
				impl.After.WrapperFlatIndices[wrapper] = next++;
		}
	}

	void BuildAfterSelection(Impl& impl)
	{
		std::unordered_set<std::wstring> removedNames;
		for (size_t index = 0; index < impl.Wrappers.size(); ++index)
		{
			if (impl.After.WrapperFlatIndices[index] == MissingIndex
				&& impl.Wrappers[index].Value)
				removedNames.insert(impl.Wrappers[index].Value->Name);
		}
		for (const auto& name : impl.BeforeSelectionNames)
			if (!removedNames.contains(name))
				impl.AfterSelectionNames.push_back(name);
		impl.AfterPrimarySelectionName = impl.BeforePrimarySelectionName;
		if (removedNames.contains(impl.AfterPrimarySelectionName))
		{
			impl.AfterPrimarySelectionName = impl.TargetName;
			if (std::find(impl.AfterSelectionNames.begin(),
				impl.AfterSelectionNames.end(), impl.TargetName)
				== impl.AfterSelectionNames.end())
				impl.AfterSelectionNames.push_back(impl.TargetName);
		}
	}

	void FinalizeEstimate(Impl& impl)
	{
		impl.EstimatedMemoryUsage = sizeof(ControlOwnedCollectionCommand)
			+ sizeof(Impl)
			+ impl.Records.capacity() * sizeof(Impl::Record)
			+ impl.Wrappers.capacity() * sizeof(Impl::Wrapper)
			+ (impl.Before.Entries.capacity() + impl.After.Entries.capacity())
				* sizeof(Impl::Entry)
			+ (impl.Before.WrapperFlatIndices.capacity()
				+ impl.After.WrapperFlatIndices.capacity()) * sizeof(size_t)
			+ StringMemory(impl.TargetName) + StringMemory(impl.Label)
			+ StringMemory(impl.BeforePrimarySelectionName)
			+ StringMemory(impl.AfterPrimarySelectionName);
		for (const auto& entry : impl.Before.Entries)
			impl.EstimatedMemoryUsage += StringMemory(entry.Text);
		for (const auto& entry : impl.After.Entries)
			impl.EstimatedMemoryUsage += StringMemory(entry.Text);
		for (const auto& name : impl.BeforeSelectionNames)
			impl.EstimatedMemoryUsage += StringMemory(name);
		for (const auto& name : impl.AfterSelectionNames)
			impl.EstimatedMemoryUsage += StringMemory(name);
		for (const auto& record : impl.Records)
			impl.EstimatedMemoryUsage +=
				record.Wrappers.capacity() * sizeof(size_t);
		// Detached runtime subtrees contain derived collections not visible in the
		// command's vectors. A per-wrapper allowance keeps history budgeting
		// proportional without retaining a full DesignDocument snapshot.
		impl.EstimatedMemoryUsage += impl.Wrappers.size() * 1024;
	}
}

ControlOwnedCollectionCommand::ControlOwnedCollectionCommand(
	std::unique_ptr<Impl> impl)
	: _impl(std::move(impl))
{
}

ControlOwnedCollectionCommand::~ControlOwnedCollectionCommand() = default;

std::unique_ptr<ControlOwnedCollectionCommand>
ControlOwnedCollectionCommand::CreateTabPages(
	DesignerCanvas* canvas,
	const std::shared_ptr<DesignerControl>& target,
	const std::vector<DesignerTabPageCollectionEdit>& pages,
	std::wstring label,
	std::wstring* outError)
{
	std::unique_ptr<Impl> impl;
	if (!Initialize(impl, canvas, target, Impl::Kind::TabPages,
		std::move(label), outError)) return {};
	auto* tab = static_cast<TabControl*>(impl->TargetInstance);
	std::unordered_set<TabPage*> used;
	for (const auto& edit : pages)
	{
		if (edit.Title.empty())
			return Fail(L"TabPage 标题不能为空。", outError), nullptr;
		size_t recordIndex = MissingIndex;
		if (edit.ExistingPage)
		{
			if (edit.ExistingPage->Parent != tab
				|| !used.insert(edit.ExistingPage).second)
				return Fail(L"TabPage 编辑结果包含无效或重复页面。", outError), nullptr;
			for (size_t index = 0; index < impl->Records.size(); ++index)
				if (impl->Records[index].Raw == edit.ExistingPage)
					recordIndex = index;
		}
		else
		{
			auto owner = std::make_unique<TabPage>(edit.Title);
			recordIndex = impl->Records.size();
			impl->Records.push_back({ owner.get(), std::move(owner), {} });
		}
		if (recordIndex == MissingIndex)
			return Fail(L"找不到 TabPage 编辑目标。", outError), nullptr;
		Impl::Entry entry;
		entry.RecordIndex = recordIndex;
		entry.Text = edit.Title;
		entry.DesignId = impl->Records[recordIndex].Raw->DesignId;
		impl->After.Entries.push_back(std::move(entry));
	}
	if (impl->After.Entries.empty())
		return Fail(L"TabControl 至少需要一个页面。", outError), nullptr;

	impl->After.SelectedIndex = impl->Before.SelectedIndex;
	Control* selectedPage = nullptr;
	if (impl->Before.SelectedIndex.Effective >= 0
		&& static_cast<size_t>(impl->Before.SelectedIndex.Effective)
			< impl->Before.Entries.size())
		selectedPage = impl->Records[
			impl->Before.Entries[impl->Before.SelectedIndex.Effective].RecordIndex].Raw;
	int selectedIndex = -1;
	for (size_t index = 0; index < impl->After.Entries.size(); ++index)
		if (impl->Records[impl->After.Entries[index].RecordIndex].Raw == selectedPage)
			selectedIndex = static_cast<int>(index);
	if (selectedIndex < 0)
		selectedIndex = (std::clamp)(
			impl->Before.SelectedIndex.Effective, 0,
			static_cast<int>(impl->After.Entries.size()) - 1);
	if (selectedIndex != impl->After.SelectedIndex.Effective)
	{
		if (impl->After.SelectedIndex.HasLocal)
			impl->After.SelectedIndex.Local = selectedIndex;
		else if (impl->After.SelectedIndex.HasBinding)
			impl->After.SelectedIndex.Binding = selectedIndex;
		else
		{
			impl->After.SelectedIndex.HasLocal = true;
			impl->After.SelectedIndex.Local = selectedIndex;
		}
		impl->After.SelectedIndex.Effective = selectedIndex;
		if (impl->After.SelectedIndex.HasTracked
			|| impl->After.SelectedIndex.HasLocal)
		{
			impl->After.SelectedIndex.HasTracked = true;
			impl->After.SelectedIndex.Tracked = {
				DesignerStyleValueKind::Int, std::to_wstring(selectedIndex) };
		}
	}
	BuildAfterWrapperOrder(*impl);
	BuildAfterSelection(*impl);
	FinalizeEstimate(*impl);
	if (outError) outError->clear();
	return std::unique_ptr<ControlOwnedCollectionCommand>(
		new ControlOwnedCollectionCommand(std::move(impl)));
}

std::unique_ptr<ControlOwnedCollectionCommand>
ControlOwnedCollectionCommand::CreateToolBarButtons(
	DesignerCanvas* canvas,
	const std::shared_ptr<DesignerControl>& target,
	const std::vector<DesignerToolBarButtonCollectionEdit>& buttons,
	std::wstring label,
	std::wstring* outError)
{
	std::unique_ptr<Impl> impl;
	if (!Initialize(impl, canvas, target, Impl::Kind::ToolBarButtons,
		std::move(label), outError)) return {};
	auto* toolBar = static_cast<ToolBar*>(impl->TargetInstance);
	std::unordered_set<Button*> used;
	for (const auto& edit : buttons)
	{
		if (edit.Text.empty() || edit.Width < 1)
			return Fail(L"ToolBar 按钮文本或宽度无效。", outError), nullptr;
		size_t recordIndex = MissingIndex;
		if (edit.ExistingButton)
		{
			if (edit.ExistingButton->Parent != toolBar
				|| !used.insert(edit.ExistingButton).second)
				return Fail(L"ToolBar 编辑结果包含无效或重复按钮。", outError), nullptr;
			for (size_t index = 0; index < impl->Records.size(); ++index)
				if (impl->Records[index].Raw == edit.ExistingButton)
					recordIndex = index;
		}
		else
		{
			auto owner = toolBar->CreateToolButton(edit.Text, edit.Width);
			recordIndex = impl->Records.size();
			impl->Records.push_back({ owner.get(), std::move(owner), {} });
		}
		if (recordIndex == MissingIndex)
			return Fail(L"找不到 ToolBar 按钮编辑目标。", outError), nullptr;

		bool hasRootWrapper = false;
		for (const auto wrapperIndex : impl->Records[recordIndex].Wrappers)
			if (impl->Wrappers[wrapperIndex].Value
				&& impl->Wrappers[wrapperIndex].Value->ControlInstance
					== impl->Records[recordIndex].Raw)
				hasRootWrapper = true;
		if (!hasRootWrapper)
		{
			const int stableId = canvas->AllocateStableControlId();
			const auto name = canvas->GenerateDefaultControlName(
				UIClass::UI_Button, L"Button");
			auto wrapper = std::make_shared<DesignerControl>(
				impl->Records[recordIndex].Raw, name,
				UIClass::UI_Button, toolBar, stableId);
			const size_t wrapperIndex = impl->Wrappers.size();
			impl->Wrappers.push_back({ std::move(wrapper), recordIndex });
			impl->Records[recordIndex].Wrappers.push_back(wrapperIndex);
			impl->Before.WrapperFlatIndices.push_back(MissingIndex);
		}

		Impl::Entry entry;
		entry.RecordIndex = recordIndex;
		entry.Text = edit.Text;
		for (const auto wrapperIndex : impl->Records[recordIndex].Wrappers)
		{
			const auto& wrapper = impl->Wrappers[wrapperIndex].Value;
			if (wrapper && wrapper->ControlInstance == impl->Records[recordIndex].Raw)
			{
				entry.DesignId = wrapper->StableId;
				break;
			}
		}
		entry.HasToolBarSizeOverride = true;
		entry.HasControlSize = true;
		entry.ControlSize = { edit.Width, toolBar->ItemHeight };
		entry.ToolBarSizeOverride = {
			edit.Width, ToolBar::AutoItemHeightOverride };
		impl->After.Entries.push_back(std::move(entry));
	}
	BuildAfterWrapperOrder(*impl);
	BuildAfterSelection(*impl);
	FinalizeEstimate(*impl);
	if (outError) outError->clear();
	return std::unique_ptr<ControlOwnedCollectionCommand>(
		new ControlOwnedCollectionCommand(std::move(impl)));
}

DesignerDocumentTransactionResult ControlOwnedCollectionCommand::Execute()
{
	return Apply(true);
}

DesignerDocumentTransactionResult ControlOwnedCollectionCommand::Undo()
{
	return Apply(false);
}

DesignerDocumentTransactionResult ControlOwnedCollectionCommand::Apply(
	bool useAfterState)
{
	if (!_impl)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"集合编辑命令无效。", false);
	const auto& expected = useAfterState ? _impl->Before : _impl->After;
	const auto& desired = useAfterState ? _impl->After : _impl->Before;
	if (expected == desired)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);

	Impl::State current;
	std::wstring error;
	if (!_impl->Capture(current, &error))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法验证集合编辑起点：" + error, false);
	if (current != expected)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"集合编辑起点与当前控件状态不一致。", false);

	try
	{
		if (!_impl->Materialize(desired, &error))
			throw std::runtime_error("materialize failed");
		Impl::State actual;
		if (!_impl->Capture(actual, &error) || actual != desired)
		{
			if (error.empty()) error = L"集合编辑结果未达到请求的精确状态。";
			throw std::runtime_error("verification failed");
		}
	}
	catch (...)
	{
		std::wstring restoreError;
		bool restored = false;
		try
		{
			restored = _impl->Materialize(expected, &restoreError);
			Impl::State restoredState;
			restored = restored
				&& _impl->Capture(restoredState, &restoreError)
				&& restoredState == expected;
		}
		catch (...) { restored = false; }
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法应用集合编辑：" + error
				+ (restoreError.empty() ? std::wstring()
					: L" 恢复失败：" + restoreError),
			restored);
	}

	_impl->NotifyAndRestoreSelection(
		useAfterState ? _impl->AfterSelectionNames
			: _impl->BeforeSelectionNames,
		useAfterState ? _impl->AfterPrimarySelectionName
			: _impl->BeforePrimarySelectionName);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

std::wstring ControlOwnedCollectionCommand::GetLabel() const
{
	return _impl ? _impl->Label : std::wstring{};
}

size_t ControlOwnedCollectionCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _impl ? _impl->EstimatedMemoryUsage : sizeof(*this);
}

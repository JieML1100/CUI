#include "ControlSubtreeCommand.h"
#include "../../DesignerCanvas.h"
#include "../../../CUI/include/SplitContainer.h"
#include "../../../CUI/include/TabControl.h"
#include "../../../CUI/include/ToolBar.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace
{
	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t StringMemory(const std::string& value) noexcept
	{
		return sizeof(std::string)
			+ value.capacity() * sizeof(std::string::value_type);
	}

	size_t DesignValueMemory(const DesignerModel::DesignValue& value) noexcept
	{
		size_t result = sizeof(value);
		if (value.is_string())
		{
			try
			{
				result += value.get<std::string>().capacity();
			}
			catch (...) {}
		}
		else if (value.is_array())
		{
			result += value.ArrayItems().capacity()
				* sizeof(DesignerModel::DesignValue);
			for (const auto& item : value.ArrayItems())
				result += DesignValueMemory(item);
		}
		else if (value.is_object())
		{
			result += value.ObjectItems().size()
				* sizeof(DesignerModel::DesignValue::object_t::value_type);
			for (const auto& [key, item] : value.ObjectItems())
				result += StringMemory(key) + DesignValueMemory(item);
		}
		return result;
	}

	DesignerModel::DesignNode NormalizeNode(
		DesignerModel::DesignNode node)
	{
		node.Id = 0;
		node.ParentId = 0;
		return node;
	}

	void RefreshParent(Control* parent)
	{
		if (!parent) return;
		if (auto* split = dynamic_cast<SplitContainer*>(parent))
		{
			split->RefreshSplitterLayout();
			return;
		}
		if (auto* panel = dynamic_cast<Panel*>(parent))
		{
			panel->InvalidateLayout();
			panel->PerformLayout();
		}
	}

	size_t SelectionMemory(
		const std::vector<std::wstring>& names) noexcept
	{
		size_t result = names.capacity() * sizeof(std::wstring);
		for (const auto& name : names) result += StringMemory(name);
		return result;
	}
}

struct ControlSubtreeCommand::DetachedPayload
{
	struct Root
	{
		std::wstring Name;
		std::unique_ptr<Control> Owner;
		bool HasToolBarSizeOverride = false;
		SIZE ToolBarSizeOverride{ -1, -1 };
	};
	struct Wrapper
	{
		std::shared_ptr<DesignerControl> Value;
		size_t FlatIndex = 0;
	};

	std::vector<Root> Roots;
	std::vector<Wrapper> Wrappers;
};

size_t DesignerSubtreeIdentity::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this) + StringMemory(Name);
}

bool DesignerControlSubtreeSnapshot::EquivalentTo(
	const DesignerControlSubtreeSnapshot& other) const noexcept
{
	return Identities == other.Identities
		&& RootPlacements.EquivalentTo(other.RootPlacements)
		&& RootAttachments == other.RootAttachments
		&& Nodes == other.Nodes;
}

size_t DesignerControlSubtreeSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this)
		+ Identities.capacity() * sizeof(DesignerSubtreeIdentity)
		+ RootPlacements.GetEstimatedMemoryUsage()
		+ RootAttachments.capacity()
			* sizeof(DesignerSubtreeRootAttachmentState)
		+ Nodes.capacity() * sizeof(DesignerModel::DesignNode);
	for (const auto& identity : Identities)
		result += identity.GetEstimatedMemoryUsage();
	for (const auto& node : Nodes)
	{
		result += StringMemory(node.ParentRef)
			+ StringMemory(node.Name)
			+ DesignValueMemory(node.Props)
			+ DesignValueMemory(node.Extra)
			+ DesignValueMemory(node.Events)
			+ DesignValueMemory(node.Bindings);
	}
	return result;
}

ControlSubtreeCommand::ControlSubtreeCommand(
	DesignerCanvas* canvas,
	DesignerControlSubtreeSnapshot snapshot,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforePrimarySelectionName,
	std::wstring afterPrimarySelectionName,
	bool presentAfter,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _snapshot(std::move(snapshot)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforePrimarySelectionName(std::move(beforePrimarySelectionName)),
	  _afterPrimarySelectionName(std::move(afterPrimarySelectionName)),
	  _presentAfter(presentAfter),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute)
{
	_estimatedMemoryUsage = sizeof(*this)
		// The command must budget for both its normalized persisted snapshot and
		// the live runtime/wrapper subtree it owns while the subtree is absent.
		// The serialized footprint is a conservative proportional proxy for the
		// derived controls' dynamic collections.
		+ _snapshot.GetEstimatedMemoryUsage() * 2
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ StringMemory(_beforePrimarySelectionName)
		+ StringMemory(_afterPrimarySelectionName)
		+ StringMemory(_label)
		+ _snapshot.Identities.size()
			* sizeof(DetachedPayload::Wrapper)
		+ _snapshot.RootPlacements.Targets.size()
			* sizeof(DetachedPayload::Root);
}

ControlSubtreeCommand::~ControlSubtreeCommand() = default;

std::shared_ptr<DesignerControl> ControlSubtreeCommand::FindControl(
	DesignerCanvas* canvas,
	const std::wstring& name,
	UIClass type,
	bool requireType)
{
	if (!canvas) return {};
	std::shared_ptr<DesignerControl> result;
	for (const auto& candidate : canvas->_designerControls)
	{
		if (!candidate || !candidate->ControlInstance
			|| candidate->Name != name
			|| (requireType && candidate->Type != type)) continue;
		if (result) return {};
		result = candidate;
	}
	return result;
}

bool ControlSubtreeCommand::ResolveParent(
	const DesignerControlPlacementState& state,
	Control*& runtimeParent,
	Control*& designerParent,
	std::wstring* outError) const
{
	runtimeParent = nullptr;
	designerParent = nullptr;
	if (!_canvas)
	{
		if (outError) *outError = L"设计画布不可用。";
		return false;
	}
	if (state.ParentKind == DesignerPlacementParentKind::Root)
	{
		runtimeParent = _canvas->_clientSurface
			? static_cast<Control*>(_canvas->_clientSurface)
			: static_cast<Control*>(_canvas->_designSurface);
		if (!runtimeParent && outError)
			*outError = L"设计器根容器不可用。";
		return runtimeParent != nullptr;
	}

	auto parent = FindControl(
		_canvas, state.ParentName, state.ParentType);
	if (!parent)
	{
		if (outError) *outError = L"找不到子树父级：" + state.ParentName;
		return false;
	}
	auto* parentInstance = parent->ControlInstance;
	switch (state.ParentKind)
	{
	case DesignerPlacementParentKind::Control:
		runtimeParent = parentInstance;
		designerParent = parentInstance;
		break;
	case DesignerPlacementParentKind::TabPage:
	{
		auto* tab = dynamic_cast<TabControl*>(parentInstance);
		if (!tab || state.ParentPageIndex < 0
			|| state.ParentPageIndex >= tab->Count)
		{
			if (outError) *outError = L"子树 TabPage 父级已经变化。";
			return false;
		}
		runtimeParent = tab->operator[](state.ParentPageIndex);
		designerParent = runtimeParent;
		break;
	}
	case DesignerPlacementParentKind::SplitFirst:
	case DesignerPlacementParentKind::SplitSecond:
	{
		auto* split = dynamic_cast<SplitContainer*>(parentInstance);
		if (!split)
		{
			if (outError) *outError = L"子树 Split 父级类型已经变化。";
			return false;
		}
		runtimeParent = state.ParentKind
			== DesignerPlacementParentKind::SplitFirst
			? static_cast<Control*>(split->FirstPanel())
			: static_cast<Control*>(split->SecondPanel());
		designerParent = split;
		break;
	}
	default:
		if (outError) *outError = L"子树父级定位器无效。";
		return false;
	}
	if (!runtimeParent)
	{
		if (outError) *outError = L"子树运行时父级不可用。";
		return false;
	}
	return true;
}

bool ControlSubtreeCommand::Capture(
	DesignerCanvas* canvas,
	const std::vector<std::shared_ptr<DesignerControl>>& roots,
	DesignerControlSubtreeSnapshot& out,
	std::wstring* outError)
{
	out = DesignerControlSubtreeSnapshot{};
	if (!canvas || roots.empty())
	{
		if (outError) *outError = L"没有可捕获的子树根。";
		return false;
	}
	try
	{
		std::vector<std::shared_ptr<DesignerControl>> filtered;
		for (const auto& candidate : roots)
		{
			if (!candidate || !candidate->ControlInstance
				|| candidate->Type == UIClass::UI_TabPage) continue;
			bool nested = false;
			for (const auto& other : roots)
			{
				if (!other || other == candidate || !other->ControlInstance)
					continue;
				if (canvas->IsDescendantOf(
					other->ControlInstance, candidate->ControlInstance))
				{
					nested = true;
					break;
				}
			}
			if (!nested
				&& std::find(filtered.begin(), filtered.end(), candidate)
					== filtered.end())
				filtered.push_back(candidate);
		}
		if (filtered.empty())
		{
			if (outError) *outError = L"选中项中没有可捕获的子树根。";
			return false;
		}
		std::stable_sort(filtered.begin(), filtered.end(),
			[canvas](const auto& left, const auto& right)
			{
				auto leftIt = std::find(
					canvas->_designerControls.begin(),
					canvas->_designerControls.end(), left);
				auto rightIt = std::find(
					canvas->_designerControls.begin(),
					canvas->_designerControls.end(), right);
				return leftIt < rightIt;
			});

		if (!ControlPlacementCommand::Capture(
			canvas, filtered, out.RootPlacements, outError)) return false;
		out.RootAttachments.reserve(filtered.size());
		for (const auto& root : filtered)
		{
			DesignerSubtreeRootAttachmentState attachment;
			if (root && root->ControlInstance)
			{
				if (auto* toolBar = dynamic_cast<ToolBar*>(
					root->ControlInstance->Parent))
					attachment.HasToolBarSizeOverride =
						toolBar->TryGetToolItemSizeOverride(
							root->ControlInstance,
							attachment.ToolBarSizeOverride);
			}
			out.RootAttachments.push_back(attachment);
		}

		std::unordered_set<Control*> rootInstances;
		for (const auto& root : filtered)
			rootInstances.insert(root->ControlInstance);
		std::unordered_set<std::wstring> names;
		for (const auto& candidate : canvas->_designerControls)
		{
			if (!candidate || !candidate->ControlInstance) continue;
			bool inSubtree = rootInstances.contains(candidate->ControlInstance);
			if (!inSubtree)
			{
				for (auto* root : rootInstances)
				{
					if (canvas->IsDescendantOf(
						root, candidate->ControlInstance))
					{
						inSubtree = true;
						break;
					}
				}
			}
			if (!inSubtree) continue;
			out.Identities.push_back({ candidate->Name, candidate->Type });
			names.insert(candidate->Name);
		}

		DesignerModel::DesignDocument document;
		if (!canvas->BuildDesignDocument(document, outError))
		{
			out = DesignerControlSubtreeSnapshot{};
			return false;
		}
		for (const auto& node : document.Nodes)
			if (names.contains(node.Name))
				out.Nodes.push_back(NormalizeNode(node));
		const size_t persistedIdentityCount = std::count_if(
			out.Identities.begin(), out.Identities.end(),
			[](const DesignerSubtreeIdentity& identity)
			{ return identity.Type != UIClass::UI_TabPage; });
		if (out.Nodes.size() != persistedIdentityCount)
		{
			out = DesignerControlSubtreeSnapshot{};
			if (outError) *outError = L"子树设计节点与运行时包装器不一致。";
			return false;
		}
		std::sort(out.Nodes.begin(), out.Nodes.end(),
			[](const auto& left, const auto& right)
			{ return left.Name < right.Name; });
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		out = DesignerControlSubtreeSnapshot{};
		if (outError) *outError = L"捕获控件子树时抛出异常。";
		return false;
	}
}

DesignerDocumentTransactionResult ControlSubtreeCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);
	}
	return Apply(
		!_presentAfter, _presentAfter,
		_afterSelectionNames, _afterPrimarySelectionName);
}

DesignerDocumentTransactionResult ControlSubtreeCommand::Undo()
{
	return Apply(
		_presentAfter, !_presentAfter,
		_beforeSelectionNames, _beforePrimarySelectionName);
}

std::wstring ControlSubtreeCommand::GetLabel() const
{
	return _label;
}

size_t ControlSubtreeCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

bool ControlSubtreeCommand::CaptureCurrent(
	DesignerControlSubtreeSnapshot& out,
	std::wstring* outError) const
{
	std::vector<std::shared_ptr<DesignerControl>> roots;
	roots.reserve(_snapshot.RootPlacements.Targets.size());
	for (size_t rootIndex = 0;
		rootIndex < _snapshot.RootPlacements.Targets.size(); ++rootIndex)
	{
		const auto& state = _snapshot.RootPlacements.Targets[rootIndex];
		auto root = FindControl(_canvas, state.TargetName, state.TargetType);
		if (!root)
		{
			if (outError) *outError = L"找不到子树根：" + state.TargetName;
			return false;
		}
		roots.push_back(root);
	}
	return Capture(_canvas, roots, out, outError);
}

bool ControlSubtreeCommand::ValidatePresent(std::wstring* outError) const
{
	if (_detached)
	{
		if (outError) *outError = L"子树同时被命令和画布拥有。";
		return false;
	}
	DesignerControlSubtreeSnapshot current;
	if (!CaptureCurrent(current, outError)) return false;
	if (!current.EquivalentTo(_snapshot))
	{
		if (outError) *outError = L"子树起点与当前控件状态不一致。";
		return false;
	}
	return true;
}

bool ControlSubtreeCommand::ValidateAbsent(std::wstring* outError) const
{
	if (!_detached
		|| _detached->Roots.size() != _snapshot.RootPlacements.Targets.size()
		|| _detached->Wrappers.size() != _snapshot.Identities.size())
	{
		if (outError) *outError = L"命令没有完整拥有缺席的控件子树。";
		return false;
	}
	for (const auto& identity : _snapshot.Identities)
	{
		for (const auto& candidate : _canvas->_designerControls)
		{
			if (candidate && candidate->Name == identity.Name)
			{
				if (outError) *outError = L"缺席子树名称已被占用：" + identity.Name;
				return false;
			}
		}
	}
	return true;
}

bool ControlSubtreeCommand::DetachToStorage(std::wstring* outError)
{
	if (!ValidatePresent(outError)) return false;
	auto payload = std::make_unique<DetachedPayload>();
	payload->Roots.reserve(_snapshot.RootPlacements.Targets.size());
	payload->Wrappers.reserve(_snapshot.Identities.size());

	for (const auto& identity : _snapshot.Identities)
	{
		auto wrapper = FindControl(_canvas, identity.Name, identity.Type);
		if (!wrapper)
		{
			if (outError) *outError = L"无法收集子树包装器：" + identity.Name;
			return false;
		}
		auto position = std::find(
			_canvas->_designerControls.begin(),
			_canvas->_designerControls.end(), wrapper);
		if (position == _canvas->_designerControls.end()) return false;
		payload->Wrappers.push_back({
			wrapper,
			static_cast<size_t>(position - _canvas->_designerControls.begin())
		});
	}

	struct Work
	{
		std::wstring Name;
		Control* Parent = nullptr;
		int Index = -1;
		std::unique_ptr<Control> Owner;
		bool HasToolBarSizeOverride = false;
		SIZE ToolBarSizeOverride{ -1, -1 };
	};
	std::vector<Work> work;
	work.reserve(_snapshot.RootPlacements.Targets.size());
	for (size_t rootIndex = 0;
		rootIndex < _snapshot.RootPlacements.Targets.size(); ++rootIndex)
	{
		const auto& state = _snapshot.RootPlacements.Targets[rootIndex];
		auto root = FindControl(_canvas, state.TargetName, state.TargetType);
		if (!root || !root->ControlInstance || !root->ControlInstance->Parent)
		{
			if (outError) *outError = L"子树根没有可分离的父级：" + state.TargetName;
			return false;
		}
		Work item{
			state.TargetName,
			root->ControlInstance->Parent,
			root->ControlInstance->Parent->IndexOfControl(root->ControlInstance),
			{}
		};
		if (rootIndex < _snapshot.RootAttachments.size())
		{
			item.HasToolBarSizeOverride = _snapshot.RootAttachments[rootIndex]
				.HasToolBarSizeOverride;
			item.ToolBarSizeOverride = _snapshot.RootAttachments[rootIndex]
				.ToolBarSizeOverride;
		}
		work.push_back(std::move(item));
	}

	auto rollback = [&]() noexcept
	{
		bool restored = true;
		std::vector<size_t> order(work.size());
		for (size_t index = 0; index < order.size(); ++index) order[index] = index;
		std::stable_sort(order.begin(), order.end(), [&work](size_t left, size_t right)
		{
			if (work[left].Parent != work[right].Parent)
				return std::less<Control*>{}(
					work[left].Parent, work[right].Parent);
			return work[left].Index < work[right].Index;
		});
		for (size_t index : order)
		{
			auto& item = work[index];
			if (!item.Owner) continue;
			try
			{
				item.Parent->InsertControl(
					(std::clamp)(item.Index, 0, item.Parent->Count),
					item.Owner.get());
				if (item.HasToolBarSizeOverride)
					static_cast<ToolBar*>(item.Parent)
						->SetToolItemSizeOverride(
							item.Owner.get(), item.ToolBarSizeOverride);
				item.Owner.release();
			}
			catch (...)
			{
				if (item.Owner && item.Owner->Parent == item.Parent)
					item.Owner.release();
				restored = false;
			}
		}
		for (const auto& item : work) RefreshParent(item.Parent);
		for (const auto& record : payload->Wrappers)
			if (record.Value)
				(void)_canvas->RefreshDesignBindings(*record.Value, nullptr);
		return restored;
	};

	try
	{
		for (size_t index = 0; index < work.size(); ++index)
		{
			auto root = FindControl(
				_canvas,
				_snapshot.RootPlacements.Targets[index].TargetName,
				_snapshot.RootPlacements.Targets[index].TargetType);
			work[index].Owner = work[index].Parent->DetachControl(
				root ? root->ControlInstance : nullptr);
			if (!work[index].Owner)
			{
				const bool restored = rollback();
				if (outError) *outError = restored
					? L"无法从父级分离子树根。"
					: L"无法从父级分离子树根，且回滚不完整。";
				return false;
			}
			if (auto* toolBar = dynamic_cast<ToolBar*>(work[index].Parent))
				toolBar->ClearToolItemSizeOverride(work[index].Owner.get());
		}
		for (const auto& record : payload->Wrappers)
			if (record.Value) _canvas->DetachDesignBindingPreview(*record.Value);
	}
	catch (...)
	{
		const bool restored = rollback();
		if (outError) *outError = restored
			? L"分离子树时抛出异常，已恢复。"
			: L"分离子树时抛出异常，且恢复不完整。";
		return false;
	}

	for (const auto& state : _snapshot.RootPlacements.Targets)
	{
		auto wrapper = FindControl(_canvas, state.TargetName, state.TargetType);
		if (wrapper) wrapper->DesignerParent = nullptr;
	}
	std::unordered_set<std::wstring> names;
	for (const auto& identity : _snapshot.Identities) names.insert(identity.Name);
	_canvas->_designerControls.erase(
		std::remove_if(
			_canvas->_designerControls.begin(),
			_canvas->_designerControls.end(),
			[&names](const auto& candidate)
			{ return candidate && names.contains(candidate->Name); }),
		_canvas->_designerControls.end());
	for (auto& item : work)
		payload->Roots.push_back({
			item.Name,
			std::move(item.Owner),
			item.HasToolBarSizeOverride,
			item.ToolBarSizeOverride
		});
	for (const auto& item : work) RefreshParent(item.Parent);
	_detached = std::move(payload);
	if (outError) outError->clear();
	return true;
}

bool ControlSubtreeCommand::AttachFromStorage(std::wstring* outError)
{
	if (!ValidateAbsent(outError)) return false;
	auto payload = std::move(_detached);

	struct Attachment
	{
		size_t RootIndex = 0;
		Control* RuntimeParent = nullptr;
		Control* DesignerParent = nullptr;
		int ChildIndex = -1;
		bool Attached = false;
	};
	std::vector<Attachment> attachments;
	attachments.reserve(_snapshot.RootPlacements.Targets.size());
	for (size_t index = 0;
		index < _snapshot.RootPlacements.Targets.size(); ++index)
	{
		const auto& state = _snapshot.RootPlacements.Targets[index];
		Control* runtimeParent = nullptr;
		Control* designerParent = nullptr;
		if (!ResolveParent(
			state, runtimeParent, designerParent, outError))
		{
			_detached = std::move(payload);
			return false;
		}
		auto root = std::find_if(
			payload->Roots.begin(), payload->Roots.end(),
			[&state](const auto& item) { return item.Name == state.TargetName; });
		if (root == payload->Roots.end() || !root->Owner)
		{
			if (outError) *outError = L"命令未拥有子树根：" + state.TargetName;
			_detached = std::move(payload);
			return false;
		}
		attachments.push_back({
			static_cast<size_t>(root - payload->Roots.begin()),
			runtimeParent,
			designerParent,
			state.ChildIndex,
			false
		});
	}

	std::vector<size_t> wrapperOrder(payload->Wrappers.size());
	for (size_t index = 0; index < wrapperOrder.size(); ++index)
		wrapperOrder[index] = index;
	std::stable_sort(wrapperOrder.begin(), wrapperOrder.end(),
		[&payload](size_t left, size_t right)
		{ return payload->Wrappers[left].FlatIndex
			< payload->Wrappers[right].FlatIndex; });
	for (size_t inserted = 0; inserted < wrapperOrder.size(); ++inserted)
	{
		if (payload->Wrappers[wrapperOrder[inserted]].FlatIndex
			> _canvas->_designerControls.size() + inserted)
		{
			if (outError) *outError = L"子树包装器顺序已经越界。";
			_detached = std::move(payload);
			return false;
		}
	}
	try
	{
		_canvas->_designerControls.reserve(
			_canvas->_designerControls.size() + payload->Wrappers.size());
	}
	catch (...)
	{
		if (outError) *outError = L"无法为子树包装器恢复分配空间。";
		_detached = std::move(payload);
		return false;
	}

	std::vector<size_t> attachOrder(attachments.size());
	for (size_t index = 0; index < attachOrder.size(); ++index)
		attachOrder[index] = index;
	std::stable_sort(attachOrder.begin(), attachOrder.end(),
		[&attachments](size_t left, size_t right)
		{
			if (attachments[left].RuntimeParent
				!= attachments[right].RuntimeParent)
				return std::less<Control*>{}(
					attachments[left].RuntimeParent,
					attachments[right].RuntimeParent);
			return attachments[left].ChildIndex
				< attachments[right].ChildIndex;
		});

	auto rollback = [&]() noexcept
	{
		bool restored = true;
		std::unordered_set<std::wstring> names;
		for (const auto& identity : _snapshot.Identities) names.insert(identity.Name);
		for (const auto& record : payload->Wrappers)
			if (record.Value) _canvas->DetachDesignBindingPreview(*record.Value);
		_canvas->_designerControls.erase(
			std::remove_if(
				_canvas->_designerControls.begin(),
				_canvas->_designerControls.end(),
				[&names](const auto& candidate)
				{ return candidate && names.contains(candidate->Name); }),
			_canvas->_designerControls.end());
		for (size_t orderIndex = attachOrder.size(); orderIndex > 0; --orderIndex)
		{
			auto& attachment = attachments[attachOrder[orderIndex - 1]];
			if (!attachment.Attached) continue;
			auto& root = payload->Roots[attachment.RootIndex];
			Control* raw = nullptr;
			for (const auto& state : _snapshot.RootPlacements.Targets)
			{
				if (state.TargetName != root.Name) continue;
				auto wrapper = std::find_if(
					payload->Wrappers.begin(), payload->Wrappers.end(),
					[&state](const auto& item)
					{ return item.Value && item.Value->Name == state.TargetName; });
				if (wrapper != payload->Wrappers.end())
					raw = wrapper->Value->ControlInstance;
				break;
			}
			try
			{
				root.Owner = attachment.RuntimeParent->DetachControl(raw);
				if (auto* toolBar =
					dynamic_cast<ToolBar*>(attachment.RuntimeParent))
					toolBar->ClearToolItemSizeOverride(raw);
				if (!root.Owner) restored = false;
			}
			catch (...)
			{
				restored = false;
			}
			attachment.Attached = false;
		}
		for (const auto& state : _snapshot.RootPlacements.Targets)
		{
			auto wrapper = std::find_if(
				payload->Wrappers.begin(), payload->Wrappers.end(),
				[&state](const auto& item)
				{ return item.Value && item.Value->Name == state.TargetName; });
			if (wrapper != payload->Wrappers.end())
				wrapper->Value->DesignerParent = nullptr;
		}
		for (const auto& attachment : attachments)
			RefreshParent(attachment.RuntimeParent);
		_detached = std::move(payload);
		return restored;
	};

	for (size_t orderIndex : attachOrder)
	{
		auto& attachment = attachments[orderIndex];
		auto& root = payload->Roots[attachment.RootIndex];
		if (attachment.ChildIndex < 0
			|| attachment.ChildIndex > attachment.RuntimeParent->Count)
		{
			const bool restored = rollback();
			if (outError) *outError = restored
				? L"子树同级顺序已经越界。"
				: L"子树同级顺序越界，且恢复不完整。";
			return false;
		}
		Control* raw = root.Owner.get();
		try
		{
			attachment.RuntimeParent->InsertControl(
				attachment.ChildIndex, raw);
			if (root.HasToolBarSizeOverride)
				static_cast<ToolBar*>(attachment.RuntimeParent)
					->SetToolItemSizeOverride(
						raw, root.ToolBarSizeOverride);
			root.Owner.release();
			attachment.Attached = true;
		}
		catch (...)
		{
			if (root.Owner && raw->Parent == attachment.RuntimeParent)
			{
				root.Owner.release();
				attachment.Attached = true;
			}
			const bool restored = rollback();
			if (outError) *outError = restored
				? L"挂载子树时抛出异常，已恢复。"
				: L"挂载子树时抛出异常，且恢复不完整。";
			return false;
		}
	}

	for (const auto& attachment : attachments)
	{
		const auto& root = payload->Roots[attachment.RootIndex];
		auto wrapper = std::find_if(
			payload->Wrappers.begin(), payload->Wrappers.end(),
			[&root](const auto& item)
			{ return item.Value && item.Value->Name == root.Name; });
		if (wrapper != payload->Wrappers.end())
			wrapper->Value->DesignerParent = attachment.DesignerParent;
	}
	for (size_t recordIndex : wrapperOrder)
	{
		auto& record = payload->Wrappers[recordIndex];
		_canvas->_designerControls.insert(
			_canvas->_designerControls.begin()
				+ static_cast<std::ptrdiff_t>(record.FlatIndex),
			record.Value);
	}

	std::wstring restoreError;
	if (!ControlPlacementCommand::Restore(
		_canvas, _snapshot.RootPlacements, &restoreError))
	{
		const bool restored = rollback();
		if (outError) *outError = L"无法恢复子树布局：" + restoreError
			+ (restored ? L"" : L"；回滚不完整。");
		return false;
	}
	for (const auto& record : payload->Wrappers)
		if (record.Value)
			(void)_canvas->RefreshDesignBindings(*record.Value, nullptr);

	DesignerControlSubtreeSnapshot current;
	std::wstring captureError;
	if (!CaptureCurrent(current, &captureError)
		|| !current.EquivalentTo(_snapshot))
	{
		const bool restored = rollback();
		if (outError) *outError = L"挂载后的子树状态不精确："
			+ captureError + (restored ? L"" : L"；回滚不完整。");
		return false;
	}
	for (const auto& attachment : attachments)
		RefreshParent(attachment.RuntimeParent);
	if (outError) outError->clear();
	return true;
}

DesignerDocumentTransactionResult ControlSubtreeCommand::Apply(
	bool expectedPresent,
	bool desiredPresent,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName)
{
	if (!_canvas || expectedPresent == desiredPresent)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"子树差量状态无效。", false);
	std::wstring error;
	if (!(expectedPresent
		? ValidatePresent(&error) : ValidateAbsent(&error)))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法验证子树差量起点：" + error, false);
	const auto previousSelection = _canvas->CaptureSelectionNames();
	const auto previousPrimary = _canvas->_selectedControl
		? _canvas->_selectedControl->Name : std::wstring{};
	const bool changed = desiredPresent
		? AttachFromStorage(&error) : DetachToStorage(&error);
	if (!changed)
	{
		std::wstring validationError;
		const bool restored = expectedPresent
			? ValidatePresent(&validationError)
			: ValidateAbsent(&validationError);
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法应用子树差量：" + error
				+ (restored || validationError.empty()
					? L"" : L" 原状态验证失败：" + validationError),
			restored);
	}
	try
	{
		_canvas->RestoreSelectionByNames(
			selectionNames, primarySelectionName, true);
	}
	catch (...)
	{
		std::wstring rollbackError;
		const bool restored = expectedPresent
			? AttachFromStorage(&rollbackError)
			: DetachToStorage(&rollbackError);
		try
		{
			_canvas->RestoreSelectionByNames(
				previousSelection, previousPrimary, true);
		}
		catch (...) {}
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"恢复子树选择时抛出异常。"
				+ (restored ? L"" : L" 回滚失败：" + rollbackError),
			restored);
	}
	_canvas->InvalidateVisual();
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

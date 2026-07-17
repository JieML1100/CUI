#include "ControlPlacementCommand.h"
#include "../../DesignerCanvas.h"
#include "../../../CUI/include/SplitContainer.h"
#include "../../../CUI/include/TabControl.h"
#include "../../../CUI/include/Panel.h"
#include <algorithm>

namespace
{
	constexpr auto MergeWindow = std::chrono::milliseconds(1000);

	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t SelectionMemory(
		const std::vector<std::wstring>& names) noexcept
	{
		size_t result = names.capacity() * sizeof(std::wstring);
		for (const auto& name : names) result += StringMemory(name);
		return result;
	}

	void RefreshLayout(DesignerCanvas* canvas, Control* control)
	{
		if (!control) return;
		if (auto* split = dynamic_cast<SplitContainer*>(control->Parent))
		{
			split->RefreshSplitterLayout();
		}
		else if (auto* parent = dynamic_cast<Panel*>(control->Parent))
		{
			parent->InvalidateLayout();
			parent->PerformLayout();
		}
		control->InvalidateVisual();
		if (canvas) canvas->InvalidateVisual();
	}

	std::wstring FirstPlacementDifference(
		const DesignerControlPlacementState& actual,
		const DesignerControlPlacementState& expected)
	{
		if (actual.TargetName != expected.TargetName) return L"TargetName";
		if (actual.TargetType != expected.TargetType) return L"TargetType";
		if (actual.ParentKind != expected.ParentKind) return L"ParentKind";
		if (actual.ParentName != expected.ParentName) return L"ParentName";
		if (actual.ParentType != expected.ParentType) return L"ParentType";
		if (actual.ParentPageIndex != expected.ParentPageIndex)
			return L"ParentPageIndex";
		if (actual.ChildIndex != expected.ChildIndex) return L"ChildIndex";
		if (actual.Location.x != expected.Location.x
			|| actual.Location.y != expected.Location.y) return L"Location";
		if (actual.Size.cx != expected.Size.cx
			|| actual.Size.cy != expected.Size.cy)
			return L"Size actual=" + std::to_wstring(actual.Size.cx)
				+ L"x" + std::to_wstring(actual.Size.cy)
				+ L", expected=" + std::to_wstring(expected.Size.cx)
				+ L"x" + std::to_wstring(expected.Size.cy);
		if (actual.Margin != expected.Margin) return L"Margin";
		if (actual.Width != expected.Width) return L"LayoutWidth";
		if (actual.Height != expected.Height) return L"LayoutHeight";
		if (actual.HAlign != expected.HAlign) return L"HAlign";
		if (actual.VAlign != expected.VAlign) return L"VAlign";
		if (actual.AnchorStyles != expected.AnchorStyles) return L"Anchor";
		if (actual.DockPosition != expected.DockPosition) return L"Dock";
		if (actual.GridRow != expected.GridRow) return L"GridRow";
		if (actual.GridColumn != expected.GridColumn) return L"GridColumn";
		if (actual.GridRowSpan != expected.GridRowSpan) return L"GridRowSpan";
		if (actual.GridColumnSpan != expected.GridColumnSpan)
			return L"GridColumnSpan";
		return L"unknown";
	}

}

std::shared_ptr<DesignerControl> ControlPlacementCommand::ResolveTarget(
	DesignerCanvas* canvas,
	const DesignerControlPlacementState& state)
{
	if (!canvas) return nullptr;
	const auto match = std::find_if(
		canvas->_designerControls.begin(),
		canvas->_designerControls.end(),
		[&state](const std::shared_ptr<DesignerControl>& candidate)
		{
			return candidate && candidate->ControlInstance
				&& candidate->Name == state.TargetName
				&& candidate->Type == state.TargetType;
		});
	return match == canvas->_designerControls.end() ? nullptr : *match;
}

bool ControlPlacementCommand::CaptureTarget(
	DesignerCanvas* canvas,
	const std::shared_ptr<DesignerControl>& target,
	DesignerControlPlacementState& out,
	std::wstring* outError)
{
	if (!canvas || !target || !target->ControlInstance
		|| target->Name.empty())
	{
		if (outError) *outError = L"布局差量目标或画布无效。";
		return false;
	}
	auto* control = target->ControlInstance;
	auto* runtimeParent = control->Parent;
	if (!runtimeParent)
	{
		if (outError) *outError = L"布局差量目标没有运行时父级。";
		return false;
	}

	DesignerControlPlacementState state;
	state.TargetName = target->Name;
	state.TargetType = target->Type;
	state.ChildIndex = runtimeParent->IndexOfControl(control);
	if (state.ChildIndex < 0)
	{
		if (outError) *outError = L"布局差量目标不在父级子集合中。";
		return false;
	}

	Control* root = canvas->_clientSurface
		? static_cast<Control*>(canvas->_clientSurface)
		: static_cast<Control*>(canvas->_designSurface);
	if (!target->DesignerParent)
	{
		if (runtimeParent != root)
		{
			if (outError) *outError = L"根级控件的运行时父级不一致。";
			return false;
		}
		state.ParentKind = DesignerPlacementParentKind::Root;
	}
	else
	{
		bool parentFound = false;
		for (const auto& candidate : canvas->_designerControls)
		{
			if (!candidate || !candidate->ControlInstance
				|| candidate->Type != UIClass::UI_TabControl)
				continue;
			auto* tabs = dynamic_cast<TabControl*>(candidate->ControlInstance);
			if (!tabs) continue;
			for (int pageIndex = 0; pageIndex < tabs->Count; ++pageIndex)
			{
				auto* page = tabs->operator[](pageIndex);
				if (page != target->DesignerParent) continue;
				if (runtimeParent != page)
				{
					if (outError) *outError =
						L"TabPage 的设计父级与运行时父级不一致。";
					return false;
				}
				state.ParentKind = DesignerPlacementParentKind::TabPage;
				state.ParentName = candidate->Name;
				state.ParentType = candidate->Type;
				state.ParentPageIndex = pageIndex;
				parentFound = true;
				break;
			}
			if (parentFound) break;
		}

		if (!parentFound)
		{
			const auto parent = std::find_if(
				canvas->_designerControls.begin(),
				canvas->_designerControls.end(),
				[target](const std::shared_ptr<DesignerControl>& candidate)
				{
					return candidate && candidate->ControlInstance
						&& candidate->ControlInstance
							== target->DesignerParent;
				});
			if (parent == canvas->_designerControls.end())
			{
				if (outError) *outError = L"无法标识布局差量的设计父级。";
				return false;
			}
			state.ParentName = (*parent)->Name;
			state.ParentType = (*parent)->Type;
			if (auto* split = dynamic_cast<SplitContainer*>(
				(*parent)->ControlInstance))
			{
				if (runtimeParent == split->FirstPanel())
					state.ParentKind =
						DesignerPlacementParentKind::SplitFirst;
				else if (runtimeParent == split->SecondPanel())
					state.ParentKind =
						DesignerPlacementParentKind::SplitSecond;
				else if (runtimeParent == split)
					state.ParentKind = DesignerPlacementParentKind::Control;
				else
				{
					if (outError) *outError =
						L"SplitContainer 的运行时区域无法标识。";
					return false;
				}
			}
			else
			{
				if (runtimeParent != (*parent)->ControlInstance)
				{
					if (outError) *outError =
						L"设计父级与运行时父级不一致。";
					return false;
				}
				state.ParentKind = DesignerPlacementParentKind::Control;
			}
		}
	}

	state.Location = control->Location;
	state.Size = control->Size;
	state.Margin = control->Margin;
	state.Width = control->LayoutWidth;
	state.Height = control->LayoutHeight;
	state.HAlign = control->HAlign;
	state.VAlign = control->VAlign;
	state.AnchorStyles = control->AnchorStyles;
	state.DockPosition = control->DockPosition;
	state.GridRow = control->GridRow;
	state.GridColumn = control->GridColumn;
	state.GridRowSpan = control->GridRowSpan;
	state.GridColumnSpan = control->GridColumnSpan;
	out = std::move(state);
	if (outError) outError->clear();
	return true;
}

bool ControlPlacementCommand::ResolveParent(
	DesignerCanvas* canvas,
	const DesignerControlPlacementState& state,
	Control*& runtimeParent,
	Control*& designerParent,
	std::wstring* outError)
{
	runtimeParent = nullptr;
	designerParent = nullptr;
	if (!canvas)
	{
		if (outError) *outError = L"布局差量画布无效。";
		return false;
	}
	if (state.ParentKind == DesignerPlacementParentKind::Root)
	{
		runtimeParent = canvas->_clientSurface
			? static_cast<Control*>(canvas->_clientSurface)
			: static_cast<Control*>(canvas->_designSurface);
		if (!runtimeParent)
		{
			if (outError) *outError = L"布局差量根容器不可用。";
			return false;
		}
		if (outError) outError->clear();
		return true;
	}

	const auto parent = std::find_if(
		canvas->_designerControls.begin(),
		canvas->_designerControls.end(),
		[&state](const std::shared_ptr<DesignerControl>& candidate)
		{
			return candidate && candidate->ControlInstance
				&& candidate->Name == state.ParentName
				&& candidate->Type == state.ParentType;
		});
	if (parent == canvas->_designerControls.end())
	{
		if (outError) *outError = L"无法解析布局差量父级 "
			+ state.ParentName + L"。";
		return false;
	}
	auto* parentControl = (*parent)->ControlInstance;
	switch (state.ParentKind)
	{
	case DesignerPlacementParentKind::Control:
		runtimeParent = parentControl;
		designerParent = parentControl;
		break;
	case DesignerPlacementParentKind::TabPage:
	{
		auto* tabs = dynamic_cast<TabControl*>(parentControl);
		if (!tabs || state.ParentPageIndex < 0
			|| state.ParentPageIndex >= tabs->Count)
		{
			if (outError) *outError = L"布局差量 TabPage 已不存在。";
			return false;
		}
		runtimeParent = tabs->operator[](state.ParentPageIndex);
		designerParent = runtimeParent;
		break;
	}
	case DesignerPlacementParentKind::SplitFirst:
	case DesignerPlacementParentKind::SplitSecond:
	{
		auto* split = dynamic_cast<SplitContainer*>(parentControl);
		if (!split)
		{
			if (outError) *outError = L"布局差量 SplitContainer 已不存在。";
			return false;
		}
		runtimeParent = state.ParentKind
			== DesignerPlacementParentKind::SplitFirst
			? split->FirstPanel() : split->SecondPanel();
		designerParent = split;
		break;
	}
	default:
		if (outError) *outError = L"布局差量父级类型无效。";
		return false;
	}
	if (!runtimeParent)
	{
		if (outError) *outError = L"布局差量运行时父级不可用。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

bool ControlPlacementCommand::ApplyStateUnchecked(
	DesignerCanvas* canvas,
	const std::shared_ptr<DesignerControl>& target,
	const DesignerControlPlacementState& state,
	std::wstring* outError)
{
	if (!target || !target->ControlInstance)
	{
		if (outError) *outError = L"布局差量目标已经失效。";
		return false;
	}
	Control* desiredRuntimeParent = nullptr;
	Control* desiredDesignerParent = nullptr;
	if (!ResolveParent(canvas, state, desiredRuntimeParent,
		desiredDesignerParent, outError))
		return false;
	auto* control = target->ControlInstance;
	auto* previousParent = control->Parent;
	if (!previousParent || state.ChildIndex < 0
		|| desiredRuntimeParent == control)
	{
		if (outError) *outError = L"布局差量父子关系无效。";
		return false;
	}
	for (Control* ancestor = desiredRuntimeParent; ancestor;
		ancestor = ancestor->Parent)
	{
		if (ancestor == control)
		{
			if (outError) *outError = L"布局差量会形成父子循环。";
			return false;
		}
	}

	try
	{
		const int previousIndex = previousParent->IndexOfControl(control);
		if (previousIndex < 0)
		{
			if (outError) *outError = L"布局差量目标不在当前父级中。";
			return false;
		}
		if (previousParent == desiredRuntimeParent)
		{
			if (state.ChildIndex >= desiredRuntimeParent->Count)
			{
				if (outError) *outError = L"布局差量同级顺序越界。";
				return false;
			}
			if (previousIndex != state.ChildIndex)
				desiredRuntimeParent->Children.Move(
					static_cast<size_t>(previousIndex),
					static_cast<size_t>(state.ChildIndex));
		}
		else
		{
			if (state.ChildIndex > desiredRuntimeParent->Count)
			{
				if (outError) *outError = L"布局差量目标父级顺序越界。";
				return false;
			}
			auto owner = previousParent->DetachControl(control);
			if (!owner)
			{
				if (outError) *outError = L"无法从原父级分离布局目标。";
				return false;
			}
			Control* raw = owner.get();
			try
			{
				desiredRuntimeParent->InsertControl(state.ChildIndex, raw);
				owner.release();
			}
			catch (...)
			{
				if (raw->Parent == desiredRuntimeParent) owner.release();
				std::unique_ptr<Control> rollbackOwner;
				if (raw->Parent)
					rollbackOwner = raw->Parent->DetachControl(raw);
				else if (owner.get() == raw)
					rollbackOwner = std::move(owner);
				if (rollbackOwner)
				{
					const int rollbackIndex = (std::clamp)(
						previousIndex, 0, previousParent->Count);
					try
					{
						previousParent->InsertControl(
							rollbackIndex, rollbackOwner.get());
						rollbackOwner.release();
					}
					catch (...)
					{
						// Preserve the live Designer instance even if a public
						// collection observer prevents reattachment.
						rollbackOwner.release();
					}
				}
				if (outError) *outError = raw->Parent == previousParent
					? L"挂载布局目标失败，已恢复原父级。"
					: L"挂载布局目标失败，且原父级恢复失败。";
				return false;
			}
		}

		target->DesignerParent = desiredDesignerParent;
		control->DockPosition = state.DockPosition;
		control->GridRow = state.GridRow;
		control->GridColumn = state.GridColumn;
		control->GridRowSpan = state.GridRowSpan;
		control->GridColumnSpan = state.GridColumnSpan;
		control->HAlign = state.HAlign;
		control->VAlign = state.VAlign;
		control->AnchorStyles = state.AnchorStyles;
		// Compatibility Size and public Length declarations are distinct: an
		// arranged/clipped control can retain a configured Size that differs
		// from the current fixed/Auto declaration. Restore both layers.
		control->Size = state.Size;
		control->LayoutWidth = state.Width;
		control->LayoutHeight = state.Height;
		control->Margin = state.Margin;
		control->Location = state.Location;
		RefreshLayout(canvas, previousParent);
		if (desiredRuntimeParent != previousParent)
			RefreshLayout(canvas, desiredRuntimeParent);
		RefreshLayout(canvas, control);
	}
	catch (...)
	{
		if (outError) *outError = L"布局树或属性 setter 抛出异常。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

bool ControlPlacementCommand::ApplyState(
	DesignerCanvas* canvas,
	const std::shared_ptr<DesignerControl>& target,
	const DesignerControlPlacementState& state,
	std::wstring* outError)
{
	DesignerControlPlacementState original;
	std::wstring captureError;
	if (!CaptureTarget(canvas, target, original, &captureError))
	{
		if (outError) *outError = L"无法建立布局目标回滚点："
			+ captureError;
		return false;
	}
	std::wstring applyError;
	bool applied = ApplyStateUnchecked(canvas, target, state, &applyError);
	DesignerControlPlacementState actual;
	if (applied)
	{
		applied = CaptureTarget(canvas, target, actual, &applyError);
		if (applied && !actual.EquivalentTo(state))
		{
			applyError = L"布局树或属性未恢复到请求的精确状态（"
				+ FirstPlacementDifference(actual, state);
			if (target && target->ControlInstance
				&& target->ControlInstance->Parent)
			{
				const auto parentSize =
					target->ControlInstance->Parent->ActualSize();
				applyError += L", parent="
					+ std::to_wstring(parentSize.cx) + L"x"
					+ std::to_wstring(parentSize.cy);
			}
			applyError += L"）。";
			applied = false;
		}
	}
	if (applied)
	{
		if (outError) outError->clear();
		return true;
	}

	std::wstring rollbackError;
	bool restored = ApplyStateUnchecked(
		canvas, target, original, &rollbackError);
	DesignerControlPlacementState restoredState;
	if (restored)
	{
		restored = CaptureTarget(
			canvas, target, restoredState, &rollbackError)
			&& restoredState.EquivalentTo(original);
		if (!restored && rollbackError.empty())
			rollbackError = L"原状态恢复后未通过精确校验。";
	}
	if (outError)
	{
		*outError = applyError.empty() ? L"无法应用布局差量。" : applyError;
		if (!restored)
			*outError += L" 原状态恢复失败：" + rollbackError;
	}
	return false;
}

bool DesignerControlPlacementState::EquivalentTo(
	const DesignerControlPlacementState& other) const noexcept
{
	return TargetName == other.TargetName
		&& TargetType == other.TargetType
		&& ParentKind == other.ParentKind
		&& ParentName == other.ParentName
		&& ParentType == other.ParentType
		&& ParentPageIndex == other.ParentPageIndex
		&& ChildIndex == other.ChildIndex
		&& Location.x == other.Location.x
		&& Location.y == other.Location.y
		&& Size.cx == other.Size.cx
		&& Size.cy == other.Size.cy
		&& Margin == other.Margin
		&& Width == other.Width
		&& Height == other.Height
		&& HAlign == other.HAlign
		&& VAlign == other.VAlign
		&& AnchorStyles == other.AnchorStyles
		&& DockPosition == other.DockPosition
		&& GridRow == other.GridRow
		&& GridColumn == other.GridColumn
		&& GridRowSpan == other.GridRowSpan
		&& GridColumnSpan == other.GridColumnSpan;
}

size_t DesignerControlPlacementState::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ TargetName.capacity() * sizeof(wchar_t)
		+ ParentName.capacity() * sizeof(wchar_t);
}

bool DesignerControlPlacementSnapshot::EquivalentTo(
	const DesignerControlPlacementSnapshot& other) const noexcept
{
	if (Targets.size() != other.Targets.size()) return false;
	for (size_t index = 0; index < Targets.size(); ++index)
		if (!Targets[index].EquivalentTo(other.Targets[index])) return false;
	return true;
}

size_t DesignerControlPlacementSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this)
		+ Targets.capacity() * sizeof(DesignerControlPlacementState);
	for (const auto& target : Targets)
		result += target.GetEstimatedMemoryUsage();
	return result;
}

ControlPlacementCommand::ControlPlacementCommand(
	DesignerCanvas* canvas,
	DesignerControlPlacementSnapshot before,
	DesignerControlPlacementSnapshot after,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforePrimarySelectionName,
	std::wstring afterPrimarySelectionName,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _before(std::move(before)),
	  _after(std::move(after)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforePrimarySelectionName(std::move(beforePrimarySelectionName)),
	  _afterPrimarySelectionName(std::move(afterPrimarySelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute),
	  _committedAt(std::chrono::steady_clock::now())
{
	RefreshEstimatedMemoryUsage();
}

bool ControlPlacementCommand::Capture(
	DesignerCanvas* canvas,
	const std::vector<std::shared_ptr<DesignerControl>>& controls,
	DesignerControlPlacementSnapshot& out,
	std::wstring* outError)
{
	out = DesignerControlPlacementSnapshot{};
	out.Targets.reserve(controls.size());
	for (const auto& target : controls)
	{
		DesignerControlPlacementState state;
		if (!CaptureTarget(canvas, target, state, outError))
		{
			out = DesignerControlPlacementSnapshot{};
			return false;
		}
		out.Targets.push_back(std::move(state));
	}
	if (out.Targets.empty())
	{
		if (outError) *outError = L"没有可捕获的布局目标。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

bool ControlPlacementCommand::Restore(
	DesignerCanvas* canvas,
	const DesignerControlPlacementSnapshot& snapshot,
	std::wstring* outError,
	bool* outOriginalRestored)
{
	if (outOriginalRestored) *outOriginalRestored = true;
	if (!canvas || snapshot.Targets.empty())
	{
		if (outError) *outError = L"布局快照或画布无效。";
		return false;
	}
	std::vector<std::shared_ptr<DesignerControl>> controls;
	std::vector<DesignerControlPlacementState> rollback;
	controls.reserve(snapshot.Targets.size());
	rollback.reserve(snapshot.Targets.size());
	for (const auto& state : snapshot.Targets)
	{
		auto control = ResolveTarget(canvas, state);
		if (!control)
		{
			if (outError) *outError = L"无法解析布局快照目标 "
				+ state.TargetName + L"。";
			return false;
		}
		DesignerControlPlacementSnapshot captured;
		if (!Capture(canvas, { control }, captured, outError)) return false;
		controls.push_back(std::move(control));
		rollback.push_back(std::move(captured.Targets.front()));
	}
	for (size_t index = 0; index < controls.size(); ++index)
	{
		std::wstring error;
		if (ApplyState(canvas, controls[index], snapshot.Targets[index], &error))
			continue;
		bool restored = true;
		for (size_t rollbackIndex = index + 1; rollbackIndex > 0; --rollbackIndex)
			restored = ApplyState(
				canvas,
				controls[rollbackIndex - 1],
				rollback[rollbackIndex - 1],
				nullptr) && restored;
		if (outError)
			*outError = error + (restored
				? L"" : L" 布局快照回滚未能完整恢复所有目标。");
		if (outOriginalRestored) *outOriginalRestored = restored;
		return false;
	}
	if (outError) outError->clear();
	return true;
}

DesignerDocumentTransactionResult ControlPlacementCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);
	}
	return Apply(
		_before, _after,
		_afterSelectionNames, _afterPrimarySelectionName);
}

DesignerDocumentTransactionResult ControlPlacementCommand::Undo()
{
	return Apply(
		_after, _before,
		_beforeSelectionNames, _beforePrimarySelectionName);
}

std::wstring ControlPlacementCommand::GetLabel() const
{
	return _label;
}

DesignerDocumentTransactionResult ControlPlacementCommand::Apply(
	const DesignerControlPlacementSnapshot& expected,
	const DesignerControlPlacementSnapshot& desired,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName) const
{
	if (!_canvas || expected.Targets.size() != desired.Targets.size()
		|| expected.Targets.empty())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"布局差量的前后状态不兼容。", false);
	for (size_t index = 0; index < expected.Targets.size(); ++index)
	{
		if (expected.Targets[index].TargetName
				!= desired.Targets[index].TargetName
			|| expected.Targets[index].TargetType
				!= desired.Targets[index].TargetType)
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"布局差量的前后目标不一致。", false);
	}
	std::vector<std::shared_ptr<DesignerControl>> controls;
	controls.reserve(expected.Targets.size());
	for (const auto& state : expected.Targets)
	{
		auto control = ResolveTarget(_canvas, state);
		if (!control
			|| std::find(controls.begin(), controls.end(), control) != controls.end())
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"布局差量的目标控件不存在或不唯一。", false);
		controls.push_back(std::move(control));
	}
	DesignerControlPlacementSnapshot actual;
	std::wstring error;
	if (!Capture(_canvas, controls, actual, &error))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法验证布局差量起点：" + error, false);
	if (!actual.EquivalentTo(expected))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"布局差量起点与当前控件状态不一致。", false);
	bool originalRestored = true;
	if (!Restore(_canvas, desired, &error, &originalRestored))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法应用布局差量：" + error, originalRestored);
	_canvas->RestoreSelectionByNames(
		selectionNames, primarySelectionName, true);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

bool ControlPlacementCommand::TryMergeWith(
	IDesignerCommand& newerCommand) noexcept
{
	auto* newer = dynamic_cast<ControlPlacementCommand*>(&newerCommand);
	if (!newer || newer == this || _canvas != newer->_canvas
		|| _label != newer->_label || _label != L"NudgeSelection"
		|| _skipInitialExecute || newer->_skipInitialExecute
		|| !_after.EquivalentTo(newer->_before)
		|| _afterSelectionNames != newer->_beforeSelectionNames
		|| _afterPrimarySelectionName
			!= newer->_beforePrimarySelectionName)
		return false;
	const auto elapsed = newer->_committedAt - _committedAt;
	if (elapsed < std::chrono::steady_clock::duration::zero()
		|| elapsed > MergeWindow)
		return false;
	_after = std::move(newer->_after);
	_afterSelectionNames = std::move(newer->_afterSelectionNames);
	_afterPrimarySelectionName =
		std::move(newer->_afterPrimarySelectionName);
	_committedAt = newer->_committedAt;
	RefreshEstimatedMemoryUsage();
	return true;
}

size_t ControlPlacementCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

void ControlPlacementCommand::RefreshEstimatedMemoryUsage() noexcept
{
	_estimatedMemoryUsage = sizeof(*this)
		+ _before.GetEstimatedMemoryUsage()
		+ _after.GetEstimatedMemoryUsage()
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ StringMemory(_beforePrimarySelectionName)
		+ StringMemory(_afterPrimarySelectionName)
		+ StringMemory(_label);
}

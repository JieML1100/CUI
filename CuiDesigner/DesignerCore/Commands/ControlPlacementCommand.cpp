#include "ControlPlacementCommand.h"
#include "../../DesignerCanvas.h"
#include "../../../CUI/include/Canvas.h"
#include "../../../CUI/include/Layout/DockPanel.h"
#include "../../../CUI/include/Layout/Grid.h"
#include "../../../CUI/include/TabControl.h"
#include "../../../CUI/include/ItemsControl.h"
#include "../../../CUI/include/TemplateInfrastructure.h"
#include "../../../CUI/include/Panel.h"
#include <algorithm>
#include <stdexcept>

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

	bool CanvasOffsetEqual(float left, float right) noexcept
	{
		return (!cui::layout::IsCanvasOffsetSet(left)
			&& !cui::layout::IsCanvasOffsetSet(right)) || left == right;
	}

	void RefreshLayout(DesignerCanvas* canvas, Control* control)
	{
		if (!control) return;
		if (auto* parent = dynamic_cast<Panel*>(control->GetVisualParent()))
		{
			parent->InvalidateLayout();
			parent->UpdateLayout();
		}
		control->InvalidateVisual();
		if (canvas) canvas->InvalidateVisual();
	}

	ItemsControl* FindAuthoredItemsOwner(
		Control* control, size_t* outIndex = nullptr) noexcept
	{
		if (!control) return nullptr;
		auto* owner = dynamic_cast<ItemsControl*>(control->GetLogicalParent());
		if (!owner || control->GetVisualParent()
			!= cui::framework::TemplateAccess::GetItemsHost(*owner))
			return nullptr;
		for (size_t index = 0; index < owner->AuthoredItemCount(); ++index)
		{
			if (owner->GetAuthoredItem(index) != control) continue;
			if (outIndex) *outIndex = index;
			return owner;
		}
		return nullptr;
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
		if (actual.ComponentContentProperty
			!= expected.ComponentContentProperty)
			return L"ComponentContentProperty";
		if (actual.ChildIndex != expected.ChildIndex) return L"ChildIndex";
		if (actual.LocalValueMask != expected.LocalValueMask)
			return L"dependency-property Local source";
		auto authored = [&](DesignerPlacementLocalValue value)
		{
			return expected.HasLocalValue(value);
		};
		if (authored(DesignerPlacementLocalValue::Margin)
			&& actual.Margin != expected.Margin) return L"Margin";
		if (authored(DesignerPlacementLocalValue::Width)
			&& actual.Width != expected.Width) return L"Width";
		if (authored(DesignerPlacementLocalValue::Height)
			&& actual.Height != expected.Height) return L"Height";
		if (authored(DesignerPlacementLocalValue::CanvasLeft)
			&& !CanvasOffsetEqual(actual.CanvasLeft, expected.CanvasLeft))
			return L"Canvas.Left";
		if (authored(DesignerPlacementLocalValue::CanvasTop)
			&& !CanvasOffsetEqual(actual.CanvasTop, expected.CanvasTop))
			return L"Canvas.Top";
		if (authored(DesignerPlacementLocalValue::CanvasRight)
			&& !CanvasOffsetEqual(actual.CanvasRight, expected.CanvasRight))
			return L"Canvas.Right";
		if (authored(DesignerPlacementLocalValue::CanvasBottom)
			&& !CanvasOffsetEqual(actual.CanvasBottom, expected.CanvasBottom))
			return L"Canvas.Bottom";
		if (authored(DesignerPlacementLocalValue::HorizontalAlignment)
			&& actual.Horizontal != expected.Horizontal)
			return L"HorizontalAlignment";
		if (authored(DesignerPlacementLocalValue::VerticalAlignment)
			&& actual.Vertical != expected.Vertical)
			return L"VerticalAlignment";
		if (authored(DesignerPlacementLocalValue::Dock)
			&& actual.DockPosition != expected.DockPosition) return L"Dock";
		if (authored(DesignerPlacementLocalValue::GridRow)
			&& actual.GridRow != expected.GridRow) return L"Grid.Row";
		if (authored(DesignerPlacementLocalValue::GridColumn)
			&& actual.GridColumn != expected.GridColumn) return L"Grid.Column";
		if (authored(DesignerPlacementLocalValue::GridRowSpan)
			&& actual.GridRowSpan != expected.GridRowSpan) return L"Grid.RowSpan";
		if (authored(DesignerPlacementLocalValue::GridColumnSpan)
			&& actual.GridColumnSpan != expected.GridColumnSpan)
			return L"Grid.ColumnSpan";
		if (authored(DesignerPlacementLocalValue::ZIndex)
			&& actual.ZIndex != expected.ZIndex) return L"ZIndex";
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
	auto* runtimeParent = control->GetVisualParent();
	if (!runtimeParent)
	{
		if (outError) *outError = L"布局差量目标没有运行时父级。";
		return false;
	}

	DesignerControlPlacementState state;
	state.TargetName = target->Name;
	state.TargetType = target->Type;
	state.ComponentContentProperty = target->ComponentContentProperty;
	state.ChildIndex = runtimeParent->IndexOfVisualChild(control);
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
			if (auto* items = dynamic_cast<ItemsControl*>(
				(*parent)->ControlInstance))
			{
				size_t itemIndex = 0;
				if (items->GetItemsSource()
					|| FindAuthoredItemsOwner(control, &itemIndex) != items)
				{
					if (outError) *outError =
						L"ItemsControl 设计父级与 authored Items 所有权不一致。";
					return false;
				}
				state.ChildIndex = static_cast<int>(itemIndex);
				state.ParentKind = DesignerPlacementParentKind::ItemsControl;
			}
			else
			{
				Control* expectedRuntimeParent = (*parent)->ControlInstance;
				if (!target->ComponentContentProperty.empty()
					&& !(*parent)->ComponentType.Empty())
				{
					const auto presenter = (*parent)->ComponentContentPresenters.find(
						target->ComponentContentProperty);
					if (presenter == (*parent)->ComponentContentPresenters.end())
					{
						if (outError) *outError =
							L"组件内容槽的运行时 Presenter 已不存在。";
						return false;
					}
					expectedRuntimeParent = presenter->second;
				}
				if (runtimeParent != expectedRuntimeParent)
				{
					if (outError) *outError =
						L"设计父级与运行时父级不一致。";
					return false;
				}
				state.ParentKind = DesignerPlacementParentKind::Control;
			}
	}

	state.Margin = control->Margin;
	state.Width = control->Width;
	state.Height = control->Height;
	state.CanvasLeft = Canvas::GetLeft(*(control));
	state.CanvasTop = Canvas::GetTop(*(control));
	state.CanvasRight = Canvas::GetRight(*(control));
	state.CanvasBottom = Canvas::GetBottom(*(control));
	state.Horizontal = control->HorizontalAlignment;
	state.Vertical = control->VerticalAlignment;
	state.DockPosition = DockPanel::GetDock(*(control));
	state.GridRow = Grid::GetRow(*(control));
	state.GridColumn = Grid::GetColumn(*(control));
	state.GridRowSpan = Grid::GetRowSpan(*(control));
	state.GridColumnSpan = Grid::GetColumnSpan(*(control));
	state.ZIndex = control->ZIndex;
	auto captureLocal = [&](DesignerPlacementLocalValue value,
		const wchar_t* propertyName)
	{
		state.SetLocalValue(value, control->HasPropertyValue(
			propertyName, DependencyPropertyValueSource::Local));
	};
	captureLocal(DesignerPlacementLocalValue::Margin, L"Margin");
	captureLocal(DesignerPlacementLocalValue::Width, L"Width");
	captureLocal(DesignerPlacementLocalValue::Height, L"Height");
	captureLocal(DesignerPlacementLocalValue::CanvasLeft, L"Canvas.Left");
	captureLocal(DesignerPlacementLocalValue::CanvasTop, L"Canvas.Top");
	captureLocal(DesignerPlacementLocalValue::CanvasRight, L"Canvas.Right");
	captureLocal(DesignerPlacementLocalValue::CanvasBottom, L"Canvas.Bottom");
	captureLocal(DesignerPlacementLocalValue::HorizontalAlignment,
		L"HorizontalAlignment");
	captureLocal(DesignerPlacementLocalValue::VerticalAlignment,
		L"VerticalAlignment");
	captureLocal(DesignerPlacementLocalValue::Dock, L"DockPanel.Dock");
	captureLocal(DesignerPlacementLocalValue::GridRow, L"Grid.Row");
	captureLocal(DesignerPlacementLocalValue::GridColumn, L"Grid.Column");
	captureLocal(DesignerPlacementLocalValue::GridRowSpan, L"Grid.RowSpan");
	captureLocal(DesignerPlacementLocalValue::GridColumnSpan,
		L"Grid.ColumnSpan");
	captureLocal(DesignerPlacementLocalValue::ZIndex, L"ZIndex");
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
		if (!(*parent)->ComponentType.Empty())
		{
			if (state.ComponentContentProperty.empty())
			{
				if (outError) *outError = L"组件子控件缺少视觉内容属性。";
				return false;
			}
			const auto presenter = (*parent)->ComponentContentPresenters.find(
				state.ComponentContentProperty);
			if (presenter == (*parent)->ComponentContentPresenters.end())
			{
				if (outError) *outError = L"组件视觉内容 Presenter 已不存在。";
				return false;
			}
			runtimeParent = presenter->second;
		}
		else
		{
			if (!state.ComponentContentProperty.empty())
			{
				if (outError) *outError = L"普通容器不能承载组件内容槽状态。";
				return false;
			}
			runtimeParent = parentControl;
		}
		designerParent = parentControl;
		break;
	case DesignerPlacementParentKind::ItemsControl:
	{
		auto* items = dynamic_cast<ItemsControl*>(parentControl);
		if (!items || items->GetItemsSource()
			|| !cui::framework::TemplateAccess::GetItemsHost(*items))
		{
			if (outError) *outError = L"布局差量 ItemsControl 已不可编辑。";
			return false;
		}
		runtimeParent = cui::framework::TemplateAccess::GetItemsHost(*items);
		designerParent = items;
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
	auto* previousParent = control->GetVisualParent();
	if (!previousParent || state.ChildIndex < 0
		|| desiredRuntimeParent == control)
	{
		if (outError) *outError = L"布局差量父子关系无效。";
		return false;
	}
	for (Control* ancestor = desiredRuntimeParent; ancestor;
		ancestor = ancestor->GetVisualParent())
	{
		if (ancestor == control)
		{
			if (outError) *outError = L"布局差量会形成父子循环。";
			return false;
		}
	}

	try
	{
		size_t previousItemIndex = 0;
		auto* previousItemsOwner =
			FindAuthoredItemsOwner(control, &previousItemIndex);
		auto* desiredItemsOwner = state.ParentKind
			== DesignerPlacementParentKind::ItemsControl
			? dynamic_cast<ItemsControl*>(desiredDesignerParent) : nullptr;
		const int previousIndex = previousItemsOwner
			? static_cast<int>(previousItemIndex)
			: previousParent->IndexOfVisualChild(control);
		if (previousIndex < 0)
		{
			if (outError) *outError = L"布局差量目标不在当前父级中。";
			return false;
		}
		if (previousItemsOwner && previousItemsOwner == desiredItemsOwner)
		{
			if (state.ChildIndex >= static_cast<int>(
				desiredItemsOwner->AuthoredItemCount()))
			{
				if (outError) *outError = L"布局差量 Items 同级顺序越界。";
				return false;
			}
			if (previousIndex != state.ChildIndex
				&& !desiredItemsOwner->MoveItemControl(
					previousItemIndex, static_cast<size_t>(state.ChildIndex)))
			{
				if (outError) *outError = L"无法移动 authored Item。";
				return false;
			}
		}
		else if (!previousItemsOwner && !desiredItemsOwner
			&& previousParent == desiredRuntimeParent)
		{
			if (state.ChildIndex >= desiredRuntimeParent->VisualChildCount())
			{
				if (outError) *outError = L"布局差量同级顺序越界。";
				return false;
			}
			if (previousIndex != state.ChildIndex)
				desiredRuntimeParent->MoveVisualChild(
					static_cast<size_t>(previousIndex),
					static_cast<size_t>(state.ChildIndex));
		}
		else
		{
			if (state.ChildIndex > desiredRuntimeParent->VisualChildCount())
			{
				if (outError) *outError = L"布局差量目标父级顺序越界。";
				return false;
			}
			auto owner = previousItemsOwner
				? previousItemsOwner->DetachItemControlAt(previousItemIndex)
				: previousParent->DetachVisualChild(control);
			if (!owner)
			{
				if (outError) *outError = L"无法从原父级分离布局目标。";
				return false;
			}
			Control* raw = owner.get();
			try
			{
				if (desiredItemsOwner)
					desiredItemsOwner->InsertItemControl(
						static_cast<size_t>(state.ChildIndex), raw);
				else
					desiredRuntimeParent->InsertVisualChild(
						state.ChildIndex, raw);
				owner.release();
			}
			catch (...)
			{
				std::unique_ptr<Control> rollbackOwner;
				if (desiredItemsOwner)
					rollbackOwner = desiredItemsOwner->DetachItemControl(raw);
				if (!rollbackOwner && raw->GetVisualParent())
					rollbackOwner = raw->GetVisualParent()->DetachVisualChild(raw);
				else if (owner.get() == raw)
					rollbackOwner = std::move(owner);
				if (rollbackOwner)
				{
					const int rollbackIndex = (std::clamp)(
						previousIndex, 0, previousParent->VisualChildCount());
					try
					{
						if (previousItemsOwner)
							previousItemsOwner->InsertItemControl(
								static_cast<size_t>(rollbackIndex),
								rollbackOwner.get());
						else
							previousParent->InsertVisualChild(
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
				if (outError) *outError = raw->GetVisualParent() == previousParent
					? L"挂载布局目标失败，已恢复原父级。"
					: L"挂载布局目标失败，且原父级恢复失败。";
				return false;
			}
		}

		target->DesignerParent = desiredDesignerParent;
		target->ComponentContentProperty = state.ComponentContentProperty;
		auto restoreLocal = [&](
			DesignerPlacementLocalValue value,
			const wchar_t* propertyName,
			auto&& valueMatches,
			auto&& setValue)
		{
			const bool wantsLocal = state.HasLocalValue(value);
			const bool hasLocal = control->HasPropertyValue(
				propertyName, DependencyPropertyValueSource::Local);
			if (wantsLocal)
			{
				// Avoid replacing a Local binding/resource expression when its
				// effective layout value already matches the snapshot.
				if (!hasLocal || !valueMatches()) setValue();
			}
			else if (hasLocal && !control->ClearPropertyValue(propertyName))
				throw std::runtime_error(
					"failed to clear placement Local value");
		};
		restoreLocal(DesignerPlacementLocalValue::Dock, L"DockPanel.Dock",
			[&] { return DockPanel::GetDock(*control) == state.DockPosition; },
			[&] { DockPanel::SetDock(*control, state.DockPosition); });
		restoreLocal(DesignerPlacementLocalValue::GridRow, L"Grid.Row",
			[&] { return Grid::GetRow(*control) == state.GridRow; },
			[&] { Grid::SetRow(*control, state.GridRow); });
		restoreLocal(DesignerPlacementLocalValue::GridColumn, L"Grid.Column",
			[&] { return Grid::GetColumn(*control) == state.GridColumn; },
			[&] { Grid::SetColumn(*control, state.GridColumn); });
		restoreLocal(DesignerPlacementLocalValue::GridRowSpan, L"Grid.RowSpan",
			[&] { return Grid::GetRowSpan(*control) == state.GridRowSpan; },
			[&] { Grid::SetRowSpan(*control, state.GridRowSpan); });
		restoreLocal(DesignerPlacementLocalValue::GridColumnSpan,
			L"Grid.ColumnSpan",
			[&] { return Grid::GetColumnSpan(*control) == state.GridColumnSpan; },
			[&] { Grid::SetColumnSpan(*control, state.GridColumnSpan); });
		restoreLocal(DesignerPlacementLocalValue::ZIndex, L"ZIndex",
			[&] { return control->ZIndex == state.ZIndex; },
			[&] { control->ZIndex = state.ZIndex; });
		restoreLocal(DesignerPlacementLocalValue::HorizontalAlignment,
			L"HorizontalAlignment",
			[&] { return control->HorizontalAlignment == state.Horizontal; },
			[&] { control->HorizontalAlignment = state.Horizontal; });
		restoreLocal(DesignerPlacementLocalValue::VerticalAlignment,
			L"VerticalAlignment",
			[&] { return control->VerticalAlignment == state.Vertical; },
			[&] { control->VerticalAlignment = state.Vertical; });
		restoreLocal(DesignerPlacementLocalValue::Width, L"Width",
			[&] { return control->Width == state.Width; },
			[&] { control->Width = state.Width; });
		restoreLocal(DesignerPlacementLocalValue::Height, L"Height",
			[&] { return control->Height == state.Height; },
			[&] { control->Height = state.Height; });
		restoreLocal(DesignerPlacementLocalValue::Margin, L"Margin",
			[&] { return control->Margin == state.Margin; },
			[&] { control->Margin = state.Margin; });
		restoreLocal(DesignerPlacementLocalValue::CanvasLeft, L"Canvas.Left",
			[&] {
				return CanvasOffsetEqual(
					Canvas::GetLeft(*control), state.CanvasLeft);
			},
			[&] { Canvas::SetLeft(*control, state.CanvasLeft); });
		restoreLocal(DesignerPlacementLocalValue::CanvasTop, L"Canvas.Top",
			[&] {
				return CanvasOffsetEqual(
					Canvas::GetTop(*control), state.CanvasTop);
			},
			[&] { Canvas::SetTop(*control, state.CanvasTop); });
		restoreLocal(DesignerPlacementLocalValue::CanvasRight, L"Canvas.Right",
			[&] {
				return CanvasOffsetEqual(
					Canvas::GetRight(*control), state.CanvasRight);
			},
			[&] { Canvas::SetRight(*control, state.CanvasRight); });
		restoreLocal(DesignerPlacementLocalValue::CanvasBottom, L"Canvas.Bottom",
			[&] {
				return CanvasOffsetEqual(
					Canvas::GetBottom(*control), state.CanvasBottom);
			},
			[&] { Canvas::SetBottom(*control, state.CanvasBottom); });
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
				&& target->ControlInstance->GetVisualParent())
			{
				const auto parentSize =
					target->ControlInstance->GetVisualParent()->GetActualSizeDip();
				applyError += L", parent="
					+ std::to_wstring(parentSize.width) + L"x"
					+ std::to_wstring(parentSize.height);
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
		&& ComponentContentProperty == other.ComponentContentProperty
		&& ChildIndex == other.ChildIndex
		&& LocalValueMask == other.LocalValueMask
		// A placement delta owns authored Local values, not effective values
		// supplied by Style, Theme, inheritance or metadata. Reattaching the
		// same subtree beneath a rebuilt parent must preserve the absence of a
		// Local contribution without freezing the old effective value as Local.
		&& (!HasLocalValue(DesignerPlacementLocalValue::Margin)
			|| Margin == other.Margin)
		&& (!HasLocalValue(DesignerPlacementLocalValue::Width)
			|| Width == other.Width)
		&& (!HasLocalValue(DesignerPlacementLocalValue::Height)
			|| Height == other.Height)
		&& (!HasLocalValue(DesignerPlacementLocalValue::CanvasLeft)
			|| CanvasOffsetEqual(CanvasLeft, other.CanvasLeft))
		&& (!HasLocalValue(DesignerPlacementLocalValue::CanvasTop)
			|| CanvasOffsetEqual(CanvasTop, other.CanvasTop))
		&& (!HasLocalValue(DesignerPlacementLocalValue::CanvasRight)
			|| CanvasOffsetEqual(CanvasRight, other.CanvasRight))
		&& (!HasLocalValue(DesignerPlacementLocalValue::CanvasBottom)
			|| CanvasOffsetEqual(CanvasBottom, other.CanvasBottom))
		&& (!HasLocalValue(
				DesignerPlacementLocalValue::HorizontalAlignment)
			|| Horizontal == other.Horizontal)
		&& (!HasLocalValue(
				DesignerPlacementLocalValue::VerticalAlignment)
			|| Vertical == other.Vertical)
		&& (!HasLocalValue(DesignerPlacementLocalValue::Dock)
			|| DockPosition == other.DockPosition)
		&& (!HasLocalValue(DesignerPlacementLocalValue::GridRow)
			|| GridRow == other.GridRow)
		&& (!HasLocalValue(DesignerPlacementLocalValue::GridColumn)
			|| GridColumn == other.GridColumn)
		&& (!HasLocalValue(DesignerPlacementLocalValue::GridRowSpan)
			|| GridRowSpan == other.GridRowSpan)
		&& (!HasLocalValue(DesignerPlacementLocalValue::GridColumnSpan)
			|| GridColumnSpan == other.GridColumnSpan)
		&& (!HasLocalValue(DesignerPlacementLocalValue::ZIndex)
			|| ZIndex == other.ZIndex);
}

bool DesignerControlPlacementState::HasLocalValue(
	DesignerPlacementLocalValue value) const noexcept
{
	return (LocalValueMask & static_cast<uint32_t>(value)) != 0;
}

void DesignerControlPlacementState::SetLocalValue(
	DesignerPlacementLocalValue value,
	bool present) noexcept
{
	const auto mask = static_cast<uint32_t>(value);
	if (present) LocalValueMask |= mask;
	else LocalValueMask &= ~mask;
}

size_t DesignerControlPlacementState::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ TargetName.capacity() * sizeof(wchar_t)
		+ ParentName.capacity() * sizeof(wchar_t)
		+ ComponentContentProperty.capacity() * sizeof(wchar_t);
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

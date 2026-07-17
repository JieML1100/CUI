#pragma once
#include "Panel.h"
#include "Form.h"
#include "Layout/LegacyCanvasAdapter.h"
#include <algorithm>
#include <cmath>
#include <utility>
#pragma comment(lib, "Imm32.lib")

namespace
{
	template<typename TValue>
	ControlPropertyOptions<Panel, TValue> PanelPropertyOptions(
		TValue defaultValue,
		int order,
		ControlPropertyEditorKind editor)
	{
		ControlPropertyOptions<Panel, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto PanelPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Panel& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	ControlPropertyOptions<Panel, float> PanelMetricOptions(
		float defaultValue,
		int order)
	{
		auto options = PanelPropertyOptions(
			defaultValue, order, ControlPropertyEditorKind::Number);
		options.Coerce = [](Panel&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	bool PanelColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}
}

UIClass Panel::Type() { return UIClass::UI_Panel; }

void Panel::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<Panel, float>(L"BorderThickness",
			[](Panel& target) { return target.BorderThickness; },
			[](Panel& target, const float& value) { target.BorderThickness = value; },
			PanelPropertySubscriber(L"BorderThickness"),
			PanelMetricOptions(1.5f, 10));
		RegisterPanelCornerRadiusMetadata<Panel>(0.0f);

		auto disabledOptions = PanelPropertyOptions(
			cui::theme::palette::DisabledOverlay,
			30,
			ControlPropertyEditorKind::Color);
		disabledOptions.Equals = PanelColorsEqual;
		BindingPropertyRegistry::Register<Panel, D2D1_COLOR_F>(L"DisabledOverlayColor",
			[](Panel& target) { return target.DisabledOverlayColor; },
			[](Panel& target, const D2D1_COLOR_F& value)
			{
				target.DisabledOverlayColor = value;
			},
			PanelPropertySubscriber(L"DisabledOverlayColor"),
			std::move(disabledOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(Panel, float, BorderThickness)
{
	return _borderThickness;
}

SET_CPP(Panel, float, BorderThickness)
{
	(void)SetPropertyField(L"BorderThickness", _borderThickness, value);
}

GET_CPP(Panel, float, CornerRadius)
{
	return _cornerRadius;
}

SET_CPP(Panel, float, CornerRadius)
{
	(void)SetPropertyField(L"CornerRadius", _cornerRadius, value);
}

GET_CPP(Panel, D2D1_COLOR_F, DisabledOverlayColor)
{
	return _disabledOverlayColor;
}

SET_CPP(Panel, D2D1_COLOR_F, DisabledOverlayColor)
{
	(void)SetPropertyField(
		L"DisabledOverlayColor", _disabledOverlayColor, value);
}

Panel::Panel()
{
}

Panel::Panel(int x, int y, int width, int height)
	: Panel()
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

Panel::~Panel() = default;

cui::core::Size Panel::MeasureCore(const cui::core::Constraints& available)
{
	const auto padding = this->GetSpecifiedLayout().padding;
	const auto contentConstraints = available.Deflate(padding);
	cui::core::Size contentDesired{};

	if (_layoutEngine)
	{
		LayoutContext context(this);
		contentDesired = _layoutEngine->Measure(context, contentConstraints);
	}
	else
	{
		for (auto* child : this->Children)
		{
			if (!child || !child->Visible) continue;
			const auto desired = child->Measure(contentConstraints);
			const auto slot = cui::layout::compat::GetLegacyCanvasSlot(*child);
			contentDesired.width = (std::max)(contentDesired.width,
				slot.location.x + desired.width + slot.margin.right);
			contentDesired.height = (std::max)(contentDesired.height,
				slot.location.y + desired.height + slot.margin.bottom);
		}
	}

	return cui::core::Size{
		contentDesired.width + padding.Horizontal(),
		contentDesired.height + padding.Vertical() };
}

void Panel::SetLayoutEngine(class LayoutEngine* engine)
{
	if (_layoutEngine.get() == engine)
	{
		InvalidateLayout();
		return;
	}
	_layoutEngine.reset(engine);
	InvalidateLayout();
}

void Panel::RequestLayout()
{
	InvalidateLayout();
}

void Panel::OnComputedLayoutSizeChanged()
{
	InvalidateLayout();
}

void Panel::PerformPendingLayout()
{
	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
		PerformLayout();
}

void Panel::InvalidateLayout()
{
	_needsLayout = true;
	if (_layoutEngine)
	{
		_layoutEngine->Invalidate();
	}

	// A child's desired geometry can affect every auto-sized ancestor. Bubble
	// the request to the root and ensure the window schedules a new frame.
	Control::RequestLayout();
}

void Panel::PerformLayout()
{
	if (!_layoutEngine)
	{
		// Compatibility path: Panel remains a WinForms-like absolute/anchor
		// container, but the geometry rules now live in one pure Canvas policy.
		const auto containerSize = this->GetActualSizeDip();
		const Thickness padding = this->Padding;
		const cui::core::Rect contentRect{
			padding.Left,
			padding.Top,
			(std::max)(0.0f, containerSize.width - padding.Left - padding.Right),
			(std::max)(0.0f, containerSize.height - padding.Top - padding.Bottom)
		};
		const cui::core::Constraints measureAvailable{ contentRect.Extent() };

		for (auto* child : this->Children)
		{
			if (!child || !child->Visible) continue;
			cui::layout::compat::ArrangeLegacyCanvasChild(
				*child, contentRect, measureAvailable);
		}
	}
	else
	{
		// 使用布局引擎
		if (_needsLayout || _layoutEngine->NeedsLayout())
		{
			LayoutContext context(this);
			const auto actualSize = this->GetActualSizeDip();
			Thickness padding = this->Padding;
			const float availableWidth = (std::max)(
				0.0f, actualSize.width - padding.Left - padding.Right);
			const float availableHeight = (std::max)(
				0.0f, actualSize.height - padding.Top - padding.Bottom);
			const cui::core::Constraints availableSize{ cui::core::Size{
				availableWidth, availableHeight } };
			_layoutEngine->Measure(context, availableSize);
			
			D2D1_RECT_F finalRect = { 
				padding.Left,
				padding.Top,
				padding.Left + availableWidth,
				padding.Top + availableHeight
			};
			_layoutEngine->Arrange(context, finalRect);
		}
	}
	
	_needsLayout = false;
}

void Panel::Update()
{
	if (this->IsVisual == false) return;
	
	// 执行布局
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformLayout();
	}
	
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (auto child : this->GetChildrenInZOrder())
			{
				if (!child || !child->Visible) continue;
				child->Update();
			}
		}
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool Panel::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	for (auto child : this->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		const auto childLocation = child->GetActualLocationDip();
		const cui::core::Rect childRect{
			childLocation, child->GetActualSizeDip() };
		if (childRect.Contains(cui::core::Point{
			(float)localX, (float)localY }))
		{
			child->ProcessMessage(
				message, wParam, lParam,
				static_cast<int>(std::floor((float)localX - childLocation.x)),
				static_cast<int>(std::floor((float)localY - childLocation.y)));
		}
	}
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (!files.empty())
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
	}
	break;
	}
	return true;
}

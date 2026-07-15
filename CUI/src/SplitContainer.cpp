#include "SplitContainer.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<SplitContainer, TValue> SplitPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags)
	{
		ControlPropertyOptions<SplitContainer, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto SplitPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			SplitContainer& target,
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

	bool SplitColorsEqual(const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}
}

UIClass SplitContainer::Type() { return UIClass::UI_SplitContainer; }

void SplitContainer::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		const auto geometryFlags = ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender;
		auto orientationOptions = SplitPropertyOptions(
			Orientation::Horizontal, L"Layout", 100, 10,
			ControlPropertyEditorKind::Choice, geometryFlags);
		orientationOptions.Coerce = [](
			SplitContainer&, const Orientation& proposed) -> std::optional<Orientation>
		{
			if (proposed != Orientation::Horizontal
				&& proposed != Orientation::Vertical) return std::nullopt;
			return proposed;
		};
		orientationOptions.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		BindingPropertyRegistry::Register<SplitContainer, Orientation>(L"SplitOrientation",
			[](SplitContainer& target) { return target.GetSplitOrientation(); },
			[](SplitContainer& target, const Orientation& value)
			{ target.SetSplitOrientation(value); },
			SplitPropertySubscriber(L"SplitOrientation"), std::move(orientationOptions));

		auto distanceOptions = SplitPropertyOptions(
			160, L"Layout", 100, 20,
			ControlPropertyEditorKind::Number, geometryFlags);
		distanceOptions.Coerce = [](
			SplitContainer& target, const int& proposed) -> std::optional<int>
		{
			return target.ClampSplitterDistance(proposed);
		};
		distanceOptions.Design.Minimum = 0.0;
		distanceOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<SplitContainer, int>(L"SplitterDistance",
			[](SplitContainer& target) { return target.GetSplitterDistance(); },
			[](SplitContainer& target, const int& value)
			{ target.SetSplitterDistance(value); },
			SplitPropertySubscriber(L"SplitterDistance"), std::move(distanceOptions));

		auto widthOptions = SplitPropertyOptions(
			6, L"Layout", 100, 30,
			ControlPropertyEditorKind::Number, geometryFlags);
		widthOptions.Coerce = [](
			SplitContainer&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(1, proposed);
		};
		widthOptions.Design.Minimum = 1.0;
		widthOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<SplitContainer, int>(L"SplitterWidth",
			[](SplitContainer& target) { return target.GetSplitterWidth(); },
			[](SplitContainer& target, const int& value)
			{ target.SetSplitterWidth(value); },
			SplitPropertySubscriber(L"SplitterWidth"), std::move(widthOptions));

		auto minimumOptions = [](int order)
		{
			auto options = SplitPropertyOptions(
				48, L"Layout", 100, order,
				ControlPropertyEditorKind::Number,
				ControlPropertyFlags::AffectsArrange
					| ControlPropertyFlags::AffectsRender);
			options.Coerce = [](
				SplitContainer&, const int& proposed) -> std::optional<int>
			{
				return (std::max)(0, proposed);
			};
			options.Design.Minimum = 0.0;
			options.Design.Step = 1.0;
			return options;
		};
		BindingPropertyRegistry::Register<SplitContainer, int>(L"Panel1MinSize",
			[](SplitContainer& target) { return target.GetPanel1MinSize(); },
			[](SplitContainer& target, const int& value)
			{ target.SetPanel1MinSize(value); },
			SplitPropertySubscriber(L"Panel1MinSize"), minimumOptions(40));
		BindingPropertyRegistry::Register<SplitContainer, int>(L"Panel2MinSize",
			[](SplitContainer& target) { return target.GetPanel2MinSize(); },
			[](SplitContainer& target, const int& value)
			{ target.SetPanel2MinSize(value); },
			SplitPropertySubscriber(L"Panel2MinSize"), minimumOptions(50));

		auto fixedOptions = SplitPropertyOptions(
			false, L"Layout", 100, 60,
			ControlPropertyEditorKind::Boolean,
			ControlPropertyFlags::AffectsRender);
		BindingPropertyRegistry::Register<SplitContainer, bool>(L"IsSplitterFixed",
			[](SplitContainer& target) { return target.GetIsSplitterFixed(); },
			[](SplitContainer& target, const bool& value)
			{ target.SetIsSplitterFixed(value); },
			SplitPropertySubscriber(L"IsSplitterFixed"), std::move(fixedOptions));

		auto colorOptions = [](D2D1_COLOR_F defaultValue, int order)
		{
			auto options = SplitPropertyOptions(
				defaultValue, L"Appearance", 200, order,
				ControlPropertyEditorKind::Color,
				ControlPropertyFlags::AffectsRender);
			options.Equals = SplitColorsEqual;
			return options;
		};
		BindingPropertyRegistry::Register<SplitContainer, D2D1_COLOR_F>(L"SplitterColor",
			[](SplitContainer& target) { return target.GetSplitterColor(); },
			[](SplitContainer& target, const D2D1_COLOR_F& value)
			{ target.SetSplitterColor(value); },
			SplitPropertySubscriber(L"SplitterColor"),
			colorOptions(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.08f }, 10));
		BindingPropertyRegistry::Register<SplitContainer, D2D1_COLOR_F>(L"SplitterHotColor",
			[](SplitContainer& target) { return target.GetSplitterHotColor(); },
			[](SplitContainer& target, const D2D1_COLOR_F& value)
			{ target.SetSplitterHotColor(value); },
			SplitPropertySubscriber(L"SplitterHotColor"),
			colorOptions(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f }, 20));
		BindingPropertyRegistry::Register<SplitContainer, D2D1_COLOR_F>(L"SplitterPressedColor",
			[](SplitContainer& target) { return target.GetSplitterPressedColor(); },
			[](SplitContainer& target, const D2D1_COLOR_F& value)
			{ target.SetSplitterPressedColor(value); },
			SplitPropertySubscriber(L"SplitterPressedColor"),
			colorOptions(D2D1_COLOR_F{ 0.35f, 0.64f, 0.96f, 0.90f }, 30));

		auto visualMetricOptions = [](float defaultValue, int order)
		{
			auto options = SplitPropertyOptions(
				defaultValue, L"Appearance", 200, order,
				ControlPropertyEditorKind::Number,
				ControlPropertyFlags::AffectsRender);
			options.Coerce = [](
				SplitContainer&, const float& proposed) -> std::optional<float>
			{
				return std::isfinite(proposed) && proposed > 0.0f ? proposed : 0.0f;
			};
			options.Design.Minimum = 0.0;
			options.Design.Step = 0.5;
			return options;
		};
		BindingPropertyRegistry::Register<SplitContainer, float>(L"SplitterCornerRadius",
			[](SplitContainer& target) { return target.GetSplitterCornerRadius(); },
			[](SplitContainer& target, const float& value)
			{ target.SetSplitterCornerRadius(value); },
			SplitPropertySubscriber(L"SplitterCornerRadius"), visualMetricOptions(3.0f, 40));
		BindingPropertyRegistry::Register<SplitContainer, float>(L"SplitterVisualInset",
			[](SplitContainer& target) { return target.GetSplitterVisualInset(); },
			[](SplitContainer& target, const float& value)
			{ target.SetSplitterVisualInset(value); },
			SplitPropertySubscriber(L"SplitterVisualInset"), visualMetricOptions(8.0f, 50));
		return true;
	}();
	(void)registered;
}

void SplitContainer::PerformPendingLayout()
{
	if (_needsLayout)
		ArrangeSplitPanels();
}

SplitContainer::SplitContainer()
	: Panel()
{
	EnsureChildPanels();
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

SplitContainer::SplitContainer(int x, int y, int width, int height)
	: SplitContainer()
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

void SplitContainer::SetSplitOrientation(::Orientation value)
{
	if (!SetPropertyField(L"SplitOrientation", SplitOrientation, value)) return;
	SetSplitterDistance(SplitterDistance);
	RefreshSplitterLayout();
}

void SplitContainer::SetSplitterWidth(int value)
{
	if (!SetPropertyField(L"SplitterWidth", SplitterWidth, value)) return;
	SetSplitterDistance(SplitterDistance);
	RefreshSplitterLayout();
}

void SplitContainer::SetPanel1MinSize(int value)
{
	if (!SetPropertyField(L"Panel1MinSize", Panel1MinSize, value)) return;
	SetSplitterDistance(SplitterDistance);
	RefreshSplitterLayout();
}

void SplitContainer::SetPanel2MinSize(int value)
{
	if (!SetPropertyField(L"Panel2MinSize", Panel2MinSize, value)) return;
	SetSplitterDistance(SplitterDistance);
	RefreshSplitterLayout();
}

void SplitContainer::SetIsSplitterFixed(bool value)
{
	(void)SetPropertyField(L"IsSplitterFixed", IsSplitterFixed, value);
}

void SplitContainer::SetSplitterColor(D2D1_COLOR_F value)
{
	(void)SetPropertyField(L"SplitterColor", SplitterColor, value);
}

void SplitContainer::SetSplitterHotColor(D2D1_COLOR_F value)
{
	(void)SetPropertyField(L"SplitterHotColor", SplitterHotColor, value);
}

void SplitContainer::SetSplitterPressedColor(D2D1_COLOR_F value)
{
	(void)SetPropertyField(L"SplitterPressedColor", SplitterPressedColor, value);
}

void SplitContainer::SetSplitterCornerRadius(float value)
{
	(void)SetPropertyField(L"SplitterCornerRadius", SplitterCornerRadius, value);
}

void SplitContainer::SetSplitterVisualInset(float value)
{
	(void)SetPropertyField(L"SplitterVisualInset", SplitterVisualInset, value);
}

void SplitContainer::EnsureChildPanels()
{
	if (_panel1 && _panel2) return;
	_panel1 = Panel::AddControl(new Panel(0, 0, 100, 100));
	_panel2 = Panel::AddControl(new Panel(0, 0, 100, 100));
	_panel1->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	_panel2->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

RECT SplitContainer::GetSplitterRect()
{
	RECT splitterRect{ 0, 0, 0, 0 };
	const auto containerSize = this->GetActualSizeDip();
	const int containerWidth = (std::max)(0, (int)std::floor(containerSize.width));
	const int containerHeight = (std::max)(0, (int)std::floor(containerSize.height));
	int distance = ClampSplitterDistance(this->SplitterDistance);
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	if (SplitOrientation == Orientation::Horizontal)
	{
		splitterRect.left = distance;
		splitterRect.top = 0;
		splitterRect.right = (distance + splitterWidth) < containerWidth ? (distance + splitterWidth) : containerWidth;
		splitterRect.bottom = containerHeight;
	}
	else
	{
		splitterRect.left = 0;
		splitterRect.top = distance;
		splitterRect.right = containerWidth;
		splitterRect.bottom = (distance + splitterWidth) < containerHeight ? (distance + splitterWidth) : containerHeight;
	}
	return splitterRect;
}

int SplitContainer::ClampSplitterDistance(int value)
{
	const auto containerSize = this->GetActualSizeDip();
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	const int panel1MinSize = (std::max)(0, Panel1MinSize);
	const int panel2MinSize = (std::max)(0, Panel2MinSize);
	int availableLength = (std::max)(0, (int)std::floor(
		SplitOrientation == Orientation::Horizontal
		? containerSize.width : containerSize.height));
	int maxDistance = availableLength - splitterWidth - panel2MinSize;
	if (maxDistance < panel1MinSize)
	{
		maxDistance = (std::max)(0, availableLength - splitterWidth);
	}
	int minDistance = (std::min)(panel1MinSize, maxDistance);
	return (std::clamp)(value, minDistance, maxDistance);
}

void SplitContainer::SetSplitterDistanceInternal(int value)
{
	if (!SetCurrentPropertyField(
		L"SplitterDistance", SplitterDistance, value)) return;
	RefreshSplitterLayout();
}

void SplitContainer::SetSplitterDistance(int value)
{
	if (!SetPropertyField(L"SplitterDistance", SplitterDistance, value)) return;
	RefreshSplitterLayout();
}

void SplitContainer::RefreshSplitterLayout()
{
	this->InvalidateLayout();
	if (!this->IsLayoutSuspended())
		ArrangeSplitPanels();
	this->InvalidateVisual();
}

void SplitContainer::ArrangeSplitPanels()
{
	EnsureChildPanels();

	auto containerSize = this->GetActualSizeDip();
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	int distance = ClampSplitterDistance(this->SplitterDistance);
	this->SplitterDistance = distance;

	if (SplitOrientation == Orientation::Horizontal)
	{
		_panel1->ApplyLayout(cui::core::Rect{
			0.0f, 0.0f, (float)distance, containerSize.height });
		float panel2Width = containerSize.width - distance - splitterWidth;
		if (panel2Width < 0) panel2Width = 0;
		_panel2->ApplyLayout(cui::core::Rect{
			(float)(distance + splitterWidth), 0.0f,
			panel2Width, containerSize.height });
	}
	else
	{
		_panel1->ApplyLayout(cui::core::Rect{
			0.0f, 0.0f, containerSize.width, (float)distance });
		float panel2Height = containerSize.height - distance - splitterWidth;
		if (panel2Height < 0) panel2Height = 0;
		_panel2->ApplyLayout(cui::core::Rect{
			0.0f, (float)(distance + splitterWidth),
			containerSize.width, panel2Height });
	}

	_needsLayout = false;
}

bool SplitContainer::HitSplitter(int localX, int localY)
{
	auto splitterRect = GetSplitterRect();
	return localX >= splitterRect.left && localX < splitterRect.right && localY >= splitterRect.top && localY < splitterRect.bottom;
}

bool SplitContainer::HitChildPanel(Panel* child, int localX, int localY, int& childX, int& childY)
{
	if (!child || !child->Visible || !child->Enable) return false;
	const auto childLocation = child->GetActualLocationDip();
	const auto childSize = child->GetActualSizeDip();
	const cui::core::Rect childRect{ childLocation, childSize };
	if (!childRect.Contains(cui::core::Point{ (float)localX, (float)localY }))
		return false;
	childX = static_cast<int>(std::floor((float)localX - childLocation.x));
	childY = static_cast<int>(std::floor((float)localY - childLocation.y));
	return true;
}

CursorKind SplitContainer::QueryCursor(int localX, int localY)
{
	if (HitSplitter(localX, localY) && !IsSplitterFixed)
	{
		return SplitOrientation == Orientation::Horizontal ? CursorKind::SizeWE : CursorKind::SizeNS;
	}
	return this->Cursor;
}

void SplitContainer::Update()
{
	if (this->IsVisual == false) return;
	if (!this->IsLayoutSuspended() && _needsLayout)
	{
		ArrangeSplitPanels();
	}

	auto d2d = this->ParentForm->Render;
	auto containerSize = this->GetActualSizeDip();
	const float actualWidth = containerSize.width;
	const float actualHeight = containerSize.height;
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	auto splitterRect = GetSplitterRect();
	D2D1_COLOR_F splitterColor = _draggingSplitter ? SplitterPressedColor : (_isSplitterHovered ? SplitterHotColor : SplitterColor);

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
			if (_panel1) _panel1->Update();
			if (_panel2) _panel2->Update();
		}
		{
			const float splitterX = static_cast<float>(splitterRect.left);
			const float splitterY = static_cast<float>(splitterRect.top);
			const float splitterW = static_cast<float>(splitterRect.right - splitterRect.left);
			const float splitterH = static_cast<float>(splitterRect.bottom - splitterRect.top);
			const float inset = (std::max)(0.0f, this->SplitterVisualInset);
			float visualX = splitterX;
			float visualY = splitterY;
			float visualW = splitterW;
			float visualH = splitterH;
			if (SplitOrientation == Orientation::Horizontal)
			{
				visualW = (std::max)(2.0f, splitterW - 2.0f);
				visualX = splitterX + (splitterW - visualW) * 0.5f;
				visualY = splitterY + (std::min)(inset, splitterH * 0.45f);
				visualH = (std::max)(0.0f, splitterH - (visualY - splitterY) * 2.0f);
			}
			else
			{
				visualH = (std::max)(2.0f, splitterH - 2.0f);
				visualY = splitterY + (splitterH - visualH) * 0.5f;
				visualX = splitterX + (std::min)(inset, splitterW * 0.45f);
				visualW = (std::max)(0.0f, splitterW - (visualX - splitterX) * 2.0f);
			}
			if (visualW > 0.0f && visualH > 0.0f)
			{
				const float splitterRadius = (std::clamp)(this->SplitterCornerRadius, 0.0f, (std::min)(visualW, visualH) * 0.5f);
				d2d->FillRoundRect(visualX, visualY, visualW, visualH, splitterColor, splitterRadius);
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

bool SplitContainer::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	if (!this->IsLayoutSuspended() && _needsLayout)
	{
		ArrangeSplitPanels();
	}

	_isSplitterHovered = HitSplitter(localX, localY);

	if (_draggingSplitter && message == WM_MOUSEMOVE)
	{
		int pointerCoordinate = SplitOrientation == Orientation::Horizontal ? localX : localY;
		SetSplitterDistanceInternal(pointerCoordinate - _splitterDragOffset);
		return true;
	}
	if (_draggingSplitter && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_draggingSplitter = false;
		this->InvalidateVisual();
	}

	if (message == WM_LBUTTONDOWN && _isSplitterHovered && !IsSplitterFixed)
	{
		auto splitterRect = GetSplitterRect();
		_draggingSplitter = true;
		_splitterDragOffset = SplitOrientation == Orientation::Horizontal ? (localX - splitterRect.left) : (localY - splitterRect.top);
		this->InvalidateVisual();
		return true;
	}

	int childX = 0;
	int childY = 0;
	if (!_isSplitterHovered)
	{
		if (HitChildPanel(_panel2, localX, localY, childX, childY))
		{
			_panel2->ProcessMessage(message, wParam, lParam, childX, childY);
		}
		else if (HitChildPanel(_panel1, localX, localY, childX, childY))
		{
			_panel1->ProcessMessage(message, wParam, lParam, childX, childY);
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
		for (int fileIndex = 0; fileIndex < (int)fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
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

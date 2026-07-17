#include "ToolBar.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<ToolBar, TValue> ToolBarPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags)
	{
		ControlPropertyOptions<ToolBar, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto ToolBarPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ToolBar& target,
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

	ControlPropertyOptions<ToolBar, int> ToolBarIntOptions(
		int defaultValue,
		int order,
		int minimum = 0)
	{
		auto options = ToolBarPropertyOptions(
			defaultValue, L"Layout", 100, order,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender);
		options.Coerce = [minimum](ToolBar&, const int& proposed)
			-> std::optional<int>
		{
			return (std::max)(minimum, proposed);
		};
		options.Design.Minimum = (double)minimum;
		options.Design.Step = 1.0;
		return options;
	}

	bool ToolBarColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<ToolBar, D2D1_COLOR_F> ToolBarColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ToolBarPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color,
			ControlPropertyFlags::AffectsRender);
		options.Equals = ToolBarColorsEqual;
		return options;
	}

	void ApplyDefaultToolButtonStyle(Button* button)
	{
		if (!button) return;
		button->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
		button->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
		button->ForeColor = cui::theme::palette::TextPrimary;
		button->UnderMouseColor = cui::theme::palette::AccentSoft;
		button->CheckedColor = cui::theme::palette::AccentSelected;
		button->Round = 0.28f;
		button->BorderThickness = 0.0f;
	}
}

UIClass ToolBarSeparator::Type() { return UIClass::UI_CUSTOM; }

ToolBarSeparator::ToolBarSeparator(int width, int height)
{
	if (width < 1) width = 1;
	if (height < 1) height = 20;
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->LineColor = cui::theme::palette::Border;
	this->Enable = false;
}

void ToolBarSeparator::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	if (actualWidth <= 0.0f || actualHeight <= 0.0f) return;
	const float lineWidth = (std::min)(1.5f, (std::max)(1.0f, actualWidth));
	const float x = (actualWidth - lineWidth) * 0.5f;
	const float inset = (std::min)(6.0f, actualHeight * 0.28f);
	const float lineHeight = (std::max)(1.0f, actualHeight - inset * 2.0f);
	this->BeginRender();
	{
		if (this->LineColor.a > 0.0f)
			d2d->FillRoundRect(x, inset, lineWidth, lineHeight, this->LineColor, lineWidth * 0.5f);
	}
	this->EndRender();
}

UIClass ToolBarSpacer::Type() { return UIClass::UI_CUSTOM; }

ToolBarSpacer::ToolBarSpacer(int width)
{
	if (width < 0) width = 0;
	this->Size = SIZE{ width,1 };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Enable = false;
}

void ToolBarSpacer::Update()
{
}

UIClass ToolBar::Type() { return UIClass::UI_ToolBar; }

void ToolBar::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		RegisterPanelCornerRadiusMetadata<ToolBar>(8.0f);
		BindingPropertyRegistry::Register<ToolBar, int>(L"HorizontalPadding",
			[](ToolBar& target) { return target.HorizontalPadding; },
			[](ToolBar& target, const int& value) { target.HorizontalPadding = value; },
			ToolBarPropertySubscriber(L"HorizontalPadding"),
			ToolBarIntOptions(6, 10));
		BindingPropertyRegistry::Register<ToolBar, int>(L"Gap",
			[](ToolBar& target) { return target.Gap; },
			[](ToolBar& target, const int& value) { target.Gap = value; },
			ToolBarPropertySubscriber(L"Gap"),
			ToolBarIntOptions(6, 20));

		auto itemHeightOptions = ToolBarIntOptions(26, 30, 1);
		itemHeightOptions.Changed = [](
			ToolBar& target, const int&, const int& newValue)
		{
			target.ApplyItemHeightToAutoItems(newValue);
		};
		BindingPropertyRegistry::Register<ToolBar, int>(L"ItemHeight",
			[](ToolBar& target) { return target.ItemHeight; },
			[](ToolBar& target, const int& value) { target.ItemHeight = value; },
			ToolBarPropertySubscriber(L"ItemHeight"),
			std::move(itemHeightOptions));

		auto cornerRatioOptions = ToolBarPropertyOptions(
			0.28f, L"Appearance", 200, 30,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsRender);
		cornerRatioOptions.Coerce = [](
			ToolBar&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		cornerRatioOptions.Design.Minimum = 0.0;
		cornerRatioOptions.Design.Step = 0.05;
		BindingPropertyRegistry::Register<ToolBar, float>(L"ItemCornerRatio",
			[](ToolBar& target) { return target.ItemCornerRatio; },
			[](ToolBar& target, const float& value) { target.ItemCornerRatio = value; },
			ToolBarPropertySubscriber(L"ItemCornerRatio"),
			std::move(cornerRatioOptions));

		BindingPropertyRegistry::Register<ToolBar, D2D1_COLOR_F>(L"SeparatorColor",
			[](ToolBar& target) { return target.SeparatorColor; },
			[](ToolBar& target, const D2D1_COLOR_F& value) { target.SeparatorColor = value; },
			ToolBarPropertySubscriber(L"SeparatorColor"),
			ToolBarColorOptions(cui::theme::palette::Border, 40));
		BindingPropertyRegistry::Register<ToolBar, D2D1_COLOR_F>(L"BottomLineColor",
			[](ToolBar& target) { return target.BottomLineColor; },
			[](ToolBar& target, const D2D1_COLOR_F& value) { target.BottomLineColor = value; },
			ToolBarPropertySubscriber(L"BottomLineColor"),
			ToolBarColorOptions(cui::theme::palette::Border, 50));
		BindingPropertyRegistry::Register<ToolBar, bool>(L"ShowBottomLine",
			[](ToolBar& target) { return target.ShowBottomLine; },
			[](ToolBar& target, const bool& value) { target.ShowBottomLine = value; },
			ToolBarPropertySubscriber(L"ShowBottomLine"),
			ToolBarPropertyOptions(
				true, L"Appearance", 200, 60,
				ControlPropertyEditorKind::Boolean,
				ControlPropertyFlags::AffectsRender));
		return true;
	}();
	(void)registered;
}

void ToolBar::PerformPendingLayout()
{
	Panel::PerformPendingLayout();
	LayoutItems();
}

ToolBar::ToolBar(int x, int y, int width, int height)
{
	InitializePanelCornerRadiusDefault(8.0f);
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->BorderThickness = 1.0f;
}

#define CUI_TOOL_BAR_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(ToolBar, type, name) { return field; } \
	SET_CPP(ToolBar, type, name) \
	{ \
		(void)SetPropertyField(propertyName, field, value); \
	}

CUI_TOOL_BAR_PROPERTY_IMPL(int, HorizontalPadding, _horizontalPadding, L"HorizontalPadding")
CUI_TOOL_BAR_PROPERTY_IMPL(int, Gap, _gap, L"Gap")
CUI_TOOL_BAR_PROPERTY_IMPL(int, ItemHeight, _itemHeight, L"ItemHeight")
CUI_TOOL_BAR_PROPERTY_IMPL(float, ItemCornerRatio, _itemCornerRatio, L"ItemCornerRatio")
CUI_TOOL_BAR_PROPERTY_IMPL(D2D1_COLOR_F, SeparatorColor, _separatorColor, L"SeparatorColor")
CUI_TOOL_BAR_PROPERTY_IMPL(D2D1_COLOR_F, BottomLineColor, _bottomLineColor, L"BottomLineColor")
CUI_TOOL_BAR_PROPERTY_IMPL(bool, ShowBottomLine, _showBottomLine, L"ShowBottomLine")

#undef CUI_TOOL_BAR_PROPERTY_IMPL

void ToolBar::ApplyItemHeightToAutoItems(int value)
{
	for (const auto& [control, overrideSize] : _toolItemSizeOverrides)
	{
		if (control && overrideSize.cy == AutoItemHeight)
			control->Height = value;
	}
	if (!this->IsLayoutSuspended())
		LayoutItems();
}

SIZE ToolBar::GetToolItemLayoutSize(Control* item)
{
	if (!item) return SIZE{ 0,0 };

	SIZE size = item->Size;
	switch (item->Type())
	{
	case UIClass::UI_Label:
	case UIClass::UI_LinkLabel:
	{
		SIZE desired = item->Measure(SIZE{ this->Width, this->Height });
		if (desired.cx > 0) size.cx = desired.cx;
		if (desired.cy > 0) size.cy = desired.cy;
		break;
	}
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
	{
		const auto actual = item->GetActualSizeDip();
		if (actual.width > 0.0f) size.cx = (LONG)std::ceil(actual.width);
		if (actual.height > 0.0f) size.cy = (LONG)std::ceil(actual.height);
		break;
	}
	default:
		if (size.cx <= 0 || size.cy <= 0)
		{
			SIZE desired = item->Measure(SIZE{ this->Width, this->Height });
			if (size.cx <= 0 && desired.cx > 0) size.cx = desired.cx;
			if (size.cy <= 0 && desired.cy > 0) size.cy = desired.cy;
		}
		break;
	}

	auto overrideIt = _toolItemSizeOverrides.find(item);
	if (overrideIt != _toolItemSizeOverrides.end())
	{
		if (overrideIt->second.cx > 0) size.cx = overrideIt->second.cx;
		if (overrideIt->second.cy == AutoItemHeight) size.cy = ItemHeight;
		else if (overrideIt->second.cy > 0) size.cy = overrideIt->second.cy;
	}

	if (size.cx < 0) size.cx = 0;
	if (size.cy <= 0) size.cy = ItemHeight;
	return size;
}

Control* ToolBar::AddToolItem(Control* item, int width, int height)
{
	if (!item) return nullptr;
	Control* control = Panel::AddControl(item);
	_toolItemSizeOverrides[control] = SIZE{ width,height };
	if (width > 0) control->Width = width;
	if (height == AutoItemHeight) control->Height = ItemHeight;
	else if (height > 0) control->Height = height;
	if (!this->IsLayoutSuspended())
		LayoutItems();
	return control;
}

bool ToolBar::TryGetToolItemSizeOverride(
	Control* item, SIZE& value) const noexcept
{
	const auto found = _toolItemSizeOverrides.find(item);
	if (found == _toolItemSizeOverrides.end()) return false;
	value = found->second;
	return true;
}

void ToolBar::SetToolItemSizeOverride(Control* item, SIZE value)
{
	if (!item || item->Parent != this)
		throw std::invalid_argument(
			"只能恢复已挂载工具项的尺寸覆盖");
	_toolItemSizeOverrides[item] = value;
	if (value.cx > 0) item->Width = value.cx;
	if (value.cy == AutoItemHeight) item->Height = ItemHeight;
	else if (value.cy > 0) item->Height = value.cy;
	if (!IsLayoutSuspended()) LayoutItems();
}

void ToolBar::ClearToolItemSizeOverride(Control* item) noexcept
{
	if (item) _toolItemSizeOverrides.erase(item);
}

Button* ToolBar::AddTextButton(std::wstring text, int width)
{
	return AddToolButton(text, width);
}

std::unique_ptr<Button> ToolBar::CreateToolButton(
	std::wstring text, int width) const
{
	auto button = std::make_unique<Button>(
		text, 0, 0, width, _itemHeight);
	ApplyDefaultToolButtonStyle(button.get());
	button->Round = _itemCornerRatio;
	return button;
}

Button* ToolBar::AddToolButton(std::wstring text, int width)
{
	return AddOwned(
		CreateToolButton(std::move(text), width),
		width, AutoItemHeightOverride);
}

Button* ToolBar::AddToolButton(Button* button)
{
	if (!button) return nullptr;
	if (button->Width <= 0) button->Width = ItemHeight;
	button->Height = ItemHeight;
	return (Button*)AddToolItem(button, -1, AutoItemHeight);
}

Button* ToolBar::AddIconButton(std::shared_ptr<BitmapSource> image, int width, std::wstring text)
{
	if (width <= 0) width = ItemHeight;
	auto button = std::make_unique<Button>(text, 0, 0, width, ItemHeight);
	button->Image = image;
	button->SizeMode = ImageSizeMode::CenterImage;
	ApplyDefaultToolButtonStyle(button.get());
	button->Round = this->ItemCornerRatio;
	return AddOwned(std::move(button), width, AutoItemHeight);
}

ComboBox* ToolBar::AddToolComboBox(std::wstring text, int width)
{
	auto combo = std::make_unique<ComboBox>(text, 0, 0, width, ItemHeight);
	combo->BackColor = cui::theme::palette::Surface;
	combo->BorderColor = cui::theme::palette::Border;
	combo->ForeColor = cui::theme::palette::TextPrimary;
	combo->ButtonBackColor = cui::theme::palette::SurfaceMuted;
	combo->HeaderHoverBackColor = cui::theme::palette::AccentSoft;
	combo->AccentColor = cui::theme::palette::Accent;
	combo->SelectedItemBackColor = cui::theme::palette::AccentSelected;
	combo->UnderMouseBackColor = cui::theme::palette::AccentSoft;
	combo->UnderMouseForeColor = cui::theme::palette::TextPrimary;
	combo->ScrollBackColor = cui::theme::palette::ScrollTrack;
	combo->ScrollForeColor = cui::theme::palette::ScrollThumb;
	combo->CornerRadius = 6.0f;
	combo->DropCornerRadius = 8.0f;
	combo->DropGap = 5.0f;
	combo->BorderThickness = 1.0f;
	return AddOwned(std::move(combo), width, AutoItemHeight);
}

CheckBox* ToolBar::AddToolCheckBox(std::wstring text, int width)
{
	auto checkBox = std::make_unique<CheckBox>(text, 0, 0);
	checkBox->ForeColor = cui::theme::palette::TextPrimary;
	checkBox->UnderMouseColor = cui::theme::palette::AccentSoft;
	if (width > 0)
	{
		checkBox->Width = width;
	}
	return AddOwned(std::move(checkBox));
}

ToolBarSeparator* ToolBar::AddSeparator(int width)
{
	auto separator = std::make_unique<ToolBarSeparator>(width, ItemHeight);
	separator->LineColor = this->SeparatorColor;
	return AddOwned(std::move(separator), width, AutoItemHeight);
}

ToolBarSpacer* ToolBar::AddSpacer(int width)
{
	auto spacer = std::make_unique<ToolBarSpacer>(width);
	return AddOwned(std::move(spacer), width, 1);
}

void ToolBar::LayoutItems()
{
	const auto toolbarSize = this->GetActualSizeDip();
	float nextItemX = (float)HorizontalPadding;
	for (int itemIndex = 0; itemIndex < this->Count; itemIndex++)
	{
		auto control = this->operator[](itemIndex);
		if (!control || !control->Visible) continue;

		SIZE itemSize = GetToolItemLayoutSize(control);
		float itemY = (toolbarSize.height - (float)itemSize.cy) * 0.5f;
		if (itemY < 0.0f) itemY = 0.0f;
		control->SetRuntimeLocation(cui::core::Point{ nextItemX, itemY });
		nextItemX += itemSize.cx + Gap;
	}
}

void ToolBar::Update()
{
	if (this->ParentForm && this->Parent == nullptr)
	{
		this->ParentForm->MainToolBar = this;
	}

	if (this->IsVisual == false) return;
	if (!this->IsLayoutSuspended())
		LayoutItems();

	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float radius = (std::clamp)(this->CornerRadius, 0.0f,
		(std::min)(actualWidth, actualHeight) * 0.5f);
	this->BeginRender();
	{
		const float border = (std::max)(0.0f, this->BorderThickness);
		const D2D1_RECT_F surface = D2D1::RectF(border * 0.5f, border * 0.5f,
			(std::max)(border * 0.5f, actualWidth - border * 0.5f),
			(std::max)(border * 0.5f, actualHeight - border * 0.5f));
		d2d->FillRoundRect(surface, this->BackColor, radius);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (int itemIndex = 0; itemIndex < this->Count; itemIndex++)
			{
				auto control = this->operator[](itemIndex);
				if (!control || !control->Visible) continue;
				if (this->ParentForm && this->ParentForm->ForegroundControl == control && !control->RenderNormalWhenForeground()) continue;
				if (auto* separator = dynamic_cast<ToolBarSeparator*>(control))
					separator->LineColor = this->SeparatorColor;
				control->Update();
			}
		}
		if (this->ShowBottomLine && this->BottomLineColor.a > 0.0f && actualHeight > 0.0f)
			d2d->DrawLine(0.0f, actualHeight - 0.5f, actualWidth, actualHeight - 0.5f, this->BottomLineColor, 1.0f);
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			d2d->DrawRoundRect(surface, this->BorderColor, border, radius);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(
				0, 0, actualWidth, actualHeight,
				this->DisabledOverlayColor, radius);
		else
			d2d->FillRect(
				0, 0, actualWidth, actualHeight,
				this->DisabledOverlayColor);
	}
	this->EndRender();
}

bool ToolBar::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	if (!this->IsLayoutSuspended())
		LayoutItems();
	for (auto control : this->GetChildrenInReverseZOrder())
	{
		if (!control || !control->Visible || !control->Enable) continue;
		const auto itemLocation = control->GetActualLocationDip();
		const cui::core::Rect itemRect{
			itemLocation, control->GetActualSizeDip() };
		if (itemRect.Contains(cui::core::Point{
			(float)localX, (float)localY }))
		{
			control->ProcessMessage(
				message, wParam, lParam,
				static_cast<int>(std::floor((float)localX - itemLocation.x)),
				static_cast<int>(std::floor((float)localY - itemLocation.y)));
			break;
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
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

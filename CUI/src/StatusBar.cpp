#include "StatusBar.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<StatusBar, TValue> StatusBarPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags)
	{
		ControlPropertyOptions<StatusBar, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto StatusBarPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			StatusBar& target,
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

	ControlPropertyOptions<StatusBar, int> StatusBarIntOptions(
		int defaultValue,
		int order)
	{
		auto options = StatusBarPropertyOptions(
			defaultValue, L"Layout", 100, order,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender);
		options.Coerce = [](
			StatusBar&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		return options;
	}

	bool StatusBarColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<StatusBar, D2D1_COLOR_F> StatusBarColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = StatusBarPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color,
			ControlPropertyFlags::AffectsRender);
		options.Equals = StatusBarColorsEqual;
		return options;
	}
}

UIClass StatusBar::Type() { return UIClass::UI_StatusBar; }

void StatusBar::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto topMostOptions = StatusBarPropertyOptions(
			true, L"Behavior", 300, 10,
			ControlPropertyEditorKind::Boolean,
			ControlPropertyFlags::AffectsRender);
		topMostOptions.Changed = [](
			StatusBar& target, const bool&, const bool& newValue)
		{
			if (!target.ParentForm) return;
			if (newValue)
				target.ParentForm->MainStatusBar = &target;
			else if (target.ParentForm->MainStatusBar == &target)
				target.ParentForm->MainStatusBar = nullptr;
		};
		BindingPropertyRegistry::Register<StatusBar, bool>(L"TopMost",
			[](StatusBar& target) { return target.TopMost; },
			[](StatusBar& target, const bool& value) { target.TopMost = value; },
			StatusBarPropertySubscriber(L"TopMost"),
			std::move(topMostOptions));

		BindingPropertyRegistry::Register<StatusBar, int>(L"HorizontalPadding",
			[](StatusBar& target) { return target.HorizontalPadding; },
			[](StatusBar& target, const int& value) { target.HorizontalPadding = value; },
			StatusBarPropertySubscriber(L"HorizontalPadding"),
			StatusBarIntOptions(6, 10));
		BindingPropertyRegistry::Register<StatusBar, int>(L"Gap",
			[](StatusBar& target) { return target.Gap; },
			[](StatusBar& target, const int& value) { target.Gap = value; },
			StatusBarPropertySubscriber(L"Gap"),
			StatusBarIntOptions(10, 20));

		auto radiusOptions = StatusBarPropertyOptions(
			8.0f, L"Appearance", 200, 30,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsRender);
		radiusOptions.Coerce = [](
			StatusBar&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		radiusOptions.Design.Minimum = 0.0;
		radiusOptions.Design.Step = 0.5;
		BindingPropertyRegistry::Register<StatusBar, float>(L"PartCornerRadius",
			[](StatusBar& target) { return target.PartCornerRadius; },
			[](StatusBar& target, const float& value) { target.PartCornerRadius = value; },
			StatusBarPropertySubscriber(L"PartCornerRadius"),
			std::move(radiusOptions));

#define CUI_REGISTER_STATUS_COLOR(name, defaultValue, order) \
		BindingPropertyRegistry::Register<StatusBar, D2D1_COLOR_F>(CUI_BINDING_WIDEN(#name), \
			[](StatusBar& target) { return target.name; }, \
			[](StatusBar& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			StatusBarPropertySubscriber(CUI_BINDING_WIDEN(#name)), \
			StatusBarColorOptions(defaultValue, order))

		CUI_REGISTER_STATUS_COLOR(SeparatorColor,
			(D2D1_COLOR_F{ 1, 1, 1, 0.12f }), 40);
		CUI_REGISTER_STATUS_COLOR(TopLineColor,
			(D2D1_COLOR_F{ 1, 1, 1, 0.12f }), 50);
		CUI_REGISTER_STATUS_COLOR(PartBackColor,
			(D2D1_COLOR_F{ 1, 1, 1, 0.06f }), 60);
		CUI_REGISTER_STATUS_COLOR(PartBorderColor,
			(D2D1_COLOR_F{ 1, 1, 1, 0.10f }), 70);

#undef CUI_REGISTER_STATUS_COLOR

#define CUI_REGISTER_STATUS_BOOL(name, defaultValue, order) \
		BindingPropertyRegistry::Register<StatusBar, bool>(CUI_BINDING_WIDEN(#name), \
			[](StatusBar& target) { return target.name; }, \
			[](StatusBar& target, const bool& value) { target.name = value; }, \
			StatusBarPropertySubscriber(CUI_BINDING_WIDEN(#name)), \
			StatusBarPropertyOptions(defaultValue, L"Appearance", 200, order, \
				ControlPropertyEditorKind::Boolean, ControlPropertyFlags::AffectsRender))

		CUI_REGISTER_STATUS_BOOL(ShowTopLine, true, 80);
		CUI_REGISTER_STATUS_BOOL(ShowBorder, false, 90);
		CUI_REGISTER_STATUS_BOOL(UsePartPills, false, 100);

#undef CUI_REGISTER_STATUS_BOOL

		return true;
	}();
	(void)registered;
}

void StatusBar::PerformPendingLayout()
{
	Panel::PerformPendingLayout();
	LayoutItems();
}

void StatusBar::UpdateCompatPointers()
{
	_leftLabel = nullptr;
	_rightLabel = nullptr;
	if (_parts.empty()) return;
	_leftLabel = _parts.front().LabelCtrl;
	_rightLabel = _parts.back().LabelCtrl;
}

void StatusBar::EnsureDefaultParts()
{
	if (!_parts.empty()) return;
	AddPart(L"", -1);
	AddPart(L"", 0);
}

StatusBar::StatusBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };

	this->BackColor = D2D1_COLOR_F{ 1, 1, 1, 0.08f };
	this->BorderColor = D2D1_COLOR_F{ 1, 1, 1, 0.12f };
	this->BorderThickness = 1.0f;
	this->ForeColor = Colors::WhiteSmoke;
}

#define CUI_STATUS_BAR_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(StatusBar, type, name) { return field; } \
	SET_CPP(StatusBar, type, name) \
	{ \
		(void)SetPropertyField(propertyName, field, value); \
	}

CUI_STATUS_BAR_PROPERTY_IMPL(bool, TopMost, _topMost, L"TopMost")
CUI_STATUS_BAR_PROPERTY_IMPL(int, HorizontalPadding, _horizontalPadding, L"HorizontalPadding")
CUI_STATUS_BAR_PROPERTY_IMPL(int, Gap, _gap, L"Gap")
CUI_STATUS_BAR_PROPERTY_IMPL(float, PartCornerRadius, _partCornerRadius, L"PartCornerRadius")
CUI_STATUS_BAR_PROPERTY_IMPL(D2D1_COLOR_F, SeparatorColor, _separatorColor, L"SeparatorColor")
CUI_STATUS_BAR_PROPERTY_IMPL(D2D1_COLOR_F, TopLineColor, _topLineColor, L"TopLineColor")
CUI_STATUS_BAR_PROPERTY_IMPL(D2D1_COLOR_F, PartBackColor, _partBackColor, L"PartBackColor")
CUI_STATUS_BAR_PROPERTY_IMPL(D2D1_COLOR_F, PartBorderColor, _partBorderColor, L"PartBorderColor")
CUI_STATUS_BAR_PROPERTY_IMPL(bool, ShowTopLine, _showTopLine, L"ShowTopLine")
CUI_STATUS_BAR_PROPERTY_IMPL(bool, ShowBorder, _showBorder, L"ShowBorder")
CUI_STATUS_BAR_PROPERTY_IMPL(bool, UsePartPills, _usePartPills, L"UsePartPills")

#undef CUI_STATUS_BAR_PROPERTY_IMPL

int StatusBar::AddPart(const std::wstring& text, int width)
{
	auto label = this->AddControl(new Label(text, 0, 0));
	label->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	label->ForeColor = this->ForeColor;
	_parts.push_back(Part{ label, width });
	UpdateCompatPointers();
	return (int)_parts.size() - 1;
}

void StatusBar::ClearParts()
{
	for (auto& p : _parts)
	{
		if (p.LabelCtrl)
		{
			this->DeleteControl(p.LabelCtrl);
			p.LabelCtrl = nullptr;
		}
	}
	_parts.clear();
	_separatorsX.clear();
	_partRects.clear();
	UpdateCompatPointers();
}

int StatusBar::PartCount() const
{
	return (int)_parts.size();
}

void StatusBar::SetPartText(int index, const std::wstring& text)
{
	if (index < 0 || index >= (int)_parts.size()) return;
	if (_parts[index].LabelCtrl) _parts[index].LabelCtrl->Text = text;
}

std::wstring StatusBar::GetPartText(int index) const
{
	if (index < 0 || index >= (int)_parts.size()) return L"";
	auto lbl = _parts[index].LabelCtrl;
	return lbl ? lbl->Text : L"";
}

int StatusBar::GetPartWidth(int index) const
{
	if (index < 0 || index >= (int)_parts.size()) return 0;
	return _parts[index].Width;
}

void StatusBar::SetPartWidth(int index, int width)
{
	if (index < 0 || index >= (int)_parts.size()) return;
	if (_parts[index].Width == width) return;
	_parts[index].Width = width;
	InvalidateLayout();
	InvalidateVisual();
}

void StatusBar::SetLeftText(const std::wstring& text)
{
	EnsureDefaultParts();
	UpdateCompatPointers();
	if (_leftLabel) _leftLabel->Text = text;
}

void StatusBar::SetRightText(const std::wstring& text)
{
	EnsureDefaultParts();
	UpdateCompatPointers();
	if (_rightLabel) _rightLabel->Text = text;
}

std::wstring StatusBar::GetLeftText() const
{
	return _leftLabel ? _leftLabel->Text : L"";
}

std::wstring StatusBar::GetRightText() const
{
	return _rightLabel ? _rightLabel->Text : L"";
}

void StatusBar::LayoutItems()
{
	if (!this->ParentForm) return;
	UpdateCompatPointers();
	if (_parts.empty())
	{
		_separatorsX.clear();
		_partRects.clear();
		return;
	}

	_separatorsX.clear();
	_partRects.assign(_parts.size(), D2D1::RectF(0, 0, 0, 0));

	const int gapTotal = (std::max)(0, (int)_parts.size() - 1) * Gap;
	const int contentWidth = (std::max)(
		0, this->Width - HorizontalPadding * 2 - gapTotal);
	std::vector<int> springIndices;
	int fixedSum = 0;
	std::vector<int> computedWidths;
	computedWidths.reserve(_parts.size());

	for (int i = 0; i < (int)_parts.size(); i++)
	{
		const auto& part = _parts[i];
		int w = part.Width;
		if (w < 0)
		{
			springIndices.push_back(i);
			computedWidths.push_back(0);
			continue;
		}
		if (!part.LabelCtrl)
		{
			computedWidths.push_back(0);
			continue;
		}

		if (w == 0)
		{
			const auto ts = part.LabelCtrl->GetActualSizeDip();
			w = (int)std::ceil(ts.width) + _partInnerPadding * 2;
		}
		computedWidths.push_back((std::max)(0, w));
		fixedSum += (std::max)(0, w);
	}

	if (springIndices.empty() && !_parts.empty())
	{
		springIndices.push_back(0);
	}

	int remaining = contentWidth - fixedSum;
	if (remaining < 0) remaining = 0;
	if (!springIndices.empty())
	{
		int each = remaining / (int)springIndices.size();
		int extra = remaining - each * (int)springIndices.size();
		for (size_t si = 0; si < springIndices.size(); si++)
		{
			int partIndex = springIndices[si];
			computedWidths[partIndex] = each + ((si == springIndices.size() - 1) ? extra : 0);
		}
	}

	int x = HorizontalPadding;
	const float partTop = 4.0f;
	const float partBottom = (std::max)(partTop, (float)this->Height - 3.0f);
	for (int i = 0; i < (int)_parts.size(); i++)
	{
		auto& part = _parts[i];
		if (!part.LabelCtrl) continue;
		int w = computedWidths[i];
		if (w < 0) w = 0;

		const auto ts = part.LabelCtrl->GetActualSizeDip();
		auto textSize = part.LabelCtrl->Font->GetTextSize(part.LabelCtrl->Text);
		float y = ((float)this->Height - ts.height) * 0.5f;
		if (y < 0.0f) y = 0.0f;

		float textX = (float)(x + _partInnerPadding);
		if (part.Width >= 0 && w > (int)textSize.width + _partInnerPadding * 2)
			textX = (float)x + ((float)w - textSize.width) * 0.5f;
		part.LabelCtrl->SetRuntimeLocation(cui::core::Point{ textX, y });
		part.LabelCtrl->ForeColor = this->ForeColor;
		if (i >= 0 && i < (int)_partRects.size())
			_partRects[i] = D2D1::RectF((float)x, partTop, (float)(x + w), partBottom);

		x += w;
		if (i != (int)_parts.size() - 1)
		{
			_separatorsX.push_back((float)x);
			x += Gap;
		}
	}
}

void StatusBar::Update()
{
	if (this->ParentForm && this->TopMost)
	{
		this->ParentForm->MainStatusBar = this;
	}

	if (this->IsVisual == false || !this->ParentForm) return;

	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformLayout();
	}

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
		if (radius > 0.0f)
			d2d->FillRoundRect(surface, this->BackColor, radius);
		else
			d2d->FillRect(surface, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		for (int i = 0; i < (int)_parts.size() && i < (int)_partRects.size(); i++)
		{
			if (!this->UsePartPills || _parts[i].Width < 0) continue;
			const auto& rect = _partRects[i];
			const float rectW = rect.right - rect.left;
			const float rectH = rect.bottom - rect.top;
			if (rectW <= 0.0f || rectH <= 0.0f) continue;
			if (this->PartBackColor.a > 0.0f)
				d2d->FillRoundRect(rect, this->PartBackColor, this->PartCornerRadius);
			if (this->PartBorderColor.a > 0.0f)
				d2d->DrawRoundRect(rect, this->PartBorderColor, 1.0f, this->PartCornerRadius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (int i = 0; i < this->Count; i++)
			{
				auto c = this->operator[](i);
				if (!c) continue;
				c->Update();
			}
		}
		for (float sx : _separatorsX)
		{
			float x = sx + (float)(Gap / 2);
			float y = 8.0f;
			float h = (std::max)(1.0f, actualHeight - y * 2.0f);
			if (this->SeparatorColor.a > 0.0f)
				d2d->FillRoundRect(x, y, 1.0f, h, this->SeparatorColor, 0.5f);
		}
		if (this->ShowTopLine && this->TopLineColor.a > 0.0f)
		{
			d2d->DrawLine(0.0f, 0.5f, actualWidth, 0.5f, this->TopLineColor, 1.0f);
		}
		if (this->ShowBorder && border > 0.0f && this->BorderColor.a > 0.0f)
		{
			if (radius > 0.0f)
				d2d->DrawRoundRect(surface, this->BorderColor, border, radius);
			else
				d2d->DrawRect(surface, this->BorderColor, border);
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

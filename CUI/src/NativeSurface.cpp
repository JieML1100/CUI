#include "../include/NativeSurface.h"

#include "../include/Window.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<NativeSurface, TValue> NativeSurfaceOptions(
		TValue defaultValue,
		std::wstring category,
		int order)
	{
		DependencyPropertyOptions<NativeSurface, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Design.Category = std::move(category);
		options.Design.CategoryOrder = 300;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto NativeSurfaceSubscriber(const wchar_t* propertyName)
	{
		return [name = std::wstring(propertyName)](
			NativeSurface& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[name, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == name)
						handler();
				});
		};
	}
}

NativeSurface::NativeSurface()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0.08f, 0.10f, 0.14f, 1.0f };
	this->RendererBorderColor = D2D1_COLOR_F{ 0.28f, 0.48f, 0.78f, 1.0f };
}

NativeSurface::~NativeSurface()
{
	DetachBehavior();
}

void NativeSurface::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyRegistry::Register<NativeSurface, std::wstring>(
			L"BehaviorKey",
			[](NativeSurface& target) { return target.GetBehaviorKey(); },
			[](NativeSurface& target, const std::wstring& value)
			{ target.SetBehaviorKey(value); },
			NativeSurfaceSubscriber(L"BehaviorKey"),
			NativeSurfaceOptions(std::wstring{}, L"Behavior", 10));
		DependencyPropertyRegistry::Register<NativeSurface, std::wstring>(
			L"PlaceholderText",
			[](NativeSurface& target) { return target.GetPlaceholderText(); },
			[](NativeSurface& target, const std::wstring& value)
			{ target.SetPlaceholderText(value); },
			NativeSurfaceSubscriber(L"PlaceholderText"),
			NativeSurfaceOptions(
				std::wstring(L"NativeSurface"), L"Appearance", 20));
		return true;
	}();
	(void)registered;
}

void NativeSurface::SetBehaviorKey(std::wstring value)
{
	if (_behaviorKey == value) return;
	DetachBehavior();
	SetPropertyField(L"BehaviorKey", _behaviorKey, std::move(value));
}

void NativeSurface::SetPlaceholderText(std::wstring value)
{
	SetPropertyField(L"PlaceholderText", _placeholderText, std::move(value));
}

void NativeSurface::SetBehavior(
	std::unique_ptr<INativeSurfaceBehavior> behavior)
{
	if (_behavior.get() == behavior.get())
	{
		(void)behavior.release();
		return;
	}
	DetachBehavior();
	_behavior = std::move(behavior);
	if (_behavior)
	{
		try
		{
			_behavior->Attach(*this);
			_lastDpiScale = GetPresentationWindow() ? GetPresentationWindow()->GetDpiScale() : 1.0f;
			_behavior->DpiChanged(*this, _lastDpiScale);
		}
		catch (...)
		{
			auto failed = std::move(_behavior);
			try { failed->Detach(*this); } catch (...) {}
			throw;
		}
	}
	RequestLayout();
	InvalidateVisual();
}

void NativeSurface::InvalidateRegion(const D2D1_RECT_F& localRect)
{
	const auto size = GetActualSizeDip();
	const float left = std::clamp(localRect.left, 0.0f, size.width);
	const float top = std::clamp(localRect.top, 0.0f, size.height);
	const float right = std::clamp(localRect.right, left, size.width);
	const float bottom = std::clamp(localRect.bottom, top, size.height);
	if (right <= left || bottom <= top) return;
	const auto origin = GetAbsoluteLocationDip();
	InvalidateVisualRect(D2D1_RECT_F{
		origin.x + left,
		origin.y + top,
		origin.x + right,
		origin.y + bottom });
}

void NativeSurface::DetachBehavior() noexcept
{
	if (!_behavior) return;
	auto behavior = std::move(_behavior);
	try { behavior->Detach(*this); } catch (...) {}
}

cui::core::Size NativeSurface::MeasureCore(
	const cui::core::Constraints& available)
{
	if (_behavior)
	{
		if (const auto desired = _behavior->Measure(*this, available))
			return desired->NonNegative();
	}
	return Control::MeasureCore(available);
}

void NativeSurface::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	auto& graphics = *GetDrawingContext();
	const auto size = GetActualSizeDip();
	const float dpiScale = GetPresentationWindow()->GetDpiScale();
	if (_behavior && std::fabs(dpiScale - _lastDpiScale) > 0.0001f)
		NotifyDpiChanged(dpiScale);

	BeginRender();
	if (_behavior)
	{
		NativeSurfaceRenderContext context{ graphics, size, dpiScale };
		_behavior->Render(*this, context);
	}
	else
	{
		graphics.FillRect(0.0f, 0.0f, size.width, size.height, RendererBackgroundColor);
		graphics.DrawRect(0.5f, 0.5f,
			(std::max)(0.0f, size.width - 1.0f),
			(std::max)(0.0f, size.height - 1.0f), RendererBorderColor, 1.0f);
		if (!_placeholderText.empty() && GetRenderFont())
		{
			const auto textSize = GetRenderFont()->GetTextSize(_placeholderText);
			graphics.DrawString(_placeholderText,
				(std::max)(0.0f, (size.width - textSize.width) * 0.5f),
				(std::max)(0.0f, (size.height - textSize.height) * 0.5f),
				RendererForegroundColor, GetRenderFont());
		}
	}
	EndRender();
}

bool NativeSurface::TryCreateInput(
	const InputReport& input,
	NativeSurfaceInputEvent& output) const
{
	output.X = static_cast<float>(input.X);
	output.Y = static_cast<float>(input.Y);
	output.ButtonStates = input.ButtonStates;
	output.Modifiers = input.Modifiers;
	output.Key = input.SystemKey == Key::None ? input.Key : Key::System;
	output.SystemKey = input.SystemKey;
	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
		output.Kind = NativeSurfaceInputKind::PointerMove; return true;
	case InputReportKind::PointerDown:
		output.Kind = NativeSurfaceInputKind::PointerDown;
		output.ChangedButton = input.ChangedButton; return true;
	case InputReportKind::PointerUp:
		output.Kind = NativeSurfaceInputKind::PointerUp;
		output.ChangedButton = input.ChangedButton; return true;
	case InputReportKind::PointerDoubleClick:
		output.Kind = NativeSurfaceInputKind::PointerDoubleClick;
		output.ChangedButton = input.ChangedButton; return true;
	case InputReportKind::MouseWheel:
	case InputReportKind::HorizontalMouseWheel:
		output.Kind = NativeSurfaceInputKind::PointerWheel;
		output.WheelDelta = input.WheelDelta; return true;
	case InputReportKind::KeyDown:
		output.Kind = NativeSurfaceInputKind::KeyDown; return true;
	case InputReportKind::KeyUp:
		output.Kind = NativeSurfaceInputKind::KeyUp; return true;
	case InputReportKind::FocusGained:
		output.Kind = NativeSurfaceInputKind::FocusGained; return true;
	case InputReportKind::FocusLost:
		output.Kind = NativeSurfaceInputKind::FocusLost; return true;
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		output.Kind = NativeSurfaceInputKind::Cancel; return true;
	default:
		return false;
	}
}

bool NativeSurface::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (!_behavior || input.Text.empty()) return false;
	NativeSurfaceInputEvent event;
	event.Kind = NativeSurfaceInputKind::TextInput;
	event.Text = input.Text;
	event.Modifiers = input.Modifiers;
	return _behavior->HandleInput(*this, event);
}

bool NativeSurface::ProcessInput(const InputReport& input)
{
	if (!IsEnabled || !IsVisible) return true;
	if (_behavior)
	{
		NativeSurfaceInputEvent event;
		if (TryCreateInput(input, event)
			&& _behavior->HandleInput(*this, event)) return true;
	}
	return Control::ProcessInput(input);
}

void NativeSurface::NotifyDpiChanged(float dpiScale)
{
	if (!std::isfinite(dpiScale) || dpiScale <= 0.0f) dpiScale = 1.0f;
	_lastDpiScale = dpiScale;
	if (_behavior) _behavior->DpiChanged(*this, dpiScale);
	Control::NotifyDpiChanged(dpiScale);
}

void NativeSurface::NotifyDeviceResourcesInvalidated() noexcept
{
	if (_behavior)
		try { _behavior->DeviceResourcesInvalidated(*this); } catch (...) {}
	Control::NotifyDeviceResourcesInvalidated();
}

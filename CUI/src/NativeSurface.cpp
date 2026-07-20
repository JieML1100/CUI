#include "../include/NativeSurface.h"

#include "../include/Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<NativeSurface, TValue> NativeSurfaceOptions(
		TValue defaultValue,
		std::wstring category,
		int order)
	{
		ControlPropertyOptions<NativeSurface, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = std::move(category);
		options.Design.CategoryOrder = 300;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Text;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto NativeSurfaceSubscriber(const wchar_t* propertyName)
	{
		return [name = std::wstring(propertyName)](
			NativeSurface& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[name, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), name.c_str()) == 0)
						handler();
				});
		};
	}
}

NativeSurface::NativeSurface()
{
	this->Size = SIZE{ 320, 180 };
	this->BackColor = D2D1_COLOR_F{ 0.08f, 0.10f, 0.14f, 1.0f };
	this->BorderColor = D2D1_COLOR_F{ 0.28f, 0.48f, 0.78f, 1.0f };
}

NativeSurface::NativeSurface(int x, int y, int width, int height)
	: NativeSurface()
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

NativeSurface::~NativeSurface()
{
	DetachBehavior();
}

void NativeSurface::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<NativeSurface, std::wstring>(
			L"BehaviorKey",
			[](NativeSurface& target) { return target.GetBehaviorKey(); },
			[](NativeSurface& target, const std::wstring& value)
			{ target.SetBehaviorKey(value); },
			NativeSurfaceSubscriber(L"BehaviorKey"),
			NativeSurfaceOptions(std::wstring{}, L"Behavior", 10));
		BindingPropertyRegistry::Register<NativeSurface, std::wstring>(
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
			_lastDpiScale = ParentForm ? ParentForm->GetDpiScale() : 1.0f;
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

void NativeSurface::Update()
{
	if (!IsVisual || !ParentForm || !ParentForm->Render) return;
	auto& graphics = *ParentForm->Render;
	const auto size = GetActualSizeDip();
	const float dpiScale = ParentForm->GetDpiScale();
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
		graphics.FillRect(0.0f, 0.0f, size.width, size.height, BackColor);
		graphics.DrawRect(0.5f, 0.5f,
			(std::max)(0.0f, size.width - 1.0f),
			(std::max)(0.0f, size.height - 1.0f), BorderColor, 1.0f);
		if (!_placeholderText.empty() && Font)
		{
			const auto textSize = Font->GetTextSize(_placeholderText);
			graphics.DrawString(_placeholderText,
				(std::max)(0.0f, (size.width - textSize.width) * 0.5f),
				(std::max)(0.0f, (size.height - textSize.height) * 0.5f),
				ForeColor, Font);
		}
	}
	EndRender();
}

bool NativeSurface::TryCreateInput(
	UINT message,
	WPARAM wParam,
	int localX,
	int localY,
	NativeSurfaceInputEvent& output) const
{
	output.X = static_cast<float>(localX);
	output.Y = static_cast<float>(localY);
	output.Alt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	output.Control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	output.Shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	switch (message)
	{
	case WM_MOUSEMOVE:
		output.Kind = NativeSurfaceInputKind::PointerMove; return true;
	case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
		output.Kind = NativeSurfaceInputKind::PointerDown;
		output.Button = FromParamToMouseButtons(message); return true;
	case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
		output.Kind = NativeSurfaceInputKind::PointerUp;
		output.Button = FromParamToMouseButtons(message); return true;
	case WM_LBUTTONDBLCLK:
		output.Kind = NativeSurfaceInputKind::PointerDoubleClick;
		output.Button = MouseButtons::Left; return true;
	case WM_MOUSEWHEEL:
		output.Kind = NativeSurfaceInputKind::PointerWheel;
		output.WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam); return true;
	case WM_KEYDOWN:
		output.Kind = NativeSurfaceInputKind::KeyDown;
		output.Key = static_cast<Keys>(wParam); return true;
	case WM_KEYUP:
		output.Kind = NativeSurfaceInputKind::KeyUp;
		output.Key = static_cast<Keys>(wParam); return true;
	case WM_CHAR:
		output.Kind = NativeSurfaceInputKind::Character;
		output.Character = static_cast<wchar_t>(wParam); return true;
	case WM_SETFOCUS:
		output.Kind = NativeSurfaceInputKind::FocusGained; return true;
	case WM_KILLFOCUS:
		output.Kind = NativeSurfaceInputKind::FocusLost; return true;
	case WM_CANCELMODE: case WM_CAPTURECHANGED:
		output.Kind = NativeSurfaceInputKind::Cancel; return true;
	default:
		return false;
	}
}

bool NativeSurface::ProcessMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	int localX,
	int localY)
{
	if (!Enable || !Visible) return true;
	if (_behavior)
	{
		NativeSurfaceInputEvent event;
		if (TryCreateInput(message, wParam, localX, localY, event)
			&& _behavior->HandleInput(*this, event)) return true;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
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

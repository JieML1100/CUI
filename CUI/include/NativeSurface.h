#pragma once

#include "Control.h"

#include <memory>
#include <optional>
#include <string>

class NativeSurface;

enum class NativeSurfaceInputKind : unsigned char
{
	PointerMove,
	PointerDown,
	PointerUp,
	PointerDoubleClick,
	PointerWheel,
	KeyDown,
	KeyUp,
	TextInput,
	FocusGained,
	FocusLost,
	Cancel,
};

/** Normalized, local-DIP input delivered to a NativeSurface behavior. */
struct NativeSurfaceInputEvent
{
	NativeSurfaceInputKind Kind = NativeSurfaceInputKind::PointerMove;
	float X = 0.0f;
	float Y = 0.0f;
	MouseButton ChangedButton = MouseButton::None;
	MouseButtonStates ButtonStates;
	int WheelDelta = 0;
	::Key Key = ::Key::None;
	::Key SystemKey = ::Key::None;
	ModifierKeys Modifiers = ModifierKeys::None;
	std::wstring Text;
};

struct NativeSurfaceRenderContext
{
	D2DGraphics& Graphics;
	cui::core::Size Bounds;
	float DpiScale = 1.0f;
};

/**
 * Application-owned high-performance behavior hosted by a declarative
 * NativeSurface. It never owns the host and must release host/device state in
 * Detach. All callbacks run on the UI thread that owns the control tree.
 */
class INativeSurfaceBehavior
{
public:
	virtual ~INativeSurfaceBehavior() = default;
	virtual void Attach(NativeSurface& host) { (void)host; }
	virtual void Detach(NativeSurface& host) noexcept { (void)host; }
	virtual std::optional<cui::core::Size> Measure(
		NativeSurface& host,
		const cui::core::Constraints& available)
	{
		(void)host;
		(void)available;
		return std::nullopt;
	}
	virtual void Render(
		NativeSurface& host,
		NativeSurfaceRenderContext& context) = 0;
	virtual bool HandleInput(
		NativeSurface& host,
		NativeSurfaceInputEvent& event)
	{
		(void)host;
		(void)event;
		return false;
	}
	virtual void DpiChanged(NativeSurface& host, float dpiScale)
	{
		(void)host;
		(void)dpiScale;
	}
	virtual void DeviceResourcesInvalidated(NativeSurface& host) noexcept
	{
		(void)host;
	}
};

class NativeSurface final : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"NativeSurface");
	}

public:
	NativeSurface();
	~NativeSurface() override;

	UIClass Type() override { return UIClass::UI_NativeSurface; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
protected:
	void OnRender() override;
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool ProcessInput(const InputReport& input) override;

public:
	const std::wstring& GetBehaviorKey() const noexcept { return _behaviorKey; }
	void SetBehaviorKey(std::wstring value);
	const std::wstring& GetPlaceholderText() const noexcept
	{
		return _placeholderText;
	}
	void SetPlaceholderText(std::wstring value);

	void SetBehavior(std::unique_ptr<INativeSurfaceBehavior> behavior);
	/** Invalidates a local-DIP subregion without promoting it to a full surface. */
	void InvalidateRegion(const D2D1_RECT_F& localRect);
	INativeSurfaceBehavior* Behavior() noexcept { return _behavior.get(); }
	const INativeSurfaceBehavior* Behavior() const noexcept
	{
		return _behavior.get();
	}
	bool HasBehavior() const noexcept { return static_cast<bool>(_behavior); }

protected:
	void NotifyDpiChanged(float dpiScale) override;
	void NotifyDeviceResourcesInvalidated() noexcept override;
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;

private:
	std::wstring _behaviorKey;
	std::wstring _placeholderText = L"NativeSurface";
	std::unique_ptr<INativeSurfaceBehavior> _behavior;
	float _lastDpiScale = 1.0f;

	bool TryCreateInput(
		const InputReport& input,
		NativeSurfaceInputEvent& output) const;
	void DetachBehavior() noexcept;
};

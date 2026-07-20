#pragma once

#include <Windows.h>

#include <string>

class Control;
class D2DGraphics;

/**
 * Stable runtime identity supplied while an application behavior is attached
 * to one XAML-declared component instance. The context is valid only for the
 * duration of Attach; behaviors should retain values or Control pointers they
 * need after returning.
 */
struct DeclarativeComponentBehaviorContext final
{
	Control& Host;
	int StableId = 0;
	std::wstring InstanceName;
	std::wstring XamlNamespace;
	std::wstring XamlTypeName;
};

/**
 * Application code attached to a component whose type, properties, events and
 * visual template remain owned by XAML.
 *
 * This is deliberately not a control factory. A behavior may observe normal
 * control events, update declared read-only state, query named template parts,
 * preprocess input delivered to the component host, and draw a final overlay.
 * Layout and the public property/event contract remain declarative; use
 * NativeSurface for a fully application-rendered high-performance surface.
 */
class IDeclarativeComponentBehavior
{
public:
	virtual ~IDeclarativeComponentBehavior() = default;

	/** Called after the complete template/content tree and styles are installed. */
	virtual bool Attach(
		Control& host,
		const DeclarativeComponentBehaviorContext& context,
		std::wstring* outError)
	{
		(void)host;
		(void)context;
		if (outError) outError->clear();
		return true;
	}

	/** Called exactly once for every behavior whose Attach was attempted. */
	virtual void Detach(Control& host) noexcept { (void)host; }

	/**
	 * Runs before the host's normal ProcessMessage implementation. Returning
	 * true consumes the message. Coordinates are host-local DIPs.
	 */
	virtual bool HandleMessage(
		Control& host,
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		int localX,
		int localY)
	{
		(void)host;
		(void)message;
		(void)wParam;
		(void)lParam;
		(void)localX;
		(void)localY;
		return false;
	}

	/** Draws inside the host's local clip after its normal visual subtree. */
	virtual void RenderOverlay(Control& host, D2DGraphics& graphics)
	{
		(void)host;
		(void)graphics;
	}

	virtual void DpiChanged(Control& host, float dpiScale)
	{
		(void)host;
		(void)dpiScale;
	}

	virtual void DeviceResourcesInvalidated(Control& host) noexcept
	{
		(void)host;
	}
};

#ifndef CUI_COMPONENT_BEHAVIOR_H_INCLUDED
#define CUI_COMPONENT_BEHAVIOR_H_INCLUDED
#pragma once

#include "CuiBuildFeatures.h"
#if !CUI_ENABLE_DYNAMIC_XAML
#error ComponentBehavior is available only in the CUI design-runtime variant
#endif

#include "Binding.h"
#include "InputReport.h"
#include "RuntimeTypeMetadata.h"

#include <d2d1.h>

#include <string>

class Control;
class D2DGraphics;

/**
 * Authored XAML identity supplied while an application behavior is attached
 * to one declared component instance. The context is valid only for the
 * duration of Attach; behaviors should retain values or Control pointers they
 * need after returning.
 */
struct DeclarativeComponentBehaviorContext final
{
	Control& Host;
	std::wstring InstanceName;
	RuntimeTypeId Type;
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
	 * Runs before the host's normal input behavior. Returning true consumes the
	 * normalized report. Coordinates are host-local DIPs.
	 */
	virtual bool HandleInput(
		Control& host,
		const InputReport& input)
	{
		(void)host;
		(void)input;
		return false;
	}

	/**
	 * Runs between PreviewTextInput and TextInput after native text has been
	 * normalized. Returning true means the behavior applied the committed text.
	 */
	virtual bool HandleTextInput(
		Control& host,
		TextCompositionEventArgs& input)
	{
		(void)host;
		(void)input;
		return false;
	}

	/** Supplies a top-level client-DIP caret rectangle for IME UI placement. */
	virtual bool TryGetTextInputCaretRect(
		Control& host,
		D2D1_RECT_F& outRect)
	{
		(void)host;
		(void)outRect;
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

protected:
	/**
	 * Publishes state through a read-only property declared by the host's XAML
	 * component contract. The operation is accepted only while this exact
	 * behavior instance is attached to the supplied host.
	 */
	bool SetReadOnlyProperty(
		Control& host,
		const std::wstring& propertyName,
		const BindingValue& value);
	bool ClearReadOnlyProperty(
		Control& host,
		const std::wstring& propertyName);
};

#endif // CUI_COMPONENT_BEHAVIOR_H_INCLUDED

#pragma once

#include "ControlWeakReference.h"

#include <cstdint>
#include <vector>

class Control;
class Window;

namespace cui::framework
{
	/**
	 * Typed framework states which propagate from one input origin through both
	 * the visual and logical parent graphs. Routed events deliberately continue
	 * to use their single routed-parent path.
	 */
	enum class ReverseInheritedPropertyKind : unsigned char
	{
		KeyboardFocusWithin,
		MouseOver,
		MouseCaptureWithin,
	};

	/**
	 * Maintains one reverse-inherited input property without property-name
	 * lookup. A transition commits every affected CLR/DP value before publishing
	 * the old-only and new-only notifications, matching WPF's cache-first model.
	 */
	class ReverseInheritedProperty final
	{
	public:
		ReverseInheritedProperty(
			Window* owner,
			ReverseInheritedPropertyKind kind) noexcept;
		ReverseInheritedProperty(const ReverseInheritedProperty&) = delete;
		ReverseInheritedProperty& operator=(
			const ReverseInheritedProperty&) = delete;

		Control* Origin() const noexcept { return _origin.Get(); }
		void SetOrigin(Control* origin, bool raiseInputEvents = true);
		void Refresh(bool raiseInputEvents = true);
		void Reset(bool raiseInputEvents = true);

	private:
		struct NotificationBatch;

		Window* _window = nullptr;
		ReverseInheritedPropertyKind _kind =
			ReverseInheritedPropertyKind::MouseCaptureWithin;
		ControlWeakReference _origin;
		std::vector<ControlWeakReference> _published;
		NotificationBatch* _activeBatch = nullptr;

		std::vector<ControlWeakReference> BuildClosure() const;
		void Reconcile(bool raiseInputEvents);
	};
}

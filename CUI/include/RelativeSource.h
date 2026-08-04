#pragma once

#include "Binding.h"
#include "Control.h"

namespace cui::binding
{
	/**
	 * Resolves the requested routed ancestor using native UIClass assignability.
	 *
	 * The routed parent follows the framework's visual, logical, then templated
	 * parent precedence. AncestorLevel is one-based among matching ancestors.
	 */
	Control* FindAncestor(
		Control& target,
		UIClass ancestorType,
		int ancestorLevel = 1) noexcept;

	/**
	 * Resolves an ancestor whose compiled component type is exactly type.
	 *
	 * Unlike the UIClass overload, this overload does not perform inheritance
	 * matching because ComponentTypeToken identifies one component type.
	 */
	Control* FindAncestor(
		Control& target,
		ComponentTypeToken ancestorType,
		int ancestorLevel = 1) noexcept;
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design compatibility overload that lowers the QName to a token. */
	Control* FindAncestor(
		Control& target,
		const RuntimeTypeId& ancestorType,
		int ancestorLevel = 1) noexcept;
#endif

	/**
	 * Creates a stable binding source that follows matching routed ancestors.
	 *
	 * The source observes visual, logical, and templated parent changes along
	 * the current route and forwards property/validation changes from whichever
	 * ancestor currently matches.
	 */
	BindingSourceReference CreateFindAncestorSource(
		Control& target,
		UIClass ancestorType,
		int ancestorLevel = 1);

	/** Exact ComponentTypeToken counterpart of CreateFindAncestorSource. */
	BindingSourceReference CreateFindAncestorSource(
		Control& target,
		ComponentTypeToken ancestorType,
		int ancestorLevel = 1);

	/**
	 * Resolves an exact DependencyProperty endpoint through a stable
	 * FindAncestor source. The resolver never falls back to property-token
	 * dispatch when the supplied source is not a FindAncestor provider.
	 */
	[[nodiscard]] CompiledSourceHandle
		ResolveCompiledFindAncestorDependencyPropertySource(
			IBindingSource& source,
			const DependencyProperty& property) noexcept;
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design compatibility overload that lowers the QName to a token. */
	BindingSourceReference CreateFindAncestorSource(
		Control& target,
		RuntimeTypeId ancestorType,
		int ancestorLevel = 1);
#endif
}

#pragma once

#include "DesignerPreviewPlugin.h"
#include "DesignerTypes.h"

#include <memory>
#include <string>
#include <vector>

/** Host-owned bridge from value-only preview plugins to normal CUI controls. */
namespace DesignerPreviewBridge
{
	using Module = std::shared_ptr<DesignerPreviewPluginModule>;

	/**
	 * Attaches one plugin session as a final local-coordinate render decorator.
	 * The Control remains entirely owned and destroyed by the CUI host.
	 */
	bool Attach(
		Control& control,
		const Module& module,
		const DesignerCustomControlType& customType,
		std::wstring* outError = nullptr);

	/** Sends a typed design value to an attached preview session. */
	bool SetValue(
		Control& control,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outError = nullptr);

	/**
	 * Probes custom descriptors against trusted modules and supplies factories
	 * only for identities a module accepts. Existing process-local factories
	 * always take precedence. Returns the number of newly bound descriptors.
	 */
	size_t AttachFactories(
		std::vector<DesignerControlDescriptor>& descriptors,
		const std::vector<Module>& modules);
}

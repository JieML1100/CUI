#pragma once

#include "DesignerTypes.h"

#include <string>
#include <string_view>
#include <vector>

/**
 * Loads portable custom-control descriptors without loading third-party code.
 * A host that already owns a concrete control implementation can attach a
 * process-local preview factory after the manifest has been validated.
 */
namespace DesignerControlCatalog
{
	using PreviewFactory =
		std::function<std::unique_ptr<Control>(int x, int y)>;

	std::vector<DesignerControlDescriptor> BuiltInDescriptors();

	/** Transactionally appends every descriptor in one UTF-8 manifest. */
	bool AppendFromXml(
		std::vector<DesignerControlDescriptor>& descriptors,
		std::string_view xml,
		std::wstring* outError = nullptr);

	/** Transactionally reads and appends one UTF-8 manifest file. */
	bool AppendFromFile(
		std::vector<DesignerControlDescriptor>& descriptors,
		const std::wstring& filePath,
		std::wstring* outError = nullptr);

	/**
	 * Attaches a real in-process preview implementation by portable XAML key.
	 * This deliberately avoids an unsafe cross-DLL C++ ownership contract.
	 */
	bool AttachPreviewFactory(
		std::vector<DesignerControlDescriptor>& descriptors,
		const std::wstring& xamlNamespace,
		const std::wstring& xamlName,
		PreviewFactory factory,
		std::wstring* outError = nullptr);
}

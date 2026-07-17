#pragma once

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace DesignerModel
{
enum class DesignDocumentFileFormat
{
	Xml,
	Xaml,
};

inline std::wstring DesignDocumentExtension(const std::wstring& filePath)
{
	auto extension = std::filesystem::path(filePath).extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
	return extension;
}

inline DesignDocumentFileFormat DetectDesignDocumentFileFormat(
	const std::wstring& filePath)
{
	return DesignDocumentExtension(filePath) == L".xaml"
		? DesignDocumentFileFormat::Xaml
		: DesignDocumentFileFormat::Xml;
}

inline bool HasDesignDocumentExtension(const std::wstring& filePath)
{
	const auto extension = DesignDocumentExtension(filePath);
	return extension == L".xml" || extension == L".xaml";
}
}

#pragma once

#include "DesignDocument.h"

namespace DesignerModel
{
class DesignDocumentSerializer
{
public:
	static bool SaveToFile(const DesignDocument& document, const std::wstring& filePath, std::wstring* outError = nullptr);
	static bool LoadFromFile(const std::wstring& filePath, DesignDocument& document, std::wstring* outError = nullptr);

private:
	static Json ToJson(const DesignDocument& document);
	static bool FromJson(const Json& root, DesignDocument& document, std::wstring* outError);
};
}
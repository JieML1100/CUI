#pragma once

#include "DesignDocument.h"
#include "../CodeGenInput.h"

namespace DesignerModel
{
class DesignDocumentCodeGenInputBuilder
{
public:
	static bool Build(const DesignDocument& document, CodeGenInput& input, std::wstring* outError = nullptr);
};
}
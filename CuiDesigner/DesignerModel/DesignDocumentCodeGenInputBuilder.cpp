#include "DesignDocumentCodeGenInputBuilder.h"
#include "../DesignerCanvas.h"

namespace DesignerModel
{
bool DesignDocumentCodeGenInputBuilder::Build(const DesignDocument& document, CodeGenInput& input, std::wstring* outError)
{
	int width = document.Form.Size.cx > 0 ? document.Form.Size.cx : 1;
	int height = document.Form.Size.cy > 0 ? document.Form.Size.cy : 1;
	auto runtimeCanvas = std::make_shared<DesignerCanvas>(0, 0, width, height);
	if (!runtimeCanvas->ApplyDesignDocument(document, outError))
	{
		return false;
	}

	input = runtimeCanvas->BuildCodeGenInput();
	input.RuntimeOwner = runtimeCanvas;
	return true;
}
}
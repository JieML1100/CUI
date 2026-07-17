#include "DesignDocumentCodeGenInputBuilder.h"
#include "RuntimeDocument.h"

namespace DesignerModel
{
bool DesignDocumentCodeGenInputBuilder::Build(const DesignDocument& document, CodeGenInput& input, std::wstring* outError)
{
	auto runtimeDocument = std::make_shared<RuntimeDocument>();
	RuntimeDocumentLoadOptions options;
	options.AllowCustomControlProxy = true;
	if (!RuntimeDocumentLoader::Load(
		document, *runtimeDocument, options, outError))
		return false;

	input = runtimeDocument->BuildCodeGenInput();
	input.RuntimeOwner = runtimeDocument;
	return true;
}
}

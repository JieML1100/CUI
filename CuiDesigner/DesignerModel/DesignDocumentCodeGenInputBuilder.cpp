#include "DesignDocumentCodeGenInputBuilder.h"
#include "RuntimeDocument.h"
#include <algorithm>

namespace DesignerModel
{
bool DesignDocumentCodeGenInputBuilder::Build(const DesignDocument& document, CodeGenInput& input, std::wstring* outError)
{
	const bool hasLocalObjectResources = std::any_of(
		document.Nodes.begin(), document.Nodes.end(), [](const auto& node)
		{ return !node.LocalObjectResources.Empty(); });
	if (!document.Components.empty()
		|| !document.DataTypes.empty()
		|| !document.DataTemplates.empty()
		|| !document.ItemsPanelTemplates.empty()
		|| !document.GroupStyles.empty()
		|| !document.DataLists.empty()
		|| !document.CollectionViews.empty()
		|| hasLocalObjectResources)
	{
		if (outError) *outError =
			L"声明组件、局部对象资源、DataType、DataList、CollectionViewSource、DataTemplate、ItemsPanelTemplate 和 GroupStyle 属于动态 XAML 类型系统，"
			L"完整 C++ UI 生成器不再尝试展开它们。";
		return false;
	}
	auto runtimeDocument = std::make_shared<RuntimeDocument>();
	RuntimeDocumentLoadOptions options;
	options.AllowNativeSurfacePlaceholder = true;
	if (!RuntimeDocumentLoader::Load(
		document, *runtimeDocument, options, outError))
		return false;

	input = runtimeDocument->BuildCodeGenInput();
	input.RuntimeOwner = runtimeDocument;
	return true;
}
}

#pragma once

#include "DesignerPropertyValue.h"
#include "DesignerTypes.h"
#include <functional>
#include <string>
#include <vector>

enum class DesignerControlPropertyEditorKind : unsigned char
{
	Text,
	FontName,
	FontSize,
	Anchor
};

/** One property owned by the Designer wrapper rather than Binding metadata. */
struct DesignerControlPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValueKind ValueKind = DesignerStyleValueKind::String;
	DesignerControlPropertyEditorKind Editor = DesignerControlPropertyEditorKind::Text;
	bool CanReset = false;
};

struct DesignerControlPropertyContext
{
	::Font* SharedFont = nullptr;
	std::function<std::wstring(DesignerControl&, const std::wstring&)> MakeUniqueName;
	std::function<void(UIClass, const std::wstring&)> SyncDefaultNameCounter;
	std::function<void(Control*, uint8_t)> ApplyAnchorStylesKeepingBounds;
};

namespace DesignerControlPropertyCatalog
{
	/** Returns presentation-ordered properties applicable to this wrapper. */
	std::vector<DesignerControlPropertyDescriptor> GetProperties(
		const DesignerControl& target);

	const DesignerControlPropertyDescriptor* Find(
		const DesignerControl& target,
		const std::wstring& propertyName);

	bool CaptureValue(
		const DesignerControl& target,
		const DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);

	bool ApplyValue(
		DesignerControl& target,
		DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	bool ResetValue(
		DesignerControl& target,
		DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
}

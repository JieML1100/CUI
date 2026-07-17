#pragma once

#include "../DesignerTypes.h"
#include "../DesignerControlPropertyCatalog.h"
#include "../DesignerFormPropertyCatalog.h"
#include "../DesignerPropertyRowCatalog.h"
#include "../DesignerPropertyEdit.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class DesignerCanvas;

class PropertyGridBinder
{
public:
	void SetCanvas(DesignerCanvas* canvas);
	DesignerCanvas* GetCanvas() const;

	void BindControl(const std::shared_ptr<DesignerControl>& control);
	void BindControls(
		const std::vector<std::shared_ptr<DesignerControl>>& controls,
		const std::shared_ptr<DesignerControl>& primaryControl = nullptr);
	bool IsFormBinding() const;
	std::shared_ptr<DesignerControl> GetBoundControl() const;
	const std::vector<std::shared_ptr<DesignerControl>>& GetBoundControls() const;
	Control* GetBoundRuntimeControl() const;

	DesignerModel::DesignFormModel CaptureFormModel() const;
	bool ApplyFormProperty(
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;
	bool ResetFormProperty(
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;
	::Font* GetDesignedFormSharedFont() const;
	std::vector<DesignerPropertyRow> GetPropertyRows() const;
	DesignerPropertyEditResult ApplyControlPropertyValue(
		const std::wstring& propertyName,
		const std::wstring& valueText) const;
	DesignerPropertyEditResult ResetControlPropertyValue(
		const std::wstring& propertyName) const;
	bool CaptureControlPropertySnapshot(
		const std::wstring& propertyName,
		DesignerPropertyBatchSnapshot& out,
		std::wstring* outError = nullptr) const;
	bool RestoreBoundControlPropertySnapshot(
		const DesignerPropertyBatchSnapshot& snapshot,
		std::wstring* outError = nullptr) const;
	DesignerControlPropertyContext CreateControlPropertyContext(
		const std::shared_ptr<DesignerControl>& target) const;

	std::vector<DesignerControlPropertyDescriptor> GetControlDesignProperties() const;
	bool CaptureControlDesignProperty(
		const std::wstring& propertyName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr) const;
	bool ApplyControlDesignProperty(
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;
	bool ApplyControlDesignProperty(
		const std::shared_ptr<DesignerControl>& target,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;
	bool ResetControlDesignProperty(
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;
	bool ResetControlDesignProperty(
		const std::shared_ptr<DesignerControl>& target,
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr) const;

	void NotifyControlChanged(Control* control) const;
	void ApplyAnchorStylesKeepingBounds(Control* control, uint8_t anchorStyles) const;
	std::wstring MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const;
	void SyncDefaultNameCounter(UIClass type, const std::wstring& name) const;
	void RemoveDesignerControlsInSubtree(Control* root) const;

private:
	std::vector<DesignerPropertyEditTarget> CreatePropertyEditTargets() const;
	DesignerCanvas* _canvas = nullptr;
	std::shared_ptr<DesignerControl> _control;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
};

#pragma once

#include "ContentPresenter.h"
#include "ItemsControl.h"

/**
 * WPF-style ItemsControl with a distinct Header slot.
 *
 * Header presentation is infrastructure, not an item. Authored/generated items
 * remain logical children of this control and visual children of ItemsHost.
 */
class HeaderedItemsControl : public ItemsControl
{
public:
	HeaderedItemsControl();
	UIClass Type() override { return UIClass::UI_HeaderedItemsControl; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;

	virtual BindingValue GetHeader() const { return _header; }
	virtual void SetHeader(BindingValue value);
	ItemTemplateReference GetHeaderTemplate() const noexcept
	{
		return _headerTemplate;
	}
	void SetHeaderTemplate(ItemTemplateReference value);
	const std::wstring& GetHeaderDisplayMemberPath() const noexcept
	{
		return _headerDisplayMemberPath;
	}
	void SetHeaderDisplayMemberPath(std::wstring value);
	const std::wstring& HeaderTypeName() const noexcept
	{
		return _headerTypeName;
	}
	void SetHeaderTypeName(std::wstring value);

	Control* SetVisualHeader(std::unique_ptr<Control> value);
	std::unique_ptr<Control> DetachVisualHeader();
	Control* GetVisualHeader() const noexcept
	{
		if (_visualHeader) return _visualHeader;
		return _templateHeaderPresenter
			? _templateHeaderPresenter->GetVisualContent() : nullptr;
	}
	ContentPresenter* GetGeneratedHeaderPresenter() const noexcept
	{
		return _headerPresenter;
	}
	ContentPresenter* GetTemplateHeaderPresenter() const noexcept
	{
		return _templateHeaderPresenter;
	}
	Control* GetGeneratedHeaderContent() const noexcept
	{
		auto* presenter = _templateHeaderPresenter
			? _templateHeaderPresenter : _headerPresenter;
		return presenter ? presenter->GetGeneratedContent() : nullptr;
	}
	bool RegisterTemplateHeaderPresenter(ContentPresenter* presenter);
	Control* GetHeaderVisual() const noexcept
	{
		if (auto* visual = GetVisualHeader()) return visual;
		return _templateHeaderPresenter
			? static_cast<Control*>(_templateHeaderPresenter)
			: static_cast<Control*>(_headerPresenter);
	}
	const std::wstring& LastHeaderError() const noexcept
	{
		return _lastHeaderError;
	}

protected:
	std::wstring GetSemanticText() const override;
	virtual void ConfigureHeaderVisual(Control& child);
	virtual void ReleaseHeaderVisual(Control& child);
	virtual cui::core::Insets GetHeaderPresentationInsets() const noexcept;
	virtual cui::core::Insets GetItemsPresentationInsets() const noexcept;
	virtual float GetHeaderSlotHeightDip(float availableWidth);
	void RequestLayout() override;
	void PerformPendingLayout() override;
	void OnControlTemplatePresentationChanged() override;
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;

private:
	BindingValue _header;
	ItemTemplateReference _headerTemplate;
	std::wstring _headerDisplayMemberPath;
	std::wstring _headerTypeName;
	std::wstring _lastHeaderError;
	Control* _visualHeader = nullptr;
	ContentPresenter* _headerPresenter = nullptr;
	ContentPresenter* _templateHeaderPresenter = nullptr;
	std::vector<Control*> _headerInfrastructure;
	bool _changingHeaderInfrastructure = false;
	bool _headeredLayoutPending = true;

	enum class HeaderInfrastructureRole
	{
		TemplateImplementation,
		LogicalSlot
	};
	Control* AddHeaderInfrastructure(
		std::unique_ptr<Control> child,
		HeaderInfrastructureRole role =
			HeaderInfrastructureRole::TemplateImplementation);
	std::unique_ptr<Control> DetachHeaderInfrastructure(Control* child);
	bool IsHeaderInfrastructure(const Control* child) const noexcept;
	bool ValidateHeaderCandidate(
		const BindingValue& header,
		const ItemTemplateReference& headerTemplate,
		std::wstring& error) const;
	bool RebuildHeaderPresenter();
};

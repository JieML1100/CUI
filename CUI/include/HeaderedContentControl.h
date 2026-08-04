#pragma once

#include "ContentControl.h"

/**
 * A ContentControl with an independent Header slot. Header and Content each
 * accept either one authored visual or data rendered through a template.
 */
class HeaderedContentControl : public ContentControl
{
public:
	HeaderedContentControl();
	UIClass Type() override { return UIClass::UI_HeaderedContentControl; }
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& HeaderProperty();
	static const DependencyProperty& HeaderTemplateProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyProperty& HeaderDisplayMemberPathProperty();
#endif
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;

	BindingValue GetHeader() const { return _header; }
	void SetHeader(BindingValue value);
	ItemTemplateReference GetHeaderTemplate() const noexcept
	{
		return _headerTemplate;
	}
	void SetHeaderTemplate(ItemTemplateReference value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& GetHeaderDisplayMemberPath() const noexcept
	{
		return _headerDisplayMemberPath;
	}
	void SetHeaderDisplayMemberPath(std::wstring value);
#endif
	[[nodiscard]] CompiledBindingPathView
		GetCompiledHeaderDisplayMemberPath() const noexcept
	{
		return _compiledHeaderDisplayMemberPath;
	}
	void SetCompiledHeaderDisplayMemberPath(CompiledBindingPathView value);
	DataTypeToken GetHeaderTypeToken() const noexcept
	{
		return _headerTypeToken;
	}
	void SetHeaderTypeToken(DataTypeToken value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& HeaderTypeName() const noexcept
	{
		return _headerTypeName;
	}
	void SetHeaderTypeName(std::wstring value);
#endif

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
	/** Framework hook used when a ControlTemplate declares ContentSource=Header. */
	bool RegisterTemplateHeaderPresenter(ContentPresenter* presenter);
	Control* GetHeaderVisual() const noexcept
	{
		if (auto* visual = GetVisualHeader()) return visual;
		return _templateHeaderPresenter
			? static_cast<Control*>(_templateHeaderPresenter)
			: static_cast<Control*>(_headerPresenter);
	}
	/**
	 * Framework layout hook returning the natural size of the semantic Header
	 * content, independent of an outer template chrome's cached DesiredSize.
	 */
	cui::core::Size MeasureHeaderPresentationContent(
		const cui::core::Constraints& available);
	const std::wstring& LastHeaderError() const noexcept
	{
		return _lastHeaderError;
	}

protected:
	std::wstring GetSemanticText() const override;
	void ConfigureContentVisual(Control& child) override;
	virtual void ConfigureHeaderVisual(Control& child);
	virtual void ReleaseHeaderVisual(Control& child);
	virtual cui::core::Insets GetHeaderPresentationInsets() const noexcept;
	/** Fallback presentation slot; a ControlTemplate may replace it entirely. */
	virtual float GetHeaderSlotHeightDip(float availableWidth);
	void PerformPendingLayout() override;
	void OnControlTemplatePresentationChanged() override;

private:
	BindingValue _header;
	ItemTemplateReference _headerTemplate;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _headerDisplayMemberPath;
#endif
	CompiledBindingPathView _compiledHeaderDisplayMemberPath;
	DataTypeToken _headerTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _headerTypeName;
#endif
	std::wstring _lastHeaderError;
	Control* _visualHeader = nullptr;
	ContentPresenter* _headerPresenter = nullptr;
	ContentPresenter* _templateHeaderPresenter = nullptr;

	bool ValidateHeaderCandidate(
		const BindingValue& header,
		const ItemTemplateReference& headerTemplate,
		std::wstring& error) const;
	void ApplyHeaderProjection(ContentPresenter& presenter) const;
#if CUI_ENABLE_DYNAMIC_XAML
	void ApplyAuthoredHeaderProjection(ContentPresenter& presenter) const;
#endif
	bool RebuildHeaderPresenter();
};

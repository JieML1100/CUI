#pragma once

#include "ContentPresenter.h"

#include <exception>

namespace cui::framework
{
	struct TemplateAccess;
}

/**
 * Owns one default content position. Authored visual content and data content
 * are mutually exclusive; data content is rendered by an internal
 * ContentPresenter so both paths keep one-child ownership semantics.
 */
class ContentControl : public Control
{
public:
	ContentControl();
	UIClass Type() override { return UIClass::UI_ContentControl; }
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& ContentProperty();
	static const DependencyProperty& ContentTemplateProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyProperty& DisplayMemberPathProperty();
#endif
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
protected:
	void OnRender() override;
public:
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;

	BindingValue GetContent() const { return _content; }
	void SetContent(BindingValue value);
	ItemTemplateReference GetContentTemplate() const noexcept
	{
		return _contentTemplate;
	}
	void SetContentTemplate(ItemTemplateReference value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& GetDisplayMemberPath() const noexcept
	{
		return _displayMemberPath;
	}
	void SetDisplayMemberPath(std::wstring value);
#endif
	[[nodiscard]] CompiledBindingPathView
		GetCompiledDisplayMemberPath() const noexcept
	{
		return _compiledDisplayMemberPath;
	}
	void SetCompiledDisplayMemberPath(CompiledBindingPathView value);
	DataTypeToken GetContentTypeToken() const noexcept
	{
		return _contentTypeToken;
	}
	void SetContentTypeToken(DataTypeToken value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ContentTypeName() const noexcept
	{
		return _contentTypeName;
	}
	void SetContentTypeName(std::wstring value);
#endif

	/** Returns the authored child, excluding the generated presenter. */
	virtual Control* GetVisualContent() const noexcept;
	/** Replaces the single authored visual Content and transfers ownership. */
	virtual Control* SetVisualContent(std::unique_ptr<Control> value);
	/** Attempts to attach Content while preserving value on failure. */
	virtual bool TrySetVisualContent(
		std::unique_ptr<Control>& value) noexcept;
	/** Detaches the authored visual Content and returns its ownership. */
	virtual std::unique_ptr<Control> DetachVisualContent();
	const std::wstring& LastContentError() const noexcept
	{
		return _lastContentError;
	}

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Group, L"ContentControl");
	}
	void OnApplyTemplate() override;
	void PreparePresentation() override;
	void PrepareMeasureCore(
		const cui::core::Constraints& available) override;
	std::wstring GetSemanticText() const override;
	ContentPresenter* GetGeneratedPresenter() const noexcept
	{
		return _presenter;
	}
	ContentPresenter* GetTemplateContentPresenter() const noexcept
	{
		return dynamic_cast<ContentPresenter*>(
			_templateContentPresenter.Get());
	}
	Control* GetGeneratedContent() const noexcept
	{
		auto* presenter = GetTemplateContentPresenter();
		if (!presenter) presenter = _presenter;
		return presenter ? presenter->GetGeneratedContent() : nullptr;
	}
	/** Framework hook used when a ControlTemplate declares ContentSource=Content. */
	bool RegisterTemplateContentPresenter(ContentPresenter* presenter);
	/** Framework-owned visual root instantiated from ControlTemplate. */
	Control* SetControlTemplateRoot(std::unique_ptr<Control> value) override;
	std::unique_ptr<Control> DetachVisualChildTemplateRoot() override;
	Control* GetControlTemplateRoot() const noexcept override
	{
		return _controlTemplateRoot;
	}

	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	void OnLocalMeasurePathInvalidated() override
	{
		_contentLayoutPending = true;
	}
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnVisualChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;
	/** Detaches the authored visual for an infrastructure projection while
	 *  retaining this ContentControl as its logical parent. */
	std::unique_ptr<Control>
		DetachVisualContentPreservingLogicalParent();

	/**
	 * Framework-owned visuals (presenters, headers, decorators) participate in
	 * layout and rendering without consuming the authored Content slot. A
	 * directly authored Header remains a logical slot child; generated
	 * presenters and template roots are implementation-only.
	 */
	enum class InfrastructureChildRole
	{
		TemplateImplementation,
		LogicalSlot
	};
	Control* AddInfrastructureChild(
		std::unique_ptr<Control> child,
		InfrastructureChildRole role =
			InfrastructureChildRole::TemplateImplementation);
	std::unique_ptr<Control> DetachInfrastructureChild(
		Control* child,
		std::exception_ptr* notificationError = nullptr);
	bool IsInfrastructureChild(const Control* child) const noexcept;
	virtual void ConfigureContentVisual(Control& child);
	void ConfigureControlTemplateVisual(Control& child) override;
	void OnControlTemplatePresentationChanged() override;
	/** Shared pending bit for semantic content-slot specializations. */
	bool _contentLayoutPending = true;

private:
	friend struct cui::framework::TemplateAccess;

	BindingValue _content;
	ItemTemplateReference _contentTemplate;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _displayMemberPath;
#endif
	CompiledBindingPathView _compiledDisplayMemberPath;
	DataTypeToken _contentTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _contentTypeName;
#endif
	std::wstring _lastContentError;
	ContentPresenter* _presenter = nullptr;
	ControlWeakReference _templateContentPresenter;
	// A generated template registers ContentSource before its visual tree is
	// necessarily owned by this control.  Never dereference or publish this
	// candidate until the active template root is known to contain it.
	ControlWeakReference _pendingTemplateContentPresenter;
	Control* _controlTemplateRoot = nullptr;
	// Visual Content is logically owned by the ContentControl, not by a
	// particular ControlTemplate instance.  A template presenter owns it only
	// while that template is active; keep the object alive here across a
	// detach/re-template interval (and when a template intentionally omits a
	// ContentPresenter).
	std::unique_ptr<Control> _unpresentedVisualContent;
	std::vector<Control*> _infrastructureChildren;
	bool _changingInfrastructure = false;
	bool _visualContentProjectionPending = false;
	bool _projectingVisualContent = false;
	bool _contentVisualConfigurationPending = false;
	bool _configuringContentVisual = false;

	Control* AttachVisualContent(std::unique_ptr<Control>& value);
	Control* FindDirectVisualContent() const noexcept;
	Control* ContentVisualForConfiguration() const noexcept;
	bool TemplateRootContains(const Control* candidate) const noexcept;
	bool CommitPendingTemplateContentPresenter();
	void ClearPendingTemplateContentPresenter() noexcept;
	void EnsureVisualContentProjection();
	void ConfigurePendingContentVisual();
	void PreserveTemplateVisualContent(
		std::exception_ptr* notificationError = nullptr);
	void RestoreUnpresentedVisualContent();
	bool ValidateContentCandidate(
		const BindingValue& content,
		const ItemTemplateReference& contentTemplate,
		std::wstring& error) const;
	void ApplyContentProjection(ContentPresenter& presenter) const;
#if CUI_ENABLE_DYNAMIC_XAML
	void ApplyAuthoredContentProjection(ContentPresenter& presenter) const;
#endif
	bool RebuildPresenter();
};

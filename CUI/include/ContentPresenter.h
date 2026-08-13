#pragma once

#include "ItemTemplate.h"
#include "Control.h"

#include <cstdint>
#include <functional>

/**
 * Presents either one authored visual or one data value. ControlTemplate
 * ContentSource slots use the visual path, while scalar/object Content uses
 * the existing DataTemplate path; the two ownership modes are exclusive.
 */
class ContentPresenter : public Control
{
public:
	ContentPresenter();
	UIClass Type() override { return UIClass::UI_ContentPresenter; }
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
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
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
	/** Returns authored visual content, excluding generated DataTemplate output. */
	Control* GetVisualContent() const noexcept;
	Control* GetGeneratedContent() const noexcept { return _generatedContent; }
	const std::wstring& LastTemplateError() const noexcept
	{
		return _lastTemplateError;
	}

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"ContentPresenter");
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

private:
	static const DependencyPropertyMetadataRegistration&
		ContentPropertyMetadataRelation();
	static const DependencyPropertyMetadataRegistration&
		ContentTemplatePropertyMetadataRelation();
	BindingValue _content;
	ItemTemplateReference _contentTemplate;
	BindingPathObservation _contentObservation;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _displayMemberPath;
#endif
	CompiledBindingPathView _compiledDisplayMemberPath;
	DataTypeToken _contentTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _contentTypeName;
#endif
	std::wstring _lastTemplateError;
	Control* _generatedContent = nullptr;
	// Rebuild callbacks can synchronously invoke a newer rebuild. Tree
	// validation therefore follows an exact transaction/candidate identity
	// instead of one shared mutation bit.
	std::uint64_t _generatedContentTransactionSerial = 0;
	std::uint64_t _generatedContentCommittedTransaction = 0;
	std::uint64_t _generatedContentMutationTransaction = 0;
	ControlWeakReference _generatedContentMutationVisual;
	size_t _generatedContentTransactionDepth = 0;
	bool _contentLayoutPending = true;

	bool ValidateContentCandidate(
		const BindingValue& content,
		const ItemTemplateReference& contentTemplate,
		std::wstring& error) const;
	std::wstring ReadProjectedDisplayText(
		const BindingSourceReference& source) const;
	BindingPathObservation ObserveProjectedDisplayPath(
		const BindingSourceReference& source,
		std::function<void()> changed) const;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring ReadAuthoredProjectedDisplayText(
		const BindingSourceReference& source) const;
	BindingPathObservation ObserveAuthoredProjectedDisplayPath(
		const BindingSourceReference& source,
		std::function<void()> changed) const;
#endif
	bool RebuildContent();
};

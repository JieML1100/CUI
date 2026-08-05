#pragma once
#include "Control.h"

/**
 * @file Image.h
 * @brief Image 的原生渲染宿主。
 *
 * XAML 公开面采用 WPF 的 Source、Stretch 与 StretchDirection；Image 只是内部
 * C++ 行为类型，不构成第二个公开类型身份。
 */
class Image : public Control
{
private:
	std::shared_ptr<BitmapSource> _source;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> _sourceCache;
	ID2D1RenderTarget* _sourceCacheTarget = nullptr;
	::Stretch _stretch =
		::Stretch::Uniform;
	StretchDirection _stretchDirection = StretchDirection::Both;
	ID2D1Bitmap* EnsureSourceCache();
	void RenderSource();
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Image, L"Image");
	}
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
public:
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	static const DependencyProperty& SourceProperty();
	static const DependencyProperty& StretchProperty();
	static const DependencyProperty& StretchDirectionProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	PROPERTY(std::shared_ptr<BitmapSource>, Source);
	GET(std::shared_ptr<BitmapSource>, Source);
	SET(std::shared_ptr<BitmapSource>, Source);
	PROPERTY(::Stretch, Stretch);
	GET(::Stretch, Stretch);
	SET(::Stretch, Stretch);
	PROPERTY(::StretchDirection, StretchDirection);
	GET(::StretchDirection, StretchDirection);
	SET(::StretchDirection, StretchDirection);
	/** @brief 创建图片控件。 */
	Image();
protected:
	void OnRender() override;
	void NotifyDeviceResourcesInvalidated() noexcept override;
};

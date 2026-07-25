#pragma once
#include "Control.h"

/**
 * @file Image.h
 * @brief Image 的原生渲染宿主。
 *
 * XAML 公开面只有 WPF 语义的 Source 与 Stretch；Image 只是内部
 * C++ 行为类型，不构成第二个公开类型身份。
 */
class Image : public Control
{
private:
	std::shared_ptr<BitmapSource> _source;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> _sourceCache;
	ID2D1RenderTarget* _sourceCacheTarget = nullptr;
	cui::drawing::ImageBrushStretch _stretch =
		cui::drawing::ImageBrushStretch::Uniform;
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
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	PROPERTY(std::shared_ptr<BitmapSource>, Source);
	GET(std::shared_ptr<BitmapSource>, Source);
	SET(std::shared_ptr<BitmapSource>, Source);
	PROPERTY(cui::drawing::ImageBrushStretch, Stretch);
	GET(cui::drawing::ImageBrushStretch, Stretch);
	SET(cui::drawing::ImageBrushStretch, Stretch);
	/** @brief 创建图片控件。 */
	Image();
protected:
	void OnRender() override;
	void NotifyDeviceResourcesInvalidated() noexcept override;
};

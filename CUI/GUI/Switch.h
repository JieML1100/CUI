#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file Switch.h
 * @brief Switch：开关控件。
 *
 * 说明：
 * - 状态通常使用基类字段 Control::Checked 表示（开/关）
 * - 交互由 ProcessMessage 处理（点击、拖动等），绘制由 Update 负责
 */
class Switch : public Control
{
	float last_width = 0.0f;
	float _thumbProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 140;
	bool _animating = false;
	void SyncAnimationState();
	void StartToggleAnimation(bool checked);
	float CurrentThumbProgress();
protected:
	bool DefaultTrackUnderMouse() const override { return true; }
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	bool DefaultClearSelectionOnMouseUp() const override { return true; }
	bool DefaultSelectOnLeftButtonDoubleClick() const override { return true; }
	bool DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const override;
	bool DefaultPostRenderOnMouseDoubleClick(UINT message, bool wasSelected) const override;
	void BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected) override;
	void BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected) override;
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 关闭状态轨道色。 */
	D2D1_COLOR_F TrackOffColor = D2D1_COLOR_F{ 0.42f, 0.46f, 0.52f, 0.38f };
	/** @brief 开启状态轨道色。 */
	D2D1_COLOR_F TrackOnColor = D2D1_COLOR_F{ 0.28f, 0.63f, 0.98f, 0.92f };
	/** @brief 轨道边框色。 */
	D2D1_COLOR_F TrackBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.18f };
	/** @brief 拇指颜色。 */
	D2D1_COLOR_F ThumbColor = D2D1_COLOR_F{ 0.98f, 0.99f, 1.0f, 1.0f };
	/** @brief 拇指阴影色。 */
	D2D1_COLOR_F ThumbShadowColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.24f };
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	/** @brief 轨道内边距，影响拇指大小。 */
	float TrackPadding = 3.0f;
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
	/** @brief 创建开关。 */
	Switch(int x = 0, int y = 0, int width = 60, int height = 22);
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
};

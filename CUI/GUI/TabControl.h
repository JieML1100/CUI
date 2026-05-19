#pragma once
#include "Control.h"
#include "Panel.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file TabControl.h
 * @brief TabControl/TabPage：分页容器控件。
 *
 * TabControl 自身继承自 Control，通过 Children 管理多个 TabPage。
 * SelectedChanged 事件沿用 Control::OnSelectedChanged。
 */

class TabPage : public Panel
{
public:
	virtual UIClass Type();
	std::shared_ptr<BitmapSource> HeaderImage;
	float HeaderImageSize = 16.0f;
	float HeaderImageGap = 6.0f;
	TabPage();
	TabPage(std::wstring text);
	void SetHeaderImage(std::shared_ptr<BitmapSource> value);
	ID2D1Bitmap* EnsureHeaderImageCache();
	void Update() override;

private:
	std::shared_ptr<BitmapSource> _headerImageCacheSource;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> _headerImageCache;
	ID2D1RenderTarget* _headerImageCacheTarget = nullptr;
};

enum class TabControlAnimationMode
{
	DirectReplace = 0,
	SlideHorizontal = 1,
};

enum class TabControlTitlePosition
{
	Top = 0,
	Bottom = 1,
	Left = 2,
	Right = 3,
};

/**
 * @brief TabControl：带标题栏的分页容器。
 *
 * - SelectedIndex 为当前选中页索引（0-based）
 * - Update 内会根据 SelectedIndex 维护各页 Visible，并绘制标题栏
 * - 为兼容 WebBrowser 等“原生子窗口控件”，切换页时会触发一次同步（见 TabControl.cpp）
 */
class TabControl : public Control
{
public:
	virtual UIClass Type();
	D2D1_COLOR_F TitleBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F SelectedTitleBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.14f };
	D2D1_COLOR_F TitleHoverBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.08f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 1.0f };
	D2D1_COLOR_F TitleMutedForeColor = Colors::DimGrey;
	float TitleCornerRadius = 7.0f;
	float TitleGap = 3.0f;
	float TitleInset = 2.0f;
	float SelectedAccentSize = 3.0f;
	bool EnableTitleScroll = true;
	int TitleScrollOffset = 0;
	int TitleScrollMouseWheelStep = 64;
	float TitleScrollButtonSize = 24.0f;
	D2D1_COLOR_F TitleScrollTrackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.12f };
	D2D1_COLOR_F TitleScrollThumbColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.58f };
	D2D1_COLOR_F TitleScrollButtonBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.76f };
	D2D1_COLOR_F TitleScrollButtonHoverBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.18f };
	TabControlAnimationMode AnimationMode = TabControlAnimationMode::DirectReplace;
	TabControlTitlePosition TitlePosition = TabControlTitlePosition::Top;
	/** @brief 当前选中页索引（0-based）。 */
	int SelectedIndex = 0;
	/** @brief 标题栏高度（像素）。 */
	int TitleHeight = 24;
	/** @brief 单个标题宽度（像素）。 */
	int TitleWidth = 120;
	float Boder = 1.5f;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);
	READONLY_PROPERTY(std::vector<Control*>&, Pages);
	GET(std::vector<Control*>&, Pages);
	/**
	 * @brief 创建 TabControl。
	 */
	TabControl(int x, int y, int width = 120, int height = 24);
	/**
	 * @brief 新增一个 TabPage。
	 * @return 新建页指针（所有权属于 TabControl）。
	 */
	TabPage* AddPage(std::wstring name);
	bool TryGetTitleIndexAt(int xof, int yof, int& outIndex);
	D2D1_RECT_F GetTitleViewportRect();
	bool IsTitleOverflowing();
	void SetTitleScrollOffset(int value);
	void ScrollTitleBy(int delta);
	void EnsureTitleVisible(int index);
	int HitTestTitleScrollButton(int xof, int yof);
	D2D1_RECT_F GetContentRect();
	std::vector<Control*> GetVisibleScenePages();
	bool ClipsChildren() override { return true; }
	D2D1_RECT_F GetChildrenClipRect() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	// 解决“拖动/松开时鼠标移出控件导致事件丢失”的问题：
	// TabControl 需要记住鼠标按下命中的子控件，并在按键按住期间持续转发 mousemove / buttonup。
	Control* _capturedChild = NULL;

	// 记录上一次选择页，用于在 Update 中检测程序切换页并同步原生子窗口控件（如 WebBrowser）
	int _lastSelectIndex = -1;
	int _displayIndex = -1;
	int _hoverTitleIndex = -1;
	int _hoverTitleScrollButton = 0;
	int _pressedTitleScrollButton = 0;
	int _pressedTitleIndex = -1;
	bool _dragTitleStrip = false;
	bool _titleStripDragMoved = false;
	int _titleDragStartPos = 0;
	int _titleDragStartOffset = 0;
	int _lastTitleEnsureIndex = -1;
	int _lastTitleEnsureCount = -1;
	float _lastTitleEnsureViewportLength = -1.0f;
	TabControlTitlePosition _lastTitleEnsurePosition = TabControlTitlePosition::Top;
	int _animFromIndex = -1;
	int _animToIndex = -1;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 180;
	float _animProgress = 1.0f;
	bool _animating = false;
	D2D1_RECT_F GetTitleRect(int index);
	void ClampSelectedIndex();
	void ClampTitleScrollOffset();
	void LayoutPage(TabPage* page, int offsetX, int offsetY = 0);
	void SyncPageVisibility();
	void FinishTransition();
	void StartTransitionTo(int newIndex);
	void EnsureSelectionState();
	float CurrentTransitionProgress();
};

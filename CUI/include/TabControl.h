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
private:
	D2D1_COLOR_F _titleBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F _selectedTitleBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _titleHoverBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _accentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _titleMutedForeColor = cui::theme::palette::TextMuted;
	float _titleCornerRadius = 7.0f;
	float _titleGap = 3.0f;
	float _titleInset = 2.0f;
	float _selectedAccentSize = 3.0f;
	bool _enableTitleScroll = true;
	float _titleScrollOffset = 0.0f;
	float _titleScrollMouseWheelStep = 64.0f;
	float _titleScrollButtonSize = 24.0f;
	D2D1_COLOR_F _titleScrollTrackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _titleScrollThumbColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _titleScrollButtonBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _titleScrollButtonHoverBackColor = cui::theme::palette::AccentSelected;
	int _animationMode = static_cast<int>(TabControlAnimationMode::DirectReplace);
	int _titlePosition = static_cast<int>(TabControlTitlePosition::Top);
	int _selectedIndex = 0;
	float _titleHeight = 24.0f;
	float _titleWidth = 120.0f;
	float _borderThickness = 1.5f;
	int _animationDurationMs = 180;

	void ApplySelectedIndexChange(int oldValue, int newValue);
	void SetCurrentSelectedIndex(int value);
	void SetCurrentTitleScrollOffset(float value);
	void PreparePageMutation();
	void ReconcilePagesAfterMutation(
		Control* previouslySelectedPage, int previousSelectedIndex);
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;

public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;

#define CUI_TAB_CONTROL_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleBackColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, SelectedTitleBackColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleHoverBackColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleMutedForeColor);
	CUI_TAB_CONTROL_PROPERTY(float, TitleCornerRadius);
	CUI_TAB_CONTROL_PROPERTY(float, TitleGap);
	CUI_TAB_CONTROL_PROPERTY(float, TitleInset);
	CUI_TAB_CONTROL_PROPERTY(float, SelectedAccentSize);
	CUI_TAB_CONTROL_PROPERTY(bool, EnableTitleScroll);
	CUI_TAB_CONTROL_PROPERTY(float, TitleScrollOffset);
	CUI_TAB_CONTROL_PROPERTY(float, TitleScrollMouseWheelStep);
	CUI_TAB_CONTROL_PROPERTY(float, TitleScrollButtonSize);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleScrollTrackColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleScrollThumbColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleScrollButtonBackColor);
	CUI_TAB_CONTROL_PROPERTY(D2D1_COLOR_F, TitleScrollButtonHoverBackColor);
	CUI_TAB_CONTROL_PROPERTY(TabControlAnimationMode, AnimationMode);
	CUI_TAB_CONTROL_PROPERTY(TabControlTitlePosition, TitlePosition);
	/** @brief 当前选中页索引（0-based）。 */
	CUI_TAB_CONTROL_PROPERTY(int, SelectedIndex);
	/** @brief 标题栏高度（DIP）。 */
	CUI_TAB_CONTROL_PROPERTY(float, TitleHeight);
	/** @brief 单个标题宽度（DIP）。 */
	CUI_TAB_CONTROL_PROPERTY(float, TitleWidth);
	CUI_TAB_CONTROL_PROPERTY(float, BorderThickness);
	CUI_TAB_CONTROL_PROPERTY(UINT, AnimationDurationMs);

#undef CUI_TAB_CONTROL_PROPERTY

	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);
	/**
	 * @brief 旧版可变页集合视图。
	 *
	 * 直接修改该 vector 不会维护父子所有权、选中页或原生子窗口状态；
	 * 新代码应使用 InsertPage/DetachPage/RemovePage/ClearPages。
	 */
	READONLY_PROPERTY(Control::ChildCollection&, Pages);
	GET(Control::ChildCollection&, Pages);
	/**
	 * @brief 创建 TabControl。
	 */
	TabControl(int x, int y, int width = 120, int height = 24);
	/**
	 * @brief 新增一个 TabPage。
	 * @return 新建页指针（所有权属于 TabControl）。
	 */
	TabPage* AddPage(std::wstring name);
	/** @brief 添加一个未挂载页并接管所有权。 */
	TabPage* AddPage(std::unique_ptr<TabPage> page);
	/**
	 * @brief 在指定位置创建页。
	 * @return 新建页；index 不在 [0, PageCount] 时返回 nullptr。
	 */
	TabPage* InsertPage(int index, std::wstring name);
	/**
	 * @brief 在指定位置接管一个未挂载页。
	 * @return 插入页；参数无效时抛出 invalid_argument/out_of_range。
	 */
	TabPage* InsertPage(int index, std::unique_ptr<TabPage> page);
	/** @brief 获取页；索引无效时返回 nullptr。 */
	TabPage* GetPage(int index) const noexcept;
	/** @brief 返回页当前索引；不存在时返回 -1。 */
	int IndexOfPage(const TabPage* page) const noexcept;
	/** @brief 分离页并将所有权交给调用方。 */
	std::unique_ptr<TabPage> DetachPageAt(int index);
	std::unique_ptr<TabPage> DetachPage(TabPage* page);
	/** @brief 移除并销毁页。 */
	bool RemovePageAt(int index);
	bool RemovePage(TabPage* page);
	/** @brief 移除并销毁全部页。 */
	void ClearPages();
	/** Selects a page and raises OnSelectedChanged when the index changes. */
	bool SelectPage(int index);
	bool TryGetTitleIndexAt(int localX, int localY, int& outIndex);
	D2D1_RECT_F GetTitleViewportRect();
	bool IsTitleOverflowing();
	void ScrollTitleBy(float delta);
	void EnsureTitleVisible(int index);
	int HitTestTitleScrollButton(int localX, int localY);
	D2D1_RECT_F GetContentRect();
	std::vector<Control*> GetVisibleScenePages();
	bool ClipsChildren() override { return true; }
	D2D1_RECT_F GetChildrenClipRect() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	void PerformPendingLayout() override;

	// 解决“拖动/松开时鼠标移出控件导致事件丢失”的问题：
	// TabControl 需要记住鼠标按下命中的子控件，并在按键按住期间持续转发 mousemove / buttonup。
	Control* _capturedChild = nullptr;

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
	float _titleDragStartOffset = 0.0f;
	int _lastTitleEnsureIndex = -1;
	int _lastTitleEnsureCount = -1;
	float _lastTitleEnsureViewportLength = -1.0f;
	TabControlTitlePosition _lastTitleEnsurePosition = TabControlTitlePosition::Top;
	int _animFromIndex = -1;
	int _animToIndex = -1;
	ULONGLONG _animStartTick = 0;
	float _animProgress = 1.0f;
	bool _animating = false;
	D2D1_RECT_F GetTitleRect(int index);
	void ClampSelectedIndex();
	void ClampTitleScrollOffset();
	void LayoutPage(TabPage* page, float offsetX, float offsetY = 0.0f);
	void SyncPageVisibility();
	void FinishTransition();
	void StartTransitionTo(int newIndex);
	void EnsureSelectionState();
	float CurrentTransitionProgress();
};

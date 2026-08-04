#pragma once
#include "Control.h"

/**
 * @file PasswordBox.h
 * @brief PasswordBox：密码输入框。
 *
 * 行为概览：
 * - 负责处理输入、选择、光标与水平滚动
 * - 显示层面通常会对文本进行掩码渲染（实现见 cpp）
 */
class PasswordBox : public Control
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TextBoxAutomationPeer>(
			*this, L"PasswordBox", true);
	}
private:
	// Password content is private editor state. It must never share the
	// Control/TextBlock/TextBox dependency-property storage path.
	std::wstring _password;
	std::wstring _passwordChar = L"*";
	int _maxLength = 0;
	cui::drawing::Brush _selectionBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::Accent);
	double _selectionOpacity = 0.4;
	cui::drawing::Brush _selectionTextBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::OnAccent);
	cui::drawing::Brush _caretBrush;
	bool _isInactiveSelectionHighlightEnabled = false;
	D2D1_SIZE_F _textSize = { 0,0 };
	Microsoft::WRL::ComPtr<IDWriteTextLayout> _textLayout;
	std::wstring _layoutText;
	IDWriteTextFormat* _layoutFont = nullptr;
	int _selectionStart = 0;
	int _selectionEnd = 0;
	float _horizontalScrollOffset = 0.0f;
public:
	using UIElement::PasswordChanged;
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	static const DependencyProperty& PasswordProperty();
	static const DependencyProperty& PasswordCharProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	PROPERTY(std::wstring, Password);
	GET(std::wstring, Password);
	SET(std::wstring, Password);
	PROPERTY(std::wstring, PasswordChar);
	GET(std::wstring, PasswordChar);
	SET(std::wstring, PasswordChar);
	PROPERTY(int, MaxLength);
	GET(int, MaxLength);
	SET(int, MaxLength);
	PROPERTY(cui::drawing::Brush, SelectionBrush);
	GET(cui::drawing::Brush, SelectionBrush);
	SET(cui::drawing::Brush, SelectionBrush);
	PROPERTY(double, SelectionOpacity);
	GET(double, SelectionOpacity);
	SET(double, SelectionOpacity);
	PROPERTY(cui::drawing::Brush, SelectionTextBrush);
	GET(cui::drawing::Brush, SelectionTextBrush);
	SET(cui::drawing::Brush, SelectionTextBrush);
	PROPERTY(cui::drawing::Brush, CaretBrush);
	GET(cui::drawing::Brush, CaretBrush);
	SET(cui::drawing::Brush, CaretBrush);
	PROPERTY(bool, IsInactiveSelectionHighlightEnabled);
	GET(bool, IsInactiveSelectionHighlightEnabled);
	SET(bool, IsInactiveSelectionHighlightEnabled);
	CursorKind QueryCursor(int localX, int localY) override { (void)localX; (void)localY; return this->IsEnabled ? CursorKind::IBeam : CursorKind::Arrow; }
	bool HitTestChildren() const override { return false; }
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(Key key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
protected:
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
public:
	PasswordBox();
private:
	void CommitPasswordEdit(std::wstring value);
	void PublishPasswordChanged();
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void InvalidateTextLayout() noexcept;
	IDWriteTextLayout* EnsureTextLayout();
	float GetTextOriginX();
	float GetTextOriginY();
	int HitTestTextPosition(float localX, float localY);
	bool GetCaretLayoutMetrics(
		int caretIndex, DWRITE_HIT_TEST_METRICS& metrics);
	void UpdateScroll(bool arrival = false);
	/** @brief 返回用于渲染/命中测试的文本（掩码或明文）。 */
	std::wstring GetDisplayText();
public:
	/** @brief 获取当前选择文本。 */
	std::wstring GetSelectedString();

	// ---- 公共选择/编辑 API（不暴露 Copy/Cut，避免密码进入剪贴板） ----
	// 注：框架的 PROPERTY/GET 宏生成的 getter 均非常量，故这些方法也不加 const。
	int GetSelectionStart();
	int GetSelectionLength();
	__declspec(property(get = GetSelectionLength)) int SelectionLength;
	int GetCaretIndex();
	void SetCaretIndex(int value);
	__declspec(property(get = GetCaretIndex, put = SetCaretIndex)) int CaretIndex;
	bool HasSelection();
	void Select(int start, int length);
	void SelectAll();
	void ClearSelection();
	void Clear();
	void InsertText(const std::wstring& text);
	bool Paste();

protected:
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect) override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};

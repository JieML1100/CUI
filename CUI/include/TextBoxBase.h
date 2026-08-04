#pragma once

#include "Control.h"
#include "ScrollViewer.h"

/**
 * WPF semantic base for editable text controls.
 *
 * TextBoxBase owns the common editing policy and presentation properties used
 * by TextBox and RichTextBox. PasswordBox is not derived from TextBoxBase in
 * WPF; it adds ownership of the selection/caret properties separately.
 */
class TextBoxBase : public Control
{
public:
	UIClass Type() override { return UIClass::UI_TextBoxBase; }
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& IsReadOnlyProperty();
	static const DependencyProperty& IsReadOnlyCaretVisibleProperty();
	static const DependencyProperty& AcceptsReturnProperty();
	static const DependencyProperty& AcceptsTabProperty();
	static const DependencyProperty& AutoWordSelectionProperty();
	static const DependencyProperty& IsUndoEnabledProperty();
	static const DependencyProperty& UndoLimitProperty();
	static const DependencyProperty&
		IsInactiveSelectionHighlightEnabledProperty();
	static const DependencyProperty& HorizontalScrollBarVisibilityProperty();
	static const DependencyProperty& VerticalScrollBarVisibilityProperty();
	static const DependencyProperty& SelectionBrushProperty();
	static const DependencyProperty& SelectionOpacityProperty();
	static const DependencyProperty& SelectionTextBrushProperty();
	static const DependencyProperty& CaretBrushProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	PROPERTY(bool, IsReadOnly);
	GET(bool, IsReadOnly);
	SET(bool, IsReadOnly);
	PROPERTY(bool, IsReadOnlyCaretVisible);
	GET(bool, IsReadOnlyCaretVisible);
	SET(bool, IsReadOnlyCaretVisible);
	PROPERTY(bool, AcceptsReturn);
	GET(bool, AcceptsReturn);
	SET(bool, AcceptsReturn);
	PROPERTY(bool, AcceptsTab);
	GET(bool, AcceptsTab);
	SET(bool, AcceptsTab);
	PROPERTY(bool, AutoWordSelection);
	GET(bool, AutoWordSelection);
	SET(bool, AutoWordSelection);
	PROPERTY(bool, IsUndoEnabled);
	GET(bool, IsUndoEnabled);
	SET(bool, IsUndoEnabled);
	PROPERTY(int, UndoLimit);
	GET(int, UndoLimit);
	SET(int, UndoLimit);
	PROPERTY(bool, IsInactiveSelectionHighlightEnabled);
	GET(bool, IsInactiveSelectionHighlightEnabled);
	SET(bool, IsInactiveSelectionHighlightEnabled);
	PROPERTY(ScrollBarVisibility, HorizontalScrollBarVisibility);
	GET(ScrollBarVisibility, HorizontalScrollBarVisibility);
	SET(ScrollBarVisibility, HorizontalScrollBarVisibility);
	PROPERTY(ScrollBarVisibility, VerticalScrollBarVisibility);
	GET(ScrollBarVisibility, VerticalScrollBarVisibility);
	SET(ScrollBarVisibility, VerticalScrollBarVisibility);
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

	bool IsAccessibilityReadOnly() const override { return _isReadOnly; }

protected:
	TextBoxBase();
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	void VisitDeclaredInheritedProperties(
		void* context, InheritedPropertyVisitor visitor) const override;
	static const DependencyPropertyMetadataRegistration&
		HorizontalScrollBarVisibilityMetadataRelation();
	static const DependencyPropertyMetadataRegistration&
		VerticalScrollBarVisibilityMetadataRelation();
	static const DependencyPropertyMetadataRegistration&
		FocusableMetadataRelation();

	bool _isReadOnly = false;
	bool _isReadOnlyCaretVisible = false;
	bool _acceptsReturn = false;
	bool _acceptsTab = false;
	bool _autoWordSelection = false;
	bool _isUndoEnabled = true;
	int _undoLimit = -1;
	bool _isInactiveSelectionHighlightEnabled = false;
	ScrollBarVisibility _horizontalScrollBarVisibility =
		ScrollBarVisibility::Hidden;
	ScrollBarVisibility _verticalScrollBarVisibility =
		ScrollBarVisibility::Hidden;
	cui::drawing::Brush _selectionBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::Accent);
	double _selectionOpacity = 0.4;
	cui::drawing::Brush _selectionTextBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::OnAccent);
	cui::drawing::Brush _caretBrush;

	virtual void OnUndoPolicyChanged() {}
	virtual void OnScrollPolicyChanged() {}
};

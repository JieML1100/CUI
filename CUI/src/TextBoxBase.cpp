#include "TextBoxBase.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<TextBoxBase, TValue> TextBoxBaseOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags =
			DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<TextBoxBase, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	using DependencyPropertyAccessor = const DependencyProperty& (*)();

	auto TextBoxBaseSubscriber(DependencyPropertyAccessor propertyAccessor)
	{
		return [propertyAccessor](
			TextBoxBase& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyAccessor, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property == &propertyAccessor()) handler();
				});
		};
	}

	DependencyPropertyOptions<TextBoxBase, cui::drawing::Brush> BrushOptions(
		cui::drawing::Brush defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = TextBoxBaseOptions(
			std::move(defaultValue)
			CUI_DESIGN_METADATA_ARGUMENTS(
				order,
				DependencyPropertyEditorKind::Text));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		)
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Convert = [](const BindingValue& value)
			-> std::optional<cui::drawing::Brush>
		{
			cui::drawing::Brush brush;
			if (value.TryGet(brush)) return brush;
			D2D1_COLOR_F color{};
			if (value.TryGet(color))
				return cui::drawing::MakeSolidColorBrush(color);
			return std::nullopt;
		};
		return options;
	}

	DependencyPropertyOptions<TextBoxBase, int> ScrollVisibilityOptions(
		ScrollBarVisibility defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = TextBoxBaseOptions(
			static_cast<int>(defaultValue)
			CUI_DESIGN_METADATA_ARGUMENTS(
				order,
				DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		// ValidateValue is owned by the ScrollViewer DependencyProperty
		// identity. AddOwner supplies only TextBoxBase metadata/defaults.
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Disabled", BindingValue(
				static_cast<int>(ScrollBarVisibility::Disabled)) },
			{ L"Auto", BindingValue(
				static_cast<int>(ScrollBarVisibility::Auto)) },
			{ L"Hidden", BindingValue(
				static_cast<int>(ScrollBarVisibility::Hidden)) },
			{ L"Visible", BindingValue(
				static_cast<int>(ScrollBarVisibility::Visible)) },
		};
		)
		return options;
	}

}

const DependencyProperty& TextBoxBase::IsReadOnlyProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"IsReadOnly"),
			[](TextBoxBase& target) { return target.IsReadOnly; },
			[](TextBoxBase& target, const bool& value)
			{ target.IsReadOnly = value; },
			TextBoxBaseSubscriber(&TextBoxBase::IsReadOnlyProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					10, DependencyPropertyEditorKind::Boolean),
				DependencyPropertyFlags::Inherits
					| DependencyPropertyFlags::AffectsRender));
	}();
	return *registration;
}

const DependencyPropertyMetadata*
TextBoxBase::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &ScrollViewer::HorizontalScrollBarVisibilityProperty())
		return &HorizontalScrollBarVisibilityMetadataRelation().Metadata();
	if (&property == &ScrollViewer::VerticalScrollBarVisibilityProperty())
		return &VerticalScrollBarVisibilityMetadataRelation().Metadata();
	if (&property == &Control::FocusableProperty())
		return &FocusableMetadataRelation().Metadata();
	return Control::ResolveExactDependencyPropertyMetadata(property);
}

void TextBoxBase::VisitDeclaredInheritedProperties(
	void* context, InheritedPropertyVisitor visitor) const
{
	Control::VisitDeclaredInheritedProperties(context, visitor);
	if (visitor) visitor(context, IsReadOnlyProperty());
}

const DependencyProperty& TextBoxBase::IsReadOnlyCaretVisibleProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"IsReadOnlyCaretVisible"),
			[](TextBoxBase& target) { return target.IsReadOnlyCaretVisible; },
			[](TextBoxBase& target, const bool& value)
			{ target.IsReadOnlyCaretVisible = value; },
			TextBoxBaseSubscriber(
				&TextBoxBase::IsReadOnlyCaretVisibleProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					20, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::AcceptsReturnProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"AcceptsReturn"),
			[](TextBoxBase& target) { return target.AcceptsReturn; },
			[](TextBoxBase& target, const bool& value)
			{ target.AcceptsReturn = value; },
			TextBoxBaseSubscriber(&TextBoxBase::AcceptsReturnProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					30, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::AcceptsTabProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"AcceptsTab"),
			[](TextBoxBase& target) { return target.AcceptsTab; },
			[](TextBoxBase& target, const bool& value)
			{ target.AcceptsTab = value; },
			TextBoxBaseSubscriber(&TextBoxBase::AcceptsTabProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					40, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::AutoWordSelectionProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"AutoWordSelection"),
			[](TextBoxBase& target) { return target.AutoWordSelection; },
			[](TextBoxBase& target, const bool& value)
			{ target.AutoWordSelection = value; },
			TextBoxBaseSubscriber(&TextBoxBase::AutoWordSelectionProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					50, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::IsUndoEnabledProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(L"IsUndoEnabled"),
			[](TextBoxBase& target) { return target.IsUndoEnabled; },
			[](TextBoxBase& target, const bool& value)
			{ target.IsUndoEnabled = value; },
			TextBoxBaseSubscriber(&TextBoxBase::IsUndoEnabledProperty),
			TextBoxBaseOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					60, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::UndoLimitProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxBaseOptions(
			-1 CUI_DESIGN_METADATA_ARGUMENTS(
				70, DependencyPropertyEditorKind::Number));
		options.Validate = [](const int& proposed) { return proposed >= -1; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = -1.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, int>(
			DependencyPropertyRegistrationLiteral(L"UndoLimit"),
			[](TextBoxBase& target) { return target.UndoLimit; },
			[](TextBoxBase& target, const int& value)
			{ target.UndoLimit = value; },
			TextBoxBaseSubscriber(&TextBoxBase::UndoLimitProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty&
TextBoxBase::IsInactiveSelectionHighlightEnabledProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, bool>(
			DependencyPropertyRegistrationLiteral(
				L"IsInactiveSelectionHighlightEnabled"),
			[](TextBoxBase& target)
			{ return target.IsInactiveSelectionHighlightEnabled; },
			[](TextBoxBase& target, const bool& value)
			{ target.IsInactiveSelectionHighlightEnabled = value; },
			TextBoxBaseSubscriber(
				&TextBoxBase::IsInactiveSelectionHighlightEnabledProperty),
			TextBoxBaseOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					80, DependencyPropertyEditorKind::Boolean)));
	}();
	return *registration;
}

const DependencyProperty&
TextBoxBase::HorizontalScrollBarVisibilityProperty()
{
	return HorizontalScrollBarVisibilityMetadataRelation().Property();
}

const DependencyProperty&
TextBoxBase::VerticalScrollBarVisibilityProperty()
{
	return VerticalScrollBarVisibilityMetadataRelation().Property();
}

const DependencyPropertyMetadataRegistration&
TextBoxBase::HorizontalScrollBarVisibilityMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		return DependencyPropertyRegistry::AddOwnerStatic<TextBoxBase, int>(
			ScrollViewer::HorizontalScrollBarVisibilityProperty(),
			[](TextBoxBase& target)
			{
				return static_cast<int>(
					target.HorizontalScrollBarVisibility);
			},
			[](TextBoxBase& target, const int& value)
			{
				target.HorizontalScrollBarVisibility =
					static_cast<ScrollBarVisibility>(value);
			},
			[](TextBoxBase& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.Property == &TextBoxBase::
							HorizontalScrollBarVisibilityProperty())
							handler();
					});
			},
			ScrollVisibilityOptions(ScrollBarVisibility::Hidden
				CUI_DESIGN_METADATA_ARGUMENTS(90)));
	}();
	return relation;
}

const DependencyPropertyMetadataRegistration&
TextBoxBase::VerticalScrollBarVisibilityMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		return DependencyPropertyRegistry::AddOwnerStatic<TextBoxBase, int>(
			ScrollViewer::VerticalScrollBarVisibilityProperty(),
			[](TextBoxBase& target)
			{
				return static_cast<int>(
					target.VerticalScrollBarVisibility);
			},
			[](TextBoxBase& target, const int& value)
			{
				target.VerticalScrollBarVisibility =
					static_cast<ScrollBarVisibility>(value);
			},
			[](TextBoxBase& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.Property == &TextBoxBase::
							VerticalScrollBarVisibilityProperty())
							handler();
					});
			},
			ScrollVisibilityOptions(ScrollBarVisibility::Hidden
				CUI_DESIGN_METADATA_ARGUMENTS(100)));
	}();
	return relation;
}

const DependencyPropertyMetadataRegistration&
TextBoxBase::FocusableMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		const auto& property = Control::FocusableProperty();
		DependencyPropertyOptions<TextBoxBase, bool> options;
		options.DefaultValue = true;
		CUI_DESIGN_METADATA_ONLY(
		const std::type_index controlOwner[] = {
			std::type_index(typeid(Control))
		};
		const auto* base = DependencyPropertyRegistry::FindRegistered(
			controlOwner, L"Focusable");
		if (!base)
			throw std::logic_error(
				"Control.Focusable must be registered before TextBoxBase");
		options.Design = base->Design();
		)
		return DependencyPropertyRegistry::OverrideMetadataStatic<
			TextBoxBase, Control, bool>(property, std::move(options));
	}();
	return relation;
}

const DependencyProperty& TextBoxBase::SelectionBrushProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<
			TextBoxBase, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"SelectionBrush"),
				[](TextBoxBase& target) { return target.SelectionBrush; },
				[](TextBoxBase& target, const cui::drawing::Brush& value)
				{ target.SelectionBrush = value; },
				TextBoxBaseSubscriber(&TextBoxBase::SelectionBrushProperty),
				BrushOptions(cui::drawing::MakeSolidColorBrush(
					cui::theme::palette::Accent)
					CUI_DESIGN_METADATA_ARGUMENTS(10)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::SelectionOpacityProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxBaseOptions(
			0.4 CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Number));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		)
		options.Validate = [](const double& proposed)
		{ return std::isfinite(proposed); };
		options.Coerce = [](TextBoxBase&, const double& proposed)
			-> std::optional<double>
		{ return (std::clamp)(proposed, 0.0, 1.0); };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Maximum = 1.0;
		options.Design.Step = 0.05;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextBoxBase, double>(
			DependencyPropertyRegistrationLiteral(L"SelectionOpacity"),
			[](TextBoxBase& target) { return target.SelectionOpacity; },
			[](TextBoxBase& target, const double& value)
			{ target.SelectionOpacity = value; },
			TextBoxBaseSubscriber(&TextBoxBase::SelectionOpacityProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::SelectionTextBrushProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<
			TextBoxBase, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"SelectionTextBrush"),
				[](TextBoxBase& target) { return target.SelectionTextBrush; },
				[](TextBoxBase& target, const cui::drawing::Brush& value)
				{ target.SelectionTextBrush = value; },
				TextBoxBaseSubscriber(&TextBoxBase::SelectionTextBrushProperty),
				BrushOptions(cui::drawing::MakeSolidColorBrush(
					cui::theme::palette::OnAccent)
					CUI_DESIGN_METADATA_ARGUMENTS(30)));
	}();
	return *registration;
}

const DependencyProperty& TextBoxBase::CaretBrushProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<
			TextBoxBase, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"CaretBrush"),
				[](TextBoxBase& target) { return target.CaretBrush; },
				[](TextBoxBase& target, const cui::drawing::Brush& value)
				{ target.CaretBrush = value; },
				TextBoxBaseSubscriber(&TextBoxBase::CaretBrushProperty),
				BrushOptions(cui::drawing::Brush{}
					CUI_DESIGN_METADATA_ARGUMENTS(40)));
	}();
	return *registration;
}

void TextBoxBase::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsReadOnlyProperty();
	(void)IsReadOnlyCaretVisibleProperty();
	(void)AcceptsReturnProperty();
	(void)AcceptsTabProperty();
	(void)AutoWordSelectionProperty();
	(void)IsUndoEnabledProperty();
	(void)UndoLimitProperty();
	(void)IsInactiveSelectionHighlightEnabledProperty();
	(void)SelectionBrushProperty();
	(void)SelectionOpacityProperty();
	(void)SelectionTextBrushProperty();
	(void)CaretBrushProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)HorizontalScrollBarVisibilityMetadataRelation();
	(void)VerticalScrollBarVisibilityMetadataRelation();
	(void)FocusableMetadataRelation();
	)
}

TextBoxBase::TextBoxBase()
{
	RegisterDependencyProperties();
}

GET_CPP(TextBoxBase, bool, IsReadOnly) { return _isReadOnly; }
SET_CPP(TextBoxBase, bool, IsReadOnly)
{
	(void)SetPropertyField(IsReadOnlyProperty(), _isReadOnly, value);
}
GET_CPP(TextBoxBase, bool, IsReadOnlyCaretVisible)
{
	return _isReadOnlyCaretVisible;
}
SET_CPP(TextBoxBase, bool, IsReadOnlyCaretVisible)
{
	(void)SetPropertyField(
		IsReadOnlyCaretVisibleProperty(),
		_isReadOnlyCaretVisible, value);
}
GET_CPP(TextBoxBase, bool, AcceptsReturn) { return _acceptsReturn; }
SET_CPP(TextBoxBase, bool, AcceptsReturn)
{
	(void)SetPropertyField(
		AcceptsReturnProperty(), _acceptsReturn, value);
}
GET_CPP(TextBoxBase, bool, AcceptsTab) { return _acceptsTab; }
SET_CPP(TextBoxBase, bool, AcceptsTab)
{
	(void)SetPropertyField(AcceptsTabProperty(), _acceptsTab, value);
}
GET_CPP(TextBoxBase, bool, AutoWordSelection)
{
	return _autoWordSelection;
}
SET_CPP(TextBoxBase, bool, AutoWordSelection)
{
	(void)SetPropertyField(
		AutoWordSelectionProperty(), _autoWordSelection, value);
}
GET_CPP(TextBoxBase, bool, IsUndoEnabled)
{
	return _isUndoEnabled;
}
SET_CPP(TextBoxBase, bool, IsUndoEnabled)
{
	if (SetPropertyField(
		IsUndoEnabledProperty(), _isUndoEnabled, value))
	{
		OnUndoPolicyChanged();
	}
}
GET_CPP(TextBoxBase, int, UndoLimit)
{
	return _undoLimit;
}
SET_CPP(TextBoxBase, int, UndoLimit)
{
	if (SetPropertyField(
		UndoLimitProperty(), _undoLimit, value))
	{
		OnUndoPolicyChanged();
	}
}
GET_CPP(TextBoxBase, bool, IsInactiveSelectionHighlightEnabled)
{
	return _isInactiveSelectionHighlightEnabled;
}
SET_CPP(TextBoxBase, bool, IsInactiveSelectionHighlightEnabled)
{
	(void)SetPropertyField(
		IsInactiveSelectionHighlightEnabledProperty(),
		_isInactiveSelectionHighlightEnabled, value);
}
GET_CPP(TextBoxBase, ScrollBarVisibility, HorizontalScrollBarVisibility)
{
	return _horizontalScrollBarVisibility;
}
SET_CPP(TextBoxBase, ScrollBarVisibility, HorizontalScrollBarVisibility)
{
	if (SetPropertyField(HorizontalScrollBarVisibilityProperty(),
		_horizontalScrollBarVisibility, value))
	{
		OnScrollPolicyChanged();
	}
}
GET_CPP(TextBoxBase, ScrollBarVisibility, VerticalScrollBarVisibility)
{
	return _verticalScrollBarVisibility;
}
SET_CPP(TextBoxBase, ScrollBarVisibility, VerticalScrollBarVisibility)
{
	if (SetPropertyField(VerticalScrollBarVisibilityProperty(),
		_verticalScrollBarVisibility, value))
	{
		OnScrollPolicyChanged();
	}
}
GET_CPP(TextBoxBase, cui::drawing::Brush, SelectionBrush)
{
	return _selectionBrush;
}
SET_CPP(TextBoxBase, cui::drawing::Brush, SelectionBrush)
{
	(void)SetPropertyField(
		SelectionBrushProperty(), _selectionBrush, std::move(value));
}
GET_CPP(TextBoxBase, double, SelectionOpacity)
{
	return _selectionOpacity;
}
SET_CPP(TextBoxBase, double, SelectionOpacity)
{
	(void)SetPropertyField(
		SelectionOpacityProperty(), _selectionOpacity, value);
}
GET_CPP(TextBoxBase, cui::drawing::Brush, SelectionTextBrush)
{
	return _selectionTextBrush;
}
SET_CPP(TextBoxBase, cui::drawing::Brush, SelectionTextBrush)
{
	(void)SetPropertyField(
		SelectionTextBrushProperty(),
		_selectionTextBrush, std::move(value));
}
GET_CPP(TextBoxBase, cui::drawing::Brush, CaretBrush)
{
	return _caretBrush;
}
SET_CPP(TextBoxBase, cui::drawing::Brush, CaretBrush)
{
	(void)SetPropertyField(
		CaretBrushProperty(), _caretBrush, std::move(value));
}

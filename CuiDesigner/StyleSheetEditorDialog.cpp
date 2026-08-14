#include "StyleSheetEditorDialog.h"
#include "DesignerControlCatalog.h"
#include "ProgrammaticControlFactory.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include <algorithm>
#include <iterator>

namespace
{
	const std::wstring kNewResource = L"<新增资源>";
	const std::wstring kNewRule = L"<新增规则>";
	const std::wstring kNewSetter = L"<新增 Setter>";

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	std::wstring RuleCaption(const DesignerStyleRule& rule, size_t index)
	{
		std::wstring selector;
		if (rule.HasType) selector = rule.ComponentType.Empty()
			? DesignerStyleSheetUtils::UIClassName(rule.Type)
			: rule.ComponentType.XamlPrefix + L":" + rule.ComponentType.XamlName;
		if (!rule.Id.empty())
		{
			if (!selector.empty()) selector += L" ";
			selector += L"#";
			selector += rule.Id;
		}
		if (!rule.BasedOn.empty()) selector += L" <- @" + rule.BasedOn;
		if (selector.empty()) selector = L"*";
		return L"规则 " + std::to_wstring(index + 1) + L"  " + selector;
	}
}

StyleSheetEditorDialog::StyleSheetEditorDialog(
	const DesignerStyleSheet& styleSheet,
	std::wstring resourceBasePath)
	: Window(),
	  ResultStyleSheet(styleSheet),
	  _resourceBasePath(std::move(resourceBasePath))
{
	this->Title = L"编辑文档样式表";
	this->Left = 250.0f;
	this->Top = 100.0f;
	this->Width = 1020.0f;
	this->Height = 760.0f;
	DesignerStyleSheetUtils::Canonicalize(ResultStyleSheet);
	this->ResizeMode = ::ResizeMode::NoResize;
	this->Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto tip = addContent(cui::designer::NewControl<Label>(
		L"资源与 Setter 使用强类型值；Trigger/MultiTrigger/DataTrigger/MultiDataTrigger 由 XAML 编辑器维护并在摘要中展示。",
		20, 12));
	tip->Width = 970.0f;
	tip->Height = 24.0f;

	// Resources
	auto resourcesTitle = addContent(cui::designer::NewControl<Label>(L"资源", 20, 48));
	resourcesTitle->Width = 450.0f;
	resourcesTitle->Height = 24.0f;
	_resourceList = addContent(cui::designer::NewControl<ComboBox>(L"", 20, 76, 450, 30));
	_resourceList->MaxDropDownHeight = 280.0f;
	auto resourceKeyLabel = addContent(cui::designer::NewControl<Label>(L"Key", 20, 118));
	resourceKeyLabel->Width = 80.0f;
	resourceKeyLabel->Height = 24.0f;
	_resourceKey = addContent(cui::designer::NewControl<TextBox>(L"", 105, 112, 365, 30));
	auto resourceKindLabel = addContent(cui::designer::NewControl<Label>(L"类型", 20, 158));
	resourceKindLabel->Width = 80.0f;
	resourceKindLabel->Height = 24.0f;
	_resourceKind = addContent(cui::designer::NewControl<ComboBox>(L"", 105, 152, 150, 30));
	auto resourceKinds = DesignerStyleSheetUtils::ValueKindNames();
	cui::designer::SetComboBoxItems(
		*_resourceKind, std::move(resourceKinds));
	_resourceKind->MaxDropDownHeight = 280.0f;
	auto resourceValueLabel = addContent(cui::designer::NewControl<Label>(L"值", 270, 158));
	resourceValueLabel->Width = 40.0f;
	resourceValueLabel->Height = 24.0f;
	_resourceValue = addContent(cui::designer::NewControl<TextBox>(L"", 310, 152, 160, 30));
	auto saveResource = addContent(cui::designer::NewControl<Button>(L"保存资源", 20, 196, 120, 32));
	auto removeResource = addContent(cui::designer::NewControl<Button>(L"删除资源", 152, 196, 120, 32));

	// Rule selector
	auto rulesTitle = addContent(cui::designer::NewControl<Label>(L"规则选择器", 500, 48));
	rulesTitle->Width = 480.0f;
	rulesTitle->Height = 24.0f;
	_ruleList = addContent(cui::designer::NewControl<ComboBox>(L"", 500, 76, 480, 30));
	_ruleList->MaxDropDownHeight = 280.0f;
	auto typeLabel = addContent(cui::designer::NewControl<Label>(L"类型", 500, 118));
	typeLabel->Width = 70.0f;
	typeLabel->Height = 24.0f;
	_ruleType = addContent(cui::designer::NewControl<ComboBox>(L"", 575, 112, 160, 30));
	std::vector<std::wstring> ruleTypes{ L"Any", L"Base" };
	for (const auto& control : DesignerControlCatalog::BuiltInDescriptors())
		ruleTypes.push_back(control.Name);
	cui::designer::SetComboBoxItems(*_ruleType, std::move(ruleTypes));
	_ruleType->MaxDropDownHeight = 336.0f;
	auto idLabel = addContent(cui::designer::NewControl<Label>(L"x:Key", 748, 118));
	idLabel->Width = 70.0f;
	idLabel->Height = 24.0f;
	_ruleId = addContent(cui::designer::NewControl<TextBox>(L"", 820, 112, 160, 30));
	auto basedOnLabel = addContent(cui::designer::NewControl<Label>(L"BasedOn", 500, 158));
	basedOnLabel->Width = 70.0f;
	basedOnLabel->Height = 24.0f;
	_ruleBasedOn = addContent(cui::designer::NewControl<TextBox>(L"", 575, 152, 160, 30));
	auto saveRule = addContent(cui::designer::NewControl<Button>(L"保存规则", 500, 192, 120, 32));
	auto removeRule = addContent(cui::designer::NewControl<Button>(L"删除规则", 632, 192, 120, 32));

	// Setter editor
	auto setterTitle = addContent(cui::designer::NewControl<Label>(L"当前规则的 Setter", 20, 286));
	setterTitle->Width = 960.0f;
	setterTitle->Height = 24.0f;
	_setterList = addContent(cui::designer::NewControl<ComboBox>(L"", 20, 314, 960, 30));
	_setterList->MaxDropDownHeight = 280.0f;
	auto propertyLabel = addContent(cui::designer::NewControl<Label>(L"属性", 20, 356));
	propertyLabel->Width = 70.0f;
	propertyLabel->Height = 24.0f;
	_setterProperty = addContent(cui::designer::NewControl<ComboBox>(L"", 95, 350, 240, 30));
	_setterProperty->MaxDropDownHeight = 336.0f;
	auto modeLabel = addContent(cui::designer::NewControl<Label>(L"来源", 350, 356));
	modeLabel->Width = 55.0f;
	modeLabel->Height = 24.0f;
	_setterMode = addContent(cui::designer::NewControl<ComboBox>(L"", 410, 350, 125, 30));
	std::vector<std::wstring> setterModes{ L"Literal", L"Resource" };
	cui::designer::SetComboBoxItems(*_setterMode, std::move(setterModes));
	auto kindLabel = addContent(cui::designer::NewControl<Label>(L"类型", 550, 356));
	kindLabel->Width = 55.0f;
	kindLabel->Height = 24.0f;
	_setterKind = addContent(cui::designer::NewControl<ComboBox>(L"", 610, 350, 130, 30));
	auto setterKinds = DesignerStyleSheetUtils::ValueKindNames();
	cui::designer::SetComboBoxItems(*_setterKind, std::move(setterKinds));
	_setterKind->MaxDropDownHeight = 280.0f;
	auto valueLabel = addContent(cui::designer::NewControl<Label>(L"值/资源键", 750, 356));
	valueLabel->Width = 90.0f;
	valueLabel->Height = 24.0f;
	_setterValue = addContent(cui::designer::NewControl<TextBox>(L"", 840, 350, 140, 30));
	auto saveSetter = addContent(cui::designer::NewControl<Button>(L"保存 Setter", 20, 394, 130, 32));
	auto removeSetter = addContent(cui::designer::NewControl<Button>(L"删除 Setter", 162, 394, 130, 32));

	_validation = addContent(cui::designer::NewControl<Label>(L"", 315, 400));
	_validation->Width = 665.0f;
	_validation->Height = 40.0f;
	auto summaryLabel = addContent(cui::designer::NewControl<Label>(L"样式表摘要", 20, 448));
	summaryLabel->Width = 960.0f;
	summaryLabel->Height = 24.0f;
	_summary = addContent(cui::designer::NewControl<RichTextBox>(L"", 20, 476, 960, 150));
	_summary->IsReadOnly = true;
	_summary->Background = Colors::White;
	_summary->BorderBrush = Colors::White;

	auto ok = addContent(cui::designer::NewControl<Button>(L"确定", 20, 646, 120, 36));
	auto cancel = addContent(cui::designer::NewControl<Button>(L"取消", 152, 646, 120, 36));

	_resourceList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedResource();
	};
	_ruleList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedRule();
	};
	_ruleType->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshSetterPropertyCatalog(_setterProperty->Text);
	};
	_setterList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedSetter();
	};
	_setterProperty->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) ApplySelectedPropertyMetadata(true);
	};
	_setterMode->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshSetterMode(true);
	};
	saveResource->Click += [this](Control*, RoutedEventArgs&) { (void)SaveResource(); };
	removeResource->Click += [this](Control*, RoutedEventArgs&) { RemoveResource(); };
	saveRule->Click += [this](Control*, RoutedEventArgs&) { (void)SaveRule(); };
	removeRule->Click += [this](Control*, RoutedEventArgs&) { RemoveRule(); };
	saveSetter->Click += [this](Control*, RoutedEventArgs&) { (void)SaveSetter(); };
	removeSetter->Click += [this](Control*, RoutedEventArgs&) { RemoveSetter(); };
	ok->Click += [this](Control*, RoutedEventArgs&) {
		DesignerStyleSheetUtils::Canonicalize(ResultStyleSheet);
		std::wstring error;
		if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			ResultStyleSheet,
			&error,
			_resourceBasePath))
		{
			ShowValidation(error, true);
			return;
		}
		Applied = true;
		this->Close();
	};
	cancel->Click += [this](Control*, RoutedEventArgs&) {
		Applied = false;
		this->Close();
	};

	RefreshResourceList();
	LoadSelectedResource();
	RefreshRuleList();
	LoadSelectedRule();
	RefreshSummary();
	ShowValidation(L"修改会暂存到此窗口；点击“确定”后应用并实时预览。", false);
}

void StyleSheetEditorDialog::SelectComboIndex(ComboBox* combo, int index)
{
	if (!combo) return;
	if (combo->ItemCount() == 0)
	{
		combo->SelectedIndex = -1;
		combo->Text.clear();
		return;
	}
	index = (std::max)(0,
		(std::min)(index, static_cast<int>(combo->ItemCount()) - 1));
	combo->SelectedIndex = index;
	combo->Text = cui::designer::ComboBoxItemText(
		*combo, static_cast<size_t>(index));
}

int StyleSheetEditorDialog::SelectedResourceIndex() const
{
	return _resourceList ? _resourceList->SelectedIndex - 1 : -1;
}

int StyleSheetEditorDialog::SelectedRuleIndex() const
{
	return _ruleList ? _ruleList->SelectedIndex - 1 : -1;
}

int StyleSheetEditorDialog::SelectedSetterIndex() const
{
	return _setterList ? _setterList->SelectedIndex - 1 : -1;
}

void StyleSheetEditorDialog::RefreshResourceList(int preferredIndex)
{
	_loading = true;
	std::vector<std::wstring> items{ kNewResource };
	items.reserve(ResultStyleSheet.Resources.size() + 1);
	for (const auto& resource : ResultStyleSheet.Resources)
		items.push_back(resource.SourceDictionary.empty()
			? resource.Key : L"[外部] " + resource.Key);
	cui::designer::SetComboBoxItems(*_resourceList, std::move(items));
	SelectComboIndex(_resourceList, preferredIndex >= 0 ? preferredIndex + 1 : 0);
	_loading = false;
}

void StyleSheetEditorDialog::LoadSelectedResource()
{
	_loading = true;
	const int index = SelectedResourceIndex();
	const auto* resource = index >= 0 && index < static_cast<int>(ResultStyleSheet.Resources.size())
		? &ResultStyleSheet.Resources[static_cast<size_t>(index)] : nullptr;
	_resourceKey->Text = resource ? resource->Key : L"";
	auto kind = resource ? resource->Value.Kind : DesignerStyleValueKind::Color;
	auto kindName = DesignerStyleSheetUtils::ValueKindName(kind);
	const int kindIndex = cui::designer::FindComboBoxItem(
		*_resourceKind, kindName);
	SelectComboIndex(_resourceKind, kindIndex < 0 ? 0 : kindIndex);
	_resourceValue->Text = resource ? resource->Value.Text : L"#FF0078D4";
	_loading = false;
}

void StyleSheetEditorDialog::RefreshRuleList(int preferredIndex)
{
	_loading = true;
	std::vector<std::wstring> items{ kNewRule };
	items.reserve(ResultStyleSheet.Rules.size() + 1);
	for (size_t index = 0; index < ResultStyleSheet.Rules.size(); ++index)
	{
		const auto& rule = ResultStyleSheet.Rules[index];
		auto caption = RuleCaption(rule, index);
		if (!rule.SourceDictionary.empty()) caption = L"[外部] " + caption;
		items.push_back(std::move(caption));
	}
	cui::designer::SetComboBoxItems(*_ruleList, std::move(items));
	SelectComboIndex(_ruleList, preferredIndex >= 0 ? preferredIndex + 1 : 0);
	_loading = false;
}

void StyleSheetEditorDialog::LoadSelectedRule()
{
	_loading = true;
	const int index = SelectedRuleIndex();
	const auto* rule = index >= 0 && index < static_cast<int>(ResultStyleSheet.Rules.size())
		? &ResultStyleSheet.Rules[static_cast<size_t>(index)] : nullptr;
	const auto typeName = rule && rule->HasType
		? (rule->ComponentType.Empty()
			? DesignerStyleSheetUtils::UIClassName(rule->Type)
			: rule->ComponentType.XamlPrefix + L":"
				+ rule->ComponentType.XamlName)
		: L"Any";
	int typeIndex = cui::designer::FindComboBoxItem(*_ruleType, typeName);
	if (typeIndex < 0 && rule && !rule->ComponentType.Empty())
	{
		cui::designer::AddComboBoxItem(*_ruleType, typeName);
		typeIndex = static_cast<int>(_ruleType->ItemCount()) - 1;
	}
	SelectComboIndex(_ruleType, typeIndex < 0 ? 0 : typeIndex);
	_ruleId->Text = rule ? rule->Id : L"";
	_ruleBasedOn->Text = rule ? rule->BasedOn : L"";
	_loading = false;
	RefreshSetterPropertyCatalog();
	RefreshSetterList();
	LoadSelectedSetter();
}

void StyleSheetEditorDialog::RefreshSetterList(int preferredIndex)
{
	_loading = true;
	std::vector<std::wstring> items{ kNewSetter };
	const int ruleIndex = SelectedRuleIndex();
	if (ruleIndex >= 0 && ruleIndex < static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		const auto& setters = ResultStyleSheet.Rules[
			static_cast<size_t>(ruleIndex)].Setters;
		items.reserve(setters.size() + 1);
		for (const auto& setter : setters)
			items.push_back(setter.PropertyName);
	}
	cui::designer::SetComboBoxItems(*_setterList, std::move(items));
	SelectComboIndex(_setterList, preferredIndex >= 0 ? preferredIndex + 1 : 0);
	_loading = false;
}

void StyleSheetEditorDialog::LoadSelectedSetter()
{
	_loading = true;
	const int ruleIndex = SelectedRuleIndex();
	const int setterIndex = SelectedSetterIndex();
	const DesignerStyleSetter* setter = nullptr;
	if (ruleIndex >= 0 && ruleIndex < static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		const auto& setters = ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)].Setters;
		if (setterIndex >= 0 && setterIndex < static_cast<int>(setters.size()))
			setter = &setters[static_cast<size_t>(setterIndex)];
	}
	const auto propertyName = setter ? setter->PropertyName
		: (_setterProperties.empty() ? std::wstring() : _setterProperties.front().Name);
	auto propertyItems = cui::designer::ComboBoxItems(*_setterProperty);
	auto propertyIt = std::find_if(propertyItems.begin(), propertyItems.end(),
		[&](const std::wstring& item) { return EqualsName(item, propertyName); });
	int propertyIndex = propertyIt == propertyItems.end() ? -1
		: static_cast<int>(propertyIt - propertyItems.begin());
	if (!propertyName.empty() && propertyIndex < 0)
	{
		cui::designer::AddComboBoxItem(*_setterProperty, propertyName);
		propertyIndex = static_cast<int>(_setterProperty->ItemCount()) - 1;
	}
	if (propertyIndex >= 0)
		SelectComboIndex(_setterProperty, propertyIndex);
	else
	{
		_setterProperty->SelectedIndex = -1;
		_setterProperty->Text = propertyName;
	}
	SelectComboIndex(_setterMode, setter && setter->UsesResource ? 1 : 0);
	const auto* property = DesignerPropertyCatalog::Find(
		_setterProperties, propertyName);
	const auto kind = property ? property->ValueKind
		: (setter && !setter->UsesResource
			? setter->Literal.Kind : DesignerStyleValueKind::String);
	const auto kindName = DesignerStyleSheetUtils::ValueKindName(kind);
	const int kindIndex = cui::designer::FindComboBoxItem(
		*_setterKind, kindName);
	SelectComboIndex(_setterKind, kindIndex < 0 ? 0 : kindIndex);
	_setterValue->Text = setter
		? (setter->UsesResource ? setter->ResourceKey : setter->Literal.Text)
		: (property ? property->SampleValue : L"");
	_setterKind->IsEnabled = !property && !(setter && setter->UsesResource);
	_loading = false;
}

void StyleSheetEditorDialog::RefreshSetterPropertyCatalog(
	const std::wstring& preferredProperty)
{
	const auto preserved = preferredProperty.empty()
		? _setterProperty->Text : preferredProperty;
	UIClass type = UIClass::UI_Base;
	const auto typeName = DesignerStyleSheetUtils::Trim(_ruleType->Text);
	if (!EqualsName(typeName, L"Any")
		&& !DesignerStyleSheetUtils::TryParseUIClass(typeName, type))
		type = UIClass::UI_Base;
	else if (EqualsName(typeName, L"Any"))
	{
		const int ruleIndex = SelectedRuleIndex();
		DesignerStyleSheet resolved;
		if (ruleIndex >= 0
			&& DesignerStyleSheetUtils::ResolveInheritance(
				ResultStyleSheet, resolved)
			&& ruleIndex < static_cast<int>(resolved.Rules.size())
			&& resolved.Rules[static_cast<size_t>(ruleIndex)].HasType)
			type = resolved.Rules[static_cast<size_t>(ruleIndex)].Type;
	}

	const auto metadata = CuiRuntime::XamlRuntimeSchema::NativeProperties(type);
	_setterProperties = DesignerPropertyCatalog::GetStyleProperties(metadata);

	_loading = true;
	std::vector<std::wstring> items;
	items.reserve(_setterProperties.size() + 1);
	for (const auto& property : _setterProperties)
		items.push_back(property.Name);
	auto selected = std::find_if(
		items.begin(), items.end(),
		[&](const std::wstring& item) { return EqualsName(item, preserved); });
	int selectedIndex = selected == items.end() ? -1
		: static_cast<int>(selected - items.begin());
	if (!preserved.empty() && selectedIndex < 0)
	{
		items.push_back(preserved);
		selectedIndex = static_cast<int>(items.size()) - 1;
	}
	cui::designer::SetComboBoxItems(
		*_setterProperty, std::move(items));
	if (selectedIndex >= 0)
		SelectComboIndex(_setterProperty, selectedIndex);
	else if (_setterProperty->ItemCount() != 0)
		SelectComboIndex(_setterProperty, 0);
	else
	{
		_setterProperty->SelectedIndex = -1;
		_setterProperty->Text.clear();
	}
	_loading = false;
	ApplySelectedPropertyMetadata(false);
}

void StyleSheetEditorDialog::ApplySelectedPropertyMetadata(bool replaceValue)
{
	const auto* property = DesignerPropertyCatalog::Find(
		_setterProperties, _setterProperty->Text);
	const bool resourceMode = EqualsName(
		DesignerStyleSheetUtils::Trim(_setterMode->Text), L"Resource");
	_setterKind->IsEnabled = !property && !resourceMode;
	if (!property) return;

	const auto kindName = DesignerStyleSheetUtils::ValueKindName(property->ValueKind);
	const int kindIndex = cui::designer::FindComboBoxItem(
		*_setterKind, kindName);
	if (kindIndex >= 0)
	{
		_loading = true;
		SelectComboIndex(_setterKind, kindIndex);
		_loading = false;
	}
	if (replaceValue && !resourceMode)
		_setterValue->Text = property->SampleValue;
}

void StyleSheetEditorDialog::RefreshSetterMode(bool replaceValue)
{
	const bool resourceMode = EqualsName(
		DesignerStyleSheetUtils::Trim(_setterMode->Text), L"Resource");
	ApplySelectedPropertyMetadata(false);
	if (!replaceValue) return;
	if (resourceMode)
	{
		if (!ResultStyleSheet.Resources.empty())
			_setterValue->Text = ResultStyleSheet.Resources.front().Key;
	}
	else
	{
		const auto* property = DesignerPropertyCatalog::Find(
			_setterProperties, _setterProperty->Text);
		if (property) _setterValue->Text = property->SampleValue;
	}
}

void StyleSheetEditorDialog::RefreshSummary()
{
	std::wstring text = L"Resources: " + std::to_wstring(ResultStyleSheet.Resources.size())
		+ L"    Rules: " + std::to_wstring(ResultStyleSheet.Rules.size());
	for (const auto& resource : ResultStyleSheet.Resources)
		text += L"\r\n  @" + resource.Key + L" = "
			+ DesignerStyleSheetUtils::ValueKindName(resource.Value.Kind) + L":" + resource.Value.Text;
	for (size_t index = 0; index < ResultStyleSheet.Rules.size(); ++index)
	{
		const auto& rule = ResultStyleSheet.Rules[index];
		text += L"\r\n  " + RuleCaption(rule, index)
			+ L"  (" + std::to_wstring(rule.Setters.size()) + L" setters, "
			+ std::to_wstring(rule.Triggers.size())
			+ L" triggers)";
		for (const auto& setter : rule.Setters)
			text += L"\r\n    " + setter.PropertyName + L" = "
				+ (setter.UsesResource ? L"@" + setter.ResourceKey
					: DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind)
						+ L":" + setter.Literal.Text);
		for (const auto& trigger : rule.Triggers)
		{
			if (!trigger.DataConditions.empty())
			{
				text += trigger.DataConditions.size() > 1
					? L"\r\n    MultiDataTrigger " : L"\r\n    DataTrigger ";
				for (size_t conditionIndex = 0;
					conditionIndex < trigger.DataConditions.size(); ++conditionIndex)
				{
					if (conditionIndex != 0) text += L" AND ";
					const auto& condition = trigger.DataConditions[conditionIndex];
					text += condition.SourceProperty + L" = " + condition.Value.Text;
				}
			}
			else
			{
				text += trigger.PropertyConditions.size() > 1
					? L"\r\n    MultiTrigger " : L"\r\n    Trigger ";
				bool wroteCondition = false;
				for (const auto& condition : trigger.PropertyConditions)
				{
					if (wroteCondition) text += L" AND ";
					text += condition.Property + L" = " + condition.Value.Text;
					wroteCondition = true;
				}
			}
			for (const auto& setter : trigger.Setters)
				text += L"\r\n      " + setter.PropertyName + L" = "
					+ (setter.UsesResource ? L"@" + setter.ResourceKey
						: DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind)
							+ L":" + setter.Literal.Text);
		}
	}
	_summary->Text = std::move(text);
}

void StyleSheetEditorDialog::ShowValidation(const std::wstring& message, bool isError)
{
	_validation->Text = message;
	_validation->Foreground = isError ? Colors::Red : Colors::DimGrey;
	_validation->InvalidateVisual();
}

bool StyleSheetEditorDialog::SaveResource()
{
	const int selected = SelectedResourceIndex();
	if (selected >= 0 && selected < static_cast<int>(ResultStyleSheet.Resources.size())
		&& !ResultStyleSheet.Resources[static_cast<size_t>(selected)]
			.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典项为只读；请在对应 XAML 文件中编辑。", true);
		return false;
	}
	DesignerStyleResource resource;
	resource.Key = DesignerStyleSheetUtils::Trim(_resourceKey->Text);
	if (resource.Key.empty())
	{
		ShowValidation(L"资源 Key 不能为空。", true);
		return false;
	}
	if (!DesignerStyleSheetUtils::TryParseValueKind(_resourceKind->Text, resource.Value.Kind))
	{
		ShowValidation(L"请选择有效的资源值类型。", true);
		return false;
	}
	resource.Value.Text = _resourceValue->Text;
	BindingValue parsed;
	std::wstring error;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		resource.Value, parsed, &error, _resourceBasePath))
	{
		ShowValidation(error, true);
		return false;
	}

	auto collision = std::find_if(ResultStyleSheet.Resources.begin(), ResultStyleSheet.Resources.end(),
		[&](const DesignerStyleResource& item) { return EqualsName(item.Key, resource.Key); });
	const auto selectedIt = selected >= 0 && selected < static_cast<int>(ResultStyleSheet.Resources.size())
		? ResultStyleSheet.Resources.begin() + selected : ResultStyleSheet.Resources.end();
	if (collision != ResultStyleSheet.Resources.end() && collision != selectedIt)
	{
		ShowValidation(L"资源 Key 已存在：" + resource.Key, true);
		return false;
	}

	int savedIndex = selected;
	if (selectedIt == ResultStyleSheet.Resources.end())
	{
		ResultStyleSheet.Resources.push_back(resource);
		savedIndex = static_cast<int>(ResultStyleSheet.Resources.size()) - 1;
	}
	else
	{
		const auto oldKey = selectedIt->Key;
		*selectedIt = resource;
		if (!EqualsName(oldKey, resource.Key))
		{
		ResourceRenames.emplace_back(oldKey, resource.Key);
		for (auto& rule : ResultStyleSheet.Rules)
		{
			for (auto& setter : rule.Setters)
				if (setter.UsesResource && EqualsName(setter.ResourceKey, oldKey))
					setter.ResourceKey = resource.Key;
			for (auto& trigger : rule.Triggers)
				for (auto& setter : trigger.Setters)
					if (setter.UsesResource && EqualsName(setter.ResourceKey, oldKey))
						setter.ResourceKey = resource.Key;
		}
		}
	}
	RefreshResourceList(savedIndex);
	LoadSelectedResource();
	RefreshSummary();
	ShowValidation(L"资源已暂存。", false);
	return true;
}

void StyleSheetEditorDialog::RemoveResource()
{
	const int selected = SelectedResourceIndex();
	if (selected < 0 || selected >= static_cast<int>(ResultStyleSheet.Resources.size()))
	{
		ShowValidation(L"请选择要删除的资源。", false);
		return;
	}
	if (!ResultStyleSheet.Resources[static_cast<size_t>(selected)]
		.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典项为只读；请在对应 XAML 文件中删除。", true);
		return;
	}
	const auto key = ResultStyleSheet.Resources[static_cast<size_t>(selected)].Key;
	for (const auto& rule : ResultStyleSheet.Rules)
	{
		for (const auto& setter : rule.Setters)
			if (setter.UsesResource && EqualsName(setter.ResourceKey, key))
			{
				ShowValidation(L"资源仍被 Setter 引用：" + key, true);
				return;
			}
		for (const auto& trigger : rule.Triggers)
			for (const auto& setter : trigger.Setters)
				if (setter.UsesResource && EqualsName(setter.ResourceKey, key))
				{
					ShowValidation(L"资源仍被 Trigger Setter 引用：" + key, true);
					return;
				}
	}
	ResultStyleSheet.Resources.erase(ResultStyleSheet.Resources.begin() + selected);
	RefreshResourceList();
	LoadSelectedResource();
	RefreshSummary();
	ShowValidation(L"资源已删除。", false);
}

bool StyleSheetEditorDialog::SaveRule()
{
	DesignerStyleRule rule;
	const int selected = SelectedRuleIndex();
	if (selected >= 0 && selected < static_cast<int>(ResultStyleSheet.Rules.size())
		&& !ResultStyleSheet.Rules[static_cast<size_t>(selected)]
			.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典规则为只读；请在对应 XAML 文件中编辑。", true);
		return false;
	}
	std::wstring previousId;
	if (selected >= 0 && selected < static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		const auto& previous = ResultStyleSheet.Rules[static_cast<size_t>(selected)];
		rule.Setters = previous.Setters;
		rule.Triggers = previous.Triggers;
		previousId = previous.Id;
	}
	const auto typeName = DesignerStyleSheetUtils::Trim(_ruleType->Text);
	if (!EqualsName(typeName, L"Any"))
	{
		rule.HasType = true;
		const DesignerStyleRule* previous = selected >= 0
			&& selected < static_cast<int>(ResultStyleSheet.Rules.size())
			? &ResultStyleSheet.Rules[static_cast<size_t>(selected)] : nullptr;
		const auto previousComponentName = previous && !previous->ComponentType.Empty()
			? previous->ComponentType.XamlPrefix + L":"
				+ previous->ComponentType.XamlName : L"";
		if (previous && EqualsName(typeName, previousComponentName))
		{
			rule.Type = previous->Type;
			rule.ComponentType = previous->ComponentType;
		}
		else if (!DesignerStyleSheetUtils::TryParseUIClass(typeName, rule.Type))
		{
			ShowValidation(L"请选择有效的控件类型。", true);
			return false;
		}
	}
	rule.Id = DesignerStyleSheetUtils::Trim(_ruleId->Text);
	rule.BasedOn = DesignerStyleSheetUtils::Trim(_ruleBasedOn->Text);

	int savedIndex = selected;
	if (selected < 0 || selected >= static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		ResultStyleSheet.Rules.push_back(std::move(rule));
		savedIndex = static_cast<int>(ResultStyleSheet.Rules.size()) - 1;
	}
	else ResultStyleSheet.Rules[static_cast<size_t>(selected)] = std::move(rule);
	const auto& savedId = ResultStyleSheet.Rules[static_cast<size_t>(savedIndex)].Id;
	if (!previousId.empty() && !EqualsName(previousId, savedId))
	{
		if (savedId.empty())
		{
			const bool referenced = std::any_of(
				ResultStyleSheet.Rules.begin(), ResultStyleSheet.Rules.end(),
				[&](const DesignerStyleRule& current)
				{
					return EqualsName(current.BasedOn, previousId);
				});
			if (referenced)
			{
				ResultStyleSheet.Rules[static_cast<size_t>(savedIndex)].Id = previousId;
				ShowValidation(L"该样式仍被 BasedOn 引用，不能移除其 x:Key。", true);
				return false;
			}
		}
		else
			for (auto& current : ResultStyleSheet.Rules)
				if (EqualsName(current.BasedOn, previousId)) current.BasedOn = savedId;
	}
	RefreshRuleList(savedIndex);
	LoadSelectedRule();
	RefreshSummary();
	ShowValidation(L"规则已暂存；请至少添加一个 Setter。", false);
	return true;
}

void StyleSheetEditorDialog::RemoveRule()
{
	const int selected = SelectedRuleIndex();
	if (selected < 0 || selected >= static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		ShowValidation(L"请选择要删除的规则。", false);
		return;
	}
	if (!ResultStyleSheet.Rules[static_cast<size_t>(selected)]
		.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典规则为只读；请在对应 XAML 文件中删除。", true);
		return;
	}
	const auto removedId = ResultStyleSheet.Rules[static_cast<size_t>(selected)].Id;
	if (!removedId.empty()
		&& std::any_of(ResultStyleSheet.Rules.begin(), ResultStyleSheet.Rules.end(),
			[&](const DesignerStyleRule& rule)
			{
				return &rule != &ResultStyleSheet.Rules[static_cast<size_t>(selected)]
					&& EqualsName(rule.BasedOn, removedId);
			}))
	{
		ShowValidation(L"该样式仍被 BasedOn 引用：" + removedId, true);
		return;
	}
	ResultStyleSheet.Rules.erase(ResultStyleSheet.Rules.begin() + selected);
	RefreshRuleList();
	LoadSelectedRule();
	RefreshSummary();
	ShowValidation(L"规则已删除。", false);
}

bool StyleSheetEditorDialog::SaveSetter()
{
	const int ruleIndex = SelectedRuleIndex();
	if (ruleIndex < 0 || ruleIndex >= static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		ShowValidation(L"请先保存并选择一条规则。", true);
		return false;
	}
	if (!ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)]
		.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典规则为只读；请在对应 XAML 文件中编辑 Setter。", true);
		return false;
	}
	DesignerStyleSetter setter;
	setter.PropertyName = DesignerStyleSheetUtils::Trim(_setterProperty->Text);
	if (setter.PropertyName.empty())
	{
		ShowValidation(L"Setter 属性名不能为空。", true);
		return false;
	}
	const auto* property = DesignerPropertyCatalog::Find(
		_setterProperties, setter.PropertyName);
	if (!property || !property->Metadata)
	{
		ShowValidation(L"请选择目标类型元数据中可写且受支持的属性。", true);
		return false;
	}
	setter.UsesResource = EqualsName(DesignerStyleSheetUtils::Trim(_setterMode->Text), L"Resource");
	if (setter.UsesResource)
	{
		setter.ResourceKey = DesignerStyleSheetUtils::Trim(_setterValue->Text);
		const auto exists = std::any_of(ResultStyleSheet.Resources.begin(), ResultStyleSheet.Resources.end(),
			[&](const DesignerStyleResource& resource) { return EqualsName(resource.Key, setter.ResourceKey); });
		if (!exists)
		{
			ShowValidation(L"资源不存在：" + setter.ResourceKey, true);
			return false;
		}
		const auto resource = std::find_if(
			ResultStyleSheet.Resources.begin(), ResultStyleSheet.Resources.end(),
			[&](const DesignerStyleResource& item)
			{
				return EqualsName(item.Key, setter.ResourceKey);
			});
		std::wstring error;
		DesignerStyleValue canonicalResource;
		if (resource == ResultStyleSheet.Resources.end()
			|| !DesignerPropertyCatalog::NormalizeStyleValue(
				*property->Metadata, resource->Value, canonicalResource,
				&error, _resourceBasePath))
		{
			ShowValidation(error.empty() ? L"资源与属性类型不兼容。" : error, true);
			return false;
		}
	}
	else
	{
		setter.Literal.Kind = property->ValueKind;
		setter.Literal.Text = _setterValue->Text;
		std::wstring error;
		DesignerStyleValue canonical;
		if (!DesignerPropertyCatalog::NormalizeStyleValue(
			*property->Metadata, setter.Literal, canonical, &error,
			_resourceBasePath))
		{
			ShowValidation(error, true);
			return false;
		}
		setter.Literal = std::move(canonical);
	}

	auto& setters = ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)].Setters;
	const int selected = SelectedSetterIndex();
	auto collision = std::find_if(setters.begin(), setters.end(),
		[&](const DesignerStyleSetter& item) { return EqualsName(item.PropertyName, setter.PropertyName); });
	const auto selectedIt = selected >= 0 && selected < static_cast<int>(setters.size())
		? setters.begin() + selected : setters.end();
	if (collision != setters.end() && collision != selectedIt)
	{
		ShowValidation(L"当前规则已包含属性：" + setter.PropertyName, true);
		return false;
	}
	int savedIndex = selected;
	if (selectedIt == setters.end())
	{
		setters.push_back(std::move(setter));
		savedIndex = static_cast<int>(setters.size()) - 1;
	}
	else *selectedIt = std::move(setter);
	RefreshSetterList(savedIndex);
	LoadSelectedSetter();
	RefreshSummary();
	ShowValidation(L"Setter 已暂存。", false);
	return true;
}

void StyleSheetEditorDialog::RemoveSetter()
{
	const int ruleIndex = SelectedRuleIndex();
	const int setterIndex = SelectedSetterIndex();
	if (ruleIndex < 0 || ruleIndex >= static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		ShowValidation(L"请选择规则。", false);
		return;
	}
	if (!ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)]
		.SourceDictionary.empty())
	{
		ShowValidation(L"外部资源字典规则为只读；请在对应 XAML 文件中删除 Setter。", true);
		return;
	}
	auto& setters = ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)].Setters;
	if (setterIndex < 0 || setterIndex >= static_cast<int>(setters.size()))
	{
		ShowValidation(L"请选择要删除的 Setter。", false);
		return;
	}
	setters.erase(setters.begin() + setterIndex);
	RefreshSetterList();
	LoadSelectedSetter();
	RefreshSummary();
	ShowValidation(L"Setter 已删除。", false);
}

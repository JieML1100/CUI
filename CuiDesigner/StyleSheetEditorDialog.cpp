#include "StyleSheetEditorDialog.h"
#include "DesignerControlFactory.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include <algorithm>

namespace
{
	const std::wstring kNewResource = L"<新增资源>";
	const std::wstring kNewRule = L"<新增规则>";
	const std::wstring kNewSetter = L"<新增 Setter>";

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::wstring RuleCaption(const DesignerStyleRule& rule, size_t index)
	{
		std::wstring selector;
		if (rule.HasType) selector = DesignerStyleSheetUtils::UIClassName(rule.Type);
		if (!rule.Id.empty())
		{
			if (!selector.empty()) selector += L" ";
			selector += L"#";
			selector += rule.Id;
		}
		if (!rule.BasedOn.empty()) selector += L" <- @" + rule.BasedOn;
		for (const auto& styleClass : rule.Classes)
		{
			if (!selector.empty()) selector += L" ";
			selector += L".";
			selector += styleClass;
		}
		if (!DesignerStyleSheetUtils::FormatStates(rule.RequiredStates).empty())
			selector += L" :" + DesignerStyleSheetUtils::FormatStates(rule.RequiredStates);
		if (selector.empty()) selector = L"*";
		return L"规则 " + std::to_wstring(index + 1) + L"  " + selector;
	}
}

StyleSheetEditorDialog::StyleSheetEditorDialog(
	const DesignerStyleSheet& styleSheet,
	std::wstring resourceBasePath)
	: Form(L"编辑文档样式表", POINT{ 250, 100 }, SIZE{ 1020, 760 }),
	  ResultStyleSheet(styleSheet),
	  _resourceBasePath(std::move(resourceBasePath))
{
	DesignerStyleSheetUtils::Canonicalize(ResultStyleSheet);
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->AllowResize = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(
		L"资源与 Setter 使用强类型值；Trigger/MultiTrigger/DataTrigger/MultiDataTrigger 由 XAML 编辑器维护并在摘要中展示。",
		20, 12));
	tip->Size = { 970, 24 };

	// Resources
	auto resourcesTitle = this->AddControl(new Label(L"资源", 20, 48));
	resourcesTitle->Size = { 450, 24 };
	_resourceList = this->AddControl(new ComboBox(L"", 20, 76, 450, 30));
	_resourceList->ExpandCount = 10;
	auto resourceKeyLabel = this->AddControl(new Label(L"Key", 20, 118));
	resourceKeyLabel->Size = { 80, 24 };
	_resourceKey = this->AddControl(new TextBox(L"", 105, 112, 365, 30));
	auto resourceKindLabel = this->AddControl(new Label(L"类型", 20, 158));
	resourceKindLabel->Size = { 80, 24 };
	_resourceKind = this->AddControl(new ComboBox(L"", 105, 152, 150, 30));
	auto resourceKinds = DesignerStyleSheetUtils::ValueKindNames();
	_resourceKind->Items = resourceKinds;
	_resourceKind->ExpandCount = 10;
	auto resourceValueLabel = this->AddControl(new Label(L"值", 270, 158));
	resourceValueLabel->Size = { 40, 24 };
	_resourceValue = this->AddControl(new TextBox(L"", 310, 152, 160, 30));
	auto saveResource = this->AddControl(new Button(L"保存资源", 20, 196, 120, 32));
	auto removeResource = this->AddControl(new Button(L"删除资源", 152, 196, 120, 32));

	// Rule selector
	auto rulesTitle = this->AddControl(new Label(L"规则选择器", 500, 48));
	rulesTitle->Size = { 480, 24 };
	_ruleList = this->AddControl(new ComboBox(L"", 500, 76, 480, 30));
	_ruleList->ExpandCount = 10;
	auto typeLabel = this->AddControl(new Label(L"类型", 500, 118));
	typeLabel->Size = { 70, 24 };
	_ruleType = this->AddControl(new ComboBox(L"", 575, 112, 160, 30));
	std::vector<std::wstring> ruleTypes{ L"Any", L"Base" };
	for (const auto& control : ControlRegistry::GetAvailableControls())
		ruleTypes.push_back(control.Name);
	_ruleType->Items = ruleTypes;
	_ruleType->ExpandCount = 12;
	auto idLabel = this->AddControl(new Label(L"StyleId", 748, 118));
	idLabel->Size = { 70, 24 };
	_ruleId = this->AddControl(new TextBox(L"", 820, 112, 160, 30));
	auto basedOnLabel = this->AddControl(new Label(L"BasedOn", 500, 158));
	basedOnLabel->Size = { 70, 24 };
	_ruleBasedOn = this->AddControl(new TextBox(L"", 575, 152, 160, 30));
	auto classesLabel = this->AddControl(new Label(L"Classes", 748, 158));
	classesLabel->Size = { 70, 24 };
	_ruleClasses = this->AddControl(new TextBox(L"", 820, 152, 160, 30));
	auto requiredLabel = this->AddControl(new Label(L"必需状态", 500, 198));
	requiredLabel->Size = { 85, 24 };
	_requiredStates = this->AddControl(new TextBox(L"", 590, 192, 175, 30));
	auto excludedLabel = this->AddControl(new Label(L"排除状态", 775, 198));
	excludedLabel->Size = { 85, 24 };
	_excludedStates = this->AddControl(new TextBox(L"", 865, 192, 115, 30));
	auto saveRule = this->AddControl(new Button(L"保存规则", 500, 236, 120, 32));
	auto removeRule = this->AddControl(new Button(L"删除规则", 632, 236, 120, 32));

	// Setter editor
	auto setterTitle = this->AddControl(new Label(L"当前规则的 Setter", 20, 286));
	setterTitle->Size = { 960, 24 };
	_setterList = this->AddControl(new ComboBox(L"", 20, 314, 960, 30));
	_setterList->ExpandCount = 10;
	auto propertyLabel = this->AddControl(new Label(L"属性", 20, 356));
	propertyLabel->Size = { 70, 24 };
	_setterProperty = this->AddControl(new ComboBox(L"", 95, 350, 240, 30));
	_setterProperty->ExpandCount = 12;
	auto modeLabel = this->AddControl(new Label(L"来源", 350, 356));
	modeLabel->Size = { 55, 24 };
	_setterMode = this->AddControl(new ComboBox(L"", 410, 350, 125, 30));
	std::vector<std::wstring> setterModes{ L"Literal", L"Resource" };
	_setterMode->Items = setterModes;
	auto kindLabel = this->AddControl(new Label(L"类型", 550, 356));
	kindLabel->Size = { 55, 24 };
	_setterKind = this->AddControl(new ComboBox(L"", 610, 350, 130, 30));
	auto setterKinds = DesignerStyleSheetUtils::ValueKindNames();
	_setterKind->Items = setterKinds;
	_setterKind->ExpandCount = 10;
	auto valueLabel = this->AddControl(new Label(L"值/资源键", 750, 356));
	valueLabel->Size = { 90, 24 };
	_setterValue = this->AddControl(new TextBox(L"", 840, 350, 140, 30));
	auto saveSetter = this->AddControl(new Button(L"保存 Setter", 20, 394, 130, 32));
	auto removeSetter = this->AddControl(new Button(L"删除 Setter", 162, 394, 130, 32));

	_validation = this->AddControl(new Label(L"", 315, 400));
	_validation->Size = { 665, 40 };
	auto summaryLabel = this->AddControl(new Label(L"样式表摘要", 20, 448));
	summaryLabel->Size = { 960, 24 };
	_summary = this->AddControl(new RichTextBox(L"", 20, 476, 960, 150));
	_summary->ReadOnly = true;
	_summary->AllowMultiLine = true;
	_summary->BackColor = Colors::White;
	_summary->FocusedColor = Colors::White;

	auto ok = this->AddControl(new Button(L"确定", 20, 646, 120, 36));
	auto cancel = this->AddControl(new Button(L"取消", 152, 646, 120, 36));

	_resourceList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedResource();
	};
	_ruleList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedRule();
	};
	_ruleType->OnSelectionChanged += [this](Control*) {
		if (!_loading) RefreshSetterPropertyCatalog(_setterProperty->Text);
	};
	_setterList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedSetter();
	};
	_setterProperty->OnSelectionChanged += [this](Control*) {
		if (!_loading) ApplySelectedPropertyMetadata(true);
	};
	_setterMode->OnSelectionChanged += [this](Control*) {
		if (!_loading) RefreshSetterMode(true);
	};
	saveResource->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveResource(); };
	removeResource->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveResource(); };
	saveRule->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveRule(); };
	removeRule->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveRule(); };
	saveSetter->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveSetter(); };
	removeSetter->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveSetter(); };
	ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		DesignerStyleSheetUtils::Canonicalize(ResultStyleSheet);
		std::wstring error;
		if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			ResultStyleSheet,
			[](UIClass type) { return DesignerControlFactory::Create(type); },
			&error,
			_resourceBasePath))
		{
			ShowValidation(error, true);
			return;
		}
		Applied = true;
		this->Close();
	};
	cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
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
	if (combo->Items.empty())
	{
		combo->SelectedIndex = -1;
		combo->Text.clear();
		return;
	}
	index = (std::max)(0, (std::min)(index, static_cast<int>(combo->Items.size()) - 1));
	combo->SelectedIndex = index;
	combo->Text = combo->Items[static_cast<size_t>(index)];
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
	_resourceList->Items.clear();
	_resourceList->Items.push_back(kNewResource);
	for (const auto& resource : ResultStyleSheet.Resources)
		_resourceList->Items.push_back(resource.SourceDictionary.empty()
			? resource.Key : L"[外部] " + resource.Key);
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
	auto kindIt = std::find(_resourceKind->Items.begin(), _resourceKind->Items.end(), kindName);
	SelectComboIndex(_resourceKind, kindIt == _resourceKind->Items.end()
		? 0 : static_cast<int>(kindIt - _resourceKind->Items.begin()));
	_resourceValue->Text = resource ? resource->Value.Text : L"#FF0078D4";
	_loading = false;
}

void StyleSheetEditorDialog::RefreshRuleList(int preferredIndex)
{
	_loading = true;
	_ruleList->Items.clear();
	_ruleList->Items.push_back(kNewRule);
	for (size_t index = 0; index < ResultStyleSheet.Rules.size(); ++index)
	{
		const auto& rule = ResultStyleSheet.Rules[index];
		auto caption = RuleCaption(rule, index);
		if (!rule.SourceDictionary.empty()) caption = L"[外部] " + caption;
		_ruleList->Items.push_back(std::move(caption));
	}
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
		? DesignerStyleSheetUtils::UIClassName(rule->Type) : L"Any";
	auto typeIt = std::find(_ruleType->Items.begin(), _ruleType->Items.end(), typeName);
	SelectComboIndex(_ruleType, typeIt == _ruleType->Items.end()
		? 0 : static_cast<int>(typeIt - _ruleType->Items.begin()));
	_ruleId->Text = rule ? rule->Id : L"";
	_ruleBasedOn->Text = rule ? rule->BasedOn : L"";
	_ruleClasses->Text = rule ? DesignerStyleSheetUtils::JoinClasses(rule->Classes) : L"";
	_requiredStates->Text = rule ? DesignerStyleSheetUtils::FormatStates(rule->RequiredStates) : L"";
	_excludedStates->Text = rule ? DesignerStyleSheetUtils::FormatStates(rule->ExcludedStates) : L"";
	_loading = false;
	RefreshSetterPropertyCatalog();
	RefreshSetterList();
	LoadSelectedSetter();
}

void StyleSheetEditorDialog::RefreshSetterList(int preferredIndex)
{
	_loading = true;
	_setterList->Items.clear();
	_setterList->Items.push_back(kNewSetter);
	const int ruleIndex = SelectedRuleIndex();
	if (ruleIndex >= 0 && ruleIndex < static_cast<int>(ResultStyleSheet.Rules.size()))
	{
		for (const auto& setter : ResultStyleSheet.Rules[static_cast<size_t>(ruleIndex)].Setters)
			_setterList->Items.push_back(setter.PropertyName);
	}
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
	auto propertyIt = std::find_if(_setterProperty->Items.begin(), _setterProperty->Items.end(),
		[&](const std::wstring& item) { return EqualsName(item, propertyName); });
	if (!propertyName.empty() && propertyIt == _setterProperty->Items.end())
	{
		_setterProperty->Items.push_back(propertyName);
		propertyIt = _setterProperty->Items.end() - 1;
	}
	if (propertyIt != _setterProperty->Items.end())
		SelectComboIndex(_setterProperty,
			static_cast<int>(propertyIt - _setterProperty->Items.begin()));
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
	auto kindIt = std::find(_setterKind->Items.begin(), _setterKind->Items.end(), kindName);
	SelectComboIndex(_setterKind, kindIt == _setterKind->Items.end()
		? 0 : static_cast<int>(kindIt - _setterKind->Items.begin()));
	_setterValue->Text = setter
		? (setter->UsesResource ? setter->ResourceKey : setter->Literal.Text)
		: (property ? property->SampleValue : L"");
	_setterKind->Enable = !property && !(setter && setter->UsesResource);
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

	_propertyProbe = DesignerControlFactory::Create(type);
	_setterProperties = _propertyProbe
		? DesignerPropertyCatalog::GetStyleProperties(*_propertyProbe)
		: std::vector<DesignerPropertyDescriptor>{};

	_loading = true;
	_setterProperty->Items.clear();
	for (const auto& property : _setterProperties)
		_setterProperty->Items.push_back(property.Name);
	auto selected = std::find_if(
		_setterProperty->Items.begin(), _setterProperty->Items.end(),
		[&](const std::wstring& item) { return EqualsName(item, preserved); });
	if (!preserved.empty() && selected == _setterProperty->Items.end())
	{
		_setterProperty->Items.push_back(preserved);
		selected = _setterProperty->Items.end() - 1;
	}
	if (selected != _setterProperty->Items.end())
		SelectComboIndex(_setterProperty,
			static_cast<int>(selected - _setterProperty->Items.begin()));
	else if (!_setterProperty->Items.empty())
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
	_setterKind->Enable = !property && !resourceMode;
	if (!property) return;

	const auto kindName = DesignerStyleSheetUtils::ValueKindName(property->ValueKind);
	const auto kind = std::find(_setterKind->Items.begin(), _setterKind->Items.end(), kindName);
	if (kind != _setterKind->Items.end())
	{
		_loading = true;
		SelectComboIndex(_setterKind,
			static_cast<int>(kind - _setterKind->Items.begin()));
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
			+ std::to_wstring(rule.Triggers.size()
				+ (rule.DataConditions.empty() ? 0u : 1u))
			+ L" triggers)";
		if (!rule.DataConditions.empty())
		{
			text += rule.DataConditions.size() > 1
				? L"\r\n    MultiDataTrigger " : L"\r\n    DataTrigger ";
			for (size_t conditionIndex = 0;
				conditionIndex < rule.DataConditions.size(); ++conditionIndex)
			{
				if (conditionIndex != 0) text += L" AND ";
				const auto& condition = rule.DataConditions[conditionIndex];
				text += condition.SourceProperty + L" = " + condition.Value.Text;
			}
		}
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
				text += trigger.Conditions.size() > 1
					? L"\r\n    MultiTrigger " : L"\r\n    Trigger ";
				for (size_t conditionIndex = 0;
					conditionIndex < trigger.Conditions.size(); ++conditionIndex)
				{
					if (conditionIndex != 0) text += L" AND ";
					const auto& condition = trigger.Conditions[conditionIndex];
					text += condition.Property + L" = "
						+ (condition.Value ? L"true" : L"false");
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
	_validation->ForeColor = isError ? Colors::Red : Colors::DimGrey;
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
		rule.DataConditions = previous.DataConditions;
		rule.Triggers = previous.Triggers;
		previousId = previous.Id;
	}
	const auto typeName = DesignerStyleSheetUtils::Trim(_ruleType->Text);
	if (!EqualsName(typeName, L"Any"))
	{
		rule.HasType = true;
		if (!DesignerStyleSheetUtils::TryParseUIClass(typeName, rule.Type))
		{
			ShowValidation(L"请选择有效的控件类型。", true);
			return false;
		}
	}
	rule.Id = DesignerStyleSheetUtils::Trim(_ruleId->Text);
	rule.BasedOn = DesignerStyleSheetUtils::Trim(_ruleBasedOn->Text);
	rule.Classes = DesignerStyleSheetUtils::SplitClasses(_ruleClasses->Text);
	if (!DesignerStyleSheetUtils::TryParseStates(_requiredStates->Text, rule.RequiredStates)
		|| !DesignerStyleSheetUtils::TryParseStates(_excludedStates->Text, rule.ExcludedStates))
	{
		ShowValidation(L"状态名称无效；可用 Hovered、Focused、Pressed、Disabled、Checked、Selected。", true);
		return false;
	}
	if ((rule.RequiredStates & rule.ExcludedStates) != ControlStyleState::None)
	{
		ShowValidation(L"同一状态不能同时为必需和排除。", true);
		return false;
	}

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
				ShowValidation(L"该样式仍被 BasedOn 引用，不能移除其 StyleId。", true);
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
	if (!property || !_propertyProbe)
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
		if (resource == ResultStyleSheet.Resources.end()
			|| !DesignerPropertyCatalog::ValidateStyleValue(
				*_propertyProbe, setter.PropertyName, resource->Value, &error,
				_resourceBasePath))
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
		if (!DesignerPropertyCatalog::ValidateStyleValue(
			*_propertyProbe, setter.PropertyName, setter.Literal, &error,
			_resourceBasePath))
		{
			ShowValidation(error, true);
			return false;
		}
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

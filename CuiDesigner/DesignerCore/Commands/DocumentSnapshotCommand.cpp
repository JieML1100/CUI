#include "DocumentSnapshotCommand.h"
#include "DesignNodeMemory.h"
#include "../../DesignerCanvas.h"

namespace
{
	constexpr auto MergeWindow = std::chrono::milliseconds(1000);

	size_t WideStringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t NarrowStringMemory(const std::string& value) noexcept
	{
		return sizeof(std::string)
			+ value.capacity() * sizeof(std::string::value_type);
	}

	size_t DesignValueMemory(
		const DesignerModel::DesignValue& value) noexcept
	{
		size_t result = sizeof(value);
		if (value.is_string())
		{
			result += value.size();
		}
		else if (value.is_array())
		{
			const auto& items = value.ArrayItems();
			result += items.capacity() * sizeof(DesignerModel::DesignValue);
			for (const auto& item : items) result += DesignValueMemory(item);
		}
		else if (value.is_object())
		{
			for (const auto& [key, item] : value.ObjectItems())
			{
				result += sizeof(std::pair<const std::string,
					DesignerModel::DesignValue>) + 3 * sizeof(void*);
				result += key.capacity();
				result += DesignValueMemory(item);
			}
		}
		return result;
	}

	size_t StyleValueMemory(const DesignerStyleValue& value) noexcept
	{
		return sizeof(value) + WideStringMemory(value.Text);
	}

	size_t DocumentMemory(
		const DesignerModel::DesignDocument& document) noexcept
	{
		size_t result = sizeof(document)
			+ NarrowStringMemory(document.Schema)
			+ WideStringMemory(document.Window.Name)
			+ WideStringMemory(document.Window.ParentRef)
			+ WideStringMemory(document.Window.XamlType.NamespaceUri)
			+ WideStringMemory(document.Window.XamlType.LocalName)
			+ DesignValueMemory(EncodeDesignNodeProperties(
				document.Window.Properties))
			+ DesignerCommandMemory::StructureHeap(document.Window.Structure)
			+ DesignerCommandMemory::TemplateStateHeap(
				document.Window.TemplateState)
			+ DesignValueMemory(EncodeDesignNodeEvents(document.Window.Events))
			+ DesignValueMemory(EncodeDesignNodeBindings(
				document.Window.Bindings));

		result += document.DataContextSchema.capacity()
			* sizeof(DesignerDataContextProperty);
		for (const auto& property : document.DataContextSchema)
			result += WideStringMemory(property.Path)
				+ WideStringMemory(property.ItemType);

		result += document.StyleSheet.Resources.capacity()
			* sizeof(DesignerStyleResource);
		result += document.StyleSheet.MergedDictionaries.capacity()
			* sizeof(std::wstring);
		for (const auto& dictionary : document.StyleSheet.MergedDictionaries)
			result += WideStringMemory(dictionary);
		for (const auto& resource : document.StyleSheet.Resources)
			result += WideStringMemory(resource.Key)
				+ WideStringMemory(resource.SourceDictionary)
				+ StyleValueMemory(resource.Value);

		result += document.Components.capacity()
			* sizeof(DesignerModel::DesignComponentDefinition);
		for (const auto& component : document.Components)
		{
			result += WideStringMemory(component.Type.XamlPrefix)
				+ WideStringMemory(component.Type.XamlName)
				+ WideStringMemory(component.Type.XamlNamespace)
				+ WideStringMemory(component.DisplayName)
				+ WideStringMemory(component.Category)
				+ WideStringMemory(component.SourceDictionary)
				+ component.Properties.capacity()
					* sizeof(DesignerComponentPropertyDescriptor)
				+ component.ContentProperties.capacity()
					* sizeof(DesignerComponentContentPropertyDescriptor)
				+ component.Events.capacity()
					* sizeof(DesignerComponentEventDescriptor)
				+ component.VisualStateGroups.capacity()
					* sizeof(DesignerVisualStateGroup)
				+ component.EventTriggers.capacity()
					* sizeof(DesignerEventTrigger)
				+ component.Template.capacity()
					* sizeof(DesignerModel::DesignNode);
			for (const auto& property : component.Properties)
			{
				result += WideStringMemory(property.Name)
					+ WideStringMemory(property.DisplayName)
					+ WideStringMemory(property.Category)
					+ WideStringMemory(property.DefaultResourceKey)
					+ StyleValueMemory(property.DefaultValue)
					+ property.Choices.capacity()
						* sizeof(DesignerComponentPropertyChoice);
				for (const auto& choice : property.Choices)
					result += WideStringMemory(choice.Value)
						+ WideStringMemory(choice.DisplayName);
			}
			for (const auto& property : component.ContentProperties)
				result += WideStringMemory(property.Name)
					+ WideStringMemory(property.DisplayName);
			for (const auto& event : component.Events)
				result += WideStringMemory(event.Name)
					+ WideStringMemory(event.DisplayName);
			for (const auto& group : component.VisualStateGroups)
			{
				result += WideStringMemory(group.Name)
					+ group.States.capacity() * sizeof(DesignerVisualState)
					+ group.Transitions.capacity()
						* sizeof(DesignerVisualTransition);
				for (const auto& transition : group.Transitions)
				{
					result += WideStringMemory(transition.FromState)
						+ WideStringMemory(transition.ToState)
						+ transition.Animations.capacity()
							* sizeof(DesignerVisualStateAnimation);
					for (const auto& animation : transition.Animations)
					{
						result += WideStringMemory(animation.TargetName)
							+ WideStringMemory(animation.PropertyName)
							+ WideStringMemory(animation.FromResourceKey)
							+ WideStringMemory(animation.ToResourceKey)
							+ WideStringMemory(animation.ByResourceKey)
							+ StyleValueMemory(animation.From)
							+ StyleValueMemory(animation.To)
							+ StyleValueMemory(animation.By)
							+ animation.KeyFrames.capacity()
								* sizeof(DesignerAnimationKeyFrame);
						for (const auto& keyFrame : animation.KeyFrames)
							result += WideStringMemory(keyFrame.ResourceKey)
								+ StyleValueMemory(keyFrame.Value);
					}
				}
				for (const auto& state : group.States)
				{
					result += WideStringMemory(state.Name)
						+ state.Conditions.capacity()
							* sizeof(DesignerVisualStateCondition)
						+ state.EventNames.capacity() * sizeof(std::wstring)
						+ state.Setters.capacity()
							* sizeof(DesignerVisualStateSetter)
						+ state.Animations.capacity()
							* sizeof(DesignerVisualStateAnimation);
					for (const auto& condition : state.Conditions)
						result += WideStringMemory(condition.PropertyName)
							+ StyleValueMemory(condition.Value);
					for (const auto& eventName : state.EventNames)
						result += WideStringMemory(eventName);
					for (const auto& setter : state.Setters)
						result += WideStringMemory(setter.TargetName)
							+ WideStringMemory(setter.PropertyName)
							+ WideStringMemory(setter.ResourceKey)
							+ StyleValueMemory(setter.Literal);
					for (const auto& animation : state.Animations)
					{
						result += WideStringMemory(animation.TargetName)
							+ WideStringMemory(animation.PropertyName)
							+ WideStringMemory(animation.FromResourceKey)
							+ WideStringMemory(animation.ToResourceKey)
							+ WideStringMemory(animation.ByResourceKey)
							+ StyleValueMemory(animation.From)
							+ StyleValueMemory(animation.To)
							+ StyleValueMemory(animation.By)
							+ animation.KeyFrames.capacity()
								* sizeof(DesignerAnimationKeyFrame);
						for (const auto& keyFrame : animation.KeyFrames)
							result += WideStringMemory(keyFrame.ResourceKey)
								+ StyleValueMemory(keyFrame.Value);
					}
				}
			}
			for (const auto& trigger : component.EventTriggers)
			{
				result += WideStringMemory(trigger.EventName)
					+ trigger.Actions.capacity()
						* sizeof(DesignerEventTriggerAction);
				for (const auto& action : trigger.Actions)
				{
					result += WideStringMemory(action.StoryboardName)
						+ action.Animations.capacity()
							* sizeof(DesignerVisualStateAnimation);
					for (const auto& animation : action.Animations)
					{
						result += WideStringMemory(animation.TargetName)
							+ WideStringMemory(animation.PropertyName)
							+ WideStringMemory(animation.FromResourceKey)
							+ WideStringMemory(animation.ToResourceKey)
							+ WideStringMemory(animation.ByResourceKey)
							+ StyleValueMemory(animation.From)
							+ StyleValueMemory(animation.To)
							+ StyleValueMemory(animation.By)
							+ animation.KeyFrames.capacity()
								* sizeof(DesignerAnimationKeyFrame);
						for (const auto& keyFrame : animation.KeyFrames)
							result += WideStringMemory(keyFrame.ResourceKey)
								+ StyleValueMemory(keyFrame.Value);
					}
				}
			}
			for (const auto& node : component.Template)
			{
				result += WideStringMemory(node.ParentRef)
					+ WideStringMemory(node.Name)
					+ WideStringMemory(node.ComponentContentProperty)
					+ WideStringMemory(node.PresentedComponentContent)
					+ DesignValueMemory(EncodeDesignNodeProperties(node.Properties))
					+ DesignerCommandMemory::StructureHeap(node.Structure)
					+ DesignerCommandMemory::TemplateStateHeap(node.TemplateState)
					+ DesignValueMemory(EncodeDesignNodeEvents(node.Events))
					+ DesignValueMemory(EncodeDesignNodeBindings(node.Bindings));
				for (const auto& [target, source] : node.TemplateBindings)
					result += WideStringMemory(target)
						+ WideStringMemory(source) + 3 * sizeof(void*);
				for (const auto& [sourceEvent, componentEvent]
					: node.TemplateEventBindings)
					result += WideStringMemory(sourceEvent)
						+ WideStringMemory(componentEvent) + 3 * sizeof(void*);
			}
		}
		result += document.ControlTemplates.capacity()
			* sizeof(DesignerModel::DesignControlTemplate);
		for (const auto& item : document.ControlTemplates)
		{
			result += WideStringMemory(item.Key)
				+ WideStringMemory(item.SourceDictionary)
				+ item.Template.capacity()
					* sizeof(DesignerModel::DesignNode)
				+ item.VisualStateGroups.capacity()
					* sizeof(DesignerVisualStateGroup)
				+ item.EventTriggers.capacity()
					* sizeof(DesignerEventTrigger);
			for (const auto& node : item.Template)
				result += WideStringMemory(node.ParentRef)
					+ WideStringMemory(node.Name)
					+ DesignValueMemory(EncodeDesignNodeProperties(node.Properties))
					+ DesignerCommandMemory::StructureHeap(node.Structure)
					+ DesignerCommandMemory::TemplateStateHeap(node.TemplateState)
					+ DesignValueMemory(EncodeDesignNodeEvents(node.Events))
					+ DesignValueMemory(EncodeDesignNodeBindings(node.Bindings));
		}
		result += document.DataTypes.capacity()
			* sizeof(DesignerModel::DesignDataTypeDefinition);
		for (const auto& type : document.DataTypes)
		{
			result += WideStringMemory(type.Name)
				+ WideStringMemory(type.SourceDictionary)
				+ type.Properties.capacity()
					* sizeof(DesignerDataContextProperty);
			for (const auto& property : type.Properties)
				result += WideStringMemory(property.Path)
					+ WideStringMemory(property.ItemType);
		}
		result += document.DataTemplates.capacity()
			* sizeof(DesignerModel::DesignDataTemplate);
		for (const auto& item : document.DataTemplates)
		{
			result += WideStringMemory(item.Key)
				+ WideStringMemory(item.DataType)
				+ WideStringMemory(item.SourceDictionary)
				+ item.Template.capacity()
					* sizeof(DesignerModel::DesignNode);
		}
		result += document.ItemsPanelTemplates.capacity()
			* sizeof(DesignerModel::DesignItemsPanelTemplate);
		for (const auto& item : document.ItemsPanelTemplates)
			result += WideStringMemory(item.Key)
				+ WideStringMemory(item.SourceDictionary);
		result += document.GroupStyles.capacity()
			* sizeof(DesignerModel::DesignGroupStyle);
		for (const auto& item : document.GroupStyles)
			result += WideStringMemory(item.Key)
				+ WideStringMemory(item.HeaderTemplate)
				+ WideStringMemory(item.SourceDictionary);
		result += document.DataLists.capacity()
			* sizeof(DesignerModel::DesignDataList);
		for (const auto& list : document.DataLists)
		{
			result += WideStringMemory(list.Key)
				+ WideStringMemory(list.ItemType)
				+ WideStringMemory(list.SourceDictionary)
				+ list.Records.capacity()
					* sizeof(DesignerModel::DesignDataRecord);
			for (const auto& record : list.Records)
				for (const auto& [path, value] : record.Fields)
					result += WideStringMemory(path)
						+ WideStringMemory(value) + 3 * sizeof(void*);
		}
		result += document.CollectionViews.capacity()
			* sizeof(DesignerModel::DesignCollectionViewSource);
		for (const auto& view : document.CollectionViews)
		{
			result += WideStringMemory(view.Key)
				+ WideStringMemory(view.SourceResource)
				+ WideStringMemory(view.SourceBindingPath)
				+ WideStringMemory(view.SourceDictionary)
				+ view.GroupDescriptions.capacity()
					* sizeof(DesignerModel::DesignCollectionGroupDescription)
				+ view.AggregateDescriptions.capacity()
					* sizeof(DesignerModel::DesignCollectionAggregateDescription)
				+ view.SortDescriptions.capacity()
					* sizeof(DesignerModel::DesignCollectionSortDescription)
				+ view.FilterDescriptions.capacity()
					* sizeof(DesignerModel::DesignCollectionFilterDescription);
			for (const auto& item : view.SortDescriptions)
				result += WideStringMemory(item.PropertyName);
			for (const auto& item : view.GroupDescriptions)
				result += WideStringMemory(item.PropertyName);
			for (const auto& item : view.AggregateDescriptions)
				result += WideStringMemory(item.Name)
					+ WideStringMemory(item.PropertyName);
			for (const auto& item : view.FilterDescriptions)
				result += WideStringMemory(item.PropertyName)
					+ WideStringMemory(item.Value);
		}

		result += document.StyleSheet.Rules.capacity()
			* sizeof(DesignerStyleRule);
		auto animationMemory = [&](const DesignerVisualStateAnimation& animation)
		{
			size_t memory = WideStringMemory(animation.TargetName)
				+ WideStringMemory(animation.PropertyName)
				+ WideStringMemory(animation.FromResourceKey)
				+ WideStringMemory(animation.ToResourceKey)
				+ WideStringMemory(animation.ByResourceKey)
				+ StyleValueMemory(animation.From)
				+ StyleValueMemory(animation.To)
				+ StyleValueMemory(animation.By)
				+ animation.KeyFrames.capacity()
					* sizeof(DesignerAnimationKeyFrame);
			for (const auto& keyFrame : animation.KeyFrames)
				memory += WideStringMemory(keyFrame.ResourceKey)
					+ StyleValueMemory(keyFrame.Value);
			return memory;
		};
		auto actionsMemory = [&](const auto& actions)
		{
			size_t memory = actions.capacity()
				* sizeof(DesignerEventTriggerAction);
			for (const auto& action : actions)
			{
				memory += WideStringMemory(action.StoryboardName)
					+ action.Animations.capacity()
						* sizeof(DesignerVisualStateAnimation);
				for (const auto& animation : action.Animations)
					memory += animationMemory(animation);
			}
			return memory;
		};
		for (const auto& rule : document.StyleSheet.Rules)
		{
			result += WideStringMemory(rule.Id)
				+ WideStringMemory(rule.BasedOn)
				+ WideStringMemory(rule.SourceDictionary)
				+ rule.Setters.capacity() * sizeof(DesignerStyleSetter)
				+ rule.PropertyConditions.capacity()
					* sizeof(DesignerStylePropertyCondition)
				+ rule.DataConditions.capacity() * sizeof(DesignerStyleDataCondition)
				+ rule.Triggers.capacity() * sizeof(DesignerStyleTrigger)
				+ actionsMemory(rule.EnterActions)
				+ actionsMemory(rule.ExitActions);
			for (const auto& setter : rule.Setters)
				result += WideStringMemory(setter.PropertyName)
					+ WideStringMemory(setter.ResourceKey)
					+ StyleValueMemory(setter.Literal);
			for (const auto& condition : rule.DataConditions)
				result += WideStringMemory(condition.SourceProperty)
					+ StyleValueMemory(condition.Value);
			for (const auto& condition : rule.PropertyConditions)
				result += WideStringMemory(condition.Property)
					+ StyleValueMemory(condition.Value);
			for (const auto& trigger : rule.Triggers)
			{
				result += trigger.PropertyConditions.capacity()
						* sizeof(DesignerStylePropertyCondition)
					+ trigger.DataConditions.capacity()
						* sizeof(DesignerStyleDataCondition)
					+ trigger.Setters.capacity() * sizeof(DesignerStyleSetter)
					+ actionsMemory(trigger.EnterActions)
					+ actionsMemory(trigger.ExitActions);
				for (const auto& condition : trigger.PropertyConditions)
					result += WideStringMemory(condition.Property)
						+ StyleValueMemory(condition.Value);
				for (const auto& condition : trigger.DataConditions)
					result += WideStringMemory(condition.SourceProperty)
						+ StyleValueMemory(condition.Value);
				for (const auto& setter : trigger.Setters)
					result += WideStringMemory(setter.PropertyName)
						+ WideStringMemory(setter.ResourceKey)
						+ StyleValueMemory(setter.Literal);
			}
		}

		result += document.Nodes.capacity()
			* sizeof(DesignerModel::DesignNode);
		for (const auto& node : document.Nodes)
		{
			result += WideStringMemory(node.ParentRef)
				+ WideStringMemory(node.Name)
				+ WideStringMemory(node.ComponentType.XamlPrefix)
				+ WideStringMemory(node.ComponentType.XamlName)
				+ WideStringMemory(node.ComponentType.XamlNamespace)
				+ WideStringMemory(node.ComponentContentProperty)
				+ WideStringMemory(node.PresentedComponentContent)
				+ DesignValueMemory(EncodeDesignNodeProperties(node.Properties))
				+ DesignerCommandMemory::StructureHeap(node.Structure)
				+ DesignerCommandMemory::TemplateStateHeap(node.TemplateState)
				+ DesignValueMemory(EncodeDesignNodeEvents(node.Events))
				+ DesignValueMemory(EncodeDesignNodeBindings(node.Bindings));
			for (const auto& [target, source] : node.TemplateBindings)
				result += WideStringMemory(target) + WideStringMemory(source)
					+ 3 * sizeof(void*);
		}
		return result;
	}

	size_t SelectionMemory(
		const std::vector<std::wstring>& names) noexcept
	{
		size_t result = names.capacity() * sizeof(std::wstring);
		for (const auto& name : names) result += WideStringMemory(name);
		return result;
	}

	bool IsMergeableLabel(const std::wstring& label) noexcept
	{
		return label == L"NudgeSelection"
			|| label.rfind(L"UpdateProperty:", 0) == 0;
	}
}

DocumentSnapshotCommand::DocumentSnapshotCommand(
	DesignerCanvas* canvas,
	DesignerModel::DesignDocument beforeDocument,
	DesignerModel::DesignDocument afterDocument,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforeSelectionName,
	std::wstring afterSelectionName,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _beforeDocument(std::move(beforeDocument)),
	  _afterDocument(std::move(afterDocument)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforeSelectionName(std::move(beforeSelectionName)),
	  _afterSelectionName(std::move(afterSelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute),
	  _committedAt(std::chrono::steady_clock::now())
{
	_estimatedMemoryUsage = sizeof(*this)
		+ DocumentMemory(_beforeDocument)
		+ DocumentMemory(_afterDocument)
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ WideStringMemory(_beforeSelectionName)
		+ WideStringMemory(_afterSelectionName)
		+ WideStringMemory(_label);
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);
	}

	return Apply(_afterDocument, _afterSelectionNames, _afterSelectionName);
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Undo()
{
	return Apply(
		_beforeDocument, _beforeSelectionNames, _beforeSelectionName);
}

std::wstring DocumentSnapshotCommand::GetLabel() const
{
	return _label;
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Apply(
	const DesignerModel::DesignDocument& document,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName) const
{
	if (!_canvas)
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法应用文档快照。", false);
	}

	DesignerModel::DesignDocument rollbackDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(rollbackDocument, &error))
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立当前文档的恢复快照：" + error);
	}
	std::vector<std::wstring> rollbackSelectionNames;
	rollbackSelectionNames.reserve(_canvas->GetSelectedControls().size());
	for (const auto& control : _canvas->GetSelectedControls())
		if (control && !control->Name.empty())
			rollbackSelectionNames.push_back(control->Name);
	const auto rollbackPrimary = _canvas->GetSelectedControl()
		? _canvas->GetSelectedControl()->Name : std::wstring{};

	if (!_canvas->ApplyDesignDocument(document, &error))
	{
		std::wstring restoreError;
		const bool restored = _canvas->ApplyDesignDocument(
			rollbackDocument, &restoreError);
		if (restored)
			_canvas->RestoreSelectionByNames(
				rollbackSelectionNames, rollbackPrimary, true);
		std::wstring message = L"无法应用文档快照：" + error;
		if (!restored)
			message += L" 文档恢复失败：" + restoreError;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			std::move(message), restored);
	}
	_canvas->RestoreSelectionByNames(selectionNames, primarySelectionName, true);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

bool DocumentSnapshotCommand::TryMergeWith(
	IDesignerCommand& newerCommand) noexcept
{
	auto* newer = dynamic_cast<DocumentSnapshotCommand*>(&newerCommand);
	if (!newer || newer == this || _canvas != newer->_canvas
		|| _label != newer->_label || !IsMergeableLabel(_label)
		|| _skipInitialExecute || newer->_skipInitialExecute
		|| _afterSelectionNames != newer->_beforeSelectionNames
		|| _afterSelectionName != newer->_beforeSelectionName
		|| !(_afterDocument == newer->_beforeDocument))
		return false;
	const auto elapsed = newer->_committedAt - _committedAt;
	if (elapsed < std::chrono::steady_clock::duration::zero()
		|| elapsed > MergeWindow)
		return false;

	_afterDocument = std::move(newer->_afterDocument);
	_afterSelectionNames = std::move(newer->_afterSelectionNames);
	_afterSelectionName = std::move(newer->_afterSelectionName);
	_committedAt = newer->_committedAt;
	_estimatedMemoryUsage = sizeof(*this)
		+ DocumentMemory(_beforeDocument)
		+ DocumentMemory(_afterDocument)
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ WideStringMemory(_beforeSelectionName)
		+ WideStringMemory(_afterSelectionName)
		+ WideStringMemory(_label);
	return true;
}

size_t DocumentSnapshotCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

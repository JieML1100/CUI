#include "RuntimeDocument.h"
#include "DesignDocumentEventIndex.h"

#include "../../CuiRuntime/include/BindingConverterRegistry.h"
#include "../../CuiRuntime/include/XamlObjectMaterializer.h"
#include "../../CuiRuntime/include/XamlRuntimeSchema.h"
#include "RuntimeDocumentTopologyReloader.h"
#include "XamlDocumentParser.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/Button.h"
#include "../../CUI/include/Canvas.h"
#include "../../CUI/include/ContextMenu.h"
#include "../../CUI/include/Layout/DockPanel.h"
#include "../../CUI/include/Layout/Grid.h"
#include "../../CUI/include/Menu.h"
#include "../../CUI/include/StyleInfrastructure.h"
#include "../../CUI/include/Window.h"
#include "../../CUI/include/XamlInfrastructure.h"

#include <Convert.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace DesignerModel
{
bool NativeSurfaceBehaviorRegistry::Register(
	std::wstring behaviorKey,
	Factory factory,
	std::wstring* outError)
{
	if (behaviorKey.empty() || !factory)
	{
		if (outError) *outError =
			L"NativeSurface 行为注册需要非空键和工厂。";
		return false;
	}
	std::scoped_lock lock(_mutex);
	_factories[std::move(behaviorKey)] = std::move(factory);
	if (outError) outError->clear();
	return true;
}

bool NativeSurfaceBehaviorRegistry::Unregister(
	const std::wstring& behaviorKey) noexcept
{
	std::scoped_lock lock(_mutex);
	return _factories.erase(behaviorKey) != 0;
}

std::unique_ptr<INativeSurfaceBehavior> NativeSurfaceBehaviorRegistry::Create(
	const std::wstring& behaviorKey,
	NativeSurface& host) const
{
	Factory factory;
	{
		std::scoped_lock lock(_mutex);
		const auto found = _factories.find(behaviorKey);
		if (found == _factories.end()) return {};
		factory = found->second;
	}
	return factory ? factory(host) : nullptr;
}

bool DeclarativeComponentBehaviorRegistry::Register(
	RuntimeTypeId componentType,
	Factory factory,
	std::wstring* outError)
{
	if (!componentType.Valid() || !factory)
	{
		if (outError) *outError =
			L"声明组件 Behavior 注册需要完整 QName 和工厂。";
		return false;
	}
	const auto key = componentType.RegistryKey();
	std::scoped_lock lock(_mutex);
	_factories[key] = std::move(factory);
	if (outError) outError->clear();
	return true;
}

bool DeclarativeComponentBehaviorRegistry::Unregister(
	const RuntimeTypeId& componentType) noexcept
{
	std::scoped_lock lock(_mutex);
	return _factories.erase(componentType.RegistryKey()) != 0;
}

std::unique_ptr<IDeclarativeComponentBehavior>
DeclarativeComponentBehaviorRegistry::Create(
	const DeclarativeComponentBehaviorContext& context) const
{
	Factory factory;
	{
		std::scoped_lock lock(_mutex);
		const auto found = _factories.find(context.Type.RegistryKey());
		if (found == _factories.end()) return {};
		factory = found->second;
	}
	return factory ? factory(context) : nullptr;
}

namespace
{
	void SetError(std::wstring* output, std::wstring value)
	{
		if (output) *output = std::move(value);
	}

	CuiRuntime::XamlMaterializationOptions MaterializationOptionsFor(
		const RuntimeDocumentLoadOptions& options)
	{
		CuiRuntime::XamlMaterializationOptions result;
		result.AllowNativeSurfacePlaceholder =
			options.AllowNativeSurfacePlaceholder;
		if (options.NativeSurfaceBehaviors)
		{
			auto registry = options.NativeSurfaceBehaviors;
			result.NativeSurfaceBehaviorFactory =
				[registry](const DesignNode&, NativeSurface& host)
				{
					return registry->Create(host.GetBehaviorKey(), host);
				};
		}
		if (options.DeclarativeComponentBehaviors)
		{
			auto registry = options.DeclarativeComponentBehaviors;
			result.DeclarativeComponentBehaviorFactory =
				[registry](const DeclarativeComponentBehaviorContext& context)
				{
					return registry->Create(context);
				};
		}
		return result;
	}

	std::vector<std::pair<std::wstring, BindingValue>>
	BuildStructuralStyleResourcesFor(
		const DesignDocument* document,
		const RuntimeDocumentLoadOptions& options)
	{
		if (!document) return {};
		return CuiRuntime::XamlObjectMaterializer::
			BuildStructuralStyleResources(
				std::make_shared<const DesignDocument>(*document),
				MaterializationOptionsFor(options));
	}

	std::vector<std::pair<std::wstring, BindingValue>>
	BuildStructuralStyleResourcesFor(
		const DesignDocument* document,
		const std::shared_ptr<const NativeSurfaceBehaviorRegistry>&
			nativeSurfaceBehaviors,
		const std::shared_ptr<const DeclarativeComponentBehaviorRegistry>&
			declarativeComponentBehaviors,
		bool allowNativeSurfacePlaceholder)
	{
		RuntimeDocumentLoadOptions options;
		options.NativeSurfaceBehaviors = nativeSurfaceBehaviors;
		options.DeclarativeComponentBehaviors =
			declarativeComponentBehaviors;
		options.AllowNativeSurfacePlaceholder =
			allowNativeSurfacePlaceholder;
		return BuildStructuralStyleResourcesFor(document, options);
	}

	class WindowRuntimeDocumentContentHost final
		: public RuntimeDocumentContentHost
	{
	public:
		explicit WindowRuntimeDocumentContentHost(::Window& window) noexcept
			: _window(&window)
		{
		}

		bool DetachContent(
			Control* content,
			std::unique_ptr<Control>& output,
			std::wstring* outError) override
		{
			if (_transactionOpen)
			{
				SetError(outError, L"Window.Content 已存在未完成的替换事务。");
				return false;
			}
			if (output)
			{
				SetError(outError, L"Window 事务只接受一个 Content。");
				return false;
			}
			auto* current = _window->GetVisualContent();
			if (content != current)
			{
				SetError(outError, L"运行时文档 Content 不再属于指定 Window。");
				return false;
			}
			if (current)
			{
				auto detached = _window->DetachVisualContent();
				if (!detached)
				{
					SetError(outError, L"Window 无法分离当前 Content。");
					return false;
				}
				output = std::move(detached);
			}
			_transactionOpen = true;
			if (outError) outError->clear();
			return true;
		}

		bool AttachContent(
			std::unique_ptr<Control>& content,
			RuntimeContentHostAttachMode mode,
			std::wstring* outError) override
		{
			const bool transactionAttach =
				mode != RuntimeContentHostAttachMode::Initial;
			if (transactionAttach != _transactionOpen)
			{
			SetError(outError, transactionAttach
					? L"Content 宿主没有可提交的分离事务。"
					: L"Content 宿主事务未完成，不能执行初始挂载。");
				return false;
			}
			if (_window->GetVisualContent())
			{
				SetError(outError, L"Window.Content 必须为空才能提交事务。");
				return false;
			}
			if (content && !_window->TrySetVisualContent(content))
			{
				SetError(outError, L"Window 无法挂载运行时文档 Content。");
				return false;
			}
			if (transactionAttach) _transactionOpen = false;
			if (outError) outError->clear();
			return true;
		}

	private:
		::Window* _window = nullptr;
		bool _transactionOpen = false;
	};

	/**
	 * Transactional snapshot for the real Window.DataContext dependency value.
	 * RuntimeDocument may project an explicitly supplied runtime source onto the
	 * Window, but it never destroys an existing Binding/DynamicResource
	 * expression to do so.
	 */
	struct WindowDataContextSnapshot
	{
		::Window* Target = nullptr;
		BindingSourceReference LocalValue;
		bool HadLocalValue = false;
		DependencyPropertyExpressionKind Expression =
			DependencyPropertyExpressionKind::None;
		bool Changed = false;

		static WindowDataContextSnapshot Capture(::Window& form)
		{
			WindowDataContextSnapshot snapshot;
			snapshot.Target = &form;
			snapshot.Expression = form.GetPropertyExpressionKind(
				L"DataContext", DependencyPropertyValueSource::Local);
			snapshot.HadLocalValue = form.HasPropertyValue(
				L"DataContext", DependencyPropertyValueSource::Local);
			if (snapshot.HadLocalValue)
			{
				BindingValue value;
				if (!form.TryGetPropertyValue(
					L"DataContext", DependencyPropertyValueSource::Local, value)
					|| !value.TryGet(snapshot.LocalValue))
					throw std::logic_error(
						"Window.DataContext local value is not a BindingSourceReference");
			}
			return snapshot;
		}

		bool Apply(
			const BindingSourceReference& value,
			std::wstring* outError)
		{
			if (!Target || !value || Target->GetDataContext() == value)
			{
				if (outError) outError->clear();
				return true;
			}
			if (Expression != DependencyPropertyExpressionKind::None)
			{
				SetError(outError,
					L"Window.DataContext 已由 Binding 或 DynamicResource 表达式占用；"
					L"运行时数据源不能隐式替换该表达式。");
				return false;
			}
			try
			{
				if (!Target->SetDataContext(value))
				{
					SetError(outError, L"无法把运行时 DataContext 应用到 Window。");
					return false;
				}
			}
			catch (...)
			{
				SetError(outError, L"应用 Window.DataContext 时抛出异常。");
				return false;
			}
			Changed = true;
			if (outError) outError->clear();
			return true;
		}

		void Restore() noexcept
		{
			if (!Target || !Changed) return;
			try
			{
				if (HadLocalValue) (void)Target->SetDataContext(LocalValue);
				else (void)Target->ClearDataContext();
			}
			catch (...) {}
			Changed = false;
		}
	};

	struct WindowPresentationSnapshot
	{
		struct LocalPropertyValue
		{
			bool Present = false;
			BindingValue Value;
			std::wstring DynamicResourceKey;

			static LocalPropertyValue Capture(
				::Control& target, const std::wstring& propertyName)
			{
				LocalPropertyValue snapshot;
				snapshot.Present = target.TryGetPropertyValue(
					propertyName, DependencyPropertyValueSource::Local,
					snapshot.Value);
				(void)target.TryGetDynamicResourceKey(
					propertyName, snapshot.DynamicResourceKey,
					DependencyPropertyValueSource::Local);
				return snapshot;
			}

			void Restore(
				::Control& target, const std::wstring& propertyName) const
			{
				(void)target.ClearDynamicResource(propertyName);
				(void)target.ClearPropertyValue(propertyName);
				if (!DynamicResourceKey.empty())
				{
					(void)target.SetDynamicResource(
						propertyName, DynamicResourceKey);
				}
				else if (Present)
				{
					(void)target.TrySetPropertyValue(propertyName, Value);
				}
			}
		};

		::Window* Target = nullptr;
		std::wstring Title;
		float Left = std::numeric_limits<float>::quiet_NaN();
		float Top = std::numeric_limits<float>::quiet_NaN();
		cui::layout::Length Width = cui::layout::Length::Auto();
		cui::layout::Length Height = cui::layout::Length::Auto();
		cui::drawing::Brush Background;
		cui::drawing::Brush Foreground;
		bool ShowInTaskbar = true;
		bool Topmost = false;
		bool IsEnabled = true;
		::Visibility Visibility = ::Visibility::Visible;
		::WindowStyle Style = ::WindowStyle::SingleBorderWindow;
		::ResizeMode Resize = ::ResizeMode::CanResize;
		std::wstring StyleResourceKey;
		LocalPropertyValue FontFamily;
		LocalPropertyValue FontSize;
		std::vector<InputBinding> InputBindings;

		static WindowPresentationSnapshot Capture(::Window& form)
		{
			WindowPresentationSnapshot snapshot;
			snapshot.Target = &form;
			snapshot.Title = form.Title;
			snapshot.Left = form.Left;
			snapshot.Top = form.Top;
			snapshot.Width = form.Width;
			snapshot.Height = form.Height;
			BindingValue brushValue;
			(void)(form.TryGetPropertyValue(L"Background", brushValue)
				&& brushValue.TryGet(snapshot.Background));
			brushValue = {};
			(void)(form.TryGetPropertyValue(L"Foreground", brushValue)
				&& brushValue.TryGet(snapshot.Foreground));
			snapshot.ShowInTaskbar = form.ShowInTaskbar;
			snapshot.Topmost = form.Topmost;
			snapshot.IsEnabled = form.IsLocallyEnabled();
			snapshot.Visibility = form.Visibility;
			snapshot.Style = form.WindowStyle;
			snapshot.Resize = form.ResizeMode;
			snapshot.StyleResourceKey =
				cui::framework::StyleAccess::ResourceKey(form);
			snapshot.InputBindings.assign(
				form.GetInputBindings().begin(), form.GetInputBindings().end());
			snapshot.FontFamily = LocalPropertyValue::Capture(
				form, L"FontFamily");
			snapshot.FontSize = LocalPropertyValue::Capture(
				form, L"FontSize");
			return snapshot;
		}

		void Restore() noexcept
		{
			if (!Target) return;
			try
			{
				Target->Title = Title;
				Target->Left = Left;
				Target->Top = Top;
				Target->Width = Width;
				Target->Height = Height;
				(void)Target->TrySetPropertyValue(
					L"Background", BindingValue(Background));
				(void)Target->TrySetPropertyValue(
					L"Foreground", BindingValue(Foreground));
				Target->ShowInTaskbar = ShowInTaskbar;
				Target->Topmost = Topmost;
				Target->IsEnabled = IsEnabled;
				Target->WindowStyle = Style;
				Target->ResizeMode = Resize;
				cui::framework::StyleAccess::SetResourceKey(
					*Target, StyleResourceKey);
				(void)Target->SetInputBindings(InputBindings);
				Target->Visibility = Visibility;
				FontFamily.Restore(*Target, L"FontFamily");
				FontSize.Restore(*Target, L"FontSize");
			}
			catch (...) {}
		}
	};

	bool HasSameWindowPresentation(
		const DesignNode& left,
		const DesignNode& right) noexcept
	{
		return left.Properties == right.Properties
			&& left.Bindings == right.Bindings;
	}

	bool ApplyWindowNode(
		const DesignNode& model,
		::Window& form,
		std::wstring* outError,
		const DesignNode* previous = nullptr)
	{
		try
		{
			if (model.Type != UIClass::UI_Window || !model.XamlType.Valid())
			{
				SetError(outError, L"运行时根节点不是有效的 XAML Window。");
				return false;
			}
			const auto& currentType = form.GetDeclarativeTypeId();
			if (currentType.Valid() && currentType != model.XamlType)
			{
				SetError(outError,
					L"Window 宿主已经绑定到另一个 XAML 类型身份。");
				return false;
			}
			if (!currentType.Valid())
			{
				const auto* builtIn = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
					model.XamlType.NamespaceUri, model.XamlType.LocalName);
				if (!builtIn || builtIn->NativeType != UIClass::UI_Window)
				{
					SetError(outError,
						L"运行时根节点的 XAML 类型不是 Window behavior host。");
					return false;
				}
				XamlSchemaContext schemaContext;
				std::wstring schemaError;
				if (!CuiRuntime::XamlRuntimeSchema::AttachBuiltInType(
					form, *builtIn, schemaContext, &schemaError))
				{
					SetError(outError,
						L"Window XAML 类型身份无法附加：" + schemaError);
					return false;
				}
			}
			cui::framework::StyleAccess::SetResourceKey(
				form, model.Properties.StyleResourceKey);

			if (previous)
			{
				for (const auto& [propertyName, assignment]
					: previous->Properties.Values)
				{
					(void)assignment;
					if (model.Properties.Find(propertyName)) continue;
					(void)form.ClearDynamicResource(propertyName);
					(void)form.ResetPropertyValue(propertyName);
				}
			}

			const std::array<const wchar_t*, 15> windowProperties = {
				L"Title", L"Left", L"Top", L"Width", L"Height",
				L"Background", L"Foreground", L"FontFamily", L"FontSize",
				L"ShowInTaskbar", L"Topmost", L"IsEnabled", L"Visibility",
				L"WindowStyle", L"ResizeMode"
			};
			// Missing XAML members are not Local values. Clear the host's native
			// constructor state so Default/Style can win, then install only the
			// properties actually authored by the Window node.
			for (const auto* propertyName : windowProperties)
			{
				if (model.Properties.Find(propertyName)) continue;
				(void)form.ClearDynamicResource(propertyName);
				(void)form.ResetPropertyValue(propertyName);
			}
			auto applyValue = [&](const std::wstring& propertyName,
				const DesignerStyleValue& value)
			{
				std::wstring canonicalName;
				std::wstring propertyError;
				if (!DesignerPropertyCatalog::ApplyValue(
					form, propertyName, value, &canonicalName, nullptr,
					&propertyError))
				{
					SetError(outError, L"应用 Window 属性 " + propertyName
						+ L" 失败：" + propertyError);
					return false;
				}
				return true;
			};
			for (const auto& [propertyName, assignment] : model.Properties.Values)
			{
				if (!applyValue(propertyName, assignment.Value)) return false;
			}
			for (const auto& [propertyName, assignment] : model.Properties.Values)
			{
				(void)form.ClearDynamicResource(propertyName);
				if (!assignment.DynamicResourceKey.empty()
					&& !form.SetDynamicResource(
						propertyName, assignment.DynamicResourceKey))
				{
					SetError(outError, L"Window 无法安装动态资源表达式："
						+ propertyName);
					return false;
				}
			}
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			SetError(outError, L"应用动态文档窗体属性时资源初始化失败。");
			return false;
		}
		catch (...)
		{
			SetError(outError, L"应用动态文档窗体属性时发生未知异常。");
			return false;
		}
	}

	bool ApplyWindowInputBindings(
		const DesignNode& model,
		::Window& form,
		const std::function<Control*(const std::wstring&)>& resolveTarget,
		std::wstring* outError)
	{
		try
		{
			std::vector<InputBinding> bindings;
			bindings.reserve(model.InputBindings.size());
			for (const auto& binding : model.InputBindings)
			{
				std::wstring gestureError;
				Control* commandTarget = nullptr;
				if (!binding.CommandTarget.empty())
				{
					commandTarget = resolveTarget
						? resolveTarget(binding.CommandTarget) : nullptr;
					if (!commandTarget)
					{
						SetError(outError,
							L"Window InputBinding.CommandTarget 无法解析："
							+ binding.CommandTarget);
						return false;
					}
				}
				if (binding.Kind == DesignInputBindingKind::Key)
				{
					KeyGesture gesture;
					if (!TryParseKeyGesture(
						binding.Gesture, gesture, &gestureError))
					{
						SetError(outError,
							L"Window KeyBinding 无效：" + gestureError);
						return false;
					}
					bindings.emplace_back(KeyBinding{
						RoutedCommand(binding.Command), gesture,
						binding.CommandParameter, commandTarget });
				}
				else
				{
					MouseGesture gesture;
					if (!TryParseMouseGesture(
						binding.Gesture, gesture, &gestureError))
					{
						SetError(outError,
							L"Window MouseBinding 无效：" + gestureError);
						return false;
					}
					bindings.emplace_back(MouseBinding{
						RoutedCommand(binding.Command), gesture,
						binding.CommandParameter, commandTarget });
				}
			}
			if (!form.SetInputBindings(std::move(bindings)))
			{
				SetError(outError, L"Window 拒绝了无效的 InputBindings。");
				return false;
			}
			if (outError) outError->clear();
			return true;
		}
		catch (...)
		{
			SetError(outError, L"应用 Window InputBindings 时资源分配失败。");
			return false;
		}
	}

	bool HasConfiguredControlEvents(const RuntimeDocument& document)
	{
		return std::any_of(
			document.Controls().begin(), document.Controls().end(),
			[](const auto& control)
			{
				return control && (!control->EventHandlers.empty()
					|| !control->CommandBindings.empty());
			});
	}

	std::wstring FromUtf8(const std::string& value)
	{
		return Convert::Utf8ToUnicode(value);
	}

	bool SameNodeShapeForInPlaceReload(
		const DesignNode& left,
		const DesignNode& right)
	{
		if (left.Type == UIClass::UI_NativeSurface)
		{
			const auto* leftBehavior = left.Properties.Find(L"BehaviorKey");
			const auto* rightBehavior = right.Properties.Find(L"BehaviorKey");
			if ((!leftBehavior) != (!rightBehavior)
				|| (leftBehavior && *leftBehavior != *rightBehavior)) return false;
		}
		return !left.NameIsGenerated
			&& !right.NameIsGenerated
			&& left.ParentRef == right.ParentRef
			&& left.Name == right.Name
			&& left.Type == right.Type
			&& left.ComponentType == right.ComponentType
			&& left.ComponentContentProperty
				== right.ComponentContentProperty
			&& left.PresentedComponentContent
				== right.PresentedComponentContent
			&& left.Order == right.Order
			&& left.Structure == right.Structure
			&& left.TemplateState == right.TemplateState
			&& left.CommandBindings == right.CommandBindings
			&& left.InputBindings == right.InputBindings
			&& left.LocalResources == right.LocalResources
			&& left.LocalObjectResources == right.LocalObjectResources;
	}

	bool HasLocalStyleRules(const DesignDocument& document)
	{
		return std::any_of(document.Nodes.begin(), document.Nodes.end(),
			[](const DesignNode& node)
			{ return !node.LocalResources.Rules.empty(); });
	}

	bool HasStructuralTemplateStyles(const DesignDocument& document)
	{
		auto hasTemplateSetter = [](const DesignerStyleSheet& sheet)
		{
			return std::any_of(sheet.Rules.begin(), sheet.Rules.end(),
				[](const auto& rule)
				{
					return std::any_of(
						rule.Setters.begin(), rule.Setters.end(), [](const auto& setter)
						{ return setter.PropertyName == L"Template"; });
				});
		};
		return hasTemplateSetter(document.StyleSheet)
			|| std::any_of(document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& node) { return hasTemplateSetter(node.LocalResources); });
	}

	bool CanReloadInPlace(
		const DesignDocument& current,
		const DesignDocument& next)
	{
		// Template is a structural value. Until runtime tree replacement is a
		// first-class property operation, any document carrying Style.Template
		// must be rebuilt so Style resource and BasedOn changes cannot leave stale
		// visual children behind.
		if (HasStructuralTemplateStyles(current)
			|| HasStructuralTemplateStyles(next))
			return false;
		if (current.Schema != next.Schema) return false;
		if (current.Window.Name != next.Window.Name) return false;
		if (current.Window.Events != next.Window.Events) return false;
		if (current.Window.CommandBindings != next.Window.CommandBindings)
			return false;
		if (current.Window.InputBindings != next.Window.InputBindings)
			return false;
		if ((current.HasResourceBackedVisualStates()
					|| next.HasResourceBackedVisualStates()
					|| HasLocalStyleRules(current)
					|| HasLocalStyleRules(next))
				&& current.StyleSheet != next.StyleSheet)
			return false;
		if (current.Components != next.Components) return false;
		if (current.ControlTemplates != next.ControlTemplates)
			return false;
		if (current.DataTypes != next.DataTypes) return false;
		if (current.DataTemplates != next.DataTemplates)
			return false;
		if (current.ItemsPanelTemplates != next.ItemsPanelTemplates)
			return false;
		if (current.GroupStyles != next.GroupStyles) return false;
		if (current.DataLists != next.DataLists) return false;
		if (current.CollectionViews != next.CollectionViews)
			return false;
		if (current.Nodes.size() != next.Nodes.size()) return false;
		std::unordered_map<std::wstring, const DesignNode*> currentByName;
		currentByName.reserve(current.Nodes.size());
		for (const auto& node : current.Nodes)
		{
			if (node.NameIsGenerated) return false;
			currentByName.emplace(node.Name, &node);
		}
		for (const auto& node : next.Nodes)
		{
			if (node.NameIsGenerated) return false;
			const auto found = currentByName.find(node.Name);
			if (found == currentByName.end()
				|| !SameNodeShapeForInPlaceReload(*found->second, node))
				return false;
		}
		return true;
	}

	struct InPlaceControlSnapshot
	{
		DesignerControl* Record = nullptr;
		Control* Target = nullptr;
		std::wstring StyleResourceKey;
		cui::layout::Length Width = cui::layout::Length::Auto();
		cui::layout::Length Height = cui::layout::Length::Auto();
		float CanvasLeft = cui::layout::UnsetCanvasOffset;
		float CanvasTop = cui::layout::UnsetCanvasOffset;
		float CanvasRight = cui::layout::UnsetCanvasOffset;
		float CanvasBottom = cui::layout::UnsetCanvasOffset;
		bool IsEnabled = true;
		::Visibility Visibility = ::Visibility::Visible;
		cui::drawing::Brush Background;
		cui::drawing::Brush Foreground;
		cui::drawing::Brush BorderBrush;
		std::wstring AutomationFullDescription;
		Thickness Margin{};
		Thickness Padding{};
		::HorizontalAlignment Horizontal = ::HorizontalAlignment::Left;
		::VerticalAlignment Vertical = ::VerticalAlignment::Top;
		Dock DockPosition = Dock::Left;
		int ZIndex = 0;
		int GridRow = 0;
		int GridColumn = 0;
		int GridRowSpan = 1;
		int GridColumnSpan = 1;
		std::map<std::wstring, DesignerStyleValue> MetadataProperties;
		std::map<std::wstring, BindingValue> LocalValues;
		std::map<std::wstring, std::wstring> StaticResources;
		std::map<std::wstring, std::wstring> DynamicResources;

		static InPlaceControlSnapshot Capture(DesignerControl& record)
		{
			InPlaceControlSnapshot result;
			result.Record = &record;
			result.Target = record.ControlInstance;
			if (!result.Target) return result;
			auto& target = *result.Target;
			result.StyleResourceKey =
				cui::framework::StyleAccess::ResourceKey(target);
			result.Width = target.Width;
			result.Height = target.Height;
			result.CanvasLeft = Canvas::GetLeft(target);
			result.CanvasTop = Canvas::GetTop(target);
			result.CanvasRight = Canvas::GetRight(target);
			result.CanvasBottom = Canvas::GetBottom(target);
			result.IsEnabled = target.IsLocallyEnabled();
			result.Visibility = target.Visibility;
			BindingValue brushValue;
			(void)(target.TryGetPropertyValue(L"Background", brushValue)
				&& brushValue.TryGet(result.Background));
			brushValue = {};
			(void)(target.TryGetPropertyValue(L"Foreground", brushValue)
				&& brushValue.TryGet(result.Foreground));
			brushValue = {};
			(void)(target.TryGetPropertyValue(L"BorderBrush", brushValue)
				&& brushValue.TryGet(result.BorderBrush));
			result.AutomationFullDescription = target.AutomationFullDescription;
			result.Margin = target.Margin;
			result.Padding = target.Padding;
			result.Horizontal = target.HorizontalAlignment;
			result.Vertical = target.VerticalAlignment;
			result.DockPosition = DockPanel::GetDock(target);
			result.ZIndex = target.ZIndex;
			result.GridRow = Grid::GetRow(target);
			result.GridColumn = Grid::GetColumn(target);
			result.GridRowSpan = Grid::GetRowSpan(target);
			result.GridColumnSpan = Grid::GetColumnSpan(target);
			result.MetadataProperties = record.MetadataProperties;
			result.StaticResources = record.MetadataPropertyResourceKeys;
			result.DynamicResources =
				record.MetadataPropertyDynamicResourceKeys;
			for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
			{
				if (!metadata) continue;
				if (target.GetPropertyExpressionKind(
					metadata->Name(), DependencyPropertyValueSource::Local)
					!= DependencyPropertyExpressionKind::None) continue;
				BindingValue value;
				if (target.TryGetPropertyValue(
					metadata->Name(), DependencyPropertyValueSource::Local, value))
					result.LocalValues.emplace(metadata->Name(), std::move(value));
			}
			return result;
		}

		void Restore() const noexcept
		{
			if (!Target || !Record) return;
			auto& target = *Target;
			try
			{
				cui::framework::StyleAccess::SetResourceKey(
					target, StyleResourceKey);
				auto canRestoreBacking = [&](const wchar_t* property)
				{
					return target.FindPropertyMetadata(property)
						&& target.GetPropertyExpressionKind(
						property, DependencyPropertyValueSource::Local)
						!= DependencyPropertyExpressionKind::Binding;
				};
				if (canRestoreBacking(L"Canvas.Left"))
					Canvas::SetLeft(target, CanvasLeft);
				if (canRestoreBacking(L"Canvas.Top"))
					Canvas::SetTop(target, CanvasTop);
				if (canRestoreBacking(L"Canvas.Right"))
					Canvas::SetRight(target, CanvasRight);
				if (canRestoreBacking(L"Canvas.Bottom"))
					Canvas::SetBottom(target, CanvasBottom);
				if (canRestoreBacking(L"Width")) target.Width = Width;
				if (canRestoreBacking(L"Height")) target.Height = Height;
				if (canRestoreBacking(L"IsEnabled")) target.IsEnabled = IsEnabled;
				if (canRestoreBacking(L"Visibility"))
					target.Visibility = Visibility;
				if (canRestoreBacking(L"Background"))
					(void)target.TrySetPropertyValue(
						L"Background", BindingValue(Background));
				if (canRestoreBacking(L"Foreground"))
					(void)target.TrySetPropertyValue(
						L"Foreground", BindingValue(Foreground));
				if (canRestoreBacking(L"BorderBrush"))
					(void)target.TrySetPropertyValue(
						L"BorderBrush", BindingValue(BorderBrush));
				if (canRestoreBacking(L"AutomationProperties.FullDescription"))
					target.AutomationFullDescription = AutomationFullDescription;
				if (canRestoreBacking(L"Margin")) target.Margin = Margin;
				if (canRestoreBacking(L"Padding")) target.Padding = Padding;
				if (canRestoreBacking(L"HorizontalAlignment"))
					target.HorizontalAlignment = Horizontal;
				if (canRestoreBacking(L"VerticalAlignment"))
					target.VerticalAlignment = Vertical;
				if (canRestoreBacking(L"DockPanel.Dock"))
					DockPanel::SetDock(target, DockPosition);
				if (canRestoreBacking(L"ZIndex")) target.ZIndex = ZIndex;
				if (canRestoreBacking(L"Grid.Row"))
					Grid::SetRow(target, GridRow);
				if (canRestoreBacking(L"Grid.Column"))
					Grid::SetColumn(target, GridColumn);
				if (canRestoreBacking(L"Grid.RowSpan"))
					Grid::SetRowSpan(target, GridRowSpan);
				if (canRestoreBacking(L"Grid.ColumnSpan"))
					Grid::SetColumnSpan(target, GridColumnSpan);
				for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
				{
					if (metadata && target.GetPropertyExpressionKind(
						metadata->Name(), DependencyPropertyValueSource::Local)
						!= DependencyPropertyExpressionKind::Binding)
						(void)target.ClearPropertyValue(metadata->Name());
				}
				for (const auto& [name, value] : LocalValues)
					(void)target.TrySetPropertyValue(name, value);
				for (const auto& [name, resourceKey] : DynamicResources)
					(void)target.SetDynamicResource(name, resourceKey);
				Record->MetadataProperties = MetadataProperties;
				Record->MetadataPropertyResourceKeys = StaticResources;
				Record->MetadataPropertyDynamicResourceKeys = DynamicResources;
			}
			catch (...)
			{
				// Runtime property restoration is best-effort inside a noexcept rollback.
			}
		}
	};

	bool IsRuntimeBound(Control& target, const wchar_t* property)
	{
		return property && target.GetPropertyExpressionKind(
			property, DependencyPropertyValueSource::Local)
			== DependencyPropertyExpressionKind::Binding;
	}

	bool ApplyMetadataPropertyChanges(
		const DesignNodeProperties& currentProperties,
		const DesignNodeProperties& nextProperties,
		DesignerControl& targetRecord,
		const DesignerControl& candidateRecord,
		std::wstring* outError)
	{
		auto* target = targetRecord.ControlInstance;
		auto* candidate = candidateRecord.ControlInstance;
		if (!target || !candidate)
		{
			SetError(outError, L"增量重载遇到无效控件实例。");
			return false;
		}
		std::map<std::wstring, bool> changedNames;
		for (const auto& [name, value] : currentProperties.Values)
		{
			const auto* found = nextProperties.Find(name);
			if (!found || *found != value)
				changedNames[name] = found != nullptr;
		}
		for (const auto& [name, value] : nextProperties.Values)
		{
			const auto* found = currentProperties.Find(name);
			if (!found || *found != value) changedNames[name] = true;
		}
		std::vector<std::wstring> ordered;
		ordered.reserve(changedNames.size());
		for (const auto& [name, present] : changedNames)
		{
			(void)present;
			ordered.push_back(name);
		}
		std::stable_sort(ordered.begin(), ordered.end(), [candidate](
			const std::wstring& left, const std::wstring& right)
		{
			const auto* leftMetadata = candidate->FindPropertyMetadata(left);
			const auto* rightMetadata = candidate->FindPropertyMetadata(right);
			if (leftMetadata && rightMetadata)
			{
				const auto& leftDesign = leftMetadata->Design();
				const auto& rightDesign = rightMetadata->Design();
				if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
					return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
				if (leftDesign.Order != rightDesign.Order)
					return leftDesign.Order < rightDesign.Order;
			}
			else if (leftMetadata != rightMetadata)
				return leftMetadata != nullptr;
			return left < right;
		});

		for (const auto& name : ordered)
		{
			const auto* metadata = candidate->FindPropertyMetadata(name);
			const auto canonical = metadata ? metadata->Name() : name;
			if (IsRuntimeBound(*target, canonical.c_str()))
			{
				SetError(outError, L"控件 “" + targetRecord.Name + L"” 的属性 “"
					+ canonical + L"” 正在绑定，当前不能原位覆盖其持久化值。");
				return false;
			}
			if (!changedNames[name])
			{
				(void)target->ClearDynamicResource(canonical);
				BindingValue candidateValue;
				if (candidate->TryGetPropertyValue(
					canonical, DependencyPropertyValueSource::Local, candidateValue))
				{
					if (!target->TrySetPropertyValue(canonical, candidateValue))
					{
						SetError(outError, L"控件 “" + targetRecord.Name
							+ L"” 无法恢复属性 “" + canonical + L"” 的候选值。");
						return false;
					}
				}
				else if (target->HasPropertyValue(
					canonical, DependencyPropertyValueSource::Local)
					&& !target->ClearPropertyValue(canonical))
				{
					SetError(outError, L"控件 “" + targetRecord.Name
						+ L"” 无法清除属性 “" + canonical + L"” 的本地值。");
					return false;
				}
				continue;
			}
			if (const auto dynamicResource =
				candidateRecord.MetadataPropertyDynamicResourceKeys.find(canonical);
				dynamicResource
					!= candidateRecord.MetadataPropertyDynamicResourceKeys.end())
			{
				if (!target->SetDynamicResource(
					canonical, dynamicResource->second))
				{
					SetError(outError, L"控件 “" + targetRecord.Name
						+ L"” 无法原位应用属性 “" + canonical
						+ L"” 的动态资源表达式。");
					return false;
				}
				continue;
			}
			BindingValue value;
			if (!candidate->TryGetPropertyValue(
				canonical, DependencyPropertyValueSource::Local, value)
				|| !target->TrySetPropertyValue(canonical, value))
			{
				SetError(outError, L"控件 “" + targetRecord.Name
					+ L"” 无法原位应用属性 “" + canonical + L"”。");
				return false;
			}
		}
		targetRecord.MetadataProperties = candidateRecord.MetadataProperties;
		targetRecord.MetadataPropertyResourceKeys =
			candidateRecord.MetadataPropertyResourceKeys;
		targetRecord.MetadataPropertyDynamicResourceKeys =
			candidateRecord.MetadataPropertyDynamicResourceKeys;
		return true;
	}

	bool ApplyControlPropertyChanges(
		const DesignNode& currentNode,
		const DesignNode& nextNode,
		DesignerControl& targetRecord,
		const DesignerControl& candidateRecord,
		std::wstring* outError)
	{
		auto* target = targetRecord.ControlInstance;
		auto* candidate = candidateRecord.ControlInstance;
		if (!target || !candidate)
		{
			SetError(outError, L"增量重载遇到无效控件实例。");
			return false;
		}
		if (currentNode.Properties.StyleResourceKey
			!= nextNode.Properties.StyleResourceKey)
			cui::framework::StyleAccess::SetResourceKey(
				*target,
				cui::framework::StyleAccess::ResourceKey(*candidate));

		return ApplyMetadataPropertyChanges(
			currentNode.Properties, nextNode.Properties,
			targetRecord, candidateRecord, outError);
	}
	bool ReadControlEventHandlers(
		const DesignNode& node,
		DesignEventHandlerMap& handlers,
		std::wstring* outError)
	{
		handlers.clear();
		for (const auto& [eventName, storedHandler] : node.Events)
		{
			if (!storedHandler.empty()) handlers.emplace(eventName, storedHandler);
		}
		return true;
	}

}

RuntimeDocument::RuntimeDocument()
	: _referenceState(
		std::make_shared<Detail::RuntimeDocumentReferenceState>())
{
	_referenceState->Document = this;
}

RuntimeDocument::RuntimeDocument(RuntimeDocument&& other) noexcept
{
	*this = std::move(other);
}

RuntimeDocument::~RuntimeDocument()
{
	if (_referenceState) _referenceState->Document = nullptr;
	ClearWindowEvents();
	ClearControlEvents();
	ClearDataBindings();
}

RuntimeDocument& RuntimeDocument::operator=(RuntimeDocument&& other) noexcept
{
	if (this == &other) return *this;
	auto referenceState = _referenceState;
	if (!referenceState)
		referenceState = std::move(other._referenceState);
	else if (other._referenceState)
		other._referenceState->Document = nullptr;

	ClearWindowEvents();
	ClearControlEvents();
	ClearDataBindings();

	_window = std::move(other._window);
	_dataContextSchema = std::move(other._dataContextSchema);
	_styleSheet = std::move(other._styleSheet);
	_dataContext = std::move(other._dataContext);
	_ownedContentRoot = std::move(other._ownedContentRoot);
	_contentRoot = other._contentRoot;
	_controls = std::move(other._controls);
	_collectionViews = std::move(other._collectionViews);
	_commandTargetReferences = std::move(other._commandTargetReferences);
	_inputBindingTargetReferences =
		std::move(other._inputBindingTargetReferences);
	_controlsByName = std::move(other._controlsByName);
	_installedBindings = std::move(other._installedBindings);
	_eventConnections = std::move(other._eventConnections);
	_windowEventConnections = std::move(other._windowEventConnections);
	_commandBindingConnections =
		std::move(other._commandBindingConnections);
	_windowCommandBindingConnections =
		std::move(other._windowCommandBindingConnections);
	_boundControlCommandHandlerCount =
		other._boundControlCommandHandlerCount;
	_boundWindowCommandHandlerCount =
		other._boundWindowCommandHandlerCount;
	_controlEventResolver = std::move(other._controlEventResolver);
	_windowEventResolver = std::move(other._windowEventResolver);
	_windowEventTarget = other._windowEventTarget;
	_appliedWindow = other._appliedWindow;
	_dataContextWindow = other._dataContextWindow;
	_commandTargetWindow = other._commandTargetWindow;
	_contentHost = std::move(other._contentHost);
	_nativeSurfaceBehaviors = std::move(other._nativeSurfaceBehaviors);
	_declarativeComponentBehaviors =
		std::move(other._declarativeComponentBehaviors);
	_allowNativeSurfacePlaceholder = other._allowNativeSurfacePlaceholder;
	_sourceDocument = std::move(other._sourceDocument);
	_pendingDocumentStyleSheet =
		std::move(other._pendingDocumentStyleSheet);
	_contentReleased = other._contentReleased;
	_referenceState = std::move(referenceState);
	other._boundControlCommandHandlerCount = 0;
	other._boundWindowCommandHandlerCount = 0;
	other._windowEventTarget = nullptr;
	other._dataContextWindow = nullptr;
	other._commandTargetWindow = nullptr;
	if (_referenceState) _referenceState->Document = this;
	return *this;
}

Control* RuntimeDocument::FindControlByName(const std::wstring& name) noexcept
{
	const auto found = _controlsByName.find(name);
	return found == _controlsByName.end() ? nullptr : found->second.Get();
}

const Control* RuntimeDocument::FindControlByName(
	const std::wstring& name) const noexcept
{
	return const_cast<RuntimeDocument*>(this)->FindControlByName(name);
}

void RuntimeDocument::RebuildControlIndex()
{
	std::unordered_map<std::wstring, ControlWeakReference> byName;
	byName.reserve(_controls.size());
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance
			|| control->NameIsGenerated) continue;
		byName.emplace(control->Name, control->ControlInstance);
	}
	_controlsByName = std::move(byName);
}

void RuntimeDocument::ReleaseSourceDocument() noexcept
{
	_sourceDocument.reset();
}

bool RuntimeDocument::HasWindowCommandTargetReferences() const noexcept
{
	return std::any_of(
		_commandTargetReferences.begin(), _commandTargetReferences.end(),
		[](const PendingCommandTargetReference& reference)
		{
			return reference.TargetsWindow;
		}) || std::any_of(
			_inputBindingTargetReferences.begin(),
			_inputBindingTargetReferences.end(),
			[](const PendingInputBindingTargetReference& reference)
			{
				return reference.TargetsWindow;
			});
}

bool RuntimeDocument::ApplyCommandTargetReferences(
	::Window* windowTarget,
	bool allowPendingWindow,
	std::vector<CommandTargetSnapshot>* rollback,
	std::wstring* outError)
{
	auto resolveRuntimeName = [&](const std::wstring& name) -> Control*
	{
		if (auto* direct = FindControlByName(name)) return direct;
		const auto firstSeparator = name.find(L'@');
		if (firstSeparator == std::wstring::npos) return nullptr;
		auto* current = FindControlByName(name.substr(0, firstSeparator));
		std::size_t partStart = firstSeparator + 1;
		while (current && partStart < name.size())
		{
			const auto separator = name.find(L'@', partStart);
			current = current->FindDeclarativeTemplatePart(name.substr(
				partStart, separator == std::wstring::npos
					? std::wstring::npos : separator - partStart));
			if (separator == std::wstring::npos) break;
			partStart = separator + 1;
		}
		return current;
	};
	auto resolveSource = [&](PendingCommandTargetReference& reference)
		-> Control*
	{
		auto* owner = resolveRuntimeName(reference.SourceName);
		// Unnamed controls deliberately do not enter the public Name index. Their
		// materialization record still owns a weak source locator for this internal
		// wiring pass. Named sources prefer namescope resolution so a recomposed
		// subtree follows the retained instance instead of its discarded placeholder.
		if (!owner) owner = reference.Source.Get();
		if (reference.MenuItemPath.empty()) return owner;
		if (!owner) return nullptr;
		const auto topIndex = reference.MenuItemPath.front();
		if (topIndex > static_cast<std::size_t>(
			(std::numeric_limits<int>::max)())) return nullptr;
		MenuItem* item = nullptr;
		if (auto* menu = dynamic_cast<Menu*>(owner))
			item = menu->GetItem(static_cast<int>(topIndex));
		else if (auto* menu = dynamic_cast<ContextMenu*>(owner))
			item = menu->GetItem(static_cast<int>(topIndex));
		if (!item) return nullptr;
		for (std::size_t depth = 1;
			depth < reference.MenuItemPath.size(); ++depth)
		{
			const auto index = reference.MenuItemPath[depth];
			if (index > static_cast<std::size_t>(
				(std::numeric_limits<int>::max)())) return nullptr;
			item = item->GetSubItem(static_cast<int>(index));
			if (!item) return nullptr;
		}
		return item;
	};
	auto capture = [](Control* source) -> CommandTargetSnapshot
	{
		CommandTargetSnapshot snapshot;
		snapshot.Source = source;
		if (auto* button = dynamic_cast<Button*>(source))
		{
			snapshot.Authored = button->HasAuthoredCommandTarget();
			snapshot.Target = button->CommandTarget;
		}
		else if (auto* item = dynamic_cast<MenuItem*>(source))
		{
			snapshot.Authored = item->HasAuthoredCommandTarget();
			snapshot.Target = item->CommandTarget;
		}
		return snapshot;
	};
	auto setTarget = [](Control* source, Control* target) -> bool
	{
		if (auto* button = dynamic_cast<Button*>(source))
		{
			button->SetCommandTarget(target);
			return true;
		}
		if (auto* item = dynamic_cast<MenuItem*>(source))
		{
			item->SetCommandTarget(target);
			return true;
		}
		return false;
	};

	std::vector<CommandTargetSnapshot> snapshots;
	snapshots.reserve(_commandTargetReferences.size()
		+ _inputBindingTargetReferences.size());
	Control pendingWindowTarget;
	for (auto& reference : _commandTargetReferences)
	{
		auto* source = resolveSource(reference);
		if (!source)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"CommandTarget source 无法解析："
				+ reference.SourceName);
			return false;
		}
		reference.Source = source;
		Control* target = nullptr;
		if (reference.TargetsWindow)
		{
			target = windowTarget;
			if (!target && allowPendingWindow) target = &pendingWindowTarget;
		}
		else target = resolveRuntimeName(reference.TargetName);
		if (!target)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"CommandTarget 无法解析："
				+ reference.SourceName + L" -> " + reference.TargetName);
			return false;
		}
		snapshots.push_back(capture(source));
		try
		{
			if (!setTarget(source, target))
			{
				RestoreCommandTargetSnapshots(snapshots);
				SetError(outError,
					L"CommandTarget source 不是 Button 或 MenuItem："
					+ reference.SourceName);
				return false;
			}
		}
		catch (...)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"应用 CommandTarget 时抛出异常："
				+ reference.SourceName);
			return false;
		}
	}
	for (auto& reference : _inputBindingTargetReferences)
	{
		auto* source = resolveRuntimeName(reference.SourceName);
		if (!source) source = reference.Source.Get();
		if (!source)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"InputBinding.CommandTarget source 无法解析："
				+ reference.SourceName);
			return false;
		}
		reference.Source = source;
		Control* target = nullptr;
		if (reference.TargetsWindow)
		{
			target = windowTarget;
			if (!target && allowPendingWindow) target = &pendingWindowTarget;
		}
		else target = resolveRuntimeName(reference.TargetName);
		if (!target)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"InputBinding.CommandTarget 无法解析："
				+ reference.SourceName + L" -> " + reference.TargetName);
			return false;
		}
		const auto current = source->GetInputBindings();
		if (reference.BindingIndex >= current.size())
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"InputBinding.CommandTarget source 索引失效："
				+ reference.SourceName);
			return false;
		}
		CommandTargetSnapshot snapshot;
		snapshot.Source = source;
		snapshot.IsInputBindingCollection = true;
		snapshot.InputBindings.assign(current.begin(), current.end());
		auto next = snapshot.InputBindings;
		std::visit([target](auto& binding)
		{
			binding.CommandTarget = target;
		}, next[reference.BindingIndex]);
		snapshots.push_back(std::move(snapshot));
		try
		{
			if (!source->SetInputBindings(std::move(next)))
			{
				RestoreCommandTargetSnapshots(snapshots);
				SetError(outError,
					L"控件拒绝已解析的 InputBinding.CommandTarget："
					+ reference.SourceName);
				return false;
			}
		}
		catch (...)
		{
			RestoreCommandTargetSnapshots(snapshots);
			SetError(outError, L"应用 InputBinding.CommandTarget 时抛出异常："
				+ reference.SourceName);
			return false;
		}
	}
	if (rollback)
		rollback->insert(
			rollback->end(), snapshots.begin(), snapshots.end());
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::RestoreCommandTargetSnapshots(
	const std::vector<CommandTargetSnapshot>& snapshots) noexcept
{
	Control expiredTargetPlaceholder;
	for (auto it = snapshots.rbegin(); it != snapshots.rend(); ++it)
	{
		auto* source = it->Source.Get();
		if (!source) continue;
		try
		{
			if (it->IsInputBindingCollection)
			{
				(void)source->SetInputBindings(it->InputBindings);
				continue;
			}
			if (auto* button = dynamic_cast<Button*>(source))
			{
				if (!it->Authored) button->ClearCommandTarget();
				else button->SetCommandTarget(
					it->Target.Get() ? it->Target.Get() : &expiredTargetPlaceholder);
			}
			else if (auto* item = dynamic_cast<MenuItem*>(source))
			{
				if (!it->Authored) item->ClearCommandTarget();
				else item->SetCommandTarget(
					it->Target.Get() ? it->Target.Get() : &expiredTargetPlaceholder);
			}
		}
		catch (...) {}
	}
}

void RuntimeDocument::RemoveDataBindings(
	std::vector<InstalledBinding>& installed) noexcept
{
	for (auto it = installed.rbegin(); it != installed.rend(); ++it)
	{
		auto* target = it->Target.Get();
		if (!target) continue;
		(void)target->DataBindings.Remove(it->Property);
	}
	installed.clear();
}

bool RuntimeDocument::InstallDataBindings(
	const std::shared_ptr<IBindingSource>& source,
	std::vector<InstalledBinding>& installed,
	std::wstring* outError,
	::Window* windowTarget,
	const DesignNode* windowNode,
	bool includeControls)
{
	const BindingSourceReference rootContext(source);
	const auto& rootNode = windowNode ? *windowNode : _window;
	auto namesEqual = [](const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	};
	auto findElement = [&](const std::wstring& name) -> Control*
	{
		if (windowTarget && namesEqual(name, rootNode.Name)) return windowTarget;
		return FindControlByName(name);
	};
	auto requiresWindowTarget = [&](const DesignerDataBinding& binding)
	{
		if (!binding.ElementName.empty()
			&& namesEqual(binding.ElementName, rootNode.Name)) return true;
		return std::any_of(
			binding.ChildBindings.begin(), binding.ChildBindings.end(),
			[&](const DesignerDataBinding& child)
			{
				return !child.ElementName.empty()
					&& namesEqual(child.ElementName, rootNode.Name);
			});
	};
	// A detached Content tree has no Window inheritance parent yet. Seed only
	// that temporary boundary; after mounting, the real Window owns the
	// inheritance context and must never be bypassed by a synthetic root value.
	if (auto* contentRoot = _contentRoot.Get();
		contentRoot && !contentRoot->GetInheritanceParent())
		cui::framework::XamlAccess::SetInheritedDataContext(
			*contentRoot, rootContext);
	struct BindingTarget final
	{
		Control* Instance = nullptr;
		DesignBindingMap Bindings;
		std::wstring Name;
		bool IsWindow = false;
	};
	std::vector<BindingTarget> targets;
	if (windowTarget)
		targets.push_back({ windowTarget, rootNode.Bindings,
			rootNode.Name.empty() ? std::wstring(L"Window") : rootNode.Name, true });
	if (includeControls)
	{
		targets.reserve(targets.size() + _controls.size());
		for (const auto& control : _controls)
			if (control && control->ControlInstance)
			{
				BindingTarget target;
				target.Instance = control->ControlInstance;
				target.Bindings.insert(
					control->DataBindings.begin(), control->DataBindings.end());
				target.Name = control->Name;
				targets.push_back(std::move(target));
			}
	}
	for (const auto& bindingTarget : targets)
	{
		if (!bindingTarget.Instance) continue;
		auto& target = *bindingTarget.Instance;
		const auto& targetName = bindingTarget.Name;
		for (const auto& [targetProperty, configuration] : bindingTarget.Bindings)
		{
			// A document is materialized before the native Window exists. Keep
			// self-root ElementName expressions authored and install them when the
			// complete XAML namescope is attached to its Window.
			if (!windowTarget && requiresWindowTarget(configuration)) continue;
			// A detached Content root cannot resolve a Binding on its own
			// DataContext until either an explicit runtime source or the real
			// Window inheritance parent exists. Keep that one expression authored;
			// every ordinary target still binds to its stable DataContext proxy.
			if (targetProperty == L"DataContext"
				&& !target.GetInheritanceParent() && !source)
				continue;
			if (configuration.IsMultiBinding())
			{
				std::wstring validationError;
				if (!DesignerBindingUtils::Validate(target, targetProperty,
					configuration, nullptr, &validationError, nullptr))
				{
					SetError(outError, L"控件 " + targetName + L"：" + validationError);
					RemoveDataBindings(installed);
					return false;
				}
				InstalledBinding state;
				state.Target = &target;
				state.Property = targetProperty;
				auto resolveSource = [&](const DesignerDataBinding& child,
					DesignerBindingUtils::ResolvedBindingSource& resolved,
					std::wstring* error)
				{
					if (!child.ElementName.empty())
					{
						resolved.Source = findElement(child.ElementName);
						if (!resolved.Source)
						{
							if (error) *error = L"ElementName 引用了不存在的控件："
								+ child.ElementName;
							return false;
						}
					}
					else if (child.RelativeSource == DesignerBindingRelativeSource::Self)
						resolved.Source = &target;
					else if (child.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
					{
						if (error) *error = L"公开文档树不能解析 TemplatedParent。";
						return false;
					}
					else if (child.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						resolved.OwnedSource = DesignerBindingUtils::CreateAncestorSource(
							target, child);
						resolved.Source = resolved.OwnedSource.Get();
					}
					else if (targetProperty == L"DataContext")
						resolved.Source = !bindingTarget.IsWindow
							&& target.GetInheritanceParent()
							? &target.GetInheritanceParent()->DataContextSource()
							: source.get();
					else
						resolved.Source = &target.DataContextSource();
					return true;
				};
				std::wstring installError;
				if (!DesignerBindingUtils::InstallBinding(target, targetProperty,
					configuration, resolveSource, &installError))
				{
					SetError(outError, L"控件 " + targetName + L"：" + installError);
					RemoveDataBindings(installed);
					return false;
				}
				installed.push_back(std::move(state));
				continue;
			}
			IBindingSource* bindingSource = nullptr;
			BindingSourceReference ownedBindingSource;
			DesignerDataContextSchema elementSourceSchema;
			const DesignerDataContextSchema* sourceSchema =
				_dataContextSchema.empty() ? nullptr : &_dataContextSchema;
			if (!configuration.ElementName.empty())
			{
				bindingSource = findElement(configuration.ElementName);
				if (!bindingSource)
				{
					SetError(outError, L"控件 " + targetName
						+ L" 的 ElementName 引用了不存在的控件："
						+ configuration.ElementName);
					RemoveDataBindings(installed);
					return false;
				}
					elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
					*bindingSource);
				sourceSchema = &elementSourceSchema;
			}
			else if (configuration.RelativeSource
				== DesignerBindingRelativeSource::Self)
			{
				bindingSource = &target;
				elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(target);
				sourceSchema = &elementSourceSchema;
			}
			else if (configuration.RelativeSource
				== DesignerBindingRelativeSource::TemplatedParent)
			{
				SetError(outError, L"控件 " + targetName
					+ L" 位于公开文档树，不能解析 TemplatedParent。");
				RemoveDataBindings(installed);
				return false;
			}
			else if (configuration.RelativeSource
				== DesignerBindingRelativeSource::FindAncestor)
			{
				ownedBindingSource = DesignerBindingUtils::CreateAncestorSource(
					target, configuration);
				bindingSource = ownedBindingSource.Get();
				if (auto* current = DesignerBindingUtils::FindAncestorSource(
					target, configuration))
				{
					elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*current);
					sourceSchema = &elementSourceSchema;
				}
				else sourceSchema = nullptr;
			}
			else if (targetProperty == L"DataContext")
			{
				bindingSource = !bindingTarget.IsWindow
					&& target.GetInheritanceParent()
					? &target.GetInheritanceParent()->DataContextSource()
					: source.get();
			}
			else
				bindingSource = &target.DataContextSource();
			if (!bindingSource)
			{
				// DataContext bindings remain authored but detached until the host
				// supplies a source. ElementName bindings do not depend on that step.
				continue;
			}
			if (bindingSource && configuration.ElementName.empty()
				&& configuration.RelativeSource
					== DesignerBindingRelativeSource::None)
			{
				elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
					*bindingSource);
				if (!elementSourceSchema.empty()) sourceSchema = &elementSourceSchema;
			}
			std::wstring validationError;
			if (!DesignerBindingUtils::Validate(
				target,
				targetProperty,
				configuration,
				nullptr,
				&validationError,
				sourceSchema))
			{
				SetError(outError, L"控件 " + targetName + L"：" + validationError);
				RemoveDataBindings(installed);
				return false;
			}

			std::shared_ptr<const IBindingValueConverter> converter;
			const auto converterName = DesignerBindingUtils::Trim(
				configuration.Converter);
			if (!converterName.empty())
			{
				converter = BindingValueConverterRegistry::Create(converterName);
				if (!converter)
				{
					SetError(outError, L"控件 " + targetName
						+ L"：无法创建 Converter：" + converterName);
					RemoveDataBindings(installed);
					return false;
				}
			}
			std::optional<BindingValue> fallbackValue;
			std::optional<BindingValue> targetNullValue;
			std::optional<BindingValue> converterParameter;
			std::wstring literalError;
			if (!DesignerBindingUtils::TryConvertOptionalLiteral(
				configuration.FallbackValue, fallbackValue, &literalError)
				|| !DesignerBindingUtils::TryConvertOptionalLiteral(
					configuration.TargetNullValue, targetNullValue, &literalError)
				|| !DesignerBindingUtils::TryConvertOptionalLiteral(
					configuration.ConverterParameter, converterParameter, &literalError))
			{
				SetError(outError, L"控件 " + targetName + L"：" + literalError);
				RemoveDataBindings(installed);
				return false;
			}

			InstalledBinding state;
			state.Target = &target;
			state.Property = targetProperty;

			auto* binding = ownedBindingSource
				? target.DataBindings.Add(
					targetProperty, std::move(ownedBindingSource),
					configuration.SourceProperty, configuration.Mode,
					configuration.UpdateMode, std::move(converter),
					std::move(fallbackValue), std::move(targetNullValue),
					std::move(converterParameter), configuration.StringFormat)
				: target.DataBindings.Add(
					targetProperty, *bindingSource, configuration.SourceProperty,
					configuration.Mode, configuration.UpdateMode,
					std::move(converter), std::move(fallbackValue),
					std::move(targetNullValue), std::move(converterParameter),
					configuration.StringFormat);
			if (!binding)
			{
				SetError(outError, L"控件 " + targetName + L"：绑定 "
					+ targetProperty + L" 失败："
					+ target.DataBindings.LastErrorMessage());
				RemoveDataBindings(installed);
				return false;
			}
			installed.push_back(std::move(state));
		}
	}
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::BindDataContext(
	std::shared_ptr<IBindingSource> source,
	std::wstring* outError)
{
	if (!source)
	{
		SetError(outError, L"未提供运行时 DataContext。");
		return false;
	}

	auto previousSource = _dataContext;
	auto* bindingWindow = _dataContextWindow
		? _dataContextWindow : _appliedWindow;
	std::optional<WindowDataContextSnapshot> windowContext;
	if (bindingWindow)
	{
		try
		{
			windowContext = WindowDataContextSnapshot::Capture(
				*bindingWindow);
		}
		catch (...)
		{
			SetError(outError,
				L"无法保存重新绑定前的 Window.DataContext 状态。");
			return false;
		}
		if (!windowContext->Apply(BindingSourceReference(source), outError))
			return false;
	}
	RemoveDataBindings(_installedBindings);
	if (auto* contentRoot = _contentRoot.Get();
		contentRoot && !contentRoot->GetInheritanceParent())
		cui::framework::XamlAccess::SetInheritedDataContext(*contentRoot, {});
	for (const auto& view : _collectionViews)
		if (view) view->BindDataContext(BindingSourceReference(source));
	std::vector<InstalledBinding> next;
	if (!InstallDataBindings(
		source, next, outError, bindingWindow, &_window))
	{
		if (windowContext) windowContext->Restore();
		for (const auto& view : _collectionViews)
			if (view) view->BindDataContext(
				BindingSourceReference(previousSource));
		std::vector<InstalledBinding> restored;
		(void)InstallDataBindings(
			previousSource, restored, nullptr, bindingWindow, &_window);
		_installedBindings = std::move(restored);
		return false;
	}
	_dataContext = std::move(source);
	_installedBindings = std::move(next);
	if (bindingWindow) _dataContextWindow = bindingWindow;
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearDataBindings()
{
	if (_installedBindings.empty() && !_dataContext) return;
	RemoveDataBindings(_installedBindings);
	// Once mounted, Window.DataContext is a real Window property with its own
	// lifetime. Clearing document bindings must not erase that host state.
	if (auto* contentRoot = _contentRoot.Get();
		contentRoot && !contentRoot->GetInheritanceParent())
		cui::framework::XamlAccess::SetInheritedDataContext(*contentRoot, {});
	for (const auto& view : _collectionViews)
		if (view) view->BindDataContext({});
	_dataContext.reset();
}

bool RuntimeDocument::BindControlEvents(
	const RuntimeControlEventResolver& resolver,
	std::wstring* outError)
{
	return BindControlEventsCore(
		resolver,
		_sourceDocument ? &*_sourceDocument : nullptr,
		outError);
}

bool RuntimeDocument::BindControlEventsCore(
	const RuntimeControlEventResolver& resolver,
	const DesignDocument* sourceDocument,
	std::wstring* outError)
{
	if (!resolver)
	{
		SetError(outError, L"未提供控件事件名称解析器。");
		return false;
	}

	std::vector<EventConnection> next;
	std::vector<std::pair<Control*, std::vector<CommandBinding>>>
		nextCommandBindings;
	size_t nextCommandHandlerCount = 0;
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance) continue;
		std::vector<CommandBinding> resolvedCommandBindings;
		for (const auto& [eventName, storedHandler] : control->EventHandlers)
		{
			auto publicEventName = eventName;
			auto eventOwnerType =
				control->ControlInstance->GetDeclarativeTypeId();
			DesignerComponentType attachedOwnerType;
			std::wstring attachedEventName;
			std::optional<DesignerEventDescriptor> descriptor;
			if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
				eventName, attachedOwnerType, attachedEventName))
			{
				const auto* owner = sourceDocument
					? sourceDocument->FindComponent(attachedOwnerType) : nullptr;
				if (owner)
				{
					const auto contract = std::find_if(
						owner->Events.begin(), owner->Events.end(),
						[&](const auto& event)
						{ return event.Name == attachedEventName; });
					if (contract != owner->Events.end())
						descriptor = DesignerEventCatalog::FromComponentEvent(*contract);
				}
				publicEventName = attachedEventName;
				eventOwnerType = RuntimeTypeId{
					attachedOwnerType.XamlNamespace,
					attachedOwnerType.XamlName };
			}
			else
				descriptor = DesignerEventCatalog::FindControlEvent(
					control->Type, eventName, control->ComponentEvents);
			if (!descriptor)
			{
				SetError(outError, L"控件 " + control->Name
					+ L" 包含未知事件：" + eventName);
				return false;
			}
			const auto handlerName = DesignerEventCatalog::NormalizeHandlerName(
				storedHandler);
			std::wstring validationError;
			if (handlerName.empty()
				|| !DesignerEventCatalog::ValidateHandlerName(
					handlerName, &validationError))
			{
				SetError(outError, L"控件 " + control->Name + L" 的事件 "
					+ publicEventName + L"：" + (validationError.empty()
						? std::wstring(L"处理函数名为空。") : validationError));
				return false;
			}

			RuntimeControlEventRequest request{
				*control->ControlInstance,
				control->Name,
				control->Type,
				eventOwnerType,
				*descriptor,
				handlerName };
			EventConnection connection;
			std::wstring resolverError;
			if (!resolver(request, connection, resolverError)
				|| !connection.Connected())
			{
				SetError(outError, L"控件 " + control->Name + L" 的事件 "
					+ publicEventName + L" 无法绑定到 " + handlerName
					+ (resolverError.empty() ? std::wstring{} : L"：" + resolverError));
				return false;
			}
			next.push_back(std::move(connection));
		}
		for (const auto& binding : control->CommandBindings)
		{
			CommandBinding resolvedBinding;
			resolvedBinding.Command = RoutedCommand(binding.Command);
			for (const auto& [eventName, storedHandler] : binding.HandlerRoutes())
			{
				if (!storedHandler || storedHandler->empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					control->Type, eventName, control->ComponentEvents);
				if (!descriptor)
				{
					SetError(outError, L"控件 " + control->Name
						+ L" 不公开命令事件：" + std::wstring(eventName));
					return false;
				}
				const auto handlerName =
					DesignerEventCatalog::NormalizeHandlerName(*storedHandler);
				std::wstring validationError;
				if (handlerName.empty()
					|| !DesignerEventCatalog::ValidateHandlerName(
						handlerName, &validationError))
				{
					SetError(outError, L"控件 " + control->Name
						+ L" 的 CommandBinding 处理器无效：" + validationError);
					return false;
				}
				RuntimeControlEventRequest request{
					*control->ControlInstance, control->Name,
					control->Type,
					control->ControlInstance->GetDeclarativeTypeId(),
					*descriptor, handlerName, binding.Command, &resolvedBinding };
				EventConnection connection;
				std::wstring resolverError;
				if (!resolver(request, connection, resolverError))
				{
					SetError(outError, L"控件 " + control->Name
						+ L" 的命令 " + binding.Command + L" 无法绑定到 "
						+ handlerName + (resolverError.empty()
							? std::wstring{} : L"：" + resolverError));
					return false;
				}
				const bool callbackResolved =
					eventName == L"PreviewCanExecute"
						? static_cast<bool>(resolvedBinding.PreviewCanExecute)
					: eventName == L"CanExecute"
						? static_cast<bool>(resolvedBinding.CanExecute)
					: eventName == L"PreviewExecuted"
						? static_cast<bool>(resolvedBinding.PreviewExecuted)
					: eventName == L"Executed"
						? static_cast<bool>(resolvedBinding.Executed)
					: false;
				if (!callbackResolved)
				{
					SetError(outError, L"控件 " + control->Name
						+ L" 的命令处理器没有进入 CommandBinding 集合："
						+ std::wstring(eventName));
					return false;
				}
				++nextCommandHandlerCount;
			}
			resolvedCommandBindings.push_back(std::move(resolvedBinding));
		}
		nextCommandBindings.emplace_back(
			control->ControlInstance, std::move(resolvedCommandBindings));
	}

	std::vector<EventConnection> nextCommandConnections;
	for (auto& [target, bindings] : nextCommandBindings)
	{
		if (!target) continue;
		for (auto& binding : bindings)
		{
			auto connection = target->AddCommandBinding(std::move(binding));
			if (!connection.Connected())
			{
				SetError(outError,
					L"控件拒绝了已解析的 XAML CommandBinding 集合。");
				return false;
			}
			nextCommandConnections.push_back(std::move(connection));
		}
	}
	_eventConnections = std::move(next);
	_commandBindingConnections = std::move(nextCommandConnections);
	_boundControlCommandHandlerCount = nextCommandHandlerCount;
	_controlEventResolver = resolver;
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearControlEvents() noexcept
{
	_eventConnections.clear();
	_commandBindingConnections.clear();
	_boundControlCommandHandlerCount = 0;
	_controlEventResolver = {};
}

bool RuntimeDocument::ApplyWindowProperties(
	::Window& form,
	std::wstring* outError)
{
	if (_appliedWindow && _appliedWindow != &form)
	{
		SetError(outError, L"运行时文档已经应用到另一个 Window。");
		return false;
	}
	std::optional<WindowPresentationSnapshot> presentation;
	try { presentation = WindowPresentationSnapshot::Capture(form); }
	catch (...)
	{
		SetError(outError, L"无法保存应用 XAML Window 前的显示状态。");
		return false;
	}
	const auto previousStyleSheet =
		cui::framework::StyleAccess::DocumentStyles(form);
	std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
	if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
		_styleSheet, runtimeStyleSheet, outError,
		_sourceDocument ? _sourceDocument->ResourceBasePath : std::wstring{},
		_sourceDocument ? _sourceDocument->Resources : nullptr,
		BuildStructuralStyleResourcesFor(
			_sourceDocument ? &*_sourceDocument : nullptr,
			_nativeSurfaceBehaviors,
			_declarativeComponentBehaviors,
			_allowNativeSurfacePlaceholder))
		|| !cui::framework::StyleAccess::SetDocumentStyles(
			form, runtimeStyleSheet, false)
		|| !ApplyWindowNode(_window, form, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		if (outError && outError->empty())
			*outError = L"无法将文档样式应用到 XAML Window。";
		return false;
	}
	if (!ApplyWindowInputBindings(_window, form,
		[&](const std::wstring& name) -> Control*
		{
			if (name == _window.Name) return &form;
			return FindControlByName(name);
		}, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}
	std::vector<CommandTargetSnapshot> commandTargetSnapshots;
	if (!ApplyCommandTargetReferences(
		&form, false, &commandTargetSnapshots, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}

	std::optional<WindowDataContextSnapshot> dataContextSnapshot;
	if (_dataContext)
	{
		try { dataContextSnapshot = WindowDataContextSnapshot::Capture(form); }
		catch (...)
		{
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				form, previousStyleSheet, false);
			presentation->Restore();
			SetError(outError,
				L"无法保存应用 XAML Window 前的 DataContext 状态。");
			return false;
		}
		if (!dataContextSnapshot->Apply(
			BindingSourceReference(_dataContext), outError))
		{
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				form, previousStyleSheet, false);
			presentation->Restore();
			return false;
		}
	}

	RemoveDataBindings(_installedBindings);
	std::vector<InstalledBinding> next;
	if (!InstallDataBindings(
		_dataContext, next, outError, &form, &_window))
	{
		RestoreCommandTargetSnapshots(commandTargetSnapshots);
		if (dataContextSnapshot) dataContextSnapshot->Restore();
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		std::vector<InstalledBinding> restored;
		(void)InstallDataBindings(_dataContext, restored, nullptr);
		_installedBindings = std::move(restored);
		return false;
	}
	_installedBindings = std::move(next);
	_appliedWindow = &form;
	if (HasWindowCommandTargetReferences()) _commandTargetWindow = &form;
	if (_dataContext) _dataContextWindow = &form;
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::BindWindowEvents(
	::Window& form,
	const RuntimeWindowEventResolver& resolver,
	std::wstring* outError)
{
	if (!resolver)
	{
		SetError(outError, L"未提供窗体事件名称解析器。");
		return false;
	}

	std::vector<EventConnection> next;
	std::vector<CommandBinding> nextCommandBindings;
	size_t nextCommandHandlerCount = 0;
	for (const auto& [eventName, storedHandler] : _window.Events)
	{
		const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
		if (!descriptor)
		{
			SetError(outError, L"窗体包含未知事件：" + eventName);
			return false;
		}
		const auto handlerName = DesignerEventCatalog::NormalizeHandlerName(
			storedHandler);
		std::wstring validationError;
		if (handlerName.empty()
			|| !DesignerEventCatalog::ValidateHandlerName(
				handlerName, &validationError))
		{
			SetError(outError, L"窗体事件 " + eventName + L"："
				+ (validationError.empty()
					? std::wstring(L"处理函数名为空。") : validationError));
			return false;
		}

		RuntimeWindowEventRequest request{
			form, _window.Name, *descriptor, handlerName };
		EventConnection connection;
		std::wstring resolverError;
		if (!resolver(request, connection, resolverError)
			|| !connection.Connected())
		{
			SetError(outError, L"窗体事件 " + eventName + L" 无法绑定到 "
				+ handlerName + (resolverError.empty()
					? std::wstring{} : L"：" + resolverError));
			return false;
		}
		next.push_back(std::move(connection));
	}
	for (const auto& binding : _window.CommandBindings)
	{
		CommandBinding resolvedBinding;
		resolvedBinding.Command = RoutedCommand(binding.Command);
		for (const auto& [eventName, storedHandler] : binding.HandlerRoutes())
		{
			if (!storedHandler || storedHandler->empty()) continue;
			const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
			if (!descriptor)
			{
				SetError(outError,
					L"Window 不公开命令事件：" + std::wstring(eventName));
				return false;
			}
			const auto handlerName =
				DesignerEventCatalog::NormalizeHandlerName(*storedHandler);
			std::wstring validationError;
			if (handlerName.empty()
				|| !DesignerEventCatalog::ValidateHandlerName(
					handlerName, &validationError))
			{
				SetError(outError, L"Window CommandBinding 处理器无效："
					+ validationError);
				return false;
			}
			RuntimeWindowEventRequest request{
				form, _window.Name, *descriptor, handlerName, binding.Command,
				&resolvedBinding };
			EventConnection connection;
			std::wstring resolverError;
			if (!resolver(request, connection, resolverError))
			{
				SetError(outError, L"Window 命令 " + binding.Command
					+ L" 无法绑定到 " + handlerName + (resolverError.empty()
						? std::wstring{} : L"：" + resolverError));
				return false;
			}
			const bool callbackResolved =
				eventName == L"PreviewCanExecute"
					? static_cast<bool>(resolvedBinding.PreviewCanExecute)
				: eventName == L"CanExecute"
					? static_cast<bool>(resolvedBinding.CanExecute)
				: eventName == L"PreviewExecuted"
					? static_cast<bool>(resolvedBinding.PreviewExecuted)
				: eventName == L"Executed"
					? static_cast<bool>(resolvedBinding.Executed)
				: false;
			if (!callbackResolved)
			{
				SetError(outError,
					L"Window 命令处理器没有进入 CommandBinding 集合："
					+ std::wstring(eventName));
				return false;
			}
			++nextCommandHandlerCount;
		}
		nextCommandBindings.push_back(std::move(resolvedBinding));
	}

	std::vector<EventConnection> nextCommandConnections;
	for (auto& binding : nextCommandBindings)
	{
		auto connection = form.AddCommandBinding(std::move(binding));
		if (!connection.Connected())
		{
			SetError(outError,
				L"Window 拒绝了已解析的 XAML CommandBinding 集合。");
			return false;
		}
		nextCommandConnections.push_back(std::move(connection));
	}
	_windowEventConnections = std::move(next);
	_windowCommandBindingConnections = std::move(nextCommandConnections);
	_boundWindowCommandHandlerCount = nextCommandHandlerCount;
	_windowEventTarget = &form;
	_windowEventResolver = resolver;
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearWindowEvents() noexcept
{
	_windowEventConnections.clear();
	_windowCommandBindingConnections.clear();
	_boundWindowCommandHandlerCount = 0;
	_windowEventResolver = {};
	_windowEventTarget = nullptr;
}

bool RuntimeDocument::AttachToWindow(
	::Window& form,
	const RuntimeWindowEventResolver& resolver,
	std::wstring* outError)
{
	return AttachToWindowCore(
		form, resolver,
		_sourceDocument ? &*_sourceDocument : nullptr,
		outError);
}

bool RuntimeDocument::AttachToWindowCore(
	::Window& form,
	const RuntimeWindowEventResolver& resolver,
	const DesignDocument* sourceDocument,
	std::wstring* outError)
{
	try
	{
		return AttachToWindowCore(
			form,
			std::make_shared<WindowRuntimeDocumentContentHost>(form),
			resolver,
			sourceDocument,
			outError);
	}
	catch (...)
	{
		SetError(outError, L"无法创建 Window 原子挂载适配器。");
		return false;
	}
}

bool RuntimeDocument::AttachToWindow(
	::Window& form,
	std::wstring* outError)
{
	return AttachToWindow(form, RuntimeWindowEventResolver{}, outError);
}

bool RuntimeDocument::AttachToWindow(
	::Window& form,
	std::shared_ptr<RuntimeDocumentContentHost> contentHost,
	const RuntimeWindowEventResolver& resolver,
	std::wstring* outError)
{
	return AttachToWindowCore(
		form, std::move(contentHost), resolver,
		_sourceDocument ? &*_sourceDocument : nullptr,
		outError);
}

bool RuntimeDocument::AttachToWindowCore(
	::Window& form,
	std::shared_ptr<RuntimeDocumentContentHost> contentHost,
	const RuntimeWindowEventResolver& resolver,
	const DesignDocument* sourceDocument,
	std::wstring* outError)
{
	if (!contentHost)
	{
		SetError(outError, L"未提供 Window 原子挂载的 Content 宿主适配器。");
		return false;
	}
	if (_contentReleased || _contentHost)
	{
		SetError(outError, L"运行时文档 Content 已经挂载或转移，不能重复挂载到 Window。");
		return false;
	}
	if (_appliedWindow || _windowEventTarget || _windowEventResolver
		|| !_windowEventConnections.empty())
	{
		SetError(outError,
			L"运行时文档已经存在独立 Window 附件；原子挂载只接受未挂载文档。");
		return false;
	}
	if (!resolver && (!_window.Events.empty()
		|| !_window.CommandBindings.empty()))
	{
		SetError(outError, L"动态文档包含 Window 事件，但原子挂载未提供名称解析器。");
		return false;
	}

	std::optional<WindowPresentationSnapshot> presentation;
	try
	{
		presentation = WindowPresentationSnapshot::Capture(form);
	}
	catch (...)
	{
		SetError(outError, L"无法保存 Window 原子挂载前的显示状态。");
		return false;
	}

	const auto previousStyleSheet =
		cui::framework::StyleAccess::DocumentStyles(form);
	std::shared_ptr<const ControlStyleSheet> runtimeStyleSheet =
		_pendingDocumentStyleSheet;
	if (!runtimeStyleSheet)
	{
		std::shared_ptr<ControlStyleSheet> builtStyleSheet;
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			_styleSheet, builtStyleSheet, outError,
			sourceDocument ? sourceDocument->ResourceBasePath : std::wstring{},
			sourceDocument ? sourceDocument->Resources : nullptr,
			BuildStructuralStyleResourcesFor(
				sourceDocument,
				_nativeSurfaceBehaviors,
				_declarativeComponentBehaviors,
				_allowNativeSurfacePlaceholder)))
		{
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				form, previousStyleSheet, false);
			presentation->Restore();
			if (outError && outError->empty())
				*outError = L"文档样式表无法应用到 XAML Window。";
			return false;
		}
		runtimeStyleSheet = std::move(builtStyleSheet);
	}
	if (!cui::framework::StyleAccess::SetDocumentStyles(
		form, runtimeStyleSheet, false))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		if (outError && outError->empty())
			*outError = L"文档样式表无法应用到 XAML Window。";
			return false;
	}
	if (!ApplyWindowNode(_window, form, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}
	if (!ApplyWindowInputBindings(_window, form,
		[&](const std::wstring& name) -> Control*
		{
			if (name == _window.Name) return &form;
			return FindControlByName(name);
		}, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}
	std::vector<CommandTargetSnapshot> commandTargetSnapshots;
	if (!ApplyCommandTargetReferences(
		&form, false, &commandTargetSnapshots, outError))
	{
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}
	const auto previousCommandTargetWindow = _commandTargetWindow;
	if (HasWindowCommandTargetReferences()) _commandTargetWindow = &form;
	auto rollbackCommandTargets = [&]() noexcept
	{
		RestoreCommandTargetSnapshots(commandTargetSnapshots);
		_commandTargetWindow = previousCommandTargetWindow;
	};
	if (resolver && !BindWindowEvents(form, resolver, outError))
	{
		rollbackCommandTargets();
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		return false;
	}
	std::optional<WindowDataContextSnapshot> dataContextSnapshot;
	if (_dataContext)
	{
		try
		{
			dataContextSnapshot = WindowDataContextSnapshot::Capture(form);
		}
		catch (...)
		{
			rollbackCommandTargets();
			ClearWindowEvents();
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				form, previousStyleSheet, false);
			presentation->Restore();
			SetError(outError,
				L"无法保存 Window 原子挂载前的 DataContext 状态。");
			return false;
		}
		if (!dataContextSnapshot->Apply(
			BindingSourceReference(_dataContext), outError))
		{
			rollbackCommandTargets();
			ClearWindowEvents();
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				form, previousStyleSheet, false);
			presentation->Restore();
			return false;
		}
	}
	RemoveDataBindings(_installedBindings);
	std::vector<InstalledBinding> mountedBindings;
	if (!InstallDataBindings(
		_dataContext, mountedBindings, outError, &form, &_window))
	{
		const auto failure = outError ? *outError : std::wstring{};
		rollbackCommandTargets();
		if (dataContextSnapshot) dataContextSnapshot->Restore();
		ClearWindowEvents();
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		std::vector<InstalledBinding> detachedBindings;
		(void)InstallDataBindings(
			_dataContext, detachedBindings, nullptr);
		_installedBindings = std::move(detachedBindings);
		SetError(outError, failure);
		return false;
	}
	_installedBindings = std::move(mountedBindings);

	if (!TransferContentRootTo(std::move(contentHost), outError))
	{
		const auto failure = outError ? *outError : std::wstring{};
		rollbackCommandTargets();
		RemoveDataBindings(_installedBindings);
		if (dataContextSnapshot) dataContextSnapshot->Restore();
		ClearWindowEvents();
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			form, previousStyleSheet, false);
		presentation->Restore();
		std::vector<InstalledBinding> detachedBindings;
		(void)InstallDataBindings(
			_dataContext, detachedBindings, nullptr);
		_installedBindings = std::move(detachedBindings);
		_appliedWindow = nullptr;
		SetError(outError, failure.empty()
			? std::wstring(L"Window Content 宿主拒绝原子挂载。") : failure);
		return false;
	}
	_appliedWindow = &form;
	_dataContextWindow = &form;
	_pendingDocumentStyleSheet.reset();
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::CommitInheritedWindowAttachments(
	RuntimeDocument& previous,
	const std::function<bool(std::wstring*)>& finalCommit,
	std::wstring* outError)
{
	std::optional<WindowPresentationSnapshot> presentation;
	std::optional<WindowDataContextSnapshot> dataContextSnapshot;
	std::shared_ptr<const ControlStyleSheet> previousWindowStyleSheet;
	bool previousWindowBindingsDetached = false;
	auto* appliedWindow = previous._appliedWindow;
	auto* dataContextWindow = previous._dataContextWindow
		? previous._dataContextWindow : appliedWindow;
	auto* commandTargetWindow = previous._commandTargetWindow
		? previous._commandTargetWindow : appliedWindow;
	std::vector<CommandTargetSnapshot> commandTargetSnapshots;
	if (!ApplyCommandTargetReferences(
		commandTargetWindow,
		commandTargetWindow == nullptr,
		&commandTargetSnapshots,
		outError)) return false;

	auto detachPreviousWindowBindings = [&]() noexcept
	{
		if (!appliedWindow) return;
		for (auto it = previous._installedBindings.begin();
			it != previous._installedBindings.end();)
		{
			if (it->Target != appliedWindow)
			{
				++it;
				continue;
			}
			(void)appliedWindow->DataBindings.Remove(it->Property);
			it = previous._installedBindings.erase(it);
		}
		previousWindowBindingsDetached = true;
	};
	auto restorePreviousWindowBindings = [&]() noexcept
	{
		if (!previousWindowBindingsDetached || !appliedWindow) return;
		try
		{
			std::vector<RuntimeDocument::InstalledBinding> restored;
			if (previous.InstallDataBindings(
				previous._dataContext, restored, nullptr,
				appliedWindow, &previous._window, false))
				previous._installedBindings.insert(
					previous._installedBindings.end(),
					std::make_move_iterator(restored.begin()),
					std::make_move_iterator(restored.end()));
		}
		catch (...) {}
		previousWindowBindingsDetached = false;
	};
	auto rollback = [&]() noexcept
	{
		RestoreCommandTargetSnapshots(commandTargetSnapshots);
		ClearWindowEvents();
		RemoveDataBindings(_installedBindings);
		if (dataContextSnapshot) dataContextSnapshot->Restore();
		if (appliedWindow)
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				*appliedWindow, previousWindowStyleSheet, false);
		if (presentation) presentation->Restore();
		restorePreviousWindowBindings();
	};

	if (appliedWindow)
	{
		try
		{
			presentation =
				WindowPresentationSnapshot::Capture(*appliedWindow);
			previousWindowStyleSheet =
				cui::framework::StyleAccess::DocumentStyles(*appliedWindow);
			if (_dataContext && dataContextWindow)
				dataContextSnapshot =
					WindowDataContextSnapshot::Capture(*dataContextWindow);
		}
		catch (...)
		{
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			SetError(outError, L"无法保存热重载前的 Window 显示状态。");
			return false;
		}
		detachPreviousWindowBindings();
		std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			_styleSheet, runtimeStyleSheet, outError,
			_sourceDocument ? _sourceDocument->ResourceBasePath : std::wstring{},
			_sourceDocument ? _sourceDocument->Resources : nullptr,
			BuildStructuralStyleResourcesFor(
				_sourceDocument ? &*_sourceDocument : nullptr,
				_nativeSurfaceBehaviors,
				_declarativeComponentBehaviors,
				_allowNativeSurfacePlaceholder))
			|| !cui::framework::StyleAccess::SetDocumentStyles(
				*appliedWindow, runtimeStyleSheet, false)
			|| (!HasSameWindowPresentation(previous._window, _window)
				&& !ApplyWindowNode(
					_window, *appliedWindow, outError, &previous._window))
			|| !ApplyWindowInputBindings(_window, *appliedWindow,
				[&](const std::wstring& name) -> Control*
				{
					if (name == _window.Name) return appliedWindow;
					return FindControlByName(name);
				}, outError))
		{
			rollback();
			if (outError && outError->empty())
				*outError = L"无法将重载样式应用到 XAML Window。";
			return false;
		}
		_appliedWindow = appliedWindow;
	}
	if (_dataContext && dataContextWindow)
	{
		if (!dataContextSnapshot->Apply(
			BindingSourceReference(_dataContext), outError))
		{
			rollback();
			return false;
		}
	}
	if (appliedWindow)
	{
		RemoveDataBindings(_installedBindings);
		std::vector<InstalledBinding> mountedBindings;
		if (!InstallDataBindings(
			_dataContext, mountedBindings, outError,
			appliedWindow, &_window))
		{
			rollback();
			return false;
		}
		_installedBindings = std::move(mountedBindings);
	}

	if (previous._windowEventTarget && previous._windowEventResolver)
	{
		if (!BindWindowEvents(
			*previous._windowEventTarget,
			previous._windowEventResolver,
			outError))
		{
			rollback();
			return false;
		}
	}
	else if (previous._appliedWindow && (!_window.Events.empty()
		|| !_window.CommandBindings.empty()))
	{
		rollback();
		SetError(outError,
			L"热重载后的动态文档包含 Window 事件，但宿主没有保留名称解析器。");
		return false;
	}

	bool committed = true;
	try { if (finalCommit) committed = finalCommit(outError); }
	catch (...)
	{
		committed = false;
		SetError(outError, L"提交 Window 运行时附件时抛出异常。");
	}
	if (committed)
	{
		_dataContextWindow = dataContextWindow;
		_commandTargetWindow = HasWindowCommandTargetReferences()
			? commandTargetWindow : nullptr;
		previousWindowBindingsDetached = false;
		return true;
	}

	rollback();
	return false;
}

std::unique_ptr<Control> RuntimeDocument::ReleaseContentRoot()
{
	if (_contentReleased) return {};
	// A raw ownership transfer has no lifetime adapter. Detach every runtime
	// attachment while Content is still guaranteed to be alive so the document
	// can later be destroyed without dereferencing an external tree.
	ClearWindowEvents();
	ClearControlEvents();
	ClearDataBindings();
	(void)ApplyCommandTargetReferences(nullptr, true, nullptr, nullptr);
	_contentReleased = true;
	_contentHost.reset();
	_dataContextWindow = nullptr;
	_commandTargetWindow = nullptr;
	return std::move(_ownedContentRoot);
}

bool RuntimeDocument::TransferContentRootTo(
	std::shared_ptr<RuntimeDocumentContentHost> host,
	std::wstring* outError)
{
	if (!host)
	{
		SetError(outError, L"未提供运行时 Content 宿主适配器。");
		return false;
	}
	if (_contentReleased)
	{
		SetError(outError, L"运行时文档 Content 所有权已经转移。");
		return false;
	}
	std::vector<CommandTargetSnapshot> commandTargetSnapshots;
	if (!ApplyCommandTargetReferences(
		_commandTargetWindow,
		!HasWindowCommandTargetReferences(),
		&commandTargetSnapshots,
		outError)) return false;
	try
	{
		if (!host->AttachContent(
			_ownedContentRoot, RuntimeContentHostAttachMode::Initial, outError))
		{
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			return false;
		}
	}
	catch (...)
	{
		RestoreCommandTargetSnapshots(commandTargetSnapshots);
		SetError(outError, L"Content 宿主初始挂载抛出异常。");
		return false;
	}
	_contentReleased = true;
	_contentHost = std::move(host);
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::TransferContentRootTo(
	::Window& form,
	std::wstring* outError)
{
	std::vector<CommandTargetSnapshot> commandTargetSnapshots;
	const auto previousCommandTargetWindow = _commandTargetWindow;
	try
	{
		if (!ApplyCommandTargetReferences(
			&form, false, &commandTargetSnapshots, outError)) return false;
		if (HasWindowCommandTargetReferences()) _commandTargetWindow = &form;
		auto dataContextSnapshot = WindowDataContextSnapshot::Capture(form);
		if (_dataContext && !dataContextSnapshot.Apply(
			BindingSourceReference(_dataContext), outError))
		{
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			_commandTargetWindow = previousCommandTargetWindow;
			return false;
		}
		if (!TransferContentRootTo(
			std::make_shared<WindowRuntimeDocumentContentHost>(form), outError))
		{
			dataContextSnapshot.Restore();
			RestoreCommandTargetSnapshots(commandTargetSnapshots);
			_commandTargetWindow = previousCommandTargetWindow;
			return false;
		}
		_dataContextWindow = &form;
		return true;
	}
	catch (...)
	{
		RestoreCommandTargetSnapshots(commandTargetSnapshots);
		_commandTargetWindow = previousCommandTargetWindow;
		SetError(outError, L"无法创建 Window Content 宿主适配器。");
		return false;
	}
}

bool RuntimeDocumentLoader::Load(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return LoadCore(
		document, output, options, false, false, outError, outDiagnostic);
}

bool RuntimeDocumentLoader::LoadCore(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	bool deferDataBindings,
	bool borrowSourceDocumentForAttach,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		if (output._contentReleased
			|| output._appliedWindow
			|| output._windowEventTarget)
		{
			SetError(outError,
				L"运行时文档已经附加到外部 Window 或 Content 宿主；"
				L"请使用 Reload 保持宿主事务。");
			return false;
		}
		DesignDocumentEventIndex eventIndex;
		if (!DesignDocumentEventIndex::Build(
			document, eventIndex, outError)) return false;
		CuiRuntime::XamlObjectTree materialized;
		if (!CuiRuntime::XamlObjectMaterializer::Materialize(
			document, materialized,
			MaterializationOptionsFor(options), outError, outDiagnostic)) return false;

		RuntimeDocument candidate;
		// A detached Load must retain the declaration until a later attach (the
		// caller may release it explicitly afterwards). LoadIntoWindow, however,
		// can borrow its caller-owned document through the immediate atomic attach
		// and avoid the otherwise large DesignDocument copy when retention is off.
		if (options.RetainSourceDocument || !borrowSourceDocumentForAttach)
			candidate._sourceDocument = document;
		const auto* sourceDocument = candidate._sourceDocument
			? &*candidate._sourceDocument : &document;
		candidate._window = document.Window;
		candidate._dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(
			candidate._dataContextSchema);
		candidate._styleSheet = document.StyleSheet;
		candidate._nativeSurfaceBehaviors = options.NativeSurfaceBehaviors;
		candidate._declarativeComponentBehaviors =
			options.DeclarativeComponentBehaviors;
		candidate._allowNativeSurfacePlaceholder =
			options.AllowNativeSurfacePlaceholder;
		candidate._controls = std::move(materialized.Controls);
		candidate._collectionViews = std::move(materialized.CollectionViews);
		candidate._commandTargetReferences.reserve(
			materialized.CommandTargets.size());
		for (auto& reference : materialized.CommandTargets)
			candidate._commandTargetReferences.push_back({
				reference.Source,
				std::move(reference.SourceName),
				std::move(reference.MenuItemPath),
				std::move(reference.TargetName),
				reference.TargetsWindow });
		candidate._inputBindingTargetReferences.reserve(
			materialized.InputBindingTargets.size());
		for (auto& reference : materialized.InputBindingTargets)
			candidate._inputBindingTargetReferences.push_back({
				reference.Source,
				std::move(reference.SourceName),
				reference.BindingIndex,
				std::move(reference.TargetName),
				reference.TargetsWindow });
		candidate._ownedContentRoot = std::move(materialized.ContentRoot);
		candidate._contentRoot = candidate._ownedContentRoot.get();
		candidate._pendingDocumentStyleSheet =
			std::move(materialized.DocumentStyleSheet);
		candidate.RebuildControlIndex();
		if (!candidate.ApplyCommandTargetReferences(
			nullptr, true, nullptr, outError)) return false;

		if (deferDataBindings)
		{
			candidate._dataContext = options.DataContext;
			for (const auto& view : candidate._collectionViews)
				if (view) view->BindDataContext(
					BindingSourceReference(options.DataContext));
		}
		else if (options.DataContext)
		{
			if (!candidate.BindDataContext(options.DataContext, outError))
				return false;
		}
		else
		{
			std::vector<RuntimeDocument::InstalledBinding> installed;
			if (!candidate.InstallDataBindings({}, installed, outError))
				return false;
			candidate._installedBindings = std::move(installed);
		}
		if (options.ControlEventResolver)
		{
			if (!candidate.BindControlEventsCore(
				options.ControlEventResolver, sourceDocument, outError))
				return false;
		}
		else if (options.RequireControlEventResolver
			&& HasConfiguredControlEvents(candidate))
		{
			SetError(outError, L"文档包含控件事件，但加载选项未提供事件名称解析器。");
			return false;
		}

		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态文档加载失败：控件构建或资源初始化抛出异常。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态文档加载失败：发生未知异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXaml(
	const std::string& xaml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::FromXaml(
			xaml, document, options.ParseOptions,
			outError, outDiagnostic)) return false;
		return Load(document, output, options, outError, outDiagnostic);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态 XAML 加载失败：文档格式无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 加载失败：发生未知解析异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXamlFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::LoadFromFile(
			filePath, document, options.ParseOptions,
			outError, outDiagnostic)) return false;
		return Load(document, output, options, outError, outDiagnostic);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态 XAML 文件加载失败：文件内容无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 文件加载失败：发生未知读取异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadIntoWindow(
	const DesignDocument& document,
	::Window& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeWindowEventResolver& formResolver,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	if (output._contentReleased
		|| output._appliedWindow
		|| output._windowEventTarget)
	{
		SetError(outError,
			L"输出运行时文档已经附加到外部 Window 或 Content 宿主；"
			L"请使用 Reload。");
		return false;
	}

	RuntimeDocument candidate;
	if (!LoadCore(
		document, candidate, options, true, true,
		outError, outDiagnostic)) return false;
	const auto* attachSourceDocument = candidate._sourceDocument
		? &*candidate._sourceDocument : &document;
	if (!candidate.AttachToWindowCore(
		form, formResolver, attachSourceDocument, outError)) return false;
	output = std::move(candidate);
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocumentLoader::LoadXamlIntoWindow(
	const std::string& xaml,
	::Window& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeWindowEventResolver& formResolver,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::FromXaml(
			xaml, document, options.ParseOptions,
			outError, outDiagnostic)) return false;
		return LoadIntoWindow(
			document, form, output, options, formResolver, outError, outDiagnostic);
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 原子挂载失败：文档格式无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXamlFileIntoWindow(
	const std::wstring& filePath,
	::Window& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeWindowEventResolver& formResolver,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::LoadFromFile(
			filePath, document, options.ParseOptions,
			outError, outDiagnostic)) return false;
		return LoadIntoWindow(
			document, form, output, options, formResolver, outError, outDiagnostic);
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 文件原子挂载失败：文件内容无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::ReloadHosted(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& effectiveOptions,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	auto host = output._contentHost;
	if (!host || !output._contentReleased)
	{
		SetError(outError, L"运行时文档没有处于外部宿主管理状态。");
		return false;
	}

	std::unique_ptr<Control> previousContent;
	try
	{
		if (!host->DetachContent(
			output._contentRoot.Get(), previousContent, outError)) return false;
	}
	catch (...)
	{
		if (previousContent)
		{
			output._ownedContentRoot = std::move(previousContent);
			output._contentRoot = output._ownedContentRoot.get();
			output._contentReleased = false;
			output._contentHost.reset();
		}
		SetError(outError, L"Content 宿主分离旧 Content 时抛出异常。");
		return false;
	}
	output._ownedContentRoot = std::move(previousContent);
	output._contentRoot = output._ownedContentRoot.get();
	output._contentReleased = false;

	auto restorePreviousContent = [&](std::wstring failure) -> bool
	{
		auto content = std::move(output._ownedContentRoot);
		std::wstring restoreError;
		bool restored = false;
		try
		{
			restored = host->AttachContent(
				content, RuntimeContentHostAttachMode::Rollback, &restoreError);
		}
		catch (...)
		{
			restoreError = L"Content 宿主回滚抛出异常。";
		}
		if (restored)
		{
			output._contentReleased = true;
			output._ownedContentRoot.reset();
			SetError(outError, std::move(failure));
			return false;
		}

		output._ownedContentRoot = std::move(content);
		output._contentRoot = output._ownedContentRoot.get();
		output._contentReleased = false;
		output._contentHost.reset();
		if (!restoreError.empty())
			failure += L"；旧 Content 恢复也失败：" + restoreError;
		SetError(outError, std::move(failure));
		return false;
	};

	auto attachCandidateContent = [host](
		RuntimeDocument& candidate,
		std::wstring* commitError) -> bool
	{
		auto content = std::move(candidate._ownedContentRoot);
		bool committed = false;
		try
		{
			committed = host->AttachContent(
				content, RuntimeContentHostAttachMode::Replacement, commitError);
		}
		catch (...)
		{
			SetError(commitError, L"Content 宿主提交候选 Content 时抛出异常。");
		}
		if (!committed)
		{
			candidate._ownedContentRoot = std::move(content);
			candidate._contentRoot = candidate._ownedContentRoot.get();
			return false;
		}
		candidate._contentReleased = true;
		candidate._contentHost = host;
		candidate._ownedContentRoot.reset();
		return true;
	};
	auto commitCandidate = [&output, &attachCandidateContent](
		RuntimeDocument& candidate,
		std::wstring* commitError) -> bool
	{
		return candidate.CommitInheritedWindowAttachments(
			output,
			[&](std::wstring* finalError)
			{
				return attachCandidateContent(candidate, finalError);
			},
			commitError);
	};

	bool recomposed = false;
	size_t reusedControlCount = 0;
	if (!RuntimeDocumentTopologyReloader::TryReload(
		document,
		output,
		effectiveOptions,
		recomposed,
		reusedControlCount,
		outError,
		commitCandidate))
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主拓扑重组失败。");
		return restorePreviousContent(failure);
	}
	if (recomposed)
	{
		(void)reusedControlCount;
		if (outMode) *outMode = RuntimeDocumentReloadMode::Recomposed;
		if (outError) outError->clear();
		return true;
	}

	RuntimeDocument candidate;
	if (!Load(document, candidate, effectiveOptions, outError, outDiagnostic))
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主替换候选加载失败。");
		return restorePreviousContent(failure);
	}
	bool candidateCommitted = false;
	try { candidateCommitted = commitCandidate(candidate, outError); }
	catch (...)
	{
		SetError(outError, L"宿主提交替换 Content 时抛出异常。");
	}
	if (!candidateCommitted)
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主拒绝提交替换 Content。");
		return restorePreviousContent(failure);
	}

	output = std::move(candidate);
	if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocumentLoader::Reload(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	try
	{
		RuntimeDocumentLoadOptions inheritedOptions = options;
		if (!inheritedOptions.NativeSurfaceBehaviors)
			inheritedOptions.NativeSurfaceBehaviors = output._nativeSurfaceBehaviors;
		if (!inheritedOptions.DeclarativeComponentBehaviors)
			inheritedOptions.DeclarativeComponentBehaviors =
				output._declarativeComponentBehaviors;
		if (options.NativeSurfaceBehaviors
			|| options.DeclarativeComponentBehaviors)
			inheritedOptions.ForceBehaviorRefresh = true;
		if (!options.NativeSurfaceBehaviors && output._allowNativeSurfacePlaceholder)
			inheritedOptions.AllowNativeSurfacePlaceholder = true;
		DesignDocumentEventIndex eventIndex;
		if (!DesignDocumentEventIndex::Build(
			document, eventIndex, outError)) return false;

		const bool sameSourceDocument = output._sourceDocument
			&& *output._sourceDocument == document;
		const bool hasExplicitRuntimeChange = options.DataContext
			|| options.ControlEventResolver
			|| options.RequireControlEventResolver
			|| options.NativeSurfaceBehaviors
			|| options.DeclarativeComponentBehaviors
			|| options.AllowNativeSurfacePlaceholder
			|| options.ForceBehaviorRefresh
			;
		if (sameSourceDocument
			&& !options.ForceResourceRefresh
			&& !hasExplicitRuntimeChange)
		{
			if (outMode) *outMode = RuntimeDocumentReloadMode::Unchanged;
			if (outError) outError->clear();
			return true;
		}

		if (output._sourceDocument
			&& !(sameSourceDocument && options.ForceResourceRefresh)
			&& !options.NativeSurfaceBehaviors
			&& !options.DeclarativeComponentBehaviors
			&& !options.AllowNativeSurfacePlaceholder
			&& !options.ForceBehaviorRefresh
			&& CanReloadInPlace(*output._sourceDocument, document))
		{
			std::unordered_map<std::wstring, const DesignNode*> currentByName;
			currentByName.reserve(output._sourceDocument->Nodes.size());
			for (const auto& node : output._sourceDocument->Nodes)
				currentByName.emplace(node.Name, &node);
			std::unordered_map<std::wstring, const DesignNode*> nextByName;
			nextByName.reserve(document.Nodes.size());
			for (const auto& node : document.Nodes)
				nextByName.emplace(node.Name, &node);

			const bool hasPropertyChanges = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					const auto found = currentByName.find(node.Name);
					return found == currentByName.end()
						|| found->second->Properties != node.Properties;
				});
			const bool hasControlBindingChanges = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					const auto found = currentByName.find(node.Name);
					return found == currentByName.end()
						|| found->second->Bindings != node.Bindings;
				});
			const bool hasWindowBindingChanges =
				output._window.Bindings != document.Window.Bindings;
			const bool hasBindingChanges =
				hasControlBindingChanges || hasWindowBindingChanges;
			const bool hasStyleChanges =
				!(output._sourceDocument->StyleSheet == document.StyleSheet);
			const bool hasSchemaChanges =
				output._sourceDocument->DataContextSchema != document.DataContextSchema;
			const bool needsCandidate = hasPropertyChanges
				|| hasBindingChanges || hasStyleChanges || hasSchemaChanges;
			CuiRuntime::XamlObjectTree reloadCandidate;
			std::unordered_map<std::wstring, const DesignerControl*> candidateByName;
			if (needsCandidate)
			{
				auto candidateOptions =
					MaterializationOptionsFor(inheritedOptions);
				// This tree is only a typed property/binding snapshot. Attaching
				// application behaviors here would publish side effects for a tree
				// that is intentionally discarded after an in-place reload.
				candidateOptions.NativeSurfaceBehaviorFactory = {};
				candidateOptions.DeclarativeComponentBehaviorFactory = {};
				candidateOptions.AllowNativeSurfacePlaceholder = true;
				if (!CuiRuntime::XamlObjectMaterializer::Materialize(
					document, reloadCandidate,
					candidateOptions, outError, outDiagnostic)) return false;
				candidateByName.reserve(reloadCandidate.Controls.size());
				for (const auto& control : reloadCandidate.Controls)
					if (control) candidateByName.emplace(control->Name, control.get());
			}

			std::vector<DesignEventHandlerMap> nextHandlers;
			std::vector<DesignEventHandlerMap> previousHandlers;
			std::vector<std::map<std::wstring, DesignerDataBinding>> nextBindings;
			std::vector<std::map<std::wstring, DesignerDataBinding>> previousBindings;
			std::vector<InPlaceControlSnapshot> propertySnapshots;
			nextHandlers.reserve(output._controls.size());
			previousHandlers.reserve(output._controls.size());
			nextBindings.reserve(output._controls.size());
			previousBindings.reserve(output._controls.size());
			propertySnapshots.reserve(output._controls.size());
			auto rollbackProperties = [&]() noexcept
			{
				for (auto it = propertySnapshots.rbegin();
					it != propertySnapshots.rend(); ++it) it->Restore();
			};
			bool hasNextEvents = false;
			for (const auto& control : output._controls)
			{
				if (!control)
				{
					SetError(outError, L"增量重载遇到无效控件记录。");
					rollbackProperties();
					return false;
				}
				const auto found = nextByName.find(control->Name);
				if (found == nextByName.end())
				{
					SetError(outError, L"增量重载无法解析控件 x:Name："
						+ control->Name);
					rollbackProperties();
					return false;
				}
				const auto currentFound = currentByName.find(control->Name);
				if (currentFound == currentByName.end())
				{
					SetError(outError, L"增量重载无法解析旧控件 x:Name："
						+ control->Name);
					rollbackProperties();
					return false;
				}
				if (currentFound->second->Properties
					!= found->second->Properties)
				{
					const auto candidateFound = candidateByName.find(control->Name);
					if (candidateFound == candidateByName.end()
						|| !candidateFound->second)
					{
						SetError(outError, L"增量重载候选树缺少控件 x:Name："
							+ control->Name);
						rollbackProperties();
						return false;
					}
					propertySnapshots.push_back(
						InPlaceControlSnapshot::Capture(*control));
					if (!ApplyControlPropertyChanges(
						*currentFound->second,
						*found->second,
						*control,
						*candidateFound->second,
						outError))
					{
						const auto inPlaceError = outError ? *outError : std::wstring{};
						rollbackProperties();
						if (!output._contentReleased)
						{
							RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
							if (!effectiveOptions.DataContext)
								effectiveOptions.DataContext = output._dataContext;
							if (!effectiveOptions.ControlEventResolver)
								effectiveOptions.ControlEventResolver =
									output._controlEventResolver;
							RuntimeDocument replacement;
							if (!Load(
									document, replacement, effectiveOptions, outError, outDiagnostic)) return false;
							if (!replacement.CommitInheritedWindowAttachments(
								output, {}, outError)) return false;
							output = std::move(replacement);
							if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
							if (outError) outError->clear();
							return true;
						}
						if (output._contentHost)
						{
							RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
							if (!effectiveOptions.DataContext)
								effectiveOptions.DataContext = output._dataContext;
							if (!effectiveOptions.ControlEventResolver)
								effectiveOptions.ControlEventResolver =
									output._controlEventResolver;
							return ReloadHosted(
								document, output, effectiveOptions, outMode, outError,
								outDiagnostic);
						}
						SetError(outError, inPlaceError);
						return false;
					}
				}
				if (hasBindingChanges)
				{
					const auto candidateFound = candidateByName.find(control->Name);
					if (candidateFound == candidateByName.end()
						|| !candidateFound->second)
					{
						SetError(outError, L"增量重载候选树缺少绑定控件 x:Name："
							+ control->Name);
						rollbackProperties();
						return false;
					}
					nextBindings.push_back(candidateFound->second->DataBindings);
					previousBindings.push_back(control->DataBindings);
				}
				DesignEventHandlerMap handlers;
				if (!ReadControlEventHandlers(
					*found->second, handlers, outError))
				{
					rollbackProperties();
					return false;
				}
				hasNextEvents = hasNextEvents || !handlers.empty();
				nextHandlers.push_back(std::move(handlers));
				previousHandlers.push_back(control->EventHandlers);
			}

			auto nextSourceDocument = document;
			auto nextWindow = document.Window;
			const auto previousWindow = output._window;
			const bool windowPresentationChanged =
				!HasSameWindowPresentation(previousWindow, nextWindow);
			std::optional<WindowPresentationSnapshot> previousWindowPresentation;
			auto rollbackWindowPresentation = [&]() noexcept
			{
				if (previousWindowPresentation)
					previousWindowPresentation->Restore();
			};
			if (output._appliedWindow && windowPresentationChanged)
			{
				try
				{
					previousWindowPresentation =
						WindowPresentationSnapshot::Capture(*output._appliedWindow);
				}
				catch (...)
				{
					rollbackProperties();
					SetError(outError,
						L"无法保存原位重载前的 Window 显示状态。");
					return false;
				}
				if (!ApplyWindowNode(
					nextWindow, *output._appliedWindow, outError, &previousWindow))
				{
					const auto reloadError = outError ? *outError : std::wstring{};
					rollbackWindowPresentation();
					rollbackProperties();
					SetError(outError, reloadError);
					return false;
				}
			}
			output._window = nextWindow;
			const auto previousDataContext = output._dataContext;
			const auto previousDataContextSchema = output._dataContextSchema;
			auto nextDataContextSchema = document.DataContextSchema;
			DesignerDataContextSchemaUtils::Canonicalize(nextDataContextSchema);
			if (hasSchemaChanges)
				output._dataContextSchema = std::move(nextDataContextSchema);
			const auto nextDataContext = options.DataContext
				? options.DataContext : previousDataContext;
			const bool changeDataContext = options.DataContext
				&& options.DataContext != previousDataContext;
			if (hasBindingChanges)
			{
				for (size_t index = 0; index < output._controls.size(); ++index)
					output._controls[index]->DataBindings = nextBindings[index];
			}
			const bool reboundDataContext =
				hasBindingChanges || hasSchemaChanges || changeDataContext
				|| (windowPresentationChanged && !nextWindow.Bindings.empty());
			auto bindConfigured = [&](const std::shared_ptr<IBindingSource>& source,
				std::wstring* error) -> bool
			{
				if (source) return output.BindDataContext(source, error);
				output.RemoveDataBindings(output._installedBindings);
				std::vector<RuntimeDocument::InstalledBinding> installed;
				if (!output.InstallDataBindings(
					{}, installed, error, output._appliedWindow,
					&output._window)) return false;
				output._installedBindings = std::move(installed);
				return true;
			};
			auto rollbackBindings = [&]() noexcept
			{
				try
				{
					output._window = previousWindow;
					if (hasSchemaChanges)
						output._dataContextSchema = previousDataContextSchema;
					if (hasBindingChanges)
					{
						for (size_t index = 0; index < output._controls.size(); ++index)
							output._controls[index]->DataBindings = previousBindings[index];
					}
					if (reboundDataContext)
						(void)bindConfigured(previousDataContext, nullptr);
				}
				catch (...)
				{
					// Best-effort rollback; callers still receive the original failure.
				}
			};
			if (reboundDataContext
				&& !bindConfigured(nextDataContext, outError))
			{
				const auto reloadError = outError ? *outError : std::wstring{};
				rollbackWindowPresentation();
				rollbackBindings();
				rollbackProperties();
				SetError(outError, reloadError);
				return false;
			}

			std::shared_ptr<ControlStyleSheet> nextRuntimeStyleSheet;
			std::shared_ptr<const ControlStyleSheet> previousContentStyleSheet;
			std::shared_ptr<const ControlStyleSheet> previousWindowStyleSheet;
			bool styleApplied = false;
			auto rollbackStyles = [&]() noexcept
			{
				if (!styleApplied) return;
				if (output._appliedWindow)
					(void)cui::framework::StyleAccess::SetDocumentStyles(
						*output._appliedWindow,
						previousWindowStyleSheet, false);
				if (auto* contentRoot = output._contentRoot.Get())
					(void)cui::framework::StyleAccess::SetDocumentStyles(
						*contentRoot,
						previousContentStyleSheet, true);
				styleApplied = false;
			};
			if (hasStyleChanges)
			{
				if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
					document.StyleSheet, nextRuntimeStyleSheet, outError,
					document.ResourceBasePath, document.Resources,
					BuildStructuralStyleResourcesFor(
						&document, inheritedOptions)))
				{
					rollbackWindowPresentation();
					rollbackBindings();
					rollbackProperties();
					return false;
				}
				if (output._appliedWindow)
					previousWindowStyleSheet =
						cui::framework::StyleAccess::DocumentStyles(
							*output._appliedWindow);
				if (auto* contentRoot = output._contentRoot.Get())
					previousContentStyleSheet =
						cui::framework::StyleAccess::DocumentStyles(
							*contentRoot);
				styleApplied = true;
				if (output._appliedWindow)
				{
					if (!cui::framework::StyleAccess::SetDocumentStyles(
						*output._appliedWindow,
						nextRuntimeStyleSheet, false))
					{
						const auto reloadError =
							L"文档样式表无法原位应用到 XAML Window。";
						rollbackStyles();
						rollbackWindowPresentation();
						rollbackBindings();
						rollbackProperties();
						SetError(outError, reloadError);
						return false;
					}
				}
				if (auto* contentRoot = output._contentRoot.Get())
				{
					if (!cui::framework::StyleAccess::SetDocumentStyles(
						*contentRoot,
						nextRuntimeStyleSheet, true))
					{
						const auto reloadError = L"文档样式表无法原位应用到完整控件树。";
						rollbackStyles();
						rollbackWindowPresentation();
						rollbackBindings();
						rollbackProperties();
						SetError(outError, reloadError);
						return false;
					}
				}
			}
			for (size_t index = 0; index < output._controls.size(); ++index)
				output._controls[index]->EventHandlers = std::move(nextHandlers[index]);

			const auto resolver = options.ControlEventResolver
				? options.ControlEventResolver : output._controlEventResolver;
			bool eventBindingSucceeded = true;
			if (resolver)
				eventBindingSucceeded = output.BindControlEvents(resolver, outError);
			else if (options.RequireControlEventResolver && hasNextEvents)
			{
				SetError(outError,
					L"增量重载后的文档包含控件事件，但没有事件名称解析器。");
				eventBindingSucceeded = false;
			}
			else
				output.ClearControlEvents();

			if (!eventBindingSucceeded)
			{
				const auto reloadError = outError ? *outError : std::wstring{};
				for (size_t index = 0; index < output._controls.size(); ++index)
					output._controls[index]->EventHandlers =
						std::move(previousHandlers[index]);
				rollbackWindowPresentation();
				rollbackStyles();
				rollbackBindings();
				rollbackProperties();
				SetError(outError, reloadError);
				return false;
			}

			output._window = std::move(nextWindow);
			output._styleSheet = document.StyleSheet;
			output._nativeSurfaceBehaviors = inheritedOptions.NativeSurfaceBehaviors;
			output._declarativeComponentBehaviors =
				inheritedOptions.DeclarativeComponentBehaviors;
			output._allowNativeSurfacePlaceholder =
				inheritedOptions.AllowNativeSurfacePlaceholder;
			output._sourceDocument = std::move(nextSourceDocument);
			if (outMode) *outMode = RuntimeDocumentReloadMode::InPlace;
			if (outError) outError->clear();
			return true;
		}

		RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
		if (!effectiveOptions.DataContext)
			effectiveOptions.DataContext = output._dataContext;
		if (!effectiveOptions.ControlEventResolver)
			effectiveOptions.ControlEventResolver = output._controlEventResolver;

		if (output._contentReleased)
		{
			if (output._contentHost)
				return ReloadHosted(
					document, output, effectiveOptions, outMode, outError,
					outDiagnostic);
			SetError(outError,
				L"Content 所有权已经转移；拓扑、结构内容、字体或不支持的属性变化"
				L"不能自动替换宿主树。通用属性、Binding、样式、事件和窗体显示"
				L"属性仍可原位重载。");
			return false;
		}

		bool recomposed = false;
		size_t reusedControlCount = 0;
		auto commitWindowAttachments = [&output](
			RuntimeDocument& candidate,
			std::wstring* commitError)
		{
			return candidate.CommitInheritedWindowAttachments(
				output, {}, commitError);
		};
		if (!RuntimeDocumentTopologyReloader::TryReload(
			document,
			output,
			effectiveOptions,
			recomposed,
			reusedControlCount,
			outError,
			commitWindowAttachments)) return false;
		if (recomposed)
		{
			(void)reusedControlCount;
			if (outMode) *outMode = RuntimeDocumentReloadMode::Recomposed;
			return true;
		}
		RuntimeDocument replacement;
		if (!Load(document, replacement, effectiveOptions, outError,
			outDiagnostic)) return false;
		if (!replacement.CommitInheritedWindowAttachments(
			output, {}, outError)) return false;
		output = std::move(replacement);
		if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态文档重载失败：构建候选状态时抛出异常。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态文档重载失败：发生未知异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::ReloadXaml(
	const std::string& xaml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	DesignDocument document;
	if (!XamlDocumentParser::FromXaml(
		xaml, document, options.ParseOptions,
		outError, outDiagnostic)) return false;
	return Reload(document, output, options, outMode, outError, outDiagnostic);
}

bool RuntimeDocumentLoader::ReloadXamlFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	DesignDocument document;
	if (!XamlDocumentParser::LoadFromFile(
		filePath, document, options.ParseOptions,
		outError, outDiagnostic)) return false;
	return Reload(document, output, options, outMode, outError, outDiagnostic);
}
}

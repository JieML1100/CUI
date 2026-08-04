#include <CuiRuntime.h>
#include <EventInfrastructure.h>
#include <StyleInfrastructure.h>
#include <Convert.h>

#include <Button.h>
#include <Canvas.h>
#include <ComboBox.h>
#include <Binding.h>
#include <BindingList.h>
#include <Window.h>
#include <ItemsControl.h>
#include <InputInfrastructure.h>
#include <ContentPresenter.h>
#include <TemplateInfrastructure.h>
#include <Label.h>
#include <Layout/StackPanel.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>

namespace
{
	void SetNodeProperty(
		DesignerModel::DesignNode& node,
		std::wstring name,
		DesignerStyleValueKind kind,
		std::wstring text)
	{
		node.Properties.Set(std::move(name),
			{ { kind, std::move(text) } });
	}

	std::wstring NodePropertyText(
		const DesignerModel::DesignNode& node,
		const std::wstring& name,
		std::wstring fallback = {})
	{
		const auto* assignment = node.Properties.Find(name);
		return assignment ? assignment->Value.Text : std::move(fallback);
	}

	int Fail(const wchar_t* stage, const std::wstring& error = {})
	{
		std::cerr << "CuiRuntime sample failed at "
			<< Convert::UnicodeToUtf8(stage ? std::wstring(stage) : std::wstring{});
		if (!error.empty()) std::cerr << ": " << Convert::UnicodeToUtf8(error);
		std::cerr << '\n';
		return 1;
	}

	struct TemporaryFile
	{
		std::wstring Path;
		~TemporaryFile()
		{
			if (!Path.empty()) (void)DeleteFileW(Path.c_str());
		}
	};

	struct ControlCounterHandler
	{
		int* Counter = nullptr;
		void Handle(Control*, RoutedEventArgs&) const
		{
			if (Counter) ++*Counter;
		}
	};

	struct WindowContentRenderedCounterHandler
	{
		int* Counter = nullptr;
		void Handle(Window*) const
		{
			if (Counter) ++*Counter;
		}
	};

	struct RuntimeSceneState
	{
		int Attached = 0;
		int Detached = 0;
		int PointerDown = 0;
	};

	class RuntimeSceneBehavior final : public INativeSurfaceBehavior
	{
	public:
		explicit RuntimeSceneBehavior(std::shared_ptr<RuntimeSceneState> state)
			: _state(std::move(state)) {}
		void Attach(NativeSurface&) override { ++_state->Attached; }
		void Detach(NativeSurface&) noexcept override { ++_state->Detached; }
		void Render(NativeSurface&, NativeSurfaceRenderContext&) override {}
		bool HandleInput(NativeSurface&, NativeSurfaceInputEvent& event) override
		{
			if (event.Kind != NativeSurfaceInputKind::PointerDown) return false;
			++_state->PointerDown;
			return true;
		}

	private:
		std::shared_ptr<RuntimeSceneState> _state;
	};

	struct SessionPollThreadContext
	{
		DesignerModel::RuntimeDocumentSession* Session = nullptr;
		DesignerModel::RuntimeDocumentWatchResult* Result = nullptr;
	};

	DWORD WINAPI PollSessionOnWrongThread(void* rawContext)
	{
		auto* context = static_cast<SessionPollThreadContext*>(rawContext);
		if (context && context->Session && context->Result)
			*context->Result = context->Session->Poll();
		return 0;
	}

	class RejectOnceWindowContentHost final
		: public DesignerModel::RuntimeDocumentContentHost
	{
	public:
		explicit RejectOnceWindowContentHost(Window& form) : _form(&form) {}

		bool RejectNextInitial = false;
		bool RejectNextReplacement = false;

		bool DetachContent(
			Control* content,
			std::unique_ptr<Control>& output,
			std::wstring* outError) override
		{
			if (_transactionOpen || output)
			{
				if (outError) *outError = L"invalid rejecting-host detach";
				return false;
			}
			if (_form->GetVisualContent() != content)
			{
				if (outError) *outError = L"rejecting host lost its Content";
				return false;
			}
			if (content)
			{
				output = _form->DetachVisualContent();
				if (!output)
				{
					if (outError) *outError = L"rejecting host detach failed";
					return false;
				}
			}
			_transactionOpen = true;
			return true;
		}

		bool AttachContent(
			std::unique_ptr<Control>& content,
			DesignerModel::RuntimeContentHostAttachMode mode,
			std::wstring* outError) override
		{
			const bool transaction = mode
				!= DesignerModel::RuntimeContentHostAttachMode::Initial;
			if (transaction != _transactionOpen)
			{
				if (outError) *outError = L"invalid rejecting-host attach";
				return false;
			}
			if (mode == DesignerModel::RuntimeContentHostAttachMode::Replacement
				&& RejectNextReplacement)
			{
				RejectNextReplacement = false;
				if (outError) *outError = L"intentional host commit rejection";
				return false;
			}
			if (mode == DesignerModel::RuntimeContentHostAttachMode::Initial
				&& RejectNextInitial)
			{
				RejectNextInitial = false;
				if (outError) *outError = L"intentional initial attach rejection";
				return false;
			}
			if (_form->GetVisualContent()
				|| (content && !_form->TrySetVisualContent(content)))
			{
				if (outError) *outError = L"rejecting-host attach failed";
				return false;
			}
			if (transaction) _transactionOpen = false;
			return true;
		}

	private:
		Window* _form = nullptr;
		bool _transactionOpen = false;
	};

	void AppendStackPanelProbeChild(
		DesignerModel::DesignDocument& document,
		int parentId,
		std::wstring name,
		std::string text)
	{
		const auto parent = std::find_if(
			document.Nodes.begin(), document.Nodes.end(),
			[&](const auto& node) { return node.Id == parentId; });
		if (parent == document.Nodes.end()) return;
		int order = 0;
		for (const auto& node : document.Nodes)
			if (node.ParentId == parentId)
				order = (std::max)(order, node.Order + 1);
		DesignerModel::DesignNode child;
		child.Id = document.AllocateNodeId();
		child.ParentId = parentId;
		child.ParentRef = parent->Name;
		child.Name = std::move(name);
		child.Type = UIClass::UI_Label;
		child.Order = order;
		SetNodeProperty(child, L"Text", DesignerStyleValueKind::String,
			Convert::Utf8ToUnicode(std::move(text)));
		document.Nodes.push_back(std::move(child));
	}
}

int wmain()
{
	const std::string xaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="RuntimeSampleWindow"
      Title="CUI dynamic XAML sample"
	  Width="480" Height="320"
      ContentRendered="HandleContentRendered">
  <Window.Resources>
    <Color x:Key="Accent">#FF0078D4</Color>
    <Style x:Key="PrimaryButton" TargetType="Button">
      <Setter Property="Background" Value="{StaticResource Accent}" />
    </Style>
	<DataType x:Key="Person">
	  <DataType.Properties>
		<Property Path="Name" Kind="String" />
		<Property Path="Role" Kind="String" />
	  </DataType.Properties>
	</DataType>
	<DataTemplate x:Key="PersonRow" DataType="Person">
	  <StackPanel Orientation="Horizontal">
		<TextBlock Text="{Binding Name}" Width="120" />
		<TextBlock Text="{Binding Role}" Width="120" />
	  </StackPanel>
	</DataTemplate>
  </Window.Resources>
	<Window.DataContextSchema>
	  <Property Path="Caption" Kind="String" />
	  <Property Path="People" Kind="Object" ObjectType="BindingList"
			ItemType="Person" CanWrite="false" />
	</Window.DataContextSchema>
  <StackPanel x:Name="rootPanel" DesignId="10"
              Width="Auto" Height="Auto"
              Orientation="Vertical">
    <Button x:Name="actionButton" DesignId="11"
            Style="{StaticResource PrimaryButton}"
			Width="180" Height="36"
			Content="{Binding Caption, Mode=OneWay}"
			Click="HandleAction" />
	<ItemsControl x:Name="peopleList" DesignId="12"
			Width="300" Height="120"
			ItemsSource="{Binding People}"
			ItemTemplate="{StaticResource PersonRow}" />
  </StackPanel>
</Window>)xaml";

	DesignerModel::DesignDocument source;
	std::wstring error;
	if (!DesignerModel::XamlDocumentParser::FromXaml(xaml, source, &error))
		return Fail(L"FromXaml", error);
	const auto canonicalXaml =
		DesignerModel::XamlDocumentSerializer::ToXaml(source);
	DesignerModel::DesignDocument xamlRoundTripped;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		canonicalXaml, xamlRoundTripped, &error)
		|| !(xamlRoundTripped == source))
		return Fail(L"canonical XAML round-trip", error);

	const std::string dataResourceXaml = R"xaml(
<Window xmlns="urn:cui" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <Window.Resources>
    <DataType x:Key="Person">
      <DataType.Properties>
        <Property Path="Id" Kind="Int" />
        <Property Path="Name" Kind="String" />
      </DataType.Properties>
    </DataType>
    <DataList x:Key="People" ItemType="Person">
      <DataRecord Id="1" Name="Declarative Alice" />
      <DataRecord Id="2" Name="Declarative Bob" />
    </DataList>
	<CollectionViewSource x:Key="RankedPeople" Source="{StaticResource People}">
	  <CollectionViewSource.FilterDescriptions>
		<FilterDescription PropertyName="Id" Operator="GreaterThan" Value="0" />
	  </CollectionViewSource.FilterDescriptions>
	  <CollectionViewSource.SortDescriptions>
		<SortDescription PropertyName="Id" Direction="Descending" />
	  </CollectionViewSource.SortDescriptions>
	</CollectionViewSource>
  </Window.Resources>
  <ComboBox x:Name="peopleSeed" ItemsSource="{StaticResource RankedPeople}"
            DisplayMemberPath="Name" SelectedValuePath="Id" />
</Window>)xaml";
	DesignerModel::RuntimeDocument dataResourceRuntime;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		dataResourceXaml, dataResourceRuntime, {}, &error))
		return Fail(L"declarative DataList load", error);
	auto* dataResourceCombo = dynamic_cast<ComboBox*>(
		dataResourceRuntime.FindControlByName(L"peopleSeed"));
	if (!dataResourceCombo || !dataResourceCombo->GetItemsSource()
		|| dataResourceCombo->GetItemsSource().Get()->Count() != 2)
		return Fail(L"declarative DataList materialization");
	dataResourceCombo->SetSelectedValue(BindingValue(2));
	if (dataResourceCombo->SelectedIndex != 0
		|| dataResourceCombo->Text != L"Declarative Bob")
		return Fail(L"typed selector SelectedValue projection");

	const std::string nativeSurfaceXaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="NativeSurfaceWindow">
  <NativeSurface x:Name="runtimeScene" DesignId="19"
      BehaviorKey="Scene3D" PlaceholderText="3D scene" Width="320" Height="180" />
</Window>)xaml";
	auto sceneState = std::make_shared<RuntimeSceneState>();
	auto surfaceBehaviors =
		std::make_shared<DesignerModel::NativeSurfaceBehaviorRegistry>();
	if (!surfaceBehaviors->Register(L"Scene3D",
		[sceneState](NativeSurface&)
		{ return std::make_unique<RuntimeSceneBehavior>(sceneState); }, &error))
		return Fail(L"NativeSurface behavior registration", error);
	DesignerModel::RuntimeDocumentLoadOptions surfaceOptions;
	surfaceOptions.NativeSurfaceBehaviors = surfaceBehaviors;
	DesignerModel::RuntimeDocument surfaceRuntime;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		nativeSurfaceXaml, surfaceRuntime, surfaceOptions, &error))
		return Fail(L"NativeSurface load", error);
	auto* runtimeScene = surfaceRuntime.FindControlByDesignId<NativeSurface>(19);
	if (!runtimeScene || !runtimeScene->HasBehavior()
		|| runtimeScene->GetBehaviorKey() != L"Scene3D")
		return Fail(L"NativeSurface identity/behavior");
	InputReport pointerDown;
	pointerDown.Kind = InputReportKind::PointerDown;
	pointerDown.X = 8;
	pointerDown.Y = 9;
	pointerDown.ChangedButton = MouseButton::Left;
	pointerDown.ButtonStates =
		MouseButtonStates::WithPressed(MouseButton::Left);
	pointerDown.ClickCount = 1;
	(void)cui::framework::InputAccess::DispatchInput(*runtimeScene, pointerDown);
	if (sceneState->Attached != 1 || sceneState->PointerDown != 1)
		return Fail(L"NativeSurface lifecycle/input");

	const std::string layoutXaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="LayoutWindow">
  <Grid x:Name="rootGrid" DesignId="20">
    <Grid.RowDefinitions>
      <RowDefinition Height="Auto" />
      <RowDefinition Height="2*" MinHeight="24" />
    </Grid.RowDefinitions>
    <Grid.ColumnDefinitions>
      <ColumnDefinition Width="*" />
      <ColumnDefinition Width="120" />
    </Grid.ColumnDefinitions>
    <TabControl x:Name="tabs" Grid.Row="1">
      <TabItem Header="General">
        <TextBlock x:Name="insideTab">Nested tab content</TextBlock>
      </TabItem>
    </TabControl>
    <Grid x:Name="split" Grid.Row="0">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="*" />
        <ColumnDefinition Width="*" />
      </Grid.ColumnDefinitions>
      <Button x:Name="firstButton" Grid.Column="0" Content="First" />
      <Button x:Name="secondButton" Grid.Column="1" Content="Second" />
    </Grid>
    <CheckBox x:Name="boundCheck"
              Grid.Row="1" Grid.Column="1"
              IsChecked="{Binding Flags.Enabled, Mode=TwoWay}"
              Visibility="{Binding Flags.Visibility}" />
  </Grid>
</Window>)xaml";
	DesignerModel::RuntimeDocument layoutRuntime;
	DesignerModel::DesignDocument layoutSource;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		layoutXaml, layoutSource, &error))
		return Fail(L"layout FromXaml", error);
	const auto canonicalLayoutXaml =
		DesignerModel::XamlDocumentSerializer::ToXaml(layoutSource);
	DesignerModel::DesignDocument layoutRoundTripped;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		canonicalLayoutXaml, layoutRoundTripped, &error)
		|| !(layoutRoundTripped == layoutSource))
		return Fail(L"layout canonical XAML round-trip", error);
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		layoutXaml, layoutRuntime, {}, &error))
		return Fail(L"nested layout XAML", error);
	if (!layoutRuntime.FindControlByName(L"insideTab")
		|| !layoutRuntime.FindControlByName(L"firstButton")
		|| !layoutRuntime.FindControlByName(L"secondButton")
		|| !layoutRuntime.FindControlByName(L"boundCheck"))
		return Fail(L"nested tab/grid materialization");
	auto* layoutControlBeforeReload =
		layoutRuntime.FindControlByName(L"insideTab");
	for (auto& node : layoutSource.Nodes)
	{
		if (node.Name != L"insideTab") continue;
		SetNodeProperty(node, L"Text", DesignerStyleValueKind::String,
			L"Reloaded tab content");
	}
	DesignerModel::RuntimeDocumentReloadMode layoutReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		layoutSource, layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"structural/property reload", error);
	auto* layoutControlAfterReload =
		layoutRuntime.FindControlByName(L"insideTab");
	BindingValue layoutLocalText;
	std::wstring layoutLocalTextValue;
	const bool hasLayoutLocalText = layoutControlAfterReload
		&& layoutControlAfterReload->TryGetPropertyValue(
			L"Text", DependencyPropertyValueSource::Local, layoutLocalText)
			&& layoutLocalText.TryGet(layoutLocalTextValue);
	std::wstring trackedLayoutText = L"<none>";
	for (const auto& control : layoutRuntime.Controls())
	{
		if (!control || control->Name != L"insideTab") continue;
		const auto tracked = control->MetadataProperties.find(L"Text");
		if (tracked != control->MetadataProperties.end())
			trackedLayoutText = tracked->second.Text;
	}
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| !layoutControlAfterReload
		|| layoutControlAfterReload != layoutControlBeforeReload
		|| layoutControlAfterReload->GetDisplayText() != L"Reloaded tab content")
		return Fail(L"metadata property in-place reload mode",
			L"mode=" + std::to_wstring(static_cast<int>(layoutReloadMode))
			+ L", same=" + std::to_wstring(
				layoutControlAfterReload == layoutControlBeforeReload)
			+ L", text=" + (layoutControlAfterReload
				? layoutControlAfterReload->GetDisplayText() : L"<null>")
			+ L", local=" + (hasLayoutLocalText
				? layoutLocalTextValue : L"<none>")
			+ L", tracked=" + trackedLayoutText);
	auto removedMetadataLayoutSource = layoutSource;
	for (auto& node : removedMetadataLayoutSource.Nodes)
	{
		if (node.Name == L"insideTab")
			node.Properties.Remove(L"Text");
	}
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		removedMetadataLayoutSource, layoutRuntime, {},
		&layoutReloadMode, &error))
		return Fail(L"metadata removal in-place reload", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| layoutRuntime.FindControlByName(L"insideTab") != layoutControlAfterReload
		|| !layoutControlAfterReload->GetDisplayText().empty())
		return Fail(L"metadata removal default value");

	auto structuralLayoutSource = removedMetadataLayoutSource;
	auto* rootGridBeforeStructuralReload =
		layoutRuntime.FindControlByName(L"rootGrid");
	for (auto& node : structuralLayoutSource.Nodes)
	{
		if (node.Name == L"rootGrid"
			&& node.Structure.GridRows
			&& node.Structure.GridRows->size() > 1)
			(*node.Structure.GridRows)[1].Minimum = 32.0;
	}
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		structuralLayoutSource, layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"structural recomposition reload", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| !layoutRuntime.FindControlByName(L"insideTab")
		|| layoutRuntime.FindControlByName(L"insideTab") != layoutControlAfterReload
		|| layoutRuntime.FindControlByName(L"rootGrid")
			== rootGridBeforeStructuralReload)
		return Fail(L"structural subtree recomposition boundary");

	auto reorderedLayoutSource = structuralLayoutSource;
	auto* tabsBeforeReorder = layoutRuntime.FindControlByName(L"tabs");
	auto* splitBeforeReorder = layoutRuntime.FindControlByName(L"split");
	auto* rootGridBeforeReorder = layoutRuntime.FindControlByName(L"rootGrid");
	for (auto& node : reorderedLayoutSource.Nodes)
	{
		if (node.Name == L"tabs") node.Order = 1000;
		else if (node.Name == L"split") node.Order = -1000;
	}
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		reorderedLayoutSource, layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"child reorder recomposition", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| layoutRuntime.FindControlByName(L"rootGrid") == rootGridBeforeReorder
		|| layoutRuntime.FindControlByName(L"tabs") != tabsBeforeReorder
		|| layoutRuntime.FindControlByName(L"split") != splitBeforeReorder)
		return Fail(L"child reorder identity preservation");

	auto addedLayoutSource = reorderedLayoutSource;
	int rootGridId = 0;
	for (const auto& node : addedLayoutSource.Nodes)
		if (node.Name == L"rootGrid") rootGridId = node.Id;
	DesignerModel::DesignNode addedLabel;
	addedLabel.Id = addedLayoutSource.AllocateNodeId();
	addedLabel.ParentId = rootGridId;
	addedLabel.Name = L"addedLabel";
	addedLabel.Type = UIClass::UI_Label;
	addedLabel.Order = 500;
	SetNodeProperty(addedLabel, L"Text", DesignerStyleValueKind::String,
		L"Added during reload");
	const auto addedLabelId = addedLabel.Id;
	addedLayoutSource.Nodes.push_back(std::move(addedLabel));
	auto* rootGridBeforeAdd = layoutRuntime.FindControlByName(L"rootGrid");
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		addedLayoutSource, layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"child add recomposition", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| layoutRuntime.FindControlByName(L"rootGrid") == rootGridBeforeAdd
		|| layoutRuntime.FindControlByName(L"tabs") != tabsBeforeReorder
		|| layoutRuntime.FindControlByName(L"split") != splitBeforeReorder
		|| !layoutRuntime.FindControlByName(L"addedLabel"))
		return Fail(L"child add identity preservation");
	auto addedLabelReference =
		layoutRuntime.ReferenceByDesignId<Label>(addedLabelId);
	if (!addedLabelReference
		|| addedLabelReference.Get()
			!= layoutRuntime.FindControlByName(L"addedLabel"))
		return Fail(L"stable typed reference creation");

	auto removedLayoutSource = addedLayoutSource;
	removedLayoutSource.Nodes.erase(
		std::remove_if(
			removedLayoutSource.Nodes.begin(), removedLayoutSource.Nodes.end(),
			[](const DesignerModel::DesignNode& node)
			{ return node.Name == L"addedLabel"; }),
		removedLayoutSource.Nodes.end());
	auto* rootGridBeforeRemove = layoutRuntime.FindControlByName(L"rootGrid");
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		removedLayoutSource, layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"child remove recomposition", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| layoutRuntime.FindControlByName(L"rootGrid") == rootGridBeforeRemove
		|| layoutRuntime.FindControlByName(L"tabs") != tabsBeforeReorder
		|| layoutRuntime.FindControlByName(L"split") != splitBeforeReorder
		|| layoutRuntime.FindControlByName(L"addedLabel")
		|| addedLabelReference)
		return Fail(L"child remove identity preservation");

	// A raw manual transfer intentionally has no host adapter, so topology
	// replacement must still be rejected instead of guessing external ownership.
	Window unmanagedLayoutHost;
	unmanagedLayoutHost.Title = L"unmanaged runtime Content";
	unmanagedLayoutHost.Width = 320.0f;
	unmanagedLayoutHost.Height = 180.0f;
	auto unmanagedLayoutContent = layoutRuntime.ReleaseContentRoot();
	unmanagedLayoutHost.SetVisualContent(std::move(unmanagedLayoutContent));
	auto unmanagedStructuralReload = removedLayoutSource;
	for (auto& node : unmanagedStructuralReload.Nodes)
		if (node.Name == L"rootGrid")
			(*node.Structure.GridRows)[1].Minimum = 40.0;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		unmanagedStructuralReload, layoutRuntime, {},
		&layoutReloadMode, &error))
		return Fail(L"unadapted host topology reload unexpectedly accepted");
	if (layoutRuntime.FindControlByName(L"insideTab")
		!= layoutControlAfterReload)
		return Fail(L"unadapted host rejection changed identity");

	auto viewModel = std::make_shared<ObservableObject>();
	viewModel->SetValue(L"Caption", std::wstring(L"Loaded from DataContext"));
	auto people = std::make_shared<ObservableBindingList>(L"Person");
	auto alice = std::make_shared<ObservableObject>();
	alice->SetValue(L"Name", std::wstring(L"Alice"));
	alice->SetValue(L"Role", std::wstring(L"Admin"));
	people->Items.push_back(BindingSourceReference(alice));
	viewModel->SetValue(L"People", BindingListReference(people));

	int clickCount = 0;
	ControlCounterHandler clickHandler{ &clickCount };
	DesignerModel::RuntimeEventHandlerRegistry eventHandlers;
	if (!eventHandlers.RegisterControl(
		L"HandleAction",
		UIClass::UI_Button,
		L"Click",
		&ButtonBase::Click,
		std::bind_front(&ControlCounterHandler::Handle, &clickHandler),
		&error))
		return Fail(L"register initial named control handler", error);
	if (!eventHandlers.RegisterWindow(
		L"HandlePointerEnter",
		L"MouseEnter",
		&Window::OnMouseEnter,
		[](Control*, MouseEventArgs&) {},
		&error))
		return Fail(L"register inherited Window Event member", error);
	if (eventHandlers.RegisterWindow(
		L"HandleAction",
		L"ContentRendered",
		&Window::ContentRendered,
		[](Window*) {},
		&error)
		|| eventHandlers.HandlerCount() != 2
		|| error.find(L"另一种事件签名") == std::wstring::npos)
		return Fail(L"reject cross-signature handler registration", error);
	if (eventHandlers.RegisterControl(
		L"HandleWrongMember",
		UIClass::UI_Button,
		L"Click",
		&Control::OnMouseMove,
		[](Control*, MouseEventArgs&) {},
		&error)
		|| eventHandlers.HandlerCount() != 2
		|| error.find(L"事件目录不一致") == std::wstring::npos)
		return Fail(L"reject wrong same-signature Event member", error);
	if (!eventHandlers.RegisterWindow(
		L"HandleWindowDrop",
		L"Drop",
		&Window::OnDrop,
		[](Control*, DragEventArgs&) {},
			&error))
		return Fail(L"register typed Window handlers", error);
	DesignerModel::RuntimeDocumentLoadOptions options;
	options.DataContext = viewModel;
	options.RequireControlEventResolver = true;
	options.ControlEventResolver = eventHandlers.ControlResolver();

	// Keep the host alive longer than RuntimeDocument's RAII event connections.
	Window host;
	host.Title = L"runtime host";
	host.Width = 480.0f;
	host.Height = 240.0f;
	DesignerModel::RuntimeDocument runtime;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		xaml, runtime, options, &error)) return Fail(L"LoadXaml", error);

	auto* button = runtime.FindControlByDesignId<Button>(11);
	if (!button || button != runtime.FindControlByName(L"actionButton"))
		return Fail(L"stable control lookup");
	auto buttonReference = runtime.ReferenceByDesignId<Button>(11);
	if (buttonReference.Get() != button)
		return Fail(L"stable typed button reference");
	auto* buttonChrome = button
		? button->FindDeclarativeTemplatePart(L"PART_Chrome")
		: nullptr;
	auto* buttonTemplateRoot = button
		? cui::framework::TemplateAccess::GetTemplateRoot(*button)
		: nullptr;
	if (button->GetDisplayText() != L"Loaded from DataContext"
		|| !cui::framework::StyleAccess::DocumentStyles(*button)
		|| !cui::framework::StyleAccess::Theme(*button)
		|| !buttonChrome
		|| !buttonTemplateRoot
		|| buttonTemplateRoot->GetVisualParent() != button
		|| buttonTemplateRoot->GetTemplatedParent() != button
		|| buttonChrome->GetVisualParent() != buttonTemplateRoot
		|| buttonChrome->GetTemplatedParent() != button
		|| button->GetCurrentVisualState(L"CommonStates") != L"Normal")
		return Fail(L"binding/style materialization");
	auto* peopleControl = runtime.FindControlByDesignId<ItemsControl>(12);
	auto* firstPersonPresenter = peopleControl
		&& peopleControl->GeneratedItemCount() == 1
		? dynamic_cast<ContentPresenter*>(peopleControl->GetGeneratedItem(0))
		: nullptr;
	auto* firstPersonRow = firstPersonPresenter
		? dynamic_cast<StackPanel*>(firstPersonPresenter->GetGeneratedContent())
		: nullptr;
	auto* firstPersonName = firstPersonRow
		? dynamic_cast<Label*>(firstPersonRow->GetVisualChild(0)) : nullptr;
	if (!firstPersonName || firstPersonName->Text != L"Alice")
		return Fail(L"typed ItemsControl DataTemplate materialization",
			L"generated=" + std::to_wstring(
				peopleControl ? peopleControl->GeneratedItemCount() : 0)
			+ L", row=" + (firstPersonRow ? L"StackPanel" : L"<null>")
			+ L", children=" + std::to_wstring(
				firstPersonRow ? firstPersonRow->VisualChildCount() : 0)
			+ L", name=" + (firstPersonName
				? std::wstring(firstPersonName->Text) : L"<null>"));
	button->Click.Invoke(button, RoutedEventArgs{});
	if (clickCount != 1) return Fail(L"control event resolver");
	if (runtime.DataContextSchema().empty()
		|| runtime.DataContextSchema().front().Path != L"Caption")
		return Fail(L"binding schema synthesis");

	viewModel->SetValue(L"AlternateCaption", std::wstring(L"Reloaded binding source"));
	auto bindingReloadSource = source;
	bindingReloadSource.DataContextSchema = {
		{ L"AlternateCaption", BindingValueKind::String, true, true, true },
		{ L"People", BindingValueKind::Object, true, false, true,
			DesignerDataObjectKind::BindingList, L"Person" }
	};
	for (auto& node : bindingReloadSource.Nodes)
		if (node.Id == 11)
		{
			node.Bindings[L"Content"].SourceProperty = L"AlternateCaption";
			node.Bindings[L"Content"].Mode = BindingMode::OneTime;
		}
	DesignerModel::RuntimeDocumentReloadMode bindingReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		bindingReloadSource, runtime, {}, &bindingReloadMode, &error))
		return Fail(L"in-place binding reload", error);
	const auto* reloadedBinding = button->DataBindings.Find(L"Content");
	if (bindingReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| !reloadedBinding
		|| reloadedBinding->Mode() != BindingMode::OneTime
		|| reloadedBinding->SourceProperty() != L"AlternateCaption"
		|| button->GetDisplayText() != L"Reloaded binding source"
		|| runtime.DataContextSchema().size() != 2
		|| std::none_of(runtime.DataContextSchema().begin(),
			runtime.DataContextSchema().end(), [](const auto& property)
			{ return property.Path == L"AlternateCaption"; })
		|| runtime.BoundDataContext() != viewModel)
		return Fail(L"in-place binding identity or attachment preservation");

	auto styleReloadSource = bindingReloadSource;
	for (auto& resource : styleReloadSource.StyleSheet.Resources)
		if (resource.Key == L"Accent") resource.Value.Text = L"#FFFF0000";
	DesignerModel::RuntimeDocumentReloadMode styleReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		styleReloadSource, runtime, {}, &styleReloadMode, &error))
		return Fail(L"in-place style reload", error);
	if (styleReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| button->Background.Color.r < 0.9f
		|| button->Background.Color.g > 0.1f
		|| button->Background.Color.b > 0.1f)
		return Fail(L"in-place style application",
			L"mode=" + std::to_wstring(static_cast<int>(styleReloadMode))
			+ L", key="
			+ cui::framework::StyleAccess::ResourceKey(*button)
			+ L", resources=" + std::to_wstring(
				styleReloadSource.StyleSheet.Resources.size())
			+ L", rules=" + std::to_wstring(
				styleReloadSource.StyleSheet.Rules.size())
			+ L", rgba=" + std::to_wstring(button->Background.Color.r)
			+ L"," + std::to_wstring(button->Background.Color.g)
			+ L"," + std::to_wstring(button->Background.Color.b)
			+ L"," + std::to_wstring(button->Background.Color.a));

	auto conflictingExpressionSource = styleReloadSource;
	for (auto& node : conflictingExpressionSource.Nodes)
	{
		if (node.Id != 11) continue;
		SetNodeProperty(node, L"Content", DesignerStyleValueKind::String,
			L"conflicting local value");
	}
	DesignerModel::RuntimeDocumentReloadMode conflictingExpressionMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		conflictingExpressionSource, runtime, {},
		&conflictingExpressionMode, &error))
		return Fail(L"duplicate local expression unexpectedly accepted");
	if (error.find(L"多个本地值表达式") == std::wstring::npos
		|| runtime.FindControlByDesignId<Button>(11) != button
		|| buttonReference.Get() != button
		|| button->GetDisplayText() != L"Reloaded binding source")
		return Fail(L"duplicate local expression transactional rejection", error);

	auto eventReloadSource = styleReloadSource;
	for (auto& node : eventReloadSource.Nodes)
		if (node.Id == 11)
			node.Events[L"Click"] = L"HandleReloadedAction";
	SetNodeProperty(eventReloadSource.Window, L"Title",
		DesignerStyleValueKind::String, L"Reloaded in place");
	int reloadedClickCount = 0;
	ControlCounterHandler reloadedClickHandler{ &reloadedClickCount };
	if (!eventHandlers.RegisterControl(
		L"HandleReloadedAction",
		UIClass::UI_Button,
		L"Click",
		&ButtonBase::Click,
		std::bind_front(
			&ControlCounterHandler::Handle, &reloadedClickHandler),
		&error))
		return Fail(L"register reloaded named control handler", error);
	DesignerModel::RuntimeDocumentLoadOptions reloadOptions;
	reloadOptions.RequireControlEventResolver = true;
	reloadOptions.ControlEventResolver = eventHandlers.ControlResolver();
	DesignerModel::RuntimeDocumentReloadMode eventReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		eventReloadSource,
		runtime, reloadOptions, &eventReloadMode, &error))
		return Fail(L"in-place event reload", error);
	if (eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| NodePropertyText(runtime.WindowNode(), L"Title", L"Window")
			!= L"Reloaded in place"
		|| runtime.BoundDataContext() != viewModel)
		return Fail(L"in-place reload identity or attachment preservation");
	button->Click.Invoke(button, RoutedEventArgs{});
	if (clickCount != 1 || reloadedClickCount != 1)
		return Fail(L"in-place event connection replacement");

	auto rejectedEventReload = eventReloadSource;
	for (auto& node : rejectedEventReload.Nodes)
		if (node.Id == 11)
		{
			node.Events[L"Click"] = L"HandleRejectedAction";
			SetNodeProperty(node, L"Visibility", DesignerStyleValueKind::String,
				L"Collapsed");
			node.Bindings[L"Content"].Mode = BindingMode::OneWay;
		}
	for (auto& resource : rejectedEventReload.StyleSheet.Resources)
		if (resource.Key == L"Accent") resource.Value.Text = L"#FF00FF00";
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedEventReload, runtime, reloadOptions, &eventReloadMode, &error))
		return Fail(L"failed in-place resolver unexpectedly accepted");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos)
		return Fail(L"unknown named control handler diagnostic", error);
	if (runtime.FindControlByDesignId(11) != button)
		return Fail(L"failed in-place reload replaced identity");
	if (!button->IsVisible)
		return Fail(L"failed in-place reload did not roll back properties");
	const auto* bindingAfterRollback = button->DataBindings.Find(L"Content");
	if (!bindingAfterRollback
		|| bindingAfterRollback->Mode() != BindingMode::OneTime
		|| button->Background.Color.r < 0.9f
		|| button->Background.Color.g > 0.1f)
		return Fail(L"failed in-place reload did not roll back binding/style state");
	button->Click.Invoke(button, RoutedEventArgs{});
	if (clickCount != 1 || reloadedClickCount != 2)
		return Fail(L"failed in-place reload did not preserve old connection");

	// A failed replacement must not disturb the already active document.
	if (DesignerModel::RuntimeDocumentLoader::LoadXaml(
		"<Window><Button x:Name=\"bad\" Unknown=\"1\" /></Window>",
		runtime, options, &error))
		return Fail(L"invalid XAML unexpectedly accepted");
	if (runtime.FindControlByName(L"actionButton") != button)
		return Fail(L"transactional failed replacement");

	auto topologyReloadSource = eventReloadSource;
	AppendStackPanelProbeChild(
		topologyReloadSource, 10, L"topologyProbe", "Topology probe");
	auto* rootPanelBeforeTopologyReload =
		runtime.FindControlByDesignId(10);
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		topologyReloadSource, runtime, {}, &eventReloadMode, &error))
		return Fail(L"event/binding topology recomposition", error);
	if (eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| runtime.FindControlByDesignId(10) == rootPanelBeforeTopologyReload
		|| runtime.FindControlByDesignId(11) != button
		|| buttonReference.Get() != button
		|| runtime.BoundDataContext() != viewModel)
		return Fail(L"event/binding topology identity preservation");
	button->Click.Invoke(button, RoutedEventArgs{});
	if (reloadedClickCount != 3)
		return Fail(L"event connection after topology recomposition");

	auto rejectedTopologyReload = topologyReloadSource;
	AppendStackPanelProbeChild(
		rejectedTopologyReload, 10,
		L"rejectedTopologyProbe", "Rejected topology probe");
	DesignerModel::RuntimeDocumentLoadOptions rejectedTopologyOptions;
	rejectedTopologyOptions.RequireControlEventResolver = true;
	rejectedTopologyOptions.ControlEventResolver = [](
		const DesignerModel::RuntimeControlEventRequest&,
		EventConnection&,
		std::wstring& resolverError)
	{
		resolverError = L"intentional topology rollback probe";
		return false;
	};
	auto* rootPanelBeforeRejectedTopology =
		runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedTopologyReload, runtime, rejectedTopologyOptions,
		&eventReloadMode, &error))
		return Fail(L"failed topology resolver unexpectedly accepted");
	if (runtime.FindControlByDesignId(10) != rootPanelBeforeRejectedTopology
		|| runtime.FindControlByDesignId(11) != button
		|| runtime.BoundDataContext() != viewModel)
		return Fail(L"failed topology recomposition did not roll back identity");
	button->Click.Invoke(button, RoutedEventArgs{});
	if (reloadedClickCount != 4)
		return Fail(L"failed topology recomposition did not preserve old event");

	int windowContentRenderedCount = 0;
	WindowContentRenderedCounterHandler windowContentRenderedHandler{ &windowContentRenderedCount };
	host.Title = L"pre-attach host state";
	host.Width = 333.0f;
	host.Height = 177.0f;
	(void)host.TrySetPropertyValue(
		L"FontFamily", BindingValue(std::wstring(L"Arial")));
	(void)host.TrySetPropertyValue(L"FontSize", BindingValue(17.0));
	auto hostPrefixOwner = std::make_unique<Button>();
	hostPrefixOwner->SetContent(BindingValue(std::wstring(L"host prefix")));
	Canvas::SetLeft(*hostPrefixOwner, 0.0f);
	Canvas::SetTop(*hostPrefixOwner, 0.0f);
	hostPrefixOwner->Width = 20.0f;
	hostPrefixOwner->Height = 20.0f;
	auto* hostPrefix = host.AddOwned(std::move(hostPrefixOwner));
	auto sharedWindowResolver = eventHandlers.WindowResolver();
	if (runtime.AttachToWindow(host, sharedWindowResolver, &error))
		return Fail(L"missing named Window handler unexpectedly attached");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos
		|| !runtime.OwnsContentRoot()
		|| runtime.HasContentHostAdapter()
		|| runtime.BoundWindowEventCount() != 0
		|| host.Title != L"pre-attach host state"
		|| !host.Width.IsFixed() || host.Width.value != 333.0f
		|| !host.Height.IsFixed() || host.Height.value != 177.0f
		|| host.FontFamily != L"Arial" || host.FontSize != 17.0
		|| host.GetPropertyValueSource(L"FontSize")
			!= DependencyPropertyValueSource::Local
		|| host.GetDataContext()
		|| host.GetVisualContent() != hostPrefix)
		return Fail(L"atomic Window event attach rollback", error);

	if (!eventHandlers.RegisterWindow(
		L"HandleContentRendered",
		L"ContentRendered",
		&Window::ContentRendered,
		std::bind_front(
			&WindowContentRenderedCounterHandler::Handle, &windowContentRenderedHandler),
		&error))
		return Fail(L"register initial named Window handler", error);
	auto rejectingInitialHost =
		std::make_shared<RejectOnceWindowContentHost>(host);
	rejectingInitialHost->RejectNextInitial = true;
	if (runtime.AttachToWindow(
		host, rejectingInitialHost, sharedWindowResolver, &error))
		return Fail(L"rejecting Content host unexpectedly attached");
	if (error.find(L"intentional initial attach rejection")
			== std::wstring::npos
		|| !runtime.OwnsContentRoot()
		|| runtime.HasContentHostAdapter()
		|| runtime.BoundWindowEventCount() != 0
		|| host.Title != L"pre-attach host state"
		|| !host.Width.IsFixed() || host.Width.value != 333.0f
		|| !host.Height.IsFixed() || host.Height.value != 177.0f
		|| host.FontFamily != L"Arial" || host.FontSize != 17.0
		|| host.GetPropertyValueSource(L"FontSize")
			!= DependencyPropertyValueSource::Local
		|| host.GetDataContext()
		|| host.GetVisualContent() != hostPrefix)
		return Fail(L"atomic Content-host attach rollback", error);
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != 0)
		return Fail(L"failed atomic attach leaked Window event connection");
	auto detachedPrefix = host.DetachVisualContent();
	if (detachedPrefix.get() != hostPrefix)
		return Fail(L"Window Content setup detach");
	detachedPrefix.reset();

	if (!runtime.AttachToWindow(host, sharedWindowResolver, &error))
		return Fail(L"atomic Window attachment", error);
	if (host.GetDataContext().Get() != viewModel.get()
		|| !runtime.ContentRoot()
		|| runtime.ContentRoot()->GetDataContext().Get() != viewModel.get())
		return Fail(L"Window DataContext inheritance after atomic attachment");
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != 1) return Fail(L"form event invocation");
	auto* rootAfterAtomicAttach = runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Load(
		source, runtime, options, &error))
		return Fail(L"direct Load replaced an attached RuntimeDocument");
	if (error.find(L"请使用 Reload") == std::wstring::npos
		|| runtime.FindControlByDesignId(10) != rootAfterAtomicAttach
		|| host.GetVisualContent() != rootAfterAtomicAttach)
		return Fail(L"attached RuntimeDocument direct-Load rejection", error);
	const auto shownBeforeRejectedDirectLoad = windowContentRenderedCount;
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != shownBeforeRejectedDirectLoad + 1)
		return Fail(L"direct-Load rejection lost Window event connection");
	(void)host.TrySetPropertyValue(L"FontSize", BindingValue(17.0));
	if (host.FontFamily != L"Arial" || host.FontSize != 17.0)
		return Fail(L"Window typography dependency-property setup");

	if (!runtime.HasContentHostAdapter()
		|| host.GetVisualContent() != runtime.FindControlByDesignId(10))
		return Fail(L"Window Content ownership placement");

	auto transferredReloadSource = topologyReloadSource;
	for (auto& node : transferredReloadSource.Nodes)
		if (node.Id == 11)
			node.Events[L"Click"] = L"HandleAfterTransfer";
	if (!eventHandlers.RegisterControl(
		L"HandleAfterTransfer",
		UIClass::UI_Button,
		L"Click",
		&ButtonBase::Click,
		std::bind_front(
			&ControlCounterHandler::Handle, &reloadedClickHandler),
		&error))
		return Fail(L"register transferred named control handler", error);
	DesignerModel::RuntimeDocumentLoadOptions transferredReloadOptions;
	transferredReloadOptions.RequireControlEventResolver = true;
	transferredReloadOptions.ControlEventResolver =
		eventHandlers.ControlResolver();
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		transferredReloadSource, runtime, transferredReloadOptions,
		&eventReloadMode, &error)
		|| eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button)
		return Fail(L"event reload after ownership transfer", error);

	auto transferredPropertyReload = transferredReloadSource;
	for (auto& node : transferredPropertyReload.Nodes)
		if (node.Id == 11)
			SetNodeProperty(node, L"Visibility", DesignerStyleValueKind::String,
				L"Collapsed");
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		transferredPropertyReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error)
		|| eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| button->IsVisible)
		return Fail(L"property reload after ownership transfer", error);

	auto rejectedWindowAttachmentReload = transferredPropertyReload;
	SetNodeProperty(rejectedWindowAttachmentReload.Window, L"Title",
		DesignerStyleValueKind::String, L"Rejected Window presentation");
	rejectedWindowAttachmentReload.Window.Events[L"ContentRendered"] =
		L"HandleReloadedContentRendered";
	AppendStackPanelProbeChild(
		rejectedWindowAttachmentReload, 10,
		L"formAttachmentProbe", "Window attachment probe");
	auto* rootBeforeRejectedWindowAttachment =
		runtime.FindControlByDesignId(10);
	const auto formTextBeforeRejectedAttachment = host.Title;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedWindowAttachmentReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"rejected Window attachment reload unexpectedly accepted");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos)
		return Fail(L"unknown named Window handler diagnostic", error);
	if (host.Title != formTextBeforeRejectedAttachment
		|| host.FontFamily != L"Arial" || host.FontSize != 17.0
		|| host.GetPropertyValueSource(L"FontSize")
			!= DependencyPropertyValueSource::Local
		|| runtime.FindControlByDesignId(10)
			!= rootBeforeRejectedWindowAttachment
		|| runtime.FindControlByDesignId(11) != button
		|| host.GetVisualContent() != rootBeforeRejectedWindowAttachment)
		return Fail(L"Window presentation/event rollback");
	const auto shownBeforeRejectedAttachment = windowContentRenderedCount;
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != shownBeforeRejectedAttachment + 1)
		return Fail(L"old Window event after attachment rollback");

	if (!eventHandlers.RegisterWindow(
		L"HandleReloadedContentRendered",
		L"ContentRendered",
		&Window::ContentRendered,
		std::bind_front(
			&WindowContentRenderedCounterHandler::Handle, &windowContentRenderedHandler),
		&error))
		return Fail(L"register hot-reloaded named Window handler", error);
	if (!runtime.BindWindowEvents(
		host, eventHandlers.WindowResolver(), &error))
		return Fail(L"install reload-capable Window resolver", error);
	if (eventHandlers.HandlerCount() != 7)
		return Fail(L"named event registry handler count");

	auto rejectedHostedTopology = transferredPropertyReload;
	AppendStackPanelProbeChild(
		rejectedHostedTopology, 10,
		L"rejectedHostedProbe", "Rejected hosted probe");
	DesignerModel::RuntimeDocumentLoadOptions rejectedHostedOptions;
	rejectedHostedOptions.RequireControlEventResolver = true;
	rejectedHostedOptions.ControlEventResolver = [](
		const DesignerModel::RuntimeControlEventRequest&,
		EventConnection&,
		std::wstring& resolverError)
	{
		resolverError = L"intentional adapted-host rollback probe";
		return false;
	};
	auto* rootBeforeHostedRollback = runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedHostedTopology, runtime, rejectedHostedOptions,
		&eventReloadMode, &error))
		return Fail(L"failed adapted-host topology unexpectedly accepted");
	if (runtime.FindControlByDesignId(10) != rootBeforeHostedRollback
		|| runtime.FindControlByDesignId(11) != button
		|| host.GetVisualContent() != rootBeforeHostedRollback)
		return Fail(L"adapted-host topology rollback placement");
	const auto clicksBeforeHostedRollbackProbe = reloadedClickCount;
	button->Click.Invoke(button, RoutedEventArgs{});
	if (reloadedClickCount != clicksBeforeHostedRollbackProbe + 1)
		return Fail(L"adapted-host topology rollback event preservation");

	auto hostedTopologyReload = rejectedWindowAttachmentReload;
	SetNodeProperty(hostedTopologyReload.Window, L"Title",
		DesignerStyleValueKind::String, L"Hosted recomposed Window");
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		hostedTopologyReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error)
		|| eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| runtime.FindControlByDesignId(10) == rootBeforeHostedRollback
		|| runtime.FindControlByDesignId(11) != button
		|| buttonReference.Get() != button
		|| host.Title != L"Hosted recomposed Window"
		|| host.GetVisualContent() != runtime.FindControlByDesignId(10))
		return Fail(L"adapted-host topology recomposition", error);
	const auto shownBeforeHostedRecomposition = windowContentRenderedCount;
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != shownBeforeHostedRecomposition + 1)
		return Fail(L"Window event after hosted recomposition");

	auto hostedReplacementReload = hostedTopologyReload;
	SetNodeProperty(hostedReplacementReload.Window, L"Title",
		DesignerStyleValueKind::String, L"Hosted replaced Window");
	DesignerModel::DesignItemsPanelTemplate replacementItemsPanel;
	replacementItemsPanel.Key = L"RuntimeReplacementItemsPanel";
	replacementItemsPanel.Value.Kind = ItemsPanelKind::Stack;
	replacementItemsPanel.Value.Orientation = Orientation::Vertical;
	hostedReplacementReload.ItemsPanelTemplates.push_back(
		std::move(replacementItemsPanel));
	for (auto& node : hostedReplacementReload.Nodes)
		if (node.Id == 12)
			node.Structure.ItemsPanel = L"RuntimeReplacementItemsPanel";
	auto* buttonBeforeHostedReplacement = button;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		hostedReplacementReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"adapted-host full replacement", error);
	button = runtime.FindControlByDesignId<Button>(11);
	if (eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::Replaced
		|| !button || button == buttonBeforeHostedReplacement
		|| !runtime.HasContentHostAdapter()
		|| buttonReference.Get() != button
		|| button->IsVisible
		|| host.Title != L"Hosted replaced Window"
		|| host.GetVisualContent() != runtime.FindControlByDesignId(10))
		return Fail(L"adapted-host replacement identity or placement",
			L"mode=" + std::to_wstring(static_cast<int>(eventReloadMode))
			+ L", button=" + std::to_wstring(button != nullptr)
			+ L", replaced=" + std::to_wstring(button != buttonBeforeHostedReplacement)
			+ L", adapter=" + std::to_wstring(runtime.HasContentHostAdapter())
			+ L", reference=" + std::to_wstring(buttonReference.Get() == button)
			+ L", visible=" + std::to_wstring(button ? button->IsVisible : true)
			+ L", content=" + std::to_wstring(
				host.GetVisualContent() == runtime.FindControlByDesignId(10))
			+ L", title=" + host.Title);
	const auto shownBeforeHostedReplacement = windowContentRenderedCount;
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != shownBeforeHostedReplacement + 1)
		return Fail(L"Window event after hosted replacement");

	auto invalidHostedCandidate = hostedReplacementReload;
	AppendStackPanelProbeChild(
		invalidHostedCandidate, 10,
		L"invalidHostedProbe", "Invalid hosted probe");
	for (auto& node : invalidHostedCandidate.Nodes)
		if (node.Id == 10)
		{
			SetNodeProperty(node, L"NoSuchRuntimeProperty",
				DesignerStyleValueKind::String, L"invalid");
		}
	auto* rootBeforeInvalidHostedCandidate =
		runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		invalidHostedCandidate, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"invalid adapted-host candidate unexpectedly accepted");
	if (runtime.FindControlByDesignId(10) != rootBeforeInvalidHostedCandidate
		|| runtime.FindControlByDesignId(11) != button
		|| host.GetVisualContent() != rootBeforeInvalidHostedCandidate)
		return Fail(L"invalid adapted-host candidate rollback");

	// File watching starts from a document canonicalized through authored XAML;
	// a Binding intentionally does not serialize its suspended local value.
	DesignerModel::DesignDocument canonicalWatcherBaseline;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		DesignerModel::XamlDocumentSerializer::ToXaml(hostedReplacementReload),
		canonicalWatcherBaseline, &error))
		return Fail(L"watcher baseline canonicalization", error);
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		canonicalWatcherBaseline, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"watcher baseline runtime alignment", error);
	hostedReplacementReload = std::move(canonicalWatcherBaseline);
	button = runtime.FindControlByDesignId<Button>(11);
	if (!button || buttonReference.Get() != button)
		return Fail(L"watcher baseline stable reference alignment");

	wchar_t temporaryDirectory[MAX_PATH]{};
	const DWORD temporaryDirectoryLength = GetTempPathW(
		static_cast<DWORD>(std::size(temporaryDirectory)), temporaryDirectory);
	if (temporaryDirectoryLength == 0
		|| temporaryDirectoryLength >= std::size(temporaryDirectory))
		return Fail(L"watcher temporary path");
	TemporaryFile watchedFile{
		std::wstring(temporaryDirectory)
			+ L"CuiRuntimeWatcher-"
			+ std::to_wstring(GetCurrentProcessId()) + L"-"
			+ std::to_wstring(GetTickCount64()) + L".cui.xaml" };
	if (!DesignerModel::XamlDocumentSerializer::SaveToFile(
		hostedReplacementReload, watchedFile.Path, &error))
		return Fail(L"watcher baseline save", error);

	DesignerModel::RuntimeDocumentFileWatcher watcher(
		std::chrono::milliseconds{ 50 });
	if (!watcher.Start(watchedFile.Path, &error))
		return Fail(L"watcher start", error);
	const auto watchTime = DesignerModel::RuntimeDocumentFileWatcher::Clock::now();
	if (watcher.PollAt(runtime, {}, watchTime).State
		!= DesignerModel::RuntimeDocumentWatchState::Idle)
		return Fail(L"watcher baseline should be idle");

	auto rejectedWatchedDocument = hostedReplacementReload;
	for (auto& node : rejectedWatchedDocument.Nodes)
		if (node.Id == 11)
			node.Events[L"Click"] = L"HandleWatcherRejected";
	if (!DesignerModel::XamlDocumentSerializer::SaveToFile(
		rejectedWatchedDocument, watchedFile.Path, &error))
		return Fail(L"watcher rejected save", error);
	auto watchResult = watcher.PollAt(runtime, {}, watchTime);
	if (watchResult.State != DesignerModel::RuntimeDocumentWatchState::Debouncing
		|| watchResult.ReloadAttempted)
		return Fail(L"watcher change detection");
	watchResult = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 49 });
	if (watchResult.State != DesignerModel::RuntimeDocumentWatchState::Debouncing)
		return Fail(L"watcher debounce window");
	watchResult = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 50 });
	if (watchResult.State != DesignerModel::RuntimeDocumentWatchState::Failed
		|| !watchResult.ReloadAttempted
		|| watchResult.Error.empty()
		|| runtime.FindControlByDesignId(11) != button
		|| button->IsVisible)
		return Fail(L"watcher failed reload rollback", watchResult.Error);
	const auto repeatedFailure = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 60 });
	if (repeatedFailure.State != DesignerModel::RuntimeDocumentWatchState::Failed
		|| repeatedFailure.ReloadAttempted)
		return Fail(L"watcher failed signature suppression");
	const auto clicksBeforeWatcherRollbackProbe = reloadedClickCount;
	button->Click.Invoke(button, RoutedEventArgs{});
	if (reloadedClickCount != clicksBeforeWatcherRollbackProbe + 1)
		return Fail(L"watcher failure did not preserve old event connection");

	auto acceptedWatchedDocument = hostedReplacementReload;
	SetNodeProperty(acceptedWatchedDocument.Window, L"Title",
		DesignerStyleValueKind::String, L"Watcher reloaded in place");
	if (!DesignerModel::XamlDocumentSerializer::SaveToFile(
		acceptedWatchedDocument, watchedFile.Path, &error))
		return Fail(L"watcher accepted save", error);
	watchResult = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 61 });
	if (watchResult.State != DesignerModel::RuntimeDocumentWatchState::Debouncing)
		return Fail(L"watcher recovery detection");
	watchResult = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 111 });
	if (watchResult.State != DesignerModel::RuntimeDocumentWatchState::Reloaded
		|| watchResult.ReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| !watchResult.ReloadAttempted
		|| runtime.FindControlByDesignId(11) != button
		|| NodePropertyText(runtime.WindowNode(), L"Title", L"Window")
			!= L"Watcher reloaded in place"
		|| host.Title != L"Watcher reloaded in place")
		return Fail(L"watcher recovery reload", watchResult.Error);
	const auto shownBeforeWatcherWindowReload = windowContentRenderedCount;
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (windowContentRenderedCount != shownBeforeWatcherWindowReload + 1)
		return Fail(L"Window event after watcher presentation reload");
	if (watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 112 }).State
		!= DesignerModel::RuntimeDocumentWatchState::Idle)
		return Fail(L"watcher duplicate notification suppression");

	TemporaryFile sessionFile{
		std::wstring(temporaryDirectory)
			+ L"CuiRuntimeSession-"
			+ std::to_wstring(GetCurrentProcessId()) + L"-"
			+ std::to_wstring(GetTickCount64()) + L".cui.xaml" };
	if (!DesignerModel::XamlDocumentSerializer::SaveToFile(
		source, sessionFile.Path, &error))
		return Fail(L"session baseline save", error);

	// Keep the Window alive longer than the session's document and connections.
	Window sessionHost;
	sessionHost.Title = L"session host before mount";
	sessionHost.Width = 320.0f;
	sessionHost.Height = 180.0f;
	DesignerModel::RuntimeDocumentSession session(
		std::chrono::milliseconds{ 25 });
	DesignerModel::RuntimeDocumentSessionMountOptions sessionOptions;
	sessionOptions.DataContext = viewModel;
	sessionOptions.WatchFile = false;
	if (session.MountFile(
		sessionFile.Path, sessionHost, sessionOptions, &error))
		return Fail(L"session unexpectedly mounted without handlers");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos
		|| session.IsMounted() || session.IsWatching()
		|| !session.SourceFile().empty()
		|| sessionHost.GetVisualContent()
		|| sessionHost.Title != L"session host before mount")
		return Fail(L"session failed initial mount rollback",
			error + L"; mounted=" + std::to_wstring(session.IsMounted())
			+ L", watching=" + std::to_wstring(session.IsWatching())
			+ L", source=" + session.SourceFile()
			+ L", content=" + std::to_wstring(
				sessionHost.GetVisualContent() != nullptr)
			+ L", title=" + sessionHost.Title);

	int sessionClickCount = 0;
	int sessionReloadedClickCount = 0;
	int sessionContentRenderedCount = 0;
	ControlCounterHandler sessionClickHandler{ &sessionClickCount };
	ControlCounterHandler sessionReloadedClickHandler{
		&sessionReloadedClickCount };
	WindowContentRenderedCounterHandler sessionContentRenderedHandler{ &sessionContentRenderedCount };
	if (!session.EventHandlers().RegisterControl(
		L"HandleAction", UIClass::UI_Button, L"Click",
		&ButtonBase::Click,
		std::bind_front(
			&ControlCounterHandler::Handle, &sessionClickHandler),
			&error)
		|| !session.EventHandlers().RegisterWindow(
			L"HandleContentRendered", L"ContentRendered", &Window::ContentRendered,
			std::bind_front(
				&WindowContentRenderedCounterHandler::Handle, &sessionContentRenderedHandler),
			&error))
		return Fail(L"session handler registration", error);
	if (!session.MountFile(
		sessionFile.Path, sessionHost, sessionOptions, &error))
		return Fail(L"session atomic mount retry", error);
	auto* sessionButton =
		session.Document().FindControlByDesignId<Button>(11);
	if (!session.IsMounted()
		|| session.MountedWindow() != &sessionHost
		|| session.SourceFile() != sessionFile.Path
		|| session.OwningThreadId() != GetCurrentThreadId()
		|| session.IsWatching()
		|| !sessionButton
		|| sessionButton->GetDisplayText() != L"Loaded from DataContext"
		|| sessionHost.GetVisualContent()
			!= session.Document().FindControlByDesignId(10))
		return Fail(L"session mounted state");
	sessionButton->Click.Invoke(sessionButton, RoutedEventArgs{});
	cui::framework::EventAccess::Raise(sessionHost.ContentRendered, &sessionHost);
	if (sessionClickCount != 1 || sessionContentRenderedCount != 1)
		return Fail(L"session initial event routing");

	DesignerModel::RuntimeDocumentWatchResult crossThreadPoll;
	SessionPollThreadContext crossThreadContext{
		&session, &crossThreadPoll };
	const HANDLE wrongThread = CreateThread(
		nullptr, 0, PollSessionOnWrongThread,
		&crossThreadContext, 0, nullptr);
	if (!wrongThread)
		return Fail(L"session UI-thread guard thread creation");
	const auto wrongThreadWait = WaitForSingleObject(wrongThread, INFINITE);
	CloseHandle(wrongThread);
	if (wrongThreadWait != WAIT_OBJECT_0)
		return Fail(L"session UI-thread guard thread wait");
	if (crossThreadPoll.State
			!= DesignerModel::RuntimeDocumentWatchState::Failed
		|| crossThreadPoll.ReloadAttempted
		|| crossThreadPoll.Error.find(L"UI 线程") == std::wstring::npos)
		return Fail(L"session UI-thread guard", crossThreadPoll.Error);
	if (!session.StartWatching(&error) || !session.IsWatching())
		return Fail(L"session delayed watcher start", error);

	auto sessionReload = source;
	SetNodeProperty(sessionReload.Window, L"Title",
		DesignerStyleValueKind::String, L"Session reloaded transactionally");
	for (auto& node : sessionReload.Nodes)
		if (node.Id == 11)
		{
			SetNodeProperty(node, L"Visibility", DesignerStyleValueKind::String,
				L"Collapsed");
			node.Events[L"Click"] = L"HandleSessionReload";
		}
	if (!DesignerModel::XamlDocumentSerializer::SaveToFile(
		sessionReload, sessionFile.Path, &error))
		return Fail(L"session reload save", error);
	const auto sessionWatchTime =
		DesignerModel::RuntimeDocumentFileWatcher::Clock::now();
	auto sessionWatchResult = session.PollAt(sessionWatchTime);
	if (sessionWatchResult.State
		!= DesignerModel::RuntimeDocumentWatchState::Debouncing)
		return Fail(L"session reload detection", sessionWatchResult.Error);
	sessionWatchResult = session.PollAt(
		sessionWatchTime + std::chrono::milliseconds{ 25 });
	if (sessionWatchResult.State
			!= DesignerModel::RuntimeDocumentWatchState::Failed
		|| !sessionWatchResult.ReloadAttempted
		|| session.Document().FindControlByDesignId(11) != sessionButton
		|| !sessionButton->IsVisible
		|| sessionHost.Title != L"CUI dynamic XAML sample")
		return Fail(L"session failed reload rollback", sessionWatchResult.Error);
	sessionButton->Click.Invoke(sessionButton, RoutedEventArgs{});
	if (sessionClickCount != 2 || sessionReloadedClickCount != 0)
		return Fail(L"session rollback preserved event route");

	if (!session.EventHandlers().RegisterControl(
		L"HandleSessionReload", UIClass::UI_Button, L"Click",
		&ButtonBase::Click,
		std::bind_front(
			&ControlCounterHandler::Handle, &sessionReloadedClickHandler),
		&error))
		return Fail(L"session late handler registration", error);
	session.RequestRetryAt(
		sessionWatchTime + std::chrono::milliseconds{ 26 });
	sessionWatchResult = session.PollAt(
		sessionWatchTime + std::chrono::milliseconds{ 51 });
	if (sessionWatchResult.State
			!= DesignerModel::RuntimeDocumentWatchState::Reloaded
		|| sessionWatchResult.ReloadMode
			!= DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| session.Document().FindControlByDesignId(11) != sessionButton
		|| sessionButton->IsVisible
		|| sessionHost.Title != L"Session reloaded transactionally")
		return Fail(L"session retry after late registration",
			sessionWatchResult.Error);
	sessionButton->Click.Invoke(sessionButton, RoutedEventArgs{});
	cui::framework::EventAccess::Raise(sessionHost.ContentRendered, &sessionHost);
	if (sessionClickCount != 2
		|| sessionReloadedClickCount != 1
		|| sessionContentRenderedCount != 2)
		return Fail(L"session reloaded event routing");

	std::wcout << L"CuiRuntime sample passed: canonical XAML round-trip, typed ItemsControl DataTemplate, declarative DataList/CollectionViewSource, "
		L"NativeSurface behavior registration, lookup, binding/schema, style, signature-safe named events, atomic initial Window "
		L"attachment/direct-Load guards, property/event in-place "
		L"reload, compound rollback, "
		L"topology subtree recomposition/rollback, adapted-host replacement/exact-slot "
		L"rollback, Window presentation/event continuation, manual ownership boundaries, "
		L"debounced file watching, and the UI-thread runtime session are active.\n";
	return 0;
}

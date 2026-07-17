#include <CuiRuntime.h>

#include <Button.h>
#include <Binding.h>
#include <Form.h>
#include <Label.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>

namespace
{
	int Fail(const wchar_t* stage, const std::wstring& error = {})
	{
		std::wcerr << L"CuiRuntime sample failed at " << stage;
		if (!error.empty()) std::wcerr << L": " << error;
		std::wcerr << L'\n';
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
		void Handle(Control*, MouseEventArgs) const
		{
			if (Counter) ++*Counter;
		}
	};

	struct FormCommandCounterHandler
	{
		int* Counter = nullptr;
		void Handle(Form*, int, int) const
		{
			if (Counter) ++*Counter;
		}
	};

	class RuntimeStatusBadge final : public Button
	{
	public:
		Event<void(Control*, int)> OnSeverityInvoked;

		RuntimeStatusBadge() : Button(L"", 0, 0, 120, 30) {}
		int Severity() const noexcept { return _severity; }
		void SetSeverity(int value)
		{
			SetPropertyField(L"Severity", _severity, value);
		}
		void EnsureBindingPropertiesRegistered() override
		{
			Button::EnsureBindingPropertiesRegistered();
			static const bool registered = []
			{
				ControlPropertyOptions<RuntimeStatusBadge, int> options;
				options.DefaultValue = 0;
				options.Design.Category = L"Custom";
				options.Design.Persistence =
					ControlPropertyPersistence::Metadata;
				BindingPropertyRegistry::Register<RuntimeStatusBadge, int>(
					L"Severity",
					[](RuntimeStatusBadge& target) { return target.Severity(); },
					[](RuntimeStatusBadge& target, const int& value)
					{ target.SetSeverity(value); }, {}, std::move(options));
				return true;
			}();
			(void)registered;
		}

	private:
		int _severity = 0;
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

	class RejectOnceFormRootHost final
		: public DesignerModel::RuntimeDocumentRootHost
	{
	public:
		explicit RejectOnceFormRootHost(Form& form) : _form(&form) {}

		bool RejectNextInitial = false;
		bool RejectNextReplacement = false;

		bool DetachRoots(
			std::span<Control* const> roots,
			std::vector<std::unique_ptr<Control>>& output,
			std::wstring* outError) override
		{
			if (_transactionOpen || roots.size() != 1 || !output.empty())
			{
				if (outError) *outError = L"invalid rejecting-host detach";
				return false;
			}
			const auto slot = _form->IndexOfControl(roots.front());
			if (slot < 0)
			{
				if (outError) *outError = L"rejecting host lost its root";
				return false;
			}
			output.reserve(1);
			auto owner = _form->DetachControl(roots.front());
			if (!owner)
			{
				if (outError) *outError = L"rejecting host detach failed";
				return false;
			}
			output.push_back(std::move(owner));
			_slot = slot;
			_transactionOpen = true;
			return true;
		}

		bool AttachRoots(
			std::vector<std::unique_ptr<Control>>& roots,
			DesignerModel::RuntimeRootHostAttachMode mode,
			std::wstring* outError) override
		{
			const bool transaction = mode
				!= DesignerModel::RuntimeRootHostAttachMode::Initial;
			if (transaction != _transactionOpen || roots.size() != 1)
			{
				if (outError) *outError = L"invalid rejecting-host attach";
				return false;
			}
			if (mode == DesignerModel::RuntimeRootHostAttachMode::Replacement
				&& RejectNextReplacement)
			{
				RejectNextReplacement = false;
				if (outError) *outError = L"intentional host commit rejection";
				return false;
			}
			if (mode == DesignerModel::RuntimeRootHostAttachMode::Initial
				&& RejectNextInitial)
			{
				RejectNextInitial = false;
				if (outError) *outError = L"intentional initial attach rejection";
				return false;
			}
			const int slot = transaction
				? _slot : static_cast<int>(_form->Controls.size());
			if (!_form->TryInsertOwned(slot, roots.front()))
			{
				if (outError) *outError = L"rejecting-host attach failed";
				return false;
			}
			roots.clear();
			if (transaction) _transactionOpen = false;
			return true;
		}

	private:
		Form* _form = nullptr;
		int _slot = 0;
		bool _transactionOpen = false;
	};
}

int wmain()
{
	const std::string xaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="RuntimeSampleForm"
      Text="CUI dynamic XAML sample"
      Width="480" Height="240"
      Command="HandleCommand">
  <Form.Resources>
    <Color x:Key="Accent">#FF0078D4</Color>
    <Style x:Key="PrimaryButton" TargetType="Button" Class="primary">
      <Setter Property="Round" Value="8" />
      <Setter Property="BackColor" Value="{StaticResource Accent}" />
    </Style>
  </Form.Resources>
  <StackPanel x:Name="rootPanel" DesignId="10"
              Width="Auto" Height="Auto"
              Orientation="Vertical" Spacing="8">
    <Button x:Name="actionButton" DesignId="11"
            Classes="primary" Style="{StaticResource PrimaryButton}"
			Width="180" Height="36"
			Text="{Binding Caption, Mode=OneWay}"
			Click="HandleAction" />
  </StackPanel>
</Form>)xaml";

	DesignerModel::DesignDocument source;
	std::wstring error;
	if (!DesignerModel::XamlDocumentParser::FromXaml(xaml, source, &error))
		return Fail(L"FromXaml", error);
	const auto xml = DesignerModel::DesignDocumentSerializer::ToXml(source);
	DesignerModel::DesignDocument roundTripped;
	if (!DesignerModel::DesignDocumentSerializer::FromXml(
		xml, roundTripped, &error) || !(roundTripped == source))
		return Fail(L"XAML -> DesignDocument XML round-trip", error);
	const auto canonicalXaml =
		DesignerModel::XamlDocumentSerializer::ToXaml(source);
	DesignerModel::DesignDocument xamlRoundTripped;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		canonicalXaml, xamlRoundTripped, &error)
		|| !(xamlRoundTripped == source))
		return Fail(L"canonical XAML round-trip", error);

	const std::string customControlXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer"
      xmlns:sample="urn:cui:samples"
      x:Name="CustomControlForm">
  <sample:StatusBadge x:Name="runtimeBadge" DesignId="19"
      d:CppType="Acme.Controls.StatusBadge"
      d:Header="Controls/StatusBadge.h"
      d:BaseType="Button" d:Constructor="Bounds"
      Text="Registered runtime custom control" Severity="3"
      OnSeverityInvoked="HandleSeverityInvoked">
    <d:CustomEvents>
      <d:Event Name="OnSeverityInvoked" DisplayName="Severity invoked"
          Field="OnSeverityInvoked" Category="Action"
          Signature="SenderInt" Order="5" Default="true" />
    </d:CustomEvents>
  </sample:StatusBadge>
</Form>)xaml";
	auto customControls =
		std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
	if (!customControls->Register(
		L"urn:cui:samples", L"StatusBadge",
		[](const DesignerModel::DesignNode&)
		{ return std::make_unique<RuntimeStatusBadge>(); }, &error))
		return Fail(L"custom control registration", error);
	DesignerModel::RuntimeDocumentLoadOptions customOptions;
	customOptions.CustomControls = customControls;
	DesignerCustomControlType statusBadgeType{
		L"sample", L"StatusBadge", L"urn:cui:samples",
		L"Acme::Controls::StatusBadge", L"Controls/StatusBadge.h",
		DesignerCustomControlConstructor::Bounds };
	DesignerCustomEventDescriptor severityEvent{
		L"OnSeverityInvoked", L"Severity invoked", "OnSeverityInvoked",
		DesignerEventCategory::Action,
		DesignerCustomEventSignature::SenderInt, 5, true };
	int customEventValue = -1;
	DesignerModel::RuntimeEventHandlerRegistry customEventHandlers;
	if (!customEventHandlers.RegisterCustomControl(
		L"HandleSeverityInvoked", statusBadgeType, severityEvent,
		&RuntimeStatusBadge::OnSeverityInvoked,
		[&customEventValue](Control*, int value) { customEventValue = value; },
		&error))
		return Fail(L"custom event registration", error);
	auto wrongSeverityEvent = severityEvent;
	wrongSeverityEvent.Signature = DesignerCustomEventSignature::SenderBool;
	if (customEventHandlers.RegisterCustomControl(
		L"HandleWrongSeverity", statusBadgeType, wrongSeverityEvent,
		&RuntimeStatusBadge::OnSeverityInvoked,
		[](Control*, int) {}, &error)
		|| error.find(L"签名预设不一致") == std::wstring::npos)
		return Fail(L"reject mismatched custom event preset", error);
	customOptions.RequireControlEventResolver = true;
	customOptions.ControlEventResolver = customEventHandlers.ControlResolver();
	DesignerModel::RuntimeDocument customRuntime;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		customControlXaml, customRuntime, customOptions, &error))
		return Fail(L"registered custom control load", error);
	auto* runtimeBadge = dynamic_cast<RuntimeStatusBadge*>(
		customRuntime.FindControlByDesignId(19));
	if (!runtimeBadge
		|| runtimeBadge->Text != L"Registered runtime custom control"
		|| runtimeBadge->Severity() != 3)
		return Fail(L"registered custom control identity/properties");
	runtimeBadge->OnSeverityInvoked.Invoke(runtimeBadge, 7);
	if (customEventValue != 7 || customRuntime.BoundControlEventCount() != 1)
		return Fail(L"registered custom control event binding");
	auto updatedCustomControlXaml = customControlXaml;
	const auto severityPosition = updatedCustomControlXaml.find("Severity=\"3\"");
	if (severityPosition == std::string::npos)
		return Fail(L"custom control reload fixture");
	updatedCustomControlXaml.replace(
		severityPosition, std::string("Severity=\"3\"").size(), "Severity=\"4\"");
	DesignerModel::RuntimeDocumentReloadMode customReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
		updatedCustomControlXaml, customRuntime, {}, &customReloadMode, &error))
		return Fail(L"registered custom control reload", error);
	if (customReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| customRuntime.FindControlByDesignId(19) != runtimeBadge
		|| runtimeBadge->Severity() != 4)
		return Fail(L"registered custom control in-place reload");
	runtimeBadge->OnSeverityInvoked.Invoke(runtimeBadge, 9);
	if (customEventValue != 9 || customRuntime.BoundControlEventCount() != 1)
		return Fail(L"registered custom event reload continuity");

	const std::string layoutXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="LayoutForm">
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
      <TabPage Header="General">
        <Label x:Name="insideTab">Nested tab content</Label>
      </TabPage>
    </TabControl>
    <SplitContainer x:Name="split" Grid.Row="0">
      <SplitContainer.FirstPanel>
        <Button x:Name="firstButton" Text="First" />
      </SplitContainer.FirstPanel>
      <SplitContainer.SecondPanel>
        <Button x:Name="secondButton" Text="Second" />
      </SplitContainer.SecondPanel>
    </SplitContainer>
    <CheckBox x:Name="boundCheck"
              Grid.Row="1" Grid.Column="1"
              Checked="{Binding Flags.Enabled, Mode=TwoWay}"
              Visibility="{Binding Flags.Visible}" />
  </Grid>
</Form>)xaml";
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
		return Fail(L"nested tab/split materialization");
	auto* layoutControlBeforeReload =
		layoutRuntime.FindControlByName(L"insideTab");
	for (auto& node : layoutSource.Nodes)
	{
		if (node.Name != L"insideTab") continue;
		node.Props["metadata"]["Text"]["value"] = "Reloaded tab content";
	}
	DesignerModel::RuntimeDocumentReloadMode layoutReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::ReloadXml(
		DesignerModel::DesignDocumentSerializer::ToXml(layoutSource),
		layoutRuntime, {}, &layoutReloadMode, &error))
		return Fail(L"structural/property reload", error);
	auto* layoutControlAfterReload =
		layoutRuntime.FindControlByName(L"insideTab");
	BindingValue layoutLocalText;
	std::wstring layoutLocalTextValue;
	const bool hasLayoutLocalText = layoutControlAfterReload
		&& layoutControlAfterReload->TryGetPropertyValue(
			L"Text", ControlPropertyValueSource::Local, layoutLocalText)
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
		|| layoutControlAfterReload->Text != L"Reloaded tab content")
		return Fail(L"metadata property in-place reload mode",
			L"mode=" + std::to_wstring(static_cast<int>(layoutReloadMode))
			+ L", same=" + std::to_wstring(
				layoutControlAfterReload == layoutControlBeforeReload)
			+ L", text=" + (layoutControlAfterReload
				? std::wstring(layoutControlAfterReload->Text) : L"<null>")
			+ L", local=" + (hasLayoutLocalText
				? layoutLocalTextValue : L"<none>")
			+ L", tracked=" + trackedLayoutText);
	auto removedMetadataLayoutSource = layoutSource;
	for (auto& node : removedMetadataLayoutSource.Nodes)
	{
		if (node.Name == L"insideTab"
			&& node.Props["metadata"].is_object())
			node.Props["metadata"].ObjectItems().erase("Text");
	}
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		removedMetadataLayoutSource, layoutRuntime, {},
		&layoutReloadMode, &error))
		return Fail(L"metadata removal in-place reload", error);
	if (layoutReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| layoutRuntime.FindControlByName(L"insideTab") != layoutControlAfterReload
		|| !layoutControlAfterReload->Text.empty())
		return Fail(L"metadata removal default value");

	auto structuralLayoutSource = removedMetadataLayoutSource;
	auto* rootGridBeforeStructuralReload =
		layoutRuntime.FindControlByName(L"rootGrid");
	for (auto& node : structuralLayoutSource.Nodes)
	{
		if (node.Name == L"rootGrid"
			&& node.Extra["rows"].is_array()
			&& node.Extra["rows"].size() > 1)
			node.Extra["rows"][1]["min"] = 32.0;
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
	addedLabel.Props["text"] = "Added during reload";
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

	// The legacy manual transfer intentionally has no host adapter, so topology
	// replacement must still be rejected instead of guessing external ownership.
	Form unmanagedLayoutHost(
		L"unmanaged runtime roots", POINT{ 0, 0 }, SIZE{ 320, 180 });
	auto unmanagedLayoutRoots = layoutRuntime.ReleaseRootControls();
	for (auto& root : unmanagedLayoutRoots)
		unmanagedLayoutHost.AddOwned(std::move(root));
	auto unmanagedStructuralReload = removedLayoutSource;
	for (auto& node : unmanagedStructuralReload.Nodes)
		if (node.Name == L"rootGrid")
			node.Extra["unmanagedTopologyProbe"] = 1;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		unmanagedStructuralReload, layoutRuntime, {},
		&layoutReloadMode, &error))
		return Fail(L"unadapted host topology reload unexpectedly accepted");
	if (layoutRuntime.FindControlByName(L"insideTab")
		!= layoutControlAfterReload)
		return Fail(L"unadapted host rejection changed identity");

	const std::string multiRootXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="MultiRootForm">
  <Button x:Name="multiFirst" DesignId="301" Text="First root" />
  <Button x:Name="multiSecond" DesignId="302"
          Text="Second root" Click="HandleMultiRoot" />
</Form>)xaml";
	DesignerModel::RuntimeDocument multiRootRuntime;
	DesignerModel::DesignDocument multiRootSource;
	if (!DesignerModel::XamlDocumentParser::FromXaml(
		multiRootXaml, multiRootSource, &error))
		return Fail(L"multi-root source parse", error);
	DesignerModel::RuntimeDocumentLoadOptions multiRootOptions;
	multiRootOptions.RequireControlEventResolver = true;
	multiRootOptions.ControlEventResolver = [](
		const DesignerModel::RuntimeControlEventRequest& request,
		EventConnection& connection,
		std::wstring& resolverError)
	{
		if (request.HandlerName != L"HandleMultiRoot")
		{
			resolverError = L"unexpected multi-root handler";
			return false;
		}
		connection = request.Target.OnMouseClick.Subscribe(
			[](Control*, MouseEventArgs) {});
		return true;
	};
	Form multiRootHost(
		L"multi-root host", POINT{ 0, 0 }, SIZE{ 320, 180 });
	auto* multiPrefix =
		multiRootHost.Add<Button>(L"prefix", 0, 0, 20, 20);
	const std::string rejectedOneCallXaml = R"xaml(
<Form xmlns="urn:cui" x:Name="RejectedAttachForm"
	  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
	  Text="must roll back" Command="HandleMissingCommand">
	  <Button x:Name="rejectedRoot" DesignId="399" />
</Form>)xaml";
	if (DesignerModel::RuntimeDocumentLoader::LoadXamlIntoForm(
		rejectedOneCallXaml,
		multiRootHost,
		multiRootRuntime,
		multiRootOptions,
		DesignerModel::RuntimeFormEventResolver{},
		&error))
		return Fail(L"one-call Form resolver failure unexpectedly accepted");
	if (error.find(L"未提供名称解析器") == std::wstring::npos
		|| !multiRootRuntime.Controls().empty()
		|| multiRootHost.Text != L"multi-root host"
		|| multiRootHost.Controls.size() != 1
		|| multiRootHost.Controls[0] != multiPrefix)
		return Fail(L"one-call XAML attach rollback", error);
	if (!DesignerModel::RuntimeDocumentLoader::LoadXamlIntoForm(
		multiRootXaml,
		multiRootHost,
		multiRootRuntime,
		multiRootOptions,
		DesignerModel::RuntimeFormEventResolver{},
		&error))
		return Fail(L"one-call XAML Form load", error);
	auto missingFormResolverReload = multiRootSource;
	missingFormResolverReload.Form.EventHandlers[L"OnCommand"] =
		L"HandleMissingCommand";
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		missingFormResolverReload, multiRootRuntime, multiRootOptions,
		&layoutReloadMode, &error))
		return Fail(L"missing future Form resolver unexpectedly accepted");
	if (error.find(L"没有保留名称解析器") == std::wstring::npos
		|| multiRootHost.Controls.size() != 3
		|| multiRootHost.Controls[0] != multiPrefix
		|| multiRootHost.Controls[1]
			!= multiRootRuntime.FindControlByDesignId(301)
		|| multiRootHost.Controls[2]
			!= multiRootRuntime.FindControlByDesignId(302))
		return Fail(L"missing future Form resolver rollback", error);
	auto* multiSuffix =
		multiRootHost.Add<Button>(L"suffix", 0, 0, 20, 20);
	auto* multiFirst = multiRootRuntime.FindControlByDesignId(301);
	auto* multiSecond = multiRootRuntime.FindControlByDesignId(302);
	auto multiSecondOwner = multiRootHost.DetachControl(multiSecond);
	std::unique_ptr<Control> multiMiddleOwner =
		std::make_unique<Button>(L"middle", 0, 0, 20, 20);
	auto* multiMiddle = multiMiddleOwner.get();
	if (!multiRootHost.TryInsertOwned(2, multiMiddleOwner)
		|| !multiRootHost.TryInsertOwned(3, multiSecondOwner))
		return Fail(L"multi-root host interleave setup");
	auto rejectedMultiRoot = multiRootSource;
	for (auto& node : rejectedMultiRoot.Nodes)
		if (node.Id == 301) node.Extra["topologyProbe"] = 1;
	DesignerModel::RuntimeDocumentLoadOptions rejectMultiRootOptions;
	rejectMultiRootOptions.RequireControlEventResolver = true;
	rejectMultiRootOptions.ControlEventResolver = [](
		const DesignerModel::RuntimeControlEventRequest&,
		EventConnection&,
		std::wstring& resolverError)
	{
		resolverError = L"intentional multi-root rollback";
		return false;
	};
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedMultiRoot, multiRootRuntime, rejectMultiRootOptions,
		&layoutReloadMode, &error))
		return Fail(L"multi-root rollback probe unexpectedly accepted");
	if (multiRootHost.Controls.size() != 5
		|| multiRootHost.Controls[0] != multiPrefix
		|| multiRootHost.Controls[1] != multiFirst
		|| multiRootHost.Controls[2] != multiMiddle
		|| multiRootHost.Controls[3] != multiSecond
		|| multiRootHost.Controls[4] != multiSuffix
		|| multiRootRuntime.FindControlByDesignId(301) != multiFirst
		|| multiRootRuntime.FindControlByDesignId(302) != multiSecond)
		return Fail(L"multi-root exact-slot rollback");

	DesignerModel::RuntimeDocument rejectedCommitRuntime;
	if (!DesignerModel::RuntimeDocumentLoader::Load(
		removedLayoutSource, rejectedCommitRuntime, {}, &error))
		return Fail(L"host commit-rejection baseline load", error);
	Form rejectedCommitHost(
		L"commit rejection host", POINT{ 0, 0 }, SIZE{ 320, 180 });
	auto* rejectedCommitPrefix =
		rejectedCommitHost.Add<Button>(L"prefix", 0, 0, 20, 20);
	auto rejectedCommitAdapter =
		std::make_shared<RejectOnceFormRootHost>(rejectedCommitHost);
	if (!rejectedCommitRuntime.TransferRootControlsTo(
		rejectedCommitAdapter, &error))
		return Fail(L"host commit-rejection transfer", error);
	auto* rejectedCommitSuffix =
		rejectedCommitHost.Add<Button>(L"suffix", 0, 0, 20, 20);
	auto* rootBeforeRejectedCommit =
		rejectedCommitRuntime.FindControlByName(L"rootGrid");
	auto* childBeforeRejectedCommit =
		rejectedCommitRuntime.FindControlByName(L"insideTab");
	auto rejectedCommitSource = removedLayoutSource;
	for (auto& node : rejectedCommitSource.Nodes)
		if (node.Name == L"rootGrid")
			node.Extra["rejectedHostCommitProbe"] = 1;
	rejectedCommitAdapter->RejectNextReplacement = true;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedCommitSource, rejectedCommitRuntime, {},
		&layoutReloadMode, &error))
		return Fail(L"host commit rejection unexpectedly accepted");
	if (rejectedCommitRuntime.FindControlByName(L"rootGrid")
			!= rootBeforeRejectedCommit
		|| rejectedCommitRuntime.FindControlByName(L"insideTab")
			!= childBeforeRejectedCommit
		|| rejectedCommitHost.Controls.size() != 3
		|| rejectedCommitHost.Controls[0] != rejectedCommitPrefix
		|| rejectedCommitHost.Controls[1] != rootBeforeRejectedCommit
		|| rejectedCommitHost.Controls[2] != rejectedCommitSuffix)
		return Fail(L"host commit rejection rollback");

	auto viewModel = std::make_shared<ObservableObject>();
	viewModel->SetValue(L"Caption", std::wstring(L"Loaded from DataContext"));

	int clickCount = 0;
	ControlCounterHandler clickHandler{ &clickCount };
	DesignerModel::RuntimeEventHandlerRegistry eventHandlers;
	if (!eventHandlers.RegisterControl(
		L"HandleAction",
		UIClass::UI_Base,
		L"OnMouseClick",
		&Control::OnMouseClick,
		std::bind_front(&ControlCounterHandler::Handle, &clickHandler),
		&error))
		return Fail(L"register initial named control handler", error);
	if (!eventHandlers.RegisterForm(
		L"HandleAction",
		L"OnMouseEnter",
		&Form::OnMouseEnter,
		[](Control*, MouseEventArgs) {},
		&error))
		return Fail(L"register inherited Form Event member", error);
	if (eventHandlers.RegisterForm(
		L"HandleAction",
		L"OnCommand",
		&Form::OnCommand,
		[](Form*, int, int) {},
		&error)
		|| eventHandlers.HandlerCount() != 1
		|| error.find(L"另一种事件签名") == std::wstring::npos)
		return Fail(L"reject cross-signature handler registration", error);
	if (eventHandlers.RegisterControl(
		L"HandleWrongMember",
		UIClass::UI_Base,
		L"OnMouseClick",
		&Control::OnMouseMove,
		[](Control*, MouseEventArgs) {},
		&error)
		|| eventHandlers.HandlerCount() != 1
		|| error.find(L"事件目录不一致") == std::wstring::npos)
		return Fail(L"reject wrong same-signature Event member", error);
	int validationChangeCount = 0;
	if (!eventHandlers.RegisterControl(
		L"HandleValidationChange",
		UIClass::UI_Base,
		L"OnValidationStateChanged",
		&Control::OnValidationStateChanged,
		[&validationChangeCount](const BindingValidationChangedEventArgs&)
		{
			++validationChangeCount;
		},
		&error))
		return Fail(L"register custom validation Event wrapper", error);
	Button validationProbe(L"validation probe", 0, 0, 20, 20);
	const auto validationDescriptor = DesignerEventCatalog::FindControlEvent(
		UIClass::UI_Button, L"OnValidationStateChanged");
	EventConnection validationConnection;
	if (!validationDescriptor)
		return Fail(L"validation Event descriptor lookup");
	DesignerModel::RuntimeControlEventRequest validationRequest{
		validationProbe,
		0,
		L"validationProbe",
		UIClass::UI_Button,
		{},
		*validationDescriptor,
		L"HandleValidationChange"
	};
	if (!eventHandlers.ControlResolver()(
		validationRequest, validationConnection, error))
		return Fail(L"resolve custom validation Event wrapper", error);
	validationProbe.OnValidationStateChanged.Notify(L"Text");
	if (validationChangeCount != 1 || !validationConnection.Connected())
		return Fail(L"custom validation Event wrapper invocation");
	if (!eventHandlers.RegisterForm(
		L"HandleFormStringPair",
		L"OnTextChanged",
		&Form::OnTextChanged,
		[](Form*, std::wstring, std::wstring) {},
		&error)
		|| !eventHandlers.RegisterForm(
			L"HandleFormStringPair",
			L"OnThemeChanged",
			&Form::OnThemeChanged,
			[](Form*, std::wstring, std::wstring) {},
			&error))
		return Fail(L"reuse handler across same typed Form signature", error);
	DesignerModel::RuntimeDocumentLoadOptions options;
	options.DataContext = viewModel;
	options.RequireControlEventResolver = true;
	options.ControlEventResolver = eventHandlers.ControlResolver();

	// Keep the host alive longer than RuntimeDocument's RAII event connections.
	Form host(L"runtime host", POINT{ 0, 0 }, SIZE{ 480, 240 });
	DesignerModel::RuntimeDocument runtime;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
		xaml, runtime, options, &error)) return Fail(L"LoadXaml", error);

	auto* button = runtime.FindControlByDesignId<Button>(11);
	if (!button || button != runtime.FindControlByName(L"actionButton"))
		return Fail(L"stable control lookup");
	auto buttonReference = runtime.ReferenceByDesignId<Button>(11);
	if (buttonReference.Get() != button)
		return Fail(L"stable typed button reference");
	if (button->Text != L"Loaded from DataContext" || !button->GetStyleSheet())
		return Fail(L"binding/style materialization");
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (clickCount != 1) return Fail(L"control event resolver");
	if (runtime.DataContextSchema().empty()
		|| runtime.DataContextSchema().front().Path != L"Caption")
		return Fail(L"binding schema synthesis");

	viewModel->SetValue(L"AlternateCaption", std::wstring(L"Reloaded binding source"));
	auto bindingReloadSource = source;
	bindingReloadSource.DataContextSchema = {
		{ L"AlternateCaption", BindingValueKind::String, true, true, true }
	};
	for (auto& node : bindingReloadSource.Nodes)
		if (node.Id == 11)
		{
			node.Bindings["Text"]["source"] = "AlternateCaption";
			node.Bindings["Text"]["mode"] = static_cast<int>(BindingMode::OneTime);
		}
	DesignerModel::RuntimeDocumentReloadMode bindingReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		bindingReloadSource, runtime, {}, &bindingReloadMode, &error))
		return Fail(L"in-place binding reload", error);
	const auto* reloadedBinding = button->DataBindings.Find(L"Text");
	if (bindingReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| !reloadedBinding
		|| reloadedBinding->Mode() != BindingMode::OneTime
		|| reloadedBinding->SourceProperty() != L"AlternateCaption"
		|| button->Text != L"Reloaded binding source"
		|| runtime.DataContextSchema().size() != 1
		|| runtime.DataContextSchema().front().Path != L"AlternateCaption"
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
		|| button->BackColor.r < 0.9f
		|| button->BackColor.g > 0.1f
		|| button->BackColor.b > 0.1f)
		return Fail(L"in-place style application");

	auto boundPropertyReloadSource = styleReloadSource;
	for (auto& node : boundPropertyReloadSource.Nodes)
	{
		if (node.Id != 11) continue;
		node.Props["metadata"]["Text"] = {
			{ "kind", "String" }, { "value", "suspended local value" }
		};
	}
	auto* buttonBeforeBoundPropertyReload = button;
	DesignerModel::RuntimeDocumentReloadMode boundPropertyReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		boundPropertyReloadSource, runtime, {},
		&boundPropertyReloadMode, &error))
		return Fail(L"bound property conservative replacement", error);
	button = runtime.FindControlByDesignId<Button>(11);
	if (boundPropertyReloadMode != DesignerModel::RuntimeDocumentReloadMode::Replaced
		|| !button || button == buttonBeforeBoundPropertyReload
		|| buttonReference.Get() != button
		|| button->Text != L"Reloaded binding source")
		return Fail(L"bound property replacement boundary");

	auto eventReloadSource = boundPropertyReloadSource;
	for (auto& node : eventReloadSource.Nodes)
		if (node.Id == 11)
			node.Events["OnMouseClick"] = "HandleReloadedAction";
	eventReloadSource.Form.Text = L"Reloaded in place";
	int reloadedClickCount = 0;
	ControlCounterHandler reloadedClickHandler{ &reloadedClickCount };
	if (!eventHandlers.RegisterControl(
		L"HandleReloadedAction",
		UIClass::UI_Base,
		L"OnMouseClick",
		&Control::OnMouseClick,
		std::bind_front(
			&ControlCounterHandler::Handle, &reloadedClickHandler),
		&error))
		return Fail(L"register reloaded named control handler", error);
	DesignerModel::RuntimeDocumentLoadOptions reloadOptions;
	reloadOptions.RequireControlEventResolver = true;
	reloadOptions.ControlEventResolver = eventHandlers.ControlResolver();
	DesignerModel::RuntimeDocumentReloadMode eventReloadMode =
		DesignerModel::RuntimeDocumentReloadMode::Unchanged;
	if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
		DesignerModel::XamlDocumentSerializer::ToXaml(eventReloadSource),
		runtime, reloadOptions, &eventReloadMode, &error))
		return Fail(L"in-place event reload", error);
	if (eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| runtime.FormModel().Text != L"Reloaded in place"
		|| runtime.BoundDataContext() != viewModel)
		return Fail(L"in-place reload identity or attachment preservation");
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (clickCount != 1 || reloadedClickCount != 1)
		return Fail(L"in-place event connection replacement");

	auto rejectedEventReload = eventReloadSource;
	for (auto& node : rejectedEventReload.Nodes)
		if (node.Id == 11)
		{
			node.Events["OnMouseClick"] = "HandleRejectedAction";
			node.Props["visible"] = false;
			node.Bindings["Text"]["mode"] = static_cast<int>(BindingMode::OneWay);
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
	if (!button->Visible)
		return Fail(L"failed in-place reload did not roll back properties");
	const auto* bindingAfterRollback = button->DataBindings.Find(L"Text");
	if (!bindingAfterRollback
		|| bindingAfterRollback->Mode() != BindingMode::OneTime
		|| button->BackColor.r < 0.9f
		|| button->BackColor.g > 0.1f)
		return Fail(L"failed in-place reload did not roll back binding/style state");
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (clickCount != 1 || reloadedClickCount != 2)
		return Fail(L"failed in-place reload did not preserve old connection");

	// A failed replacement must not disturb the already active document.
	if (DesignerModel::RuntimeDocumentLoader::LoadXaml(
		"<Form><Button x:Name=\"bad\" Unknown=\"1\" /></Form>",
		runtime, options, &error))
		return Fail(L"invalid XAML unexpectedly accepted");
	if (runtime.FindControlByName(L"actionButton") != button)
		return Fail(L"transactional failed replacement");

	auto topologyReloadSource = eventReloadSource;
	for (auto& node : topologyReloadSource.Nodes)
		if (node.Id == 10) node.Extra["runtimeTopologyProbe"] = 1;
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
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (reloadedClickCount != 3)
		return Fail(L"event connection after topology recomposition");

	auto rejectedTopologyReload = topologyReloadSource;
	for (auto& node : rejectedTopologyReload.Nodes)
		if (node.Id == 10) node.Extra["runtimeTopologyProbe"] = 2;
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
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (reloadedClickCount != 4)
		return Fail(L"failed topology recomposition did not preserve old event");

	int commandCount = 0;
	FormCommandCounterHandler commandHandler{ &commandCount };
	Font borrowedFormFont(L"Arial", 17.0f);
	host.Text = L"pre-attach host state";
	host.Size = SIZE{ 333, 177 };
	host.SetFontEx(&borrowedFormFont, false);
	auto* hostPrefix = host.Add<Button>(L"host prefix", 0, 0, 20, 20);
	auto sharedFormResolver = eventHandlers.FormResolver();
	if (runtime.AttachToForm(host, sharedFormResolver, &error))
		return Fail(L"missing named Form handler unexpectedly attached");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos
		|| !runtime.OwnsRootControls()
		|| runtime.HasRootHostAdapter()
		|| runtime.BoundFormEventCount() != 0
		|| host.Text != L"pre-attach host state"
		|| host.Size.cx != 333 || host.Size.cy != 177
		|| host.GetConfiguredFont() != &borrowedFormFont
		|| host.OwnsConfiguredFont()
		|| host.Controls.size() != 1 || host.Controls[0] != hostPrefix)
		return Fail(L"atomic Form event attach rollback", error);

	if (!eventHandlers.RegisterForm(
		L"HandleCommand",
		L"OnCommand",
		&Form::OnCommand,
		std::bind_front(
			&FormCommandCounterHandler::Handle, &commandHandler),
		&error))
		return Fail(L"register initial named Form handler", error);
	auto rejectingInitialHost =
		std::make_shared<RejectOnceFormRootHost>(host);
	rejectingInitialHost->RejectNextInitial = true;
	if (runtime.AttachToForm(
		host, rejectingInitialHost, sharedFormResolver, &error))
		return Fail(L"rejecting root host unexpectedly attached");
	if (error.find(L"intentional initial attach rejection")
			== std::wstring::npos
		|| !runtime.OwnsRootControls()
		|| runtime.HasRootHostAdapter()
		|| runtime.BoundFormEventCount() != 0
		|| host.Text != L"pre-attach host state"
		|| host.Size.cx != 333 || host.Size.cy != 177
		|| host.GetConfiguredFont() != &borrowedFormFont
		|| host.OwnsConfiguredFont()
		|| host.Controls.size() != 1 || host.Controls[0] != hostPrefix)
		return Fail(L"atomic root-host attach rollback", error);
	host.OnCommand.Invoke(&host, 0, 0);
	if (commandCount != 0)
		return Fail(L"failed atomic attach leaked Form event connection");

	if (!runtime.AttachToForm(host, sharedFormResolver, &error))
		return Fail(L"atomic Form attachment", error);
	host.OnCommand.Invoke(&host, 1, 0);
	if (commandCount != 1) return Fail(L"form event invocation");
	auto* rootAfterAtomicAttach = runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Load(
		source, runtime, options, &error))
		return Fail(L"direct Load replaced an attached RuntimeDocument");
	if (error.find(L"请使用 Reload") == std::wstring::npos
		|| runtime.FindControlByDesignId(10) != rootAfterAtomicAttach
		|| host.Controls.size() != 2
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != rootAfterAtomicAttach)
		return Fail(L"attached RuntimeDocument direct-Load rejection", error);
	const auto commandsBeforeRejectedDirectLoad = commandCount;
	host.OnCommand.Invoke(&host, 1, 0);
	if (commandCount != commandsBeforeRejectedDirectLoad + 1)
		return Fail(L"direct-Load rejection lost Form event connection");
	host.SetFontEx(&borrowedFormFont, false);
	if (host.GetConfiguredFont() != &borrowedFormFont
		|| host.OwnsConfiguredFont())
		return Fail(L"borrowed Form font setup");

	auto* hostSuffix = host.Add<Button>(L"host suffix", 0, 0, 20, 20);
	if (!runtime.HasRootHostAdapter()
		|| host.Controls.size() != 3
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != runtime.FindControlByDesignId(10)
		|| host.Controls[2] != hostSuffix)
		return Fail(L"adapted root ownership placement");

	auto transferredReloadSource = topologyReloadSource;
	for (auto& node : transferredReloadSource.Nodes)
		if (node.Id == 11)
			node.Events["OnMouseClick"] = "HandleAfterTransfer";
	if (!eventHandlers.RegisterControl(
		L"HandleAfterTransfer",
		UIClass::UI_Base,
		L"OnMouseClick",
		&Control::OnMouseClick,
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
			node.Props["visible"] = false;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		transferredPropertyReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error)
		|| eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| runtime.FindControlByDesignId(11) != button
		|| button->Visible)
		return Fail(L"property reload after ownership transfer", error);

	auto rejectedFormAttachmentReload = transferredPropertyReload;
	rejectedFormAttachmentReload.Form.Text = L"Rejected Form presentation";
	rejectedFormAttachmentReload.Form.EventHandlers[L"OnCommand"] =
		L"HandleReloadedCommand";
	for (auto& node : rejectedFormAttachmentReload.Nodes)
		if (node.Id == 10) node.Extra["formAttachmentProbe"] = 1;
	auto* rootBeforeRejectedFormAttachment =
		runtime.FindControlByDesignId(10);
	const auto formTextBeforeRejectedAttachment = host.Text;
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		rejectedFormAttachmentReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"rejected Form attachment reload unexpectedly accepted");
	if (error.find(L"未注册运行时处理函数") == std::wstring::npos)
		return Fail(L"unknown named Form handler diagnostic", error);
	if (host.Text != formTextBeforeRejectedAttachment
		|| host.GetConfiguredFont() != &borrowedFormFont
		|| host.OwnsConfiguredFont()
		|| runtime.FindControlByDesignId(10)
			!= rootBeforeRejectedFormAttachment
		|| runtime.FindControlByDesignId(11) != button
		|| host.Controls[1] != rootBeforeRejectedFormAttachment)
		return Fail(L"Form presentation/event rollback");
	const auto commandsBeforeRejectedAttachment = commandCount;
	host.OnCommand.Invoke(&host, 2, 0);
	if (commandCount != commandsBeforeRejectedAttachment + 1)
		return Fail(L"old Form event after attachment rollback");

	if (!eventHandlers.RegisterForm(
		L"HandleReloadedCommand",
		L"OnCommand",
		&Form::OnCommand,
		std::bind_front(
			&FormCommandCounterHandler::Handle, &commandHandler),
		&error))
		return Fail(L"register hot-reloaded named Form handler", error);
	if (!runtime.BindFormEvents(
		host, eventHandlers.FormResolver(), &error))
		return Fail(L"install reload-capable Form resolver", error);
	if (eventHandlers.HandlerCount() != 7)
		return Fail(L"named event registry handler count");

	auto rejectedHostedTopology = transferredPropertyReload;
	for (auto& node : rejectedHostedTopology.Nodes)
		if (node.Id == 10) node.Extra["hostedTopologyProbe"] = 1;
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
		|| host.Controls.size() != 3
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != rootBeforeHostedRollback
		|| host.Controls[2] != hostSuffix)
		return Fail(L"adapted-host topology rollback placement");
	const auto clicksBeforeHostedRollbackProbe = reloadedClickCount;
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (reloadedClickCount != clicksBeforeHostedRollbackProbe + 1)
		return Fail(L"adapted-host topology rollback event preservation");

	auto hostedTopologyReload = rejectedFormAttachmentReload;
	hostedTopologyReload.Form.Text = L"Hosted recomposed Form";
	for (auto& node : hostedTopologyReload.Nodes)
		if (node.Id == 10) node.Extra["hostedTopologyProbe"] = 2;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		hostedTopologyReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error)
		|| eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::Recomposed
		|| runtime.FindControlByDesignId(10) == rootBeforeHostedRollback
		|| runtime.FindControlByDesignId(11) != button
		|| buttonReference.Get() != button
		|| host.Text != L"Hosted recomposed Form"
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != runtime.FindControlByDesignId(10)
		|| host.Controls[2] != hostSuffix)
		return Fail(L"adapted-host topology recomposition", error);
	const auto commandsBeforeHostedRecomposition = commandCount;
	host.OnCommand.Invoke(&host, 3, 0);
	if (commandCount != commandsBeforeHostedRecomposition + 1)
		return Fail(L"Form event after hosted recomposition");

	auto hostedReplacementReload = hostedTopologyReload;
	hostedReplacementReload.Form.Text = L"Hosted replaced Form";
	for (auto& node : hostedReplacementReload.Nodes)
		if (node.Id == 11)
			node.Extra["runtimeStructureProbe"] = 1;
	auto* buttonBeforeHostedReplacement = button;
	if (!DesignerModel::RuntimeDocumentLoader::Reload(
		hostedReplacementReload, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"adapted-host full replacement", error);
	button = runtime.FindControlByDesignId<Button>(11);
	if (eventReloadMode != DesignerModel::RuntimeDocumentReloadMode::Replaced
		|| !button || button == buttonBeforeHostedReplacement
		|| !runtime.HasRootHostAdapter()
		|| buttonReference.Get() != button
		|| button->Visible
		|| host.Text != L"Hosted replaced Form"
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != runtime.FindControlByDesignId(10)
		|| host.Controls[2] != hostSuffix)
		return Fail(L"adapted-host replacement identity or placement");
	const auto commandsBeforeHostedReplacement = commandCount;
	host.OnCommand.Invoke(&host, 4, 0);
	if (commandCount != commandsBeforeHostedReplacement + 1)
		return Fail(L"Form event after hosted replacement");

	auto invalidHostedCandidate = hostedReplacementReload;
	for (auto& node : invalidHostedCandidate.Nodes)
		if (node.Id == 10)
		{
			node.Extra["invalidHostedTopologyProbe"] = 1;
			node.Props["metadata"]["NoSuchRuntimeProperty"] = {
				{ "kind", "String" }, { "value", "invalid" }
			};
		}
	auto* rootBeforeInvalidHostedCandidate =
		runtime.FindControlByDesignId(10);
	if (DesignerModel::RuntimeDocumentLoader::Reload(
		invalidHostedCandidate, runtime, transferredReloadOptions,
		&eventReloadMode, &error))
		return Fail(L"invalid adapted-host candidate unexpectedly accepted");
	if (runtime.FindControlByDesignId(10) != rootBeforeInvalidHostedCandidate
		|| runtime.FindControlByDesignId(11) != button
		|| host.Controls[0] != hostPrefix
		|| host.Controls[1] != rootBeforeInvalidHostedCandidate
		|| host.Controls[2] != hostSuffix)
		return Fail(L"invalid adapted-host candidate rollback");

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
			node.Events["OnMouseClick"] = "HandleWatcherRejected";
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
		|| button->Visible)
		return Fail(L"watcher failed reload rollback", watchResult.Error);
	const auto repeatedFailure = watcher.PollAt(
		runtime, {}, watchTime + std::chrono::milliseconds{ 60 });
	if (repeatedFailure.State != DesignerModel::RuntimeDocumentWatchState::Failed
		|| repeatedFailure.ReloadAttempted)
		return Fail(L"watcher failed signature suppression");
	const auto clicksBeforeWatcherRollbackProbe = reloadedClickCount;
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	if (reloadedClickCount != clicksBeforeWatcherRollbackProbe + 1)
		return Fail(L"watcher failure did not preserve old event connection");

	auto acceptedWatchedDocument = hostedReplacementReload;
	acceptedWatchedDocument.Form.Text = L"Watcher reloaded in place";
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
		|| runtime.FormModel().Text != L"Watcher reloaded in place"
		|| host.Text != L"Watcher reloaded in place")
		return Fail(L"watcher recovery reload", watchResult.Error);
	const auto commandsBeforeWatcherFormReload = commandCount;
	host.OnCommand.Invoke(&host, 5, 0);
	if (commandCount != commandsBeforeWatcherFormReload + 1)
		return Fail(L"Form event after watcher presentation reload");
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

	// Keep the Form alive longer than the session's document and connections.
	Form sessionHost(
		L"session host before mount", POINT{ 0, 0 }, SIZE{ 320, 180 });
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
		|| !sessionHost.Controls.empty()
		|| sessionHost.Text != L"session host before mount")
		return Fail(L"session failed initial mount rollback", error);

	int sessionClickCount = 0;
	int sessionReloadedClickCount = 0;
	int sessionCommandCount = 0;
	ControlCounterHandler sessionClickHandler{ &sessionClickCount };
	ControlCounterHandler sessionReloadedClickHandler{
		&sessionReloadedClickCount };
	FormCommandCounterHandler sessionCommandHandler{ &sessionCommandCount };
	if (!session.EventHandlers().RegisterControl(
		L"HandleAction", UIClass::UI_Base, L"OnMouseClick",
		&Control::OnMouseClick,
		std::bind_front(
			&ControlCounterHandler::Handle, &sessionClickHandler),
		&error)
		|| !session.EventHandlers().RegisterForm(
			L"HandleCommand", L"OnCommand", &Form::OnCommand,
			std::bind_front(
				&FormCommandCounterHandler::Handle, &sessionCommandHandler),
			&error))
		return Fail(L"session handler registration", error);
	if (!session.MountFile(
		sessionFile.Path, sessionHost, sessionOptions, &error))
		return Fail(L"session atomic mount retry", error);
	auto* sessionButton =
		session.Document().FindControlByDesignId<Button>(11);
	if (!session.IsMounted()
		|| session.MountedForm() != &sessionHost
		|| session.SourceFile() != sessionFile.Path
		|| session.OwningThreadId() != GetCurrentThreadId()
		|| session.IsWatching()
		|| !sessionButton
		|| sessionButton->Text != L"Loaded from DataContext"
		|| sessionHost.Controls.size() != 1
		|| sessionHost.Controls[0]
			!= session.Document().FindControlByDesignId(10))
		return Fail(L"session mounted state");
	sessionButton->OnMouseClick.Invoke(sessionButton, MouseEventArgs{});
	sessionHost.OnCommand.Invoke(&sessionHost, 1, 0);
	if (sessionClickCount != 1 || sessionCommandCount != 1)
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
	sessionReload.Form.Text = L"Session reloaded transactionally";
	for (auto& node : sessionReload.Nodes)
		if (node.Id == 11)
		{
			node.Props["visible"] = false;
			node.Events["OnMouseClick"] = "HandleSessionReload";
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
		|| !sessionButton->Visible
		|| sessionHost.Text != L"CUI dynamic XAML sample")
		return Fail(L"session failed reload rollback", sessionWatchResult.Error);
	sessionButton->OnMouseClick.Invoke(sessionButton, MouseEventArgs{});
	if (sessionClickCount != 2 || sessionReloadedClickCount != 0)
		return Fail(L"session rollback preserved event route");

	if (!session.EventHandlers().RegisterControl(
		L"HandleSessionReload", UIClass::UI_Base, L"OnMouseClick",
		&Control::OnMouseClick,
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
		|| sessionButton->Visible
		|| sessionHost.Text != L"Session reloaded transactionally")
		return Fail(L"session retry after late registration",
			sessionWatchResult.Error);
	sessionButton->OnMouseClick.Invoke(sessionButton, MouseEventArgs{});
	sessionHost.OnCommand.Invoke(&sessionHost, 2, 0);
	if (sessionClickCount != 2
		|| sessionReloadedClickCount != 1
		|| sessionCommandCount != 2)
		return Fail(L"session reloaded event routing");

	std::wcout << L"CuiRuntime sample passed: canonical XAML/XML round-trip, registered custom controls, lookup, "
		L"binding/schema, style, signature-safe named events, atomic initial Form "
		L"attachment/direct-Load guards, property/event in-place "
		L"reload, compound rollback, "
		L"topology subtree recomposition/rollback, adapted-host replacement/exact-slot "
		L"rollback, Form presentation/event continuation, manual ownership boundaries, "
		L"debounced file watching, and the UI-thread runtime session are active.\n";
	return 0;
}

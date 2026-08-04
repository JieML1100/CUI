[CmdletBinding()]
param(
    [Alias('ProductionLibrary')]
    [string]$ProductionCuiLib,

    [Alias('DesignLibrary')]
    [string]$DesignCuiRuntimeLib,

    [Alias('DesignCoreLibrary')]
    [string]$DesignCuiCoreLib,

    [string]$ClosureMap,

    [string]$FullMap,

    [string]$DumpbinPath,

    [switch]$SourceOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$registryNames = @(
    'BindingValueConverterRegistry'
    'MultiBindingValueConverterRegistry'
)

$dependencyPropertyStorageNames = @(
    'DependencyPropertyMetadataCache'
    'RegisteredDependencyProperties'
    'RegisteredBindingProperties'
    'RegisteredBindingPropertyTokenLayers'
    'BindingPropertyMutex'
    'NextDependencyPropertyGlobalIndex'
)

$dependencyPropertyRegistryMethodNames = @(
    'CreateStandalone'
    'Register'
    'RegisterReadOnly'
    'AddOwner'
    'OverrideMetadata'
    'ResolveMetadata'
    'Find'
    'FindProperty'
    'FindNative'
    'FindNativeCore'
    'GetProperties'
    'FindRegistered'
    'GetRegisteredProperties'
)

$dependencyPropertyBoundaryName = 'DependencyPropertyDynamicRegistry'
$dependencyObjectBoundaryName = 'DependencyObjectNameCompatibility'
$controlDesignBoundaryName = 'ControlDesignCompatibility'
$dependencyPropertySymbolPatterns = @(
    '\?\?[01]DependencyPropertyMetadataCache@@'
    '\?(?:AddLayer|FindResolved|Layers|LayersEqual|PublishLayers|Resolve)@DependencyPropertyMetadataCache@@'
    '\?RegisteredDependencyProperties@@'
    '\?RegisteredBindingProperties@@'
    '\?RegisteredBindingPropertyTokenLayers@@'
    '\?BindingPropertyMutex@@'
    '\?NextDependencyPropertyGlobalIndex@@'
    '\?Register@DependencyPropertyRegistry@@'
    '\?RegisterReadOnly@DependencyPropertyRegistry@@'
    '\?AddOwner@DependencyPropertyRegistry@@'
    '\?OverrideMetadata@DependencyPropertyRegistry@@'
    '\?FindProperty@DependencyPropertyRegistry@@'
    '\?FindNative@DependencyPropertyRegistry@@'
    '\?FindNativeCore@DependencyPropertyRegistry@@'
    '\?FindRegistered@DependencyPropertyRegistry@@'
    '\?GetRegisteredProperties@DependencyPropertyRegistry@@'
    '\?GetProperties@DependencyPropertyRegistry@@'
    '\?ResolveMetadata@DependencyPropertyRegistry@@'
    '\?CreateStandalone@DependencyPropertyRegistry@@'
    '\?Find@DependencyPropertyRegistry@@'
    'IsPropertyNameLess'
    'IsRegistryPropertyNameLess'
)
$dependencyObjectNameMethodNames = @(
    'FindDependencyProperty'
    'FindPropertyMetadata'
    'TryGetPropertyValue'
    'TrySetPropertyValue'
    'TrySetPropertyValueOwned'
    'CanAcquireBindingPropertyValue'
    'TryAttachBindingPropertyExpression'
    'TrySetBindingPropertyValue'
    'TrySetCurrentPropertyValue'
    'TrySetReadOnlyPropertyValue'
    'ClearReadOnlyPropertyValue'
    'CoerceValue'
    'ClearPropertyValue'
    'ClearPropertyValueOwned'
    'ClearBindingPropertyValue'
    'IsBindingExpressionOwner'
    'HasPropertyValue'
    'GetPropertyValueSource'
    'GetPropertyExpressionKind'
    'ResetPropertyValue'
    'IsPropertyValueDefault'
    'TrySetPropertyBaseValue'
    'TryGetValue'
    'TrySetValue'
    'TryGetPropertyMetadata'
)
$dependencyObjectMethodSymbolPatterns = [ordered]@{}
foreach ($methodName in $dependencyObjectNameMethodNames) {
    # The std::wstring marker is part of the decorated signature. Requiring it
    # keeps native DependencyProperty/DependencyPropertyKey overloads out of
    # this Design-name compatibility boundary.
    $dependencyObjectMethodSymbolPatterns[$methodName] = (
        '\?{0}@DependencyObject@@[^\r\n]*\?\$basic_string@_W' -f
            [regex]::Escape($methodName))
}
$dependencyObjectAuxiliarySymbolPatterns = [ordered]@{
    GetProperties = '\?GetProperties@DependencyObject@@'
}
$dependencyObjectSymbolPatterns = @(
    $dependencyObjectMethodSymbolPatterns.Values
) + @($dependencyObjectAuxiliarySymbolPatterns.Values)
$controlDesignSymbolDefinitions = @(
    [pscustomobject]@{
        Name = 'SetDeclarativeTypeDescriptor'; Category = 'DescriptorPropertyBag'
        Pattern = '\?SetDeclarativeTypeDescriptor@Control@@IEAA_N'
    }
    [pscustomobject]@{
        Name = 'FindObjectPropertyMetadataByName'; Category = 'DescriptorPropertyBag'
        Pattern = '\?FindObjectPropertyMetadataByName@Control@@MEBAPEBVDependencyPropertyMetadata@@[^\r\n]*\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'GetObjectPropertyMetadata'; Category = 'DescriptorPropertyBag'
        Pattern = '\?GetObjectPropertyMetadata@Control@@MEBA'
    }
    [pscustomobject]@{
        Name = 'TryGetDeclarativePropertyBacking'; Category = 'DescriptorPropertyBag'
        Pattern = '\?TryGetDeclarativePropertyBacking@Control@@IEBA_N'
    }
    [pscustomobject]@{
        Name = 'TrySetDeclarativePropertyBacking'; Category = 'DescriptorPropertyBag'
        Pattern = '\?TrySetDeclarativePropertyBacking@Control@@IEAA_N'
    }
    [pscustomobject]@{
        Name = 'FindDeclarativeTemplatePart(nonconst,wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?FindDeclarativeTemplatePart@Control@@QEAAPEAV1@AEBV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'FindDeclarativeTemplatePart(const,wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?FindDeclarativeTemplatePart@Control@@QEBAPEBV1@AEBV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'FindDeclarativeContentPresenter(nonconst,wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?FindDeclarativeContentPresenter@Control@@QEAAPEAV1@AEBV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'FindDeclarativeContentPresenter(const,wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?FindDeclarativeContentPresenter@Control@@QEBAPEBV1@AEBV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'RegisterDeclarativeTemplatePart(wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?RegisterDeclarativeTemplatePart@Control@@IEAA_NV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'RegisterDeclarativeContentPresenter(wstring)'; Category = 'WstringScopePresenter'
        Pattern = '\?RegisterDeclarativeContentPresenter@Control@@IEAA_NV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'FindDeclarativeEvent(wstring)'; Category = 'WstringEvent'
        Pattern = '\?FindDeclarativeEvent@Control@@QEBAPEBUDeclarativeEventDefinition@@AEBV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'RaiseDeclarativeEvent(wstring)'; Category = 'WstringEvent'
        Pattern = '\?RaiseDeclarativeEvent@Control@@QEAA_NV\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'IDeclarativeComponentBehavior::SetReadOnlyProperty'; Category = 'Behavior'
        Pattern = '\?SetReadOnlyProperty@IDeclarativeComponentBehavior@@IEAA_N[^\r\n]*\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'IDeclarativeComponentBehavior::ClearReadOnlyProperty'; Category = 'Behavior'
        Pattern = '\?ClearReadOnlyProperty@IDeclarativeComponentBehavior@@IEAA_N[^\r\n]*\?\$basic_string@_W'
    }
    [pscustomobject]@{
        Name = 'SetDeclarativeComponentBehavior'; Category = 'Behavior'
        Pattern = '\?SetDeclarativeComponentBehavior@Control@@IEAA_N'
    }
    [pscustomobject]@{
        Name = 'ClearDeclarativeComponentBehavior'; Category = 'Behavior'
        Pattern = '\?ClearDeclarativeComponentBehavior@Control@@IEAAXXZ'
    }
)
$controlDesignCategoryExpectedCounts = [ordered]@{
    DescriptorPropertyBag = 5
    WstringScopePresenter = 6
    WstringEvent = 2
    Behavior = 4
}
$controlDesignSymbolPatterns = @(
    $controlDesignSymbolDefinitions | ForEach-Object { $_.Pattern }
)
$controlDesignRegistrarPattern =
    '\?RegisterDependencyProperties@Control@@SAXXZ'
$contentProjectionBoundaryName = 'ContentProjectionDesignCompatibility'
$collectionAdaptersBoundaryName = 'CollectionAdaptersDesignCompatibility'
$styleMutableBoundaryName = 'StyleMutableDesignCompatibility'
$visualStateBoundaryName = 'VisualStateDesignCompatibility'
$contentProjectionMigratedDefinitions = @(
    [pscustomobject]@{ Name = 'ContentPresenter::DisplayMemberPathProperty'; Pattern = '\?DisplayMemberPathProperty@ContentPresenter@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'ContentPresenter::SetDisplayMemberPath(wstring)'; Pattern = '\?SetDisplayMemberPath@ContentPresenter@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ContentPresenter::SetContentTypeName(wstring)'; Pattern = '\?SetContentTypeName@ContentPresenter@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ContentControl::DisplayMemberPathProperty'; Pattern = '\?DisplayMemberPathProperty@ContentControl@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'ContentControl::SetDisplayMemberPath(wstring)'; Pattern = '\?SetDisplayMemberPath@ContentControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ContentControl::SetContentTypeName(wstring)'; Pattern = '\?SetContentTypeName@ContentControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'HeaderedContentControl::HeaderDisplayMemberPathProperty'; Pattern = '\?HeaderDisplayMemberPathProperty@HeaderedContentControl@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'HeaderedContentControl::SetHeaderDisplayMemberPath(wstring)'; Pattern = '\?SetHeaderDisplayMemberPath@HeaderedContentControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'HeaderedContentControl::SetHeaderTypeName(wstring)'; Pattern = '\?SetHeaderTypeName@HeaderedContentControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'HeaderedItemsControl::HeaderDisplayMemberPathProperty'; Pattern = '\?HeaderDisplayMemberPathProperty@HeaderedItemsControl@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'HeaderedItemsControl::SetHeaderDisplayMemberPath(wstring)'; Pattern = '\?SetHeaderDisplayMemberPath@HeaderedItemsControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'HeaderedItemsControl::SetHeaderTypeName(wstring)'; Pattern = '\?SetHeaderTypeName@HeaderedItemsControl@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ItemContainerControl::InitializeItem(wstring)'; Pattern = '\?InitializeItem@ItemContainerControl@@QEAA_NAEBVBindingSourceReference@@AEBVItemTemplateReference@@AEBV\?\$basic_string@_W' }
)
$contentProjectionHookDefinitions = @(
    [pscustomobject]@{ Name = 'ContentPresenter::ReadProjectedDisplayText'; Pattern = '\?ReadProjectedDisplayText@ContentPresenter@@AEBA' }
    [pscustomobject]@{ Name = 'ContentPresenter::ObserveProjectedDisplayPath'; Pattern = '\?ObserveProjectedDisplayPath@ContentPresenter@@AEBA' }
    [pscustomobject]@{ Name = 'ContentControl::ApplyContentProjection'; Pattern = '\?ApplyContentProjection@ContentControl@@AEBAXAEAVContentPresenter@@@Z' }
    [pscustomobject]@{ Name = 'HeaderedContentControl::ApplyHeaderProjection'; Pattern = '\?ApplyHeaderProjection@HeaderedContentControl@@AEBAXAEAVContentPresenter@@@Z' }
    [pscustomobject]@{ Name = 'HeaderedItemsControl::ApplyHeaderProjection'; Pattern = '\?ApplyHeaderProjection@HeaderedItemsControl@@AEBAXAEAVContentPresenter@@@Z' }
)
$contentProjectionAuthoredAdapterDefinitions = @(
    [pscustomobject]@{ Name = 'ContentPresenter::ReadAuthoredProjectedDisplayText'; Pattern = '\?ReadAuthoredProjectedDisplayText@ContentPresenter@@AEBA' }
    [pscustomobject]@{ Name = 'ContentPresenter::ObserveAuthoredProjectedDisplayPath'; Pattern = '\?ObserveAuthoredProjectedDisplayPath@ContentPresenter@@AEBA' }
    [pscustomobject]@{ Name = 'ContentControl::ApplyAuthoredContentProjection'; Pattern = '\?ApplyAuthoredContentProjection@ContentControl@@AEBAXAEAVContentPresenter@@@Z' }
    [pscustomobject]@{ Name = 'HeaderedContentControl::ApplyAuthoredHeaderProjection'; Pattern = '\?ApplyAuthoredHeaderProjection@HeaderedContentControl@@AEBAXAEAVContentPresenter@@@Z' }
    [pscustomobject]@{ Name = 'HeaderedItemsControl::ApplyAuthoredHeaderProjection'; Pattern = '\?ApplyAuthoredHeaderProjection@HeaderedItemsControl@@AEBAXAEAVContentPresenter@@@Z' }
)
$contentProjectionRegistrarDefinitions = @(
    'ContentPresenter'
    'ContentControl'
    'HeaderedContentControl'
    'HeaderedItemsControl'
    'ItemContainerControl'
) | ForEach-Object {
    [pscustomobject]@{
        Name = "$_`::RegisterDependencyProperties"
        Pattern = ('\?RegisterDependencyProperties@{0}@@SAXXZ' -f $_)
    }
}
$collectionAdapterDefinitions = @(
    [pscustomobject]@{ Name = 'CollectionViewSource::ItemTypeName'; Pattern = '\?ItemTypeName@CollectionViewSource@@UEBAAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'CollectionViewSource::SetSourceBindingPath(wstring)'; Pattern = '\?SetSourceBindingPath@CollectionViewSource@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ItemsControl::DisplayMemberPathProperty'; Pattern = '\?DisplayMemberPathProperty@ItemsControl@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'ItemsControl::SetDisplayMemberPath(wstring)'; Pattern = '\?SetDisplayMemberPath@ItemsControl@@UEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ListBoxItem::Initialize(wstring)'; Pattern = '\?Initialize@ListBoxItem@@QEAA_NAEBVBindingSourceReference@@AEBVItemTemplateReference@@AEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Selector::SelectedValuePathProperty'; Pattern = '\?SelectedValuePathProperty@Selector@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'Selector::SetSelectedValuePath(wstring)'; Pattern = '\?SetSelectedValuePath@Selector@@QEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'TreeView::SelectedValuePathProperty'; Pattern = '\?SelectedValuePathProperty@TreeView@@SAAEBVDependencyProperty@@XZ' }
    [pscustomobject]@{ Name = 'TreeView::SetDisplayMemberPath(wstring)'; Pattern = '\?SetDisplayMemberPath@TreeView@@UEAAXV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'TreeView::SetImplicitItemTemplateResolver'; Pattern = '\?SetImplicitItemTemplateResolver@TreeView@@QEAAXV\?\$function@[^\r\n]*\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'TreeView::SetSelectedValuePath(wstring)'; Pattern = '\?SetSelectedValuePath@TreeView@@QEAAXV\?\$basic_string@_W' }
)
$collectionRegistrarHookDefinitions = @(
    'ItemsControl'
    'Selector'
    'TreeViewItem'
    'TreeView'
) | ForEach-Object {
    [pscustomobject]@{
        Name = "$_`::RegisterDesignDependencyProperties"
        Pattern = ('\?RegisterDesignDependencyProperties@{0}@@CAXXZ' -f $_)
    }
}
$collectionSharedRegistrarDefinitions = @(
    'ItemsControl'
    'ListBoxItem'
    'Selector'
    'TreeViewItem'
    'TreeView'
) | ForEach-Object {
    [pscustomobject]@{
        Name = "$_`::RegisterDependencyProperties"
        Pattern = ('\?RegisterDependencyProperties@{0}@@SAXXZ' -f $_)
    }
}
$collectionAuthoredAdapterDefinitions = @(
    [pscustomobject]@{ Name = 'cui::design::AuthoredBindingListItemTypeName'; Pattern = '\?AuthoredBindingListItemTypeName@design@cui@@' }
    [pscustomobject]@{ Name = 'cui::design::HasAuthoredCollectionDescriptionPath(overloads)'; Pattern = '\?HasAuthoredCollectionDescriptionPath@design@cui@@'; PositiveCount = 4 }
    [pscustomobject]@{ Name = 'cui::design::TryReadAuthoredCollectionDescription(overloads)'; Pattern = '\?TryReadAuthoredCollectionDescription@design@cui@@'; PositiveCount = 4 }
    [pscustomobject]@{ Name = 'cui::design::AuthoredCollectionGroupPropertyName'; Pattern = '\?AuthoredCollectionGroupPropertyName@design@cui@@' }
    [pscustomobject]@{ Name = 'CollectionViewSource::ClearAuthoredSourceBindingPath'; Pattern = '\?ClearAuthoredSourceBindingPath@CollectionViewSource@@' }
    [pscustomobject]@{ Name = 'CollectionViewSource::HasAuthoredSourceBindingPath'; Pattern = '\?HasAuthoredSourceBindingPath@CollectionViewSource@@' }
    [pscustomobject]@{ Name = 'CollectionViewSource::ResolveAuthoredSourceBinding'; Pattern = '\?ResolveAuthoredSourceBinding@CollectionViewSource@@' }
    [pscustomobject]@{ Name = 'CollectionViewSource::AppendAuthoredItemObservations'; Pattern = '\?AppendAuthoredItemObservations@CollectionViewSource@@' }
    [pscustomobject]@{ Name = 'ItemsControl::ReadAuthoredDisplayMemberText'; Pattern = '\?ReadAuthoredDisplayMemberText@ItemsControl@@' }
    [pscustomobject]@{ Name = 'ItemsControl::ObserveAuthoredDisplayMemberPath'; Pattern = '\?ObserveAuthoredDisplayMemberPath@ItemsControl@@' }
    [pscustomobject]@{ Name = 'ItemsControl::ApplyAuthoredGeneratedItemProjection'; Pattern = '\?ApplyAuthoredGeneratedItemProjection@ItemsControl@@' }
    [pscustomobject]@{ Name = 'Selector::HasAuthoredSelectedValuePath'; Pattern = '\?HasAuthoredSelectedValuePath@Selector@@' }
    [pscustomobject]@{ Name = 'Selector::TryReadAuthoredSelectedValue'; Pattern = '\?TryReadAuthoredSelectedValue@Selector@@' }
    [pscustomobject]@{ Name = 'Selector::ObserveAuthoredSelectedValuePath'; Pattern = '\?ObserveAuthoredSelectedValuePath@Selector@@' }
    [pscustomobject]@{ Name = 'Selector::TryReadAuthoredSelectedValueAt'; Pattern = '\?TryReadAuthoredSelectedValueAt@Selector@@' }
    [pscustomobject]@{ Name = 'Selector::FindAuthoredSelectedValue'; Pattern = '\?FindAuthoredSelectedValue@Selector@@' }
    [pscustomobject]@{ Name = 'Selector::InitializeAuthoredGeneratedContainer'; Pattern = '\?InitializeAuthoredGeneratedContainer@Selector@@' }
    [pscustomobject]@{ Name = 'TreeView::ResolveAuthoredImplicitItemTemplate'; Pattern = '\?ResolveAuthoredImplicitItemTemplate@TreeView@@' }
    [pscustomobject]@{ Name = 'TreeView::AppendAuthoredItemTypeDiagnostic'; Pattern = '\?AppendAuthoredItemTypeDiagnostic@TreeView@@' }
    [pscustomobject]@{ Name = 'TreeView::ApplyAuthoredGeneratedContainerProjection'; Pattern = '\?ApplyAuthoredGeneratedContainerProjection@TreeView@@' }
    [pscustomobject]@{ Name = 'TreeView::HasAuthoredSelectedValuePath'; Pattern = '\?HasAuthoredSelectedValuePath@TreeView@@' }
    [pscustomobject]@{ Name = 'TreeView::ReadAuthoredSelectedValue'; Pattern = '\?ReadAuthoredSelectedValue@TreeView@@' }
    [pscustomobject]@{ Name = 'TreeView::ObserveAuthoredSelectedValuePath'; Pattern = '\?ObserveAuthoredSelectedValuePath@TreeView@@' }
)

$styleMutableDesignDefinitions = @(
    [pscustomobject]@{ Name = 'cui::style::design::ValidateDynamicDataPathReference'; Pattern = '\?ValidateDynamicDataPathReference@design@style@cui@@' }
    [pscustomobject]@{ Name = 'cui::style::design::FindNamedPropertyMetadata'; Pattern = '\?FindNamedPropertyMetadata@design@style@cui@@' }
    [pscustomobject]@{ Name = 'cui::style::design::TryReadDynamicDataPath'; Pattern = '\?TryReadDynamicDataPath@design@style@cui@@' }
    [pscustomobject]@{ Name = 'cui::style::design::TryParseDataPathSegments'; Pattern = '\?TryParseDataPathSegments@design@style@cui@@' }
    [pscustomobject]@{ Name = 'ControlStyleValue::Resource(wstring)'; Pattern = '\?Resource@ControlStyleValue@@SA\?AU1@V\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleValue::DynamicResource(wstring)'; Pattern = '\?DynamicResource@ControlStyleValue@@SA\?AU1@V\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleSetter::ControlStyleSetter(wstring overloads)'; Pattern = '\?\?0ControlStyleSetter@@QEAA@V\?\$basic_string@_W'; PositiveCount = 2 }
    [pscustomobject]@{ Name = 'ControlStyleSetter::Resource(wstring)'; Pattern = '\?Resource@ControlStyleSetter@@SA\?AU1@V\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleSetter::DynamicResource(wstring)'; Pattern = '\?DynamicResource@ControlStyleSetter@@SA\?AU1@V\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStylePropertyCondition::ControlStylePropertyCondition(wstring)'; Pattern = '\?\?0ControlStylePropertyCondition@@QEAA@V\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleSelector::MatchesTargetType'; Pattern = '\?MatchesTargetType@ControlStyleSelector@@' }
    [pscustomobject]@{ Name = 'ControlStyleSelector::MatchesConditions'; Pattern = '\?MatchesConditions@ControlStyleSelector@@' }
    [pscustomobject]@{ Name = 'ControlStyleSelector::IsConditional'; Pattern = '\?IsConditional@ControlStyleSelector@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::CreateCompiled(mutable program)'; Pattern = '\?CreateCompiled@ControlStyleSheet@@[^\r\n]*UCompiledStyleProgram@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::Rules'; Pattern = '\?Rules@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::Revision'; Pattern = '\?Revision@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::AddRule'; Pattern = '\?AddRule@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::RemoveRule'; Pattern = '\?RemoveRule@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::ClearRules'; Pattern = '\?ClearRules@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::SetResource'; Pattern = '\?SetResource@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::RemoveResource'; Pattern = '\?RemoveResource@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::ClearResources'; Pattern = '\?ClearResources@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::UsesPropertyCondition(wstring)'; Pattern = '\?UsesPropertyCondition@ControlStyleSheet@@QEBA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::UsesPropertyCondition(Control,wstring)'; Pattern = '\?UsesPropertyCondition@ControlStyleSheet@@QEBA_NAEAVControl@@AEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::DataConditionPathsFor'; Pattern = '\?DataConditionPathsFor@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::DataConditionPaths'; Pattern = '\?DataConditionPaths@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::CandidateRuleIdentity'; Pattern = '\?CandidateRuleIdentity@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::EnsureConditionCaches'; Pattern = '\?EnsureConditionCaches@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::SubscribeChanged'; Pattern = '\?SubscribeChanged@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::MatchesDataConditions'; Pattern = '\?MatchesDataConditions@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::NotifyChanged'; Pattern = '\?NotifyChanged@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::TryPopulateDesignTriggerActions'; Pattern = '\?TryPopulateDesignTriggerActions@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'Control::SetDynamicResource(wstring public)'; Pattern = '\?SetDynamicResource@Control@@QEAA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Control::SetDynamicResource(wstring owned)'; Pattern = '\?SetDynamicResource@Control@@IEAA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Control::TrySetDynamicResourceExpressionOwned(wstring)'; Pattern = '\?TrySetDynamicResourceExpressionOwned@Control@@IEAA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Control::ClearDynamicResource(wstring public)'; Pattern = '\?ClearDynamicResource@Control@@QEAA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Control::ClearDynamicResource(wstring owned)'; Pattern = '\?ClearDynamicResource@Control@@IEAA_NAEBV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'Control::TryGetDynamicResourceKey(wstring)'; Pattern = '\?TryGetDynamicResourceKey@Control@@QEAA_NAEBV\?\$basic_string@_W' }
)
$styleFlavorOverrideDefinitions = @(
    [pscustomobject]@{ Name = 'ControlStyleSheet::RuleCount'; Pattern = '\?RuleCount@ControlStyleSheet@@QEBA_KXZ' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::TryGetResource'; Pattern = '\?TryGetResource@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::Resolve'; Pattern = '\?Resolve@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::HasPropertyConditionsFor'; Pattern = '\?HasPropertyConditionsFor@ControlStyleSheet@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::UsesPropertyCondition(args)'; Pattern = '\?UsesPropertyCondition@ControlStyleSheet@@QEBA_NAEAVControl@@AEBUDependencyPropertyChangedEventArgs@@' }
    [pscustomobject]@{ Name = 'ControlStyleSheet::HasDataConditionsFor'; Pattern = '\?HasDataConditionsFor@ControlStyleSheet@@' }
)
$visualStateDesignDefinitions = @(
    [pscustomobject]@{ Name = 'Control::DefineVisualStateGroups'; Pattern = '\?DefineVisualStateGroups@Control@@' }
    [pscustomobject]@{ Name = 'Control::DefineDeclarativeInteractions'; Pattern = '\?DefineDeclarativeInteractions@Control@@' }
    [pscustomobject]@{ Name = 'Control::GoToVisualState(wstring overloads)'; Pattern = '\?GoToVisualState@Control@@QEAA_NAEBV\?\$basic_string@_W'; PositiveCount = 4 }
    [pscustomobject]@{ Name = 'Control::GetCurrentVisualState(wstring)'; Pattern = '\?GetCurrentVisualState@Control@@QEBA\?AV\?\$basic_string@_W' }
    [pscustomobject]@{ Name = 'cui::framework::design::ResolveVisualStateAnimationOperands'; Pattern = '\?ResolveVisualStateAnimationOperands@design@framework@cui@@' }
)
$visualStateBridgeDefinitions = @(
    [pscustomobject]@{ Name = 'Control::InstallDesignInteractionDefinitions'; Pattern = '\?InstallDesignInteractionDefinitions@Control@@' }
    [pscustomobject]@{ Name = 'Control::DeclarativeVisualStateRuntime::TryBuildAnimation'; Pattern = '(?<!\?)\?TryBuildAnimation@DeclarativeVisualStateRuntime@Control@@' }
)
$contentProjectionMigratedSymbolPatterns = @(
    ($contentProjectionMigratedDefinitions +
        $contentProjectionAuthoredAdapterDefinitions) |
        ForEach-Object { $_.Pattern }
)
$collectionAdaptersSymbolPatterns = @(
    ($collectionAdapterDefinitions + $collectionRegistrarHookDefinitions +
        $collectionAuthoredAdapterDefinitions) |
        ForEach-Object { $_.Pattern }
)
$styleMutableSymbolPatterns = @(
    $styleMutableDesignDefinitions | ForEach-Object { $_.Pattern }
)
$visualStateSymbolPatterns = @(
    $visualStateDesignDefinitions | ForEach-Object { $_.Pattern }
)
$physicalBoundaryNames = @($registryNames) + @(
    $dependencyPropertyBoundaryName
    $dependencyObjectBoundaryName
    $controlDesignBoundaryName
    $contentProjectionBoundaryName
    $collectionAdaptersBoundaryName
    $styleMutableBoundaryName
    $visualStateBoundaryName
)

$requiredDesignArchiveMembers = @(
    'BindingConverterRegistry.Design.obj'
    'DependencyPropertyRegistry.Design.obj'
    'DependencyObject.Design.obj'
    'Control.Design.obj'
    'ContentProjection.Design.obj'
    'CollectionAdapters.Design.obj'
    'StyleMutableBackend.Design.obj'
    'VisualState.Design.obj'
    'XamlSchema.Design.obj'
)

$mapDependencyObjectSymbolPattern = '(?:' +
    (($dependencyObjectMethodSymbolPatterns.Values +
        $dependencyObjectAuxiliarySymbolPatterns.Values) -join '|') + ')'
$mapDynamicDependencyPropertyPattern = '(?:' +
    (($dependencyPropertySymbolPatterns +
        ($dependencyPropertyStorageNames | ForEach-Object {
            [regex]::Escape($_)
        }) + @('IsPropertyNameLess', 'IsRegistryPropertyNameLess')) -join '|') +
    ')'
$mapConverterRegistryPattern = '(?:' +
    (($registryNames | ForEach-Object { [regex]::Escape($_) }) -join '|') +
    ')'
$mapForbiddenChecks = @(
    [pscustomobject]@{
        Name = 'Design/archive library provenance'
        Pattern = '(?i:(?:CuiRuntime|CUIDesignCore|XmlLite):)'
    }
    [pscustomobject]@{
        Name = 'Design object member'
        Pattern = '(?i:\.Design\.obj\b)'
    }
    [pscustomobject]@{
        Name = 'runtime XAML implementation'
        Pattern = '(?:XamlObjectMaterializer|XamlFrameworkTheme|XamlSchema)'
    }
    [pscustomobject]@{
        Name = 'dependency-property name-sort helper'
        Pattern = '(?:IsPropertyNameLess|IsRegistryPropertyNameLess)'
    }
    [pscustomobject]@{
        Name = 'DependencyObject name compatibility member'
        Pattern = $mapDependencyObjectSymbolPattern
    }
    [pscustomobject]@{
        Name = 'dynamic dependency-property registry/storage'
        Pattern = $mapDynamicDependencyPropertyPattern
    }
    [pscustomobject]@{
        Name = 'binding converter registry'
        Pattern = $mapConverterRegistryPattern
    }
    [pscustomobject]@{
        Name = 'Control Design compatibility member'
        Pattern = '(?:' + ($controlDesignSymbolPatterns -join '|') + ')'
    }
    [pscustomobject]@{
        Name = 'content-projection Design compatibility member'
        Pattern = '(?:' +
            ($contentProjectionMigratedSymbolPatterns -join '|') + ')'
    }
    [pscustomobject]@{
        Name = 'collection-adapter Design compatibility member'
        Pattern = '(?:' + ($collectionAdaptersSymbolPatterns -join '|') + ')'
    }
    [pscustomobject]@{
        Name = 'style mutable/name Design compatibility member'
        Pattern = '(?:' + ($styleMutableSymbolPatterns -join '|') + ')'
    }
    [pscustomobject]@{
        Name = 'visual-state name/definition Design compatibility member'
        Pattern = '(?:' + ($visualStateSymbolPatterns -join '|') + ')'
    }
)

function Resolve-LeafPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "A8f physical-boundary gate cannot find $Description`: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Resolve-DumpbinPath {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        return Resolve-LeafPath -Path $ExplicitPath -Description 'dumpbin.exe'
    }

    $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($env:VCToolsInstallDir) {
        $candidates.Add((Join-Path $env:VCToolsInstallDir `
            'bin\Hostx64\x64\dumpbin.exe'))
        $candidates.Add((Join-Path $env:VCToolsInstallDir `
            'bin\Hostx86\x86\dumpbin.exe'))
    }

    $programFilesX86 = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $programFilesX86 `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installationPath = & $vswhere -latest -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1
        if ($installationPath) {
            $toolRoot = Join-Path $installationPath 'VC\Tools\MSVC'
            if (Test-Path -LiteralPath $toolRoot -PathType Container) {
                $toolVersions = @(Get-ChildItem -LiteralPath $toolRoot `
                    -Directory | Sort-Object Name -Descending)
                foreach ($toolVersion in $toolVersions) {
                    $candidates.Add((Join-Path $toolVersion.FullName `
                        'bin\Hostx64\x64\dumpbin.exe'))
                    $candidates.Add((Join-Path $toolVersion.FullName `
                        'bin\Hostx86\x86\dumpbin.exe'))
                }
            }
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw ('A8f physical-boundary gate cannot locate dumpbin.exe. ' +
        'Run from a Visual Studio developer shell or pass -DumpbinPath.')
}

function Get-SourcePatternMatches {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern
    )

    return @(Select-String -LiteralPath $Path -Pattern $Pattern `
        -AllMatches -CaseSensitive)
}

function Format-SourceMatchSamples {
    param([object[]]$Matches)

    return (($Matches | Select-Object -First 4 | ForEach-Object {
        '{0}:{1}: {2}' -f $_.Path, $_.LineNumber, $_.Line.Trim()
    }) -join [Environment]::NewLine)
}

function Assert-SourcePatternAbsent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $matches = @(Get-SourcePatternMatches -Path $Path -Pattern $Pattern)
    if ($matches.Count -ne 0) {
        $samples = Format-SourceMatchSamples -Matches $matches
        throw ("A8f source boundary failed: $Description. " +
            "Found $($matches.Count) matching line(s)." +
            [Environment]::NewLine + $samples)
    }
    Write-Host ("[PASS] {0}: 0 forbidden matching lines" -f $Description)
}

function Assert-SourcePatternPresent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $matches = @(Get-SourcePatternMatches -Path $Path -Pattern $Pattern)
    if ($matches.Count -eq 0) {
        throw "A8f source boundary failed: $Description was not found in $Path"
    }
    Write-Host ("[PASS] {0}: {1} matching line(s)" -f `
        $Description, $matches.Count)
}

function Assert-SourceTextPatternAbsent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $text = Get-Content -LiteralPath $Path -Raw
    $matches = @([regex]::Matches(
        $text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant))
    if ($matches.Count -ne 0) {
        $sample = $matches[0].Value.Trim()
        throw ("A8f source boundary failed: $Description. " +
            "Found $($matches.Count) matching definition(s)." +
            [Environment]::NewLine + $sample)
    }
    Write-Host ("[PASS] {0}: 0 forbidden definitions" -f $Description)
}

function Assert-SourceTextPatternPresent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $text = Get-Content -LiteralPath $Path -Raw
    $matches = @([regex]::Matches(
        $text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant))
    if ($matches.Count -eq 0) {
        throw "A8f source boundary failed: $Description was not found in $Path"
    }
    Write-Host ("[PASS] {0}: {1} matching definition(s)" -f `
        $Description, $matches.Count)
}

function Assert-SourceTextPatternCount {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [int]$ExpectedCount,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $text = Get-Content -LiteralPath $Path -Raw
    $matches = @([regex]::Matches(
        $text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant))
    if ($matches.Count -ne $ExpectedCount) {
        $samples = @($matches | Select-Object -First 4 | ForEach-Object {
            $_.Value.Trim()
        }) -join [Environment]::NewLine
        throw ("A8f source boundary failed: $Description expected " +
            "$ExpectedCount definition(s), found $($matches.Count)." +
            [Environment]::NewLine + $samples)
    }
    Write-Host ("[PASS] {0}: {1}/{1} definitions" -f `
        $Description, $ExpectedCount)
}

function Test-SourceBoundary {
    param([string]$RepositoryRoot)

    $productionHeader = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\include\Binding.h') `
        -Description 'Production Binding header'
    $productionSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\src\Binding.cpp') `
        -Description 'Production Binding source'
    $runtimeHeader = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\include\BindingConverterRegistry.h') `
        -Description 'Design converter-registry header'
    $runtimeSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\BindingConverterRegistry.Design.cpp') `
        -Description 'Design converter-registry source'
    $productionDependencyObjectSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\src\DependencyObject.cpp') `
        -Description 'Production DependencyObject source'
    $runtimeDependencyObjectSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\DependencyObject.Design.cpp') `
        -Description 'Design DependencyObject compatibility source'
    $runtimeDependencyPropertyRegistrySource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\DependencyPropertyRegistry.Design.cpp') `
        -Description 'Design dependency-property registry source'
    $productionControlSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\src\Control.cpp') `
        -Description 'Production Control source'
    $runtimeControlSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\Control.Design.cpp') `
        -Description 'Design Control compatibility source'
    $runtimeContentProjectionSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\ContentProjection.Design.cpp') `
        -Description 'Design content-projection compatibility source'
    $runtimeCollectionAdaptersSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\CollectionAdapters.Design.cpp') `
        -Description 'Design collection-adapter compatibility source'
    $productionStyleSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\src\Style.cpp') `
        -Description 'Production/shared Style source'
    $runtimeStyleMutableSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\StyleMutableBackend.Design.cpp') `
        -Description 'Design mutable-style backend source'
    $runtimeVisualStateSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\VisualState.Design.cpp') `
        -Description 'Design visual-state name adapter source'
    $productionContentSources = [ordered]@{}
    foreach ($sourceName in @(
            'ContentPresenter', 'ContentControl', 'HeaderedContentControl',
            'HeaderedItemsControl', 'ItemContainer')) {
        $productionContentSources[$sourceName] = Resolve-LeafPath `
            -Path (Join-Path $RepositoryRoot "CUI\src\$sourceName.cpp") `
            -Description "Production $sourceName source"
    }
    $productionCollectionSources = [ordered]@{}
    foreach ($sourceName in @(
            'CollectionViewSource', 'ItemsControl', 'Selector', 'TreeView')) {
        $productionCollectionSources[$sourceName] = Resolve-LeafPath `
            -Path (Join-Path $RepositoryRoot "CUI\src\$sourceName.cpp") `
            -Description "Production $sourceName source"
    }

    $legacyXamlSchemaSource = Join-Path $RepositoryRoot `
        'CUI\src\XamlSchema.cpp'
    if (Test-Path -LiteralPath $legacyXamlSchemaSource) {
        throw ('A8f source boundary failed: CUI still owns XamlSchema.cpp: ' +
            $legacyXamlSchemaSource)
    }
    Write-Host '[PASS] CUI source tree no longer owns XamlSchema.cpp'

    $runtimeXamlSchemaSource = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot `
            'CuiRuntime\Runtime\XamlSchema.Design.cpp') `
        -Description 'Design XamlSchema source'
    $productionProject = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CUI\CUI.vcxproj') `
        -Description 'Production CUI project'
    $runtimeProject = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CuiRuntime\CuiRuntime.vcxproj') `
        -Description 'Design CuiRuntime project'
    $runtimeProjectFilters = Resolve-LeafPath `
        -Path (Join-Path $RepositoryRoot 'CuiRuntime\CuiRuntime.vcxproj.filters') `
        -Description 'Design CuiRuntime project filters'

    Assert-SourcePatternAbsent -Path $productionProject `
        -Pattern 'src\\XamlSchema\.cpp' `
        -Description 'CUI project XamlSchema implementation ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\XamlSchema\.Design\.cpp' `
        -Description 'CuiRuntime project XamlSchema Design implementation'
    Assert-SourcePatternPresent -Path $runtimeXamlSchemaSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'XamlSchema Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\DependencyObject\.Design\.cpp' `
        -Description 'CuiRuntime project DependencyObject Design compatibility'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\DependencyPropertyRegistry\.Design\.cpp' `
        -Description 'CuiRuntime project dependency-property registry ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\Control\.Design\.cpp' `
        -Description 'CuiRuntime project Control Design compatibility ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\ContentProjection\.Design\.cpp' `
        -Description 'CuiRuntime project content-projection Design ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\CollectionAdapters\.Design\.cpp' `
        -Description 'CuiRuntime project collection-adapter Design ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\StyleMutableBackend\.Design\.cpp' `
        -Description 'CuiRuntime project mutable-style Design ownership'
    Assert-SourcePatternPresent -Path $runtimeProject `
        -Pattern 'Runtime\\VisualState\.Design\.cpp' `
        -Description 'CuiRuntime project visual-state Design ownership'
    Assert-SourcePatternPresent -Path $runtimeProjectFilters `
        -Pattern ('Runtime\\StyleMutableBackend\.Design\.cpp[^<]*<Filter>' +
            'Design Compatibility</Filter>') `
        -Description 'mutable-style Design source filter ownership'
    Assert-SourcePatternPresent -Path $runtimeProjectFilters `
        -Pattern ('Runtime\\VisualState\.Design\.cpp[^<]*<Filter>' +
            'Design Compatibility</Filter>') `
        -Description 'visual-state Design source filter ownership'
    Assert-SourcePatternPresent -Path $runtimeDependencyObjectSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'DependencyObject Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeDependencyPropertyRegistrySource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'dependency-property registry Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeControlSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'Control Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeContentProjectionSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'content-projection Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeCollectionAdaptersSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'collection-adapter Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeStyleMutableSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'mutable-style backend Design-only compilation guard'
    Assert-SourcePatternPresent -Path $runtimeVisualStateSource `
        -Pattern '#if\s+!CUI_ENABLE_DYNAMIC_XAML' `
        -Description 'visual-state adapter Design-only compilation guard'

    $publicTypePattern = ('^\s*(?:class|struct)\s+' +
        '(?:[A-Za-z_]\w*\s+)*(?:' +
        'BindingValueConverterMetadata|' +
        'MultiBindingValueConverterMetadata|' +
        'BindingValueConverterRegistry|' +
        'MultiBindingValueConverterRegistry)\b')
    Assert-SourcePatternAbsent -Path $productionHeader `
        -Pattern $publicTypePattern `
        -Description 'Production Binding.h registry class/metadata declarations'

    $implementationPattern = ('\b(?:' +
        'BindingValueConverterMetadata|' +
        'MultiBindingValueConverterMetadata|' +
        'BindingValueConverterRegistry|' +
        'MultiBindingValueConverterRegistry|' +
        'ConverterRegistryEntry|' +
        'MultiConverterRegistryEntry|' +
        'BuiltInBindingConverters|' +
        'RegisteredBindingConverters|' +
        'RegisteredMultiBindingConverters|' +
        'BindingConverterMutex|' +
        'MultiBindingConverterMutex)\b')
    Assert-SourcePatternAbsent -Path $productionSource `
        -Pattern $implementationPattern `
        -Description 'Production Binding.cpp registry implementation/storage'

    $dependencyPropertyMethodPattern = ('\bDependencyPropertyRegistry\s*::\s*' +
        '(?:' + (($dependencyPropertyRegistryMethodNames | ForEach-Object {
            [regex]::Escape($_)
        }) -join '|') + ')\s*\(')
    $dependencyPropertyStoragePattern = ('\b(?:' +
        (($dependencyPropertyStorageNames | ForEach-Object {
            [regex]::Escape($_)
        }) -join '|') + '|IsPropertyNameLess|IsRegistryPropertyNameLess)\b')
    Assert-SourcePatternAbsent -Path $productionSource `
        -Pattern $dependencyPropertyMethodPattern `
        -Description 'Production Binding.cpp dynamic dependency-property methods'
    Assert-SourcePatternAbsent -Path $productionSource `
        -Pattern $dependencyPropertyStoragePattern `
        -Description 'Production Binding.cpp dependency-property storage/name-sort helpers'

    foreach ($methodName in $dependencyPropertyRegistryMethodNames) {
        Assert-SourcePatternPresent -Path $runtimeDependencyPropertyRegistrySource `
            -Pattern ('\bDependencyPropertyRegistry\s*::\s*{0}\s*\(' -f `
                [regex]::Escape($methodName)) `
            -Description ("Design dependency-property registry method {0}" -f `
                $methodName)
    }
    Assert-SourcePatternPresent -Path $runtimeDependencyPropertyRegistrySource `
        -Pattern '\bDependencyPropertyRegistry\s*::\s*GetMetadata\s*\(' `
        -Description 'Design dependency-property registry GetMetadata implementation'
    foreach ($storageName in $dependencyPropertyStorageNames) {
        Assert-SourcePatternPresent -Path $runtimeDependencyPropertyRegistrySource `
            -Pattern ('\b{0}\b' -f [regex]::Escape($storageName)) `
            -Description ("Design dependency-property storage {0}" -f $storageName)
    }
    Assert-SourcePatternPresent -Path $runtimeDependencyPropertyRegistrySource `
        -Pattern '\bIsRegistryPropertyNameLess\b' `
        -Description 'Design dependency-property registry name-sort helper'

    Assert-SourcePatternAbsent -Path $productionProject `
        -Pattern ('DependencyObject\.Design\.cpp|' +
            'DependencyPropertyRegistry\.Design\.cpp|Control\.Design\.cpp|' +
            'ContentProjection\.Design\.cpp|CollectionAdapters\.Design\.cpp|' +
            'StyleMutableBackend\.Design\.cpp|VisualState\.Design\.cpp') `
        -Description 'CUI project Design compatibility source ownership'
    foreach ($methodName in $dependencyObjectNameMethodNames) {
        $nameMethodDefinitionTemplate = (
            '(?s)\bDependencyObject\s*::\s*{0}\s*\(\s*' +
            'const\s+std\s*::\s*wstring\s*&')
        $nameMethodDefinitionPattern = $nameMethodDefinitionTemplate -f
            [regex]::Escape($methodName)
        Assert-SourceTextPatternAbsent -Path $productionDependencyObjectSource `
            -Pattern $nameMethodDefinitionPattern `
            -Description ("Production DependencyObject::{0}(std::wstring)" -f `
                $methodName)
        Assert-SourceTextPatternPresent -Path $runtimeDependencyObjectSource `
            -Pattern $nameMethodDefinitionPattern `
            -Description ("Design DependencyObject::{0}(std::wstring)" -f `
                $methodName)
    }
    Assert-SourcePatternAbsent -Path $productionDependencyObjectSource `
        -Pattern '\bDependencyObject\s*::\s*GetProperties\s*\(' `
        -Description 'Production DependencyObject property enumeration implementation'
    Assert-SourcePatternPresent -Path $runtimeDependencyObjectSource `
        -Pattern '\bDependencyObject\s*::\s*GetProperties\s*\(' `
        -Description 'Design DependencyObject property enumeration implementation'

    $controlDescriptorSourcePattern = ('(?s)\bControl\s*::\s*(?:' +
        'SetDeclarativeTypeDescriptor|FindObjectPropertyMetadataByName|' +
        'GetObjectPropertyMetadata|TryGetDeclarativePropertyBacking|' +
        'TrySetDeclarativePropertyBacking)\s*\(')
    $controlScopeSourcePattern = ('(?s)(?:' +
        '\bControl\s*::\s*(?:FindDeclarativeTemplatePart|' +
        'FindDeclarativeContentPresenter)\s*\(\s*' +
        'const\s+std\s*::\s*wstring\s*&|' +
        '\bControl\s*::\s*(?:RegisterDeclarativeTemplatePart|' +
        'RegisterDeclarativeContentPresenter)\s*\(\s*' +
        'std\s*::\s*wstring\b)')
    $controlEventSourcePattern = ('(?s)(?:' +
        '\bControl\s*::\s*FindDeclarativeEvent\s*\(\s*' +
        'const\s+std\s*::\s*wstring\s*&|' +
        '\bControl\s*::\s*RaiseDeclarativeEvent\s*\(\s*' +
        'std\s*::\s*wstring\b)')
    $controlBehaviorSourcePattern = ('(?s)(?:' +
        '\bIDeclarativeComponentBehavior\s*::\s*' +
        '(?:SetReadOnlyProperty|ClearReadOnlyProperty)\s*\(|' +
        '\bControl\s*::\s*(?:SetDeclarativeComponentBehavior|' +
        'ClearDeclarativeComponentBehavior)\s*\()')
    foreach ($sourceGroup in @(
            @{ Name = 'descriptor/property-bag'; Pattern = $controlDescriptorSourcePattern; Count = 5 },
            @{ Name = 'wstring scope/presenter'; Pattern = $controlScopeSourcePattern; Count = 6 },
            @{ Name = 'wstring event'; Pattern = $controlEventSourcePattern; Count = 2 },
            @{ Name = 'behavior'; Pattern = $controlBehaviorSourcePattern; Count = 4 })) {
        Assert-SourceTextPatternCount -Path $productionControlSource `
            -Pattern $sourceGroup.Pattern -ExpectedCount 0 `
            -Description ("Production Control {0} compatibility" -f `
                $sourceGroup.Name)
        Assert-SourceTextPatternCount -Path $runtimeControlSource `
            -Pattern $sourceGroup.Pattern -ExpectedCount $sourceGroup.Count `
            -Description ("Design Control {0} compatibility" -f `
                $sourceGroup.Name)
    }
    Assert-SourceTextPatternCount -Path $runtimeControlSource `
        -Pattern '(?s)\bControl\s*::\s*RegisterDependencyProperties\s*\(\s*\)' `
        -ExpectedCount 1 -Description 'Design Control registrar ownership'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern ('(?s)#if\s+!CUI_ENABLE_DYNAMIC_XAML\s+' +
            'void\s+Control\s*::\s*RegisterDependencyProperties\s*' +
            '\(\s*\)\s*\{\s*\}\s*#endif') `
        -ExpectedCount 1 `
        -Description 'allowed empty Production Control registrar'

    $contentMigratedSourcePattern = ('(?s)\b(?:' +
        'ContentPresenter\s*::\s*(?:DisplayMemberPathProperty|' +
        'SetDisplayMemberPath|SetContentTypeName)|' +
        'ContentControl\s*::\s*(?:DisplayMemberPathProperty|' +
        'SetDisplayMemberPath|SetContentTypeName)|' +
        'HeaderedContentControl\s*::\s*(?:HeaderDisplayMemberPathProperty|' +
        'SetHeaderDisplayMemberPath|SetHeaderTypeName)|' +
        'HeaderedItemsControl\s*::\s*(?:HeaderDisplayMemberPathProperty|' +
        'SetHeaderDisplayMemberPath|SetHeaderTypeName))\s*\(|' +
        '\bItemContainerControl\s*::\s*InitializeItem\s*\(\s*' +
        'const\s+BindingSourceReference\s*&[^\)]*' +
        'const\s+std\s*::\s*wstring\s*&\s*displayMemberPath')
    $contentCompiledHookSourcePattern = ('(?s)\b(?:' +
        'ContentPresenter\s*::\s*(?:ReadProjectedDisplayText|' +
        'ObserveProjectedDisplayPath)|' +
        'ContentControl\s*::\s*ApplyContentProjection|' +
        'HeaderedContentControl\s*::\s*ApplyHeaderProjection|' +
        'HeaderedItemsControl\s*::\s*ApplyHeaderProjection)\s*\(')
    $contentAuthoredAdapterSourcePattern = ('(?s)\b(?:' +
        'ContentPresenter\s*::\s*(?:ReadAuthoredProjectedDisplayText|' +
        'ObserveAuthoredProjectedDisplayPath)|' +
        'ContentControl\s*::\s*ApplyAuthoredContentProjection|' +
        'HeaderedContentControl\s*::\s*ApplyAuthoredHeaderProjection|' +
        'HeaderedItemsControl\s*::\s*ApplyAuthoredHeaderProjection)\s*\(')
    $contentRegistrarSourcePattern = ('(?s)\bvoid\s+(?:ContentPresenter|' +
        'ContentControl|HeaderedContentControl|HeaderedItemsControl|' +
        'ItemContainerControl)\s*::\s*RegisterDependencyProperties\s*\(')
    $contentStorageDefinitionPattern = ('(?s)\bconst\s+DependencyProperty\s*\*' +
        '\s*(?:contentPresenterDisplayMemberPathProperty|' +
        'contentControlDisplayMemberPathProperty|' +
        'headeredContentDisplayMemberPathProperty|' +
        'headeredItemsDisplayMemberPathProperty)\s*=\s*nullptr')
    foreach ($productionSource in $productionContentSources.Values) {
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $contentMigratedSourcePattern -ExpectedCount 0 `
            -Description 'Production content-projection migrated definitions'
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $contentStorageDefinitionPattern -ExpectedCount 0 `
            -Description 'Production content-projection dynamic DP storage'
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $contentAuthoredAdapterSourcePattern -ExpectedCount 0 `
            -Description 'Production content authored-adapter definitions'
    }
    Assert-SourceTextPatternCount -Path $runtimeContentProjectionSource `
        -Pattern $contentMigratedSourcePattern -ExpectedCount 13 `
        -Description 'Design content-projection migrated members'
    Assert-SourceTextPatternCount -Path $runtimeContentProjectionSource `
        -Pattern $contentCompiledHookSourcePattern -ExpectedCount 0 `
        -Description 'Design archive excludes compiled content hooks'
    Assert-SourceTextPatternCount -Path $runtimeContentProjectionSource `
        -Pattern $contentAuthoredAdapterSourcePattern -ExpectedCount 5 `
        -Description 'Design content-projection authored adapters'
    Assert-SourceTextPatternCount -Path $runtimeContentProjectionSource `
        -Pattern $contentRegistrarSourcePattern -ExpectedCount 5 `
        -Description 'Design content-projection registrars'
    Assert-SourceTextPatternCount -Path $runtimeContentProjectionSource `
        -Pattern $contentStorageDefinitionPattern -ExpectedCount 4 `
        -Description 'Design content-projection dynamic DP storage'
    Assert-SourceTextPatternCount `
        -Path $productionContentSources.ContentPresenter `
        -Pattern $contentCompiledHookSourcePattern -ExpectedCount 2 `
        -Description 'allowed Production ContentPresenter compiled hooks'
    foreach ($sourceName in @(
            'ContentControl', 'HeaderedContentControl',
            'HeaderedItemsControl')) {
        Assert-SourceTextPatternCount `
            -Path $productionContentSources[$sourceName] `
            -Pattern $contentCompiledHookSourcePattern -ExpectedCount 1 `
            -Description "allowed Production $sourceName compiled hook"
    }
    foreach ($sourceName in $productionContentSources.Keys) {
        Assert-SourceTextPatternCount `
            -Path $productionContentSources[$sourceName] `
            -Pattern $contentRegistrarSourcePattern -ExpectedCount 1 `
            -Description "allowed Production $sourceName base registrar"
    }

    $collectionDirectSourcePattern = ('(?s)\b(?:' +
        'const\s+std\s*::\s*wstring\s*&\s*' +
        'CollectionViewSource\s*::\s*ItemTypeName|' +
        'void\s+CollectionViewSource\s*::\s*SetSourceBindingPath|' +
        'const\s+DependencyProperty\s*&\s*ItemsControl\s*::\s*' +
        'DisplayMemberPathProperty|' +
        'void\s+ItemsControl\s*::\s*SetDisplayMemberPath|' +
        'const\s+DependencyProperty\s*&\s*Selector\s*::\s*' +
        'SelectedValuePathProperty|' +
        'void\s+Selector\s*::\s*SetSelectedValuePath|' +
        'const\s+DependencyProperty\s*&\s*TreeView\s*::\s*' +
        'SelectedValuePathProperty|' +
        'void\s+TreeView\s*::\s*(?:SetDisplayMemberPath|' +
        'SetImplicitItemTemplateResolver|SetSelectedValuePath))\s*\(|' +
        '\bbool\s+ListBoxItem\s*::\s*Initialize\s*\(\s*' +
        'const\s+BindingSourceReference\s*&[^\)]*' +
        'const\s+std\s*::\s*wstring\s*&\s*displayMemberPath')
    $collectionHookSourcePattern = ('(?s)\bvoid\s+(?:ItemsControl|Selector|' +
        'TreeViewItem|TreeView)\s*::\s*' +
        'RegisterDesignDependencyProperties\s*\(')
    $collectionHelperDefinitionPattern = ('(?s)\bconst\s+' +
        'DependencyProperty\s*&\s*(?:RegisteredItemsControlProperty|' +
        'RegisteredSelectorProperty|RegisteredTreeProperty)\s*\(')
    foreach ($productionSource in $productionCollectionSources.Values) {
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $collectionDirectSourcePattern -ExpectedCount 0 `
            -Description 'Production collection-adapter migrated definitions'
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $collectionHookSourcePattern -ExpectedCount 0 `
            -Description 'Production collection Design registrar hooks'
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $collectionHelperDefinitionPattern -ExpectedCount 0 `
            -Description 'Production collection name-registry helpers'
    }
    Assert-SourceTextPatternCount -Path $runtimeCollectionAdaptersSource `
        -Pattern $collectionDirectSourcePattern -ExpectedCount 11 `
        -Description 'Design collection-adapter migrated members'
    Assert-SourceTextPatternCount -Path $runtimeCollectionAdaptersSource `
        -Pattern $collectionHookSourcePattern -ExpectedCount 4 `
        -Description 'Design collection registrar hooks'
    Assert-SourceTextPatternCount -Path $runtimeCollectionAdaptersSource `
        -Pattern $collectionHelperDefinitionPattern -ExpectedCount 3 `
        -Description 'Design collection name-registry helpers'
    $collectionAuthoredFreeDefinitionPattern = ('(?s)\b(?:' +
        'AuthoredBindingListItemTypeName|' +
        'HasAuthoredCollectionDescriptionPath|' +
        'TryReadAuthoredCollectionDescription|' +
        'AuthoredCollectionGroupPropertyName)\s*\([^;{]*\)\s*' +
        '(?:noexcept\s*)?\{')
    $collectionAuthoredMemberSourcePattern = ('(?s)\b(?:' +
        'CollectionViewSource\s*::\s*(?:ClearAuthoredSourceBindingPath|' +
        'HasAuthoredSourceBindingPath|ResolveAuthoredSourceBinding|' +
        'AppendAuthoredItemObservations)|' +
        'ItemsControl\s*::\s*(?:ReadAuthoredDisplayMemberText|' +
        'ObserveAuthoredDisplayMemberPath|' +
        'ApplyAuthoredGeneratedItemProjection)|' +
        'Selector\s*::\s*(?:HasAuthoredSelectedValuePath|' +
        'TryReadAuthoredSelectedValue|ObserveAuthoredSelectedValuePath|' +
        'TryReadAuthoredSelectedValueAt|FindAuthoredSelectedValue|' +
        'InitializeAuthoredGeneratedContainer)|' +
        'TreeView\s*::\s*(?:ResolveAuthoredImplicitItemTemplate|' +
        'AppendAuthoredItemTypeDiagnostic|' +
        'ApplyAuthoredGeneratedContainerProjection|' +
        'HasAuthoredSelectedValuePath|ReadAuthoredSelectedValue|' +
        'ObserveAuthoredSelectedValuePath))\s*\(')
    foreach ($productionSource in $productionCollectionSources.Values) {
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $collectionAuthoredFreeDefinitionPattern `
            -ExpectedCount 0 `
            -Description 'Production collection authored free definitions'
        Assert-SourceTextPatternCount -Path $productionSource `
            -Pattern $collectionAuthoredMemberSourcePattern `
            -ExpectedCount 0 `
            -Description 'Production collection authored member definitions'
    }
    Assert-SourceTextPatternCount -Path $runtimeCollectionAdaptersSource `
        -Pattern $collectionAuthoredFreeDefinitionPattern -ExpectedCount 10 `
        -Description 'Design collection authored free adapters'
    Assert-SourceTextPatternCount -Path $runtimeCollectionAdaptersSource `
        -Pattern $collectionAuthoredMemberSourcePattern -ExpectedCount 19 `
        -Description 'Design collection authored member adapters'
    Assert-SourceTextPatternCount `
        -Path $productionCollectionSources.ItemsControl `
        -Pattern '\bvoid\s+ItemsControl\s*::\s*RegisterDependencyProperties\s*\(' `
        -ExpectedCount 1 -Description 'allowed shared ItemsControl registrar'
    Assert-SourceTextPatternCount `
        -Path $productionCollectionSources.Selector `
        -Pattern ('\bvoid\s+(?:ListBoxItem|Selector)\s*::\s*' +
            'RegisterDependencyProperties\s*\(') `
        -ExpectedCount 2 -Description 'allowed shared Selector registrars'
    Assert-SourceTextPatternCount `
        -Path $productionCollectionSources.TreeView `
        -Pattern ('\bvoid\s+(?:TreeViewItem|TreeView)\s*::\s*' +
            'RegisterDependencyProperties\s*\(') `
        -ExpectedCount 2 -Description 'allowed shared TreeView registrars'

    $styleDesignHookSourcePattern = ('(?s)\b(?:' +
        'ValidateDynamicDataPathReference|FindNamedPropertyMetadata|' +
        'TryReadDynamicDataPath|TryParseDataPathSegments)\s*' +
        '\([^;{]*\)\s*\{')
    $styleBuilderSourcePattern = ('(?m)^\s*(?:' +
        'ControlStyleValue\s+ControlStyleValue\s*::\s*' +
            '(?:Resource|DynamicResource)|' +
        'ControlStyleSetter\s*::\s*ControlStyleSetter|' +
        'ControlStyleSetter\s+ControlStyleSetter\s*::\s*' +
            '(?:Resource|DynamicResource)|' +
        'ControlStylePropertyCondition\s*::\s*' +
            'ControlStylePropertyCondition)\s*\(')
    $styleSelectorSourcePattern = ('\bControlStyleSelector\s*::\s*' +
        '(?:MatchesTargetType|MatchesConditions|IsConditional)\s*\(')
    $styleSheetSourcePattern = ('\bControlStyleSheet\s*::\s*(?:' +
        'CreateCompiled|Rules|Revision|AddRule|RemoveRule|ClearRules|' +
        'RuleCount|SetResource|RemoveResource|ClearResources|' +
        'TryGetResource|Resolve|UsesPropertyCondition|' +
        'HasPropertyConditionsFor|HasDataConditionsFor|' +
        'DataConditionPathsFor|DataConditionPaths|CandidateRuleIdentity|' +
        'EnsureConditionCaches|SubscribeChanged|MatchesDataConditions|' +
        'NotifyChanged|TryPopulateDesignTriggerActions)\s*\(')
    $styleNameResourceSourcePattern = ('\bControl\s*::\s*(?:' +
        'SetDynamicResource|TrySetDynamicResourceExpressionOwned|' +
        'ClearDynamicResource|TryGetDynamicResourceKey)\s*\(')
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleDesignHookSourcePattern -ExpectedCount 4 `
        -Description 'Design mutable-style name/data-path hooks'
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleBuilderSourcePattern -ExpectedCount 7 `
        -Description 'Design mutable-style builder/value definitions'
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleSelectorSourcePattern -ExpectedCount 3 `
        -Description 'Design mutable-style selector definitions'
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleSheetSourcePattern -ExpectedCount 25 `
        -Description 'Design mutable-style sheet definitions'
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleNameResourceSourcePattern -ExpectedCount 6 `
        -Description 'Design name-based DynamicResource definitions'
    $styleFlavorSourcePattern = ('(?s)(?:\bControlStyleSheet\s*::\s*(?:' +
        'RuleCount|TryGetResource|Resolve|HasPropertyConditionsFor|' +
        'HasDataConditionsFor)\s*\(|' +
        '\bControlStyleSheet\s*::\s*UsesPropertyCondition\s*\(\s*' +
        'Control\s*&[^;{]*DependencyPropertyChangedEventArgs\s*&)')
    Assert-SourceTextPatternCount -Path $productionStyleSource `
        -Pattern $styleFlavorSourcePattern -ExpectedCount 6 `
        -Description 'Production compiled style flavor overrides'
    Assert-SourceTextPatternCount -Path $runtimeStyleMutableSource `
        -Pattern $styleFlavorSourcePattern -ExpectedCount 6 `
        -Description 'Design mutable style flavor overrides'

    $visualStateDefinitionSourcePattern = ('\bControl\s*::\s*(?:' +
        'DefineVisualStateGroups|DefineDeclarativeInteractions)\s*\(')
    $visualStateStringGoToSourcePattern = ('(?s)\bControl\s*::\s*' +
        'GoToVisualState\s*\(\s*const\s+std\s*::\s*wstring\s*&')
    $visualStateStringGetSourcePattern = ('(?s)\bControl\s*::\s*' +
        'GetCurrentVisualState\s*\(\s*const\s+std\s*::\s*wstring\s*&')
    $visualStateResolverDefinitionPattern = ('(?s)\bbool\s+' +
        'ResolveVisualStateAnimationOperands\s*\([^;{]*\)\s*\{')
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern $visualStateDefinitionSourcePattern -ExpectedCount 0 `
        -Description 'Production/shared Control visual-state definition adapters'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern $visualStateStringGoToSourcePattern -ExpectedCount 0 `
        -Description 'Production/shared Control string GoToVisualState adapters'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern $visualStateStringGetSourcePattern -ExpectedCount 0 `
        -Description 'Production/shared Control string state query adapter'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern $visualStateResolverDefinitionPattern -ExpectedCount 0 `
        -Description 'Production/shared Control animation operand resolver'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern $visualStateDefinitionSourcePattern -ExpectedCount 2 `
        -Description 'Design visual-state definition adapters'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern $visualStateStringGoToSourcePattern -ExpectedCount 4 `
        -Description 'Design string GoToVisualState overloads'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern $visualStateStringGetSourcePattern -ExpectedCount 1 `
        -Description 'Design string visual-state query'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern $visualStateResolverDefinitionPattern -ExpectedCount 1 `
        -Description 'Design visual-state animation operand resolver'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern '\bControl\s*::\s*InstallDesignInteractionDefinitions\s*\(' `
        -ExpectedCount 1 -Description 'allowed DesignCore interaction bridge'
    Assert-SourceTextPatternCount -Path $productionControlSource `
        -Pattern '(?m)^\s*bool\s+TryBuildAnimation\s*\(' `
        -ExpectedCount 1 -Description 'allowed DesignCore animation build bridge'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern '\bInstallDesignInteractionDefinitions\s*\(' `
        -ExpectedCount 1 `
        -Description 'Design visual-state bridge invocation'
    Assert-SourceTextPatternCount -Path $runtimeVisualStateSource `
        -Pattern '(?m)^\s*bool\s+TryBuildAnimation\s*\(' `
        -ExpectedCount 0 -Description 'Design object excludes animation builder'

    $runtimeDeclarations = @(
        @{ Pattern = '^\s*struct\s+BindingValueConverterMetadata\b'; Name = 'single converter metadata declaration' }
        @{ Pattern = '^\s*struct\s+MultiBindingValueConverterMetadata\b'; Name = 'multi converter metadata declaration' }
        @{ Pattern = '^\s*class\s+BindingValueConverterRegistry\b'; Name = 'single converter registry declaration' }
        @{ Pattern = '^\s*class\s+MultiBindingValueConverterRegistry\b'; Name = 'multi converter registry declaration' }
    )
    foreach ($declaration in $runtimeDeclarations) {
        Assert-SourcePatternPresent -Path $runtimeHeader `
            -Pattern $declaration.Pattern `
            -Description ("Design header {0}" -f $declaration.Name)
    }

    Assert-SourcePatternPresent -Path $runtimeSource `
        -Pattern '^\s*#\s*include\s*[<"][^>"]*BindingConverterRegistry\.h[>"]' `
        -Description 'Design source includes its Runtime-only registry header'

    $registryMethods = @('Register', 'Unregister', 'Find', 'GetConverters', 'Create')
    foreach ($registryName in $registryNames) {
        foreach ($methodName in $registryMethods) {
            $methodPattern = ('\b{0}\s*::\s*{1}\s*\(' -f `
                [regex]::Escape($registryName), [regex]::Escape($methodName))
            Assert-SourcePatternPresent -Path $runtimeSource `
                -Pattern $methodPattern `
                -Description ("Design source {0}::{1}" -f `
                    $registryName, $methodName)
        }
    }

    $runtimeStoragePatterns = @(
        @{ Pattern = '\bRegisteredBindingConverters\s*\('; Name = 'single converter registry storage' }
        @{ Pattern = '\bBindingConverterMutex\s*\('; Name = 'single converter registry mutex' }
        @{ Pattern = '\bRegisteredMultiBindingConverters\s*\('; Name = 'multi converter registry storage' }
        @{ Pattern = '\bMultiBindingConverterMutex\s*\('; Name = 'multi converter registry mutex' }
    )
    foreach ($storage in $runtimeStoragePatterns) {
        Assert-SourcePatternPresent -Path $runtimeSource `
            -Pattern $storage.Pattern `
            -Description ("Design source {0}" -f $storage.Name)
    }

    Write-Host ('[PASS] Source ownership: Production registry/name compatibility ' +
        'definitions=0; Design converter, dependency-property and object ' +
        'compatibility units own their implementations/storage')
}

function Get-LibraryRegistrySymbols {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Dumpbin,

        [Parameter(Mandatory = $true)]
        [string]$LibraryPath
    )

    $counts = [ordered]@{}
    $samples = @{}
    $symbolPatterns = @{}
    foreach ($registryName in $registryNames) {
        $counts[$registryName] = 0
        $samples[$registryName] = [System.Collections.Generic.List[string]]::new()
        # BindingValueConverterRegistry is a suffix of the multi-value class
        # name, so plain substring matching would let a multi-only archive
        # falsely satisfy the single-value positive check.
        $symbolPatterns[$registryName] = [regex]::new(
            ('(?<![A-Za-z0-9_]){0}(?![A-Za-z0-9_])' -f
                [regex]::Escape($registryName)),
            [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)
    }
    foreach ($boundaryName in @(
            $dependencyPropertyBoundaryName,
            $dependencyObjectBoundaryName,
            $controlDesignBoundaryName,
            $contentProjectionBoundaryName,
            $collectionAdaptersBoundaryName,
            $styleMutableBoundaryName,
            $visualStateBoundaryName)) {
        $counts[$boundaryName] = 0
        $samples[$boundaryName] =
            [System.Collections.Generic.List[string]]::new()
    }
    $dependencyObjectMethodCounts = [ordered]@{}
    $dependencyObjectMethodSamples = @{}
    foreach ($methodName in @($dependencyObjectNameMethodNames) +
            @($dependencyObjectAuxiliarySymbolPatterns.Keys)) {
        $dependencyObjectMethodCounts[$methodName] = 0
        $dependencyObjectMethodSamples[$methodName] =
            [System.Collections.Generic.List[string]]::new()
    }
    $controlDefinitionCounts = [ordered]@{}
    $controlDefinitionSamples = @{}
    foreach ($definition in $controlDesignSymbolDefinitions) {
        $controlDefinitionCounts[$definition.Name] = 0
        $controlDefinitionSamples[$definition.Name] =
            [System.Collections.Generic.List[string]]::new()
    }
    $controlRegistrarCount = 0
    $controlRegistrarSamples = [System.Collections.Generic.List[string]]::new()
    $definitionSets = [ordered]@{
        ContentProjectionMigrated = $contentProjectionMigratedDefinitions
        ContentProjectionHooks = $contentProjectionHookDefinitions
        ContentProjectionAuthoredAdapters =
            $contentProjectionAuthoredAdapterDefinitions
        ContentProjectionRegistrars = $contentProjectionRegistrarDefinitions
        CollectionAdapters = $collectionAdapterDefinitions
        CollectionRegistrarHooks = $collectionRegistrarHookDefinitions
        CollectionSharedRegistrars = $collectionSharedRegistrarDefinitions
        CollectionAuthoredAdapters = $collectionAuthoredAdapterDefinitions
        StyleMutableDesign = $styleMutableDesignDefinitions
        StyleFlavorOverrides = $styleFlavorOverrideDefinitions
        VisualStateDesign = $visualStateDesignDefinitions
        VisualStateBridges = $visualStateBridgeDefinitions
    }
    $definitionSetCounts = @{}
    $definitionSetSamples = @{}
    foreach ($setName in $definitionSets.Keys) {
        $definitionSetCounts[$setName] = [ordered]@{}
        $definitionSetSamples[$setName] = @{}
        foreach ($definition in $definitionSets[$setName]) {
            $definitionSetCounts[$setName][$definition.Name] = 0
            $definitionSetSamples[$setName][$definition.Name] =
                [System.Collections.Generic.List[string]]::new()
        }
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Dumpbin
    $startInfo.Arguments = ('/nologo /linkermember:2 "{0}"' -f $LibraryPath)
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $lineCount = 0L
    try {
        if (-not $process.Start()) {
            throw "dumpbin.exe failed to start for $LibraryPath"
        }
        $stderrTask = $process.StandardError.ReadToEndAsync()
        while (($line = $process.StandardOutput.ReadLine()) -ne $null) {
            ++$lineCount
            foreach ($registryName in $registryNames) {
                if (-not $symbolPatterns[$registryName].IsMatch($line)) {
                    continue
                }
                $counts[$registryName] = [int]$counts[$registryName] + 1
                if ($samples[$registryName].Count -lt 3) {
                    $samples[$registryName].Add($line.Trim())
                }
            }
            foreach ($entry in @(
                    @{ Name = $dependencyPropertyBoundaryName;
                       Patterns = $dependencyPropertySymbolPatterns },
                    @{ Name = $dependencyObjectBoundaryName;
                       Patterns = $dependencyObjectSymbolPatterns },
                    @{ Name = $controlDesignBoundaryName;
                       Patterns = $controlDesignSymbolPatterns },
                    @{ Name = $contentProjectionBoundaryName;
                       Patterns = $contentProjectionMigratedSymbolPatterns },
                    @{ Name = $collectionAdaptersBoundaryName;
                       Patterns = $collectionAdaptersSymbolPatterns },
                    @{ Name = $styleMutableBoundaryName;
                       Patterns = $styleMutableSymbolPatterns },
                    @{ Name = $visualStateBoundaryName;
                       Patterns = $visualStateSymbolPatterns })) {
                $matched = $false
                foreach ($pattern in $entry.Patterns) {
                    if ($line -match $pattern) {
                        $matched = $true
                        break
                    }
                }
                if (-not $matched) {
                    continue
                }
                $counts[$entry.Name] = [int]$counts[$entry.Name] + 1
                if ($samples[$entry.Name].Count -lt 3) {
                    $samples[$entry.Name].Add($line.Trim())
                }
            }
            foreach ($entry in $dependencyObjectMethodSymbolPatterns.GetEnumerator()) {
                if ($line -notmatch $entry.Value) {
                    continue
                }
                $dependencyObjectMethodCounts[$entry.Key] =
                    [int]$dependencyObjectMethodCounts[$entry.Key] + 1
                if ($dependencyObjectMethodSamples[$entry.Key].Count -lt 3) {
                    $dependencyObjectMethodSamples[$entry.Key].Add($line.Trim())
                }
            }
            foreach ($entry in $dependencyObjectAuxiliarySymbolPatterns.GetEnumerator()) {
                if ($line -notmatch $entry.Value) {
                    continue
                }
                $dependencyObjectMethodCounts[$entry.Key] =
                    [int]$dependencyObjectMethodCounts[$entry.Key] + 1
                if ($dependencyObjectMethodSamples[$entry.Key].Count -lt 3) {
                    $dependencyObjectMethodSamples[$entry.Key].Add($line.Trim())
                }
            }
            foreach ($definition in $controlDesignSymbolDefinitions) {
                if ($line -notmatch $definition.Pattern) {
                    continue
                }
                $controlDefinitionCounts[$definition.Name] =
                    [int]$controlDefinitionCounts[$definition.Name] + 1
                if ($controlDefinitionSamples[$definition.Name].Count -lt 3) {
                    $controlDefinitionSamples[$definition.Name].Add($line.Trim())
                }
            }
            if ($line -match $controlDesignRegistrarPattern) {
                ++$controlRegistrarCount
                if ($controlRegistrarSamples.Count -lt 3) {
                    $controlRegistrarSamples.Add($line.Trim())
                }
            }
            foreach ($setName in $definitionSets.Keys) {
                foreach ($definition in $definitionSets[$setName]) {
                    if ($line -notmatch $definition.Pattern) {
                        continue
                    }
                    $definitionSetCounts[$setName][$definition.Name] =
                        [int]$definitionSetCounts[$setName][$definition.Name] + 1
                    if ($definitionSetSamples[$setName][$definition.Name].Count `
                        -lt 3) {
                        $definitionSetSamples[$setName][$definition.Name].Add(
                            $line.Trim())
                    }
                }
            }
        }
        $process.WaitForExit()
        $stderr = $stderrTask.GetAwaiter().GetResult().Trim()
        if ($process.ExitCode -ne 0) {
            throw ("dumpbin.exe exited $($process.ExitCode) for $LibraryPath. " +
                "stderr: $stderr")
        }
    }
    finally {
        $process.Dispose()
    }

    return [pscustomobject]@{
        Library = $LibraryPath
        Lines = $lineCount
        Counts = $counts
        Samples = $samples
        DependencyObjectMethodCounts = $dependencyObjectMethodCounts
        DependencyObjectMethodSamples = $dependencyObjectMethodSamples
        ControlDefinitionCounts = $controlDefinitionCounts
        ControlDefinitionSamples = $controlDefinitionSamples
        ControlRegistrarCount = $controlRegistrarCount
        ControlRegistrarSamples = $controlRegistrarSamples
        DefinitionSetCounts = $definitionSetCounts
        DefinitionSetSamples = $definitionSetSamples
    }
}

function Get-LibraryArchiveMembers {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Dumpbin,

        [Parameter(Mandatory = $true)]
        [string]$LibraryPath
    )

    $members = [System.Collections.Generic.List[string]]::new()
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Dumpbin
    $startInfo.Arguments = ('/nologo /archivemembers "{0}"' -f $LibraryPath)
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "dumpbin.exe failed to start for $LibraryPath"
        }
        $stderrTask = $process.StandardError.ReadToEndAsync()
        while (($line = $process.StandardOutput.ReadLine()) -ne $null) {
            if ($line -notmatch
                '^\s*Archive member name at [^:]+:\s*(.+?)\s*$') {
                continue
            }
            $memberText = $Matches[1].Trim()
            # For long COFF member names dumpbin prints the string-table
            # offset first, followed by the resolved path.
            $memberText = $memberText -replace '^/\d+\s+', ''
            if (-not $memberText -or $memberText -match '^/\d+$') {
                throw ("A8f archive boundary could not resolve member name: " +
                    $line.Trim())
            }
            $normalizedPath = $memberText.Replace('/', '\')
            $members.Add([System.IO.Path]::GetFileName($normalizedPath))
        }
        $process.WaitForExit()
        $stderr = $stderrTask.GetAwaiter().GetResult().Trim()
        if ($process.ExitCode -ne 0) {
            throw ("dumpbin.exe exited $($process.ExitCode) for $LibraryPath. " +
                "stderr: $stderr")
        }
    }
    finally {
        $process.Dispose()
    }

    if ($members.Count -eq 0) {
        throw ("A8f archive boundary found no members in dumpbin output for " +
            $LibraryPath)
    }
    return [pscustomobject]@{
        Library = $LibraryPath
        Members = $members
    }
}

function Test-ExcludedArchiveBoundary {
    param(
        [object]$Result,
        [string]$LibraryRole
    )

    $forbidden = @($Result.Members | Where-Object {
        $_ -match '(?i)\.Design\.obj$' -or
        $_ -match '(?i)^XamlSchema(?:\.Design)?\.obj$'
    } | Sort-Object -Unique)
    if ($forbidden.Count -ne 0) {
        throw ("A8f archive boundary failed: $LibraryRole contains forbidden " +
            "Design/XamlSchema archive member(s): " + ($forbidden -join ', '))
    }
    Write-Host (("[PASS] {0} archive excludes *.Design.obj and XamlSchema: " +
        "members={1}") -f $LibraryRole, $Result.Members.Count)
}

function Test-DesignArchiveBoundary {
    param([object]$Result)

    foreach ($requiredMember in $requiredDesignArchiveMembers) {
        if (@($Result.Members) -icontains $requiredMember) {
            continue
        }
        throw ("A8f archive boundary failed: Design CuiRuntime.lib does not " +
            "own required archive member $requiredMember")
    }
    Write-Host (("[PASS] Design CuiRuntime.lib owns all {0} required A8f " +
        "archive members") -f $requiredDesignArchiveMembers.Count)
}

function Test-FinalMapBoundary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$MapPath,

        [Parameter(Mandatory = $true)]
        [string]$MapRole,

        [Parameter(Mandatory = $true)]
        [int]$MaximumCuiMembers
    )

    $resolvedMap = Resolve-LeafPath -Path $MapPath `
        -Description "$MapRole linker MAP"
    $mapText = [System.IO.File]::ReadAllText($resolvedMap)
    $regexOptions =
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant -bor
        [System.Text.RegularExpressions.RegexOptions]::Multiline
    $memberMatches = [regex]::Matches(
        $mapText,
        '(?<![A-Za-z0-9_])CUI:([^\s]+\.obj)(?=\s|$)',
        $regexOptions)
    $members = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($match in $memberMatches) {
        [void]$members.Add($match.Groups[1].Value)
    }
    if ($members.Count -eq 0) {
        throw ("A8f MAP boundary failed: $MapRole contains no parseable " +
            'CUI:<member>.obj ownership records.')
    }
    if ($members.Count -gt $MaximumCuiMembers) {
        $memberList = @($members | Sort-Object) -join ', '
        throw ("A8f MAP boundary failed: $MapRole links $($members.Count) " +
            "unique CUI object members; maximum is $MaximumCuiMembers." +
            [Environment]::NewLine + $memberList)
    }

    foreach ($check in $mapForbiddenChecks) {
        $matches = [regex]::Matches(
            $mapText, $check.Pattern, $regexOptions)
        if ($matches.Count -eq 0) {
            continue
        }
        $samples = @($matches | Select-Object -First 4 | ForEach-Object {
            $_.Value
        } | Sort-Object -Unique) -join ', '
        throw ("A8f MAP boundary failed: $MapRole contains " +
            "$($matches.Count) forbidden $($check.Name) trace(s): $samples")
    }

    Write-Host (("[PASS] {0} MAP: unique CUI members={1}/{2}; " +
        "Design/XML/name-registry traces=0") -f `
        $MapRole, $members.Count, $MaximumCuiMembers)
}

function Format-LibraryStats {
    param([object]$Result)

    $parts = @("lines=$($Result.Lines)")
    foreach ($boundaryName in $physicalBoundaryNames) {
        $parts += ("{0}={1}" -f $boundaryName,
            $Result.Counts[$boundaryName])
    }
    return $parts -join ', '
}

function Test-ExcludedLibraryBoundary {
    param(
        [object]$Result,
        [string]$LibraryRole
    )

    foreach ($boundaryName in $physicalBoundaryNames) {
        $count = [int]$Result.Counts[$boundaryName]
        if ($count -eq 0) {
            continue
        }
        $sample = $Result.Samples[$boundaryName] -join [Environment]::NewLine
        throw ("A8f binary boundary failed: $LibraryRole contains " +
            "$count $boundaryName symbol line(s)." +
            [Environment]::NewLine + $sample)
    }
    Write-Host ("[PASS] {0} excludes A8f Design symbols: {1}" -f `
        $LibraryRole, (Format-LibraryStats -Result $Result))
}

function Test-ControlRegistrarBoundary {
    param(
        [object]$Result,
        [string]$LibraryRole,
        [int]$ExpectedCount
    )

    $count = [int]$Result.ControlRegistrarCount
    if ($count -ne $ExpectedCount) {
        $sample = $Result.ControlRegistrarSamples -join [Environment]::NewLine
        throw ("A8f binary boundary failed: $LibraryRole Control registrar " +
            "definitions=$count, expected=$ExpectedCount." +
            [Environment]::NewLine + $sample)
    }
    Write-Host ("[PASS] {0} Control registrar definitions={1}/{1}" -f `
        $LibraryRole, $ExpectedCount)
}

function Test-DefinitionSetBoundary {
    param(
        [object]$Result,
        [string]$SetName,
        [object[]]$Definitions,
        [string]$LibraryRole,
        [int]$ExpectedCount
    )

    $positiveTotal = 0
    foreach ($definition in $Definitions) {
        $positiveCount = 1
        if ($definition.PSObject.Properties.Name -contains 'PositiveCount') {
            $positiveCount = [int]$definition.PositiveCount
        }
        if ($positiveCount -lt 1) {
            throw ("A8f gate definition $($definition.Name) has invalid " +
                "PositiveCount=$positiveCount.")
        }
        $positiveTotal += $positiveCount
        $definitionExpectedCount = $ExpectedCount * $positiveCount
        $count = [int]$Result.DefinitionSetCounts[$SetName][$definition.Name]
        if ($count -eq $definitionExpectedCount) {
            continue
        }
        $sample = $Result.DefinitionSetSamples[$SetName][$definition.Name] -join `
            [Environment]::NewLine
        throw ("A8f binary boundary failed: $LibraryRole " +
            "$($definition.Name) definitions=$count, " +
            "expected=$definitionExpectedCount." +
            [Environment]::NewLine + $sample)
    }
    Write-Host ("[PASS] {0} {1}: symbols={2}, ownership={3}" -f `
        $LibraryRole, $SetName, ($positiveTotal * $ExpectedCount),
        $ExpectedCount)
}

function Test-DesignLibraryBoundary {
    param([object]$Result)

    foreach ($boundaryName in $physicalBoundaryNames) {
        $count = [int]$Result.Counts[$boundaryName]
        if ($count -ne 0) {
            continue
        }
        throw ("A8f binary boundary failed: Design CuiRuntime.lib contains no " +
            "$boundaryName symbol in dumpbin /linkermember:2 output.")
    }
    foreach ($methodName in $Result.DependencyObjectMethodCounts.Keys) {
        $count = [int]$Result.DependencyObjectMethodCounts[$methodName]
        if ($count -ne 0) {
            continue
        }
        throw ("A8f binary boundary failed: Design CuiRuntime.lib contains no " +
            "DependencyObject::$methodName Design compatibility definition.")
    }
    $categoryCounts = [ordered]@{}
    foreach ($categoryName in $controlDesignCategoryExpectedCounts.Keys) {
        $categoryCounts[$categoryName] = 0
    }
    foreach ($definition in $controlDesignSymbolDefinitions) {
        $count = [int]$Result.ControlDefinitionCounts[$definition.Name]
        if ($count -ne 1) {
            $sample = $Result.ControlDefinitionSamples[$definition.Name] -join `
                [Environment]::NewLine
            throw ("A8f binary boundary failed: Design CuiRuntime.lib " +
                "Control $($definition.Name) definitions=$count, expected=1." +
                [Environment]::NewLine + $sample)
        }
        $categoryCounts[$definition.Category] =
            [int]$categoryCounts[$definition.Category] + $count
    }
    foreach ($categoryName in $controlDesignCategoryExpectedCounts.Keys) {
        $expected = [int]$controlDesignCategoryExpectedCounts[$categoryName]
        $actual = [int]$categoryCounts[$categoryName]
        if ($actual -ne $expected) {
            throw ("A8f binary boundary failed: Design CuiRuntime.lib " +
                "Control $categoryName definitions=$actual, expected=$expected.")
        }
    }
    Test-ControlRegistrarBoundary -Result $Result `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'ContentProjectionMigrated' `
        -Definitions $contentProjectionMigratedDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'ContentProjectionHooks' `
        -Definitions $contentProjectionHookDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 0
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'ContentProjectionAuthoredAdapters' `
        -Definitions $contentProjectionAuthoredAdapterDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'ContentProjectionRegistrars' `
        -Definitions $contentProjectionRegistrarDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'CollectionAdapters' `
        -Definitions $collectionAdapterDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'CollectionRegistrarHooks' `
        -Definitions $collectionRegistrarHookDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'CollectionSharedRegistrars' `
        -Definitions $collectionSharedRegistrarDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 0
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'CollectionAuthoredAdapters' `
        -Definitions $collectionAuthoredAdapterDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'StyleMutableDesign' `
        -Definitions $styleMutableDesignDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'StyleFlavorOverrides' `
        -Definitions $styleFlavorOverrideDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'VisualStateDesign' `
        -Definitions $visualStateDesignDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 1
    Test-DefinitionSetBoundary -Result $Result `
        -SetName 'VisualStateBridges' `
        -Definitions $visualStateBridgeDefinitions `
        -LibraryRole 'Design CuiRuntime.lib' -ExpectedCount 0
    Write-Host ("[PASS] Design CuiRuntime.lib owns physical compatibility units: {0}" -f `
        (Format-LibraryStats -Result $Result))
    Write-Host (("[PASS] Design CuiRuntime.lib owns all {0} DependencyObject " +
        "name/enumeration member definitions") -f `
        $Result.DependencyObjectMethodCounts.Count)
    Write-Host ('[PASS] Design CuiRuntime.lib owns Control definitions: ' +
        'descriptor/property-bag=5, wstring scope/presenter=6, ' +
        'wstring event=2, behavior=4, Design registrar=1')
    Write-Host ('[PASS] Design CuiRuntime.lib owns content projection: ' +
        'migrated=13, authored adapters=5, compiled hooks=0, registrars=5; ' +
        'collection direct=11, registrar hooks=4, authored adapters=29, ' +
        'shared registrars=0')
    Write-Host ('[PASS] Design CuiRuntime.lib owns final backends: ' +
        'style Design-only=39, style flavor overrides=6, ' +
        'visual-state name/definition=7, operand resolver=1, core bridges=0')
}

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..'))
Write-Host 'CUI A8f complete physical-boundary gate'
Write-Host "Repository: $repositoryRoot"

Test-SourceBoundary -RepositoryRoot $repositoryRoot

$hasClosureMap = -not [string]::IsNullOrWhiteSpace($ClosureMap)
$hasFullMap = -not [string]::IsNullOrWhiteSpace($FullMap)
if ($hasClosureMap -xor $hasFullMap) {
    throw 'A8f MAP boundary requires -ClosureMap and -FullMap together.'
}

if ($SourceOnly) {
    Write-Host '[PASS] Source-only mode requested; binary symbol/archive checks skipped.'
    Write-Host 'CUI A8f physical-boundary source gate passed.'
    exit 0
}

if (-not $ProductionCuiLib -or -not $DesignCuiCoreLib -or
    -not $DesignCuiRuntimeLib) {
    throw ('Binary mode requires -ProductionCuiLib, -DesignCuiCoreLib and ' +
        '-DesignCuiRuntimeLib. ' +
        'Use -SourceOnly only when migrated libraries have not been built yet.')
}

$resolvedProductionLibrary = Resolve-LeafPath -Path $ProductionCuiLib `
    -Description 'Production CUI.lib'
$resolvedDesignLibrary = Resolve-LeafPath -Path $DesignCuiRuntimeLib `
    -Description 'Design CuiRuntime.lib'
$resolvedDesignCoreLibrary = Resolve-LeafPath -Path $DesignCuiCoreLib `
    -Description 'Design CUIDesignCore.lib'
$resolvedDumpbin = Resolve-DumpbinPath -ExplicitPath $DumpbinPath

Write-Host "dumpbin: $resolvedDumpbin"
$productionResult = Get-LibraryRegistrySymbols -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedProductionLibrary
Test-ExcludedLibraryBoundary -Result $productionResult `
    -LibraryRole 'Production CUI.lib'
Test-ControlRegistrarBoundary -Result $productionResult `
    -LibraryRole 'Production CUI.lib (allowed empty stub)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'ContentProjectionMigrated' `
    -Definitions $contentProjectionMigratedDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'ContentProjectionHooks' `
    -Definitions $contentProjectionHookDefinitions `
    -LibraryRole 'Production CUI.lib (compiled hooks)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'ContentProjectionAuthoredAdapters' `
    -Definitions $contentProjectionAuthoredAdapterDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'ContentProjectionRegistrars' `
    -Definitions $contentProjectionRegistrarDefinitions `
    -LibraryRole 'Production CUI.lib (base registrars)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'CollectionAdapters' `
    -Definitions $collectionAdapterDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'CollectionRegistrarHooks' `
    -Definitions $collectionRegistrarHookDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'CollectionSharedRegistrars' `
    -Definitions $collectionSharedRegistrarDefinitions `
    -LibraryRole 'Production CUI.lib (shared registrars)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'CollectionAuthoredAdapters' `
    -Definitions $collectionAuthoredAdapterDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'StyleMutableDesign' `
    -Definitions $styleMutableDesignDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'StyleFlavorOverrides' `
    -Definitions $styleFlavorOverrideDefinitions `
    -LibraryRole 'Production CUI.lib (compiled overrides)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'VisualStateDesign' `
    -Definitions $visualStateDesignDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $productionResult `
    -SetName 'VisualStateBridges' `
    -Definitions $visualStateBridgeDefinitions `
    -LibraryRole 'Production CUI.lib' -ExpectedCount 0

$designCoreResult = Get-LibraryRegistrySymbols -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedDesignCoreLibrary
Test-ExcludedLibraryBoundary -Result $designCoreResult `
    -LibraryRole 'Design CUIDesignCore.lib'
Test-ControlRegistrarBoundary -Result $designCoreResult `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'ContentProjectionMigrated' `
    -Definitions $contentProjectionMigratedDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'ContentProjectionHooks' `
    -Definitions $contentProjectionHookDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib (compiled hooks)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'ContentProjectionAuthoredAdapters' `
    -Definitions $contentProjectionAuthoredAdapterDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'ContentProjectionRegistrars' `
    -Definitions $contentProjectionRegistrarDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'CollectionAdapters' `
    -Definitions $collectionAdapterDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'CollectionRegistrarHooks' `
    -Definitions $collectionRegistrarHookDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'CollectionSharedRegistrars' `
    -Definitions $collectionSharedRegistrarDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib (shared registrars)' -ExpectedCount 1
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'CollectionAuthoredAdapters' `
    -Definitions $collectionAuthoredAdapterDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'StyleMutableDesign' `
    -Definitions $styleMutableDesignDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'StyleFlavorOverrides' `
    -Definitions $styleFlavorOverrideDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'VisualStateDesign' `
    -Definitions $visualStateDesignDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib' -ExpectedCount 0
Test-DefinitionSetBoundary -Result $designCoreResult `
    -SetName 'VisualStateBridges' `
    -Definitions $visualStateBridgeDefinitions `
    -LibraryRole 'Design CUIDesignCore.lib (narrow bridges)' -ExpectedCount 1

$designResult = Get-LibraryRegistrySymbols -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedDesignLibrary
Test-DesignLibraryBoundary -Result $designResult

$productionArchive = Get-LibraryArchiveMembers -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedProductionLibrary
Test-ExcludedArchiveBoundary -Result $productionArchive `
    -LibraryRole 'Production CUI.lib'

$designCoreArchive = Get-LibraryArchiveMembers -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedDesignCoreLibrary
Test-ExcludedArchiveBoundary -Result $designCoreArchive `
    -LibraryRole 'Design CUIDesignCore.lib'

$designArchive = Get-LibraryArchiveMembers -Dumpbin $resolvedDumpbin `
    -LibraryPath $resolvedDesignLibrary
Test-DesignArchiveBoundary -Result $designArchive

if ($hasClosureMap) {
    Test-FinalMapBoundary -MapPath $ClosureMap `
        -MapRole 'Closure' -MaximumCuiMembers 39
    Test-FinalMapBoundary -MapPath $FullMap `
        -MapRole 'Full' -MaximumCuiMembers 72
}

Write-Host 'CUI A8f physical-boundary gate passed.'

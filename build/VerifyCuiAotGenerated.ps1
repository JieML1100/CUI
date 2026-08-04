[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GeneratedBase,

    [ValidateSet('Document', 'Theme')]
    [string]$ArtifactKind = 'Document',

    # Optional semicolon-separated handwritten production sources. Generated
    # output is checked below; these files close the other half of the
    # boundary so an application cannot reintroduce dynamic XAML through its
    # code-behind.
    [string]$SourceFiles = '',

    [switch]$AllowKnownBlockers
)

$ErrorActionPreference = 'Stop'

$resolvedBase = [System.IO.Path]::GetFullPath($GeneratedBase)
if ($ArtifactKind -eq 'Theme') {
    $generatedFiles = @(
        "$resolvedBase.g.h"
        "$resolvedBase.g.cpp"
        "$resolvedBase.program.g.h"
        "$resolvedBase.program.g.cpp"
    )
    $generatedCpp = "$resolvedBase.program.g.cpp"
    $generatedCppFiles = @(
        "$resolvedBase.g.cpp"
        "$resolvedBase.program.g.cpp"
    )
} else {
    $generatedFiles = @(
        "$resolvedBase.g.h"
        "$resolvedBase.g.cpp"
        "$resolvedBase.handlers.g.inc"
    )
    $generatedCpp = "$resolvedBase.g.cpp"
    $generatedCppFiles = @($generatedCpp)
}

$missingFiles = @($generatedFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missingFiles.Count -ne 0) {
    throw "CUI AOT gate is missing generated output: $($missingFiles -join ', ')"
}

if ($ArtifactKind -eq 'Theme') {
    $legacyCarrierFiles = @(
        "$resolvedBase.carrier.g.h"
        "$resolvedBase.carrier.g.cpp"
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    if ($legacyCarrierFiles.Count -ne 0) {
        throw "CUI ThemeProgram output still has legacy Control carrier artifacts: $($legacyCarrierFiles -join ', ')"
    }
}

# Keep this list about build/runtime dependencies rather than user-facing text.
# For example, a literal that happens to mention "RuntimeDocumentSession" is not
# itself a dependency; an include or namespace-qualified use is.
$forbiddenPatterns = @(
    [pscustomobject]@{
        Name = 'dynamic framework/runtime header'
        Regex = '^\s*#\s*include\s*[<"][^">]*(?:CuiRuntime|XamlFrameworkTheme|XamlInfrastructure|RuntimeDocument|XamlDocument|XamlObjectMaterializer|XmlLite)[^">]*[>"]'
    }
    [pscustomobject]@{
        Name = 'dynamic framework/runtime namespace'
        Regex = '\b(?:CuiRuntime|DesignerModel|XmlLite)\s*::'
    }
    [pscustomobject]@{
        Name = 'dynamic event-registry bridge'
        Regex = '\bRegisterDeclarativeEventHandlers\s*\('
    }
    [pscustomobject]@{
        Name = 'dynamic document-reference bridge'
        Regex = '^\s*class\s+\w+References\s+final\s*$'
    }
    [pscustomobject]@{
        Name = 'dynamic XAML mutation bridge'
        Regex = '\bcui::framework::XamlAccess\s*::'
    }
    [pscustomobject]@{
        Name = 'runtime declarative type construction'
        Regex = '\bDeclarativeTypeDescriptor\s*::\s*Create\s*\('
    }
    [pscustomobject]@{
        Name = 'name-bearing component type identity in generated production code'
        Regex = '(?:\bRuntimeTypeId\s*(?:\{|[&*]|[A-Za-z_]\w*\s*[;=({,)]|::)|(?:<|,)\s*RuntimeTypeId\s*[,>])'
    }
    [pscustomobject]@{
        Name = 'component content presenter registered by property name'
        Regex = '\b(?:RegisterComponentContentPresenter|FindDeclarativeContentPresenter)\s*\('
    }
    [pscustomobject]@{
        Name = 'design identity mutation'
        Regex = '\bDesignIdentityAccess\s*::\s*Set\s*\('
    }
    [pscustomobject]@{
        Name = 'template factory reapplies Theme to templated owner'
        Regex = '::Apply\s*\(\s*__templateOwner\s*,\s*true\s*,\s*&__templateThemeError'
    }
    [pscustomobject]@{
        Name = 'template factory reapplies document Style to templated owner'
        Regex = 'SetDocumentStyles\s*\(\s*__templateOwner\s*,'
    }
    [pscustomobject]@{
        Name = 'template factory redundantly installs a Style environment'
        Regex = 'SetEnvironment\s*\(\s*\*__cuiStaticTemplateOwner'
    }
    [pscustomobject]@{
        Name = 'compiled component property enumeration table'
        Regex = '\bGetCompiledComponentPropertiesCore\s*\('
    }
    [pscustomobject]@{
        Name = 'compiled component metadata name scan'
        Regex = '\bcandidate\s*->\s*Name\s*\(\s*\)\s*==\s*propertyName\b'
    }
)

$violations = [System.Collections.Generic.List[object]]::new()
$productionStylePatterns = @(
    [pscustomobject]@{
        Name = 'production code constructs a mutable Style sheet'
        Regex = '\bstd\s*::\s*make_shared\s*<\s*ControlStyleSheet\s*>'
    }
    [pscustomobject]@{
        Name = 'production code mutates Style structure after construction'
        Regex = '(?:->|\.)\s*(?:AddRule|RemoveRule|ClearRules|SetResource|RemoveResource|ClearResources)\s*\('
    }
    [pscustomobject]@{
        Name = 'production code rebuilds mutable Style rule objects'
        Regex = '\b(?:ControlStyleRule|ControlStyleSelector|ControlStyleSetter)\b'
    }
    [pscustomobject]@{
        Name = 'production code embeds the Design owned Style builder'
        Regex = '\bCompiledStyleProgram\s*\{'
    }
    [pscustomobject]@{
        Name = 'production code embeds an owned declarative animation graph'
        Regex = '\bDeclarativeVisualStateAnimation\b'
    }
    [pscustomobject]@{
        Name = 'production code embeds an owned declarative action graph'
        Regex = '\bDeclarativeEventTriggerActionDefinition\b'
    }
    [pscustomobject]@{
        Name = 'production code assigns a named storyboard reference'
        Regex = '\.\s*StoryboardName\s*='
    }
    [pscustomobject]@{
        Name = 'production code assigns a parsed storyboard object path'
        Regex = '\.\s*ObjectPath\s*='
    }
)
foreach ($file in $generatedCppFiles) {
    foreach ($pattern in $productionStylePatterns) {
        foreach ($match in Select-String -LiteralPath $file -Pattern $pattern.Regex) {
            $violations.Add([pscustomobject]@{
                File = $file
                Line = $match.LineNumber
                Kind = $pattern.Name
                Text = $match.Line.Trim()
            })
        }
    }
}
if ($ArtifactKind -eq 'Theme') {
    $themeProgramPatterns = @(
        [pscustomobject]@{
            Name = 'legacy framework-theme carrier ABI'
            Regex = '\b(?:FrameworkThemeCarrier|CompiledThemeStyleSheet)\b'
        }
        [pscustomobject]@{
            Name = 'ThemeProgram captures a synthetic Control owner'
            Regex = 'SetApplyCallback\s*\(\s*\[\s*this(?:\s*,|\s*\])'
        }
        [pscustomobject]@{
            Name = 'ThemeProgram dereferences a synthetic Control owner'
            Regex = '(?:\bthis\s*->|\*\s*this\b)'
        }
        [pscustomobject]@{
            Name = 'legacy nested compiled Style payload'
            Regex = '\bCompiledStyleBuild(?:Payload|Resource)\b'
        }
        [pscustomobject]@{
            Name = 'ThemeProgram incrementally mutates a structural Style pool'
            Regex = '\b__styleProgram\s*\.\s*(?:Resources|Rules|PropertyConditions|DataConditions|Setters|Groups|RuleIndexes|PropertyWatchers|DataPathWatchers|GlobalPropertyWatchers|GlobalDataPathWatchers)\s*\.\s*(?:reserve|push_back|emplace_back)\s*\('
        }
    )
    foreach ($pattern in $themeProgramPatterns) {
        foreach ($match in Select-String -LiteralPath $generatedCpp -Pattern $pattern.Regex) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = $match.LineNumber
                Kind = $pattern.Name
                Text = $match.Line.Trim()
            })
        }
    }
    $themeProgramText = [System.IO.File]::ReadAllText($generatedCpp)
    foreach ($requirement in @(
        [pscustomobject]@{
            Name = 'ThemeProgram does not emit the flat Style program contract'
            Regex = '\bCompiledStyleProgramView\b'
        }
        [pscustomobject]@{
            Name = 'ThemeProgram does not publish an immutable compiled sheet'
            Regex = '\bControlStyleSheet\s*::\s*CreateCompiled\s*\('
        }
        [pscustomobject]@{
            Name = 'ThemeProgram does not publish the typed Style value view'
            Regex = '//\s*ValuePools\b'
        }
    )) {
        if (-not [regex]::IsMatch($themeProgramText, $requirement.Regex)) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = $requirement.Name
                Text = 'required generated construct is absent'
            })
        }
    }

}
$compiledInteractionProgramTotal = 0
foreach ($file in $generatedFiles) {
    foreach ($pattern in $forbiddenPatterns) {
        foreach ($match in Select-String -LiteralPath $file -Pattern $pattern.Regex) {
            $violations.Add([pscustomobject]@{
                File = $file
                Line = $match.LineNumber
                Kind = $pattern.Name
                Text = $match.Line.Trim()
            })
        }
    }

    # A production artifact must bind every generated property access to a
    # DependencyProperty identity or a typed C++ API.  Scanning the complete
    # file also catches a formatted call whose first string argument starts on
    # the following line.
    $generatedText = [System.IO.File]::ReadAllText($file)

    # A v4 Style view must publish every flat storyboard field and its
    # immutable DataTrigger path view explicitly.
    # Comparing counts protects aggregate initializers when more than one
    # document/local resource scope is emitted into the same translation unit.
    $compiledStyleViewCount = [regex]::Matches(
        $generatedText, '\bCompiledStyleProgramView\s*\{').Count
    $compiledStyleViews = [regex]::Matches(
        $generatedText,
        '(?s)\bCompiledStyleProgramView\s*\{(?<Body>.*?)\r?\n\s*\},\s*\r?\n\s*std\s*::\s*vector\s*<\s*BindingValue\s*>')
    if ($compiledStyleViews.Count -ne $compiledStyleViewCount) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated compiled Style view has an unrecognized initializer shape'
            Text = ("views={0}, parsed={1}" -f
                $compiledStyleViewCount, $compiledStyleViews.Count)
        })
    }
    foreach ($view in $compiledStyleViews) {
        foreach ($field in @(
            'PropertyOperands'
            'ObjectPathChildIndices'
            'ObjectPaths'
            'KeyFrames'
            'Animations'
            'Storyboards'
            'Actions'
            'DataPaths'
        )) {
            if (-not [regex]::IsMatch(
                $view.Groups['Body'].Value, ('//\s*{0}\b' -f $field))) {
                $violations.Add([pscustomobject]@{
                    File = $file
                    Line = 0
                    Kind = 'generated compiled Style view omits a flat storyboard field'
                    Text = $field
                })
            }
        }
    }

    # Every static DataTrigger pool must be backed by token-only compiled path
    # steps and a view table. Authored member names belong to the Design model,
    # not the Production Style string pool.
    foreach ($conditionTable in [regex]::Matches(
        $generatedText,
        '(?m)^\s*static\s+constexpr\s+CompiledStyleDataConditionOp\s+([A-Za-z_][A-Za-z0-9_]*)_program_data_conditions\s*\[\s*\]\s*=')) {
        $prefix = [regex]::Escape($conditionTable.Groups[1].Value)
        foreach ($required in @(
            [pscustomobject]@{
                Name = 'compiled path step table'
                Regex = ('\bstatic\s+constexpr\s+CompiledBindingPathStep\s+{0}_program_data_path_[0-9]+\s*\[' -f $prefix)
            }
            [pscustomobject]@{
                Name = 'compiled path view table'
                Regex = ('\bstatic\s+constexpr\s+CompiledBindingPathView\s+{0}_program_data_paths\s*\[' -f $prefix)
            }
        )) {
            if (-not [regex]::IsMatch($generatedText, $required.Regex)) {
                $violations.Add([pscustomobject]@{
                    File = $file
                    Line = 0
                    Kind = 'generated Style DataTrigger lacks a name-free path table'
                    Text = ("style={0}, missing={1}" -f
                        $conditionTable.Groups[1].Value, $required.Name)
                })
            }
        }
    }

    # An action-bearing Style scope must have a complete immutable numeric
    # execution graph. Object-path tables remain optional for direct DPs, but
    # string ObjectPath assignment is forbidden by the production patterns.
    foreach ($actionTable in [regex]::Matches(
        $generatedText,
        '(?m)^\s*static\s+constexpr\s+CompiledInteractionActionOp\s+([A-Za-z_][A-Za-z0-9_]*)_program_actions\s*\[\s*\]\s*=')) {
        $prefix = [regex]::Escape($actionTable.Groups[1].Value)
        foreach ($requiredTable in @(
            [pscustomobject]@{
                Type = 'CompiledInteractionPropertyOperand'
                Suffix = 'property_operands'
            }
            [pscustomobject]@{
                Type = 'CompiledInteractionAnimationOp'
                Suffix = 'animations'
            }
            [pscustomobject]@{
                Type = 'CompiledInteractionStoryboardOp'
                Suffix = 'storyboards'
            }
        )) {
            $requiredPattern = ('\bstatic\s+(?:constexpr|const)\s+{0}\s+{1}_program_{2}\s*\[' -f
                $requiredTable.Type, $prefix, $requiredTable.Suffix)
            if (-not [regex]::IsMatch($generatedText, $requiredPattern)) {
                $violations.Add([pscustomobject]@{
                    File = $file
                    Line = 0
                    Kind = 'generated Style action table lacks a flat backing table'
                    Text = ("style={0}, missing={1}" -f
                        $actionTable.Groups[1].Value,
                        $requiredTable.Suffix)
                })
            }
        }
    }

    foreach ($match in [regex]::Matches(
        $generatedText,
        '\bTry(?:Get|Set)(?:Current)?PropertyValue\s*\(\s*L"')) {
        $line = 1
        if ($match.Index -ne 0) {
            $line += ([regex]::Matches(
                $generatedText.Substring(0, $match.Index), "`n")).Count
        }
        $violations.Add([pscustomobject]@{
            File = $file
            Line = $line
            Kind = 'string property access in generated production code'
            Text = ([regex]::Replace($match.Value, '\s+', ' ')).Trim()
        })
    }
    foreach ($namedAccess in @(
        [pscustomobject]@{
            Kind = 'string DependencyPropertyAccess in generated production code'
            Regex = '\bDependencyPropertyAccess\s*::\s*(?:SetValue|SetBaseValue|SetDynamicResource|ClearValue|ClearDynamicResource)\s*\(\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string target-property query in generated production code'
            Regex = '\b(?:FindDependencyProperty|FindPropertyMetadata|ClearPropertyValue|HasPropertyValue|GetPropertyValueSource|GetPropertyExpressionKind|ResetPropertyValue|IsPropertyValueDefault|CoerceValue|TryGetDynamicResourceKey)\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Binding constructor target in generated production code'
            Regex = '\b(?:Binding|MultiBinding)\s*[\(\{]\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string ordinary Binding source path in generated production code'
            Regex = '\bDataBindings\s*\.\s*Add\s*\(\s*[^,\r\n]+,\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string MultiBinding child source path in generated production code'
            Regex = '\bMultiBindingSource(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string TryGetBindingPathValue path in generated production code'
            Regex = '\bTryGetBindingPathValue\s*\(\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string ObserveBindingPaths path in generated production code'
            Regex = '\bObserveBindingPaths\s*\(\s*[^,\r\n]+,\s*\{\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionViewSource binding path in generated production code'
            Regex = '\bSetSourceBindingPath\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionView member description in generated production code'
            Regex = '\bSet(?:Group|Sort|Filter)Descriptions\s*\(\s*\{\s*\{\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionView aggregate member description in generated production code'
            Regex = '\bSetAggregateDescriptions\s*\(\s*\{\s*\{\s*L"[^"\r\n]*"\s*,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string item projection member path in generated production code'
            Regex = '\bSet(?:DisplayMemberPath|SelectedValuePath|HeaderDisplayMemberPath)\s*\(\s*(?:std\s*::\s*wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'dynamic item projection DependencyProperty in generated production code'
            Regex = '\b(?:DisplayMemberPath|SelectedValuePath|HeaderDisplayMemberPath)Property\s*\('
        }
        [pscustomobject]@{
            Kind = 'runtime binding converter registry lookup in generated production code'
            Regex = '\b(?:BindingValueConverterRegistry|MultiBindingValueConverterRegistry)\s*::\s*Create\s*\('
        }
        [pscustomobject]@{
            Kind = 'string ObservableObject property definition in generated production code'
            Regex = '(?:->|\.)\s*DefineProperty\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'mutable ObservableObject record in generated production code'
            Regex = '\b(?:std\s*::\s*)?make_shared\s*<\s*ObservableObject\s*>'
        }
        [pscustomobject]@{
            Kind = 'mutable ObservableBindingList in generated production code'
            Regex = '\b(?:std\s*::\s*)?make_shared\s*<\s*ObservableBindingList\s*>'
        }
        [pscustomobject]@{
            Kind = 'runtime record schema construction in generated production code'
            Regex = '(?:->|\.)\s*DefineProperty\s*\('
        }
        [pscustomobject]@{
            Kind = 'runtime data type name dispatch in generated production code'
            Regex = '(?:->|\.)\s*(?:DataTypeName|ItemTypeName)\s*\('
        }
        [pscustomobject]@{
            Kind = 'name based implicit DataTemplate resolver in generated production code'
            Regex = '(?:->|\.)\s*SetImplicitItemTemplateResolver\s*\('
        }
        [pscustomobject]@{
            Kind = 'string DynamicResource property in generated production code'
            Regex = '(?:->|\.)SetDynamicResource\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Style Setter property in generated production code'
            Regex = '\bControlStyleSetter\s*(?:::\s*(?:Resource|DynamicResource))?\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Style Trigger condition in generated production code'
            Regex = '\b(?:Property|Data)Conditions\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string ControlStylePropertyCondition in generated production code'
            Regex = '\bControlStylePropertyCondition\s*[\(\{]\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'dynamic DependencyPropertyReference in generated production code'
            Regex = '\bDependencyPropertyReference(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'string VisualState condition property in generated production code'
            Regex = '\b(?:DeclarativeVisualStateCondition(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]|Conditions\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{?)\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'string VisualState setter property in generated production code'
            Regex = '\b(?:DeclarativeVisualStateSetter(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]|Setters\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{?)\s*[^,\r\n]+,\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'legacy string animation property in generated production code'
            Regex = '\banimation\s*\.\s*PropertyName\b'
        }
        [pscustomobject]@{
            Kind = 'legacy mutable interaction builder in generated production code'
            Regex = '\b(?:DeclarativeVisualStateCondition|DeclarativeVisualStateSetter|DeclarativeVisualStateDefinition|DeclarativeVisualTransitionDefinition|DeclarativeVisualStateGroupDefinition|DeclarativeEventTriggerDefinition)\b'
        }
        [pscustomobject]@{
            Kind = 'static instance-owned compiled interaction value pool'
            Regex = '\bstatic\s+const\s+BindingValue\s+__cuiInteraction_values\b'
        }
        [pscustomobject]@{
            Kind = 'string VisualState group or state identity in generated production code'
            Regex = '\b(?:group|state)\s*\.\s*Name\s*='
        }
        [pscustomobject]@{
            Kind = 'string VisualTransition selector in generated production code'
            Regex = '\btransition\s*\.\s*(?:FromState|ToState)\s*='
        }
        [pscustomobject]@{
            Kind = 'legacy mutable interaction installation in generated production code'
            Regex = '\b(?:cui\s*::\s*framework\s*::\s*)?TemplateAccess\s*::\s*DefineInteractions\s*\('
        }
        [pscustomobject]@{
            Kind = 'string TemplatePart registration in generated production code'
            Regex = '\bRegisterTemplatePart\s*\(\s*[^,]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string TemplatePart lookup in generated production code'
            Regex = '\bFindDeclarativeTemplatePart\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'deferred VisualState TargetName in generated production code'
            Regex = '\.\s*TargetName\b'
        }
        [pscustomobject]@{
            Kind = 'deferred VisualState event names in generated production code'
            Regex = '\.\s*EventNames?\b'
        }
        [pscustomobject]@{
            Kind = 'string declarative-event args identity in generated production code'
            Regex = '\b(?:args|e)\s*\.\s*(?:Name|OwnerType)\b'
        }
        [pscustomobject]@{
            Kind = 'string declarative-event raise in generated production code'
            Regex = '\bRaiseDeclarativeEvent\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'compiled component event name lookup in generated production code'
            Regex = '\bFindCompiledComponentEventCore\b'
        }
        [pscustomobject]@{
            Kind = 'foldable constexpr component event identity in generated production code'
            Regex = '\bstatic\s+constexpr\s+DeclarativeEventDefinition\b'
        }
        [pscustomobject]@{
            Kind = 'string Binding target in generated production code'
            Regex = '\bDataBindings\s*\.\s*Add\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string MultiBinding target in generated production code'
            Regex = '\bDataBindings\s*\.\s*AddMulti\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string TemplateBinding endpoint in generated production code'
            Regex = '\bDataBindings\s*\.\s*AddTemplateBinding\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string BindingCollection target management in generated production code'
            Regex = '\bDataBindings\s*\.\s*(?:Find|FindMulti|Remove|UpdateTarget|UpdateSource)\s*\(\s*L"'
        }
    )) {
        foreach ($match in [regex]::Matches(
            $generatedText, $namedAccess.Regex)) {
            $line = 1
            if ($match.Index -ne 0) {
                $line += ([regex]::Matches(
                    $generatedText.Substring(0, $match.Index), "`n")).Count
            }
            $violations.Add([pscustomobject]@{
                File = $file
                Line = $line
                Kind = $namedAccess.Kind
                Text = ([regex]::Replace($match.Value, '\s+', ' ')).Trim()
            })
        }
    }

    # Template names and VisualState targets are compile-time operands. Every
    # emitted registration must carry a non-zero numeric token, and every
    # emitted setter/animation must publish its already-resolved Control* (or
    # nullptr for the owner) rather than deferring a name lookup to runtime.
    $templatePartRegistrations = [regex]::Matches(
        $generatedText, '\bRegisterTemplatePart\s*\(\s*[^,]+,')
    $numericTemplatePartRegistrations = [regex]::Matches(
        $generatedText,
        '\bRegisterTemplatePart\s*\(\s*[^,]+,\s*TemplatePartToken\s*\{\s*[1-9][0-9]*ULL\s*\}\s*,')
    if ($templatePartRegistrations.Count -ne
        $numericTemplatePartRegistrations.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated template namescope does not use non-zero numeric tokens'
            Text = ("registrations={0}, tokenized={1}" -f
                $templatePartRegistrations.Count,
                $numericTemplatePartRegistrations.Count)
        })
    }

    $setterDeclarations = [regex]::Matches(
        $generatedText,
        '\bDeclarativeVisualStateSetter\s+setter\s*;')
    $setterTargets = [regex]::Matches(
        $generatedText, '\bsetter\s*\.\s*Target\s*=')
    if ($setterDeclarations.Count -ne $setterTargets.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated VisualState setter lacks a direct target'
            Text = ("setters={0}, direct-targets={1}" -f
                $setterDeclarations.Count, $setterTargets.Count)
        })
    }

    $eventTriggerDeclarations = [regex]::Matches(
        $generatedText,
        '\bDeclarativeEventTriggerDefinition\s+trigger\s*;')
    $typedEventTriggers = [regex]::Matches(
        $generatedText,
        '\btrigger\s*\.\s*(?:Event|RoutedEvent)\s*=')
    if ($eventTriggerDeclarations.Count -ne $typedEventTriggers.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated EventTrigger lacks a compiled event identity'
            Text = ("triggers={0}, typed-identities={1}" -f
                $eventTriggerDeclarations.Count, $typedEventTriggers.Count)
        })
    }

    $componentEventAccessors = [regex]::Matches(
        $generatedText,
        '\bconst\s+DeclarativeEventDefinition&\s+[A-Za-z_][A-Za-z0-9_:]*::[A-Za-z_][A-Za-z0-9_]*Event\(\)\s+noexcept')
    $componentEventDefinitions = [regex]::Matches(
        $generatedText,
        '\bstatic\s+DeclarativeEventDefinition\s+value\s*\(')
    if ($componentEventAccessors.Count -ne $componentEventDefinitions.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated component event accessor lacks stable identity storage'
            Text = ("accessors={0}, definitions={1}" -f
                $componentEventAccessors.Count,
                $componentEventDefinitions.Count)
        })
    }

    # Every immutable interaction program must live in process-static storage
    # and be paired with call-local value/target tables plus one strongly typed
    # installation call. This also rejects a program that is never consumed.
    $compiledInteractionPrograms = [regex]::Matches(
        $generatedText,
        '(?m)^\s*static\s+const\s+CompiledInteractionProgramView\s+__cuiInteractionProgram\s*\{')
    $compiledInteractionProgramTotal += $compiledInteractionPrograms.Count
    $compiledInteractionValueTables = [regex]::Matches(
        $generatedText,
        '(?m)^\s*const\s+BindingValue\s+__cuiInteraction_values\s*\[\s*\]\s*=')
    $compiledInteractionTargetTables = [regex]::Matches(
        $generatedText,
        '(?m)^\s*std\s*::\s*array\s*<\s*Control\s*\*\s*,\s*[1-9][0-9]*\s*>\s+__cuiInteractionTargets\s*\{')
    $compiledInteractionInstalls = [regex]::Matches(
        $generatedText,
        '\bTemplateAccess\s*::\s*InstallCompiledInteractions\s*\(')
    $typedCompiledInteractionInstalls = [regex]::Matches(
        $generatedText,
        '\bTemplateAccess\s*::\s*InstallCompiledInteractions\s*\(\s*[^,\r\n]+,\s*__cuiInteractionProgram\s*,\s*(?:\{\s*\}|std\s*::\s*span\s*<\s*const\s+BindingValue\s*>\s*\{\s*__cuiInteraction_values\s*\})\s*,\s*std\s*::\s*span\s*<\s*Control\s*\*\s*const\s*>\s*\{\s*__cuiInteractionTargets\s*\}\s*,')
    if ($compiledInteractionValueTables.Count -gt
            $compiledInteractionPrograms.Count -or
        $compiledInteractionPrograms.Count -ne
            $compiledInteractionTargetTables.Count -or
        $compiledInteractionPrograms.Count -ne
            $compiledInteractionInstalls.Count -or
        $compiledInteractionInstalls.Count -ne
            $typedCompiledInteractionInstalls.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated compiled interaction program is not installed through typed target slots'
            Text = ("programs={0}, values={1}, targets={2}, installs={3}, typed-installs={4}" -f
                $compiledInteractionPrograms.Count,
                $compiledInteractionValueTables.Count,
                $compiledInteractionTargetTables.Count,
                $compiledInteractionInstalls.Count,
                $typedCompiledInteractionInstalls.Count)
        })
    }

    $compiledInteractionBackingTables = [regex]::Matches(
        $generatedText,
        '\bstatic\s+(?:constexpr|const)\s+[^;\r\n]+\s+__cuiInteraction_[A-Za-z0-9_]+\s*\[\s*\]\s*=')
	$compiledInteractionLocalBackingTables = [regex]::Matches(
		$generatedText,
		'(?m)^\s*(?!static\s)(?:constexpr|const)\s+[^;\r\n]+\s+__cuiInteraction_(?!values\b)[A-Za-z0-9_]+\s*\[\s*\]\s*=')
	foreach ($match in $compiledInteractionLocalBackingTables) {
		$violations.Add([pscustomobject]@{
			File = $file
			Line = 0
			Kind = 'generated compiled interaction structural table has call-local lifetime'
			Text = $match.Value.Trim()
		})
	}
    if ($compiledInteractionPrograms.Count -ne 0 -and
        $compiledInteractionBackingTables.Count -lt
            $compiledInteractionPrograms.Count) {
        $violations.Add([pscustomobject]@{
            File = $file
            Line = 0
            Kind = 'generated compiled interaction program lacks static backing tables'
            Text = ("programs={0}, backing-tables={1}" -f
                $compiledInteractionPrograms.Count,
                $compiledInteractionBackingTables.Count)
        })
    }
}

# These checked-in gate fixtures intentionally author VSM/EventTrigger data.
# A zero-program translation is therefore a lowering omission, not a valid
# interaction-free document. Other consumer documents remain free to emit none.
$compiledInteractionFixtureNames = @(
    'DemoWindow'
    'NamespacedWindow'
    'CuiFrameworkTheme'
    'CornerRadiusVisualStateTheme'
)
if ($compiledInteractionFixtureNames -contains
        [System.IO.Path]::GetFileName($resolvedBase) -and
    $compiledInteractionProgramTotal -eq 0) {
    $violations.Add([pscustomobject]@{
        File = $generatedCpp
        Line = 0
        Kind = 'known interaction fixture emitted no compiled interaction program'
        Text = [System.IO.Path]::GetFileName($resolvedBase)
    })
}

if ($ArtifactKind -eq 'Document') {
    $generatedHeaderText = [System.IO.File]::ReadAllText("$resolvedBase.g.h")
    $generatedDocumentCppText = [System.IO.File]::ReadAllText($generatedCpp)
    # Detect the fixture independently from the ABI being verified. Otherwise
    # deleting the entire component-token hook would also disable this check.
    $compiledComponentFixtureNames = @('DemoWindow')
    $hasCompiledComponent =
        $compiledComponentFixtureNames -contains [System.IO.Path]::GetFileName($resolvedBase) -or
        [regex]::IsMatch(
            $generatedHeaderText,
            '\b(?:FindCompiledComponentPropertyCore|ComponentDefinition)\b')
    if ($hasCompiledComponent) {
        if (-not [regex]::IsMatch(
            $generatedHeaderText,
            '(?s)\bComponentTypeToken\s+GetCompiledComponentTypeTokenCore\s*\(')) {
            $violations.Add([pscustomobject]@{
                File = "$resolvedBase.g.h"
                Line = 0
                Kind = 'compiled component omits the component-type token ABI'
                Text = 'ComponentTypeToken GetCompiledComponentTypeTokenCore()'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '(?s)\bComponentTypeToken\s+[^\r\n{;]+::ComponentTypeId\s*\(\s*\)\s*noexcept\s*\{\s*return\s+ComponentTypeToken\s*\{\s*[1-9][0-9]*ULL\s*\}\s*;\s*\}')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component omits a non-zero component-type token'
                Text = 'ComponentTypeId() returns ComponentTypeToken{ non-zero ULL }'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '(?s)\bGetCompiledComponentTypeTokenCore\s*\(\s*\)\s*const\s*noexcept\s*\{\s*return\s+ComponentTypeId\s*\(\s*\)\s*;\s*\}')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component token hook does not return its static identity'
                Text = 'GetCompiledComponentTypeTokenCore() returns ComponentTypeId()'
            })
        }
        if ([regex]::IsMatch(
            $generatedHeaderText,
            '(?s)\bFindCompiledComponentPropertyCore\s*\(.{0,160}?\bstd\s*::\s*wstring\b')) {
            $violations.Add([pscustomobject]@{
                File = "$resolvedBase.g.h"
                Line = 0
                Kind = 'compiled component retains a property-name lookup ABI'
                Text = 'FindCompiledComponentPropertyCore(std::wstring)'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedHeaderText,
            '(?s)\bFindCompiledComponentPropertyCore\s*\(.{0,160}?\bComponentPropertyToken\b')) {
            $violations.Add([pscustomobject]@{
                File = "$resolvedBase.g.h"
                Line = 0
                Kind = 'compiled component omits the property-token lookup ABI'
                Text = 'ComponentPropertyToken'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '(?s)\bFindCompiledComponentPropertyCore\s*\(.{0,240}?switch\s*\(\s*property\.Value\s*\)')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component omits direct token dispatch'
                Text = 'switch (property.Value)'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '(?m)^\s*//\s*CUI:AOT\s+dependency-property\s*=\s*static\b')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component omits accessor-owned static dependency property storage'
                Text = '// CUI:AOT dependency-property=static'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '\bDependencyPropertyRegistry\s*::\s*(?:RegisterStatic|RegisterReadOnlyStatic)\s*<')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component omits static dependency property factory'
                Text = 'DependencyPropertyRegistry::RegisterStatic<...>'
            })
        }
        if (-not [regex]::IsMatch(
            $generatedDocumentCppText,
            '(?m)^\s*//\s*CUI:AOT\s+dependency-property-identity\s*=\s*token\b') -or
            -not [regex]::IsMatch(
                $generatedDocumentCppText,
                '\bBindingSourcePropertyToken\s*\{\s*[1-9][0-9]*ULL\s*\}')) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'compiled component retains name-derived Production dependency property identity'
                Text = '// CUI:AOT dependency-property-identity=token'
            })
        }
        foreach ($legacyRegistration in [regex]::Matches(
            $generatedDocumentCppText,
            '\bDependencyPropertyRegistry\s*::\s*(?:Register|RegisterReadOnly)\s*<')) {
            $line = 1
            if ($legacyRegistration.Index -ne 0) {
                $line += ([regex]::Matches(
                    $generatedDocumentCppText.Substring(
                        0, $legacyRegistration.Index), "`n")).Count
            }
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = $line
                Kind = 'compiled component still publishes dependency property through the runtime registry'
                Text = ([regex]::Replace(
                    $legacyRegistration.Value, '\s+', ' ')).Trim()
            })
        }
        foreach ($eagerPropertyTouch in [regex]::Matches(
            $generatedDocumentCppText,
            '(?m)^\s*\(void\)\s*[A-Za-z_][A-Za-z0-9_]*Property\s*\(\s*\)\s*;\s*$')) {
            $line = 1
            if ($eagerPropertyTouch.Index -ne 0) {
                $line += ([regex]::Matches(
                    $generatedDocumentCppText.Substring(
                        0, $eagerPropertyTouch.Index), "`n")).Count
            }
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = $line
                Kind = 'compiled component constructor eagerly materializes dependency property storage'
                Text = $eagerPropertyTouch.Value.Trim()
            })
        }
    }

    # These fixtures deliberately contain statically resolvable Binding
    # sources.  Their generated Production code must therefore exercise the
    # direct-source ABI instead of proving only that the legacy token adapter
    # remains available.  Keep this as a positive capability check while the
    # remaining Binding scopes are migrated; the broader zero-legacy check is
    # added only after all scopes share the direct emitter.
    $compiledDirectBindingFixtureNames = @('DemoWindow')
    if ($compiledDirectBindingFixtureNames -contains
            [System.IO.Path]::GetFileName($resolvedBase)) {
        $directBindingMarkers = [regex]::Matches(
            $generatedDocumentCppText,
            '(?m)^\s*//\s*CUI:AOT\s+binding-source\s*=\s*direct-dp\b')
        if ($directBindingMarkers.Count -eq 0) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'known Binding fixture emitted no direct-source marker'
                Text = '// CUI:AOT binding-source=direct-dp'
            })
        }

        $compiledDependencyPropertySources = [regex]::Matches(
            $generatedDocumentCppText,
            '\bMakeCompiledDependencyPropertySource\s*\(')
        if ($compiledDependencyPropertySources.Count -eq 0) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = 0
                Kind = 'known Binding fixture emitted no direct dependency-property source'
                Text = 'MakeCompiledDependencyPropertySource(...)'
            })
        }
    }
}

$productionSourcePatterns = @(
    [pscustomobject]@{
        Name = 'dynamic XAML header in production source'
        Regex = '^\s*#\s*include\s*[<"][^">]*(?:CuiRuntime|XamlInfrastructure|XamlObjectMaterializer|RuntimeDocument|XamlDocument|ComponentBehavior|XmlLite)[^">]*[>"]'
    }
    [pscustomobject]@{
        Name = 'dynamic XAML namespace in production source'
        Regex = '\b(?:CuiRuntime|DesignerModel|XmlLite)\s*::'
    }
    [pscustomobject]@{
        Name = 'dynamic XAML mutation bridge in production source'
        Regex = '\bcui::framework::XamlAccess\s*::'
    }
    [pscustomobject]@{
        Name = 'dynamic component behavior in production source'
        Regex = '\b(?:IDeclarativeComponentBehavior|DeclarativeComponentBehaviorContext|GetDeclarativeComponentBehavior|SetComponentBehavior)\b'
    }
    [pscustomobject]@{
        Name = 'dynamic declarative type identity in production source'
        Regex = '(?:\bDeclarativeTypeDescriptor\b|\bRuntimeTypeId\s*(?:\{|[&*]|[A-Za-z_]\w*\s*[;=({,)]|::)|(?:<|,)\s*RuntimeTypeId\s*[,>])'
    }
    [pscustomobject]@{
        Name = 'string declarative event dispatch in production source'
        Regex = '\bRaiseDeclarativeEvent\s*\('
    }
)

$resolvedSourceFiles = @()
if (-not [string]::IsNullOrWhiteSpace($SourceFiles)) {
    $resolvedSourceFiles = @(
        $SourceFiles.Split(
            [char]';',
            [System.StringSplitOptions]::RemoveEmptyEntries) |
            ForEach-Object { [System.IO.Path]::GetFullPath($_.Trim()) } |
            Sort-Object -Unique
    )
}
$missingSourceFiles = @(
    $resolvedSourceFiles |
        Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
)
if ($missingSourceFiles.Count -ne 0) {
    throw "CUI AOT gate is missing production source: $($missingSourceFiles -join ', ')"
}
foreach ($file in $resolvedSourceFiles) {
    foreach ($pattern in @($productionSourcePatterns) + @($productionStylePatterns)) {
        foreach ($match in Select-String -LiteralPath $file -Pattern $pattern.Regex) {
            $violations.Add([pscustomobject]@{
                File = $file
                Line = $match.LineNumber
                Kind = $pattern.Name
                Text = $match.Line.Trim()
            })
        }
    }

    # DependencyProperty identity overloads are legitimate WPF runtime
    # primitives. Reject only the name-based overload, including calls whose
    # first string argument is formatted onto the next line.
    $sourceText = [System.IO.File]::ReadAllText($file)
    foreach ($match in [regex]::Matches(
        $sourceText,
        '\bTry(?:Get|Set)(?:Current)?PropertyValue\s*\(\s*L"')) {
        $line = 1
        if ($match.Index -ne 0) {
            $line += ([regex]::Matches(
                $sourceText.Substring(0, $match.Index), "`n")).Count
        }
        $violations.Add([pscustomobject]@{
            File = $file
            Line = $line
            Kind = 'string property access in production source'
            Text = ([regex]::Replace($match.Value, '\s+', ' ')).Trim()
        })
    }
    foreach ($namedAccess in @(
        [pscustomobject]@{
            Kind = 'string DependencyPropertyAccess in production source'
            Regex = '\bDependencyPropertyAccess\s*::\s*(?:SetValue|SetBaseValue|SetDynamicResource|ClearValue|ClearDynamicResource)\s*\(\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string target-property query in production source'
            Regex = '\b(?:FindDependencyProperty|FindPropertyMetadata|ClearPropertyValue|HasPropertyValue|GetPropertyValueSource|GetPropertyExpressionKind|ResetPropertyValue|IsPropertyValueDefault|CoerceValue|TryGetDynamicResourceKey)\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Binding constructor target in production source'
            Regex = '\b(?:Binding|MultiBinding)\s*[\(\{]\s*[^,\r\n]+,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionViewSource binding path in production source'
            Regex = '\bSetSourceBindingPath\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionView member description in production source'
            Regex = '\bSet(?:Group|Sort|Filter)Descriptions\s*\(\s*\{\s*\{\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string CollectionView aggregate member description in production source'
            Regex = '\bSetAggregateDescriptions\s*\(\s*\{\s*\{\s*L"[^"\r\n]*"\s*,\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string item projection member path in production source'
            Regex = '\bSet(?:DisplayMemberPath|SelectedValuePath|HeaderDisplayMemberPath)\s*\(\s*(?:std\s*::\s*wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'string DynamicResource property in production source'
            Regex = '(?:->|\.)SetDynamicResource\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Style Setter property in production source'
            Regex = '\bControlStyleSetter\s*(?:::\s*(?:Resource|DynamicResource))?\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string Style Trigger condition in production source'
            Regex = '\bPropertyConditions\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string ControlStylePropertyCondition in production source'
            Regex = '\bControlStylePropertyCondition\s*[\(\{]\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'dynamic DependencyPropertyReference in production source'
            Regex = '\bDependencyPropertyReference(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'string VisualState condition property in production source'
            Regex = '\b(?:DeclarativeVisualStateCondition(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]|Conditions\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{?)\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'string VisualState setter property in production source'
            Regex = '\b(?:DeclarativeVisualStateSetter(?:\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:=)?\s*)?[\(\{]|Setters\s*\.\s*(?:push_back|emplace_back)\s*\(\s*\{?)\s*[^,\r\n]+,\s*(?:std::wstring\s*[\(\{]\s*)?L"'
        }
        [pscustomobject]@{
            Kind = 'legacy string animation property in production source'
            Regex = '\banimation\s*\.\s*PropertyName\b'
        }
        [pscustomobject]@{
            Kind = 'string Binding target in production source'
            Regex = '\bDataBindings\s*\.\s*Add\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string MultiBinding target in production source'
            Regex = '\bDataBindings\s*\.\s*AddMulti\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string TemplateBinding endpoint in production source'
            Regex = '\bDataBindings\s*\.\s*AddTemplateBinding\s*\(\s*L"'
        }
        [pscustomobject]@{
            Kind = 'string BindingCollection target management in production source'
            Regex = '\bDataBindings\s*\.\s*(?:Find|FindMulti|Remove|UpdateTarget|UpdateSource)\s*\(\s*L"'
        }
    )) {
        foreach ($match in [regex]::Matches(
            $sourceText, $namedAccess.Regex)) {
            $line = 1
            if ($match.Index -ne 0) {
                $line += ([regex]::Matches(
                    $sourceText.Substring(0, $match.Index), "`n")).Count
            }
            $violations.Add([pscustomobject]@{
                File = $file
                Line = $line
                Kind = $namedAccess.Kind
                Text = ([regex]::Replace($match.Value, '\s+', ' ')).Trim()
            })
        }
    }
}

# Built-in Theme selectors are authoritative by UIClass. Adding the built-in
# urn:cui QName to the same selector would make Theme applicability depend on
# dynamic XAML identity, which production-generated controls do not carry.
$typedSelectors = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal)
foreach ($match in Select-String -LiteralPath $generatedCpp `
    -Pattern '^\s*([A-Za-z_][A-Za-z0-9_]*)\.Type\s*=') {
    [void]$typedSelectors.Add($match.Matches[0].Groups[1].Value)
}
foreach ($match in Select-String -LiteralPath $generatedCpp `
    -Pattern '^\s*([A-Za-z_][A-Za-z0-9_]*)\.DeclarativeTypeNamespace\s*=\s*L"urn:cui";') {
    $selector = $match.Matches[0].Groups[1].Value
    if ($typedSelectors.Contains($selector)) {
        $violations.Add([pscustomobject]@{
            File = $generatedCpp
            Line = $match.LineNumber
            Kind = 'native UIClass selector redundantly requires QName'
            Text = $match.Line.Trim()
        })
    }
}

# CUITest's DemoImage is a document ResourceDictionary object referenced by
# several authored Image.Source properties. StaticResource must preserve one
# object instance per generated window rather than re-running the bitmap
# factory at every property assignment.
$hasDemoImageResource = @(Select-String -LiteralPath $generatedCpp `
    -Pattern '__styleSheet->SetResource\(L"DemoImage",').Count -ne 0
if ($hasDemoImageResource) {
    $demoImageSetters = @(Select-String -LiteralPath $generatedCpp `
        -Pattern '__styleSheet->SetResource\(L"DemoImage",\s*(__documentStaticResource_DemoImage_[A-Za-z0-9_]+)\);')
    if ($demoImageSetters.Count -ne 1) {
        $violations.Add([pscustomobject]@{
            File = $generatedCpp
            Line = 0
            Kind = 'document StaticResource is not materialized once'
            Text = 'DemoImage'
        })
    } else {
        $demoImageVariable = $demoImageSetters[0].Matches[0].Groups[1].Value
        $declarations = @(Select-String -LiteralPath $generatedCpp `
            -Pattern ("^\s*const auto {0}\s*=\s*BindingValue\(cui::resources::LoadBitmapResource\(" -f
                [regex]::Escape($demoImageVariable)))
        $typedUses = @(Select-String -LiteralPath $generatedCpp `
            -Pattern ("SetSource\(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>\({0}\)\)" -f
                [regex]::Escape($demoImageVariable)))
        $nameBasedUses = @(Select-String -LiteralPath $generatedCpp `
            -Pattern ("TrySetPropertyValue\(L`"Source`",\s*{0}\)" -f
                [regex]::Escape($demoImageVariable)))
        $inlineSourceFactories = @(Select-String -LiteralPath $generatedCpp `
            -Pattern '(?:SetSource|TrySetPropertyValue\(L"Source").*LoadBitmapResource\(')
        if ($declarations.Count -ne 1 -or
            $typedUses.Count -lt 2 -or
            $nameBasedUses.Count -ne 0 -or
            $inlineSourceFactories.Count -ne 0) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = $demoImageSetters[0].LineNumber
                Kind = 'document StaticResource sharing contract is incomplete'
                Text = $demoImageVariable
            })
        }
    }
}

$hasScopeContractResource = @(Select-String -LiteralPath $generatedCpp `
    -Pattern 'SetResource\(L"ContractSharedImage",').Count -ne 0
if ($hasScopeContractResource) {
    $contractSetters = @(Select-String -LiteralPath $generatedCpp `
        -Pattern '__styleSheet->SetResource\(L"ContractSharedImage",\s*(__documentStaticResource_ContractSharedImage_[A-Za-z0-9_]+)\);')
    if ($contractSetters.Count -ne 1) {
        $violations.Add([pscustomobject]@{
            File = $generatedCpp
            Line = 0
            Kind = 'scope contract root StaticResource is not shared'
            Text = 'ContractSharedImage'
        })
    } else {
        $contractVariable = $contractSetters[0].Matches[0].Groups[1].Value
        $rootUses = @(Select-String -LiteralPath $generatedCpp `
            -Pattern ("(?:rootImage1|rootImage2)->SetSource\(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>\({0}\)\)" -f
                [regex]::Escape($contractVariable)))
        $shadowedUse = @(Select-String -LiteralPath $generatedCpp `
            -Pattern ("localImage->SetSource\(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>\({0}\)\)" -f
                [regex]::Escape($contractVariable)))
        $localFactory = @(Select-String -LiteralPath $generatedCpp `
            -Pattern 'localImage->SetSource\(cui::resources::LoadBitmapResource\(')
        $nameBasedUses = @(Select-String -LiteralPath $generatedCpp `
            -Pattern '(?:rootImage1|rootImage2|localImage)->TrySetPropertyValue\(L"Source"')
        if ($rootUses.Count -ne 2 -or
            $shadowedUse.Count -ne 0 -or
            $localFactory.Count -ne 1 -or
            $nameBasedUses.Count -ne 0) {
            $violations.Add([pscustomobject]@{
                File = $generatedCpp
                Line = $contractSetters[0].LineNumber
                Kind = 'lexically shadowed StaticResource reused root instance'
                Text = $contractVariable
            })
        }
    }
}

# Every generated ControlTemplate visual has an explicit WPF logical-tree
# projection after visual attachment: normally null, or the authored content
# owner for a projected ContentPresenter subtree.
$templateAttachments = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal)
foreach ($match in Select-String -LiteralPath $generatedCpp `
    -Pattern 'std::move\(__owned_(__cuiStaticTemplateOwner[A-Za-z0-9_]*)\)') {
    foreach ($capture in $match.Matches) {
        [void]$templateAttachments.Add($capture.Groups[1].Value)
    }
}
$templateLogicalParents = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal)
foreach ($match in Select-String -LiteralPath $generatedCpp `
    -Pattern 'SetLogicalParent\(\*(__cuiStaticTemplateOwner[A-Za-z0-9_]*),') {
    foreach ($capture in $match.Matches) {
        [void]$templateLogicalParents.Add($capture.Groups[1].Value)
    }
}
foreach ($templateVisual in $templateAttachments) {
    if (-not $templateLogicalParents.Contains($templateVisual)) {
        $violations.Add([pscustomobject]@{
            File = $generatedCpp
            Line = 0
            Kind = 'generated template visual has no explicit logical parent'
            Text = $templateVisual
        })
    }
}

if ($violations.Count -ne 0) {
    $message = 'CUI artifact crossed the production-only AOT boundary.'
    if ($AllowKnownBlockers) {
        Write-Warning "$message Known blockers are allowed for this diagnostic compile only."
    } else {
        Write-Host $message
    }
    foreach ($violation in $violations) {
        Write-Host ("{0}({1}): {2}: {3}" -f
            $violation.File,
            $violation.Line,
            $violation.Kind,
            $violation.Text)
    }
    if (-not $AllowKnownBlockers) {
        exit 1
    }
}

if ($violations.Count -eq 0) {
    Write-Host "CUI AOT $ArtifactKind runtime-boundary gate passed: $resolvedBase"
}

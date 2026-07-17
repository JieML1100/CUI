#include "TestRunner.h"
#include <AnchorPickerPopup.h>
#include <Button.h>
#include <CheckBox.h>
#include <CalendarView.h>
#include <ColorPicker.h>
#include <ComboBox.h>
#include <ContextMenu.h>
#include <Core/Geometry.h>
#include <DateTimePicker.h>
#include <Expander.h>
#include <Form.h>
#include <GroupBox.h>
#include <GridView.h>
#include <Layout/LayoutEngine.h>
#include <Layout/LayoutDeferral.h>
#include <Layout/LayoutState.h>
#include <Layout/LegacyCanvasLayout.h>
#include <Layout/DockPanel.h>
#include <Layout/GridPanel.h>
#include <Layout/StackPanel.h>
#include <Layout/WrapPanel.h>
#include <ListView.h>
#include <LinkLabel.h>
#include <LoadingRing.h>
#include <MediaPlayer.h>
#include <Menu.h>
#include <NavigationView.h>
#include <NotifyIcon.h>
#include <NumericUpDown.h>
#include <ObservableCollection.h>
#include <PagedGridView.h>
#include <Panel.h>
#include <PasswordBox.h>
#include <ProgressBar.h>
#include <ProgressRing.h>
#include <PropertyGrid.h>
#include <PictureBox.h>
#include <RadioBox.h>
#include <ScrollView.h>
#include <Slider.h>
#include <SplitContainer.h>
#include <StatusBar.h>
#include <Style.h>
#include <Switch.h>
#include <TabControl.h>
#include <Taskbar.h>
#include <TextBox.h>
#include <ToolBar.h>
#include <TreeView.h>
#include <WebBrowser.h>
#include "../CuiDesigner/DesignerBindingUtils.h"
#include "../CuiDesigner/DesignerCore/CommandManager.h"
#include "../CuiDesigner/DesignerControlPropertyCatalog.h"
#include "../CuiDesigner/DesignerDataContextSchemaUtils.h"
#include "../CuiDesigner/DesignerFormPropertyCatalog.h"
#include "../CuiDesigner/DesignerCustomEditorCatalog.h"
#include "../CuiDesigner/DesignerPropertyCatalog.h"
#include "../CuiDesigner/DesignerPropertyEdit.h"
#include "../CuiDesigner/DesignerPropertyRowCatalog.h"
#include "../CuiDesigner/DesignerStyleSheetUtils.h"
#include "../CuiDesigner/CodeGenInput.h"
#include "../CuiDesigner/CodeGenerator.h"
#include "../CuiDesigner/DesignerEventCatalog.h"
#include "../CuiDesigner/DesignerModel/AtomicFile.h"
#include "../CuiDesigner/DesignerModel/DesignCodeGenerationService.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentControlPool.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentGraph.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentEventIndex.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentSerializer.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentCodeGenInputBuilder.h"
#include "../CuiDesigner/DesignerModel/RuntimeDocument.h"
#include "../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../CuiDesigner/DesignerModel/XamlDocumentSerializer.h"
#include "../CuiDesigner/DesignerModel/DesignRecoveryStore.h"
#include <algorithm>
#include <chrono>
#include <concepts>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <oleacc.h>
#include <set>
#include <uiautomationclient.h>

static_assert(std::same_as<
    decltype(std::declval<Control&>().DetachControl(static_cast<Control*>(nullptr))),
    std::unique_ptr<Control>>);
static_assert(std::same_as<
    decltype(std::declval<Control&>().Measure(std::declval<const cui::core::Constraints&>())),
    cui::core::Size>);
static_assert(std::same_as<
    decltype(std::declval<const Control&>().GetLayoutWidth()),
    cui::layout::Length>);
static_assert(std::same_as<
	decltype(WebBrowser::NavigationCompletedArgs{}.WebErrorStatus), int>);
static_assert(std::same_as<
	decltype(WebBrowser::ProcessFailedArgs{}.Kind), int>);

namespace
{
    class LayoutScopeProbe final
    {
    public:
        int SuspendCount = 0;
        int ResumeCount = 0;
        bool LastPerformLayout = true;

        void SuspendLayout() { ++SuspendCount; }
        void ResumeLayout(bool performLayout)
        {
            ++ResumeCount;
            LastPerformLayout = performLayout;
        }
    };

    class LegacyLayoutProbe final : public LayoutEngine
    {
    public:
        Control* ReceivedContainer = nullptr;

        SIZE Measure(Control* container, SIZE availableSize) override
        {
            ReceivedContainer = container;
            return SIZE{ availableSize.cx / 2, availableSize.cy / 2 };
        }
    };

    class ContextLayoutProbe final : public LayoutEngine
    {
    public:
        bool ReceivedWindowRoot = false;
        int ReceivedChildCount = 0;

        SIZE Measure(LayoutContext& context, SIZE availableSize) override
        {
            ReceivedWindowRoot = context.IsWindowRoot();
            ReceivedChildCount = context.ChildCount();
            return availableSize;
        }
    };

    class FractionalLayoutProbe final : public LayoutEngine
    {
    public:
        cui::core::Constraints ReceivedConstraints{};

        cui::core::Size Measure(
            LayoutContext&,
            const cui::core::Constraints& available) override
        {
            ReceivedConstraints = available;
            return { 17.625f, 9.375f };
        }
    };

	class IndexedVirtualProbe final :
		public Control,
		public IAccessibilityVirtualizedControl
	{
	public:
		int LegacyChildrenQueries = 0;
		int IndexedCountQueries = 0;
		int IndexedChildQueries = 0;
		int IndexedSiblingQueries = 0;
		int DirectHitQueries = 0;
		std::array<uint32_t, 3> Ids{
			AllocateAccessibilityVirtualId(),
			AllocateAccessibilityVirtualId(),
			AllocateAccessibilityVirtualId() };

		IndexedVirtualProbe()
		{
			Location = POINT{ 530, 385 };
			Size = SIZE{ 180, 90 };
		}

		UIClass Type() override { return UIClass::UI_CUSTOM; }
		void GetAccessibilityVirtualChildren(
			uint32_t parentId, std::vector<uint32_t>& result) override
		{
			++LegacyChildrenQueries;
			result.clear();
			if (parentId == 0) result.assign(Ids.begin(), Ids.end());
		}
		bool TryGetAccessibilityVirtualNode(
			uint32_t id, AccessibilityVirtualNode& result) override
		{
			const auto position = std::find(Ids.begin(), Ids.end(), id);
			if (position == Ids.end()) return false;
			const int index = static_cast<int>(position - Ids.begin());
			result = {};
			result.Id = id;
			result.Role = AccessibleRole::ListItem;
			result.Name = L"Fast " + std::to_wstring(index);
			result.AutomationId = L"fast-node-" + std::to_wstring(index);
			result.BoundsDip = D2D1::RectF(
				0.0f, index * 30.0f, 180.0f, (index + 1) * 30.0f);
			result.Visible = true;
			result.Enabled = true;
			result.Row = index;
			return true;
		}
		size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override
		{
			++IndexedCountQueries;
			return parentId == 0 ? Ids.size() : 0;
		}
		bool TryGetAccessibilityVirtualChildAt(
			uint32_t parentId, size_t index, uint32_t& result) override
		{
			++IndexedChildQueries;
			result = 0;
			if (parentId != 0 || index >= Ids.size()) return false;
			result = Ids[index];
			return true;
		}
		bool TryGetAccessibilityVirtualSibling(
			uint32_t parentId, uint32_t id, bool next, uint32_t& result) override
		{
			++IndexedSiblingQueries;
			result = 0;
			if (parentId != 0) return false;
			const auto position = std::find(Ids.begin(), Ids.end(), id);
			if (position == Ids.end()) return false;
			const ptrdiff_t index = position - Ids.begin();
			const ptrdiff_t sibling = next ? index + 1 : index - 1;
			if (sibling < 0 || sibling >= static_cast<ptrdiff_t>(Ids.size()))
				return false;
			result = Ids[static_cast<size_t>(sibling)];
			return true;
		}
		bool TryHitTestAccessibilityVirtualNode(
			float localX, float localY, uint32_t& result) override
		{
			++DirectHitQueries;
			result = 0;
			if (localX < 0.0f || localX >= 180.0f
				|| localY < 0.0f || localY >= 90.0f) return false;
			result = Ids[static_cast<size_t>(localY / 30.0f)];
			return true;
		}
	};

    struct BindingPayload final
    {
        int Id = 0;
        std::wstring Title;

        friend bool operator==(const BindingPayload&, const BindingPayload&) = default;
    };

	class MetadataObservableObject final : public ObservableObject
	{
	public:
		using ObservableObject::ClearAllValidationIssues;
		using ObservableObject::ClearValidationIssues;
		using ObservableObject::SetValidationError;
		using ObservableObject::SetValidationIssues;
		using ObservableObject::SetCurrentValue;
	};

    class MetadataBindingControl final : public Control
    {
    public:
        BindingPayload Payload;
        PropertyChangedEvent MetadataChanged;

        void EnsureBindingPropertiesRegistered() override
        {
            Control::EnsureBindingPropertiesRegistered();
            static const bool registered = []
            {
                BindingPropertyRegistry::Register<MetadataBindingControl, BindingPayload>(
                    L"Payload",
                    [](MetadataBindingControl& target) { return target.Payload; },
                    [](MetadataBindingControl& target, const BindingPayload& value)
                    {
                        target.SetPayload(value);
                    },
                    [](MetadataBindingControl& target,
						BindingPropertyMetadata::ChangeHandler handler,
						DataSourceUpdateMode)
					{
						return target.MetadataChanged.Subscribe(
							[handler = std::move(handler)](const PropertyChangedEventArgs&) { handler(); });
					});
                return true;
            }();
            (void)registered;
        }

        void SetPayload(BindingPayload value)
        {
            if (Payload == value) return;
            Payload = std::move(value);
            MetadataChanged.Notify(L"Payload");
        }
    };

	class PropertySystemControl final : public Control
	{
	private:
		int _level = 4;

	public:
		int ChangedCallbackCount = 0;
		UIClass Type() override { return UIClass::UI_CUSTOM; }
		int GetLevel() const noexcept { return _level; }
		void SetLevel(int value)
		{
			SetPropertyField(L"Level", _level, value);
		}
		void SetCurrentLevel(int value)
		{
			SetCurrentPropertyField(L"Level", _level, value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			Control::EnsureBindingPropertiesRegistered();
			static const bool registered = []
			{
				BindingPropertyRegistry::Register<PropertySystemControl, int>(
					L"Level",
					[](PropertySystemControl& target) { return target.GetLevel(); },
					[](PropertySystemControl& target, const int& value)
					{
						target.SetLevel(value);
					},
					[](PropertySystemControl& target,
						BindingPropertyMetadata::ChangeHandler handler,
						DataSourceUpdateMode)
					{
						return target.OnPropertyValueChanged.Subscribe(
							[handler = std::move(handler)](
								Control*, const ControlPropertyChangedEventArgs& e)
							{
								if (e.PropertyName == L"Level") handler();
							});
					},
					ControlPropertyOptions<PropertySystemControl, int>{
						4,
						ControlPropertyFlags::AffectsMeasure
							| ControlPropertyFlags::AffectsRender,
						[](PropertySystemControl&, const int& proposed)
							-> std::optional<int>
						{
							return (std::clamp)(proposed, 0, 10);
						},
						[](PropertySystemControl& target, const int&, const int&)
						{
							++target.ChangedCallbackCount;
						} });
				return true;
			}();
			(void)registered;
		}
	};

    class FractionalMeasureControl final : public Control
    {
    public:
        cui::core::Size Intrinsic{};

        explicit FractionalMeasureControl(cui::core::Size intrinsic)
            : Intrinsic(intrinsic)
        {
        }

        cui::core::Size MeasureCore(const cui::core::Constraints&) override
        {
            return Intrinsic;
        }
    };

    class WidthAwareMeasureControl final : public Control
    {
    public:
        cui::core::Constraints LastConstraints{};

        cui::core::Size MeasureCore(const cui::core::Constraints& available) override
        {
            LastConstraints = available.Normalized();
            const bool wraps = LastConstraints.maximum.width < 100.0f;
            return { 100.0f, wraps ? 40.0f : 10.0f };
        }
    };
}

int main()
{
    cui::test::Runner runner;

	runner.Add("Control render decorator is explicit host-owned state", []
	{
		Control control;
		CUI_EXPECT_FALSE(control.HasRenderDecorator());
		control.SetRenderDecorator([](Control&, D2DGraphics&) {});
		CUI_EXPECT_TRUE(control.HasRenderDecorator());
		control.SetRenderDecorator({});
		CUI_EXPECT_FALSE(control.HasRenderDecorator());
	});

    runner.Add("Runner assertions remain active", []
    {
        bool failureObserved = false;
        try
        {
            cui::test::ExpectTrue(false, "intentional failure", __FILE__, __LINE__);
        }
        catch (const cui::test::AssertionFailure&)
        {
            failureObserved = true;
        }

        CUI_EXPECT_TRUE(failureObserved);
    });

    runner.Add("Runner compares values", []
    {
        int evaluationCount = 0;
        CUI_EXPECT_EQ(1, ++evaluationCount);
        CUI_EXPECT_FALSE(evaluationCount == 0);
        CUI_EXPECT_NEAR(0.3, 0.1 + 0.2, 1.0e-12);
    });

	runner.Add("Designer command history preserves failed undo and redo entries", []
	{
		class ProbeCommand final : public IDesignerCommand
		{
		public:
			ProbeCommand(int* state, bool failUndo, bool failRedo)
				: _state(state), _failUndo(failUndo), _failRedo(failRedo)
			{
			}

			DesignerDocumentTransactionResult Execute() override
			{
				++_executeCount;
				if (_executeCount > 1 && _failRedo)
					return DesignerDocumentTransactionResult::Failure(
						DesignerDocumentTransactionState::Failed,
						L"probe redo failed");
				if (_state) ++*_state;
				return DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Committed);
			}

			DesignerDocumentTransactionResult Undo() override
			{
				if (_failUndo)
					return DesignerDocumentTransactionResult::Failure(
						DesignerDocumentTransactionState::Failed,
						L"probe undo failed");
				if (_state) --*_state;
				return DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Committed);
			}

			std::wstring GetLabel() const override
			{
				return L"Probe";
			}

		private:
			int* _state = nullptr;
			bool _failUndo = false;
			bool _failRedo = false;
			int _executeCount = 0;
		};

		int failedUndoState = 0;
		CommandManager failedUndo;
		auto executeFailedUndo = failedUndo.Execute(
			std::make_unique<ProbeCommand>(
				&failedUndoState, true, false));
		CUI_EXPECT_TRUE(executeFailedUndo.HasChanges());
		const auto failedUndoStateId = failedUndo.GetCurrentStateId();
		const auto failedUndoMemory = failedUndo.GetHistoryMemoryUsage();
		CUI_EXPECT_TRUE(failedUndo.IsDirty());
		CUI_EXPECT_EQ(1, failedUndoState);
		CUI_EXPECT_TRUE(failedUndo.GetUndoLabel() == L"Probe");
		auto failedUndoResult = failedUndo.Undo();
		CUI_EXPECT_FALSE(failedUndoResult.Succeeded());
		CUI_EXPECT_TRUE(failedUndoResult.Error == L"probe undo failed");
		CUI_EXPECT_TRUE(failedUndoResult.DocumentRestored);
		CUI_EXPECT_EQ(1, failedUndoState);
		CUI_EXPECT_EQ(1ULL, failedUndo.GetUndoCount());
		CUI_EXPECT_EQ(0ULL, failedUndo.GetRedoCount());
		CUI_EXPECT_EQ(failedUndoStateId,
			failedUndo.GetCurrentStateId());
		CUI_EXPECT_EQ(failedUndoMemory,
			failedUndo.GetHistoryMemoryUsage());

		int failedRedoState = 0;
		CommandManager failedRedo;
		CUI_EXPECT_TRUE(failedRedo.Execute(
			std::make_unique<ProbeCommand>(
				&failedRedoState, false, true)).HasChanges());
		CUI_EXPECT_TRUE(failedRedo.Undo().HasChanges());
		const auto failedRedoStateId = failedRedo.GetCurrentStateId();
		const auto failedRedoMemory = failedRedo.GetHistoryMemoryUsage();
		CUI_EXPECT_EQ(0, failedRedoState);
		CUI_EXPECT_TRUE(failedRedo.GetRedoLabel() == L"Probe");
		auto failedRedoResult = failedRedo.Redo();
		CUI_EXPECT_FALSE(failedRedoResult.Succeeded());
		CUI_EXPECT_TRUE(failedRedoResult.Error == L"probe redo failed");
		CUI_EXPECT_EQ(0, failedRedoState);
		CUI_EXPECT_EQ(0ULL, failedRedo.GetUndoCount());
		CUI_EXPECT_EQ(1ULL, failedRedo.GetRedoCount());
		CUI_EXPECT_EQ(failedRedoStateId,
			failedRedo.GetCurrentStateId());
		CUI_EXPECT_EQ(failedRedoMemory,
			failedRedo.GetHistoryMemoryUsage());

		int successState = 0;
		CommandManager success;
		CUI_EXPECT_TRUE(success.Execute(
			std::make_unique<ProbeCommand>(
				&successState, false, false)).HasChanges());
		CUI_EXPECT_TRUE(success.Undo().HasChanges());
		CUI_EXPECT_EQ(0, successState);
		CUI_EXPECT_TRUE(success.Redo().HasChanges());
		CUI_EXPECT_EQ(1, successState);
		CUI_EXPECT_TRUE(success.IsDirty());
		success.MarkSaved();
		const auto savedStateId = success.GetSavedStateId();
		CUI_EXPECT_EQ(savedStateId, success.GetCurrentStateId());
		CUI_EXPECT_FALSE(success.IsDirty());
		CUI_EXPECT_TRUE(success.Undo().HasChanges());
		CUI_EXPECT_TRUE(success.IsDirty());
		CUI_EXPECT_TRUE(success.Redo().HasChanges());
		CUI_EXPECT_FALSE(success.IsDirty());
		CUI_EXPECT_TRUE(success.Undo().HasChanges());
		CUI_EXPECT_TRUE(success.Execute(
			std::make_unique<ProbeCommand>(
				&successState, false, false)).HasChanges());
		CUI_EXPECT_EQ(1, successState);
		CUI_EXPECT_TRUE(success.IsDirty());
		CUI_EXPECT_TRUE(savedStateId != success.GetCurrentStateId());
		CUI_EXPECT_EQ(0ULL, success.GetRedoCount());
		const auto branchStateId = success.GetCurrentStateId();
		success.Clear();
		CUI_EXPECT_FALSE(success.IsDirty());
		CUI_EXPECT_TRUE(branchStateId != success.GetCurrentStateId());
		CUI_EXPECT_EQ(success.GetCurrentStateId(),
			success.GetSavedStateId());
		CUI_EXPECT_EQ(0ULL, success.GetUndoCount());
		CUI_EXPECT_EQ(0ULL, success.GetRedoCount());
		success.ResetAsUnsaved();
		CUI_EXPECT_TRUE(success.IsDirty());
		CUI_EXPECT_TRUE(success.GetCurrentStateId()
			!= success.GetSavedStateId());
		CUI_EXPECT_EQ(0ULL, success.GetUndoCount());
		CUI_EXPECT_EQ(0ULL, success.GetRedoCount());
		success.MarkSaved();
		CUI_EXPECT_FALSE(success.IsDirty());

		CommandManager empty;
		CUI_EXPECT_FALSE(empty.IsDirty());
		CUI_EXPECT_TRUE(empty.Undo().State
			== DesignerDocumentTransactionState::Unchanged);
		CUI_EXPECT_TRUE(empty.Redo().State
			== DesignerDocumentTransactionState::Unchanged);
	});

	runner.Add("Designer recovery snapshots are atomic isolated and versioned", []
	{
		wchar_t temporaryDirectory[MAX_PATH]{};
		CUI_EXPECT_TRUE(::GetTempPathW(
			MAX_PATH, temporaryDirectory) != 0);
		const auto processStart =
			DesignerModel::DesignRecoveryStore::GetCurrentProcessStartTime();
		const auto recoveryPath =
			DesignerModel::DesignRecoveryStore::MakeSessionFilePath(
				temporaryDirectory,
				::GetCurrentProcessId(),
				processStart + ::GetTickCount64());
		(void)DesignerModel::DesignRecoveryStore::DeleteFile(recoveryPath);

		DesignerModel::DesignRecoverySnapshot expected;
		expected.OwnerProcessId = ::GetCurrentProcessId();
		expected.OwnerProcessStartTime = processStart;
		expected.OriginalFilePath = L"C:\\设计\\窗口.cui.xml";
		expected.Document.Form.Text = L"恢复窗口";
		expected.Document.Form.Size = { 987, 654 };
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::SaveToFile(
			expected, recoveryPath, &error));
		CUI_EXPECT_TRUE(error.empty());

		DesignerModel::DesignRecoverySnapshot loaded;
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::LoadFromFile(
			recoveryPath, loaded, &error));
		CUI_EXPECT_EQ(expected.OwnerProcessId, loaded.OwnerProcessId);
		CUI_EXPECT_EQ(expected.OwnerProcessStartTime,
			loaded.OwnerProcessStartTime);
		CUI_EXPECT_TRUE(expected.OriginalFilePath == loaded.OriginalFilePath);
		CUI_EXPECT_TRUE(expected.Document == loaded.Document);
		CUI_EXPECT_TRUE(
			DesignerModel::DesignRecoveryStore::IsOwnerProcessRunning(loaded));
		loaded.OwnerProcessId = 0;
		CUI_EXPECT_FALSE(
			DesignerModel::DesignRecoveryStore::IsOwnerProcessRunning(loaded));

		auto replacement = expected;
		replacement.Document.Form.Text = L"不应覆盖";
		const HANDLE locked = ::CreateFileW(
			recoveryPath.c_str(), GENERIC_READ, 0, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		CUI_EXPECT_TRUE(locked != INVALID_HANDLE_VALUE);
		if (locked != INVALID_HANDLE_VALUE)
		{
			CUI_EXPECT_FALSE(DesignerModel::DesignRecoveryStore::SaveToFile(
				replacement, recoveryPath, &error));
			CUI_EXPECT_FALSE(error.empty());
			(void)::CloseHandle(locked);
		}
		DesignerModel::DesignRecoverySnapshot afterLockedSave;
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::LoadFromFile(
			recoveryPath, afterLockedSave, &error));
		CUI_EXPECT_TRUE(afterLockedSave.Document == expected.Document);
		WIN32_FIND_DATAW temporaryData{};
		const HANDLE temporaryFind = ::FindFirstFileW(
			(recoveryPath + L".~cui-*.tmp").c_str(), &temporaryData);
		CUI_EXPECT_TRUE(temporaryFind == INVALID_HANDLE_VALUE);
		if (temporaryFind != INVALID_HANDLE_VALUE)
			(void)::FindClose(temporaryFind);

		std::vector<DesignerModel::DesignRecoveryFile> recoveryFiles;
		CUI_EXPECT_TRUE(
			DesignerModel::DesignRecoveryStore::EnumerateRecoveryFiles(
				temporaryDirectory, recoveryFiles, &error));
		CUI_EXPECT_TRUE(std::any_of(
			recoveryFiles.begin(), recoveryFiles.end(),
			[&](const auto& file) { return file.Path == recoveryPath; }));

		CUI_EXPECT_TRUE(DesignerModel::AtomicFile::Write(
			recoveryPath, "invalid recovery envelope", &error));
		DesignerModel::DesignRecoverySnapshot unchanged;
		unchanged.OriginalFilePath = L"sentinel";
		CUI_EXPECT_FALSE(DesignerModel::DesignRecoveryStore::LoadFromFile(
			recoveryPath, unchanged, &error));
		CUI_EXPECT_TRUE(unchanged.OriginalFilePath == L"sentinel");
		CUI_EXPECT_FALSE(error.empty());
		std::wstring quarantinedPath;
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::QuarantineFile(
			recoveryPath, &quarantinedPath, &error));
		CUI_EXPECT_FALSE(quarantinedPath.empty());
		CUI_EXPECT_TRUE(::GetFileAttributesW(quarantinedPath.c_str())
			!= INVALID_FILE_ATTRIBUTES);
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::DeleteFile(
			quarantinedPath, &error));
		CUI_EXPECT_TRUE(DesignerModel::DesignRecoveryStore::DeleteFile(
			recoveryPath, &error));
	});

	runner.Add("Designer history coalesces safely and enforces a memory budget", []
	{
		class MergeProbeCommand final : public IDesignerCommand
		{
		public:
			MergeProbeCommand(
				int* state,
				int after,
				std::wstring key,
				size_t bytes = 100)
				: _state(state),
				  _before(state ? *state : 0),
				  _after(after),
				  _key(std::move(key)),
				  _bytes(bytes)
			{
			}

			DesignerDocumentTransactionResult Execute() override
			{
				if (_state) *_state = _after;
				return DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Committed);
			}

			DesignerDocumentTransactionResult Undo() override
			{
				if (_state) *_state = _before;
				return DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Committed);
			}

			std::wstring GetLabel() const override { return _key; }

			bool TryMergeWith(IDesignerCommand& command) noexcept override
			{
				auto* newer = dynamic_cast<MergeProbeCommand*>(&command);
				if (!newer || newer->_state != _state
					|| newer->_key != _key || _after != newer->_before)
					return false;
				_after = newer->_after;
				_bytes = newer->_bytes;
				return true;
			}

			size_t GetEstimatedMemoryUsage() const noexcept override
			{
				return _bytes;
			}

		private:
			int* _state = nullptr;
			int _before = 0;
			int _after = 0;
			std::wstring _key;
			size_t _bytes = 0;
		};

		int mergedState = 0;
		CommandManager merged;
		CUI_EXPECT_TRUE(merged.Execute(
			std::make_unique<MergeProbeCommand>(
				&mergedState, 1, L"Property:X", 100)).HasChanges());
		const auto firstMergedStateId = merged.GetCurrentStateId();
		CUI_EXPECT_TRUE(merged.Execute(
			std::make_unique<MergeProbeCommand>(
				&mergedState, 2, L"Property:X", 150)).HasChanges());
		CUI_EXPECT_EQ(2, mergedState);
		CUI_EXPECT_EQ(1ULL, merged.GetUndoCount());
		CUI_EXPECT_EQ(150ULL, merged.GetHistoryMemoryUsage());
		CUI_EXPECT_TRUE(firstMergedStateId != merged.GetCurrentStateId());
		CUI_EXPECT_TRUE(merged.Undo().HasChanges());
		CUI_EXPECT_EQ(0, mergedState);
		CUI_EXPECT_TRUE(merged.Redo().HasChanges());
		CUI_EXPECT_EQ(2, mergedState);

		int savedState = 0;
		CommandManager savedBoundary;
		CUI_EXPECT_TRUE(savedBoundary.Execute(
			std::make_unique<MergeProbeCommand>(
				&savedState, 1, L"Property:X")).HasChanges());
		savedBoundary.MarkSaved();
		const auto exactSavedStateId = savedBoundary.GetSavedStateId();
		CUI_EXPECT_TRUE(savedBoundary.Execute(
			std::make_unique<MergeProbeCommand>(
				&savedState, 2, L"Property:X")).HasChanges());
		CUI_EXPECT_EQ(2ULL, savedBoundary.GetUndoCount());
		CUI_EXPECT_TRUE(savedBoundary.IsDirty());
		CUI_EXPECT_TRUE(savedBoundary.Undo().HasChanges());
		CUI_EXPECT_EQ(1, savedState);
		CUI_EXPECT_FALSE(savedBoundary.IsDirty());
		CUI_EXPECT_EQ(exactSavedStateId,
			savedBoundary.GetCurrentStateId());

		int branchState = 0;
		CommandManager branch;
		CUI_EXPECT_TRUE(branch.Execute(
			std::make_unique<MergeProbeCommand>(
				&branchState, 1, L"Property:X")).HasChanges());
		CUI_EXPECT_TRUE(branch.Execute(
			std::make_unique<MergeProbeCommand>(
				&branchState, 2, L"Property:Y")).HasChanges());
		CUI_EXPECT_TRUE(branch.Undo().HasChanges());
		CUI_EXPECT_TRUE(branch.Execute(
			std::make_unique<MergeProbeCommand>(
				&branchState, 2, L"Property:X")).HasChanges());
		CUI_EXPECT_EQ(2ULL, branch.GetUndoCount());
		CUI_EXPECT_EQ(0ULL, branch.GetRedoCount());

		int budgetState = 0;
		CommandManager budget;
		budget.SetHistoryMemoryLimit(250);
		for (int value = 1; value <= 3; ++value)
			CUI_EXPECT_TRUE(budget.Execute(
				std::make_unique<MergeProbeCommand>(
					&budgetState, value,
					L"Distinct:" + std::to_wstring(value), 100)).HasChanges());
		CUI_EXPECT_EQ(2ULL, budget.GetUndoCount());
		CUI_EXPECT_EQ(200ULL, budget.GetHistoryMemoryUsage());
		CUI_EXPECT_EQ(250ULL, budget.GetHistoryMemoryLimit());
		CUI_EXPECT_TRUE(budget.Undo().HasChanges());
		CUI_EXPECT_EQ(2, budgetState);
		CUI_EXPECT_TRUE(budget.Undo().HasChanges());
		CUI_EXPECT_EQ(1, budgetState);
		CUI_EXPECT_TRUE(budget.Undo().State
			== DesignerDocumentTransactionState::Unchanged);

		int oversizedState = 0;
		CommandManager oversized;
		oversized.SetHistoryMemoryLimit(100);
		CUI_EXPECT_TRUE(oversized.Execute(
			std::make_unique<MergeProbeCommand>(
				&oversizedState, 1, L"Large", 500)).HasChanges());
		CUI_EXPECT_EQ(1ULL, oversized.GetUndoCount());
		CUI_EXPECT_EQ(500ULL, oversized.GetHistoryMemoryUsage());

		int redoState = 0;
		CommandManager redoBudget;
		redoBudget.SetHistoryMemoryLimit(1000);
		for (int value = 1; value <= 3; ++value)
			CUI_EXPECT_TRUE(redoBudget.Execute(
				std::make_unique<MergeProbeCommand>(
					&redoState, value,
					L"Redo:" + std::to_wstring(value), 100)).HasChanges());
		CUI_EXPECT_TRUE(redoBudget.Undo().HasChanges());
		CUI_EXPECT_TRUE(redoBudget.Undo().HasChanges());
		CUI_EXPECT_TRUE(redoBudget.Undo().HasChanges());
		redoBudget.SetHistoryMemoryLimit(150);
		CUI_EXPECT_EQ(0ULL, redoBudget.GetUndoCount());
		CUI_EXPECT_EQ(1ULL, redoBudget.GetRedoCount());
		CUI_EXPECT_EQ(100ULL, redoBudget.GetHistoryMemoryUsage());
		CUI_EXPECT_TRUE(redoBudget.Redo().HasChanges());
		CUI_EXPECT_EQ(1, redoState);

		int pressureState = 0;
		CommandManager pressure;
		pressure.SetHistoryMemoryLimit(500);
		for (int value = 1; value <= 1000; ++value)
		{
			CUI_EXPECT_TRUE(pressure.Execute(
				std::make_unique<MergeProbeCommand>(
					&pressureState, value,
					L"Pressure:" + std::to_wstring(value), 100)).HasChanges());
			if (value == 500) pressure.MarkSaved();
		}
		CUI_EXPECT_EQ(1000, pressureState);
		CUI_EXPECT_EQ(5ULL, pressure.GetUndoCount());
		CUI_EXPECT_EQ(500ULL, pressure.GetHistoryMemoryUsage());
		CUI_EXPECT_TRUE(pressure.IsDirty());
		for (int step = 0; step < 5; ++step)
			CUI_EXPECT_TRUE(pressure.Undo().HasChanges());
		CUI_EXPECT_EQ(995, pressureState);
		CUI_EXPECT_TRUE(pressure.IsDirty());
		CUI_EXPECT_TRUE(pressure.Undo().State
			== DesignerDocumentTransactionState::Unchanged);
	});

    runner.Add("DIP rectangles normalize and clip", []
    {
        using namespace cui::core;

        const Rect normalized = Rect{ 20.0f, 30.0f, -10.0f, -20.0f }.Normalized();
        CUI_EXPECT_EQ((Rect{ 10.0f, 10.0f, 10.0f, 20.0f }), normalized);
        CUI_EXPECT_TRUE(normalized.Contains(Point{ 10.0f, 10.0f }));
        CUI_EXPECT_FALSE(normalized.Contains(Point{ 20.0f, 30.0f }));

        const Rect clipped = normalized.Intersection(Rect{ 15.0f, 0.0f, 20.0f, 20.0f });
        CUI_EXPECT_EQ((Rect{ 15.0f, 10.0f, 5.0f, 10.0f }), clipped);
        CUI_EXPECT_EQ((Rect{ 10.0f, 0.0f, 25.0f, 30.0f }),
            normalized.Union(Rect{ 15.0f, 0.0f, 20.0f, 20.0f }));
    });

    runner.Add("Fractional hit bounds stay half open", []
    {
        using namespace cui::core;

        const Rect left{ 0.0f, 0.0f, 10.5f, 20.0f };
        const Rect right{ 10.5f, 0.0f, 8.25f, 20.0f };

        CUI_EXPECT_TRUE(left.Contains(Point{ 10.499f, 5.0f }));
        CUI_EXPECT_FALSE(left.Contains(Point{ 10.5f, 5.0f }));
        CUI_EXPECT_TRUE(right.Contains(Point{ 10.5f, 5.0f }));
        CUI_EXPECT_FALSE(right.Contains(Point{ 18.75f, 5.0f }));
    });

    runner.Add("Fractional child bounds union preserves negative origins", []
    {
        using namespace cui::core;

        const Rect first{ -2.25f, 4.5f, 10.0f, 8.0f };
        const Rect second{ 7.5f, -3.25f, 4.75f, 6.0f };
        const Rect bounds = first.Union(second);

        CUI_EXPECT_NEAR(-2.25f, bounds.Left(), 0.0001f);
        CUI_EXPECT_NEAR(-3.25f, bounds.Top(), 0.0001f);
        CUI_EXPECT_NEAR(12.25f, bounds.Right(), 0.0001f);
        CUI_EXPECT_NEAR(12.5f, bounds.Bottom(), 0.0001f);
    });

    runner.Add("Layout deferral coalesces nested work", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LayoutDeferral deferral;
        deferral.Suspend();
        deferral.Suspend();
        deferral.QueueLayout();
        deferral.QueueVisual(Rect{ 10.0f, 20.0f, 30.0f, 40.0f });
        deferral.QueueVisual(Rect{ -5.0f, 25.0f, 20.0f, 10.0f }, true);

        const auto inner = deferral.Resume();
        CUI_EXPECT_FALSE(inner.ready);
        CUI_EXPECT_TRUE(deferral.IsSuspended());

        const auto outer = deferral.Resume();
        CUI_EXPECT_TRUE(outer.ready);
        CUI_EXPECT_TRUE(outer.layoutRequested);
        CUI_EXPECT_TRUE(outer.visualRequested);
        CUI_EXPECT_TRUE(outer.immediate);
        CUI_EXPECT_FALSE(outer.fullVisual);
        CUI_EXPECT_EQ((Rect{ -5.0f, 20.0f, 45.0f, 40.0f }), outer.visualBounds);
        CUI_EXPECT_FALSE(deferral.IsSuspended());
    });

    runner.Add("Full visual invalidation supersedes deferred bounds", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LayoutDeferral deferral;
        deferral.Suspend();
        deferral.QueueVisual(Rect{ 1.0f, 2.0f, 3.0f, 4.0f });
        deferral.QueueFullVisual();
        deferral.QueueVisual(Rect{ 50.0f, 60.0f, 10.0f, 10.0f }, true);

        const auto work = deferral.Resume();
        CUI_EXPECT_TRUE(work.ready);
        CUI_EXPECT_TRUE(work.visualRequested);
        CUI_EXPECT_TRUE(work.fullVisual);
        CUI_EXPECT_TRUE(work.immediate);
        CUI_EXPECT_TRUE(work.visualBounds.IsEmpty());
    });

    runner.Add("Layout scope resumes exactly once", []
    {
        using namespace cui::layout;

        LayoutScopeProbe probe;
        {
            auto scope = DeferLayout(probe, true);
            CUI_EXPECT_TRUE(scope.IsActive());
            CUI_EXPECT_EQ(1, probe.SuspendCount);
            CUI_EXPECT_EQ(0, probe.ResumeCount);
            scope.Commit();
            CUI_EXPECT_FALSE(scope.IsActive());
        }
        CUI_EXPECT_EQ(1, probe.ResumeCount);
        CUI_EXPECT_TRUE(probe.LastPerformLayout);
    });

    runner.Add("Layout scope avoids synchronous layout while unwinding", []
    {
        using namespace cui::layout;

        LayoutScopeProbe probe;
        try
        {
            auto scope = DeferLayout(probe, true);
            throw 42;
        }
        catch (int)
        {
        }

        CUI_EXPECT_EQ(1, probe.SuspendCount);
        CUI_EXPECT_EQ(1, probe.ResumeCount);
        CUI_EXPECT_FALSE(probe.LastPerformLayout);
    });

    runner.Add("Constraints normalize clamp and deflate", []
    {
        using namespace cui::core;

        const Constraints normalized = Constraints{
            Size{ -5.0f, 20.0f },
            Size{ 10.0f, 5.0f }
        }.Normalized();
        CUI_EXPECT_EQ((Size{ 0.0f, 20.0f }), normalized.minimum);
        CUI_EXPECT_EQ((Size{ 10.0f, 20.0f }), normalized.maximum);
        CUI_EXPECT_EQ((Size{ 10.0f, 20.0f }), normalized.Constrain(Size{ 50.0f, 1.0f }));

        const Constraints deflated = Constraints{ Size{ 100.0f, 80.0f } }
            .Deflate(Insets{ 10.0f, 5.0f, 20.0f, 15.0f });
        CUI_EXPECT_EQ((Size{ 70.0f, 60.0f }), deflated.maximum);
    });

    runner.Add("Layout state keeps specified and computed data separate", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        const LayoutStyle specified{
            .width = Length::Fixed(120.0f),
            .height = Length::Fixed(30.0f)
        };
        LayoutState computed;

        computed.CommitMeasure(Size{ 90.0f, 25.0f }, Constraints{ Size{ 200.0f, 100.0f } });
        computed.CommitArrange(Rect{ 10.0f, 20.0f, 180.0f, 40.0f });
        computed.CommitPaint();

        CUI_EXPECT_EQ(Length::Fixed(120.0f), specified.width);
        CUI_EXPECT_EQ(Length::Fixed(30.0f), specified.height);
        CUI_EXPECT_EQ((Size{ 90.0f, 25.0f }), computed.desiredSize);
        CUI_EXPECT_EQ((Rect{ 10.0f, 20.0f, 180.0f, 40.0f }), computed.arrangedRect);
        CUI_EXPECT_FALSE(computed.NeedsMeasure());
        CUI_EXPECT_FALSE(computed.NeedsArrange());
        CUI_EXPECT_FALSE(computed.NeedsPaint());

        computed.InvalidateMeasure();
        CUI_EXPECT_TRUE(computed.NeedsMeasure());
        CUI_EXPECT_TRUE(computed.NeedsArrange());
        CUI_EXPECT_TRUE(computed.NeedsPaint());
    });

    runner.Add("Auto and fractional DIP lengths remain explicit", []
    {
        using namespace cui::layout;

        LayoutStyle style;
        CUI_EXPECT_TRUE(style.width.IsAuto());
        CUI_EXPECT_TRUE(style.height.IsAuto());

        style.width = Length::Fixed(42.625f);
        CUI_EXPECT_TRUE(style.width.IsFixed());
        CUI_EXPECT_NEAR(42.625f, style.width.value, 0.0001f);

        style.width = Length::Auto();
        CUI_EXPECT_TRUE(style.width.IsAuto());
        CUI_EXPECT_NEAR(0.0f, style.width.value, 0.0001f);

        const GridLength percent = GridLength::Percent(37.5f);
        CUI_EXPECT_TRUE(percent.IsPercent());
        CUI_EXPECT_NEAR(37.5f, percent.Value, 0.0001f);
    });

    runner.Add("Event connections unsubscribe with RAII", []
    {
        Event<void(int)> event;
        int observed = 0;
        {
            auto connection = event.Subscribe([&](int value) { observed += value; });
            CUI_EXPECT_TRUE(connection.Connected());
            CUI_EXPECT_EQ(1ULL, event.Count());
            event.Invoke(3);
            CUI_EXPECT_EQ(3, observed);
        }

        CUI_EXPECT_EQ(0ULL, event.Count());
        event.Invoke(4);
        CUI_EXPECT_EQ(3, observed);
    });

	runner.Add("Observable collections publish precise and batched changes", []
	{
		ObservableCollection<int> values{ 10, 20 };
		std::vector<CollectionChangedEventArgs> ownerChanges;
		std::vector<CollectionChangedEventArgs> publicChanges;
		values.SetOwnerChangedHandler([&](const CollectionChangedEventArgs& args)
		{
			ownerChanges.push_back(args);
			CUI_EXPECT_EQ(args.NewSize, values.size());
		});
		auto connection = values.Changed.Subscribe(
			[&](ObservableCollection<int>*, const CollectionChangedEventArgs& args)
			{ publicChanges.push_back(args); });

		values.push_back(30);
		CUI_EXPECT_EQ(CollectionChangeAction::Add, ownerChanges.back().Action);
		CUI_EXPECT_EQ(2ULL, ownerChanges.back().NewIndex);
		CUI_EXPECT_EQ(1ULL, ownerChanges.back().NewCount);
		values.insert(values.begin() + 1, 15);
		CUI_EXPECT_EQ(1ULL, ownerChanges.back().NewIndex);
		CUI_EXPECT_TRUE(values.Replace(2, 25));
		CUI_EXPECT_EQ(CollectionChangeAction::Replace, ownerChanges.back().Action);
		CUI_EXPECT_TRUE(values.Move(0, 3));
		CUI_EXPECT_EQ(CollectionChangeAction::Move, ownerChanges.back().Action);
		CUI_EXPECT_EQ(0ULL, ownerChanges.back().OldIndex);
		CUI_EXPECT_EQ(3ULL, ownerChanges.back().NewIndex);
		CUI_EXPECT_TRUE(values.SwapIndices(0, 3));
		CUI_EXPECT_EQ(CollectionChangeAction::Swap, ownerChanges.back().Action);
		values.erase(values.begin() + 1, values.begin() + 3);
		CUI_EXPECT_EQ(CollectionChangeAction::Remove, ownerChanges.back().Action);
		CUI_EXPECT_EQ(2ULL, ownerChanges.back().OldCount);

		const size_t beforeBatch = ownerChanges.size();
		{
			auto update = values.DeferNotifications();
			values.push_back(40);
			values.push_back(50);
			values.erase(values.begin());
			CUI_EXPECT_EQ(beforeBatch, ownerChanges.size());
		}
		CUI_EXPECT_EQ(beforeBatch + 1, ownerChanges.size());
		CUI_EXPECT_EQ(CollectionChangeAction::Reset, ownerChanges.back().Action);
		CUI_EXPECT_EQ(publicChanges.size(), ownerChanges.size());

		std::vector<int>& compatibleView = values;
		std::reverse(compatibleView.begin(), compatibleView.end());
		values.NotifyReset();
		CUI_EXPECT_EQ(CollectionChangeAction::Reset, ownerChanges.back().Action);
		std::vector<int> replacement{ 7, 8, 9 };
		values = replacement;
		CUI_EXPECT_EQ(3ULL, values.size());
		CUI_EXPECT_EQ(CollectionChangeAction::Reset, ownerChanges.back().Action);
	});

	runner.Add("Control children synchronize ownership under direct mutation", []
	{
		Control parent;
		size_t publicChanges = 0;
		std::vector<CollectionChangeAction> actions;
		auto connection = parent.Children.Changed.Subscribe(
			[&](ObservableCollection<Control*>*,
				const CollectionChangedEventArgs& change)
			{
				++publicChanges;
				actions.push_back(change.Action);
				for (auto* child : parent.Children)
					CUI_EXPECT_TRUE(child && child->Parent == &parent);
			});

		auto firstOwner = std::make_unique<Control>();
		auto* first = firstOwner.release();
		parent.Children.push_back(first);
		CUI_EXPECT_TRUE(first->Parent == &parent);
		CUI_EXPECT_EQ(CollectionChangeAction::Add, actions.back());

		auto secondOwner = std::make_unique<Control>();
		auto* second = secondOwner.release();
		parent.Children.insert(parent.Children.begin(), second);
		CUI_EXPECT_TRUE(second->Parent == &parent);
		CUI_EXPECT_TRUE(parent.Children.Move(0, 1));
		CUI_EXPECT_TRUE(parent.Children[1] == second);
		CUI_EXPECT_EQ(CollectionChangeAction::Move, actions.back());

		auto replacementOwner = std::make_unique<Control>();
		auto* replacement = replacementOwner.release();
		CUI_EXPECT_TRUE(parent.Children.Replace(1, replacement));
		std::unique_ptr<Control> detachedSecond(second);
		CUI_EXPECT_TRUE(second->Parent == nullptr);
		CUI_EXPECT_TRUE(replacement->Parent == &parent);

		const size_t beforeBatch = publicChanges;
		auto batchFirstOwner = std::make_unique<Control>();
		auto* batchFirst = batchFirstOwner.release();
		auto batchSecondOwner = std::make_unique<Control>();
		auto* batchSecond = batchSecondOwner.release();
		std::unique_ptr<Control> detachedFirst;
		{
			auto update = parent.Children.DeferNotifications();
			parent.Children.push_back(batchFirst);
			parent.Children.push_back(batchSecond);
			CUI_EXPECT_TRUE(batchFirst->Parent == &parent);
			CUI_EXPECT_TRUE(batchSecond->Parent == &parent);
			auto found = std::find(
				parent.Children.begin(), parent.Children.end(), first);
			CUI_EXPECT_TRUE(found != parent.Children.end());
			parent.Children.erase(found);
			CUI_EXPECT_TRUE(first->Parent == nullptr);
			detachedFirst.reset(first);
			CUI_EXPECT_EQ(beforeBatch, publicChanges);
		}
		CUI_EXPECT_EQ(beforeBatch + 1, publicChanges);
		CUI_EXPECT_EQ(CollectionChangeAction::Reset, actions.back());

		const size_t beforeRejected = publicChanges;
		const size_t sizeBeforeRejected = parent.Children.size();
		bool duplicateRejected = false;
		try
		{
			parent.Children.push_back(replacement);
		}
		catch (const std::logic_error&)
		{
			duplicateRejected = true;
		}
		CUI_EXPECT_TRUE(duplicateRejected);
		CUI_EXPECT_EQ(sizeBeforeRejected, parent.Children.size());
		CUI_EXPECT_EQ(beforeRejected, publicChanges);
		CUI_EXPECT_TRUE(replacement->Parent == &parent);
		const size_t beforeRejectedBatch = publicChanges;
		parent.Children.BeginUpdate();
		bool batchedDuplicateRejected = false;
		try
		{
			parent.Children.push_back(replacement);
		}
		catch (const std::logic_error&)
		{
			batchedDuplicateRejected = true;
		}
		parent.Children.EndUpdate();
		CUI_EXPECT_TRUE(batchedDuplicateRejected);
		CUI_EXPECT_EQ(beforeRejectedBatch, publicChanges);

		bool nullRejected = false;
		try
		{
			parent.Children.Replace(0, nullptr);
		}
		catch (const std::invalid_argument&)
		{
			nullRejected = true;
		}
		CUI_EXPECT_TRUE(nullRejected);
		CUI_EXPECT_EQ(sizeBeforeRejected, parent.Children.size());

		Control otherParent;
		bool otherOwnerRejected = false;
		try
		{
			otherParent.Children.push_back(replacement);
		}
		catch (const std::logic_error&)
		{
			otherOwnerRejected = true;
		}
		CUI_EXPECT_TRUE(otherOwnerRejected);
		CUI_EXPECT_TRUE(otherParent.Children.empty());
		CUI_EXPECT_TRUE(replacement->Parent == &parent);

		bool cycleRejected = false;
		try
		{
			batchFirst->Children.push_back(&parent);
		}
		catch (const std::logic_error&)
		{
			cycleRejected = true;
		}
		CUI_EXPECT_TRUE(cycleRejected);
		CUI_EXPECT_TRUE(batchFirst->Children.empty());

		std::vector<Control*>& vectorView = parent.Children;
		std::reverse(vectorView.begin(), vectorView.end());
		parent.Children.NotifyReset();
		CUI_EXPECT_EQ(CollectionChangeAction::Reset, actions.back());
		for (auto* child : parent.Children)
			CUI_EXPECT_TRUE(child->Parent == &parent);

		TabControl tabs(0, 0, 240, 160);
		auto* pageA = tabs.AddPage(L"A");
		auto* pageB = tabs.AddPage(L"B");
		ObservableObject tabSource;
		tabSource.SetValue(L"Index", 1);
		CUI_EXPECT_TRUE(tabs.DataBindings.Add(
			L"SelectedIndex", tabSource, L"Index", BindingMode::TwoWay)
			!= nullptr);
		auto insertedOwner = std::make_unique<TabPage>(L"Inserted");
		auto* inserted = insertedOwner.release();
		tabs.Pages.insert(tabs.Pages.begin(), inserted);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == pageB);
		CUI_EXPECT_EQ(2, tabs.SelectedIndex);
		CUI_EXPECT_EQ(2, tabSource.GetValue<int>(L"Index"));
		CUI_EXPECT_TRUE(tabs.Pages.Move(2, 0));
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == pageB);
		CUI_EXPECT_EQ(0, tabs.SelectedIndex);
		CUI_EXPECT_EQ(0, tabSource.GetValue<int>(L"Index"));
		tabs.Pages.erase(tabs.Pages.begin());
		std::unique_ptr<TabPage> detachedPage(pageB);
		CUI_EXPECT_TRUE(pageB->Parent == nullptr);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == inserted);
		CUI_EXPECT_TRUE(pageA->Parent == &tabs);

		auto invalidTabChild = std::make_unique<Control>();
		bool invalidTabChildRejected = false;
		try
		{
			tabs.Children.push_back(invalidTabChild.get());
		}
		catch (const std::logic_error&)
		{
			invalidTabChildRejected = true;
		}
		CUI_EXPECT_TRUE(invalidTabChildRejected);
		CUI_EXPECT_TRUE(invalidTabChild->Parent == nullptr);

		Control apiParent;
		auto* appended = apiParent.Add<Control>();
		auto* insertedByApi = apiParent.InsertOwned(
			0, std::make_unique<Control>());
		CUI_EXPECT_EQ(0, apiParent.IndexOfControl(insertedByApi));
		CUI_EXPECT_TRUE(apiParent.ContainsControl(appended));
		auto detachedByIndex = apiParent.DetachControlAt(0);
		CUI_EXPECT_TRUE(detachedByIndex.get() == insertedByApi);
		CUI_EXPECT_TRUE(insertedByApi->Parent == nullptr);
		CUI_EXPECT_TRUE(apiParent.DeleteControlAt(0));
		CUI_EXPECT_TRUE(apiParent.Children.empty());
		apiParent.Add<Control>();
		apiParent.Add<Control>();
		apiParent.ClearControls();
		CUI_EXPECT_TRUE(apiParent.Children.empty());
	});

	runner.Add("Form transactional root insertion preserves caller ownership", []
	{
		Form form(L"transactional roots", POINT{ 0, 0 }, SIZE{ 240, 120 });
		std::unique_ptr<Control> first = std::make_unique<Control>();
		auto* firstRaw = first.get();
		CUI_EXPECT_TRUE(form.TryInsertOwned(0, first));
		CUI_EXPECT_FALSE(first);
		CUI_EXPECT_EQ(0, form.IndexOfControl(firstRaw));
		CUI_EXPECT_TRUE(firstRaw->ParentForm == &form);

		auto detached = form.DetachControl(firstRaw);
		CUI_EXPECT_TRUE(detached.get() == firstRaw);
		CUI_EXPECT_TRUE(firstRaw->ParentForm == nullptr);
		CUI_EXPECT_FALSE(form.TryInsertOwned(2, detached));
		CUI_EXPECT_TRUE(detached.get() == firstRaw);
		CUI_EXPECT_TRUE(form.Controls.empty());

		std::unique_ptr<Control> prefix = std::make_unique<Control>();
		auto* prefixRaw = prefix.get();
		CUI_EXPECT_TRUE(form.TryInsertOwned(0, prefix));
		CUI_EXPECT_TRUE(form.TryInsertOwned(0, detached));
		CUI_EXPECT_EQ(0, form.IndexOfControl(firstRaw));
		CUI_EXPECT_EQ(1, form.IndexOfControl(prefixRaw));
		CUI_EXPECT_TRUE(form.DetachControl(firstRaw).get() == firstRaw);
		CUI_EXPECT_EQ(0, form.IndexOfControl(prefixRaw));
	});

	runner.Add("Collection-backed controls stay coherent under direct mutation", []
	{
		ComboBox combo(L"", 0, 0, 180, 28);
		size_t comboChanges = 0;
		auto comboConnection = combo.Items.Changed.Subscribe(
			[&](ComboBox::ItemCollection*, const CollectionChangedEventArgs&)
			{ ++comboChanges; });
		combo.Items.push_back(L"Alpha");
		combo.Items.push_back(L"Beta");
		CUI_EXPECT_TRUE(combo.SelectItem(1));
		std::vector<uint32_t> comboIds;
		combo.GetAccessibilityVirtualChildren(0, comboIds);
		CUI_EXPECT_EQ(2ULL, comboIds.size());
		const uint32_t betaId = comboIds[1];
		combo.Items.insert(combo.Items.begin(), L"Zero");
		CUI_EXPECT_EQ(2, combo.SelectedIndex);
		CUI_EXPECT_EQ(std::wstring(L"Beta"), combo.Text);
		combo.GetAccessibilityVirtualChildren(0, comboIds);
		CUI_EXPECT_EQ(betaId, comboIds[2]);
		CUI_EXPECT_TRUE(combo.Items.Move(2, 0));
		CUI_EXPECT_EQ(0, combo.SelectedIndex);
		combo.GetAccessibilityVirtualChildren(0, comboIds);
		CUI_EXPECT_EQ(betaId, comboIds[0]);
		const size_t comboBeforeBatch = comboChanges;
		{
			auto update = combo.Items.DeferNotifications();
			combo.Items.push_back(L"Gamma");
			combo.Items.push_back(L"Delta");
		}
		CUI_EXPECT_EQ(comboBeforeBatch + 1, comboChanges);
		CUI_EXPECT_EQ(std::wstring(L"Beta"), combo.Text);

		ListView list(0, 0, 240, 120);
		list.Items.push_back(ListViewItem(L"Alpha"));
		list.Items.push_back(ListViewItem(L"Beta"));
		CUI_EXPECT_TRUE(list.SelectItem(1));
		const uint32_t listBetaId = list.Items[1].AccessibilityId;
		list.Items.insert(list.Items.begin(), ListViewItem(L"Zero"));
		CUI_EXPECT_EQ(2, list.SelectedIndex);
		CUI_EXPECT_EQ(listBetaId, list.Items[2].AccessibilityId);
		CUI_EXPECT_TRUE(list.Items.Move(2, 0));
		CUI_EXPECT_EQ(0, list.SelectedIndex);
		CUI_EXPECT_EQ(listBetaId, list.Items[0].AccessibilityId);
		list.Items.erase(list.Items.begin());
		CUI_EXPECT_EQ(-1, list.SelectedIndex);

		GridView grid(0, 0, 300, 120);
		size_t columnChanges = 0;
		size_t rowChanges = 0;
		bool collectionObserversSawReadyAccessibilityIds = true;
		auto observeReadyAccessibilityIds = [&]
		{
			for (const auto& column : grid.Columns)
				collectionObserversSawReadyAccessibilityIds =
					collectionObserversSawReadyAccessibilityIds
					&& column.AccessibilityId != 0;
			for (const auto& row : grid.Rows)
				collectionObserversSawReadyAccessibilityIds =
					collectionObserversSawReadyAccessibilityIds
					&& row.AccessibilityId != 0;
		};
		auto columnConnection = grid.Columns.Changed.Subscribe(
			[&](GridView::ColumnCollection*, const CollectionChangedEventArgs&)
			{ ++columnChanges; observeReadyAccessibilityIds(); });
		auto rowConnection = grid.Rows.Changed.Subscribe(
			[&](GridView::RowCollection*, const CollectionChangedEventArgs&)
			{ ++rowChanges; observeReadyAccessibilityIds(); });
		grid.BeginUpdate();
		grid.Columns.push_back(GridViewColumn(
			L"A", 100.0f, ColumnType::Text, true));
		grid.Columns.push_back(GridViewColumn(
			L"B", 100.0f, ColumnType::Text, true));
		GridViewRow firstRow;
		firstRow.Cells = { CellValue(L"A1"), CellValue(L"B1") };
		GridViewRow secondRow;
		secondRow.Cells = { CellValue(L"A2"), CellValue(L"B2") };
		grid.Rows.push_back(firstRow);
		grid.Rows.push_back(secondRow);
		grid.EndUpdate();
		CUI_EXPECT_EQ(1ULL, columnChanges);
		CUI_EXPECT_EQ(1ULL, rowChanges);
		CUI_EXPECT_TRUE(collectionObserversSawReadyAccessibilityIds);
		CUI_EXPECT_TRUE(grid.SelectCell(1, 1));
		const uint32_t selectedRowId = grid.Rows[1].AccessibilityId;
		const uint32_t selectedColumnId = grid.Columns[1].AccessibilityId;
		CUI_EXPECT_TRUE(grid.Rows.Move(1, 0));
		CUI_EXPECT_EQ(0, grid.SelectedRowIndex);
		CUI_EXPECT_EQ(selectedRowId, grid.Rows[0].AccessibilityId);
		CUI_EXPECT_TRUE(grid.Columns.Move(1, 0));
		CUI_EXPECT_EQ(0, grid.SelectedColumnIndex);
		CUI_EXPECT_EQ(selectedColumnId, grid.Columns[0].AccessibilityId);
		CUI_EXPECT_EQ(std::wstring(L"B2"), grid.GetCell(0, 0)->Text);
		grid.Columns.insert(grid.Columns.begin() + 1,
			GridViewColumn(L"C", 80.0f, ColumnType::Text, true));
		CUI_EXPECT_EQ(3ULL, grid.Rows[0].Cells.size());
		CUI_EXPECT_EQ(std::wstring(L"A2"), grid.GetCell(2, 0)->Text);
		grid.Columns.erase(grid.Columns.begin());
		CUI_EXPECT_EQ(0, grid.SelectedColumnIndex);
		CUI_EXPECT_EQ(2ULL, grid.Rows[0].Cells.size());
		grid.Rows.Sort([](const GridViewRow& left, const GridViewRow& right)
		{
			return left.Cells[1].Text < right.Cells[1].Text;
		});
		CUI_EXPECT_EQ(1, grid.SelectedRowIndex);
		CUI_EXPECT_EQ(selectedRowId, grid.Rows[1].AccessibilityId);
		const size_t columnsBeforeReorderBatch = columnChanges;
		const size_t rowsBeforeReorderBatch = rowChanges;
		grid.BeginUpdate();
		CUI_EXPECT_TRUE(grid.Columns.Move(0, 1));
		CUI_EXPECT_TRUE(grid.Rows.Move(1, 0));
		grid.EndUpdate();
		CUI_EXPECT_EQ(columnsBeforeReorderBatch + 1, columnChanges);
		CUI_EXPECT_EQ(rowsBeforeReorderBatch + 1, rowChanges);
		CUI_EXPECT_EQ(1, grid.SelectedColumnIndex);
		CUI_EXPECT_EQ(0, grid.SelectedRowIndex);
		CUI_EXPECT_EQ(selectedRowId, grid.Rows[0].AccessibilityId);
		CUI_EXPECT_EQ(std::wstring(L"A2"), grid.GetCell(0, 0)->Text);

		TreeView tree(0, 0, 240, 120);
		size_t rootChanges = 0;
		auto rootConnection = tree.Root->Children.Changed.Subscribe(
			[&](TreeNode::ChildCollection*, const CollectionChangedEventArgs&)
			{ ++rootChanges; });
		auto* parent = tree.Root->AddChild(
			std::make_unique<TreeNode>(L"Parent"));
		CUI_EXPECT_TRUE(parent != nullptr);
		CUI_EXPECT_EQ(1ULL, rootChanges);
		size_t childChanges = 0;
		auto childConnection = parent->Children.Changed.Subscribe(
			[&](TreeNode::ChildCollection*, const CollectionChangedEventArgs&)
			{ ++childChanges; });
		auto* child = new TreeNode(L"Child");
		const uint32_t childId = child->AccessibilityId;
		parent->Children.push_back(child);
		CUI_EXPECT_EQ(1ULL, childChanges);
		std::vector<uint32_t> treeChildren;
		tree.GetAccessibilityVirtualChildren(parent->AccessibilityId, treeChildren);
		CUI_EXPECT_EQ(1ULL, treeChildren.size());
		CUI_EXPECT_EQ(childId, treeChildren[0]);
		CUI_EXPECT_TRUE(tree.SelectAccessibilityVirtualNode(
			childId, AccessibilitySelectionAction::Select));
		CUI_EXPECT_EQ(child, tree.SelectedNode);
		auto detached = parent->DetachChildAt(0);
		CUI_EXPECT_TRUE(detached != nullptr);
		CUI_EXPECT_EQ(nullptr, tree.SelectedNode);
		AccessibilityVirtualNode removedNode;
		CUI_EXPECT_FALSE(tree.TryGetAccessibilityVirtualNode(
			childId, removedNode));
		CUI_EXPECT_EQ(childId,
			parent->AddChild(std::move(detached))->AccessibilityId);
		CUI_EXPECT_TRUE(tree.TryGetAccessibilityVirtualNode(
			childId, removedNode));
		CUI_EXPECT_EQ(nullptr, parent->AddChild(parent));
		CUI_EXPECT_TRUE(parent->RemoveChildAt(0));
	});

	runner.Add("Virtual containers expose scroll and Details table semantics", []
	{
		auto hasPattern = [](AccessibilityVirtualPattern value,
			AccessibilityVirtualPattern pattern)
		{
			return HasAccessibilityVirtualPattern(value, pattern);
		};

		ListView details(0, 0, 180, 90);
		details.ViewMode = ListViewViewMode::Details;
		details.HeaderHeight = 30.0f;
		details.RowHeight = 30.0f;
		details.AddColumn(ListViewColumn(L"Name", 100.0f));
		details.AddColumn(ListViewColumn(L"Status", 100.0f));
		for (int index = 0; index < 6; ++index)
		{
			ListViewItem item(L"Item " + std::to_wstring(index));
			item.SubItems.push_back(index % 2 == 0 ? L"Ready" : L"Busy");
			details.AddItem(item);
		}
		const auto detailsInfo = details.GetAccessibilityVirtualContainerInfo();
		CUI_EXPECT_TRUE(hasPattern(detailsInfo.Patterns,
			AccessibilityVirtualPattern::Selection));
		CUI_EXPECT_TRUE(hasPattern(detailsInfo.Patterns,
			AccessibilityVirtualPattern::Grid));
		CUI_EXPECT_TRUE(hasPattern(detailsInfo.Patterns,
			AccessibilityVirtualPattern::Table));
		CUI_EXPECT_TRUE(hasPattern(detailsInfo.Patterns,
			AccessibilityVirtualPattern::Scroll));
		CUI_EXPECT_EQ(6, detailsInfo.RowCount);
		CUI_EXPECT_EQ(2, detailsInfo.ColumnCount);

		std::vector<uint32_t> headers;
		details.GetAccessibilityVirtualColumnHeaders(headers);
		CUI_EXPECT_EQ(2ULL, headers.size());
		AccessibilityVirtualNode headerNode;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualNode(
			headers[1], headerNode));
		CUI_EXPECT_EQ(AccessibleRole::HeaderItem, headerNode.Role);
		CUI_EXPECT_EQ(std::wstring(L"Status"), headerNode.Name);

		uint32_t statusCellId = 0;
		CUI_EXPECT_TRUE(details.GetAccessibilityVirtualItemAt(
			0, 1, statusCellId));
		AccessibilityVirtualNode statusCell;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualNode(
			statusCellId, statusCell));
		CUI_EXPECT_TRUE(hasPattern(statusCell.Patterns,
			AccessibilityVirtualPattern::GridItem));
		CUI_EXPECT_TRUE(hasPattern(statusCell.Patterns,
			AccessibilityVirtualPattern::TableItem));
		CUI_EXPECT_EQ(0, statusCell.Row);
		CUI_EXPECT_EQ(1, statusCell.Column);
		CUI_EXPECT_EQ(std::wstring(L"Ready"), statusCell.Value);
		CUI_EXPECT_EQ(details.Items[0].AccessibilityId, statusCell.ParentId);
		std::vector<uint32_t> rowChildren;
		details.GetAccessibilityVirtualChildren(
			details.Items[0].AccessibilityId, rowChildren);
		CUI_EXPECT_EQ(2ULL, rowChildren.size());
		CUI_EXPECT_EQ(statusCellId, rowChildren[1]);

		const uint32_t firstRowId = details.Items[0].AccessibilityId;
		CUI_EXPECT_TRUE(details.Items.Move(0, 3));
		uint32_t movedCellId = 0;
		CUI_EXPECT_TRUE(details.GetAccessibilityVirtualItemAt(3, 1, movedCellId));
		CUI_EXPECT_EQ(statusCellId, movedCellId);
		CUI_EXPECT_EQ(firstRowId, details.Items[3].AccessibilityId);
		CUI_EXPECT_TRUE(details.Columns.Move(1, 0));
		CUI_EXPECT_TRUE(details.GetAccessibilityVirtualItemAt(3, 0, movedCellId));
		CUI_EXPECT_EQ(statusCellId, movedCellId);
		details.Columns.erase(details.Columns.begin());
		AccessibilityVirtualNode removedCell;
		CUI_EXPECT_FALSE(details.TryGetAccessibilityVirtualNode(
			statusCellId, removedCell));

		AccessibilityScrollInfo scrollInfo;
		CUI_EXPECT_TRUE(details.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_FALSE(scrollInfo.HorizontallyScrollable);
		CUI_EXPECT_TRUE(scrollInfo.VerticallyScrollable);
		CUI_EXPECT_EQ(AccessibilityScrollNoChange,
			scrollInfo.HorizontalScrollPercent);
		CUI_EXPECT_NEAR(0.0, scrollInfo.VerticalScrollPercent, 0.000001);
		CUI_EXPECT_TRUE(details.ScrollAccessibility(
			AccessibilityScrollAmount::NoAmount,
			AccessibilityScrollAmount::SmallIncrement));
		CUI_EXPECT_TRUE(details.ScrollYOffset > 0.0f);
		CUI_EXPECT_TRUE(details.SetAccessibilityScrollPercent(
			AccessibilityScrollNoChange, 100.0));
		CUI_EXPECT_TRUE(details.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_NEAR(100.0, scrollInfo.VerticalScrollPercent, 0.000001);
		CUI_EXPECT_FALSE(details.SetAccessibilityScrollPercent(10.0, 50.0));

		ComboBox combo(L"", 0, 0, 160, 28);
		combo.ExpandCount = 2;
		combo.Items = std::vector<std::wstring>{
			L"One", L"Two", L"Three", L"Four", L"Five" };
		CUI_EXPECT_TRUE(combo.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_TRUE(scrollInfo.VerticallyScrollable);
		CUI_EXPECT_TRUE(combo.ScrollAccessibility(
			AccessibilityScrollAmount::NoAmount,
			AccessibilityScrollAmount::LargeIncrement));
		CUI_EXPECT_TRUE(combo.Expand);
		CUI_EXPECT_EQ(2, combo.ExpandScroll);
		CUI_EXPECT_TRUE(combo.SetAccessibilityScrollPercent(
			AccessibilityScrollNoChange, 100.0));
		CUI_EXPECT_EQ(3, combo.ExpandScroll);

		TreeView tree(0, 0, 160, 56);
		for (int index = 0; index < 8; ++index)
			tree.Root->AddChild(std::make_unique<TreeNode>(
				L"Node " + std::to_wstring(index)));
		CUI_EXPECT_TRUE(tree.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_TRUE(scrollInfo.VerticallyScrollable);
		CUI_EXPECT_TRUE(tree.ScrollAccessibility(
			AccessibilityScrollAmount::NoAmount,
			AccessibilityScrollAmount::LargeIncrement));
		CUI_EXPECT_EQ(2, tree.ScrollIndex);
		CUI_EXPECT_TRUE(tree.SetAccessibilityScrollPercent(
			AccessibilityScrollNoChange, 100.0));
		CUI_EXPECT_EQ(6, tree.ScrollIndex);

		GridView grid(0, 0, 180, 90);
		for (int column = 0; column < 3; ++column)
			grid.AddColumn(GridViewColumn(
				L"Column " + std::to_wstring(column), 100.0f,
				ColumnType::Text, true));
		for (int rowIndex = 0; rowIndex < 10; ++rowIndex)
		{
			GridViewRow row;
			for (int column = 0; column < 3; ++column)
				row.Cells.emplace_back(L"Cell");
			grid.AddRow(row);
		}
		CUI_EXPECT_TRUE(grid.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_TRUE(scrollInfo.HorizontallyScrollable);
		CUI_EXPECT_TRUE(scrollInfo.VerticallyScrollable);
		CUI_EXPECT_TRUE(grid.ScrollAccessibility(
			AccessibilityScrollAmount::SmallIncrement,
			AccessibilityScrollAmount::SmallIncrement));
		CUI_EXPECT_TRUE(grid.ScrollXOffset > 0.0f);
		CUI_EXPECT_TRUE(grid.ScrollYOffset > 0.0f);
		CUI_EXPECT_TRUE(grid.SetAccessibilityScrollPercent(100.0, 100.0));
		CUI_EXPECT_TRUE(grid.GetAccessibilityScrollInfo(scrollInfo));
		CUI_EXPECT_NEAR(100.0, scrollInfo.HorizontalScrollPercent, 0.000001);
		CUI_EXPECT_NEAR(100.0, scrollInfo.VerticalScrollPercent, 0.000001);
	});

	runner.Add("ListView visible ranges cover only viewport candidates", []
	{
		std::vector<ListViewItem> items;
		for (int index = 0; index < 100; ++index)
			items.emplace_back(L"Item " + std::to_wstring(index));

		ListView list(0, 0, 240, 180);
		list.RowHeight = 40.0f;
		list.SetItems(items);
		int start = -1;
		int end = -1;
		list.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(0, start);
		CUI_EXPECT_EQ(5, end);
		list.SetScrollOffset(80.0f);
		list.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(2, start);
		CUI_EXPECT_EQ(7, end);
		CUI_EXPECT_EQ(2, list.HitTestItem(10, 0));
		CUI_EXPECT_EQ(6, list.HitTestItem(10, 179));

		ListView details(0, 0, 240, 180);
		details.ViewMode = ListViewViewMode::Details;
		details.RowHeight = 40.0f;
		details.SetItems(items);
		details.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(0, start);
		CUI_EXPECT_EQ(4, end);
		details.SetScrollOffset(80.0f);
		details.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(2, start);
		CUI_EXPECT_EQ(6, end);

		ListView tiles(0, 0, 240, 180);
		tiles.ViewMode = ListViewViewMode::Tile;
		tiles.TileHeight = 50.0f;
		tiles.IconSize = 16.0f;
		tiles.SetItems(items);
		tiles.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(0, start);
		CUI_EXPECT_EQ(4, end);
		tiles.SetScrollOffset(50.0f);
		tiles.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(1, start);
		CUI_EXPECT_EQ(5, end);

		ListView icons(0, 0, 240, 180);
		icons.ViewMode = ListViewViewMode::Icon;
		icons.IconItemWidth = 80.0f;
		icons.IconItemHeight = 60.0f;
		icons.IconSize = 16.0f;
		icons.ItemGap = 8.0f;
		icons.SetItems(items);
		icons.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(0, start);
		CUI_EXPECT_EQ(6, end);
		CUI_EXPECT_EQ(0, icons.HitTestItem(10, 10));
		CUI_EXPECT_EQ(1, icons.HitTestItem(90, 10));
		CUI_EXPECT_EQ(-1, icons.HitTestItem(80, 10));
		icons.SetScrollOffset(60.0f);
		icons.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(2, start);
		CUI_EXPECT_EQ(8, end);
		CUI_EXPECT_EQ(2, icons.HitTestItem(10, 10));
		CUI_EXPECT_EQ(3, icons.HitTestItem(90, 10));

		ListBox listBox(0, 0, 240, 180);
		listBox.ViewMode = ListViewViewMode::Icon;
		listBox.RowHeight = 40.0f;
		listBox.SetItems(items);
		listBox.GetVisibleItemRange(start, end);
		CUI_EXPECT_EQ(0, start);
		CUI_EXPECT_EQ(5, end);
	});

	runner.Add("ListView incremental batches keep identity and selection bounded", []
	{
		ListView list(0, 0, 320, 160);
		list.ViewMode = ListViewViewMode::Details;
		list.AddItem(ListViewItem(L"Selected"));
		CUI_EXPECT_TRUE(list.SelectItem(0));
		const uint32_t selectedId = list.Items[0].AccessibilityId;
		CUI_EXPECT_TRUE(selectedId != 0);

		size_t itemChanges = 0;
		size_t columnChanges = 0;
		bool observerSawReadyIndex = false;
		bool columnObserverSawReset = false;
		auto itemConnection = list.Items.Changed.Subscribe(
			[&](ListView::ItemCollection*, const CollectionChangedEventArgs& change)
			{
				++itemChanges;
				uint32_t lastId = 0;
				observerSawReadyIndex = change.Action == CollectionChangeAction::Reset
					&& list.SelectedIndex == 0
					&& list.TryGetAccessibilityVirtualChildAt(
						0, list.Columns.size() + list.Items.size() - 1, lastId)
					&& lastId == list.Items.back().AccessibilityId;
			});
		auto columnConnection = list.Columns.Changed.Subscribe(
			[&](ListView::ColumnCollection*, const CollectionChangedEventArgs& change)
			{
				++columnChanges;
				columnObserverSawReset =
					change.Action == CollectionChangeAction::Reset;
			});

		constexpr int appendCount = 4096;
		ListViewItem duplicateIdentity = list.Items[0];
		duplicateIdentity.Selected = false;
		{
			auto update = list.DeferUpdates();
			list.AddColumn(ListViewColumn(L"Name", 160.0f));
			list.AddColumn(ListViewColumn(L"Value", 120.0f));
			for (int index = 0; index < appendCount; ++index)
			{
				list.AddItem(ListViewItem(
					L"Item " + std::to_wstring(index)));
				CUI_EXPECT_EQ(1ULL,
					list.LastAccessibilityIndexUpdateWork());
				CUI_EXPECT_TRUE(list.LastSelectionUpdateWork() <= 1);
			}
			list.AddItem(duplicateIdentity);
			CUI_EXPECT_EQ(1ULL,
				list.LastAccessibilityIndexUpdateWork());
			CUI_EXPECT_TRUE(list.Items.back().AccessibilityId != selectedId);
			CUI_EXPECT_EQ(0ULL, itemChanges);
			CUI_EXPECT_EQ(0ULL, columnChanges);
			CUI_EXPECT_TRUE(list.IsUpdating());
			CUI_EXPECT_EQ(selectedId, list.Items[0].AccessibilityId);
		}
		CUI_EXPECT_FALSE(list.IsUpdating());
		CUI_EXPECT_EQ(1ULL, itemChanges);
		CUI_EXPECT_EQ(1ULL, columnChanges);
		CUI_EXPECT_TRUE(observerSawReadyIndex);
		CUI_EXPECT_TRUE(columnObserverSawReset);
		CUI_EXPECT_EQ(0, list.SelectedIndex);
		CUI_EXPECT_EQ(selectedId, list.Items[0].AccessibilityId);
		const size_t beforeNestedBatch = itemChanges;
		list.BeginUpdate();
		list.BeginUpdate();
		list.AddItem(ListViewItem(L"Nested"));
		list.EndUpdate();
		CUI_EXPECT_TRUE(list.IsUpdating());
		CUI_EXPECT_EQ(beforeNestedBatch, itemChanges);
		list.EndUpdate();
		CUI_EXPECT_FALSE(list.IsUpdating());
		CUI_EXPECT_EQ(beforeNestedBatch + 1, itemChanges);

		uint32_t selectedCellId = 0;
		CUI_EXPECT_TRUE(list.GetAccessibilityVirtualItemAt(
			0, 1, selectedCellId));
		CUI_EXPECT_EQ(1ULL, list.MaterializedAccessibilityCellCount());

		const size_t itemCount = list.Items.size();
		CUI_EXPECT_TRUE(list.Items.Move(0, itemCount - 1));
		CUI_EXPECT_EQ(static_cast<int>(itemCount - 1), list.SelectedIndex);
		CUI_EXPECT_EQ(selectedId, list.Items.back().AccessibilityId);
		uint32_t movedCellId = 0;
		CUI_EXPECT_TRUE(list.GetAccessibilityVirtualItemAt(
			static_cast<int>(itemCount - 1), 1, movedCellId));
		CUI_EXPECT_EQ(selectedCellId, movedCellId);
		CUI_EXPECT_EQ(itemCount, list.LastAccessibilityIndexUpdateWork());
		CUI_EXPECT_TRUE(list.LastSelectionUpdateWork() <= 1);

		CUI_EXPECT_TRUE(list.Items.SwapIndices(itemCount - 1, 0));
		CUI_EXPECT_EQ(0, list.SelectedIndex);
		CUI_EXPECT_EQ(selectedId, list.Items[0].AccessibilityId);
		CUI_EXPECT_EQ(2ULL, list.LastAccessibilityIndexUpdateWork());
		CUI_EXPECT_TRUE(list.LastSelectionUpdateWork() <= 1);

		list.Items.insert(list.Items.begin(), ListViewItem(L"Prefix"));
		CUI_EXPECT_EQ(1, list.SelectedIndex);
		CUI_EXPECT_EQ(selectedId, list.Items[1].AccessibilityId);
		CUI_EXPECT_EQ(list.Items.size(),
			list.LastAccessibilityIndexUpdateWork());
		CUI_EXPECT_TRUE(list.RemoveItemAt(1));
		CUI_EXPECT_EQ(-1, list.SelectedIndex);
		AccessibilityVirtualNode removed;
		CUI_EXPECT_FALSE(list.TryGetAccessibilityVirtualNode(
			selectedId, removed));
		CUI_EXPECT_FALSE(list.TryGetAccessibilityVirtualNode(
			selectedCellId, removed));
		CUI_EXPECT_EQ(0ULL, list.MaterializedAccessibilityCellCount());
		const uint32_t replacedId = list.Items[5].AccessibilityId;
		CUI_EXPECT_TRUE(list.Items.Replace(
			5, ListViewItem(L"Replacement")));
		CUI_EXPECT_EQ(1ULL, list.LastAccessibilityIndexUpdateWork());
		CUI_EXPECT_TRUE(list.Items[5].AccessibilityId != 0);
		CUI_EXPECT_TRUE(list.Items[5].AccessibilityId != replacedId);
		CUI_EXPECT_FALSE(list.TryGetAccessibilityVirtualNode(
			replacedId, removed));

		ListView directFlags(0, 0, 240, 120);
		std::vector<ListViewItem> directValues;
		for (int index = 0; index < 32; ++index)
			directValues.emplace_back(L"Direct " + std::to_wstring(index));
		directFlags.SetItems(std::move(directValues));
		directFlags.Items[10].Selected = true;
		directFlags.Items.NotifyReset();
		CUI_EXPECT_EQ(10, directFlags.SelectedIndex);
		CUI_EXPECT_EQ(1ULL, directFlags.GetSelectedIndices().size());
		CUI_EXPECT_EQ(directFlags.Items.size(),
			directFlags.LastAccessibilityIndexUpdateWork());
		CUI_EXPECT_EQ(directFlags.Items.size(),
			directFlags.LastSelectionUpdateWork());

		ListView multiple(0, 0, 240, 120);
		multiple.SelectionMode = ListViewSelectionMode::Multiple;
		std::vector<ListViewItem> values{
			ListViewItem(L"Zero"), ListViewItem(L"One"),
			ListViewItem(L"Two"), ListViewItem(L"Three") };
		values[1].Selected = true;
		values[3].Selected = true;
		multiple.SetItems(std::move(values));
		const uint32_t threeId = multiple.Items[3].AccessibilityId;
		CUI_EXPECT_EQ(1, multiple.SelectedIndex);
		CUI_EXPECT_TRUE(multiple.Items.Move(3, 0));
		CUI_EXPECT_EQ(0, multiple.SelectedIndex);
		CUI_EXPECT_EQ(threeId, multiple.Items[0].AccessibilityId);
		CUI_EXPECT_EQ(2ULL, multiple.GetSelectedIndices().size());
	});

	runner.Add("Indexed virtual queries stay coherent on large collections", []
	{
		constexpr size_t listRowCount = 12000;
		constexpr size_t listColumnCount = 6;
		ListView details(0, 0, 480, 180);
		details.ViewMode = ListViewViewMode::Details;
		for (size_t column = 0; column < listColumnCount; ++column)
			details.AddColumn(ListViewColumn(
				L"Column " + std::to_wstring(column), 80.0f));
		std::vector<ListViewItem> items;
		items.reserve(listRowCount);
		for (size_t row = 0; row < listRowCount; ++row)
		{
			ListViewItem item(L"Row " + std::to_wstring(row));
			for (size_t column = 1; column < listColumnCount; ++column)
				item.SubItems.push_back(L"Value " + std::to_wstring(column));
			items.push_back(std::move(item));
		}
		details.SetItems(items);
		details.RowHeight = 40.0f;
		int visibleStart = -1;
		int visibleEnd = -1;
		details.GetVisibleItemRange(visibleStart, visibleEnd);
		CUI_EXPECT_EQ(0, visibleStart);
		CUI_EXPECT_EQ(4, visibleEnd);
		details.SetScrollOffset(400000.0f);
		details.GetVisibleItemRange(visibleStart, visibleEnd);
		CUI_EXPECT_TRUE(visibleStart > 0);
		CUI_EXPECT_TRUE(visibleEnd - visibleStart <= 4);
		details.SetScrollOffset(0.0f);
		CUI_EXPECT_EQ(0ULL, details.MaterializedAccessibilityCellCount());
		CUI_EXPECT_EQ(listRowCount + listColumnCount,
			details.GetAccessibilityVirtualChildCount(0));
		CUI_EXPECT_EQ(0ULL, details.MaterializedAccessibilityCellCount());
		uint32_t firstRowId = 0;
		uint32_t lastRowId = 0;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualChildAt(
			0, listColumnCount, firstRowId));
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualChildAt(
			0, listColumnCount + listRowCount - 1, lastRowId));
		uint32_t previousRowId = 0;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualSibling(
			0, lastRowId, false, previousRowId));
		CUI_EXPECT_EQ(details.Items[listRowCount - 2].AccessibilityId,
			previousRowId);
		uint32_t lastCellId = 0;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualChildAt(
			lastRowId, listColumnCount - 1, lastCellId));
		CUI_EXPECT_EQ(1ULL, details.MaterializedAccessibilityCellCount());
		AccessibilityVirtualNode lastCell;
		CUI_EXPECT_TRUE(details.TryGetAccessibilityVirtualNode(
			lastCellId, lastCell));
		CUI_EXPECT_EQ(static_cast<int>(listRowCount - 1), lastCell.Row);
		CUI_EXPECT_EQ(static_cast<int>(listColumnCount - 1), lastCell.Column);
		CUI_EXPECT_TRUE(details.Items.Move(listRowCount - 1, 0));
		uint32_t movedCellId = 0;
		CUI_EXPECT_TRUE(details.GetAccessibilityVirtualItemAt(
			0, static_cast<int>(listColumnCount - 1), movedCellId));
		CUI_EXPECT_EQ(lastCellId, movedCellId);
		CUI_EXPECT_TRUE(details.RemoveItemAt(0));
		CUI_EXPECT_FALSE(details.TryGetAccessibilityVirtualNode(
			lastCellId, lastCell));

		constexpr size_t comboItemCount = 12000;
		ComboBox combo(L"", 0, 0, 220, 28);
		std::vector<std::wstring> choices;
		choices.reserve(comboItemCount);
		for (size_t index = 0; index < comboItemCount; ++index)
			choices.push_back(L"Choice " + std::to_wstring(index));
		combo.Items = choices;
		uint32_t originalFirstChoice = 0;
		uint32_t originalLastChoice = 0;
		CUI_EXPECT_TRUE(combo.TryGetAccessibilityVirtualChildAt(
			0, 0, originalFirstChoice));
		CUI_EXPECT_TRUE(combo.TryGetAccessibilityVirtualChildAt(
			0, comboItemCount - 1, originalLastChoice));
		std::reverse(choices.begin(), choices.end());
		combo.Items = choices;
		uint32_t reversedFirstChoice = 0;
		uint32_t reversedLastChoice = 0;
		CUI_EXPECT_TRUE(combo.TryGetAccessibilityVirtualChildAt(
			0, 0, reversedFirstChoice));
		CUI_EXPECT_TRUE(combo.TryGetAccessibilityVirtualChildAt(
			0, comboItemCount - 1, reversedLastChoice));
		CUI_EXPECT_EQ(originalLastChoice, reversedFirstChoice);
		CUI_EXPECT_EQ(originalFirstChoice, reversedLastChoice);
		combo.Items.erase(combo.Items.begin());
		AccessibilityVirtualNode removedChoice;
		CUI_EXPECT_FALSE(combo.TryGetAccessibilityVirtualNode(
			originalLastChoice, removedChoice));

		constexpr size_t treeNodeCount = 5000;
		TreeView tree(0, 0, 240, 140);
		std::vector<TreeNode*>& rawNodes = tree.Root->Children;
		rawNodes.reserve(treeNodeCount);
		for (size_t index = 0; index < treeNodeCount; ++index)
			rawNodes.push_back(new TreeNode(L"Node " + std::to_wstring(index)));
		tree.Root->Children.NotifyReset();
		CUI_EXPECT_EQ(treeNodeCount,
			tree.GetAccessibilityVirtualChildCount(0));
		uint32_t lastTreeId = 0;
		CUI_EXPECT_TRUE(tree.TryGetAccessibilityVirtualChildAt(
			0, treeNodeCount - 1, lastTreeId));
		AccessibilityVirtualNode lastTreeNode;
		CUI_EXPECT_TRUE(tree.TryGetAccessibilityVirtualNode(
			lastTreeId, lastTreeNode));
		CUI_EXPECT_EQ(static_cast<int>(treeNodeCount - 1), lastTreeNode.Row);
		CUI_EXPECT_TRUE(tree.Root->Children.Move(treeNodeCount - 1, 0));
		CUI_EXPECT_TRUE(tree.TryGetAccessibilityVirtualNode(
			lastTreeId, lastTreeNode));
		CUI_EXPECT_EQ(0, lastTreeNode.Row);
		uint32_t hitTreeId = 0;
		CUI_EXPECT_TRUE(tree.TryHitTestAccessibilityVirtualNode(
			10.0f, 10.0f, hitTreeId));
		CUI_EXPECT_EQ(lastTreeId, hitTreeId);
		CUI_EXPECT_TRUE(tree.Root->RemoveChildAt(0));
		CUI_EXPECT_FALSE(tree.TryGetAccessibilityVirtualNode(
			lastTreeId, lastTreeNode));

		constexpr size_t gridRowCount = 3000;
		constexpr size_t gridColumnCount = 6;
		GridView grid(0, 0, 480, 180);
		grid.BeginUpdate();
		for (size_t column = 0; column < gridColumnCount; ++column)
			grid.AddColumn(GridViewColumn(
				L"Grid " + std::to_wstring(column), 80.0f,
				ColumnType::Text, true));
		for (size_t row = 0; row < gridRowCount; ++row)
		{
			GridViewRow value;
			for (size_t column = 0; column < gridColumnCount; ++column)
				value.Cells.emplace_back(L"Cell");
			grid.AddRow(value);
		}
		grid.EndUpdate();
		CUI_EXPECT_EQ(0ULL, grid.MaterializedAccessibilityCellCount());
		CUI_EXPECT_EQ(gridRowCount + gridColumnCount,
			grid.GetAccessibilityVirtualChildCount(0));
		CUI_EXPECT_EQ(0ULL, grid.MaterializedAccessibilityCellCount());
		uint32_t gridCellId = 0;
		CUI_EXPECT_TRUE(grid.GetAccessibilityVirtualItemAt(
			static_cast<int>(gridRowCount - 1),
			static_cast<int>(gridColumnCount - 1), gridCellId));
		CUI_EXPECT_EQ(1ULL, grid.MaterializedAccessibilityCellCount());
		AccessibilityVirtualNode gridCell;
		CUI_EXPECT_TRUE(grid.TryGetAccessibilityVirtualNode(gridCellId, gridCell));
		CUI_EXPECT_EQ(static_cast<int>(gridRowCount - 1), gridCell.Row);
		CUI_EXPECT_EQ(static_cast<int>(gridColumnCount - 1), gridCell.Column);
		CUI_EXPECT_TRUE(grid.Rows.Move(gridRowCount - 1, 0));
		uint32_t movedGridCellId = 0;
		CUI_EXPECT_TRUE(grid.GetAccessibilityVirtualItemAt(
			0, static_cast<int>(gridColumnCount - 1), movedGridCellId));
		CUI_EXPECT_EQ(gridCellId, movedGridCellId);
		CUI_EXPECT_TRUE(grid.Columns.Move(gridColumnCount - 1, 0));
		CUI_EXPECT_TRUE(grid.GetAccessibilityVirtualItemAt(
			0, 0, movedGridCellId));
		CUI_EXPECT_EQ(gridCellId, movedGridCellId);
		bool removalObserverSawPrunedCells = false;
		auto gridRowsConnection = grid.Rows.Changed.Subscribe(
			[&](GridView::RowCollection*, const CollectionChangedEventArgs& change)
			{
				if (change.Action == CollectionChangeAction::Remove)
					removalObserverSawPrunedCells =
						grid.MaterializedAccessibilityCellCount() == 0;
			});
		CUI_EXPECT_TRUE(grid.RemoveRowAt(0));
		CUI_EXPECT_TRUE(removalObserverSawPrunedCells);
		CUI_EXPECT_FALSE(grid.TryGetAccessibilityVirtualNode(
			gridCellId, gridCell));
		CUI_EXPECT_EQ(0ULL, grid.MaterializedAccessibilityCellCount());
	});

	 runner.Add("Binding metadata supports arbitrary value types", []
    {
        ObservableObject source;
        source.SetValue(L"Payload", BindingPayload{ 7, L"initial" });

        MetadataBindingControl target;
        Binding* binding = target.DataBindings.Add(
            L"Payload", source, L"Payload", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ((BindingPayload{ 7, L"initial" }), target.Payload);

        source.SetValue(L"Payload", BindingPayload{ 8, L"source" });
        CUI_EXPECT_EQ((BindingPayload{ 8, L"source" }), target.Payload);

        target.SetPayload(BindingPayload{ 9, L"target" });
        CUI_EXPECT_EQ(
            (BindingPayload{ 9, L"target" }),
            source.GetValue<BindingPayload>(L"Payload"));
    });

	runner.Add("Control property metadata coerces defaults and invalidates layout", []
	{
		using namespace cui::core;

		PropertySystemControl target;
		const auto* metadata = target.FindPropertyMetadata(L"Level");
		CUI_EXPECT_TRUE(metadata != nullptr);
		CUI_EXPECT_TRUE(metadata->HasDefaultValue());
		CUI_EXPECT_TRUE(HasControlPropertyFlag(
			metadata->Flags(), ControlPropertyFlags::AffectsMeasure));
		CUI_EXPECT_TRUE(HasControlPropertyFlag(
			metadata->Flags(), ControlPropertyFlags::AffectsRender));

		BindingValue defaultValue;
		int typedDefault = 0;
		CUI_EXPECT_TRUE(metadata->TryGetDefaultValue(defaultValue));
		CUI_EXPECT_TRUE(defaultValue.TryGet(typedDefault));
		CUI_EXPECT_EQ(4, typedDefault);
		CUI_EXPECT_TRUE(target.IsPropertyValueDefault(L"Level"));

		target.Measure(Constraints{ Size{ 200.0f, 100.0f } });
		target.ApplyLayout(Rect{ 0.0f, 0.0f, 120.0f, 20.0f });
		CUI_EXPECT_FALSE(target.GetComputedLayout().NeedsMeasure());

		int notifications = 0;
		std::wstring changedName;
		int oldLevel = -1;
		int newLevel = -1;
		auto connection = target.OnPropertyValueChanged.Subscribe(
			[&](Control*, const ControlPropertyChangedEventArgs& e)
			{
				++notifications;
				changedName = e.PropertyName;
				e.OldValue.TryGet(oldLevel);
				e.NewValue.TryGet(newLevel);
			});

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(L"Level", BindingValue(99)));
		CUI_EXPECT_EQ(10, target.GetLevel());
		CUI_EXPECT_EQ(1, notifications);
		CUI_EXPECT_EQ(std::wstring(L"Level"), changedName);
		CUI_EXPECT_EQ(4, oldLevel);
		CUI_EXPECT_EQ(10, newLevel);
		CUI_EXPECT_EQ(1, target.ChangedCallbackCount);
		CUI_EXPECT_TRUE(target.GetComputedLayout().NeedsMeasure());
		CUI_EXPECT_FALSE(target.IsPropertyValueDefault(L"Level"));

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(L"Level", BindingValue(12)));
		CUI_EXPECT_EQ(10, target.GetLevel());
		CUI_EXPECT_EQ(1, notifications);
		CUI_EXPECT_EQ(1, target.ChangedCallbackCount);

		CUI_EXPECT_TRUE(target.ResetPropertyValue(L"Level"));
		CUI_EXPECT_EQ(4, target.GetLevel());
		CUI_EXPECT_EQ(2, notifications);
		CUI_EXPECT_TRUE(target.IsPropertyValueDefault(L"Level"));
		target.SetLevel(-3);
		CUI_EXPECT_EQ(0, target.GetLevel());
		CUI_EXPECT_EQ(3, notifications);

		BindingValue current;
		int typedCurrent = -1;
		CUI_EXPECT_TRUE(target.TryGetPropertyValue(L"Level", current));
		CUI_EXPECT_TRUE(current.TryGet(typedCurrent));
		CUI_EXPECT_EQ(0, typedCurrent);
		CUI_EXPECT_FALSE(target.TrySetPropertyValue(
			L"Level", BindingValue(BindingPayload{ 1, L"wrong type" })));
		CUI_EXPECT_FALSE(target.TrySetPropertyValue(L"Missing", BindingValue(1)));
		CUI_EXPECT_FALSE(target.ResetPropertyValue(L"Missing"));

		ObservableObject source;
		source.SetValue(L"Level", 50);
		CUI_EXPECT_TRUE(target.DataBindings.Add(
			L"Level", source, L"Level", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_EQ(10, target.GetLevel());
		target.SetCurrentLevel(7);
		CUI_EXPECT_EQ(7, source.GetValue<int>(L"Level"));
		source.SetValue(L"Level", 8);
		CUI_EXPECT_EQ(8, target.GetLevel());
	});

	runner.Add("Built-in grid properties use shared property semantics", []
	{
		Control target;
		int notifications = 0;
		auto connection = target.OnPropertyValueChanged.Subscribe(
			[&](Control*, const ControlPropertyChangedEventArgs& e)
			{
				if (e.PropertyName == L"GridRow"
					|| e.PropertyName == L"GridColumn"
					|| e.PropertyName == L"GridRowSpan"
					|| e.PropertyName == L"GridColumnSpan")
					++notifications;
			});

		CUI_EXPECT_TRUE(target.IsPropertyValueDefault(L"GridRow"));
		target.GridRow = -4;
		target.GridColumn = -2;
		target.GridRowSpan = 0;
		target.GridColumnSpan = -1;
		CUI_EXPECT_EQ(0, target.GridRow);
		CUI_EXPECT_EQ(0, target.GridColumn);
		CUI_EXPECT_EQ(1, target.GridRowSpan);
		CUI_EXPECT_EQ(1, target.GridColumnSpan);
		CUI_EXPECT_EQ(0, notifications);

		target.GridRow = 2;
		target.GridColumn = 3;
		target.GridRowSpan = 4;
		target.GridColumnSpan = 5;
		CUI_EXPECT_EQ(4, notifications);
		CUI_EXPECT_FALSE(target.IsPropertyValueDefault(L"GridRow"));
		CUI_EXPECT_TRUE(target.ResetPropertyValue(L"GridRow"));
		CUI_EXPECT_EQ(0, target.GridRow);
		CUI_EXPECT_EQ(5, notifications);
	});

	runner.Add("Control property value sources follow deterministic precedence", []
	{
		PropertySystemControl target;
		CUI_EXPECT_EQ(std::wstring(L"Binding"), std::wstring(
			ControlPropertyValueSourceName(ControlPropertyValueSource::Binding)));
		int notifications = 0;
		auto connection = target.OnPropertyValueChanged.Subscribe(
			[&](Control*, const ControlPropertyChangedEventArgs& e)
			{
				if (e.PropertyName == L"Level") ++notifications;
			});

		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(2), ControlPropertyValueSource::Theme));
		CUI_EXPECT_EQ(2, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Theme,
			target.GetPropertyValueSource(L"Level"));

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(3), ControlPropertyValueSource::Style));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(5), ControlPropertyValueSource::Binding));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(9), ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(9, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Local,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_EQ(4, notifications);

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(-5), ControlPropertyValueSource::Theme));
		CUI_EXPECT_EQ(9, target.GetLevel());
		CUI_EXPECT_EQ(4, notifications);
		BindingValue themeValue;
		int typedTheme = -1;
		CUI_EXPECT_TRUE(target.TryGetPropertyValue(
			L"Level", ControlPropertyValueSource::Theme, themeValue));
		CUI_EXPECT_TRUE(themeValue.TryGet(typedTheme));
		CUI_EXPECT_EQ(0, typedTheme);

		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(5, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Binding));
		CUI_EXPECT_EQ(3, target.GetLevel());
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Style));
		CUI_EXPECT_EQ(0, target.GetLevel());
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Theme));
		CUI_EXPECT_EQ(4, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_EQ(8, notifications);
		CUI_EXPECT_FALSE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Default));
		CUI_EXPECT_FALSE(target.TrySetPropertyValue(
			L"Level", BindingValue(1), ControlPropertyValueSource::Default));

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(6), ControlPropertyValueSource::Style));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(L"Level", BindingValue(8)));
		CUI_EXPECT_TRUE(target.ResetPropertyValue(L"Level"));
		CUI_EXPECT_EQ(6, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_EQ(1ULL,
			target.ClearPropertyValues(ControlPropertyValueSource::Style));
		CUI_EXPECT_EQ(4, target.GetLevel());

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(6), ControlPropertyValueSource::Style));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(L"Level", BindingValue(8)));
		CUI_EXPECT_TRUE(target.TrySetPropertyBaseValue(
			L"Level", BindingValue(7)));
		CUI_EXPECT_EQ(8, target.GetLevel());
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(6, target.GetLevel());
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Style));
		CUI_EXPECT_EQ(7, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(target.TrySetPropertyBaseValue(
			L"Level", BindingValue(4)));
		CUI_EXPECT_EQ(4, target.GetLevel());

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(2), ControlPropertyValueSource::Theme));
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"ValidationCornerRadius", BindingValue(7.0f),
			ControlPropertyValueSource::Theme));
		CUI_EXPECT_EQ(2ULL,
			target.ClearPropertyValues(ControlPropertyValueSource::Theme));
		CUI_EXPECT_EQ(4, target.GetLevel());
		CUI_EXPECT_EQ(4.0f, target.ValidationCornerRadius);
	});

	runner.Add("Binding owns and releases its property value source", []
	{
		PropertySystemControl target;
		target.SetLevel(6);
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(3), ControlPropertyValueSource::Style));

		ObservableObject source;
		source.SetValue(L"Level", 9);
		Binding* binding = target.DataBindings.Add(
			L"Level", source, L"Level", BindingMode::OneWay);
		CUI_EXPECT_TRUE(binding != nullptr);
		CUI_EXPECT_EQ(9, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(target.HasPropertyValue(
			L"Level", ControlPropertyValueSource::Binding));

		CUI_EXPECT_TRUE(target.DataBindings.Add(
			L"level", source, L"Level", BindingMode::OneWay) == nullptr);
		CUI_EXPECT_EQ(BindingError::DuplicateTargetProperty,
			target.DataBindings.LastError());

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(8), ControlPropertyValueSource::Local));
		source.SetValue(L"Level", 10);
		CUI_EXPECT_EQ(8, target.GetLevel());
		BindingValue bindingValue;
		int typedBinding = 0;
		CUI_EXPECT_TRUE(target.TryGetPropertyValue(
			L"Level", ControlPropertyValueSource::Binding, bindingValue));
		CUI_EXPECT_TRUE(bindingValue.TryGet(typedBinding));
		CUI_EXPECT_EQ(10, typedBinding);
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(10, target.GetLevel());
		target.DataBindings.Clear();
		CUI_EXPECT_EQ(3, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_FALSE(target.HasPropertyValue(
			L"Level", ControlPropertyValueSource::Binding));

		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Style));
		CUI_EXPECT_EQ(6, target.GetLevel());
	});

	runner.Add("Standalone bindings cannot steal an owned target value", []
	{
		PropertySystemControl target;
		target.SetLevel(6);
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Level", BindingValue(3), ControlPropertyValueSource::Style));

		ObservableObject firstSource;
		firstSource.SetValue(L"Level", 9);
		ObservableObject secondSource;
		secondSource.SetValue(L"Level", 11);
		{
			Binding first(
				&target, L"Level", &firstSource, L"Level", BindingMode::OneWay);
			CUI_EXPECT_TRUE(first.IsValid());
			CUI_EXPECT_EQ(9, target.GetLevel());
			CUI_EXPECT_FALSE(target.TrySetPropertyValue(
				L"Level", BindingValue(12), ControlPropertyValueSource::Binding));
			CUI_EXPECT_FALSE(target.ClearPropertyValue(
				L"Level", ControlPropertyValueSource::Binding));

			Binding second(
				&target, L"Level", &secondSource, L"Level", BindingMode::OneWay);
			CUI_EXPECT_FALSE(second.IsValid());
			CUI_EXPECT_EQ(BindingError::DuplicateTargetProperty, second.LastError());
			CUI_EXPECT_EQ(9, target.GetLevel());
		}

		CUI_EXPECT_EQ(3, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Style));
		CUI_EXPECT_EQ(6, target.GetLevel());
	});

	runner.Add("Style sheets cascade by id class state and source order", []
	{
		PropertySystemControl target;
		target.SetStyleId(L"hero");
		CUI_EXPECT_TRUE(target.AddStyleClass(L"primary"));
		CUI_EXPECT_FALSE(target.AddStyleClass(L"PRIMARY"));

		auto sheet = std::make_shared<ControlStyleSheet>();
		ControlStyleSelector base;
		base.Type = UIClass::UI_Base;
		const auto baseRule = sheet->AddRule(
			base, { ControlStyleSetter(L"Level", BindingValue(1)) });
		ControlStyleSelector typed;
		typed.Type = UIClass::UI_CUSTOM;
		const auto typedRule = sheet->AddRule(
			typed, { ControlStyleSetter(L"Level", BindingValue(6)) });
		ControlStyleSelector primary;
		primary.Classes.push_back(L"primary");
		const auto primaryRule = sheet->AddRule(
			primary, { ControlStyleSetter(L"Level", BindingValue(2)) });
		ControlStyleSelector hovered;
		hovered.RequiredStates = ControlStyleState::Hovered;
		const auto hoveredRule = sheet->AddRule(
			hovered, { ControlStyleSetter(L"Level", BindingValue(3)) });
		ControlStyleSelector identified;
		identified.Id = L"hero";
		const auto identifiedRule = sheet->AddRule(
			identified, { ControlStyleSetter(L"Level", BindingValue(9)) });
		CUI_EXPECT_TRUE(baseRule != 0 && typedRule != 0 && primaryRule != 0
			&& hoveredRule != 0 && identifiedRule != 0);

		CUI_EXPECT_TRUE(target.SetStyleSheet(sheet));
		CUI_EXPECT_EQ(9, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			target.GetPropertyValueSource(L"Level"));
		CUI_EXPECT_TRUE(sheet->RemoveRule(identifiedRule));
		CUI_EXPECT_EQ(2, target.GetLevel());
		const auto invalidRule = sheet->AddRule(identified, {
			ControlStyleSetter(L"Level", BindingValue(L"not-a-number")),
			ControlStyleSetter(L"MissingProperty", BindingValue(1)) });
		CUI_EXPECT_EQ(2, target.GetLevel());
		auto invalidResolution = sheet->Resolve(target);
		CUI_EXPECT_FALSE(invalidResolution.Success());
		CUI_EXPECT_EQ(2ULL, invalidResolution.Issues.size());
		CUI_EXPECT_EQ(ControlStyleResolutionIssueCode::InvalidValue,
			invalidResolution.Issues[0].Code);
		CUI_EXPECT_EQ(ControlStyleResolutionIssueCode::PropertyNotFound,
			invalidResolution.Issues[1].Code);
		CUI_EXPECT_TRUE(sheet->RemoveRule(invalidRule));
		ControlStyleSelector composite;
		composite.Classes = { L"primary", L"danger" };
		composite.ExcludedStates = ControlStyleState::Hovered;
		CUI_EXPECT_FALSE(composite.Matches(target));
		CUI_EXPECT_TRUE(target.AddStyleClass(L"danger"));
		CUI_EXPECT_TRUE(composite.Matches(target));

		MouseEventArgs mouse(MouseButtons::None, 0, 0, 0, 0);
		target.OnMouseEnter(&target, mouse);
		CUI_EXPECT_FALSE(composite.Matches(target));
		CUI_EXPECT_TRUE(HasControlStyleState(
			target.GetEffectiveStyleState(), ControlStyleState::Hovered));
		CUI_EXPECT_EQ(3, target.GetLevel());
		target.OnMouseLeave(&target, mouse);
		CUI_EXPECT_EQ(2, target.GetLevel());

		CUI_EXPECT_TRUE(sheet->RemoveRule(primaryRule));
		CUI_EXPECT_EQ(6, target.GetLevel());
		CUI_EXPECT_TRUE(target.SetStyleSheet(nullptr));
		CUI_EXPECT_EQ(4, target.GetLevel());
		CUI_EXPECT_FALSE(target.HasPropertyValue(
			L"Level", ControlPropertyValueSource::Style));
	});

	runner.Add("Style resources hot reload through Theme and preserve precedence", []
	{
		PropertySystemControl target;
		auto theme = std::make_shared<ControlStyleSheet>();
		ControlStyleSelector all;
		theme->AddRule(all, {
			ControlStyleSetter::Resource(L"Level", L"Control.Level") });

		CUI_EXPECT_FALSE(target.SetThemeStyleSheet(theme));
		CUI_EXPECT_EQ(4, target.GetLevel());
		auto unresolved = theme->Resolve(target);
		CUI_EXPECT_FALSE(unresolved.Success());
		CUI_EXPECT_EQ(1ULL, unresolved.Issues.size());
		CUI_EXPECT_EQ(ControlStyleResolutionIssueCode::MissingResource,
			unresolved.Issues[0].Code);

		CUI_EXPECT_TRUE(theme->SetResource(L"control.level", BindingValue(2)));
		CUI_EXPECT_EQ(2, target.GetLevel());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Theme,
			target.GetPropertyValueSource(L"Level"));

		auto style = std::make_shared<ControlStyleSheet>();
		style->AddRule(all, {
			ControlStyleSetter(L"Level", BindingValue(3)) });
		CUI_EXPECT_TRUE(target.SetStyleSheet(style));
		CUI_EXPECT_EQ(3, target.GetLevel());
		CUI_EXPECT_TRUE(theme->SetResource(L"Control.Level", BindingValue(5)));
		CUI_EXPECT_EQ(3, target.GetLevel());
		BindingValue themeValue;
		int typedTheme = 0;
		CUI_EXPECT_TRUE(target.TryGetPropertyValue(
			L"Level", ControlPropertyValueSource::Theme, themeValue));
		CUI_EXPECT_TRUE(themeValue.TryGet(typedTheme));
		CUI_EXPECT_EQ(5, typedTheme);

		CUI_EXPECT_TRUE(target.TrySetPropertyValue(L"Level", BindingValue(8)));
		CUI_EXPECT_TRUE(target.SetStyleSheet(nullptr));
		CUI_EXPECT_EQ(8, target.GetLevel());
		CUI_EXPECT_TRUE(target.ClearPropertyValue(
			L"Level", ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(5, target.GetLevel());
		CUI_EXPECT_TRUE(theme->RemoveResource(L"CONTROL.LEVEL"));
		CUI_EXPECT_EQ(4, target.GetLevel());
		CUI_EXPECT_TRUE(theme->SetResource(L"Control.Level", BindingValue(6)));
		CUI_EXPECT_EQ(6, target.GetLevel());
	});

	runner.Add("Style states refresh automatically from control events", []
	{
		PropertySystemControl target;
		auto sheet = std::make_shared<ControlStyleSheet>();
		ControlStyleSelector all;
		sheet->AddRule(all, {
			ControlStyleSetter(L"Level", BindingValue(1)) });
		auto addStateRule = [&sheet](ControlStyleState state, int value)
		{
			ControlStyleSelector selector;
			selector.RequiredStates = state;
			sheet->AddRule(selector, {
				ControlStyleSetter(L"Level", BindingValue(value)) });
		};
		addStateRule(ControlStyleState::Hovered, 2);
		addStateRule(ControlStyleState::Focused, 3);
		addStateRule(ControlStyleState::Pressed, 4);
		addStateRule(ControlStyleState::Checked, 5);
		addStateRule(ControlStyleState::Disabled, 6);
		CUI_EXPECT_TRUE(target.SetStyleSheet(sheet));
		CUI_EXPECT_EQ(1, target.GetLevel());

		MouseEventArgs mouse(MouseButtons::Left, 1, 0, 0, 0);
		target.OnMouseEnter(&target, mouse);
		CUI_EXPECT_EQ(2, target.GetLevel());
		target.OnGotFocus(&target);
		CUI_EXPECT_EQ(3, target.GetLevel());
		target.OnMouseDown(&target, mouse);
		CUI_EXPECT_EQ(4, target.GetLevel());
		target.OnMouseUp(&target, mouse);
		CUI_EXPECT_EQ(3, target.GetLevel());
		target.OnLostFocus(&target);
		CUI_EXPECT_EQ(2, target.GetLevel());
		target.OnMouseLeave(&target, mouse);
		CUI_EXPECT_EQ(1, target.GetLevel());

		target.Checked = true;
		target.OnChecked(&target);
		CUI_EXPECT_EQ(5, target.GetLevel());
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Enabled", BindingValue(false)));
		CUI_EXPECT_EQ(6, target.GetLevel());
		CUI_EXPECT_TRUE(target.TrySetPropertyValue(
			L"Enabled", BindingValue(true)));
		CUI_EXPECT_EQ(5, target.GetLevel());
	});

	runner.Add("Control trees inherit attached sheets and follow rule lifetime", []
	{
		Control root;
		auto sheet = std::make_shared<ControlStyleSheet>();
		ControlStyleSelector childSelector;
		childSelector.Id = L"child";
		const auto initialRule = sheet->AddRule(childSelector, {
			ControlStyleSetter(L"Level", BindingValue(7)) });
		CUI_EXPECT_TRUE(root.SetStyleSheet(sheet));

		auto ownedChild = std::make_unique<PropertySystemControl>();
		ownedChild->SetStyleId(L"child");
		auto* child = root.AddOwned(std::move(ownedChild));
		CUI_EXPECT_TRUE(sheet == child->GetStyleSheet());
		CUI_EXPECT_EQ(7, child->GetLevel());

		const auto replacementRule = sheet->AddRule(childSelector, {
			ControlStyleSetter(L"Level", BindingValue(8)) });
		CUI_EXPECT_EQ(8, child->GetLevel());
		CUI_EXPECT_TRUE(sheet->RemoveRule(replacementRule));
		CUI_EXPECT_EQ(7, child->GetLevel());
		CUI_EXPECT_TRUE(sheet->RemoveRule(initialRule));
		CUI_EXPECT_EQ(4, child->GetLevel());
		CUI_EXPECT_TRUE(root.SetStyleSheet(nullptr));
		CUI_EXPECT_TRUE(child->GetStyleSheet() == nullptr);
	});

	runner.Add("Common interactive appearance properties share value-source metadata", []
	{
		auto sheet = std::make_shared<ControlStyleSheet>();
		const D2D1_COLOR_F buttonStyleColor{ 0.1f, 0.2f, 0.8f, 0.3f };
		ControlStyleSelector buttonSelector;
		buttonSelector.Type = UIClass::UI_Button;
		sheet->AddRule(buttonSelector, {
			ControlStyleSetter(L"UnderMouseColor", BindingValue(buttonStyleColor)),
			ControlStyleSetter(L"BorderThickness", BindingValue(-3.0f)) });
		ControlStyleSelector textSelector;
		textSelector.Type = UIClass::UI_TextBox;
		sheet->AddRule(textSelector, {
			ControlStyleSetter(L"FocusedColor", BindingValue(
				D2D1_COLOR_F{ 0.9f, 0.3f, 0.2f, 1.0f })),
			ControlStyleSetter(L"CornerRadius", BindingValue(-2.0f)) });
		ControlStyleSelector comboSelector;
		comboSelector.Type = UIClass::UI_ComboBox;
		sheet->AddRule(comboSelector, {
			ControlStyleSetter(L"HeaderHoverBackColor", BindingValue(
				D2D1_COLOR_F{ 0.2f, 0.7f, 0.4f, 0.25f })),
			ControlStyleSetter(L"DropCornerRadius", BindingValue(-1.0f)) });

		Button button(L"Button", 0, 0);
		const D2D1_COLOR_F localColor{ 0.8f, 0.1f, 0.1f, 0.4f };
		button.UnderMouseColor = localColor;
		CUI_EXPECT_EQ(ControlPropertyValueSource::Local,
			button.GetPropertyValueSource(L"UnderMouseColor"));
		const auto* buttonMetadata = button.FindPropertyMetadata(L"UnderMouseColor");
		CUI_EXPECT_TRUE(buttonMetadata != nullptr);
		CUI_EXPECT_TRUE(HasControlPropertyFlag(
			buttonMetadata->Flags(), ControlPropertyFlags::TracksLocalValue));
		CUI_EXPECT_TRUE(button.SetStyleSheet(sheet));
		CUI_EXPECT_EQ(localColor.r, button.UnderMouseColor.r);
		BindingValue storedStyle;
		D2D1_COLOR_F typedStyle{};
		CUI_EXPECT_TRUE(button.TryGetPropertyValue(
			L"UnderMouseColor", ControlPropertyValueSource::Style, storedStyle));
		CUI_EXPECT_TRUE(storedStyle.TryGet(typedStyle));
		CUI_EXPECT_EQ(buttonStyleColor.b, typedStyle.b);
		CUI_EXPECT_TRUE(button.ClearPropertyValue(
			L"UnderMouseColor", ControlPropertyValueSource::Local));
		CUI_EXPECT_EQ(buttonStyleColor.r, button.UnderMouseColor.r);
		CUI_EXPECT_EQ(0.0f, button.BorderThickness);
		button.BorderThickness = 2.5f;
		CUI_EXPECT_EQ(ControlPropertyValueSource::Local,
			button.GetPropertyValueSource(L"BorderThickness"));
		CUI_EXPECT_TRUE(button.ResetPropertyValue(L"BorderThickness"));
		CUI_EXPECT_EQ(0.0f, button.BorderThickness);

		TextBox textBox(L"", 0, 0);
		CUI_EXPECT_TRUE(textBox.SetStyleSheet(sheet));
		CUI_EXPECT_EQ(0.0f, textBox.CornerRadius);
		CUI_EXPECT_EQ(0.9f, textBox.FocusedColor.r);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			textBox.GetPropertyValueSource(L"FocusedColor"));

		ComboBox comboBox(L"", 0, 0);
		CUI_EXPECT_TRUE(comboBox.SetStyleSheet(sheet));
		CUI_EXPECT_EQ(0.0f, comboBox.DropCornerRadius);
		CUI_EXPECT_EQ(0.7f, comboBox.HeaderHoverBackColor.g);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			comboBox.GetPropertyValueSource(L"HeaderHoverBackColor"));

		CUI_EXPECT_TRUE(button.SetStyleSheet(nullptr));
		CUI_EXPECT_EQ(1.5f, button.BorderThickness);
		CUI_EXPECT_TRUE(button.IsPropertyValueDefault(L"BorderThickness"));
	});

	runner.Add("Designer style sheets validate typed values and drive runtime styles", []
	{
		DesignerStyleSheet styleSheet;
		styleSheet.Resources.push_back({
			L" Accent ", { DesignerStyleValueKind::Color, L"#804080C0" } });
		DesignerStyleRule rule;
		rule.HasType = true;
		rule.Type = UIClass::UI_Button;
		rule.Classes = { L"primary", L" PRIMARY " };
		rule.RequiredStates = ControlStyleState::Hovered;
		rule.Setters = {
			{ L"UnderMouseColor", true, L"Accent", {} },
			{ L"Round", false, L"", { DesignerStyleValueKind::Float, L"7.25" } }
		};
		styleSheet.Rules.push_back(std::move(rule));

		DesignerStyleSheetUtils::Canonicalize(styleSheet);
		CUI_EXPECT_EQ(std::wstring(L"Accent"), styleSheet.Resources[0].Key);
		CUI_EXPECT_EQ(1ULL, styleSheet.Rules[0].Classes.size());
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerStyleSheetUtils::Validate(styleSheet, &error));
		CUI_EXPECT_TRUE(error.empty());

		std::shared_ptr<ControlStyleSheet> runtime;
		CUI_EXPECT_TRUE(DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			styleSheet, runtime, &error));
		Button button(L"Styled", 0, 0);
		CUI_EXPECT_TRUE(button.AddStyleClass(L"primary"));
		button.SetStyleState(ControlStyleState::Hovered);
		CUI_EXPECT_TRUE(button.SetStyleSheet(runtime));
		CUI_EXPECT_EQ(7.25f, button.Round);
		CUI_EXPECT_EQ(64.0f / 255.0f, button.UnderMouseColor.r);
		CUI_EXPECT_EQ(128.0f / 255.0f, button.UnderMouseColor.g);
		CUI_EXPECT_EQ(128.0f / 255.0f, button.UnderMouseColor.a);

		DesignerStyleValue thickness{
			DesignerStyleValueKind::Thickness, L"4, 8" };
		BindingValue converted;
		Thickness typed;
		CUI_EXPECT_TRUE(DesignerStyleSheetUtils::TryConvertValue(
			thickness, converted, &error));
		CUI_EXPECT_TRUE(converted.TryGet(typed));
		CUI_EXPECT_EQ(4.0f, typed.Left);
		CUI_EXPECT_EQ(8.0f, typed.Top);

		auto invalid = styleSheet;
		invalid.Rules[0].Setters[0].ResourceKey = L"Missing";
		CUI_EXPECT_FALSE(DesignerStyleSheetUtils::Validate(invalid, &error));
		CUI_EXPECT_TRUE(error.find(L"不存在") != std::wstring::npos);
		invalid = styleSheet;
		invalid.Rules[0].ExcludedStates = ControlStyleState::Hovered;
		CUI_EXPECT_FALSE(DesignerStyleSheetUtils::Validate(invalid, &error));
	});

	runner.Add("Binding collections find and remove individual target bindings", []
	{
		Button target(L"Local", 0, 0);
		MetadataObservableObject source;
		source.SetValue(L"Caption", std::wstring(L"Bound"));
		source.SetValidationError(L"Caption", L"caption is invalid", L"caption");
		(void)target.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local);
		auto* binding = target.DataBindings.Add(
			L"Text", source, L"Caption", BindingMode::OneWay);
		CUI_EXPECT_TRUE(binding != nullptr);
		CUI_EXPECT_EQ(binding, target.DataBindings.Find(L"text"));
		const auto& readOnlyBindings = target.DataBindings;
		CUI_EXPECT_EQ(binding, readOnlyBindings.Find(L"TEXT"));
		int validationChanges = 0;
		auto validationConnection = target.DataBindings.ValidationChanged().Subscribe(
			[&validationChanges](const BindingValidationChangedEventArgs&)
			{
				++validationChanges;
			});
		CUI_EXPECT_TRUE(target.DataBindings.Remove(L"tExT"));
		CUI_EXPECT_EQ(1, validationChanges);
		CUI_EXPECT_EQ(0ULL, target.DataBindings.Count());
		CUI_EXPECT_TRUE(target.DataBindings.Find(L"Text") == nullptr);
		CUI_EXPECT_FALSE(target.DataBindings.Remove(L"Text"));
		source.SetValue(L"Caption", std::wstring(L"Changed"));
		CUI_EXPECT_FALSE(target.Text == L"Changed");
	});

	runner.Add("Designer property catalog projects writable runtime style metadata", []
	{
		Button button(L"Catalog", 0, 0);
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(button);
		const auto* color = DesignerPropertyCatalog::Find(properties, L"undermousecolor");
		const auto* round = DesignerPropertyCatalog::Find(properties, L"Round");
		const auto* raised = DesignerPropertyCatalog::Find(properties, L"Raised");
		const auto* margin = DesignerPropertyCatalog::Find(properties, L"Margin");
		const auto* alignment = DesignerPropertyCatalog::Find(properties, L"HAlign");
		CUI_EXPECT_TRUE(color != nullptr);
		CUI_EXPECT_TRUE(round != nullptr);
		CUI_EXPECT_TRUE(raised != nullptr);
		CUI_EXPECT_TRUE(margin != nullptr);
		CUI_EXPECT_TRUE(alignment != nullptr);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Color, color->ValueKind);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, round->ValueKind);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Bool, raised->ValueKind);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Thickness, margin->ValueKind);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Int, alignment->ValueKind);
		CUI_EXPECT_FALSE(color->SampleValue.empty());

		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ValidateStyleValue(
			button, L"Round", { DesignerStyleValueKind::Float, L"8.5" }, &error));
		CUI_EXPECT_TRUE(error.empty());
		CUI_EXPECT_FALSE(DesignerPropertyCatalog::ValidateStyleValue(
			button, L"Round", { DesignerStyleValueKind::String, L"8.5" }, &error));
		CUI_EXPECT_TRUE(error.find(L"Float") != std::wstring::npos);
		CUI_EXPECT_FALSE(DesignerPropertyCatalog::ValidateStyleValue(
			button, L"Missing", { DesignerStyleValueKind::Float, L"1" }, &error));
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ValidateStyleValue(
			button, L"HAlign", { DesignerStyleValueKind::Int, L"2" }, &error));

		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			button,
			L"round",
			{ DesignerStyleValueKind::Float, L"9.25" },
			&canonicalName,
			&effectiveValue,
			&error));
		CUI_EXPECT_EQ(std::wstring(L"Round"), canonicalName);
		CUI_EXPECT_EQ(9.25f, button.Round);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, effectiveValue.Kind);
		DesignerStyleValue capturedValue;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::CaptureValue(
			button, L"ROUND", &canonicalName, capturedValue, &error));
		CUI_EXPECT_EQ(effectiveValue, capturedValue);
		CUI_EXPECT_FALSE(DesignerPropertyCatalog::ApplyValue(
			button,
			L"Round",
			{ DesignerStyleValueKind::String, L"10" },
			nullptr,
			nullptr,
			&error));

		DesignerStyleSheet styleSheet;
		styleSheet.Resources.push_back({
			L"Accent", { DesignerStyleValueKind::Color, L"#FF336699" } });
		DesignerStyleRule rule;
		rule.HasType = true;
		rule.Type = UIClass::UI_Button;
		rule.Setters = {
			{ L"UnderMouseColor", true, L"Accent", {} },
			{ L"Round", false, L"", { DesignerStyleValueKind::Float, L"6" } }
		};
		styleSheet.Rules.push_back(rule);
		auto factory = [](UIClass type) -> std::unique_ptr<Control>
		{
			if (type == UIClass::UI_Button)
				return std::make_unique<Button>(L"Probe", 0, 0);
			if (type == UIClass::UI_Base)
				return std::make_unique<Control>();
			return nullptr;
		};
		CUI_EXPECT_TRUE(DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			styleSheet, factory, &error));
		styleSheet.Rules[0].Setters[1].Literal.Kind = DesignerStyleValueKind::Int;
		CUI_EXPECT_FALSE(DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			styleSheet, factory, &error));
		CUI_EXPECT_TRUE(error.find(L"Float") != std::wstring::npos);
		styleSheet.Rules[0].Setters[1].Literal.Kind = DesignerStyleValueKind::Float;
		styleSheet.Rules[0].HasType = false;
		CUI_EXPECT_FALSE(DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			styleSheet, factory, &error));
		CUI_EXPECT_TRUE(error.find(L"UnderMouseColor") != std::wstring::npos);
		styleSheet.Rules[0].HasType = true;
		styleSheet.Resources[0].Value.Kind = DesignerStyleValueKind::Float;
		styleSheet.Resources[0].Value.Text = L"1";
		CUI_EXPECT_FALSE(DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			styleSheet, factory, &error));
		CUI_EXPECT_TRUE(error.find(L"Color") != std::wstring::npos);
	});

	runner.Add("Control property design metadata drives the Designer catalog", []
	{
		Button button(L"Design metadata", 0, 0);
		const auto styleProperties = DesignerPropertyCatalog::GetStyleProperties(button);
		const auto* round = DesignerPropertyCatalog::Find(styleProperties, L"Round");
		const auto* horizontal = DesignerPropertyCatalog::Find(styleProperties, L"HAlign");
		CUI_EXPECT_TRUE(round != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Appearance"), round->Category);
		CUI_EXPECT_EQ(200, round->CategoryOrder);
		CUI_EXPECT_EQ(80, round->Order);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, round->Editor);
		CUI_EXPECT_TRUE(round->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *round->Minimum);
		CUI_EXPECT_TRUE(round->Step.has_value());
		CUI_EXPECT_EQ(0.5, *round->Step);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, round->Persistence);
		CUI_EXPECT_TRUE(horizontal != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, horizontal->Editor);
		CUI_EXPECT_EQ(4ULL, horizontal->Choices.size());
		CUI_EXPECT_EQ(std::wstring(L"Left"), horizontal->Choices[0].DisplayName);
		CUI_EXPECT_EQ(std::wstring(L"0"), horizontal->Choices[0].ValueText);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Legacy, horizontal->Persistence);

		const auto browsable = DesignerPropertyCatalog::GetBrowsableProperties(button);
		const auto indexOf = [&](const wchar_t* name)
		{
			for (size_t index = 0; index < browsable.size(); ++index)
				if (_wcsicmp(browsable[index].Name.c_str(), name) == 0)
					return static_cast<int>(index);
			return -1;
		};
		const int layoutWidthIndex = indexOf(L"LayoutWidth");
		const int roundIndex = indexOf(L"Round");
		const int checkedIndex = indexOf(L"Checked");
		CUI_EXPECT_TRUE(layoutWidthIndex >= 0);
		CUI_EXPECT_TRUE(roundIndex > layoutWidthIndex);
		CUI_EXPECT_TRUE(checkedIndex > roundIndex);
		CUI_EXPECT_EQ(std::wstring(L"Width (Auto)"),
			browsable[static_cast<size_t>(layoutWidthIndex)].DisplayName);
		CUI_EXPECT_EQ(-1, indexOf(L"Width"));
		CUI_EXPECT_EQ(-1, indexOf(L"Visible"));
		CUI_EXPECT_EQ(-1, indexOf(L"BackColor"));

		Control plainControl;
		const auto plain = DesignerPropertyCatalog::GetBrowsableProperties(plainControl);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(plain, L"Checked") == nullptr);

		ProgressBar progress(0, 0, 100, 20);
		const auto progressProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(progress);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(progressProperties, L"Value") == nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(progressProperties, L"MaxValue") == nullptr);

		ScrollView scroll(0, 0, 100, 100);
		const auto scrollProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(scroll);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			scrollProperties, L"ScrollXOffset") == nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			scrollProperties, L"ScrollYOffset") == nullptr);
	});

	runner.Add("PropertyGrid catalog includes legacy scalars and registered structural editors", []
	{
		Button button(L"Metadata panel", 4, 6);
		auto properties = DesignerPropertyCatalog::GetPropertyGridProperties(button);
		const auto* text = DesignerPropertyCatalog::Find(properties, L"Text");
		const auto* enabled = DesignerPropertyCatalog::Find(properties, L"Enable");
		const auto* enabledAlias = DesignerPropertyCatalog::Find(properties, L"Enabled");
		const auto* left = DesignerPropertyCatalog::Find(properties, L"Left");
		const auto* zIndex = DesignerPropertyCatalog::Find(properties, L"ZIndex");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Legacy, text->Persistence);
		CUI_EXPECT_TRUE(enabled != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Enabled"), enabled->DisplayName);
		CUI_EXPECT_TRUE(enabledAlias == nullptr);
		CUI_EXPECT_TRUE(left != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"X"), left->DisplayName);
		CUI_EXPECT_TRUE(zIndex != nullptr);

		GridPanel grid;
		button.Parent = &grid;
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(button);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"GridRow") != nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"DockPosition") == nullptr);
		DockPanel dock;
		button.Parent = &dock;
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(button);
		const auto* dockPosition = DesignerPropertyCatalog::Find(properties, L"DockPosition");
		CUI_EXPECT_TRUE(dockPosition != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Dock"), dockPosition->DisplayName);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"GridRow") == nullptr);
		button.Parent = nullptr;

		LinkLabel link(L"Link", 0, 0);
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(link);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"Visited") != nullptr);

		LoadingRing loading(0, 0);
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(loading);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"Active") != nullptr);

		ProgressRing ring(0, 0);
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(ring);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"PercentageValue") != nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"ShowPercentage") != nullptr);

		PictureBox picture(0, 0);
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(picture);
		const auto* sizeMode = DesignerPropertyCatalog::Find(properties, L"SizeMode");
		CUI_EXPECT_TRUE(sizeMode != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, sizeMode->Editor);
		CUI_EXPECT_EQ(4ULL, sizeMode->Choices.size());
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			picture, L"SizeMode", { DesignerStyleValueKind::Int, L"2" },
			nullptr, nullptr, &error));
		CUI_EXPECT_EQ(ImageSizeMode::StretchImage, picture.SizeMode);
		CUI_EXPECT_TRUE(picture.ResetPropertyValue(L"SizeMode"));
		CUI_EXPECT_EQ(ImageSizeMode::Zoom, picture.SizeMode);

		DateTimePicker picker;
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(picker);
		const auto* mode = DesignerPropertyCatalog::Find(properties, L"Mode");
		CUI_EXPECT_TRUE(mode != nullptr);
		CUI_EXPECT_EQ(3ULL, mode->Choices.size());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"AllowModeSwitch") != nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"Expand") != nullptr);

		TreeView tree(0, 0);
		properties = DesignerPropertyCatalog::GetPropertyGridProperties(tree);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(properties, L"SelectedBackColor") != nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"UnderMouseItemBackColor") != nullptr);

		CUI_EXPECT_FALSE(DesignerCustomEditorCatalog::Register({}));
		CUI_EXPECT_TRUE(DesignerCustomEditorCatalog::Register({
			L"Probe", UIClass::UI_Base, L"Edit probe...", 20,
			DesignerCustomEditorKind::ComboBoxItems }));
		CUI_EXPECT_TRUE(DesignerCustomEditorCatalog::Register({
			L"probe", UIClass::UI_Base, L"Edit replacement...", 10,
			DesignerCustomEditorKind::GridViewColumns }));
		const auto probeEditors =
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_Base);
		CUI_EXPECT_EQ(1ULL, probeEditors.size());
		CUI_EXPECT_EQ(std::wstring(L"Edit replacement..."),
			probeEditors.front().ButtonText);
		CUI_EXPECT_EQ(DesignerCustomEditorKind::GridViewColumns,
			probeEditors.front().Kind);

		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_ComboBox).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_GridView).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_TabControl).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_ToolBar).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_TreeView).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_GridPanel).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_Menu).size());
		CUI_EXPECT_EQ(1ULL,
			DesignerCustomEditorCatalog::GetEditors(UIClass::UI_StatusBar).size());
	});

	runner.Add("Designer property edits share metadata persistence and reset semantics", []
	{
		Button button(L"Tracked metadata", 0, 0);
		DesignerPropertyCatalog::TrackedPropertyValues trackedValues;
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;

		CUI_EXPECT_TRUE(button.TrySetPropertyValue(
			L"Round", BindingValue(5.0f), ControlPropertyValueSource::Style));
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			trackedValues,
			L"round",
			{ DesignerStyleValueKind::Float, L"9.25" },
			&canonicalName,
			&effectiveValue,
			&error));
		CUI_EXPECT_EQ(std::wstring(L"Round"), canonicalName);
		CUI_EXPECT_EQ(1ULL, trackedValues.size());
		CUI_EXPECT_EQ(effectiveValue, trackedValues.at(L"Round"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Local,
			button.GetPropertyValueSource(L"Round"));

		trackedValues[L"round"] = {
			DesignerStyleValueKind::Float, L"stale" };
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			trackedValues,
			L"ROUND",
			{ DesignerStyleValueKind::Float, L"7.5" }));
		CUI_EXPECT_EQ(1ULL, trackedValues.size());
		CUI_EXPECT_EQ(std::wstring(L"7.5"), trackedValues.at(L"Round").Text);

		trackedValues[L"margin"] = {
			DesignerStyleValueKind::Thickness, L"99" };
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			trackedValues,
			L"Margin",
			{ DesignerStyleValueKind::Thickness, L"1, 2" }));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Local,
			button.GetPropertyValueSource(L"Margin"));
		CUI_EXPECT_TRUE(trackedValues.find(L"margin") == trackedValues.end());
		CUI_EXPECT_EQ(1ULL, trackedValues.size());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			trackedValues,
			L"GridRow",
			{ DesignerStyleValueKind::Int, L"-4" },
			nullptr,
			&effectiveValue));
		CUI_EXPECT_EQ(0, button.GridRow);
		CUI_EXPECT_EQ(std::wstring(L"0"), effectiveValue.Text);
		CUI_EXPECT_EQ(1ULL, trackedValues.size());

		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ResetAndUntrackValue(
			button,
			trackedValues,
			L"Round",
			&canonicalName,
			&effectiveValue,
			&error));
		CUI_EXPECT_TRUE(trackedValues.empty());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			button.GetPropertyValueSource(L"Round"));
		CUI_EXPECT_EQ(std::wstring(L"5"), effectiveValue.Text);
		// Reset is idempotent when a lower-priority source is already effective.
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ResetAndUntrackValue(
			button, trackedValues, L"Round"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			button.GetPropertyValueSource(L"Round"));

		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ResetAndUntrackValue(
			button, trackedValues, L"Margin"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			button.GetPropertyValueSource(L"Margin"));
		CUI_EXPECT_FALSE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			trackedValues,
			L"Round",
			{ DesignerStyleValueKind::String, L"invalid" },
			nullptr,
			nullptr,
			&error));
		CUI_EXPECT_TRUE(trackedValues.empty());
	});

	runner.Add("Designer form properties use one typed model and reset contract", []
	{
		DesignerModel::DesignFormModel form;
		const auto& properties = DesignerFormPropertyCatalog::GetProperties();
		CUI_EXPECT_EQ(21ULL, properties.size());
		std::set<std::wstring> names;
		for (const auto& property : properties)
		{
			auto canonical = property.Name;
			std::transform(canonical.begin(), canonical.end(), canonical.begin(), towlower);
			CUI_EXPECT_TRUE(names.insert(std::move(canonical)).second);
			DesignerStyleValue current;
			CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::CaptureValue(
				form, property.Name, current));
			CUI_EXPECT_EQ(property.DefaultValue, current);
		}

		DesignerStyleValue effective;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"width", { DesignerStyleValueKind::Int, L"12" },
			&effective, &error));
		CUI_EXPECT_EQ(50, form.Size.cx);
		CUI_EXPECT_EQ(std::wstring(L"50"), effective.Text);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"HeadHeight", { DesignerStyleValueKind::Int, L"-8" },
			&effective, &error));
		CUI_EXPECT_EQ(0, form.HeadHeight);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"FontSize", { DesignerStyleValueKind::Float, L"999" },
			&effective, &error));
		CUI_EXPECT_EQ(200.0f, form.FontSize);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"Name", { DesignerStyleValueKind::String, L"" },
			&effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"MainForm"), form.Name);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"BackColor",
			{ DesignerStyleValueKind::Color, L"#80402010" },
			&effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"#80402010"), effective.Text);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"Visible", { DesignerStyleValueKind::Bool, L"false" },
			&effective, &error));
		CUI_EXPECT_FALSE(form.Visible);
		CUI_EXPECT_FALSE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"Visible", { DesignerStyleValueKind::String, L"false" },
			nullptr, &error));
		CUI_EXPECT_FALSE(DesignerFormPropertyCatalog::ApplyValue(
			form, L"Missing", { DesignerStyleValueKind::Int, L"1" },
			nullptr, &error));

		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ResetValue(
			form, L"Width", &effective, &error));
		CUI_EXPECT_EQ(800, form.Size.cx);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ResetValue(
			form, L"FontSize", &effective, &error));
		CUI_EXPECT_EQ(18.0f, form.FontSize);
		CUI_EXPECT_TRUE(DesignerFormPropertyCatalog::ResetValue(
			form, L"Visible", &effective, &error));
		CUI_EXPECT_TRUE(form.Visible);

		form.FontName.clear();
		form.FontSize = 42.0f;
		DesignerModel::DesignDocument document;
		document.Form = form;
		const auto xml = DesignerModel::DesignDocumentSerializer::ToXml(document);
		DesignerModel::DesignDocument loaded;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			xml, loaded, &error));
		CUI_EXPECT_TRUE(loaded.Form.FontName.empty());
		CUI_EXPECT_EQ(42.0f, loaded.Form.FontSize);
	});

	runner.Add("Designer-only control properties use one typed catalog and reset contract", []
	{
		Button button(L"Catalog", 0, 0);
		::Font sharedFont(L"Segoe UI", 19.0f);
		button.SetFontEx(&sharedFont, false);
		DesignerControl target(
			&button, L"button1", UIClass::UI_Button);

		std::wstring requestedName;
		std::wstring synchronizedName;
		uint8_t appliedAnchor = 0;
		DesignerControlPropertyContext context;
		context.SharedFont = &sharedFont;
		context.MakeUniqueName = [&](DesignerControl&, const std::wstring& desired)
		{
			requestedName = desired;
			return std::wstring(L"button2");
		};
		context.SyncDefaultNameCounter = [&](UIClass type, const std::wstring& name)
		{
			CUI_EXPECT_EQ(UIClass::UI_Button, type);
			synchronizedName = name;
		};
		context.ApplyAnchorStylesKeepingBounds = [&](Control* control, uint8_t value)
		{
			CUI_EXPECT_TRUE(control == &button);
			appliedAnchor = value;
			control->AnchorStyles = value;
		};

		const auto properties = DesignerControlPropertyCatalog::GetProperties(target);
		CUI_EXPECT_EQ(6ULL, properties.size());
		CUI_EXPECT_EQ(std::wstring(L"Name"), properties[0].Name);
		CUI_EXPECT_EQ(std::wstring(L"Anchor"), properties[1].Name);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::Find(
			target, L"styleclasses") != nullptr);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::Find(
			target, L"MediaFile") == nullptr);
		CUI_EXPECT_FALSE(DesignerControlPropertyCatalog::Find(
			target, L"Name")->CanReset);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::Find(
			target, L"Anchor")->CanReset);

		DesignerStyleValue current;
		DesignerStyleValue effective;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::CaptureValue(
			target, context, L"FontName", current, &error));
		CUI_EXPECT_TRUE(current.Text.empty());
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::CaptureValue(
			target, context, L"FontSize", current, &error));
		CUI_EXPECT_EQ(std::wstring(L"19"), current.Text);

		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"Name",
			{ DesignerStyleValueKind::String, L"requested" }, &effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"requested"), requestedName);
		CUI_EXPECT_EQ(std::wstring(L"button2"), target.Name);
		CUI_EXPECT_EQ(target.Name, synchronizedName);
		CUI_EXPECT_EQ(target.Name, effective.Text);

		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"StyleId",
			{ DesignerStyleValueKind::String, L"  primary-button  " },
			&effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"primary-button"), button.GetStyleId());
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"StyleClasses",
			{ DesignerStyleValueKind::String,
				L" primary, compact, PRIMARY, " }, &effective, &error));
		CUI_EXPECT_EQ(2ULL, button.GetStyleClasses().size());
		CUI_EXPECT_EQ(std::wstring(L"primary, compact"), effective.Text);

		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"FontName",
			{ DesignerStyleValueKind::String, L" Consolas " }, &effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"Consolas"), button.Font->FontName);
		CUI_EXPECT_TRUE(button.Font != &sharedFont);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"FontSize",
			{ DesignerStyleValueKind::Float, L"999" }, &effective, &error));
		CUI_EXPECT_EQ(200.0f, button.Font->FontSize);
		CUI_EXPECT_FALSE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"FontSize",
			{ DesignerStyleValueKind::Float, L"nan" }, nullptr, &error));
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ResetValue(
			target, context, L"FontSize", &effective, &error));
		CUI_EXPECT_EQ(19.0f, button.Font->FontSize);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ResetValue(
			target, context, L"FontName", &effective, &error));
		CUI_EXPECT_TRUE(button.Font == &sharedFont);
		CUI_EXPECT_TRUE(effective.Text.empty());

		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"Anchor",
			{ DesignerStyleValueKind::Int, L"255" }, &effective, &error));
		const auto allAnchors = static_cast<uint8_t>(AnchorStyles::Left
			| AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom);
		CUI_EXPECT_EQ(allAnchors, appliedAnchor);
		CUI_EXPECT_EQ(std::to_wstring(allAnchors), effective.Text);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ResetValue(
			target, context, L"Anchor", &effective, &error));
		CUI_EXPECT_EQ(static_cast<uint8_t>(0), appliedAnchor);

		MediaPlayer media(0, 0, 320, 180);
		DesignerControl mediaTarget(
			&media, L"mediaPlayer1", UIClass::UI_MediaPlayer);
		const auto mediaProperties =
			DesignerControlPropertyCatalog::GetProperties(mediaTarget);
		CUI_EXPECT_EQ(7ULL, mediaProperties.size());
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ApplyValue(
			mediaTarget, context, L"MediaFile",
			{ DesignerStyleValueKind::String, L"  C:\\media\\clip.mp4  " },
			&effective, &error));
		CUI_EXPECT_EQ(std::wstring(L"C:\\media\\clip.mp4"),
			mediaTarget.DesignStrings[L"mediaFile"]);
		CUI_EXPECT_TRUE(DesignerControlPropertyCatalog::ResetValue(
			mediaTarget, context, L"MediaFile", &effective, &error));
		CUI_EXPECT_TRUE(mediaTarget.DesignStrings.find(L"mediaFile")
			== mediaTarget.DesignStrings.end());

		CUI_EXPECT_FALSE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"Anchor",
			{ DesignerStyleValueKind::String, L"Left" }, nullptr, &error));
		CUI_EXPECT_FALSE(DesignerControlPropertyCatalog::ApplyValue(
			target, context, L"Missing",
			{ DesignerStyleValueKind::String, L"value" }, nullptr, &error));
		CUI_EXPECT_FALSE(DesignerControlPropertyCatalog::ResetValue(
			target, context, L"Name", nullptr, &error));
	});

	runner.Add("Designer property rows merge sources into one ordered presentation model", []
	{
		auto expectContiguousCategories = [](const auto& rows)
		{
			std::set<std::wstring> completed;
			std::wstring current;
			for (const auto& row : rows)
			{
				std::wstring category = row.Category;
				std::transform(category.begin(), category.end(),
					category.begin(), towlower);
				if (category == current) continue;
				CUI_EXPECT_TRUE(completed.insert(category).second);
				current = std::move(category);
			}
		};

		DesignerModel::DesignFormModel form;
		form.FontName.clear();
		form.FontSize = 21.0f;
		const auto formRows = DesignerPropertyRowCatalog::GetFormRows(form);
		CUI_EXPECT_EQ(21ULL, formRows.size());
		expectContiguousCategories(formRows);
		for (const auto& row : formRows)
		{
			CUI_EXPECT_EQ(DesignerPropertyRowSource::Form, row.Source);
			CUI_EXPECT_TRUE(row.CanReset);
		}
		const auto* formFontName = DesignerPropertyRowCatalog::Find(
			formRows, L"fontname");
		const auto* formFontSize = DesignerPropertyRowCatalog::Find(
			formRows, L"FontSize");
		const auto* formVisible = DesignerPropertyRowCatalog::Find(
			formRows, L"Visible");
		const auto* formBackColor = DesignerPropertyRowCatalog::Find(
			formRows, L"BackColor");
		CUI_EXPECT_TRUE(formFontName != nullptr);
		CUI_EXPECT_TRUE(formFontSize != nullptr);
		CUI_EXPECT_TRUE(formVisible != nullptr);
		CUI_EXPECT_TRUE(formBackColor != nullptr);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::FontName,
			formFontName->Editor);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::FontSize,
			formFontSize->Editor);
		CUI_EXPECT_EQ(std::wstring(L"21"), formFontSize->Value.Text);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::Boolean,
			formVisible->Editor);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::Color,
			formBackColor->Editor);

		Button button(L"Rows", 0, 0);
		DesignerControl target(
			&button, L"button1", UIClass::UI_Button);
		DesignerControlPropertyContext context;
		const auto rows = DesignerPropertyRowCatalog::GetControlRows(
			target, context);
		expectContiguousCategories(rows);
		std::set<std::wstring> expectedNames;
		for (const auto& property :
			DesignerControlPropertyCatalog::GetProperties(target))
		{
			auto name = property.Name;
			std::transform(name.begin(), name.end(), name.begin(), towlower);
			expectedNames.insert(std::move(name));
		}
		for (const auto& property :
			DesignerPropertyCatalog::GetPropertyGridProperties(button))
		{
			auto name = property.Name;
			std::transform(name.begin(), name.end(), name.begin(), towlower);
			expectedNames.insert(std::move(name));
		}
		CUI_EXPECT_EQ(expectedNames.size(), rows.size());
		std::set<std::wstring> actualNames;
		for (const auto& row : rows)
		{
			auto name = row.Name;
			std::transform(name.begin(), name.end(), name.begin(), towlower);
			CUI_EXPECT_TRUE(actualNames.insert(std::move(name)).second);
		}

		const auto* name = DesignerPropertyRowCatalog::Find(rows, L"Name");
		const auto* text = DesignerPropertyRowCatalog::Find(rows, L"Text");
		const auto* anchor = DesignerPropertyRowCatalog::Find(rows, L"Anchor");
		const auto* fontName = DesignerPropertyRowCatalog::Find(rows, L"FontName");
		const auto* horizontal = DesignerPropertyRowCatalog::Find(rows, L"HAlign");
		CUI_EXPECT_TRUE(name != nullptr);
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(anchor != nullptr);
		CUI_EXPECT_TRUE(fontName != nullptr);
		CUI_EXPECT_TRUE(horizontal != nullptr);
		CUI_EXPECT_EQ(DesignerPropertyRowSource::ControlDesign, name->Source);
		CUI_EXPECT_EQ(DesignerPropertyRowSource::RuntimeMetadata, text->Source);
		CUI_EXPECT_FALSE(name->CanReset);
		CUI_EXPECT_TRUE(text->CanReset);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::Anchor, anchor->Editor);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::FontName, fontName->Editor);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::Choice, horizontal->Editor);
		CUI_EXPECT_TRUE(!horizontal->Choices.empty());

		ProgressBar progress(0, 0, 200, 20);
		DesignerControl progressTarget(
			&progress, L"progressBar1", UIClass::UI_ProgressBar);
		const auto progressRows = DesignerPropertyRowCatalog::GetControlRows(
			progressTarget, context);
		const auto* percentage = DesignerPropertyRowCatalog::Find(
			progressRows, L"PercentageValue");
		CUI_EXPECT_TRUE(percentage != nullptr);
		CUI_EXPECT_EQ(DesignerPropertyRowEditorKind::FloatSlider,
			percentage->Editor);
		CUI_EXPECT_TRUE(percentage->Minimum.has_value());
		CUI_EXPECT_TRUE(percentage->Maximum.has_value());

		MediaPlayer media(0, 0, 320, 180);
		DesignerControl mediaTarget(
			&media, L"mediaPlayer1", UIClass::UI_MediaPlayer);
		const auto mediaRows = DesignerPropertyRowCatalog::GetControlRows(
			mediaTarget, context);
		const auto* mediaFile = DesignerPropertyRowCatalog::Find(
			mediaRows, L"MediaFile");
		CUI_EXPECT_TRUE(mediaFile != nullptr);
		CUI_EXPECT_EQ(DesignerPropertyRowSource::ControlDesign,
			mediaFile->Source);
		CUI_EXPECT_EQ(std::wstring(L"Data"), mediaFile->Category);
	});

	runner.Add("Designer property rows expose value sources and tokenized filtering", []
	{
		Button button(L"Rows", 0, 0);
		(void)button.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local);
		CUI_EXPECT_TRUE(button.TrySetPropertyValue(
			L"Text", BindingValue(std::wstring(L"Styled")),
			ControlPropertyValueSource::Style));
		DesignerControl target(
			&button, L"button1", UIClass::UI_Button);
		DesignerControlPropertyContext context;
		auto rows = DesignerPropertyRowCatalog::GetControlRows(target, context);
		const auto* text = DesignerPropertyRowCatalog::Find(rows, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(text->EffectiveValueSource.has_value());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			*text->EffectiveValueSource);
		for (const auto& row : rows)
		{
			if (row.Source != DesignerPropertyRowSource::RuntimeMetadata) continue;
			CUI_EXPECT_TRUE(row.EffectiveValueSource.has_value());
			CUI_EXPECT_EQ(button.GetPropertyValueSource(row.Name),
				*row.EffectiveValueSource);
		}

		const auto styleRows = DesignerPropertyRowCatalog::FilterRows(
			rows, L"样式 text");
		CUI_EXPECT_EQ(1ULL, styleRows.size());
		CUI_EXPECT_EQ(std::wstring(L"Text"), styleRows[0].Name);
		const auto fontRows = DesignerPropertyRowCatalog::FilterRows(
			rows, L"appearance font");
		CUI_EXPECT_TRUE(fontRows.size() >= 2);
		for (const auto& row : fontRows)
			CUI_EXPECT_EQ(std::wstring(L"Appearance"), row.Category);
		const auto alignmentRows = DesignerPropertyRowCatalog::FilterRows(
			rows, L"stretch halign");
		CUI_EXPECT_EQ(1ULL, alignmentRows.size());
		CUI_EXPECT_EQ(std::wstring(L"HAlign"), alignmentRows[0].Name);
		CUI_EXPECT_EQ(rows.size(), DesignerPropertyRowCatalog::FilterRows(
			rows, L"   ").size());
		CUI_EXPECT_TRUE(DesignerPropertyRowCatalog::FilterRows(
			rows, L"not-a-property").empty());
		CUI_EXPECT_TRUE(DesignerPropertyRowCatalog::MatchesFilterText(
			L"DataContext Schema 数据上下文", L"schema data"));
		CUI_EXPECT_FALSE(DesignerPropertyRowCatalog::MatchesFilterText(
			L"DataContext Schema", L"schema style"));

		ObservableObject source;
		source.SetValue(L"Caption", std::wstring(L"Bound"));
		CUI_EXPECT_TRUE(button.DataBindings.Add(
			L"Text", source, L"Caption", BindingMode::OneWay) != nullptr);
		rows = DesignerPropertyRowCatalog::GetControlRows(target, context);
		text = DesignerPropertyRowCatalog::Find(rows, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			*text->EffectiveValueSource);
		const auto bindingRows = DesignerPropertyRowCatalog::FilterRows(
			rows, L"绑定 text");
		CUI_EXPECT_EQ(1ULL, bindingRows.size());
		CUI_EXPECT_EQ(std::wstring(L"Text"), bindingRows[0].Name);
	});

	runner.Add("Designer property rows expose binding validation and style diagnostics", []
	{
		Button button(L"Local caption", 0, 0);
		DesignerControl target(
			&button, L"button1", UIClass::UI_Button);
		target.DataBindings[L"Text"] = {
			L"Profile.Caption",
			BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged,
			L"StringTrim"
		};
		target.BindingPreviewStates[L"Text"] = {
			DesignerBindingPreviewStatus::Detached,
			L"未设置设计期 DataContext"
		};
		DesignerControlPropertyContext context;
		auto rows = DesignerPropertyRowCatalog::GetControlRows(target, context);
		const auto* text = DesignerPropertyRowCatalog::Find(rows, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(text->HasConfiguredBinding);
		CUI_EXPECT_FALSE(text->IsReadOnly);
		CUI_EXPECT_EQ(1ULL, text->Diagnostics.size());
		CUI_EXPECT_TRUE(text->Diagnostics.front().Summary.find(L"Profile.Caption")
			!= std::wstring::npos);
		CUI_EXPECT_EQ(1ULL, DesignerPropertyRowCatalog::FilterRows(
			rows, L"profile.caption stringtrim datacontext").size());

		MetadataObservableObject source;
		target.DataBindings[L"Text"].SourceProperty = L"Caption";
		source.SetValue(L"Caption", std::wstring(L"Bound caption"));
		source.SetValidationError(
			L"Caption", L"caption is required", L"required");
		(void)button.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local);
		CUI_EXPECT_TRUE(button.DataBindings.Add(
			L"Text", source, L"Caption", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged,
			BindingValueConverterRegistry::Create(L"StringTrim")) != nullptr);
		target.BindingPreviewStates[L"Text"] = {
			DesignerBindingPreviewStatus::Active,
			L"设计期预览绑定已连接"
		};
		rows = DesignerPropertyRowCatalog::GetControlRows(target, context);
		text = DesignerPropertyRowCatalog::Find(rows, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(text->IsReadOnly);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			*text->EffectiveValueSource);
		CUI_EXPECT_TRUE(std::any_of(
			text->Diagnostics.begin(), text->Diagnostics.end(),
			[](const DesignerPropertyDiagnostic& diagnostic)
			{
				return diagnostic.Kind
					== DesignerPropertyDiagnosticKind::Validation
					&& diagnostic.Severity
					== BindingValidationSeverity::Error;
			}));
		CUI_EXPECT_EQ(1ULL, DesignerPropertyRowCatalog::FilterRows(
			rows, L"caption required error").size());

		auto sheet = std::make_shared<ControlStyleSheet>();
		ControlStyleSelector selector;
		selector.Type = UIClass::UI_Button;
		const auto ruleId = sheet->AddRule(std::move(selector), {
			ControlStyleSetter(L"Round", BindingValue(8.0f))
		});
		CUI_EXPECT_TRUE(button.SetStyleSheet(sheet));
		button.Round = 5.0f;
		rows = DesignerPropertyRowCatalog::GetControlRows(target, context);
		const auto* round = DesignerPropertyRowCatalog::Find(rows, L"Round");
		CUI_EXPECT_TRUE(round != nullptr);
		CUI_EXPECT_TRUE(std::any_of(
			round->Diagnostics.begin(), round->Diagnostics.end(),
			[ruleId](const DesignerPropertyDiagnostic& diagnostic)
			{
				return diagnostic.Kind == DesignerPropertyDiagnosticKind::Style
					&& diagnostic.Summary.find(std::to_wstring(ruleId))
						!= std::wstring::npos
					&& diagnostic.Details.find(L"Local")
						!= std::wstring::npos;
			}));
		CUI_EXPECT_EQ(1ULL, DesignerPropertyRowCatalog::FilterRows(
			rows, L"style local round").size());
	});

	runner.Add("Designer multi-selection rows intersect properties and expose mixed values", []
	{
		Button first(L"First", 0, 0);
		Button second(L"Second", 0, 0);
		(void)first.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local);
		(void)second.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local);
		CUI_EXPECT_TRUE(first.TrySetPropertyValue(
			L"Text", BindingValue(std::wstring(L"One")),
			ControlPropertyValueSource::Local));
		CUI_EXPECT_TRUE(second.TrySetPropertyValue(
			L"Text", BindingValue(std::wstring(L"Two")),
			ControlPropertyValueSource::Style));
		DesignerControl firstTarget(
			&first, L"button1", UIClass::UI_Button);
		DesignerControl secondTarget(
			&second, L"button2", UIClass::UI_Button);
		DesignerControlPropertyContext context;
		const auto firstRows = DesignerPropertyRowCatalog::GetControlRows(
			firstTarget, context);
		auto secondRows = DesignerPropertyRowCatalog::GetControlRows(
			secondTarget, context);
		const auto common = DesignerPropertyRowCatalog::GetCommonControlRows(
			{ firstRows, secondRows });
		CUI_EXPECT_TRUE(DesignerPropertyRowCatalog::Find(common, L"Name") == nullptr);
		const auto* text = DesignerPropertyRowCatalog::Find(common, L"Text");
		const auto* anchor = DesignerPropertyRowCatalog::Find(common, L"Anchor");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(anchor != nullptr);
		CUI_EXPECT_TRUE(text->HasMixedValue);
		CUI_EXPECT_TRUE(text->HasMixedValueSource);
		CUI_EXPECT_FALSE(text->IsReadOnly);
		CUI_EXPECT_FALSE(text->EffectiveValueSource.has_value());
		CUI_EXPECT_FALSE(anchor->HasMixedValue);
		const auto mixedRows = DesignerPropertyRowCatalog::FilterRows(
			common, L"混合值 text");
		CUI_EXPECT_EQ(1ULL, mixedRows.size());
		CUI_EXPECT_EQ(std::wstring(L"Text"), mixedRows.front().Name);

		ObservableObject source;
		source.SetValue(L"Caption", std::wstring(L"Bound"));
		CUI_EXPECT_TRUE(second.DataBindings.Add(
			L"Text", source, L"Caption", BindingMode::OneWay) != nullptr);
		secondRows = DesignerPropertyRowCatalog::GetControlRows(
			secondTarget, context);
		const auto boundCommon =
			DesignerPropertyRowCatalog::GetCommonControlRows(
				{ firstRows, secondRows });
		const auto* boundText = DesignerPropertyRowCatalog::Find(
			boundCommon, L"Text");
		CUI_EXPECT_TRUE(boundText != nullptr);
		CUI_EXPECT_TRUE(boundText->IsReadOnly);
		CUI_EXPECT_EQ(1ULL, DesignerPropertyRowCatalog::FilterRows(
			boundCommon, L"只读 text").size());

		auto* incompatible = const_cast<DesignerPropertyRow*>(
			DesignerPropertyRowCatalog::Find(secondRows, L"Text"));
		CUI_EXPECT_TRUE(incompatible != nullptr);
		incompatible->Editor = DesignerPropertyRowEditorKind::Color;
		const auto compatibleOnly =
			DesignerPropertyRowCatalog::GetCommonControlRows(
				{ firstRows, secondRows });
		CUI_EXPECT_TRUE(DesignerPropertyRowCatalog::Find(
			compatibleOnly, L"Text") == nullptr);
	});

	runner.Add("Designer property edit transactions validate targets and report failures", []
	{
		DesignerControlPropertyContext context;
		ProgressBar progress(0, 0, 200, 20);
		DesignerControl progressTarget(
			&progress, L"progress1", UIClass::UI_ProgressBar);
		const auto progressRows = DesignerPropertyRowCatalog::GetControlRows(
			progressTarget, context);
		const auto* percentage = DesignerPropertyRowCatalog::Find(
			progressRows, L"PercentageValue");
		CUI_EXPECT_TRUE(percentage != nullptr);
		const auto initialPercentage = progress.GetPercentageValue();
		auto invalid = DesignerPropertyEdit::Apply(
			{ { &progressTarget, context } }, *percentage, L"not-a-number");
		CUI_EXPECT_FALSE(invalid.Succeeded);
		CUI_EXPECT_EQ(0ULL, invalid.AppliedCount);
		CUI_EXPECT_TRUE(invalid.Error.find(L"progress1") != std::wstring::npos);
		CUI_EXPECT_NEAR(initialPercentage,
			progress.GetPercentageValue(), 0.0001f);

		Button first(L"First", 0, 0);
		Button second(L"Second", 0, 0);
		DesignerControl firstTarget(
			&first, L"button1", UIClass::UI_Button);
		DesignerControl secondTarget(
			&second, L"button2", UIClass::UI_Button);
		auto firstRows = DesignerPropertyRowCatalog::GetControlRows(
			firstTarget, context);
		auto secondRows = DesignerPropertyRowCatalog::GetControlRows(
			secondTarget, context);
		auto common = DesignerPropertyRowCatalog::GetCommonControlRows(
			{ firstRows, secondRows });
		const auto* text = DesignerPropertyRowCatalog::Find(common, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		auto applied = DesignerPropertyEdit::Apply(
			{ { &firstTarget, context }, { &secondTarget, context } },
			*text,
			L"Shared");
		CUI_EXPECT_TRUE(applied.Succeeded);
		CUI_EXPECT_EQ(2ULL, applied.AppliedCount);
		CUI_EXPECT_EQ(std::wstring(L"Shared"), first.Text);
		CUI_EXPECT_EQ(std::wstring(L"Shared"), second.Text);
		const auto* anchor = DesignerPropertyRowCatalog::Find(common, L"Anchor");
		CUI_EXPECT_TRUE(anchor != nullptr);
		const auto firstAnchor = first.AnchorStyles;
		const auto secondAnchor = second.AnchorStyles;
		DesignerControlPropertyContext failingContext;
		failingContext.ApplyAnchorStylesKeepingBounds = [](
			Control* control, uint8_t value)
		{
			if (value == AnchorStyles::Right) throw 1;
			control->AnchorStyles = value;
		};
		auto rolledBack = DesignerPropertyEdit::Apply(
			{ { &firstTarget, context }, { &secondTarget, failingContext } },
			*anchor,
			std::to_wstring(AnchorStyles::Right));
		CUI_EXPECT_FALSE(rolledBack.Succeeded);
		CUI_EXPECT_EQ(0ULL, rolledBack.AppliedCount);
		CUI_EXPECT_TRUE(rolledBack.Error.find(L"setter") != std::wstring::npos);
		CUI_EXPECT_EQ(firstAnchor, first.AnchorStyles);
		CUI_EXPECT_EQ(secondAnchor, second.AnchorStyles);

		ObservableObject source;
		source.SetValue(L"Caption", std::wstring(L"Bound"));
		CUI_EXPECT_TRUE(second.ClearPropertyValue(
			L"Text", ControlPropertyValueSource::Local));
		CUI_EXPECT_TRUE(second.DataBindings.Add(
			L"Text", source, L"Caption", BindingMode::OneWay) != nullptr);
		firstRows = DesignerPropertyRowCatalog::GetControlRows(
			firstTarget, context);
		secondRows = DesignerPropertyRowCatalog::GetControlRows(
			secondTarget, context);
		common = DesignerPropertyRowCatalog::GetCommonControlRows(
			{ firstRows, secondRows });
		text = DesignerPropertyRowCatalog::Find(common, L"Text");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(text->IsReadOnly);
		auto rejected = DesignerPropertyEdit::Apply(
			{ { &firstTarget, context }, { &secondTarget, context } },
			*text,
			L"Rejected");
		CUI_EXPECT_FALSE(rejected.Succeeded);
		CUI_EXPECT_EQ(0ULL, rejected.AppliedCount);
		CUI_EXPECT_TRUE(rejected.Error.find(L"Binding") != std::wstring::npos);
		CUI_EXPECT_EQ(std::wstring(L"Shared"), first.Text);
		CUI_EXPECT_EQ(std::wstring(L"Bound"), second.Text);
		auto reset = DesignerPropertyEdit::Reset(
			{ { &firstTarget, context }, { &secondTarget, context } }, *text);
		CUI_EXPECT_FALSE(reset.Succeeded);
		CUI_EXPECT_EQ(0ULL, reset.AppliedCount);
		CUI_EXPECT_TRUE(reset.Error.find(L"Binding") != std::wstring::npos);
		CUI_EXPECT_EQ(std::wstring(L"Shared"), first.Text);
		CUI_EXPECT_EQ(std::wstring(L"Bound"), second.Text);
	});

	runner.Add("Container layout properties use Designer metadata end to end", []
	{
		StackPanel stack;
		const auto stackProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(stack);
		const auto* orientation =
			DesignerPropertyCatalog::Find(stackProperties, L"Orientation");
		const auto* spacing =
			DesignerPropertyCatalog::Find(stackProperties, L"Spacing");
		const auto* horizontal = DesignerPropertyCatalog::Find(
			stackProperties, L"HorizontalContentAlignment");
		const auto* vertical = DesignerPropertyCatalog::Find(
			stackProperties, L"VerticalContentAlignment");
		CUI_EXPECT_TRUE(orientation != nullptr);
		CUI_EXPECT_TRUE(spacing != nullptr);
		CUI_EXPECT_TRUE(horizontal != nullptr);
		CUI_EXPECT_TRUE(vertical != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, orientation->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, orientation->Editor);
		CUI_EXPECT_EQ(2ULL, orientation->Choices.size());
		CUI_EXPECT_EQ(std::wstring(L"0"), orientation->Choices[0].ValueText);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, spacing->Editor);
		CUI_EXPECT_TRUE(spacing->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *spacing->Minimum);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, spacing->Persistence);
		CUI_EXPECT_EQ(4ULL, horizontal->Choices.size());
		CUI_EXPECT_EQ(4ULL, vertical->Choices.size());
		CUI_EXPECT_TRUE(orientation->Order < spacing->Order);
		CUI_EXPECT_TRUE(spacing->Order < horizontal->Order);
		CUI_EXPECT_TRUE(horizontal->Order < vertical->Order);

		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			stack, L"Spacing", { DesignerStyleValueKind::Float, L"-4" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_NEAR(0.0f, stack.GetSpacing(), 0.0001f);
		CUI_EXPECT_EQ(std::wstring(L"0"), effectiveValue.Text);

		auto stackDesigner = std::make_shared<DesignerControl>(
			&stack, L"stackLayout", UIClass::UI_StackPanel);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			stack, L"Orientation", { DesignerStyleValueKind::Int, L"0" },
			&canonicalName, &effectiveValue, &error));
		stackDesigner->MetadataProperties[canonicalName] = effectiveValue;
		CUI_EXPECT_EQ(Orientation::Horizontal, stack.GetOrientation());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			stack, L"Spacing", { DesignerStyleValueKind::Float, L"6.5" },
			&canonicalName, &effectiveValue, &error));
		stackDesigner->MetadataProperties[canonicalName] = effectiveValue;

		WrapPanel wrap;
		const auto wrapProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(wrap);
		const auto* itemWidth =
			DesignerPropertyCatalog::Find(wrapProperties, L"ItemWidth");
		const auto* itemHeight =
			DesignerPropertyCatalog::Find(wrapProperties, L"ItemHeight");
		CUI_EXPECT_TRUE(itemWidth != nullptr);
		CUI_EXPECT_TRUE(itemHeight != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Item Width (0 = Auto)"), itemWidth->DisplayName);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, itemWidth->Persistence);
		CUI_EXPECT_TRUE(itemWidth->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *itemWidth->Minimum);
		CUI_EXPECT_TRUE(itemWidth->Order < itemHeight->Order);

		auto wrapDesigner = std::make_shared<DesignerControl>(
			&wrap, L"wrapLayout", UIClass::UI_WrapPanel);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			wrap, L"ItemWidth", { DesignerStyleValueKind::Float, L"72.25" },
			&canonicalName, &effectiveValue, &error));
		wrapDesigner->MetadataProperties[canonicalName] = effectiveValue;
		CUI_EXPECT_NEAR(72.25f, wrap.GetItemWidth(), 0.0001f);

		CodeGenInput input;
		input.Controls = { stackDesigner, wrapDesigner };
		CodeGenerator generator(L"ContainerMetadataForm", input);
		const auto header = generator.GenerateHeader();
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(header.find("#include \"Binding.h\"") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"stackLayout->TrySetPropertyValue(L\"Orientation\", BindingValue(0))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"stackLayout->TrySetPropertyValue(L\"Spacing\", BindingValue(6.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"wrapLayout->TrySetPropertyValue(L\"ItemWidth\", BindingValue(72.25f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("stackLayout->SetOrientation(") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("stackLayout->SetSpacing(") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("wrapLayout->SetItemWidth(") == std::string::npos);
	});

	runner.Add("Dock and split containers use observable Designer metadata", []
	{
		DockPanel dock;
		const auto dockProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(dock);
		const auto* lastChildFill =
			DesignerPropertyCatalog::Find(dockProperties, L"LastChildFill");
		CUI_EXPECT_TRUE(lastChildFill != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, lastChildFill->Editor);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, lastChildFill->Persistence);
		dock.SetLastChildFill(false);
		CUI_EXPECT_FALSE(dock.GetLastChildFill());
		CUI_EXPECT_FALSE(dock.IsPropertyValueDefault(L"LastChildFill"));
		CUI_EXPECT_TRUE(dock.ResetPropertyValue(L"LastChildFill"));
		CUI_EXPECT_TRUE(dock.GetLastChildFill());

		ObservableObject dockSource;
		dockSource.SetValue(L"Fill", false);
		CUI_EXPECT_TRUE(dock.DataBindings.Add(
			L"LastChildFill", dockSource, L"Fill", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_FALSE(dock.GetLastChildFill());
		dock.SetLastChildFill(true);
		CUI_EXPECT_TRUE(dockSource.GetValue<bool>(L"Fill"));

		SplitContainer split(0, 0, 360, 220);
		const auto splitProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(split);
		const auto* orientation =
			DesignerPropertyCatalog::Find(splitProperties, L"SplitOrientation");
		const auto* distance =
			DesignerPropertyCatalog::Find(splitProperties, L"SplitterDistance");
		const auto* width =
			DesignerPropertyCatalog::Find(splitProperties, L"SplitterWidth");
		const auto* fixed =
			DesignerPropertyCatalog::Find(splitProperties, L"IsSplitterFixed");
		const auto* splitterColor =
			DesignerPropertyCatalog::Find(splitProperties, L"SplitterColor");
		const auto* cornerRadius =
			DesignerPropertyCatalog::Find(splitProperties, L"SplitterCornerRadius");
		CUI_EXPECT_TRUE(orientation != nullptr);
		CUI_EXPECT_TRUE(distance != nullptr);
		CUI_EXPECT_TRUE(width != nullptr);
		CUI_EXPECT_TRUE(fixed != nullptr);
		CUI_EXPECT_TRUE(splitterColor != nullptr);
		CUI_EXPECT_TRUE(cornerRadius != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, orientation->Editor);
		CUI_EXPECT_EQ(2ULL, orientation->Choices.size());
		CUI_EXPECT_EQ(std::wstring(L"Layout"), distance->Category);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, fixed->Editor);
		CUI_EXPECT_EQ(std::wstring(L"Appearance"), splitterColor->Category);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, splitterColor->Editor);
		CUI_EXPECT_TRUE(width->Minimum.has_value());
		CUI_EXPECT_EQ(1.0, *width->Minimum);
		CUI_EXPECT_TRUE(cornerRadius->Minimum.has_value());
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, cornerRadius->Persistence);

		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			split, L"SplitterWidth", { DesignerStyleValueKind::Int, L"-5" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_EQ(1, split.GetSplitterWidth());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			split, L"Panel2MinSize", { DesignerStyleValueKind::Int, L"100" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			split, L"SplitterDistance", { DesignerStyleValueKind::Int, L"999" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_EQ(259, split.GetSplitterDistance());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			split, L"SplitOrientation", { DesignerStyleValueKind::Int, L"1" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_EQ(Orientation::Vertical, split.GetSplitOrientation());
		CUI_EXPECT_EQ(119, split.GetSplitterDistance());
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			split, L"SplitterCornerRadius", { DesignerStyleValueKind::Float, L"-2" },
			&canonicalName, &effectiveValue, &error));
		CUI_EXPECT_NEAR(0.0f, split.GetSplitterCornerRadius(), 0.0001f);

		ObservableObject splitSource;
		splitSource.SetValue(L"Distance", 80);
		SplitContainer boundSplit(0, 0, 360, 220);
		CUI_EXPECT_TRUE(boundSplit.DataBindings.Add(
			L"SplitterDistance", splitSource, L"Distance", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_EQ(80, boundSplit.GetSplitterDistance());
		boundSplit.SetSplitterDistance(96);
		CUI_EXPECT_EQ(96, splitSource.GetValue<int>(L"Distance"));

		auto dockDesigner = std::make_shared<DesignerControl>(
			&dock, L"dockLayout", UIClass::UI_DockPanel);
		auto splitDesigner = std::make_shared<DesignerControl>(
			&split, L"splitLayout", UIClass::UI_SplitContainer);
		auto applyTracked = [&](Control& target,
			const std::shared_ptr<DesignerControl>& designerControl,
			const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				target, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designerControl->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(dock, dockDesigner, L"LastChildFill",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(split, splitDesigner, L"SplitOrientation",
			{ DesignerStyleValueKind::Int, L"1" });
		applyTracked(split, splitDesigner, L"SplitterDistance",
			{ DesignerStyleValueKind::Int, L"80" });
		applyTracked(split, splitDesigner, L"SplitterCornerRadius",
			{ DesignerStyleValueKind::Float, L"4.5" });

		CodeGenInput input;
		input.Controls = { dockDesigner, splitDesigner };
		CodeGenerator generator(L"DockSplitMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"dockLayout->TrySetPropertyValue(L\"LastChildFill\", BindingValue(false))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"splitLayout->TrySetPropertyValue(L\"SplitOrientation\", BindingValue(1))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"splitLayout->TrySetPropertyValue(L\"SplitterDistance\", BindingValue(80))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"splitLayout->TrySetPropertyValue(L\"SplitterCornerRadius\", BindingValue(4.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("dockLayout->SetLastChildFill(") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("splitLayout->SplitterDistance =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("splitLayout->SplitOrientation =") == std::string::npos);
	});

	runner.Add("Slider and numeric input share range and Designer metadata", []
	{
		Slider slider(0, 0, 240, 32);
		const auto sliderProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(slider);
		const auto* sliderMinimum =
			DesignerPropertyCatalog::Find(sliderProperties, L"Min");
		const auto* sliderValue =
			DesignerPropertyCatalog::Find(sliderProperties, L"Value");
		const auto* sliderTrackColor =
			DesignerPropertyCatalog::Find(sliderProperties, L"TrackBackColor");
		const auto* sliderTrackHeight =
			DesignerPropertyCatalog::Find(sliderProperties, L"TrackHeight");
		CUI_EXPECT_TRUE(sliderMinimum != nullptr);
		CUI_EXPECT_TRUE(sliderValue != nullptr);
		CUI_EXPECT_TRUE(sliderTrackColor != nullptr);
		CUI_EXPECT_TRUE(sliderTrackHeight != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Range"), sliderMinimum->Category);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, sliderValue->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, sliderTrackColor->Editor);
		CUI_EXPECT_TRUE(sliderTrackHeight->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *sliderTrackHeight->Minimum);

		int sliderValueChanges = 0;
		auto sliderValueConnection = slider.OnValueChanged.Subscribe(
			[&](Control*, float, float) { ++sliderValueChanges; });
		slider.Value = 80.0f;
		slider.Max = 40.0f;
		CUI_EXPECT_NEAR(40.0f, slider.Value, 0.0001f);
		CUI_EXPECT_EQ(2, sliderValueChanges);
		slider.Min = 50.0f;
		CUI_EXPECT_NEAR(50.0f, slider.Min, 0.0001f);
		CUI_EXPECT_NEAR(50.0f, slider.Max, 0.0001f);
		CUI_EXPECT_NEAR(50.0f, slider.Value, 0.0001f);
		CUI_EXPECT_EQ(3, sliderValueChanges);
		slider.Max = 20.0f;
		CUI_EXPECT_NEAR(50.0f, slider.Max, 0.0001f);
		slider.Min = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(50.0f, slider.Min, 0.0001f);

		Slider snapped(0, 0, 240, 32);
		snapped.Min = -10.0f;
		snapped.Max = 10.0f;
		snapped.Step = 3.0f;
		snapped.Value = 4.0f;
		CUI_EXPECT_NEAR(4.0f, snapped.Value, 0.0001f);
		snapped.SnapToStep = true;
		CUI_EXPECT_NEAR(5.0f, snapped.Value, 0.0001f);
		snapped.Step = -2.0f;
		CUI_EXPECT_NEAR(0.0f, snapped.Step, 0.0001f);
		snapped.TrackHeight = -3.0f;
		CUI_EXPECT_NEAR(0.0f, snapped.TrackHeight, 0.0001f);

		Slider styledValue(0, 0, 240, 32);
		CUI_EXPECT_TRUE(styledValue.TrySetPropertyValue(
			L"Value", BindingValue(75.0f), ControlPropertyValueSource::Style));
		styledValue.Max = 25.0f;
		CUI_EXPECT_NEAR(25.0f, styledValue.Value, 0.0001f);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Style,
			styledValue.GetPropertyValueSource(L"Value"));

		ObservableObject sliderSource;
		sliderSource.SetValue(L"Current", 12.0f);
		Slider boundSlider(0, 0, 240, 32);
		CUI_EXPECT_TRUE(boundSlider.DataBindings.Add(
			L"Value", sliderSource, L"Current", BindingMode::TwoWay) != nullptr);
		boundSlider.Value = 18.0f;
		CUI_EXPECT_NEAR(18.0f,
			sliderSource.GetValue<float>(L"Current"), 0.0001f);

		NumericUpDown numeric(0, 0, 140, 30);
		const auto numericProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(numeric);
		const auto* decimalPlaces =
			DesignerPropertyCatalog::Find(numericProperties, L"DecimalPlaces");
		const auto* selectAll =
			DesignerPropertyCatalog::Find(numericProperties, L"SelectAllOnFocus");
		const auto* buttonWidth =
			DesignerPropertyCatalog::Find(numericProperties, L"ButtonWidth");
		const auto* buttonColor =
			DesignerPropertyCatalog::Find(numericProperties, L"ButtonBackColor");
		CUI_EXPECT_TRUE(decimalPlaces != nullptr);
		CUI_EXPECT_TRUE(selectAll != nullptr);
		CUI_EXPECT_TRUE(buttonWidth != nullptr);
		CUI_EXPECT_TRUE(buttonColor != nullptr);
		CUI_EXPECT_TRUE(decimalPlaces->Maximum.has_value());
		CUI_EXPECT_EQ(15.0, *decimalPlaces->Maximum);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, selectAll->Editor);
		CUI_EXPECT_EQ(std::wstring(L"Appearance"), buttonWidth->Category);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, buttonColor->Editor);

		int numericValueChanges = 0;
		auto numericValueConnection = numeric.OnValueChanged.Subscribe(
			[&](NumericUpDown*, double, double) { ++numericValueChanges; });
		numeric.SnapToStep = false;
		numeric.Value = 75.0;
		numeric.Max = 30.0;
		CUI_EXPECT_NEAR(30.0, numeric.Value, 0.0000001);
		CUI_EXPECT_EQ(2, numericValueChanges);
		numeric.Min = 40.0;
		CUI_EXPECT_NEAR(40.0, numeric.Max, 0.0000001);
		CUI_EXPECT_NEAR(40.0, numeric.Value, 0.0000001);
		CUI_EXPECT_EQ(3, numericValueChanges);
		numeric.DecimalPlaces = 99;
		CUI_EXPECT_EQ(15, numeric.DecimalPlaces);
		numeric.DecimalPlaces = -2;
		CUI_EXPECT_EQ(0, numeric.DecimalPlaces);
		numeric.Step = -4.0;
		CUI_EXPECT_NEAR(0.0, numeric.Step, 0.0000001);
		numeric.ButtonWidth = -8.0f;
		CUI_EXPECT_NEAR(0.0f, numeric.ButtonWidth, 0.0001f);

		ObservableObject numericSource;
		numericSource.SetValue(L"Current", 6.5);
		NumericUpDown boundNumeric(0, 0, 140, 30);
		boundNumeric.SnapToStep = false;
		CUI_EXPECT_TRUE(boundNumeric.DataBindings.Add(
			L"Value", numericSource, L"Current", BindingMode::TwoWay) != nullptr);
		boundNumeric.Value = 7.25;
		CUI_EXPECT_NEAR(7.25,
			numericSource.GetValue<double>(L"Current"), 0.0000001);

		Slider generatedSlider(0, 0, 240, 32);
		NumericUpDown generatedNumeric(0, 0, 140, 30);
		auto sliderDesigner = std::make_shared<DesignerControl>(
			&generatedSlider, L"rangeSlider", UIClass::UI_Slider);
		auto numericDesigner = std::make_shared<DesignerControl>(
			&generatedNumeric, L"numericInput", UIClass::UI_NumericUpDown);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](Control& target,
			const std::shared_ptr<DesignerControl>& designerControl,
			const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				target, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designerControl->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(generatedSlider, sliderDesigner, L"Min",
			{ DesignerStyleValueKind::Float, L"-100" });
		applyTracked(generatedSlider, sliderDesigner, L"Max",
			{ DesignerStyleValueKind::Float, L"-20" });
		applyTracked(generatedSlider, sliderDesigner, L"Step",
			{ DesignerStyleValueKind::Float, L"5" });
		applyTracked(generatedSlider, sliderDesigner, L"SnapToStep",
			{ DesignerStyleValueKind::Bool, L"true" });
		applyTracked(generatedSlider, sliderDesigner, L"Value",
			{ DesignerStyleValueKind::Float, L"-33" });
		applyTracked(generatedSlider, sliderDesigner, L"TrackHeight",
			{ DesignerStyleValueKind::Float, L"7.5" });
		applyTracked(generatedNumeric, numericDesigner, L"Min",
			{ DesignerStyleValueKind::Double, L"-10" });
		applyTracked(generatedNumeric, numericDesigner, L"Max",
			{ DesignerStyleValueKind::Double, L"10" });
		applyTracked(generatedNumeric, numericDesigner, L"Step",
			{ DesignerStyleValueKind::Double, L"0.25" });
		applyTracked(generatedNumeric, numericDesigner, L"DecimalPlaces",
			{ DesignerStyleValueKind::Int, L"2" });
		applyTracked(generatedNumeric, numericDesigner, L"SnapToStep",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(generatedNumeric, numericDesigner, L"Value",
			{ DesignerStyleValueKind::Double, L"3.75" });

		CodeGenInput input;
		input.Controls = { sliderDesigner, numericDesigner };
		CodeGenerator generator(L"RangeMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto sliderMinPosition = cpp.find(
			"rangeSlider->TrySetPropertyValue(L\"Min\", BindingValue(-100.f))");
		const auto sliderMaxPosition = cpp.find(
			"rangeSlider->TrySetPropertyValue(L\"Max\", BindingValue(-20.f))");
		const auto sliderValuePosition = cpp.find(
			"rangeSlider->TrySetPropertyValue(L\"Value\", BindingValue(-35.f))");
		CUI_EXPECT_TRUE(sliderMinPosition != std::string::npos);
		CUI_EXPECT_TRUE(sliderMaxPosition != std::string::npos);
		CUI_EXPECT_TRUE(sliderValuePosition != std::string::npos);
		CUI_EXPECT_TRUE(sliderMinPosition < sliderMaxPosition);
		CUI_EXPECT_TRUE(sliderMaxPosition < sliderValuePosition);
		CUI_EXPECT_TRUE(cpp.find(
			"numericInput->TrySetPropertyValue(L\"DecimalPlaces\", BindingValue(2))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"rangeSlider->TrySetPropertyValue(L\"TrackHeight\", BindingValue(7.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("rangeSlider->Min =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("rangeSlider->SnapToStep =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("numericInput->DecimalPlaces =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("numericInput->UseMouseWheel =") == std::string::npos);
	});

	runner.Add("GroupBox and Expander use complete Designer metadata", []
	{
		GroupBox groupBox(L"Details", 0, 0, 240, 180);
		const auto groupProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(groupBox);
		const auto* captionMargin =
			DesignerPropertyCatalog::Find(groupProperties, L"CaptionMarginLeft");
		const auto* captionRadius =
			DesignerPropertyCatalog::Find(groupProperties, L"CaptionCornerRadius");
		const auto* captionBack =
			DesignerPropertyCatalog::Find(groupProperties, L"CaptionBackColor");
		CUI_EXPECT_TRUE(captionMargin != nullptr);
		CUI_EXPECT_TRUE(captionRadius != nullptr);
		CUI_EXPECT_TRUE(captionBack != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Caption"), captionMargin->Category);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata,
			captionMargin->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, captionRadius->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, captionBack->Editor);
		CUI_EXPECT_TRUE(captionMargin->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *captionMargin->Minimum);

		groupBox.CaptionPaddingY = -3.0f;
		CUI_EXPECT_NEAR(0.0f, groupBox.CaptionPaddingY, 0.0001f);
		groupBox.CaptionMarginLeft = 18.0f;
		groupBox.CaptionMarginLeft = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(18.0f, groupBox.CaptionMarginLeft, 0.0001f);
		groupBox.CaptionCornerRadius = -4.0f;
		CUI_EXPECT_NEAR(0.0f, groupBox.CaptionCornerRadius, 0.0001f);

		Expander expander(L"Advanced", 0, 0, 260, 160);
		const auto expanderProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(expander);
		const auto* headerHeight =
			DesignerPropertyCatalog::Find(expanderProperties, L"HeaderHeight");
		const auto* animationDuration =
			DesignerPropertyCatalog::Find(expanderProperties, L"AnimationDurationMs");
		const auto* expanded =
			DesignerPropertyCatalog::Find(expanderProperties, L"IsExpanded");
		const auto* surfaceColor =
			DesignerPropertyCatalog::Find(expanderProperties, L"SurfaceColor");
		CUI_EXPECT_TRUE(headerHeight != nullptr);
		CUI_EXPECT_TRUE(animationDuration != nullptr);
		CUI_EXPECT_TRUE(expanded != nullptr);
		CUI_EXPECT_TRUE(surfaceColor != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Layout"), headerHeight->Category);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Int,
			animationDuration->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, expanded->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, surfaceColor->Editor);
		CUI_EXPECT_TRUE(animationDuration->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *animationDuration->Minimum);

		int expandedChanges = 0;
		auto expandedConnection = expander.OnExpandedChanged.Subscribe(
			[&](Expander*, bool) { ++expandedChanges; });
		expander.AnimationDurationMs = 0;
		expander.SetExpanded(false);
		CUI_EXPECT_FALSE(expander.IsExpanded);
		CUI_EXPECT_EQ(1, expandedChanges);
		expander.SetExpanded(false);
		CUI_EXPECT_EQ(1, expandedChanges);
		expander.SetExpanded(true);
		CUI_EXPECT_TRUE(expander.IsExpanded);
		CUI_EXPECT_EQ(2, expandedChanges);
		expander.HeaderHeight = -8.0f;
		CUI_EXPECT_NEAR(0.0f, expander.HeaderHeight, 0.0001f);
		expander.HeaderHeight = 42.0f;
		expander.HeaderHeight = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(42.0f, expander.HeaderHeight, 0.0001f);
		expander.Border = -2.0f;
		CUI_EXPECT_NEAR(0.0f, expander.Border, 0.0001f);
		CUI_EXPECT_TRUE(expander.TrySetPropertyValue(
			L"AnimationDurationMs", BindingValue(-25)));
		CUI_EXPECT_EQ(0U, expander.AnimationDurationMs);

		ObservableObject source;
		source.SetValue(L"Expanded", false);
		Expander boundExpander(L"Bound", 0, 0, 260, 160);
		boundExpander.AnimationDurationMs = 0;
		CUI_EXPECT_TRUE(boundExpander.DataBindings.Add(
			L"IsExpanded", source, L"Expanded", BindingMode::TwoWay) != nullptr);
		boundExpander.Toggle();
		CUI_EXPECT_TRUE(source.GetValue<bool>(L"Expanded"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			boundExpander.GetPropertyValueSource(L"IsExpanded"));

		GroupBox generatedGroup(L"Generated", 0, 0, 240, 180);
		Expander generatedExpander(L"Generated", 0, 0, 260, 160);
		auto groupDesigner = std::make_shared<DesignerControl>(
			&generatedGroup, L"detailsGroup", UIClass::UI_GroupBox);
		auto expanderDesigner = std::make_shared<DesignerControl>(
			&generatedExpander, L"advancedPanel", UIClass::UI_Expander);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](Control& target,
			const std::shared_ptr<DesignerControl>& designerControl,
			const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				target, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designerControl->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(generatedGroup, groupDesigner, L"CaptionPaddingY",
			{ DesignerStyleValueKind::Float, L"4.5" });
		applyTracked(generatedGroup, groupDesigner, L"CaptionBackColor",
			{ DesignerStyleValueKind::Color, L"#CC102030" });
		applyTracked(generatedExpander, expanderDesigner, L"HeaderHeight",
			{ DesignerStyleValueKind::Float, L"44" });
		applyTracked(generatedExpander, expanderDesigner, L"AnimationDurationMs",
			{ DesignerStyleValueKind::Int, L"240" });
		applyTracked(generatedExpander, expanderDesigner, L"IsExpanded",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(generatedExpander, expanderDesigner, L"AccentColor",
			{ DesignerStyleValueKind::Color, L"#FF336699" });

		CodeGenInput input;
		input.Controls = { groupDesigner, expanderDesigner };
		CodeGenerator generator(L"ContainerMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"detailsGroup->TrySetPropertyValue(L\"CaptionPaddingY\", BindingValue(4.5f))")
			!= std::string::npos);
		const auto animationPosition = cpp.find(
			"advancedPanel->TrySetPropertyValue(L\"AnimationDurationMs\", BindingValue(240))");
		const auto expandedPosition = cpp.find(
			"advancedPanel->TrySetPropertyValue(L\"IsExpanded\", BindingValue(false))");
		CUI_EXPECT_TRUE(animationPosition != std::string::npos);
		CUI_EXPECT_TRUE(expandedPosition != std::string::npos);
		CUI_EXPECT_TRUE(animationPosition < expandedPosition);
		CUI_EXPECT_TRUE(cpp.find("detailsGroup->CaptionPaddingY =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("advancedPanel->HeaderHeight =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("advancedPanel->SetExpanded(") == std::string::npos);
	});

	runner.Add("ScrollView configuration uses metadata while offsets stay transient", []
	{
		ScrollView scroll(0, 0, 100, 80);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(scroll);
		const auto* autoContent =
			DesignerPropertyCatalog::Find(properties, L"AutoContentSize");
		const auto* contentSize =
			DesignerPropertyCatalog::Find(properties, L"ContentSize");
		const auto* scrollBarThickness =
			DesignerPropertyCatalog::Find(properties, L"ScrollBarThickness");
		const auto* wheelStep =
			DesignerPropertyCatalog::Find(properties, L"MouseWheelStep");
		const auto* borderThickness =
			DesignerPropertyCatalog::Find(properties, L"BorderThickness");
		const auto* scrollBackColor =
			DesignerPropertyCatalog::Find(properties, L"ScrollBackColor");
		CUI_EXPECT_TRUE(autoContent != nullptr);
		CUI_EXPECT_TRUE(contentSize != nullptr);
		CUI_EXPECT_TRUE(scrollBarThickness != nullptr);
		CUI_EXPECT_TRUE(wheelStep != nullptr);
		CUI_EXPECT_TRUE(borderThickness != nullptr);
		CUI_EXPECT_TRUE(scrollBackColor != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, autoContent->Editor);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Size, contentSize->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, scrollBarThickness->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, scrollBackColor->Editor);
		CUI_EXPECT_TRUE(scrollBarThickness->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *scrollBarThickness->Minimum);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"ScrollXOffset") == nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"ScrollYOffset") == nullptr);

		scroll.ContentSize = SIZE{ -20, 240 };
		CUI_EXPECT_EQ(0L, scroll.ContentSize.cx);
		CUI_EXPECT_EQ(240L, scroll.ContentSize.cy);
		scroll.ScrollBarThickness = -3.0f;
		CUI_EXPECT_NEAR(0.0f, scroll.ScrollBarThickness, 0.0001f);
		scroll.ScrollBarThickness = 12.0f;
		scroll.ScrollBarThickness = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(12.0f, scroll.ScrollBarThickness, 0.0001f);
		scroll.BorderThickness = -2.0f;
		CUI_EXPECT_NEAR(0.0f, scroll.BorderThickness, 0.0001f);
		scroll.MouseWheelStep = -8;
		CUI_EXPECT_EQ(0, scroll.MouseWheelStep);

		scroll.AutoContentSize = false;
		scroll.ContentSize = SIZE{ 300, 240 };
		auto clip = scroll.GetChildrenClipRect();
		CUI_EXPECT_NEAR(88.0f, clip.right, 0.0001f);
		CUI_EXPECT_NEAR(68.0f, clip.bottom, 0.0001f);

		ObservableObject source;
		source.SetValue(L"WheelStep", 24);
		ScrollView boundScroll(0, 0, 100, 80);
		CUI_EXPECT_TRUE(boundScroll.DataBindings.Add(
			L"MouseWheelStep", source, L"WheelStep", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_EQ(24, boundScroll.MouseWheelStep);
		boundScroll.MouseWheelStep = 60;
		CUI_EXPECT_EQ(60, source.GetValue<int>(L"WheelStep"));

		ScrollView generatedScroll(0, 0, 240, 180);
		auto scrollDesigner = std::make_shared<DesignerControl>(
			&generatedScroll, L"scrollPanel", UIClass::UI_ScrollView);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generatedScroll, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			scrollDesigner->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"AutoContentSize",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(L"ContentSize",
			{ DesignerStyleValueKind::Size, L"320, 240" });
		applyTracked(L"AlwaysShowVScroll",
			{ DesignerStyleValueKind::Bool, L"true" });
		applyTracked(L"ScrollBarThickness",
			{ DesignerStyleValueKind::Float, L"10.5" });
		applyTracked(L"MouseWheelStep",
			{ DesignerStyleValueKind::Int, L"72" });
		applyTracked(L"BorderThickness",
			{ DesignerStyleValueKind::Float, L"2.5" });
		applyTracked(L"ScrollForeColor",
			{ DesignerStyleValueKind::Color, L"#FF204060" });
		generatedScroll.ScrollXOffset = 12;
		generatedScroll.ScrollYOffset = 18;

		CodeGenInput input;
		input.Controls = { scrollDesigner };
		CodeGenerator generator(L"ScrollMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto autoPosition = cpp.find(
			"scrollPanel->TrySetPropertyValue(L\"AutoContentSize\", BindingValue(false))");
		const auto sizePosition = cpp.find(
			"scrollPanel->TrySetPropertyValue(L\"ContentSize\", BindingValue(SIZE{ 320, 240 }))");
		CUI_EXPECT_TRUE(autoPosition != std::string::npos);
		CUI_EXPECT_TRUE(sizePosition != std::string::npos);
		CUI_EXPECT_TRUE(autoPosition < sizePosition);
		CUI_EXPECT_TRUE(cpp.find(
			"scrollPanel->TrySetPropertyValue(L\"ScrollBarThickness\", BindingValue(10.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"scrollPanel->TrySetPropertyValue(L\"MouseWheelStep\", BindingValue(72))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("scrollPanel->ScrollBackColor =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("scrollPanel->AutoContentSize =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("scrollPanel->ContentSize =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("scrollPanel->SetScrollOffset(") == std::string::npos);
	});

	runner.Add("Panel visual metadata shares storage across derived controls", []
	{
		Panel panel;
		const auto panelProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(panel);
		const auto* borderThickness =
			DesignerPropertyCatalog::Find(panelProperties, L"BorderThickness");
		const auto* cornerRadius =
			DesignerPropertyCatalog::Find(panelProperties, L"CornerRadius");
		const auto* disabledOverlay =
			DesignerPropertyCatalog::Find(panelProperties, L"DisabledOverlayColor");
		CUI_EXPECT_TRUE(borderThickness != nullptr);
		CUI_EXPECT_TRUE(cornerRadius != nullptr);
		CUI_EXPECT_TRUE(disabledOverlay != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, cornerRadius->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, disabledOverlay->Editor);
		CUI_EXPECT_TRUE(cornerRadius->Minimum.has_value());
		CUI_EXPECT_EQ(0.0, *cornerRadius->Minimum);

		panel.BorderThickness = -2.0f;
		panel.CornerRadius = -4.0f;
		CUI_EXPECT_NEAR(0.0f, panel.BorderThickness, 0.0001f);
		CUI_EXPECT_NEAR(0.0f, panel.CornerRadius, 0.0001f);
		panel.CornerRadius = 3.5f;
		panel.CornerRadius = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(3.5f, panel.CornerRadius, 0.0001f);

		ObservableObject source;
		source.SetValue(L"Radius", 6.0f);
		Panel boundPanel;
		CUI_EXPECT_TRUE(boundPanel.DataBindings.Add(
			L"CornerRadius", source, L"Radius", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_NEAR(6.0f, boundPanel.CornerRadius, 0.0001f);
		boundPanel.CornerRadius = 4.0f;
		CUI_EXPECT_NEAR(4.0f,
			source.GetValue<float>(L"Radius"), 0.0001f);

		ToolBar toolBar(0, 0, 320, 34);
		CUI_EXPECT_NEAR(8.0f, toolBar.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(toolBar.IsPropertyValueDefault(L"CornerRadius"));
		const auto* toolBarRadiusMetadata =
			toolBar.FindPropertyMetadata(L"CornerRadius");
		CUI_EXPECT_TRUE(toolBarRadiusMetadata != nullptr);
		CUI_EXPECT_TRUE(toolBarRadiusMetadata->OwnerType()
			== std::type_index(typeid(ToolBar)));
		Panel& toolBarBase = toolBar;
		toolBarBase.CornerRadius = 5.0f;
		CUI_EXPECT_NEAR(5.0f, toolBar.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(toolBar.ResetPropertyValue(L"CornerRadius"));
		CUI_EXPECT_NEAR(8.0f, toolBarBase.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(toolBar.TrySetPropertyValue(
			L"CornerRadius", BindingValue(2.5f),
			ControlPropertyValueSource::Style));
		CUI_EXPECT_NEAR(2.5f, toolBarBase.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(toolBar.ClearPropertyValue(
			L"CornerRadius", ControlPropertyValueSource::Style));
		CUI_EXPECT_NEAR(8.0f, toolBar.CornerRadius, 0.0001f);
		ObservableObject toolBarSource;
		toolBarSource.SetValue(L"Radius", 9.0f);
		ToolBar boundToolBar(0, 0, 320, 34);
		CUI_EXPECT_TRUE(boundToolBar.DataBindings.Add(
			L"CornerRadius", toolBarSource, L"Radius", BindingMode::TwoWay)
			!= nullptr);
		CUI_EXPECT_NEAR(9.0f, boundToolBar.CornerRadius, 0.0001f);
		boundToolBar.CornerRadius = 4.25f;
		CUI_EXPECT_NEAR(4.25f,
			toolBarSource.GetValue<float>(L"Radius"), 0.0001f);

		StatusBar statusBar(0, 0, 320, 26);
		Panel& statusBarBase = statusBar;
		CUI_EXPECT_NEAR(0.0f, statusBar.CornerRadius, 0.0001f);
		statusBarBase.CornerRadius = 3.0f;
		CUI_EXPECT_NEAR(3.0f, statusBar.CornerRadius, 0.0001f);

		PagedGridView pagedGrid(0, 0, 320, 220);
		CUI_EXPECT_NEAR(8.0f, pagedGrid.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(pagedGrid.IsPropertyValueDefault(L"CornerRadius"));
		const auto* pagedRadiusMetadata =
			pagedGrid.FindPropertyMetadata(L"CornerRadius");
		CUI_EXPECT_TRUE(pagedRadiusMetadata != nullptr);
		CUI_EXPECT_TRUE(pagedRadiusMetadata->OwnerType()
			== std::type_index(typeid(PagedGridView)));
		Panel& pagedGridBase = pagedGrid;
		pagedGridBase.CornerRadius = 4.5f;
		CUI_EXPECT_NEAR(4.5f, pagedGrid.CornerRadius, 0.0001f);
		CUI_EXPECT_TRUE(pagedGrid.ResetPropertyValue(L"CornerRadius"));
		CUI_EXPECT_NEAR(8.0f, pagedGridBase.CornerRadius, 0.0001f);

		Expander expander;
		Panel& expanderBase = expander;
		CUI_EXPECT_NEAR(7.0f, expander.CornerRadius, 0.0001f);
		expanderBase.CornerRadius = 3.0f;
		CUI_EXPECT_NEAR(3.0f, expander.CornerRadius, 0.0001f);
		const D2D1_COLOR_F overlay{ 0.1f, 0.2f, 0.3f, 0.4f };
		expanderBase.DisabledOverlayColor = overlay;
		CUI_EXPECT_EQ(overlay.r, expander.DisabledOverlayColor.r);
		CUI_EXPECT_EQ(overlay.g, expander.DisabledOverlayColor.g);
		CUI_EXPECT_EQ(overlay.b, expander.DisabledOverlayColor.b);
		CUI_EXPECT_EQ(overlay.a, expander.DisabledOverlayColor.a);

		ScrollView scroll;
		const auto* scrollBorderMetadata =
			scroll.FindPropertyMetadata(L"BorderThickness");
		CUI_EXPECT_TRUE(scrollBorderMetadata != nullptr);
		CUI_EXPECT_TRUE(scrollBorderMetadata->OwnerType()
			== std::type_index(typeid(Panel)));

		Panel generatedPanel;
		auto panelDesigner = std::make_shared<DesignerControl>(
			&generatedPanel, L"surface", UIClass::UI_Panel);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generatedPanel, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			panelDesigner->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"BorderThickness",
			{ DesignerStyleValueKind::Float, L"2.5" });
		applyTracked(L"CornerRadius",
			{ DesignerStyleValueKind::Float, L"6" });

		CodeGenInput input;
		input.Controls = { panelDesigner };
		CodeGenerator generator(L"PanelMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"surface->TrySetPropertyValue(L\"BorderThickness\", BindingValue(2.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"surface->TrySetPropertyValue(L\"CornerRadius\", BindingValue(6.f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("surface->CornerRadius =") == std::string::npos);
	});

	runner.Add("ToolBar and StatusBar use metadata without hiding Padding", []
	{
		ToolBar toolBar(0, 0, 320, 34);
		const auto toolProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(toolBar);
		const auto* padding = toolBar.FindPropertyMetadata(L"Padding");
		const auto* horizontalPadding =
			DesignerPropertyCatalog::Find(toolProperties, L"HorizontalPadding");
		const auto* gap =
			DesignerPropertyCatalog::Find(toolProperties, L"Gap");
		const auto* itemHeight =
			DesignerPropertyCatalog::Find(toolProperties, L"ItemHeight");
		const auto* itemCornerRatio =
			DesignerPropertyCatalog::Find(toolProperties, L"ItemCornerRatio");
		const auto* separatorColor =
			DesignerPropertyCatalog::Find(toolProperties, L"SeparatorColor");
		const auto* showBottomLine =
			DesignerPropertyCatalog::Find(toolProperties, L"ShowBottomLine");
		CUI_EXPECT_TRUE(padding != nullptr);
		CUI_EXPECT_TRUE(horizontalPadding != nullptr);
		CUI_EXPECT_TRUE(gap != nullptr);
		CUI_EXPECT_TRUE(itemHeight != nullptr);
		CUI_EXPECT_TRUE(itemCornerRatio != nullptr);
		CUI_EXPECT_TRUE(separatorColor != nullptr);
		CUI_EXPECT_TRUE(showBottomLine != nullptr);
		CUI_EXPECT_TRUE(padding->ValueType()
			== std::type_index(typeid(Thickness)));
		CUI_EXPECT_EQ(DesignerStyleValueKind::Int, horizontalPadding->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, separatorColor->Editor);

		toolBar.Padding = Thickness(1.0f, 2.0f, 3.0f, 4.0f);
		toolBar.HorizontalPadding = 12;
		CUI_EXPECT_NEAR(1.0f, toolBar.Padding.Left, 0.0001f);
		CUI_EXPECT_NEAR(2.0f, toolBar.Padding.Top, 0.0001f);
		CUI_EXPECT_EQ(12, toolBar.HorizontalPadding);
		toolBar.HorizontalPadding = -3;
		toolBar.Gap = -2;
		toolBar.ItemHeight = 0;
		CUI_EXPECT_EQ(0, toolBar.HorizontalPadding);
		CUI_EXPECT_EQ(0, toolBar.Gap);
		CUI_EXPECT_EQ(1, toolBar.ItemHeight);
		toolBar.ItemCornerRatio = 0.4f;
		toolBar.ItemCornerRatio = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(0.4f, toolBar.ItemCornerRatio, 0.0001f);

		ToolBar autoHeightToolBar(0, 0, 320, 34);
		auto* first = autoHeightToolBar.AddToolButton(L"A", 40);
		auto* second = autoHeightToolBar.AddToolButton(L"B", 50);
		CUI_EXPECT_EQ(26, first->Height);
		CUI_EXPECT_EQ(26, second->Height);
		autoHeightToolBar.HorizontalPadding = 11;
		autoHeightToolBar.Gap = 7;
		autoHeightToolBar.ItemHeight = 30;
		autoHeightToolBar.LayoutItems();
		CUI_EXPECT_EQ(30, first->Height);
		CUI_EXPECT_EQ(30, second->Height);
		CUI_EXPECT_NEAR(11.0f,
			first->GetActualLocationDip().x, 0.0001f);
		CUI_EXPECT_NEAR(58.0f,
			second->GetActualLocationDip().x, 0.0001f);

		ObservableObject toolSource;
		toolSource.SetValue(L"Spacing", 9);
		ToolBar boundToolBar(0, 0, 320, 34);
		CUI_EXPECT_TRUE(boundToolBar.DataBindings.Add(
			L"Gap", toolSource, L"Spacing", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_EQ(9, boundToolBar.Gap);
		boundToolBar.Gap = 13;
		CUI_EXPECT_EQ(13, toolSource.GetValue<int>(L"Spacing"));

		StatusBar statusBar(0, 0, 320, 26);
		const auto statusProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(statusBar);
		const auto* statusPadding = statusBar.FindPropertyMetadata(L"Padding");
		const auto* statusHorizontalPadding =
			DesignerPropertyCatalog::Find(statusProperties, L"HorizontalPadding");
		const auto* topMost =
			DesignerPropertyCatalog::Find(statusProperties, L"TopMost");
		const auto* partCornerRadius =
			DesignerPropertyCatalog::Find(statusProperties, L"PartCornerRadius");
		const auto* usePartPills =
			DesignerPropertyCatalog::Find(statusProperties, L"UsePartPills");
		CUI_EXPECT_TRUE(statusPadding != nullptr);
		CUI_EXPECT_TRUE(statusHorizontalPadding != nullptr);
		CUI_EXPECT_TRUE(topMost != nullptr);
		CUI_EXPECT_TRUE(partCornerRadius != nullptr);
		CUI_EXPECT_TRUE(usePartPills != nullptr);
		CUI_EXPECT_TRUE(statusPadding->ValueType()
			== std::type_index(typeid(Thickness)));
		CUI_EXPECT_EQ(DesignerStyleValueKind::Int,
			statusHorizontalPadding->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, topMost->Editor);
		statusBar.Padding = Thickness(2.0f);
		statusBar.HorizontalPadding = -8;
		statusBar.Gap = -4;
		statusBar.PartCornerRadius = -2.0f;
		CUI_EXPECT_NEAR(2.0f, statusBar.Padding.Left, 0.0001f);
		CUI_EXPECT_EQ(0, statusBar.HorizontalPadding);
		CUI_EXPECT_EQ(0, statusBar.Gap);
		CUI_EXPECT_NEAR(0.0f, statusBar.PartCornerRadius, 0.0001f);

		ToolBar generatedToolBar(0, 0, 320, 34);
		StatusBar generatedStatusBar(0, 0, 320, 26);
		generatedToolBar.Padding = Thickness(1.0f, 2.0f, 3.0f, 4.0f);
		generatedStatusBar.AddPart(L"Ready", -1);
		auto toolDesigner = std::make_shared<DesignerControl>(
			&generatedToolBar, L"toolStrip", UIClass::UI_ToolBar);
		auto statusDesigner = std::make_shared<DesignerControl>(
			&generatedStatusBar, L"statusStrip", UIClass::UI_StatusBar);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](Control& target,
			const std::shared_ptr<DesignerControl>& designer,
			const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				target, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designer->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(generatedToolBar, toolDesigner, L"HorizontalPadding",
			{ DesignerStyleValueKind::Int, L"12" });
		applyTracked(generatedToolBar, toolDesigner, L"Gap",
			{ DesignerStyleValueKind::Int, L"9" });
		applyTracked(generatedToolBar, toolDesigner, L"ItemHeight",
			{ DesignerStyleValueKind::Int, L"30" });
		applyTracked(generatedToolBar, toolDesigner, L"ShowBottomLine",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(generatedStatusBar, statusDesigner, L"TopMost",
			{ DesignerStyleValueKind::Bool, L"false" });
		applyTracked(generatedStatusBar, statusDesigner, L"HorizontalPadding",
			{ DesignerStyleValueKind::Int, L"7" });
		applyTracked(generatedStatusBar, statusDesigner, L"Gap",
			{ DesignerStyleValueKind::Int, L"8" });
		applyTracked(generatedStatusBar, statusDesigner, L"ShowBorder",
			{ DesignerStyleValueKind::Bool, L"true" });

		CodeGenInput input;
		input.Controls = { toolDesigner, statusDesigner };
		CodeGenerator generator(L"BarMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find("toolStrip->Padding = Thickness(")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"toolStrip->TrySetPropertyValue(L\"HorizontalPadding\", BindingValue(12))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"toolStrip->TrySetPropertyValue(L\"ItemHeight\", BindingValue(30))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"statusStrip->TrySetPropertyValue(L\"TopMost\", BindingValue(false))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"statusStrip->TrySetPropertyValue(L\"ShowBorder\", BindingValue(true))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("toolStrip->Gap =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("toolStrip->ItemHeight =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("statusStrip->TopMost =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("statusStrip->AddPart(L\"Ready\", -1)")
			!= std::string::npos);
	});

	runner.Add("TabControl uses float metadata and preserves interactive bindings", []
	{
		TabControl tabs(0, 0, 200, 160);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(tabs);
		const auto* selectedIndex =
			DesignerPropertyCatalog::Find(properties, L"SelectedIndex");
		const auto* titleHeight =
			DesignerPropertyCatalog::Find(properties, L"TitleHeight");
		const auto* titlePosition =
			DesignerPropertyCatalog::Find(properties, L"TitlePosition");
		const auto* animationDuration =
			DesignerPropertyCatalog::Find(properties, L"AnimationDurationMs");
		const auto* accentColor =
			DesignerPropertyCatalog::Find(properties, L"AccentColor");
		CUI_EXPECT_TRUE(selectedIndex != nullptr);
		CUI_EXPECT_TRUE(titleHeight != nullptr);
		CUI_EXPECT_TRUE(titlePosition != nullptr);
		CUI_EXPECT_TRUE(animationDuration != nullptr);
		CUI_EXPECT_TRUE(accentColor != nullptr);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, titleHeight->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, titlePosition->Editor);
		CUI_EXPECT_EQ(4ULL, titlePosition->Choices.size());
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, accentColor->Editor);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata,
			selectedIndex->Persistence);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"TitleScrollOffset") == nullptr);
		const auto* scrollOffsetMetadata =
			tabs.FindPropertyMetadata(L"TitleScrollOffset");
		CUI_EXPECT_TRUE(scrollOffsetMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			scrollOffsetMetadata->Design().Persistence);

		tabs.TitleHeight = 28.5f;
		CUI_EXPECT_NEAR(28.5f, tabs.TitleHeight, 0.0001f);
		tabs.TitleHeight = -4.0f;
		CUI_EXPECT_NEAR(0.0f, tabs.TitleHeight, 0.0001f);
		tabs.TitleWidth = 86.25f;
		tabs.TitleWidth = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(86.25f, tabs.TitleWidth, 0.0001f);
		CUI_EXPECT_FALSE(tabs.TrySetPropertyValue(
			L"TitlePosition", BindingValue(99)));
		CUI_EXPECT_EQ(TabControlTitlePosition::Top, tabs.TitlePosition);

		TabControl pendingSelection(0, 0, 200, 160);
		CUI_EXPECT_TRUE(pendingSelection.TrySetPropertyValue(
			L"SelectedIndex", BindingValue(2)));
		pendingSelection.AddPage(L"First");
		pendingSelection.AddPage(L"Second");
		pendingSelection.AddPage(L"Third");
		CUI_EXPECT_EQ(2, pendingSelection.SelectedIndex);

		for (int index = 0; index < 4; ++index)
			tabs.AddPage(L"Page " + std::to_wstring(index + 1));
		tabs.TitleWidth = 80.0f;
		ObservableObject source;
		source.SetValue(L"TabIndex", 1);
		source.SetValue(L"TabOffset", 20.0f);
		CUI_EXPECT_TRUE(tabs.DataBindings.Add(
			L"SelectedIndex", source, L"TabIndex", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(tabs.DataBindings.Add(
			L"TitleScrollOffset", source, L"TabOffset", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(tabs.SelectPage(2));
		CUI_EXPECT_EQ(2, source.GetValue<int>(L"TabIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			tabs.GetPropertyValueSource(L"SelectedIndex"));
		const float oldOffset = tabs.TitleScrollOffset;
		tabs.ScrollTitleBy(7.5f);
		CUI_EXPECT_NEAR(oldOffset + 7.5f,
			source.GetValue<float>(L"TabOffset"), 0.0001f);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			tabs.GetPropertyValueSource(L"TitleScrollOffset"));

		TabControl generatedTabs(0, 0, 320, 200);
		generatedTabs.AddPage(L"First");
		generatedTabs.AddPage(L"Second");
		generatedTabs.AddPage(L"Third");
		auto tabsDesigner = std::make_shared<DesignerControl>(
			&generatedTabs, L"tabsView", UIClass::UI_TabControl);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generatedTabs, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			tabsDesigner->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"SelectedIndex",
			{ DesignerStyleValueKind::Int, L"2" });
		applyTracked(L"AnimationMode",
			{ DesignerStyleValueKind::Int, L"1" });
		applyTracked(L"TitlePosition",
			{ DesignerStyleValueKind::Int, L"2" });
		applyTracked(L"TitleHeight",
			{ DesignerStyleValueKind::Float, L"28.5" });
		applyTracked(L"AccentColor",
			{ DesignerStyleValueKind::Color, L"#FF336699" });

		CodeGenInput input;
		input.Controls = { tabsDesigner };
		CodeGenerator generator(L"TabMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto selectedPosition = cpp.find(
			"tabsView->TrySetPropertyValue(L\"SelectedIndex\", BindingValue(2))");
		const auto pagePosition = cpp.find("tabsView->AddPage(L\"First\")");
		CUI_EXPECT_TRUE(selectedPosition != std::string::npos);
		CUI_EXPECT_TRUE(pagePosition != std::string::npos);
		CUI_EXPECT_TRUE(selectedPosition < pagePosition);
		CUI_EXPECT_TRUE(cpp.find(
			"tabsView->TrySetPropertyValue(L\"TitleHeight\", BindingValue(28.5f))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"tabsView->TrySetPropertyValue(L\"TitlePosition\", BindingValue(2))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("tabsView->SelectedIndex =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("tabsView->AnimationMode =") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("tabsView->TitlePosition =") == std::string::npos);
	});

	runner.Add("TabControl page mutations preserve ownership and selected identity", []
	{
		TabControl tabs(0, 0, 320, 200);
		auto* first = tabs.AddPage(L"First");
		auto* second = tabs.AddPage(L"Second");
		auto* third = tabs.AddPage(L"Third");
		CUI_EXPECT_EQ(3, tabs.PageCount);
		CUI_EXPECT_TRUE(tabs.GetPage(0) == first);
		CUI_EXPECT_EQ(1, tabs.IndexOfPage(second));
		CUI_EXPECT_TRUE(tabs.GetPage(-1) == nullptr);
		CUI_EXPECT_TRUE(tabs.InsertPage(-1, L"Invalid") == nullptr);

		ObservableObject source;
		source.SetValue(L"TabIndex", 1);
		CUI_EXPECT_TRUE(tabs.DataBindings.Add(
			L"SelectedIndex", source, L"TabIndex", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == second);

		auto* inserted = tabs.InsertPage(0, L"Inserted");
		CUI_EXPECT_TRUE(inserted != nullptr);
		CUI_EXPECT_TRUE(inserted->Parent == &tabs);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == second);
		CUI_EXPECT_EQ(2, tabs.SelectedIndex);
		CUI_EXPECT_EQ(2, source.GetValue<int>(L"TabIndex"));

		auto detached = tabs.DetachPage(first);
		CUI_EXPECT_TRUE(detached.get() == first);
		CUI_EXPECT_TRUE(detached->Parent == nullptr);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == second);
		CUI_EXPECT_EQ(1, tabs.SelectedIndex);
		CUI_EXPECT_EQ(1, source.GetValue<int>(L"TabIndex"));

		int selectedChangedCount = 0;
		tabs.OnSelectedChanged += [&selectedChangedCount](Control*)
		{
			++selectedChangedCount;
		};
		const int beforeSelectedRemoval = selectedChangedCount;
		CUI_EXPECT_TRUE(tabs.RemovePage(second));
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == third);
		CUI_EXPECT_EQ(1, tabs.SelectedIndex);
		CUI_EXPECT_EQ(beforeSelectedRemoval + 1, selectedChangedCount);
		CUI_EXPECT_EQ(1, source.GetValue<int>(L"TabIndex"));

		tabs.AnimationMode = TabControlAnimationMode::SlideHorizontal;
		CUI_EXPECT_TRUE(tabs.SelectPage(0));
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == inserted);
		auto owned = std::make_unique<TabPage>(L"Owned");
		auto* ownedRaw = owned.get();
		CUI_EXPECT_TRUE(tabs.InsertPage(0, std::move(owned)) == ownedRaw);
		CUI_EXPECT_TRUE(owned == nullptr);
		CUI_EXPECT_TRUE(tabs.GetPage(tabs.SelectedIndex) == inserted);
		CUI_EXPECT_EQ(1, tabs.SelectedIndex);
		CUI_EXPECT_FALSE(tabs.IsAnimationRunning());

		bool rejectedInvalidOwnedIndex = false;
		try
		{
			tabs.InsertPage(99, std::make_unique<TabPage>(L"Rejected"));
		}
		catch (const std::out_of_range&)
		{
			rejectedInvalidOwnedIndex = true;
		}
		CUI_EXPECT_TRUE(rejectedInvalidOwnedIndex);

		tabs.ClearPages();
		CUI_EXPECT_EQ(0, tabs.PageCount);
		CUI_EXPECT_TRUE(tabs.GetPage(0) == nullptr);
		CUI_EXPECT_EQ(0, tabs.SelectedIndex);
		CUI_EXPECT_EQ(0, source.GetValue<int>(L"TabIndex"));
		CUI_EXPECT_FALSE(tabs.RemovePageAt(0));
	});

	runner.Add("Menu item trees expose observable and ownership-safe mutations", []
	{
		Menu menu(0, 0, 320, 28);
		auto* file = menu.AddItem(L"File");
		auto* edit = menu.AddItem(L"Edit");
		CUI_EXPECT_TRUE(file != nullptr);
		CUI_EXPECT_TRUE(edit != nullptr);
		CUI_EXPECT_TRUE(file->Parent == &menu);
		CUI_EXPECT_EQ(0, menu.IndexOfItem(file));
		auto invalidMenuChild = std::make_unique<Control>();
		bool invalidMenuChildRejected = false;
		try
		{
			menu.Children.push_back(invalidMenuChild.get());
		}
		catch (const std::logic_error&)
		{
			invalidMenuChildRejected = true;
		}
		CUI_EXPECT_TRUE(invalidMenuChildRejected);
		CUI_EXPECT_TRUE(invalidMenuChild->Parent == nullptr);

		int subItemChanges = 0;
		file->SubItems.Changed += [&subItemChanges](
			MenuItem::SubItemCollection*, const CollectionChangedEventArgs&)
		{
			++subItemChanges;
		};
		auto* open = file->AddSubItem(L"Open", 10);
		auto* save = file->AddSubItem(L"Save", 11);
		CUI_EXPECT_TRUE(open != nullptr);
		CUI_EXPECT_TRUE(save != nullptr);
		CUI_EXPECT_TRUE(open->ParentItem() == file);
		CUI_EXPECT_EQ(2, subItemChanges);
		CUI_EXPECT_TRUE(file->SubItems.Move(1, 0));
		CUI_EXPECT_EQ(3, subItemChanges);
		CUI_EXPECT_TRUE(file->GetSubItem(0) == save);
		CUI_EXPECT_TRUE(save->ParentItem() == file);

		auto detachedSave = file->DetachSubItem(save);
		CUI_EXPECT_TRUE(detachedSave.get() == save);
		CUI_EXPECT_TRUE(detachedSave->ParentItem() == nullptr);
		CUI_EXPECT_TRUE(file->AddSubItem(std::move(detachedSave)) == save);
		CUI_EXPECT_TRUE(save->ParentItem() == file);

		const int beforeBatch = subItemChanges;
		file->SubItems.BeginUpdate();
		auto* recent = file->AddSubItem(L"Recent", 12);
		auto* exit = file->AddSubItem(L"Exit", 13);
		CUI_EXPECT_TRUE(file->SubItems.Move(3, 1));
		file->SubItems.EndUpdate();
		CUI_EXPECT_EQ(beforeBatch + 1, subItemChanges);
		CUI_EXPECT_TRUE(recent->ParentItem() == file);
		CUI_EXPECT_TRUE(exit->ParentItem() == file);
		const int beforeRemoveBatch = subItemChanges;
		file->SubItems.BeginUpdate();
		CUI_EXPECT_TRUE(file->RemoveSubItem(recent));
		CUI_EXPECT_TRUE(file->RemoveSubItem(exit));
		file->SubItems.EndUpdate();
		CUI_EXPECT_EQ(beforeRemoveBatch + 1, subItemChanges);

		auto* rawDetached = file->GetSubItem(0);
		file->SubItems.erase(file->SubItems.begin());
		CUI_EXPECT_TRUE(rawDetached->ParentItem() == nullptr);
		std::unique_ptr<MenuItem> directEraseOwner(rawDetached);

		auto* inserted = menu.InsertItem(0, L"Inserted");
		CUI_EXPECT_TRUE(inserted != nullptr);
		CUI_EXPECT_TRUE(menu.GetItem(0) == inserted);
		CUI_EXPECT_TRUE(menu.GetItem(1) == file);
		auto detachedFile = menu.DetachItem(file);
		CUI_EXPECT_TRUE(detachedFile.get() == file);
		CUI_EXPECT_TRUE(detachedFile->Parent == nullptr);
		CUI_EXPECT_TRUE(file->GetSubItem(0)->ParentItem() == file);
		CUI_EXPECT_TRUE(menu.AddItem(std::move(detachedFile)) == file);
		CUI_EXPECT_TRUE(file->Parent == &menu);

		CUI_EXPECT_TRUE(menu.RemoveItem(edit));
		CUI_EXPECT_FALSE(menu.RemoveItem(edit));
		CUI_EXPECT_TRUE(menu.AddSeparator()->Separator);
		menu.ClearItems();
		CUI_EXPECT_EQ(0, menu.Count);
		CUI_EXPECT_TRUE(menu.GetItem(0) == nullptr);

		ContextMenu contextMenu;
		auto* tools = contextMenu.AddItem(L"Tools", 20);
		auto* diagnostics = tools->AddSubItem(L"Diagnostics", 21);
		CUI_EXPECT_EQ(1, contextMenu.ItemCount());
		CUI_EXPECT_TRUE(contextMenu.FindItemById(21) == diagnostics);
		CUI_EXPECT_TRUE(contextMenu.FindItemByText(L"Diagnostics")
			== diagnostics);
		CUI_EXPECT_TRUE(contextMenu.FindItemById(21, false) == nullptr);
		auto detachedDiagnostics = contextMenu.DetachItem(diagnostics);
		CUI_EXPECT_TRUE(detachedDiagnostics.get() == diagnostics);
		CUI_EXPECT_TRUE(diagnostics->ParentItem() == nullptr);
		CUI_EXPECT_TRUE(tools->AddSubItem(std::move(detachedDiagnostics))
			== diagnostics);
		CUI_EXPECT_TRUE(contextMenu.RemoveItemById(21));
		CUI_EXPECT_TRUE(contextMenu.FindItemById(21) == nullptr);
		CUI_EXPECT_TRUE(contextMenu.InsertItem(
			0, std::make_unique<MenuItem>(L"First", 19)) != nullptr);
		CUI_EXPECT_EQ(2, contextMenu.ItemCount());
		contextMenu.ClearItems();
		CUI_EXPECT_EQ(0, contextMenu.ItemCount());
	});

	runner.Add("ComboBox metadata preserves selection expand and scroll bindings", []
	{
		ComboBox combo(L"", 0, 0, 180, 28);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(combo);
		const auto* selectedIndex =
			DesignerPropertyCatalog::Find(properties, L"SelectedIndex");
		const auto* expandCount =
			DesignerPropertyCatalog::Find(properties, L"ExpandCount");
		const auto* animationDuration =
			DesignerPropertyCatalog::Find(properties, L"AnimationDurationMs");
		const auto* dropGap =
			DesignerPropertyCatalog::Find(properties, L"DropGap");
		const auto* accentColor =
			DesignerPropertyCatalog::Find(properties, L"AccentColor");
		CUI_EXPECT_TRUE(selectedIndex != nullptr);
		CUI_EXPECT_TRUE(expandCount != nullptr);
		CUI_EXPECT_TRUE(animationDuration != nullptr);
		CUI_EXPECT_TRUE(dropGap != nullptr);
		CUI_EXPECT_TRUE(accentColor != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata,
			selectedIndex->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, expandCount->Editor);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, dropGap->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, accentColor->Editor);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"Expand") == nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"ExpandScroll") == nullptr);
		const auto* expandMetadata = combo.FindPropertyMetadata(L"Expand");
		const auto* scrollMetadata = combo.FindPropertyMetadata(L"ExpandScroll");
		CUI_EXPECT_TRUE(expandMetadata != nullptr);
		CUI_EXPECT_TRUE(scrollMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			expandMetadata->Design().Persistence);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			scrollMetadata->Design().Persistence);

		combo.ExpandCount = 0;
		CUI_EXPECT_EQ(1, combo.ExpandCount);
		combo.DropGap = 6.25f;
		combo.DropGap = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(6.25f, combo.DropGap, 0.0001f);

		ComboBox pending(L"", 0, 0, 180, 28);
		CUI_EXPECT_TRUE(pending.TrySetPropertyValue(
			L"SelectedIndex", BindingValue(2)));
		std::vector<std::wstring> pendingItems{ L"First", L"Second", L"Third" };
		pending.Items = pendingItems;
		CUI_EXPECT_EQ(2, pending.SelectedIndex);
		CUI_EXPECT_EQ(std::wstring(L"Third"), pending.Text);

		ComboBox pendingState(L"", 0, 0, 180, 28);
		pendingState.ExpandCount = 2;
		pendingState.AnimationDurationMs = 0;
		ObservableObject pendingSource;
		pendingSource.SetValue(L"Index", 3);
		pendingSource.SetValue(L"Scroll", 2);
		pendingSource.SetValue(L"Expanded", true);
		CUI_EXPECT_TRUE(pendingState.DataBindings.Add(
			L"SelectedIndex", pendingSource, L"Index", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(pendingState.DataBindings.Add(
			L"ExpandScroll", pendingSource, L"Scroll", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(pendingState.DataBindings.Add(
			L"Expand", pendingSource, L"Expanded", BindingMode::TwoWay) != nullptr);
		std::vector<std::wstring> pendingStateItems{
			L"Zero", L"One", L"Two", L"Three" };
		pendingState.Items = pendingStateItems;
		CUI_EXPECT_EQ(3, pendingState.SelectedIndex);
		CUI_EXPECT_EQ(2, pendingState.ExpandScroll);
		CUI_EXPECT_TRUE(pendingState.Expand);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			pendingState.GetPropertyValueSource(L"Expand"));

		std::vector<std::wstring> items{
			L"One", L"Two", L"Three", L"Four", L"Five", L"Six" };
		combo.Items = items;
		combo.ExpandCount = 2;
		combo.AnimationDurationMs = 0;
		ObservableObject source;
		source.SetValue(L"ComboIndex", 1);
		source.SetValue(L"ComboExpanded", false);
		source.SetValue(L"ComboScroll", 0);
		CUI_EXPECT_TRUE(combo.DataBindings.Add(
			L"SelectedIndex", source, L"ComboIndex", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(combo.DataBindings.Add(
			L"Expand", source, L"ComboExpanded", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(combo.DataBindings.Add(
			L"ExpandScroll", source, L"ComboScroll", BindingMode::TwoWay) != nullptr);
		int selectionChanges = 0;
		combo.OnSelectionChanged += [&selectionChanges](Control*)
		{
			++selectionChanges;
		};
		CUI_EXPECT_TRUE(combo.SelectItem(4));
		CUI_EXPECT_EQ(4, source.GetValue<int>(L"ComboIndex"));
		CUI_EXPECT_EQ(3, source.GetValue<int>(L"ComboScroll"));
		CUI_EXPECT_EQ(1, selectionChanges);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			combo.GetPropertyValueSource(L"SelectedIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			combo.GetPropertyValueSource(L"ExpandScroll"));
		combo.SetExpanded(true);
		CUI_EXPECT_TRUE(source.GetValue<bool>(L"ComboExpanded"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			combo.GetPropertyValueSource(L"Expand"));
		combo.ScrollBy(1);
		CUI_EXPECT_EQ(4, source.GetValue<int>(L"ComboScroll"));
		combo.SetExpanded(false);
		CUI_EXPECT_FALSE(source.GetValue<bool>(L"ComboExpanded"));

		ComboBox equalUpdates(L"", 0, 0, 180, 28);
		std::vector<std::wstring> oneItem{ L"Only" };
		equalUpdates.Items = oneItem;
		CUI_EXPECT_TRUE(equalUpdates.SelectItem(0));
		equalUpdates.SetExpanded(false);
		equalUpdates.ScrollBy(1);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			equalUpdates.GetPropertyValueSource(L"SelectedIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			equalUpdates.GetPropertyValueSource(L"Expand"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Default,
			equalUpdates.GetPropertyValueSource(L"ExpandScroll"));

		ComboBox generatedCombo(L"", 0, 0, 180, 28);
		std::vector<std::wstring> generatedItems{ L"Alpha", L"Beta", L"Gamma" };
		generatedCombo.Items = generatedItems;
		auto comboDesigner = std::make_shared<DesignerControl>(
			&generatedCombo, L"comboChoice", UIClass::UI_ComboBox);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generatedCombo, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			comboDesigner->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"SelectedIndex",
			{ DesignerStyleValueKind::Int, L"2" });
		applyTracked(L"ExpandCount",
			{ DesignerStyleValueKind::Int, L"2" });
		applyTracked(L"AnimationDurationMs",
			{ DesignerStyleValueKind::Int, L"90" });

		CodeGenInput input;
		input.Controls = { comboDesigner };
		CodeGenerator generator(L"ComboMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto itemsPosition = cpp.find(
			"comboChoice->Items = __comboItems_comboChoice");
		const auto selectedPosition = cpp.find(
			"comboChoice->TrySetPropertyValue(L\"SelectedIndex\", BindingValue(2))");
		CUI_EXPECT_TRUE(itemsPosition != std::string::npos);
		CUI_EXPECT_TRUE(selectedPosition != std::string::npos);
		CUI_EXPECT_TRUE(itemsPosition < selectedPosition);
		CUI_EXPECT_TRUE(cpp.find(
			"comboChoice->TrySetPropertyValue(L\"ExpandCount\", BindingValue(2))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"comboChoice->TrySetPropertyValue(L\"AnimationDurationMs\", BindingValue(90))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("comboChoice->SelectedIndex =")
			== std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("comboChoice->ExpandCount =")
			== std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("Items.Clear()") == std::string::npos);
	});

	runner.Add("ListView metadata preserves structural multi-selection and bindings", []
	{
		ListView list(0, 0, 240, 120);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(list);
		const auto* viewMode =
			DesignerPropertyCatalog::Find(properties, L"ViewMode");
		const auto* selectionMode =
			DesignerPropertyCatalog::Find(properties, L"SelectionMode");
		const auto* rowHeight =
			DesignerPropertyCatalog::Find(properties, L"RowHeight");
		const auto* selectedBack =
			DesignerPropertyCatalog::Find(properties, L"SelectedItemBackColor");
		CUI_EXPECT_TRUE(viewMode != nullptr);
		CUI_EXPECT_TRUE(selectionMode != nullptr);
		CUI_EXPECT_TRUE(rowHeight != nullptr);
		CUI_EXPECT_TRUE(selectedBack != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, viewMode->Editor);
		CUI_EXPECT_EQ(4ULL, viewMode->Choices.size());
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, selectionMode->Editor);
		CUI_EXPECT_EQ(2ULL, selectionMode->Choices.size());
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, rowHeight->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, selectedBack->Editor);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"SelectedIndex") == nullptr);
		const auto* selectedIndexMetadata =
			list.FindPropertyMetadata(L"SelectedIndex");
		const auto* scrollMetadata =
			list.FindPropertyMetadata(L"ScrollYOffset");
		CUI_EXPECT_TRUE(selectedIndexMetadata != nullptr);
		CUI_EXPECT_TRUE(scrollMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			selectedIndexMetadata->Design().Persistence);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			scrollMetadata->Design().Persistence);

		ListBox listBox(0, 0, 200, 120);
		const auto listBoxProperties =
			DesignerPropertyCatalog::GetBrowsableProperties(listBox);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			listBoxProperties, L"ViewMode") == nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			listBoxProperties, L"ShowColumnHeaders") == nullptr);
		CUI_EXPECT_FALSE(listBox.ShowColumnHeaders);
		const auto* listBoxHeadersMetadata =
			listBox.FindPropertyMetadata(L"ShowColumnHeaders");
		CUI_EXPECT_TRUE(listBoxHeadersMetadata != nullptr);
		BindingValue listBoxHeadersDefault;
		CUI_EXPECT_TRUE(listBoxHeadersMetadata->TryGetDefaultValue(
			listBoxHeadersDefault));
		bool listBoxHeadersDefaultValue = true;
		CUI_EXPECT_TRUE(listBoxHeadersDefault.TryGet(
			listBoxHeadersDefaultValue));
		CUI_EXPECT_FALSE(listBoxHeadersDefaultValue);
		listBox.ShowColumnHeaders = true;
		CUI_EXPECT_FALSE(listBox.ShowColumnHeaders);

		list.RowHeight = 33.5f;
		list.RowHeight = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(33.5f, list.RowHeight, 0.0001f);
		CUI_EXPECT_FALSE(list.TrySetPropertyValue(
			L"ViewMode", BindingValue(99)));
		CUI_EXPECT_EQ(ListViewViewMode::List, list.ViewMode);

		ListView multi(0, 0, 240, 120);
		multi.SelectionMode = ListViewSelectionMode::Multiple;
		std::vector<ListViewItem> multiItems{
			ListViewItem(L"Zero"), ListViewItem(L"One"), ListViewItem(L"Two") };
		multiItems[0].Selected = true;
		multiItems[2].Selected = true;
		multi.SetItems(std::move(multiItems));
		CUI_EXPECT_EQ(0, multi.SelectedIndex);
		CUI_EXPECT_EQ(2ULL, multi.GetSelectedIndices().size());
		multi.SelectionMode = ListViewSelectionMode::Single;
		CUI_EXPECT_EQ(1ULL, multi.GetSelectedIndices().size());
		CUI_EXPECT_EQ(0, multi.SelectedIndex);

		ListView pending(0, 0, 240, 120);
		CUI_EXPECT_TRUE(pending.TrySetPropertyValue(
			L"SelectedIndex", BindingValue(2)));
		std::vector<ListViewItem> pendingItems{
			ListViewItem(L"Zero"), ListViewItem(L"One"), ListViewItem(L"Two") };
		pendingItems[0].Selected = true;
		pendingItems[1].Selected = true;
		pending.SetItems(std::move(pendingItems));
		CUI_EXPECT_EQ(2, pending.SelectedIndex);
		CUI_EXPECT_EQ(1ULL, pending.GetSelectedIndices().size());
		CUI_EXPECT_TRUE(pending.Items[2].Selected);

		ListView explicitlyEmpty(0, 0, 240, 120);
		ObservableObject emptySource;
		emptySource.SetValue(L"ListIndex", -1);
		CUI_EXPECT_TRUE(explicitlyEmpty.DataBindings.Add(
			L"SelectedIndex", emptySource, L"ListIndex", BindingMode::TwoWay) != nullptr);
		std::vector<ListViewItem> explicitlyEmptyItems{
			ListViewItem(L"Zero"), ListViewItem(L"One") };
		explicitlyEmptyItems[1].Selected = true;
		explicitlyEmpty.SetItems(std::move(explicitlyEmptyItems));
		CUI_EXPECT_TRUE(explicitlyEmpty.GetSelectedIndices().empty());
		CUI_EXPECT_EQ(-1, emptySource.GetValue<int>(L"ListIndex"));
		ListViewItem selectedLate(L"Two");
		selectedLate.Selected = true;
		explicitlyEmpty.AddItem(selectedLate);
		CUI_EXPECT_TRUE(explicitlyEmpty.GetSelectedIndices().empty());
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			explicitlyEmpty.GetPropertyValueSource(L"SelectedIndex"));

		ListView bound(0, 0, 240, 72);
		bound.SelectionMode = ListViewSelectionMode::Multiple;
		std::vector<ListViewItem> boundItems;
		for (int index = 0; index < 12; ++index)
			boundItems.emplace_back(L"Item " + std::to_wstring(index));
		bound.SetItems(std::move(boundItems));
		ObservableObject source;
		source.SetValue(L"ListIndex", 0);
		source.SetValue(L"ListOffset", 0.0f);
		CUI_EXPECT_TRUE(bound.DataBindings.Add(
			L"SelectedIndex", source, L"ListIndex", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(bound.DataBindings.Add(
			L"ScrollYOffset", source, L"ListOffset", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(bound.SelectItem(2, false, false));
		CUI_EXPECT_EQ(2, source.GetValue<int>(L"ListIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			bound.GetPropertyValueSource(L"SelectedIndex"));
		CUI_EXPECT_TRUE(bound.SelectItem(1, true, false));
		CUI_EXPECT_EQ(2ULL, bound.GetSelectedIndices().size());
		CUI_EXPECT_EQ(1, source.GetValue<int>(L"ListIndex"));
		bound.SetScrollOffset(45.0f);
		CUI_EXPECT_NEAR(45.0f,
			source.GetValue<float>(L"ListOffset"), 0.0001f);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			bound.GetPropertyValueSource(L"ScrollYOffset"));

		ListView generatedList(0, 0, 240, 120);
		generatedList.SelectionMode = ListViewSelectionMode::Multiple;
		std::vector<ListViewItem> generatedItems{
			ListViewItem(L"Alpha"), ListViewItem(L"Beta"), ListViewItem(L"Gamma") };
		generatedItems[0].Selected = true;
		generatedItems[2].Selected = true;
		generatedList.SetItems(std::move(generatedItems));
		auto listDesigner = std::make_shared<DesignerControl>(
			&generatedList, L"resultList", UIClass::UI_ListView);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			generatedList, L"SelectionMode",
			{ DesignerStyleValueKind::Int, L"1" },
			&canonicalName, &effectiveValue, &error));
		listDesigner->MetadataProperties[canonicalName] = effectiveValue;
		CodeGenInput input;
		input.Controls = { listDesigner };
		CodeGenerator generator(L"ListMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto metadataPosition = cpp.find(
			"resultList->TrySetPropertyValue(L\"SelectionMode\", BindingValue(1))");
		const auto itemsPosition = cpp.find(
			"resultList->SetItems(std::move(__listItems_resultList))");
		CUI_EXPECT_TRUE(metadataPosition != std::string::npos);
		CUI_EXPECT_TRUE(itemsPosition != std::string::npos);
		CUI_EXPECT_TRUE(metadataPosition < itemsPosition);
		CUI_EXPECT_TRUE(cpp.find("resultList->SelectionMode =")
			== std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("resultList->SelectedIndex =")
			== std::string::npos);
		size_t selectedFlagCount = 0;
		for (size_t position = cpp.find(".Selected = true;");
			position != std::string::npos;
			position = cpp.find(".Selected = true;", position + 1))
		{
			++selectedFlagCount;
		}
		CUI_EXPECT_EQ(2ULL, selectedFlagCount);
	});

	runner.Add("GridView metadata, batch updates, editing and codegen stay coherent", []
	{
		GridView grid(0, 0, 240, 72);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(grid);
		const auto* fullRow =
			DesignerPropertyCatalog::Find(properties, L"FullRowSelect");
		const auto* headHeight =
			DesignerPropertyCatalog::Find(properties, L"HeadHeight");
		const auto* scrollBarSize =
			DesignerPropertyCatalog::Find(properties, L"ScrollBarSize");
		const auto* selectedBack =
			DesignerPropertyCatalog::Find(properties, L"SelectedItemBackColor");
		CUI_EXPECT_TRUE(fullRow != nullptr);
		CUI_EXPECT_TRUE(grid.FullRowSelect);
		CUI_EXPECT_TRUE(headHeight != nullptr);
		CUI_EXPECT_TRUE(scrollBarSize != nullptr);
		CUI_EXPECT_TRUE(selectedBack != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, fullRow->Editor);
		CUI_EXPECT_EQ(DesignerStyleValueKind::Float, headHeight->ValueKind);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, selectedBack->Editor);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"SelectedRowIndex") == nullptr);
		const auto* selectedRowMetadata =
			grid.FindPropertyMetadata(L"SelectedRowIndex");
		const auto* scrollMetadata =
			grid.FindPropertyMetadata(L"ScrollYOffset");
		CUI_EXPECT_TRUE(selectedRowMetadata != nullptr);
		CUI_EXPECT_TRUE(scrollMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			selectedRowMetadata->Design().Persistence);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			scrollMetadata->Design().Persistence);

		grid.HeadHeight = 31.5f;
		grid.HeadHeight = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(31.5f, grid.HeadHeight, 0.0001f);
		grid.RowHeight = -5.0f;
		CUI_EXPECT_NEAR(0.0f, grid.RowHeight, 0.0001f);

		grid.BeginUpdate();
		grid.BeginUpdate();
		CUI_EXPECT_TRUE(grid.IsUpdating());
		grid.AddColumn(GridViewColumn(L"Name", 100.0f, ColumnType::Text, true));
		grid.AddColumn(GridViewColumn(L"State", 80.0f, ColumnType::Text, false));
		for (int index = 0; index < 12; ++index)
		{
			GridViewRow row;
			row.Cells = {
				CellValue(L"Item " + std::to_wstring(index)),
				CellValue(L"Ready") };
			grid.AddRow(row);
		}
		grid.EndUpdate();
		CUI_EXPECT_TRUE(grid.IsUpdating());
		grid.EndUpdate();
		CUI_EXPECT_FALSE(grid.IsUpdating());
		CUI_EXPECT_EQ(12ULL, grid.RowCount());
		CUI_EXPECT_EQ(2ULL, grid.ColumnCount());

		CUI_EXPECT_TRUE(grid.SetCellValue(1, 0, CellValue(L"Busy")));
		CUI_EXPECT_TRUE(grid.GetCell(1, 0) != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Busy"), grid.GetCell(1, 0)->Text);
		CUI_EXPECT_TRUE(grid.BeginEdit(0, 0));
		CUI_EXPECT_TRUE(grid.IsEditing());
		CUI_EXPECT_TRUE(grid.SetEditingText(L"Cancelled"));
		CUI_EXPECT_TRUE(grid.CancelEdit());
		CUI_EXPECT_EQ(std::wstring(L"Item 0"), grid.GetCell(0, 0)->Text);
		CUI_EXPECT_TRUE(grid.BeginEdit(0, 0));
		CUI_EXPECT_TRUE(grid.SetEditingText(L"Committed"));
		CUI_EXPECT_TRUE(grid.CommitEdit());
		CUI_EXPECT_EQ(std::wstring(L"Committed"), grid.GetCell(0, 0)->Text);
		// Earlier editing intentionally established local selection values; clear
		// them so the following binding becomes the effective source.
		CUI_EXPECT_TRUE(grid.ClearPropertyValue(
			L"SelectedColumnIndex", ControlPropertyValueSource::Local));
		CUI_EXPECT_TRUE(grid.ClearPropertyValue(
			L"SelectedRowIndex", ControlPropertyValueSource::Local));

		ObservableObject source;
		source.SetValue(L"GridColumn", 0);
		source.SetValue(L"GridRow", 0);
		source.SetValue(L"GridOffset", 0.0f);
		CUI_EXPECT_TRUE(grid.DataBindings.Add(
			L"SelectedColumnIndex", source, L"GridColumn", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(grid.DataBindings.Add(
			L"SelectedRowIndex", source, L"GridRow", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(grid.DataBindings.Add(
			L"ScrollYOffset", source, L"GridOffset", BindingMode::TwoWay) != nullptr);
		CUI_EXPECT_TRUE(grid.SelectCell(1, 10));
		CUI_EXPECT_EQ(1, source.GetValue<int>(L"GridColumn"));
		CUI_EXPECT_EQ(10, source.GetValue<int>(L"GridRow"));
		CUI_EXPECT_TRUE(source.GetValue<float>(L"GridOffset") > 0.0f);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			grid.GetPropertyValueSource(L"SelectedRowIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			grid.GetPropertyValueSource(L"ScrollYOffset"));

		CUI_EXPECT_TRUE(grid.RemoveColumnAt(0));
		CUI_EXPECT_EQ(1ULL, grid.ColumnCount());
		CUI_EXPECT_EQ(std::wstring(L"Busy"), grid.GetCell(0, 0)->Text);
		CUI_EXPECT_FALSE(grid.RemoveColumnAt(8));
		CUI_EXPECT_TRUE(grid.RemoveRowAt(11));
		CUI_EXPECT_FALSE(grid.RemoveRowAt(99));

		GridView generatedGrid(0, 0, 240, 120);
		GridViewColumn button(L"Action", 90.0f, ColumnType::Button, false);
		button.ButtonText = L"Run";
		generatedGrid.AddColumn(button);
		GridViewColumn combo(L"State", 110.0f, ColumnType::ComboBox, false);
		combo.ComboBoxItems = { L"Ready", L"Busy" };
		generatedGrid.AddColumn(combo);
		auto gridDesigner = std::make_shared<DesignerControl>(
			&generatedGrid, L"ordersGrid", UIClass::UI_GridView);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			generatedGrid, L"FullRowSelect",
			{ DesignerStyleValueKind::Bool, L"true" },
			&canonicalName, &effectiveValue, &error));
		gridDesigner->MetadataProperties[canonicalName] = effectiveValue;
		CodeGenInput input;
		input.Controls = { gridDesigner };
		CodeGenerator generator(L"GridMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto metadataPosition = cpp.find(
			"ordersGrid->TrySetPropertyValue(L\"FullRowSelect\", BindingValue(true))");
		const auto columnsPosition = cpp.find("GridViewColumn __gridColumn1");
		CUI_EXPECT_TRUE(metadataPosition != std::string::npos);
		CUI_EXPECT_TRUE(columnsPosition != std::string::npos);
		CUI_EXPECT_TRUE(metadataPosition < columnsPosition);
		CUI_EXPECT_TRUE(cpp.find("ordersGrid->DeferUpdates()") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__gridColumn1.ButtonText = L\"Run\"") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"__gridColumn2.ComboBoxItems = { L\"Ready\", L\"Busy\" }") != std::string::npos);
	});

	runner.Add("PagedGridView metadata and atomic paging stay coherent", []
	{
		PagedGridView paged(0, 0, 240, 120);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(paged);
		const auto* pageSize =
			DesignerPropertyCatalog::Find(properties, L"PageSize");
		const auto* pagerHeight =
			DesignerPropertyCatalog::Find(properties, L"PagerHeight");
		const auto* pagerBack =
			DesignerPropertyCatalog::Find(properties, L"PagerBackColor");
		CUI_EXPECT_TRUE(pageSize != nullptr);
		CUI_EXPECT_TRUE(pagerHeight != nullptr);
		CUI_EXPECT_TRUE(pagerBack != nullptr);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"PageIndex") == nullptr);
		const auto* pageIndexMetadata =
			paged.FindPropertyMetadata(L"PageIndex");
		CUI_EXPECT_TRUE(pageIndexMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			pageIndexMetadata->Design().Persistence);

		paged.PagerHeight = -4.0f;
		CUI_EXPECT_NEAR(0.0f, paged.PagerHeight, 0.0001f);
		paged.PagerGap = 3.5f;
		paged.PagerGap = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(3.5f, paged.PagerGap, 0.0001f);
		paged.PageSize = 2;
		paged.SetColumns({ GridViewColumn(L"Name", 120.0f) });
		std::vector<GridViewRow> rows;
		for (int index = 0; index < 5; ++index)
		{
			GridViewRow row;
			row.Cells = { CellValue(L"Row " + std::to_wstring(index)) };
			rows.push_back(std::move(row));
		}
		paged.SetRows(std::move(rows));
		CUI_EXPECT_EQ(5ULL, paged.RowCount());
		CUI_EXPECT_EQ(3, paged.PageCount);
		CUI_EXPECT_EQ(2ULL, paged.Grid->RowCount());
		CUI_EXPECT_EQ(std::wstring(L"Row 0"),
			paged.Grid->RowAt(0).Cells[0].Text);

		ObservableObject source;
		source.SetValue(L"Page", 0);
		CUI_EXPECT_TRUE(paged.DataBindings.Add(
			L"PageIndex", source, L"Page", BindingMode::TwoWay) != nullptr);
		paged.NextPage();
		CUI_EXPECT_EQ(1, source.GetValue<int>(L"Page"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			paged.GetPropertyValueSource(L"PageIndex"));
		CUI_EXPECT_EQ(std::wstring(L"Row 2"),
			paged.Grid->RowAt(0).Cells[0].Text);

		paged.BeginUpdate();
		paged.BeginUpdate();
		GridViewRow row5;
		row5.Cells = { CellValue(L"Row 5") };
		paged.AddRow(row5);
		CUI_EXPECT_TRUE(paged.IsUpdating());
		paged.EndUpdate();
		CUI_EXPECT_TRUE(paged.IsUpdating());
		paged.EndUpdate();
		CUI_EXPECT_FALSE(paged.IsUpdating());
		CUI_EXPECT_EQ(6ULL, paged.RowCount());
		CUI_EXPECT_TRUE(paged.RemoveRowAt(5));
		CUI_EXPECT_FALSE(paged.RemoveRowAt(99));
		CUI_EXPECT_TRUE(paged.RemoveColumnAt(0));
		CUI_EXPECT_FALSE(paged.RemoveColumnAt(0));
	});

	runner.Add("PagedGridView observable sources keep every page aligned", []
	{
		PagedGridView paged(0, 0, 300, 140);
		paged.PageSize = 2;
		size_t rowChanges = 0;
		size_t columnChanges = 0;
		bool waitingForRowBatch = false;
		bool rowBatchObservedSynchronized = false;
		auto rowConnection = paged.Rows.Changed.Subscribe(
			[&](PagedGridView::RowCollection*,
				const CollectionChangedEventArgs& change)
			{
				++rowChanges;
				if (waitingForRowBatch
					&& change.Action == CollectionChangeAction::Reset)
				{
					rowBatchObservedSynchronized =
						paged.Grid->RowCount() == 2
						&& paged.Grid->GetCell(0, 0)
						&& paged.Grid->GetCell(0, 0)->Text == L"A0";
				}
			});
		auto columnConnection = paged.Columns.Changed.Subscribe(
			[&](PagedGridView::ColumnCollection*,
				const CollectionChangedEventArgs&)
			{ ++columnChanges; });

		paged.Columns.push_back(GridViewColumn(
			L"A", 100.0f, ColumnType::Text, true));
		paged.Columns.push_back(GridViewColumn(
			L"B", 100.0f, ColumnType::Text, true));
		for (int index = 0; index < 4; ++index)
		{
			GridViewRow row;
			row.Cells = {
				CellValue(L"A" + std::to_wstring(index)),
				CellValue(L"B" + std::to_wstring(index)) };
			paged.Rows.push_back(std::move(row));
		}
		CUI_EXPECT_EQ(4ULL, paged.RowCount());
		CUI_EXPECT_EQ(2ULL, paged.Grid->RowCount());
		CUI_EXPECT_EQ(std::wstring(L"A0"),
			paged.Grid->GetCell(0, 0)->Text);

		GridViewRow lead;
		lead.Cells = { CellValue(L"Lead A"), CellValue(L"Lead B") };
		paged.Rows.insert(paged.Rows.begin(), std::move(lead));
		const uint32_t leadId = paged.Rows[0].AccessibilityId;
		CUI_EXPECT_TRUE(leadId != 0);
		CUI_EXPECT_EQ(std::wstring(L"Lead A"),
			paged.Grid->GetCell(0, 0)->Text);

		CUI_EXPECT_TRUE(paged.Columns.Move(1, 0));
		CUI_EXPECT_EQ(std::wstring(L"B3"), paged.RowAt(4).Cells[0].Text);
		CUI_EXPECT_EQ(std::wstring(L"A3"), paged.RowAt(4).Cells[1].Text);
		const size_t columnsBeforeBatch = columnChanges;
		{
			auto update = paged.Columns.DeferNotifications();
			CUI_EXPECT_TRUE(paged.Columns.Move(0, 1));
			paged.Columns.insert(paged.Columns.begin() + 1,
				GridViewColumn(L"C", 80.0f, ColumnType::Text, true));
		}
		CUI_EXPECT_EQ(columnsBeforeBatch + 1, columnChanges);
		CUI_EXPECT_EQ(3ULL, paged.ColumnCount());
		CUI_EXPECT_EQ(std::wstring(L"A3"), paged.RowAt(4).Cells[0].Text);
		CUI_EXPECT_EQ(CellValue().Text, paged.RowAt(4).Cells[1].Text);
		CUI_EXPECT_EQ(std::wstring(L"B3"), paged.RowAt(4).Cells[2].Text);

		const size_t rowsBeforeBatch = rowChanges;
		waitingForRowBatch = true;
		paged.BeginUpdate();
		CUI_EXPECT_TRUE(paged.Rows.Move(0, paged.Rows.size() - 1));
		GridViewRow tail;
		tail.Cells = {
			CellValue(L"Tail A"), CellValue(L""), CellValue(L"Tail B") };
		paged.Rows.push_back(std::move(tail));
		paged.EndUpdate();
		waitingForRowBatch = false;
		CUI_EXPECT_EQ(rowsBeforeBatch + 1, rowChanges);
		CUI_EXPECT_TRUE(rowBatchObservedSynchronized);
		CUI_EXPECT_EQ(leadId, paged.Rows[4].AccessibilityId);
		CUI_EXPECT_EQ(std::wstring(L"A0"),
			paged.Grid->GetCell(0, 0)->Text);
	});

	runner.Add("PropertyGrid metadata, binding, editing and codegen stay coherent", []
	{
		PropertyGridView propertyGrid(0, 0, 240, 80);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(propertyGrid);
		const auto* showHeader =
			DesignerPropertyCatalog::Find(properties, L"ShowHeader");
		const auto* rowHeight =
			DesignerPropertyCatalog::Find(properties, L"RowHeight");
		const auto* accent =
			DesignerPropertyCatalog::Find(properties, L"AccentColor");
		CUI_EXPECT_TRUE(showHeader != nullptr);
		CUI_EXPECT_TRUE(rowHeight != nullptr);
		CUI_EXPECT_TRUE(accent != nullptr);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Boolean, showHeader->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Color, accent->Editor);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::Find(
			properties, L"SelectedIndex") == nullptr);
		const auto* selectedMetadata =
			propertyGrid.FindPropertyMetadata(L"SelectedIndex");
		const auto* scrollMetadata =
			propertyGrid.FindPropertyMetadata(L"ScrollYOffset");
		CUI_EXPECT_TRUE(selectedMetadata != nullptr);
		CUI_EXPECT_TRUE(scrollMetadata != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			selectedMetadata->Design().Persistence);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Transient,
			scrollMetadata->Design().Persistence);

		propertyGrid.RowHeight = -2.0f;
		CUI_EXPECT_NEAR(0.0f, propertyGrid.RowHeight, 0.0001f);
		propertyGrid.RowHeight = 24.0f;
		propertyGrid.RowHeight = std::numeric_limits<float>::quiet_NaN();
		CUI_EXPECT_NEAR(24.0f, propertyGrid.RowHeight, 0.0001f);
		propertyGrid.SetHeaderLabels(L"Property name", L"Current value");
		CUI_EXPECT_EQ(std::wstring(L"Property name"),
			propertyGrid.GetNameHeaderLabel());
		CUI_EXPECT_EQ(std::wstring(L"Current value"),
			propertyGrid.GetValueHeaderLabel());

		std::vector<PropertyGridItem> items;
		for (int index = 0; index < 8; ++index)
		{
			PropertyGridItem item(
				L"General", L"Item " + std::to_wstring(index),
				L"Value " + std::to_wstring(index));
			item.Tag = static_cast<UINT64>(index + 10);
			items.push_back(std::move(item));
		}
		propertyGrid.SetItems(std::move(items));
		CUI_EXPECT_EQ(8ULL, propertyGrid.ItemCount());

		PropertyGridView replacementState(0, 0, 240, 90);
		std::vector<PropertyGridItem> stateItems;
		for (int index = 0; index < 12; ++index)
		{
			stateItems.emplace_back(
				index < 2 ? L"First" : L"Second",
				L"State " + std::to_wstring(index),
				L"Before");
		}
		replacementState.SetItems(stateItems);
		replacementState.CollapseCategory(L"First", true);
		replacementState.SetScrollOffset(80.0f);
		const float replacementScroll = replacementState.ScrollYOffset;
		CUI_EXPECT_TRUE(replacementScroll > 0.0f);
		for (auto& item : stateItems) item.Value = L"After";
		replacementState.SetItems(std::move(stateItems));
		CUI_EXPECT_TRUE(replacementState.IsCategoryCollapsed(L"First"));
		CUI_EXPECT_NEAR(replacementScroll,
			replacementState.ScrollYOffset, 0.0001f);

		CUI_EXPECT_TRUE(propertyGrid.BeginEdit(0));
		CUI_EXPECT_TRUE(propertyGrid.IsEditing());
		CUI_EXPECT_TRUE(propertyGrid.SetEditingText(L"Cancelled"));
		CUI_EXPECT_TRUE(propertyGrid.CancelEdit());
		CUI_EXPECT_EQ(std::wstring(L"Value 0"), propertyGrid.GetValue(0));
		CUI_EXPECT_TRUE(propertyGrid.BeginEdit(0));
		CUI_EXPECT_TRUE(propertyGrid.SetEditingText(L"Committed"));
		CUI_EXPECT_TRUE(propertyGrid.CommitEdit());
		CUI_EXPECT_EQ(std::wstring(L"Committed"), propertyGrid.GetValue(0));

		PropertyGridView nativeDesignerSemantics(0, 0, 240, 120);
		PropertyGridItem mixedBool(
			L"Common", L"Visible", L"True", PropertyGridValueType::Bool);
		mixedBool.IsMixed = true;
		PropertyGridItem action(
			L"Data", L"Bindings", L"Edit...", PropertyGridValueType::Action);
		PropertyGridItem resettable(
			L"Layout", L"Width", L"120", PropertyGridValueType::Number);
		resettable.CanReset = true;
		PropertyGridItem anchor(
			L"Layout", L"Anchor", L"3", PropertyGridValueType::Anchor);
		nativeDesignerSemantics.SetItems(
			{ mixedBool, action, resettable, anchor });
		int changedCount = 0;
		int actionCount = 0;
		int resetCount = 0;
		nativeDesignerSemantics.OnValueChanged +=
			[&](PropertyGridView*, int, std::wstring, std::wstring)
			{ ++changedCount; };
		nativeDesignerSemantics.OnItemClick +=
			[&](PropertyGridView*, int index)
			{ if (index == 1) ++actionCount; };
		nativeDesignerSemantics.OnResetRequested +=
			[&](PropertyGridView*, int index)
			{ if (index == 2) ++resetCount; };
		CUI_EXPECT_TRUE(nativeDesignerSemantics.SetValue(0, L"True"));
		CUI_EXPECT_FALSE(nativeDesignerSemantics.Items[0].IsMixed);
		CUI_EXPECT_EQ(1, changedCount);
		CUI_EXPECT_TRUE(nativeDesignerSemantics.ActivateItem(1));
		CUI_EXPECT_EQ(1, actionCount);
		CUI_EXPECT_FALSE(nativeDesignerSemantics.BeginEdit(1));
		CUI_EXPECT_TRUE(nativeDesignerSemantics.RequestReset(2));
		CUI_EXPECT_EQ(1, resetCount);
		CUI_EXPECT_FALSE(nativeDesignerSemantics.RequestReset(1));
		CUI_EXPECT_FALSE(nativeDesignerSemantics.BeginEdit(3));

		uint8_t parsedAnchors = AnchorStyles::None;
		CUI_EXPECT_TRUE(AnchorPickerPopup::TryParseAnchors(
			L"5", parsedAnchors));
		CUI_EXPECT_EQ(static_cast<int>(AnchorStyles::Left | AnchorStyles::Right),
			static_cast<int>(parsedAnchors));
		CUI_EXPECT_TRUE(AnchorPickerPopup::TryParseAnchors(
			L"Left + Top (3)", parsedAnchors));
		CUI_EXPECT_EQ(3, static_cast<int>(parsedAnchors));
		CUI_EXPECT_EQ(std::wstring(L"Top, Left, Right"),
			AnchorPickerPopup::AnchorToString(
				AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right));
		AnchorPickerPopup anchorPicker;
		anchorPicker.SetSelectedAnchors(
			AnchorStyles::Left | AnchorStyles::Top);
		int anchorChanged = 0;
		std::wstring anchorValue;
		anchorPicker.OnAnchorChanged +=
			[&](AnchorPickerPopup*, uint8_t anchors, std::wstring value)
			{
				++anchorChanged;
				parsedAnchors = anchors;
				anchorValue = std::move(value);
			};
		CUI_EXPECT_TRUE(anchorPicker.ToggleAnchor(AnchorStyles::Right));
		CUI_EXPECT_EQ(1, anchorChanged);
		CUI_EXPECT_EQ(7, static_cast<int>(parsedAnchors));
		CUI_EXPECT_EQ(std::wstring(L"7"), anchorValue);
		CUI_EXPECT_FALSE(anchorPicker.ToggleAnchor(3));

		PropertyGridView nativeSlider(0, 0, 240, 120);
		PropertyGridItem sliderItem(
			L"Data", L"Opacity", L"0", PropertyGridValueType::Slider);
		sliderItem.Minimum = 0.0;
		sliderItem.Maximum = 10.0;
		sliderItem.Step = 1.0;
		nativeSlider.SetItems({ sliderItem });
		int sliderStarted = 0;
		int sliderCompleted = 0;
		int sliderCanceled = 0;
		nativeSlider.OnEditStarted +=
			[&](PropertyGridView*, int) { ++sliderStarted; };
		nativeSlider.OnEditCompleted +=
			[&](PropertyGridView*, int) { ++sliderCompleted; };
		nativeSlider.OnEditCanceled +=
			[&](PropertyGridView*, int) { ++sliderCanceled; };
		CUI_EXPECT_FALSE(nativeSlider.BeginEdit(0));
		CUI_EXPECT_TRUE(nativeSlider.ProcessMessage(
			WM_LBUTTONDOWN, 0, 0, 180, 62));
		CUI_EXPECT_TRUE(nativeSlider.ProcessMessage(
			WM_MOUSEMOVE, 0, 0, 198, 62));
		CUI_EXPECT_TRUE(nativeSlider.ProcessMessage(
			WM_LBUTTONUP, 0, 0, 198, 62));
		CUI_EXPECT_EQ(1, sliderStarted);
		CUI_EXPECT_EQ(1, sliderCompleted);
		CUI_EXPECT_TRUE(nativeSlider.GetValue(0) != L"0");
		CUI_EXPECT_TRUE(nativeSlider.ProcessMessage(
			WM_LBUTTONDOWN, 0, 0, 180, 62));
		CUI_EXPECT_TRUE(nativeSlider.ProcessMessage(
			WM_CANCELMODE, 0, 0, 180, 62));
		CUI_EXPECT_EQ(1, sliderCanceled);

		ObservableObject source;
		source.SetValue(L"SelectedProperty", 0);
		source.SetValue(L"PropertyOffset", 0.0f);
		CUI_EXPECT_TRUE(propertyGrid.DataBindings.Add(
			L"SelectedIndex", source, L"SelectedProperty", BindingMode::TwoWay)
			!= nullptr);
		CUI_EXPECT_TRUE(propertyGrid.DataBindings.Add(
			L"ScrollYOffset", source, L"PropertyOffset", BindingMode::TwoWay)
			!= nullptr);
		CUI_EXPECT_TRUE(propertyGrid.SelectItem(6));
		CUI_EXPECT_EQ(6, source.GetValue<int>(L"SelectedProperty"));
		CUI_EXPECT_TRUE(source.GetValue<float>(L"PropertyOffset") > 0.0f);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			propertyGrid.GetPropertyValueSource(L"SelectedIndex"));
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			propertyGrid.GetPropertyValueSource(L"ScrollYOffset"));
		CUI_EXPECT_TRUE(propertyGrid.ClearSelection());
		CUI_EXPECT_EQ(-1, source.GetValue<int>(L"SelectedProperty"));
		CUI_EXPECT_FALSE(propertyGrid.ClearSelection());

		PropertyGridView generated(0, 0, 240, 120);
		PropertyGridItem generatedItem(
			L"Data", L"Mode", L"Ready", PropertyGridValueType::Enum);
		generatedItem.Description = L"Current mode";
		generatedItem.Options = { L"Ready", L"Busy" };
		generatedItem.Tag = 42;
		generatedItem.IsMixed = true;
		generatedItem.CanReset = true;
		PropertyGridItem generatedSlider(
			L"Data", L"Opacity", L"0.5", PropertyGridValueType::Slider);
		generatedSlider.Minimum = 0.1;
		generatedSlider.Maximum = 0.9;
		generatedSlider.Step = 0.05;
		PropertyGridItem generatedAnchor(
			L"Layout", L"Anchor", L"3", PropertyGridValueType::Anchor);
		generated.SetItems({ generatedItem, generatedSlider, generatedAnchor });
		auto designer = std::make_shared<DesignerControl>(
			&generated, L"settingsGrid", UIClass::UI_PropertyGrid);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
			generated, L"ShowHeader",
			{ DesignerStyleValueKind::Bool, L"false" },
			&canonicalName, &effectiveValue, &error));
		designer->MetadataProperties[canonicalName] = effectiveValue;
		CodeGenInput input;
		input.Controls = { designer };
		CodeGenerator generator(L"PropertyGridMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto metadataPosition = cpp.find(
			"settingsGrid->TrySetPropertyValue(L\"ShowHeader\", BindingValue(false))");
		const auto itemsPosition = cpp.find(
			"settingsGrid->SetItems(std::move(__propertyItems_settingsGrid))");
		CUI_EXPECT_TRUE(metadataPosition != std::string::npos);
		CUI_EXPECT_TRUE(itemsPosition != std::string::npos);
		CUI_EXPECT_TRUE(metadataPosition < itemsPosition);
		CUI_EXPECT_TRUE(cpp.find("settingsGrid->ShowHeader =")
			== std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_1.Tag = 42ULL")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_1.IsMixed = true")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_1.CanReset = true")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_2.Minimum = 0.1")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_2.Maximum = 0.9")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("__propertyItem_settingsGrid_2.Step = 0.05")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"L\"Anchor\", L\"3\", static_cast<PropertyGridValueType>(8)")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"__propertyItem_settingsGrid_1.Options.push_back(L\"Busy\")")
			!= std::string::npos);
	});

	runner.Add("PropertyGrid observable items preserve binding and edit identity", []
	{
		PropertyGridView propertyGrid(0, 0, 260, 120);
		size_t collectionChanges = 0;
		auto connection = propertyGrid.Items.Changed.Subscribe(
			[&](PropertyGridView::ItemCollection*,
				const CollectionChangedEventArgs&)
			{ ++collectionChanges; });
		propertyGrid.Items.push_back(PropertyGridItem(
			L"General", L"First", L"One"));
		propertyGrid.Items.push_back(PropertyGridItem(
			L"General", L"Second", L"Two"));
		propertyGrid.Items.push_back(PropertyGridItem(
			L"General", L"Third", L"Three"));
		ObservableObject source;
		source.SetValue(L"Selected", 1);
		CUI_EXPECT_TRUE(propertyGrid.DataBindings.Add(
			L"SelectedIndex", source, L"Selected", BindingMode::TwoWay)
			!= nullptr);
		CUI_EXPECT_EQ(1, propertyGrid.SelectedIndex);
		CUI_EXPECT_TRUE(propertyGrid.BeginEdit(1));
		const uint32_t selectedId = propertyGrid.Items[1].CollectionId;
		CUI_EXPECT_TRUE(selectedId != 0);

		propertyGrid.Items.insert(propertyGrid.Items.begin(),
			PropertyGridItem(L"General", L"Inserted", L"Zero"));
		CUI_EXPECT_EQ(2, propertyGrid.SelectedIndex);
		CUI_EXPECT_EQ(2, source.GetValue<int>(L"Selected"));
		CUI_EXPECT_EQ(2, propertyGrid.GetEditingIndex());
		CUI_EXPECT_EQ(selectedId,
			propertyGrid.Items[2].CollectionId);
		CUI_EXPECT_TRUE(propertyGrid.Items.Move(2, 0));
		CUI_EXPECT_EQ(0, propertyGrid.SelectedIndex);
		CUI_EXPECT_EQ(0, source.GetValue<int>(L"Selected"));
		CUI_EXPECT_EQ(0, propertyGrid.GetEditingIndex());

		const size_t beforeBatch = collectionChanges;
		{
			auto update = propertyGrid.Items.DeferNotifications();
			CUI_EXPECT_TRUE(propertyGrid.Items.Move(0, 2));
			propertyGrid.Items.push_back(PropertyGridItem(
				L"Advanced", L"Fourth", L"Four"));
		}
		CUI_EXPECT_EQ(beforeBatch + 1, collectionChanges);
		CUI_EXPECT_EQ(2, propertyGrid.SelectedIndex);
		CUI_EXPECT_EQ(2, source.GetValue<int>(L"Selected"));
		CUI_EXPECT_EQ(2, propertyGrid.GetEditingIndex());
		CUI_EXPECT_EQ(selectedId,
			propertyGrid.Items[2].CollectionId);

		propertyGrid.Items.erase(propertyGrid.Items.begin() + 2);
		CUI_EXPECT_EQ(-1, propertyGrid.SelectedIndex);
		CUI_EXPECT_EQ(-1, source.GetValue<int>(L"Selected"));
		CUI_EXPECT_FALSE(propertyGrid.IsEditing());
	});

	runner.Add("PropertyGrid editable enum combines free text and suggestions", []
	{
		CUI_EXPECT_EQ(
			static_cast<int>(PropertyGridValueType::Anchor) + 1,
			static_cast<int>(PropertyGridValueType::EditableEnum));
		PropertyGridView propertyGrid(0, 0, 260, 120);
		PropertyGridItem handler(
			L"Events", L"OnMouseClick", L"HandleSave",
			PropertyGridValueType::EditableEnum);
		handler.Options = { L"saveButton_OnMouseClick", L"HandleSave" };
		propertyGrid.Items.push_back(std::move(handler));
		CUI_EXPECT_TRUE(propertyGrid.BeginEdit(0));
		CUI_EXPECT_TRUE(propertyGrid.IsEditing());
		CUI_EXPECT_TRUE(propertyGrid.CommitEdit());
		CUI_EXPECT_TRUE(propertyGrid.HandlesNavigationKey(VK_F4));
	});

	runner.Add("MediaPlayer metadata, binding, safe controls and codegen stay coherent", []
	{
		MediaPlayer media(0, 0, 320, 180);
		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(media);
		const auto* autoPlay =
			DesignerPropertyCatalog::Find(properties, L"AutoPlay");
		const auto* volume =
			DesignerPropertyCatalog::Find(properties, L"Volume");
		const auto* playbackRate =
			DesignerPropertyCatalog::Find(properties, L"PlaybackRate");
		const auto* hardware =
			DesignerPropertyCatalog::Find(properties, L"EnableHardwareDecode");
		const auto* nv12 =
			DesignerPropertyCatalog::Find(properties, L"PreferNv12VideoOutput");
		const auto* renderMode =
			DesignerPropertyCatalog::Find(properties, L"RenderMode");
		CUI_EXPECT_TRUE(autoPlay != nullptr);
		CUI_EXPECT_TRUE(volume != nullptr);
		CUI_EXPECT_TRUE(playbackRate != nullptr);
		CUI_EXPECT_TRUE(hardware != nullptr);
		CUI_EXPECT_TRUE(nv12 != nullptr);
		CUI_EXPECT_TRUE(renderMode != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata, volume->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, volume->Editor);
		CUI_EXPECT_TRUE(volume->Minimum.has_value());
		CUI_EXPECT_TRUE(volume->Maximum.has_value());
		CUI_EXPECT_EQ(0.0, *volume->Minimum);
		CUI_EXPECT_EQ(1.0, *volume->Maximum);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, renderMode->Editor);
		CUI_EXPECT_EQ(5ULL, renderMode->Choices.size());

		int propertyChanges = 0;
		auto propertyConnection = media.OnPropertyValueChanged.Subscribe(
			[&](Control*, const ControlPropertyChangedEventArgs& args)
			{
				if (args.PropertyName == L"Volume") ++propertyChanges;
			});
		media.Volume = 2.0;
		CUI_EXPECT_NEAR(1.0, media.Volume, 0.0000001);
		CUI_EXPECT_EQ(0, propertyChanges);
		media.Volume = 0.35;
		CUI_EXPECT_NEAR(0.35, media.Volume, 0.0000001);
		CUI_EXPECT_EQ(1, propertyChanges);
		media.PlaybackRate = 12.0f;
		CUI_EXPECT_NEAR(4.0f, media.PlaybackRate, 0.0001f);
		CUI_EXPECT_FALSE(media.TrySetPropertyValue(
			L"RenderMode", BindingValue(99)));
		CUI_EXPECT_EQ(MediaPlayer::VideoRenderMode::Fit, media.RenderMode);

		ObservableObject source;
		source.SetValue(L"Level", 0.6);
		MediaPlayer boundMedia(0, 0, 320, 180);
		auto* volumeBinding = boundMedia.DataBindings.Add(
			L"Volume", source, L"Level", BindingMode::TwoWay);
		CUI_EXPECT_TRUE(volumeBinding != nullptr);
		CUI_EXPECT_TRUE(volumeBinding->IsValid());
		CUI_EXPECT_NEAR(0.6, boundMedia.Volume, 0.0000001);
		CUI_EXPECT_TRUE(boundMedia.TrySetCurrentPropertyValue(
			L"Volume", BindingValue(0.45)));
		CUI_EXPECT_NEAR(0.45,
			source.GetValue<double>(L"Level"), 0.0000001);
		CUI_EXPECT_EQ(ControlPropertyValueSource::Binding,
			boundMedia.GetPropertyValueSource(L"Volume"));

		CUI_EXPECT_FALSE(media.TryPlay());
		CUI_EXPECT_FALSE(media.TryPause());
		CUI_EXPECT_FALSE(media.TryStop());
		CUI_EXPECT_FALSE(media.TryResume());
		CUI_EXPECT_FALSE(media.TrySeek(1.0));
		CUI_EXPECT_FALSE(media.SeekBy(5.0));
		CUI_EXPECT_FALSE(media.SetProgress(0.5));
		CUI_EXPECT_FALSE(media.TogglePlayback());
		CUI_EXPECT_TRUE(media.IsStopped());
		CUI_EXPECT_FALSE(media.IsLoaded());

		int detailedErrors = 0;
		HRESULT lastEventError = S_OK;
		auto errorConnection = media.OnMediaError.Subscribe(
			[&](MediaPlayer*, HRESULT error)
			{
				++detailedErrors;
				lastEventError = error;
			});
		CUI_EXPECT_FALSE(media.Load(L""));
		CUI_EXPECT_EQ(1, detailedErrors);
		CUI_EXPECT_TRUE(FAILED(lastEventError));
		CUI_EXPECT_TRUE(media.HasMediaError());
		media.ClearMediaError();
		CUI_EXPECT_FALSE(media.HasMediaError());
		media.Close();

		MediaPlayer generated(0, 0, 320, 180);
		auto designer = std::make_shared<DesignerControl>(
			&generated, L"previewPlayer", UIClass::UI_MediaPlayer);
		designer->DesignStrings[L"mediaFile"] = L"C:\\media\\clip.mp4";
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generated, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designer->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"AutoPlay", { DesignerStyleValueKind::Bool, L"false" });
		applyTracked(L"Volume", { DesignerStyleValueKind::Double, L"0.25" });
		applyTracked(L"PlaybackRate", { DesignerStyleValueKind::Float, L"1.5" });
		applyTracked(L"RenderMode", {
			DesignerStyleValueKind::Int,
			std::to_wstring(static_cast<int>(MediaPlayer::VideoRenderMode::Fill)) });
		CodeGenInput input;
		input.Controls = { designer };
		CodeGenerator generator(L"MediaMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto autoPlayPosition = cpp.find(
			"previewPlayer->TrySetPropertyValue(L\"AutoPlay\", BindingValue(false))");
		const auto volumePosition = cpp.find(
			"previewPlayer->TrySetPropertyValue(L\"Volume\", BindingValue(0.25))");
		const auto loadPosition = cpp.find(
			"previewPlayer->Load(L\"C:\\\\media\\\\clip.mp4\")");
		CUI_EXPECT_TRUE(autoPlayPosition != std::string::npos);
		CUI_EXPECT_TRUE(volumePosition != std::string::npos);
		CUI_EXPECT_TRUE(loadPosition != std::string::npos);
		CUI_EXPECT_TRUE(autoPlayPosition < loadPosition);
		CUI_EXPECT_TRUE(volumePosition < loadPosition);
		CUI_EXPECT_TRUE(cpp.find("previewPlayer->AutoPlay =")
			== std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("previewPlayer->RenderMode =")
			== std::string::npos);
	});

	runner.Add("WebBrowser ABI, pending navigation, metadata and codegen stay coherent", []
	{
		WebBrowser browser(0, 0, 500, 360);
		CUI_EXPECT_FALSE(browser.IsInitialized());
		CUI_EXPECT_FALSE(browser.IsWebViewReady());
#if defined(CUI_TEST_WEBVIEW2_DISABLED)
		CUI_EXPECT_EQ(WebBrowser::InitializationState::Unsupported,
			browser.GetInitializationState());
		CUI_EXPECT_EQ(E_NOTIMPL, browser.GetLastInitializationError());
		CUI_EXPECT_EQ(E_NOTIMPL, browser.GetLastEnvironmentError());
		CUI_EXPECT_EQ(E_NOTIMPL, browser.GetLastControllerError());
		CUI_EXPECT_EQ(E_NOTIMPL, browser.GetLastWebViewError());
		CUI_EXPECT_FALSE(browser.TryInitialize());
		CUI_EXPECT_FALSE(browser.TryNavigate(L"https://example.test/first"));
		CUI_EXPECT_FALSE(browser.TrySetHtml(L"<h1>latest</h1>"));
		CUI_EXPECT_FALSE(browser.HasPendingNavigation());
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::None,
			browser.GetPendingNavigationKind());
#else
		CUI_EXPECT_EQ(WebBrowser::InitializationState::NotStarted,
			browser.GetInitializationState());
		CUI_EXPECT_EQ(E_PENDING, browser.GetLastInitializationError());
		CUI_EXPECT_FALSE(browser.TryInitialize());

		CUI_EXPECT_TRUE(browser.TryNavigate(L"https://example.test/first"));
		CUI_EXPECT_TRUE(browser.HasPendingNavigation());
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::Url,
			browser.GetPendingNavigationKind());
		CUI_EXPECT_EQ(std::wstring(L"https://example.test/first"),
			browser.GetPendingUrl());
		CUI_EXPECT_TRUE(browser.TrySetHtml(L"<h1>latest</h1>"));
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::Html,
			browser.GetPendingNavigationKind());
		CUI_EXPECT_TRUE(browser.GetPendingUrl().empty());
		CUI_EXPECT_TRUE(browser.TryNavigate(L"https://example.test/latest"));
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::Url,
			browser.GetPendingNavigationKind());
		CUI_EXPECT_EQ(std::wstring(L"https://example.test/latest"),
			browser.GetPendingUrl());
		browser.ClearPendingNavigation();
		CUI_EXPECT_FALSE(browser.HasPendingNavigation());
		CUI_EXPECT_FALSE(browser.TryNavigate(L""));
#endif
		CUI_EXPECT_FALSE(browser.TryReload());
		CUI_EXPECT_FALSE(browser.TryStop());
		CUI_EXPECT_FALSE(browser.TryGoBack());
		CUI_EXPECT_FALSE(browser.TryGoForward());

		const auto properties =
			DesignerPropertyCatalog::GetBrowsableProperties(browser);
		const auto* initialUrl =
			DesignerPropertyCatalog::Find(properties, L"InitialUrl");
		const auto* zoom =
			DesignerPropertyCatalog::Find(properties, L"ZoomFactor");
		const auto* contextMenus = DesignerPropertyCatalog::Find(
			properties, L"AreDefaultContextMenusEnabled");
		const auto* statusBar =
			DesignerPropertyCatalog::Find(properties, L"IsStatusBarEnabled");
		const auto* zoomControl =
			DesignerPropertyCatalog::Find(properties, L"IsZoomControlEnabled");
		CUI_EXPECT_TRUE(initialUrl != nullptr);
		CUI_EXPECT_TRUE(zoom != nullptr);
		CUI_EXPECT_TRUE(contextMenus != nullptr);
		CUI_EXPECT_TRUE(statusBar != nullptr);
		CUI_EXPECT_TRUE(zoomControl != nullptr);
		CUI_EXPECT_EQ(ControlPropertyPersistence::Metadata,
			initialUrl->Persistence);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Text, initialUrl->Editor);
		CUI_EXPECT_EQ(ControlPropertyEditorKind::Number, zoom->Editor);
		CUI_EXPECT_TRUE(zoom->Minimum.has_value());
		CUI_EXPECT_TRUE(zoom->Maximum.has_value());
		CUI_EXPECT_TRUE(zoom->Step.has_value());
		CUI_EXPECT_EQ(0.25, *zoom->Minimum);
		CUI_EXPECT_EQ(5.0, *zoom->Maximum);
		CUI_EXPECT_EQ(0.05, *zoom->Step);

		browser.ZoomFactor = 99.0;
		CUI_EXPECT_NEAR(5.0, browser.ZoomFactor, 0.0000001);
		browser.ZoomFactor = std::numeric_limits<double>::quiet_NaN();
		CUI_EXPECT_NEAR(5.0, browser.ZoomFactor, 0.0000001);
		browser.ZoomFactor = 0.01;
		CUI_EXPECT_NEAR(0.25, browser.ZoomFactor, 0.0000001);
		browser.AreDefaultContextMenusEnabled = false;
		browser.IsStatusBarEnabled = true;
		browser.IsZoomControlEnabled = false;
		CUI_EXPECT_FALSE(browser.AreDefaultContextMenusEnabled);
		CUI_EXPECT_TRUE(browser.IsStatusBarEnabled);
		CUI_EXPECT_FALSE(browser.IsZoomControlEnabled);

		ObservableObject source;
		source.SetValue(L"BrowserZoom", 1.75);
		WebBrowser boundBrowser(0, 0, 500, 360);
		auto* zoomBinding = boundBrowser.DataBindings.Add(
			L"ZoomFactor", source, L"BrowserZoom", BindingMode::TwoWay);
		CUI_EXPECT_TRUE(zoomBinding != nullptr);
		CUI_EXPECT_TRUE(zoomBinding->IsValid());
		CUI_EXPECT_NEAR(1.75, boundBrowser.ZoomFactor, 0.0000001);
		CUI_EXPECT_TRUE(boundBrowser.TrySetCurrentPropertyValue(
			L"ZoomFactor", BindingValue(2.25)));
		CUI_EXPECT_NEAR(2.25,
			source.GetValue<double>(L"BrowserZoom"), 0.0000001);

		WebBrowser generated(0, 0, 500, 360);
		auto designer = std::make_shared<DesignerControl>(
			&generated, L"helpBrowser", UIClass::UI_WebBrowser);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				generated, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designer->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"InitialUrl", {
			DesignerStyleValueKind::String, L"https://docs.example.test/" });
		applyTracked(L"ZoomFactor", {
			DesignerStyleValueKind::Double, L"1.25" });
		applyTracked(L"IsStatusBarEnabled", {
			DesignerStyleValueKind::Bool, L"true" });
		CUI_EXPECT_EQ(std::wstring(L"https://docs.example.test/"),
			generated.InitialUrl);
#if defined(CUI_TEST_WEBVIEW2_DISABLED)
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::None,
			generated.GetPendingNavigationKind());
#else
		CUI_EXPECT_EQ(WebBrowser::PendingNavigationKind::Url,
			generated.GetPendingNavigationKind());
#endif

		CodeGenInput input;
		input.Controls = { designer };
		CodeGenerator generator(L"WebBrowserMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		const auto urlPosition = cpp.find(
			"helpBrowser->TrySetPropertyValue(L\"InitialUrl\", BindingValue(L\"https://docs.example.test/\"))");
		const auto zoomPosition = cpp.find(
			"helpBrowser->TrySetPropertyValue(L\"ZoomFactor\", BindingValue(1.25))");
		const auto statusPosition = cpp.find(
			"helpBrowser->TrySetPropertyValue(L\"IsStatusBarEnabled\", BindingValue(true))");
		CUI_EXPECT_TRUE(urlPosition != std::string::npos);
		CUI_EXPECT_TRUE(zoomPosition != std::string::npos);
		CUI_EXPECT_TRUE(statusPosition != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("helpBrowser->Navigate(") == std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("helpBrowser->ZoomFactor =") == std::string::npos);
	});

	runner.Add("NotifyIcon keeps Unicode menus value-safe and diagnosable", []
	{
		NotifyIconMenuItem root(L"操作", 10);
		root.HasSubMenu = true;
		CUI_EXPECT_TRUE(root.TryAddSubItem(
			NotifyIconMenuItem(L"打开窗口", 11)));
		CUI_EXPECT_TRUE(root.TryAddSubItem(
			NotifyIconMenuItem(L"暂停任务", 12, false)));
		CUI_EXPECT_FALSE(root.TryAddSubItem(
			NotifyIconMenuItem(L"重复命令", 11)));
		root.SubMenu = reinterpret_cast<HMENU>(static_cast<UINT_PTR>(1));
		NotifyIconMenuItem copiedRoot = root;
		CUI_EXPECT_EQ(nullptr, copiedRoot.SubMenu);
		CUI_EXPECT_EQ(std::wstring(L"打开窗口"), copiedRoot.SubItems[0].Text);
		NotifyIconMenuItem utf8Item(
			"\xE9\x80\x9A\xE7\x9F\xA5", 50);
		CUI_EXPECT_EQ(std::wstring(L"通知"), utf8Item.Text);

		NotifyIcon icon;
		CUI_EXPECT_FALSE(icon.IsInitialized());
		CUI_EXPECT_FALSE(icon.IsVisible());
		CUI_EXPECT_TRUE(icon.TrySetToolTip(L"CUI 通知中心"));
		CUI_EXPECT_EQ(std::wstring(L"CUI 通知中心"), icon.GetToolTip());
		CUI_EXPECT_FALSE(icon.TryInitialize(
			reinterpret_cast<HWND>(static_cast<UINT_PTR>(1)), 1, WM_COMMAND));
		CUI_EXPECT_EQ(E_INVALIDARG, icon.GetLastError());
		CUI_EXPECT_FALSE(icon.TryInitialize(nullptr, 1));
		CUI_EXPECT_EQ(E_HANDLE, icon.GetLastError());
		CUI_EXPECT_FALSE(icon.TryShow());
		CUI_EXPECT_EQ(E_HANDLE, icon.GetLastError());
		CUI_EXPECT_TRUE(icon.TryHide());

		CUI_EXPECT_TRUE(icon.TryAddMenuItem(copiedRoot));
		CUI_EXPECT_TRUE(icon.TryAddMenuSeparator());
		CUI_EXPECT_TRUE(icon.TryAddMenuItem(
			NotifyIconMenuItem(L"退出", 30)));
		CUI_EXPECT_EQ(3ULL, icon.MenuItemCount());
		CUI_EXPECT_EQ(5ULL, icon.MenuItemCount(true));
		CUI_EXPECT_FALSE(icon.TryAddMenuItem(
			NotifyIconMenuItem(L"重复退出", 30)));
		CUI_EXPECT_EQ(E_INVALIDARG, icon.GetLastError());

		auto* nested = icon.FindMenuItem(12);
		CUI_EXPECT_TRUE(nested != nullptr);
		CUI_EXPECT_FALSE(nested->Enabled);
		CUI_EXPECT_TRUE(icon.TryEnableMenuItem(12, true));
		CUI_EXPECT_TRUE(icon.FindMenuItem(12)->Enabled);
		CUI_EXPECT_TRUE(icon.TrySetMenuItemText(12, L"继续任务"));
		CUI_EXPECT_EQ(std::wstring(L"继续任务"),
			icon.FindMenuItem(12)->Text);
		CUI_EXPECT_FALSE(icon.TryEnableMenuItem(999, true));
		CUI_EXPECT_TRUE(FAILED(icon.GetLastError()));

		CUI_EXPECT_TRUE(icon.RemoveMenuItem(11));
		CUI_EXPECT_TRUE(icon.FindMenuItem(11) == nullptr);
		CUI_EXPECT_EQ(4ULL, icon.MenuItemCount(true));
		CUI_EXPECT_FALSE(icon.RemoveMenuItem(11));

		auto* detached = icon.CreateSubMenu(L"更多", 40);
		CUI_EXPECT_TRUE(detached != nullptr);
		CUI_EXPECT_TRUE(detached->TryAddSubItem(
			NotifyIconMenuItem(L"诊断", 41)));
		CUI_EXPECT_TRUE(icon.TryAddMenuItem(*detached));
		CUI_EXPECT_TRUE(icon.FindMenuItem(41) != nullptr);
		CUI_EXPECT_EQ(6ULL, icon.MenuItemCount(true));

		icon.ClearMenu();
		CUI_EXPECT_EQ(0ULL, icon.MenuItemCount());
		CUI_EXPECT_FALSE(icon.TryShowContextMenu(0, 0));
		CUI_EXPECT_EQ(E_HANDLE, icon.GetLastError());
	});

	runner.Add("Taskbar uses per-instance COM ownership and safe failures", []
	{
		Taskbar first(nullptr);
		Taskbar second(nullptr);
		CUI_EXPECT_EQ(nullptr, first.Handle);
		CUI_EXPECT_EQ(Taskbar::ProgressState::NoProgress, first.GetState());
		CUI_EXPECT_EQ(0ULL, first.GetValue());
		CUI_EXPECT_EQ(0ULL, first.GetTotal());
		CUI_EXPECT_FALSE(first.TrySetValue(50, 100));
		CUI_EXPECT_TRUE(FAILED(first.GetLastError()));
		CUI_EXPECT_FALSE(second.TrySetNormal());
		CUI_EXPECT_TRUE(FAILED(second.GetLastError()));
		CUI_EXPECT_FALSE(first.TrySetState(static_cast<TBPFLAG>(999)));
		CUI_EXPECT_EQ(E_INVALIDARG, first.GetLastError());
		CUI_EXPECT_FALSE(first.Initialize(nullptr));
		CUI_EXPECT_TRUE(FAILED(first.GetLastError()));

		// Compatibility wrappers must remain null-safe and preserve diagnostics.
		first.SetValue(1, 0);
		first.SetIndeterminate();
		first.SetPaused();
		first.SetError();
		first.SetNormal();
		first.Clear();
		CUI_EXPECT_TRUE(FAILED(first.GetLastError()));
	});

	runner.Add("Keyboard navigation uses stable tab order and unified actions", []
	{
		Panel root;
		auto* later = root.Add<Button>(L"&Later", 0, 0);
		auto* editor = root.Add<TextBox>(L"A&B", 0, 30);
		auto* earlier = root.Add<Button>(L"&Earlier", 0, 60);
		auto* label = root.Add<Label>(L"Not focusable", 0, 90);
		auto* skipped = root.Add<Button>(L"Skipped", 0, 120);
		later->TabIndex = 2;
		editor->TabIndex = 1;
		earlier->TabIndex = 1;
		label->TabIndex = 0;
		skipped->TabIndex = 0;
		skipped->IsTabStop = false;

		std::vector<Control*> roots{ &root };
		auto order = Form::BuildTabOrder(
			std::span<Control* const>(roots.data(), roots.size()));
		CUI_EXPECT_EQ(3ULL, order.size());
		CUI_EXPECT_EQ(editor, order[0]);
		CUI_EXPECT_EQ(earlier, order[1]);
		CUI_EXPECT_EQ(later, order[2]);
		CUI_EXPECT_EQ(L'\0', editor->GetEffectiveAccessKey());
		CUI_EXPECT_EQ(L'E', earlier->GetEffectiveAccessKey());
		CUI_EXPECT_EQ(std::wstring(L"Earlier"),
			earlier->GetEffectiveAccessibleName());
		CUI_EXPECT_EQ(std::wstring(L"Alt+E"),
			earlier->GetEffectiveKeyboardShortcut());
		CUI_EXPECT_TRUE(editor->GetEffectiveAccessibleName().empty());
		Button escapedAccessMarker(L"Save && Close", 0, 0);
		CUI_EXPECT_EQ(std::wstring(L"Save & Close"),
			escapedAccessMarker.GetDisplayText());

		root.Enable = false;
		CUI_EXPECT_TRUE(Form::BuildTabOrder(
			std::span<Control* const>(roots.data(), roots.size())).empty());
		root.Enable = true;
		root.Visible = false;
		CUI_EXPECT_TRUE(Form::BuildTabOrder(
			std::span<Control* const>(roots.data(), roots.size())).empty());
		root.Visible = true;

		int buttonClicks = 0;
		auto buttonConnection = earlier->OnMouseClick.Subscribe(
			[&](Control*, MouseEventArgs) { ++buttonClicks; });
		CUI_EXPECT_TRUE(earlier->Invoke());
		CUI_EXPECT_EQ(1, buttonClicks);

		CheckBox checkBox(L"&Remember", 0, 0);
		int checkedChanges = 0;
		auto checkedConnection = checkBox.OnChecked.Subscribe(
			[&](Control*) { ++checkedChanges; });
		CUI_EXPECT_TRUE(checkBox.Invoke());
		CUI_EXPECT_TRUE(checkBox.Checked);
		CUI_EXPECT_EQ(1, checkedChanges);
		CUI_EXPECT_TRUE(checkBox.Invoke());
		CUI_EXPECT_FALSE(checkBox.Checked);
		CUI_EXPECT_EQ(2, checkedChanges);

		PasswordBox password(L"secret", 0, 0);
		auto passwordSnapshot = password.GetAccessibilitySnapshot();
		CUI_EXPECT_TRUE(passwordSnapshot.Password);
		CUI_EXPECT_TRUE(passwordSnapshot.Name.empty());
		CUI_EXPECT_TRUE(passwordSnapshot.Value.empty());
		password.AccessibleName = L"Account password";
		password.AutomationId = L"accountPassword";
		passwordSnapshot = password.GetAccessibilitySnapshot();
		CUI_EXPECT_EQ(std::wstring(L"Account password"), passwordSnapshot.Name);
		CUI_EXPECT_EQ(std::wstring(L"accountPassword"), passwordSnapshot.AutomationId);
		CUI_EXPECT_EQ(AccessibleRole::PasswordBox, passwordSnapshot.Role);

		const auto* tabIndexMetadata = earlier->FindPropertyMetadata(L"TabIndex");
		const auto* accessibleNameMetadata = earlier->FindPropertyMetadata(L"AccessibleName");
		const auto* roleMetadata = earlier->FindPropertyMetadata(L"AccessibleRole");
		CUI_EXPECT_TRUE(tabIndexMetadata != nullptr);
		CUI_EXPECT_TRUE(accessibleNameMetadata != nullptr);
		CUI_EXPECT_TRUE(roleMetadata != nullptr);
		CUI_EXPECT_EQ(BindingValueKind::Int, tabIndexMetadata->ValueKind());
		CUI_EXPECT_EQ(BindingValueKind::String, accessibleNameMetadata->ValueKind());
	});

	runner.Add("Designer persists keyboard and accessibility metadata", []
	{
		Button button(L"Run", 0, 0);
		auto designer = std::make_shared<DesignerControl>(
			&button, L"runButton", UIClass::UI_Button);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto applyTracked = [&](const wchar_t* propertyName,
			DesignerStyleValue value)
		{
			CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyValue(
				button, propertyName, value,
				&canonicalName, &effectiveValue, &error));
			designer->MetadataProperties[canonicalName] = effectiveValue;
		};
		applyTracked(L"TabIndex", { DesignerStyleValueKind::Int, L"4" });
		applyTracked(L"IsTabStop", { DesignerStyleValueKind::Bool, L"false" });
		applyTracked(L"AccessKey", { DesignerStyleValueKind::String, L"R" });
		applyTracked(L"AccessibleName", {
			DesignerStyleValueKind::String, L"Run command" });
		applyTracked(L"AutomationId", {
			DesignerStyleValueKind::String, L"runCommand" });
		applyTracked(L"AccessibleRole", {
			DesignerStyleValueKind::Int,
			std::to_wstring(static_cast<int>(AccessibleRole::Button)) });

		CUI_EXPECT_EQ(4, button.TabIndex);
		CUI_EXPECT_FALSE(button.IsTabStop);
		CUI_EXPECT_EQ(std::wstring(L"Run command"), button.AccessibleName);
		CUI_EXPECT_EQ(AccessibleRole::Button, button.AccessibleRole);
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(button);
		const auto* role = DesignerPropertyCatalog::Find(properties, L"AccessibleRole");
		CUI_EXPECT_TRUE(role != nullptr);
		if (role)
		{
			CUI_EXPECT_EQ(DesignerStyleValueKind::Int, role->ValueKind);
			CUI_EXPECT_EQ(ControlPropertyEditorKind::Choice, role->Editor);
			CUI_EXPECT_TRUE(!role->Choices.empty());
		}

		CodeGenInput input;
		input.Controls = { designer };
		CodeGenerator generator(L"KeyboardMetadataForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"runButton->TrySetPropertyValue(L\"TabIndex\", BindingValue(4))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"runButton->TrySetPropertyValue(L\"IsTabStop\", BindingValue(false))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"runButton->TrySetPropertyValue(L\"AccessibleName\", BindingValue(L\"Run command\"))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"runButton->TrySetPropertyValue(L\"AccessibleRole\", BindingValue(6))")
			!= std::string::npos);
	});

	runner.Add("Form exposes custom controls through a lifetime-safe MSAA bridge", []
	{
		Form form(L"Accessibility host", POINT{ 120, 120 }, SIZE{ 320, 200 });
		CUI_EXPECT_TRUE(form.Handle != nullptr);
		if (!form.Handle) return;

		auto* button = form.Add<Button>(L"&Run", 16, 20, 120, 30);
		button->AccessibleName = L"Run command";
		button->AccessibleDescription = L"Starts the selected command";
		button->AccessibleHelpText = L"Press Enter or Alt+R";
		button->AutomationId = L"runCommand";
		CUI_EXPECT_TRUE(form.SetDefaultButton(button));
		CUI_EXPECT_TRUE(form.SetCancelButton(button));
		CUI_EXPECT_EQ(button, form.GetDefaultButton());
		CUI_EXPECT_EQ(button, form.GetCancelButton());

		int clicks = 0;
		auto clickConnection = button->OnMouseClick.Subscribe(
			[&](Control*, MouseEventArgs) { ++clicks; });
		IAccessible* accessible = nullptr;
		const HRESULT queryResult = ::AccessibleObjectFromWindow(
			form.Handle, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
			reinterpret_cast<void**>(&accessible));
		CUI_EXPECT_TRUE(SUCCEEDED(queryResult));
		CUI_EXPECT_TRUE(accessible != nullptr);
		if (!accessible)
		{
			::DestroyWindow(form.Handle);
			return;
		}

		long childCount = 0;
		CUI_EXPECT_EQ(S_OK, accessible->get_accChildCount(&childCount));
		CUI_EXPECT_EQ(1L, childCount);
		VARIANT child{};
		child.vt = VT_I4;
		child.lVal = 1;
		BSTR name = nullptr;
		CUI_EXPECT_EQ(S_OK, accessible->get_accName(child, &name));
		CUI_EXPECT_TRUE(name != nullptr);
		if (name)
		{
			CUI_EXPECT_EQ(std::wstring(L"Run command"), std::wstring(name));
			::SysFreeString(name);
		}
		VARIANT role{};
		CUI_EXPECT_EQ(S_OK, accessible->get_accRole(child, &role));
		CUI_EXPECT_EQ(VT_I4, role.vt);
		CUI_EXPECT_EQ((LONG)ROLE_SYSTEM_PUSHBUTTON, role.lVal);
		BSTR shortcut = nullptr;
		CUI_EXPECT_EQ(S_OK,
			accessible->get_accKeyboardShortcut(child, &shortcut));
		CUI_EXPECT_TRUE(shortcut != nullptr);
		if (shortcut)
		{
			CUI_EXPECT_EQ(std::wstring(L"Alt+R"), std::wstring(shortcut));
			::SysFreeString(shortcut);
		}

		CUI_EXPECT_EQ(S_OK, accessible->accDoDefaultAction(child));
		CUI_EXPECT_EQ(1, clicks);
		CUI_EXPECT_TRUE(form.MoveFocus(true));
		CUI_EXPECT_EQ(button, form.Selected);
		VARIANT focused{};
		CUI_EXPECT_EQ(S_OK, accessible->get_accFocus(&focused));
		CUI_EXPECT_EQ(VT_I4, focused.vt);
		CUI_EXPECT_EQ(1L, focused.lVal);

		::DestroyWindow(form.Handle);
		childCount = -1;
		CUI_EXPECT_EQ(CO_E_OBJNOTCONNECTED,
			accessible->get_accChildCount(&childCount));
		accessible->Release();
	});

	runner.Add("Form exposes native UIA fragments and control patterns", []
	{
		const HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		struct ComScope
		{
			bool Active = false;
			~ComScope() { if (Active) ::CoUninitialize(); }
		} comScope{ SUCCEEDED(comResult) };
		Form form(L"Native UIA host", POINT{ 140, 140 }, SIZE{ 520, 360 });
		CUI_EXPECT_TRUE(form.Handle != nullptr);
		if (!form.Handle)
			return;

		auto* panel = form.Add<Panel>(8, 8, 480, 310);
		panel->AutomationId = L"contentPanel";
		auto* button = panel->Add<Button>(L"&Run", 10, 10, 100, 28);
		button->AutomationId = L"runButton";
		button->AccessibleName = L"Run command";
		auto* check = panel->Add<CheckBox>(L"&Remember", 10, 46);
		check->AutomationId = L"rememberCheck";
		auto* editor = panel->Add<TextBox>(L"initial", 10, 80, 150, 28);
		editor->AutomationId = L"nameEditor";
		editor->AccessibleName = L"Name";
		auto* password = panel->Add<PasswordBox>(L"secret", 170, 80, 150, 28);
		password->AutomationId = L"passwordEditor";
		password->AccessibleName = L"Password";
		auto* slider = panel->Add<Slider>(10, 118, 220, 30);
		slider->AutomationId = L"volumeSlider";
		slider->Min = 0.0f;
		slider->Max = 10.0f;
		slider->Step = 0.5f;
		slider->Value = 2.0f;
		auto* combo = panel->Add<ComboBox>(L"", 10, 156, 150, 28);
		combo->AutomationId = L"choiceCombo";
		std::vector<std::wstring> comboItems{ L"One", L"Two" };
		combo->Items = comboItems;

		auto* tabs = panel->Add<TabControl>(250, 118, 210, 150);
		tabs->AutomationId = L"mainTabs";
		auto* firstPage = tabs->AddPage(L"First");
		firstPage->AutomationId = L"firstPage";
		auto* secondPage = tabs->AddPage(L"Second");
		secondPage->AutomationId = L"secondPage";

		int clicks = 0;
		auto clickConnection = button->OnMouseClick.Subscribe(
			[&](Control*, MouseEventArgs) { ++clicks; });

		Microsoft::WRL::ComPtr<IUIAutomation> automation;
		const HRESULT automationResult = ::CoCreateInstance(
			CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&automation));
		CUI_EXPECT_TRUE(SUCCEEDED(automationResult));
		if (!automation)
		{
			::DestroyWindow(form.Handle);
			return;
		}

		Microsoft::WRL::ComPtr<IUIAutomationElement> root;
		CUI_EXPECT_EQ(S_OK, automation->ElementFromHandle(form.Handle, &root));
		CUI_EXPECT_TRUE(root != nullptr);
		auto findByAutomationId = [&](const wchar_t* automationId)
		{
			Microsoft::WRL::ComPtr<IUIAutomationElement> element;
			VARIANT expected{};
			expected.vt = VT_BSTR;
			expected.bstrVal = ::SysAllocString(automationId);
			Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
			if (expected.bstrVal
				&& SUCCEEDED(automation->CreatePropertyCondition(
					UIA_AutomationIdPropertyId, expected, &condition))
				&& condition && root)
			{
				(void)root->FindFirst(
					TreeScope_Descendants, condition.Get(), &element);
			}
			::VariantClear(&expected);
			return element;
		};

		auto buttonElement = findByAutomationId(L"runButton");
		CUI_EXPECT_TRUE(buttonElement != nullptr);
		if (buttonElement)
		{
			BSTR name = nullptr;
			CUI_EXPECT_EQ(S_OK, buttonElement->get_CurrentName(&name));
			CUI_EXPECT_EQ(std::wstring(L"Run command"),
				name ? std::wstring(name) : std::wstring{});
			::SysFreeString(name);
			CONTROLTYPEID controlType = 0;
			CUI_EXPECT_EQ(S_OK,
				buttonElement->get_CurrentControlType(&controlType));
			CUI_EXPECT_EQ((CONTROLTYPEID)UIA_ButtonControlTypeId, controlType);
			Microsoft::WRL::ComPtr<IUIAutomationInvokePattern> invoke;
			CUI_EXPECT_EQ(S_OK, buttonElement->GetCurrentPatternAs(
				UIA_InvokePatternId, IID_PPV_ARGS(&invoke)));
			CUI_EXPECT_TRUE(invoke != nullptr);
			if (invoke) CUI_EXPECT_EQ(S_OK, invoke->Invoke());
			CUI_EXPECT_EQ(1, clicks);

			Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> walker;
			CUI_EXPECT_EQ(S_OK, automation->get_ControlViewWalker(&walker));
			Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
			if (walker)
				CUI_EXPECT_EQ(S_OK,
					walker->GetParentElement(buttonElement.Get(), &parent));
			BSTR parentId = nullptr;
			if (parent)
				CUI_EXPECT_EQ(S_OK, parent->get_CurrentAutomationId(&parentId));
			CUI_EXPECT_EQ(std::wstring(L"contentPanel"),
				parentId ? std::wstring(parentId) : std::wstring{});
			::SysFreeString(parentId);
		}

		auto checkElement = findByAutomationId(L"rememberCheck");
		Microsoft::WRL::ComPtr<IUIAutomationTogglePattern> toggle;
		CUI_EXPECT_TRUE(checkElement != nullptr);
		if (checkElement)
			CUI_EXPECT_EQ(S_OK, checkElement->GetCurrentPatternAs(
				UIA_TogglePatternId, IID_PPV_ARGS(&toggle)));
		if (toggle)
		{
			CUI_EXPECT_EQ(S_OK, toggle->Toggle());
			ToggleState state = ToggleState_Off;
			CUI_EXPECT_EQ(S_OK, toggle->get_CurrentToggleState(&state));
			CUI_EXPECT_EQ(ToggleState_On, state);
		}

		auto editorElement = findByAutomationId(L"nameEditor");
		Microsoft::WRL::ComPtr<IUIAutomationValuePattern> editorValue;
		if (editorElement)
			CUI_EXPECT_EQ(S_OK, editorElement->GetCurrentPatternAs(
				UIA_ValuePatternId, IID_PPV_ARGS(&editorValue)));
		CUI_EXPECT_TRUE(editorValue != nullptr);
		if (editorValue)
		{
			BSTR updatedText = ::SysAllocString(L"updated");
			CUI_EXPECT_TRUE(updatedText != nullptr);
			if (updatedText)
				CUI_EXPECT_EQ(S_OK, editorValue->SetValue(updatedText));
			::SysFreeString(updatedText);
			CUI_EXPECT_EQ(std::wstring(L"updated"),
				static_cast<std::wstring>(editor->Text));
		}

		auto passwordElement = findByAutomationId(L"passwordEditor");
		BOOL isPassword = FALSE;
		if (passwordElement)
			CUI_EXPECT_EQ(S_OK,
				passwordElement->get_CurrentIsPassword(&isPassword));
		CUI_EXPECT_TRUE(isPassword != FALSE);
		Microsoft::WRL::ComPtr<IUIAutomationValuePattern> passwordValue;
		if (passwordElement)
			(void)passwordElement->GetCurrentPatternAs(
				UIA_ValuePatternId, IID_PPV_ARGS(&passwordValue));
		BSTR exposedPassword = nullptr;
		if (passwordValue)
			CUI_EXPECT_EQ(S_OK,
				passwordValue->get_CurrentValue(&exposedPassword));
		CUI_EXPECT_TRUE(!exposedPassword || std::wstring(exposedPassword).empty());
		::SysFreeString(exposedPassword);

		auto sliderElement = findByAutomationId(L"volumeSlider");
		Microsoft::WRL::ComPtr<IUIAutomationRangeValuePattern> range;
		if (sliderElement)
			CUI_EXPECT_EQ(S_OK, sliderElement->GetCurrentPatternAs(
				UIA_RangeValuePatternId, IID_PPV_ARGS(&range)));
		CUI_EXPECT_TRUE(range != nullptr);
		if (range)
		{
			double minimum = -1.0, maximum = -1.0, step = -1.0;
			CUI_EXPECT_EQ(S_OK, range->get_CurrentMinimum(&minimum));
			CUI_EXPECT_EQ(S_OK, range->get_CurrentMaximum(&maximum));
			CUI_EXPECT_EQ(S_OK, range->get_CurrentSmallChange(&step));
			CUI_EXPECT_EQ(0.0, minimum);
			CUI_EXPECT_EQ(10.0, maximum);
			CUI_EXPECT_EQ(0.5, step);
			CUI_EXPECT_EQ(S_OK, range->SetValue(7.5));
			CUI_EXPECT_EQ(7.5f, static_cast<float>(slider->Value));
		}

		auto comboElement = findByAutomationId(L"choiceCombo");
		Microsoft::WRL::ComPtr<IUIAutomationExpandCollapsePattern> expand;
		if (comboElement)
			CUI_EXPECT_EQ(S_OK, comboElement->GetCurrentPatternAs(
				UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&expand)));
		if (expand)
		{
			CUI_EXPECT_EQ(S_OK, expand->Expand());
			CUI_EXPECT_TRUE(combo->Expand);
			CUI_EXPECT_EQ(S_OK, expand->Collapse());
			CUI_EXPECT_FALSE(combo->Expand);
		}

		auto secondPageElement = findByAutomationId(L"secondPage");
		Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> selectionItem;
		if (secondPageElement)
			CUI_EXPECT_EQ(S_OK, secondPageElement->GetCurrentPatternAs(
				UIA_SelectionItemPatternId, IID_PPV_ARGS(&selectionItem)));
		if (selectionItem)
		{
			CUI_EXPECT_EQ(S_OK, selectionItem->Select());
			CUI_EXPECT_EQ(1, static_cast<int>(tabs->SelectedIndex));
			BOOL selected = FALSE;
			CUI_EXPECT_EQ(S_OK, selectionItem->get_CurrentIsSelected(&selected));
			CUI_EXPECT_TRUE(selected != FALSE);
		}

		::DestroyWindow(form.Handle);
		if (buttonElement)
		{
			BSTR detachedName = nullptr;
			CUI_EXPECT_TRUE(FAILED(buttonElement->get_CurrentName(&detachedName)));
			::SysFreeString(detachedName);
		}
	});

	runner.Add("Virtual collections expose native UIA item and grid patterns", []
	{
		const HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		struct ComScope
		{
			bool Active = false;
			~ComScope() { if (Active) ::CoUninitialize(); }
		} comScope{ SUCCEEDED(comResult) };

		Form form(L"Virtual UIA host", POINT{ 160, 160 }, SIZE{ 760, 560 });
		CUI_EXPECT_TRUE(form.Handle != nullptr);
		if (!form.Handle) return;

		auto* list = form.Add<ListView>(10, 10, 220, 150);
		list->AutomationId = L"virtualList";
		list->ShowCheckBoxes = true;
		list->SelectionMode = ListViewSelectionMode::Multiple;
		ListViewItem alpha(L"Alpha");
		ListViewItem beta(L"Beta");
		beta.Checked = true;
		list->AddItem(alpha);
		list->AddItem(beta);

		auto* combo = form.Add<ComboBox>(L"", 250, 10, 180, 28);
		combo->AutomationId = L"virtualCombo";
		std::vector<std::wstring> choices{ L"One", L"Two", L"Three" };
		combo->Items = choices;

		auto* tree = form.Add<TreeView>(250, 60, 220, 150);
		tree->AutomationId = L"virtualTree";
		auto* parentNode = new TreeNode(L"Parent");
		auto* childNode = new TreeNode(L"Child");
		parentNode->Children.push_back(childNode);
		tree->Root->Children.push_back(parentNode);

		auto* grid = form.Add<GridView>(10, 180, 500, 190);
		grid->AutomationId = L"virtualGrid";
		grid->AddColumn(GridViewColumn(L"Name", 180.0f, ColumnType::Text, true));
		grid->AddColumn(GridViewColumn(L"Enabled", 110.0f, ColumnType::Check, false));
		grid->AddColumn(GridViewColumn(L"Action", 120.0f, ColumnType::Button, false));
		grid->Columns[2].ButtonText = L"Run";
		GridViewRow row;
		row.Cells.emplace_back(L"Editable");
		row.Cells.emplace_back(false);
		row.Cells.emplace_back(L"");
		grid->AddRow(row);

		auto* detailsList = form.Add<ListView>(10, 385, 500, 125);
		detailsList->AutomationId = L"detailsList";
		detailsList->ViewMode = ListViewViewMode::Details;
		detailsList->HeaderHeight = 28.0f;
		detailsList->RowHeight = 28.0f;
		detailsList->AddColumn(ListViewColumn(L"Name", 220.0f));
		detailsList->AddColumn(ListViewColumn(L"Status", 180.0f));
		for (int index = 0; index < 8; ++index)
		{
			ListViewItem item(L"Entry " + std::to_wstring(index));
			item.SubItems.push_back(index % 2 == 0 ? L"Ready" : L"Busy");
			detailsList->AddItem(item);
		}
		auto* indexedProbe = form.Add<IndexedVirtualProbe>();
		indexedProbe->AutomationId = L"indexedProbe";

		Microsoft::WRL::ComPtr<IUIAutomation> automation;
		CUI_EXPECT_EQ(S_OK, ::CoCreateInstance(
			CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&automation)));
		if (!automation)
		{
			::DestroyWindow(form.Handle);
			return;
		}
		Microsoft::WRL::ComPtr<IUIAutomationElement> root;
		CUI_EXPECT_EQ(S_OK, automation->ElementFromHandle(form.Handle, &root));
		auto findByAutomationId = [&](const wchar_t* automationId)
		{
			Microsoft::WRL::ComPtr<IUIAutomationElement> element;
			VARIANT expected{};
			expected.vt = VT_BSTR;
			expected.bstrVal = ::SysAllocString(automationId);
			Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
			if (expected.bstrVal && root
				&& SUCCEEDED(automation->CreatePropertyCondition(
					UIA_AutomationIdPropertyId, expected, &condition))
				&& condition)
				(void)root->FindFirst(TreeScope_Descendants,
					condition.Get(), &element);
			::VariantClear(&expected);
			return element;
		};
		Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> walker;
		CUI_EXPECT_EQ(S_OK, automation->get_ControlViewWalker(&walker));

		auto indexedElement = findByAutomationId(L"indexedProbe");
		Microsoft::WRL::ComPtr<IUIAutomationElement> indexedFirst;
		Microsoft::WRL::ComPtr<IUIAutomationElement> indexedSecond;
		if (walker && indexedElement)
		{
			CUI_EXPECT_EQ(S_OK, walker->GetFirstChildElement(
				indexedElement.Get(), &indexedFirst));
			if (indexedFirst)
				CUI_EXPECT_EQ(S_OK, walker->GetNextSiblingElement(
					indexedFirst.Get(), &indexedSecond));
		}
		CUI_EXPECT_TRUE(indexedFirst != nullptr);
		CUI_EXPECT_TRUE(indexedSecond != nullptr);
		CUI_EXPECT_TRUE(indexedProbe->IndexedCountQueries > 0);
		CUI_EXPECT_TRUE(indexedProbe->IndexedChildQueries > 0);
		CUI_EXPECT_TRUE(indexedProbe->IndexedSiblingQueries > 0);
		CUI_EXPECT_EQ(0, indexedProbe->LegacyChildrenQueries);
		uint32_t indexedHitId = 0;
		CUI_EXPECT_TRUE(indexedProbe->TryHitTestAccessibilityVirtualNode(
			10.0f, 45.0f, indexedHitId));
		CUI_EXPECT_EQ(indexedProbe->Ids[1], indexedHitId);
		CUI_EXPECT_TRUE(indexedProbe->DirectHitQueries > 0);
		CUI_EXPECT_EQ(0, indexedProbe->LegacyChildrenQueries);

		auto listElement = findByAutomationId(L"virtualList");
		CUI_EXPECT_TRUE(listElement != nullptr);
		Microsoft::WRL::ComPtr<IUIAutomationElement> alphaElement;
		if (walker && listElement)
			CUI_EXPECT_EQ(S_OK, walker->GetFirstChildElement(
				listElement.Get(), &alphaElement));
		CUI_EXPECT_TRUE(alphaElement != nullptr);
		if (alphaElement)
		{
			BSTR name = nullptr;
			CUI_EXPECT_EQ(S_OK, alphaElement->get_CurrentName(&name));
			CUI_EXPECT_EQ(std::wstring(L"Alpha"),
				name ? std::wstring(name) : std::wstring{});
			::SysFreeString(name);
			CONTROLTYPEID type = 0;
			CUI_EXPECT_EQ(S_OK, alphaElement->get_CurrentControlType(&type));
			CUI_EXPECT_EQ((CONTROLTYPEID)UIA_ListItemControlTypeId, type);
		}
		Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> alphaSelection;
		Microsoft::WRL::ComPtr<IUIAutomationTogglePattern> alphaToggle;
		if (alphaElement)
		{
			CUI_EXPECT_EQ(S_OK, alphaElement->GetCurrentPatternAs(
				UIA_SelectionItemPatternId, IID_PPV_ARGS(&alphaSelection)));
			CUI_EXPECT_EQ(S_OK, alphaElement->GetCurrentPatternAs(
				UIA_TogglePatternId, IID_PPV_ARGS(&alphaToggle)));
		}
		if (alphaSelection)
		{
			CUI_EXPECT_EQ(S_OK, alphaSelection->Select());
		}
		CUI_EXPECT_TRUE(list->Items[0].Selected);
		if (alphaToggle) CUI_EXPECT_EQ(S_OK, alphaToggle->Toggle());
		CUI_EXPECT_TRUE(list->Items[0].Checked);

		Microsoft::WRL::ComPtr<IUIAutomationSelectionPattern> listSelection;
		if (listElement)
			CUI_EXPECT_EQ(S_OK, listElement->GetCurrentPatternAs(
				UIA_SelectionPatternId, IID_PPV_ARGS(&listSelection)));
		CUI_EXPECT_EQ(ListViewSelectionMode::Multiple,
			static_cast<ListViewSelectionMode>(list->SelectionMode));
		BOOL canSelectMultiple = FALSE;
		if (listSelection)
			CUI_EXPECT_EQ(S_OK, listSelection->get_CurrentCanSelectMultiple(
				&canSelectMultiple));
		CUI_EXPECT_TRUE(canSelectMultiple != FALSE);
		Microsoft::WRL::ComPtr<IUIAutomationElementArray> selectedItems;
		if (listSelection)
			CUI_EXPECT_EQ(S_OK, listSelection->GetCurrentSelection(&selectedItems));
		int selectedCount = 0;
		if (selectedItems)
			CUI_EXPECT_EQ(S_OK, selectedItems->get_Length(&selectedCount));
		CUI_EXPECT_EQ(1, selectedCount);
		Microsoft::WRL::ComPtr<IUIAutomationElement> betaElement;
		Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> betaSelection;
		if (walker && alphaElement)
			CUI_EXPECT_EQ(S_OK, walker->GetNextSiblingElement(
				alphaElement.Get(), &betaElement));
		CUI_EXPECT_TRUE(betaElement != nullptr);
		if (betaElement)
			CUI_EXPECT_EQ(S_OK, betaElement->GetCurrentPatternAs(
				UIA_SelectionItemPatternId, IID_PPV_ARGS(&betaSelection)));
		CUI_EXPECT_TRUE(betaSelection != nullptr);
		if (alphaSelection)
			CUI_EXPECT_EQ(S_OK, alphaSelection->AddToSelection());
		CUI_EXPECT_TRUE(list->Items[0].Selected);
		CUI_EXPECT_FALSE(list->Items[1].Selected);
		if (betaSelection)
			CUI_EXPECT_EQ(S_OK, betaSelection->AddToSelection());
		CUI_EXPECT_TRUE(list->Items[0].Selected);
		CUI_EXPECT_TRUE(list->Items[1].Selected);
		selectedItems.Reset();
		if (listSelection)
			CUI_EXPECT_EQ(S_OK, listSelection->GetCurrentSelection(&selectedItems));
		selectedCount = 0;
		if (selectedItems)
			CUI_EXPECT_EQ(S_OK, selectedItems->get_Length(&selectedCount));
		CUI_EXPECT_EQ(2, selectedCount);
		if (alphaSelection) CUI_EXPECT_EQ(S_OK, alphaSelection->Select());
		CUI_EXPECT_FALSE(list->Items[1].Selected);

		CUI_EXPECT_TRUE(list->SwapItems(0, 1));
		BSTR movedName = nullptr;
		if (alphaElement)
			CUI_EXPECT_EQ(S_OK, alphaElement->get_CurrentName(&movedName));
		CUI_EXPECT_EQ(std::wstring(L"Alpha"),
			movedName ? std::wstring(movedName) : std::wstring{});
		::SysFreeString(movedName);

		auto comboElement = findByAutomationId(L"virtualCombo");
		Microsoft::WRL::ComPtr<IUIAutomationElement> firstChoice;
		Microsoft::WRL::ComPtr<IUIAutomationElement> secondChoice;
		if (walker && comboElement)
		{
			CUI_EXPECT_EQ(S_OK, walker->GetFirstChildElement(
				comboElement.Get(), &firstChoice));
			if (firstChoice)
				CUI_EXPECT_EQ(S_OK, walker->GetNextSiblingElement(
					firstChoice.Get(), &secondChoice));
		}
		Microsoft::WRL::ComPtr<IUIAutomationScrollItemPattern> choiceScroll;
		Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> choiceSelection;
		if (secondChoice)
		{
			CUI_EXPECT_EQ(S_OK, secondChoice->GetCurrentPatternAs(
				UIA_ScrollItemPatternId, IID_PPV_ARGS(&choiceScroll)));
			CUI_EXPECT_EQ(S_OK, secondChoice->GetCurrentPatternAs(
				UIA_SelectionItemPatternId, IID_PPV_ARGS(&choiceSelection)));
		}
		if (choiceScroll) CUI_EXPECT_EQ(S_OK, choiceScroll->ScrollIntoView());
		CUI_EXPECT_TRUE(combo->Expand);
		if (choiceSelection) CUI_EXPECT_EQ(S_OK, choiceSelection->Select());
		CUI_EXPECT_EQ(1, static_cast<int>(combo->SelectedIndex));
		if (choiceSelection)
			CUI_EXPECT_EQ(S_OK, choiceSelection->AddToSelection());
		Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern> firstChoiceSelection;
		if (firstChoice)
			CUI_EXPECT_EQ(S_OK, firstChoice->GetCurrentPatternAs(
				UIA_SelectionItemPatternId,
				IID_PPV_ARGS(&firstChoiceSelection)));
		if (firstChoiceSelection)
			CUI_EXPECT_TRUE(FAILED(firstChoiceSelection->AddToSelection()));

		auto treeElement = findByAutomationId(L"virtualTree");
		Microsoft::WRL::ComPtr<IUIAutomationElement> parentElement;
		if (walker && treeElement)
			CUI_EXPECT_EQ(S_OK, walker->GetFirstChildElement(
				treeElement.Get(), &parentElement));
		CONTROLTYPEID parentType = 0;
		if (parentElement)
			CUI_EXPECT_EQ(S_OK, parentElement->get_CurrentControlType(&parentType));
		CUI_EXPECT_EQ((CONTROLTYPEID)UIA_TreeItemControlTypeId, parentType);
		Microsoft::WRL::ComPtr<IUIAutomationExpandCollapsePattern> treeExpand;
		if (parentElement)
			CUI_EXPECT_EQ(S_OK, parentElement->GetCurrentPatternAs(
				UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&treeExpand)));
		if (treeExpand) CUI_EXPECT_EQ(S_OK, treeExpand->Expand());
		CUI_EXPECT_TRUE(parentNode->Expand);
		Microsoft::WRL::ComPtr<IUIAutomationElement> childElement;
		if (walker && parentElement)
			CUI_EXPECT_EQ(S_OK, walker->GetFirstChildElement(
				parentElement.Get(), &childElement));
		BSTR childName = nullptr;
		if (childElement)
			CUI_EXPECT_EQ(S_OK, childElement->get_CurrentName(&childName));
		CUI_EXPECT_EQ(std::wstring(L"Child"),
			childName ? std::wstring(childName) : std::wstring{});
		::SysFreeString(childName);

		auto gridElement = findByAutomationId(L"virtualGrid");
		Microsoft::WRL::ComPtr<IUIAutomationGridPattern> gridPattern;
		Microsoft::WRL::ComPtr<IUIAutomationTablePattern> tablePattern;
		if (gridElement)
		{
			CUI_EXPECT_EQ(S_OK, gridElement->GetCurrentPatternAs(
				UIA_GridPatternId, IID_PPV_ARGS(&gridPattern)));
			CUI_EXPECT_EQ(S_OK, gridElement->GetCurrentPatternAs(
				UIA_TablePatternId, IID_PPV_ARGS(&tablePattern)));
		}
		int rowCount = 0, columnCount = 0;
		if (gridPattern)
		{
			CUI_EXPECT_EQ(S_OK, gridPattern->get_CurrentRowCount(&rowCount));
			CUI_EXPECT_EQ(S_OK, gridPattern->get_CurrentColumnCount(&columnCount));
		}
		CUI_EXPECT_EQ(1, rowCount);
		CUI_EXPECT_EQ(3, columnCount);
		Microsoft::WRL::ComPtr<IUIAutomationElement> textCell;
		if (gridPattern)
			CUI_EXPECT_EQ(S_OK, gridPattern->GetItem(0, 0, &textCell));
		Microsoft::WRL::ComPtr<IUIAutomationValuePattern> cellValue;
		Microsoft::WRL::ComPtr<IUIAutomationGridItemPattern> gridItem;
		if (textCell)
		{
			CUI_EXPECT_EQ(S_OK, textCell->GetCurrentPatternAs(
				UIA_ValuePatternId, IID_PPV_ARGS(&cellValue)));
			CUI_EXPECT_EQ(S_OK, textCell->GetCurrentPatternAs(
				UIA_GridItemPatternId, IID_PPV_ARGS(&gridItem)));
		}
		BSTR replacement = ::SysAllocString(L"Changed");
		if (cellValue && replacement)
			CUI_EXPECT_EQ(S_OK, cellValue->SetValue(replacement));
		::SysFreeString(replacement);
		CUI_EXPECT_EQ(std::wstring(L"Changed"), grid->Rows[0].Cells[0].Text);
		int cellRow = -1, cellColumn = -1;
		if (gridItem)
		{
			CUI_EXPECT_EQ(S_OK, gridItem->get_CurrentRow(&cellRow));
			CUI_EXPECT_EQ(S_OK, gridItem->get_CurrentColumn(&cellColumn));
		}
		CUI_EXPECT_EQ(0, cellRow);
		CUI_EXPECT_EQ(0, cellColumn);
		Microsoft::WRL::ComPtr<IUIAutomationElementArray> columnHeaders;
		if (tablePattern)
			CUI_EXPECT_EQ(S_OK,
				tablePattern->GetCurrentColumnHeaders(&columnHeaders));
		int headerCount = 0;
		if (columnHeaders)
			CUI_EXPECT_EQ(S_OK, columnHeaders->get_Length(&headerCount));
		CUI_EXPECT_EQ(3, headerCount);

		Microsoft::WRL::ComPtr<IUIAutomationElement> checkCell;
		if (gridPattern)
			CUI_EXPECT_EQ(S_OK, gridPattern->GetItem(0, 1, &checkCell));
		Microsoft::WRL::ComPtr<IUIAutomationTogglePattern> cellToggle;
		if (checkCell)
			CUI_EXPECT_EQ(S_OK, checkCell->GetCurrentPatternAs(
				UIA_TogglePatternId, IID_PPV_ARGS(&cellToggle)));
		if (cellToggle) CUI_EXPECT_EQ(S_OK, cellToggle->Toggle());
		CUI_EXPECT_TRUE(grid->Rows[0].Cells[1].GetBool());
		Microsoft::WRL::ComPtr<IUIAutomationScrollItemPattern> cellScroll;
		if (checkCell)
			CUI_EXPECT_EQ(S_OK, checkCell->GetCurrentPatternAs(
				UIA_ScrollItemPatternId, IID_PPV_ARGS(&cellScroll)));
		if (cellScroll) CUI_EXPECT_EQ(S_OK, cellScroll->ScrollIntoView());
		CUI_EXPECT_EQ(-1, static_cast<int>(grid->SelectedRowIndex));
		CUI_EXPECT_EQ(-1, static_cast<int>(grid->SelectedColumnIndex));

		auto detailsElement = findByAutomationId(L"detailsList");
		Microsoft::WRL::ComPtr<IUIAutomationGridPattern> detailsGrid;
		Microsoft::WRL::ComPtr<IUIAutomationTablePattern> detailsTable;
		Microsoft::WRL::ComPtr<IUIAutomationScrollPattern> detailsScroll;
		if (detailsElement)
		{
			CUI_EXPECT_EQ(S_OK, detailsElement->GetCurrentPatternAs(
				UIA_GridPatternId, IID_PPV_ARGS(&detailsGrid)));
			CUI_EXPECT_EQ(S_OK, detailsElement->GetCurrentPatternAs(
				UIA_TablePatternId, IID_PPV_ARGS(&detailsTable)));
			CUI_EXPECT_EQ(S_OK, detailsElement->GetCurrentPatternAs(
				UIA_ScrollPatternId, IID_PPV_ARGS(&detailsScroll)));
		}
		CUI_EXPECT_TRUE(detailsGrid != nullptr);
		CUI_EXPECT_TRUE(detailsTable != nullptr);
		CUI_EXPECT_TRUE(detailsScroll != nullptr);
		rowCount = 0;
		columnCount = 0;
		if (detailsGrid)
		{
			CUI_EXPECT_EQ(S_OK, detailsGrid->get_CurrentRowCount(&rowCount));
			CUI_EXPECT_EQ(S_OK, detailsGrid->get_CurrentColumnCount(&columnCount));
		}
		CUI_EXPECT_EQ(8, rowCount);
		CUI_EXPECT_EQ(2, columnCount);
		Microsoft::WRL::ComPtr<IUIAutomationElement> detailsCell;
		if (detailsGrid)
			CUI_EXPECT_EQ(S_OK, detailsGrid->GetItem(0, 1, &detailsCell));
		Microsoft::WRL::ComPtr<IUIAutomationGridItemPattern> detailsGridItem;
		Microsoft::WRL::ComPtr<IUIAutomationTableItemPattern> detailsTableItem;
		if (detailsCell)
		{
			CUI_EXPECT_EQ(S_OK, detailsCell->GetCurrentPatternAs(
				UIA_GridItemPatternId, IID_PPV_ARGS(&detailsGridItem)));
			CUI_EXPECT_EQ(S_OK, detailsCell->GetCurrentPatternAs(
				UIA_TableItemPatternId, IID_PPV_ARGS(&detailsTableItem)));
		}
		cellRow = -1;
		cellColumn = -1;
		if (detailsGridItem)
		{
			CUI_EXPECT_EQ(S_OK, detailsGridItem->get_CurrentRow(&cellRow));
			CUI_EXPECT_EQ(S_OK, detailsGridItem->get_CurrentColumn(&cellColumn));
		}
		CUI_EXPECT_EQ(0, cellRow);
		CUI_EXPECT_EQ(1, cellColumn);
		Microsoft::WRL::ComPtr<IUIAutomationElementArray> detailsHeaders;
		if (detailsTable)
			CUI_EXPECT_EQ(S_OK,
				detailsTable->GetCurrentColumnHeaders(&detailsHeaders));
		headerCount = 0;
		if (detailsHeaders)
			CUI_EXPECT_EQ(S_OK, detailsHeaders->get_Length(&headerCount));
		CUI_EXPECT_EQ(2, headerCount);
		Microsoft::WRL::ComPtr<IUIAutomationElementArray> cellHeaders;
		if (detailsTableItem)
			CUI_EXPECT_EQ(S_OK,
				detailsTableItem->GetCurrentColumnHeaderItems(&cellHeaders));
		int cellHeaderCount = 0;
		if (cellHeaders)
			CUI_EXPECT_EQ(S_OK, cellHeaders->get_Length(&cellHeaderCount));
		CUI_EXPECT_EQ(1, cellHeaderCount);
		BOOL verticallyScrollable = FALSE;
		double verticalPercent = -2.0;
		if (detailsScroll)
		{
			CUI_EXPECT_EQ(S_OK, detailsScroll->get_CurrentVerticallyScrollable(
				&verticallyScrollable));
			CUI_EXPECT_EQ(S_OK, detailsScroll->SetScrollPercent(
				UIA_ScrollPatternNoScroll, 100.0));
			CUI_EXPECT_EQ(S_OK, detailsScroll->get_CurrentVerticalScrollPercent(
				&verticalPercent));
			CUI_EXPECT_TRUE(FAILED(detailsScroll->SetScrollPercent(
				50.0, UIA_ScrollPatternNoScroll)));
			CUI_EXPECT_EQ(E_INVALIDARG, detailsScroll->SetScrollPercent(
				UIA_ScrollPatternNoScroll, 101.0));
		}
		CUI_EXPECT_TRUE(verticallyScrollable != FALSE);
		CUI_EXPECT_NEAR(100.0, verticalPercent, 0.000001);
		CUI_EXPECT_TRUE(detailsList->ScrollYOffset > 0.0f);
		if (detailsScroll)
			CUI_EXPECT_EQ(S_OK, detailsScroll->Scroll(
				ScrollAmount_NoAmount, ScrollAmount_SmallDecrement));
		CUI_EXPECT_TRUE(detailsList->ScrollYOffset > 0.0f);
		CUI_EXPECT_TRUE(detailsList->RemoveItemAt(0));
		BSTR removedCellName = nullptr;
		if (detailsCell)
			CUI_EXPECT_TRUE(FAILED(detailsCell->get_CurrentName(&removedCellName)));
		::SysFreeString(removedCellName);

		CUI_EXPECT_TRUE(list->RemoveItemAt(1));
		BSTR removedName = nullptr;
		if (alphaElement)
			CUI_EXPECT_TRUE(FAILED(alphaElement->get_CurrentName(&removedName)));
		::SysFreeString(removedName);

		::DestroyWindow(form.Handle);
		BSTR detachedChild = nullptr;
		if (childElement)
			CUI_EXPECT_TRUE(FAILED(childElement->get_CurrentName(&detachedChild)));
		::SysFreeString(detachedChild);
	});

	runner.Add("System visual preferences drive contrast motion text and focus cues", []
	{
		auto belowRange = SystemVisualPreferences{};
		belowRange.TextScalePercent = 40;
		CUI_EXPECT_EQ(100U, Application::NormalizeSystemVisualPreferences(
			belowRange).TextScalePercent);
		auto aboveRange = SystemVisualPreferences{};
		aboveRange.TextScalePercent = 400;
		CUI_EXPECT_EQ(225U, Application::NormalizeSystemVisualPreferences(
			aboveRange).TextScalePercent);
		const auto queried = Application::QuerySystemVisualPreferences();
		CUI_EXPECT_TRUE(queried.TextScalePercent >= 100U);
		CUI_EXPECT_TRUE(queried.TextScalePercent <= 225U);

		Form form(L"System preferences", POINT{ 160, 160 }, SIZE{ 320, 200 });
		CUI_EXPECT_TRUE(form.Handle != nullptr);
		if (!form.Handle) return;
		auto* editor = form.Add<TextBox>(L"Scaled", 12, 12, 140, 28);
		editor->BackColor = D2D1_COLOR_F{ 0.8f, 0.1f, 0.7f, 1.0f };
		editor->ForeColor = D2D1_COLOR_F{ 0.1f, 0.7f, 0.2f, 1.0f };
		editor->Font = new Font(L"Arial", 10.0f);
		auto* toggle = form.Add<Switch>(12, 52, 46, 24);

		SystemVisualPreferences preferences;
		preferences.HighContrast = true;
		preferences.AnimationsEnabled = false;
		preferences.KeyboardCuesAlwaysVisible = true;
		preferences.TextScalePercent = 150;
		form.ApplySystemVisualPreferences(preferences);
		CUI_EXPECT_TRUE(form.GetSystemVisualPreferences().HighContrast);
		CUI_EXPECT_FALSE(form.AreSystemAnimationsEnabled());
		CUI_EXPECT_TRUE(form.ShouldShowKeyboardFocusVisual());
		CUI_EXPECT_EQ(1.5f, form.GetTextScaleFactor());
		CUI_EXPECT_EQ(15.0f, editor->Font->FontSize);
		CUI_EXPECT_EQ(21.0f, form.GetFont()->FontSize);

		auto systemColor = [](int index)
		{
			const COLORREF color = ::GetSysColor(index);
			return D2D1_COLOR_F{
				(float)GetRValue(color) / 255.0f,
				(float)GetGValue(color) / 255.0f,
				(float)GetBValue(color) / 255.0f, 1.0f };
		};
		const auto back = editor->BackColor;
		const auto fore = editor->ForeColor;
		const auto expectedBack = systemColor(COLOR_WINDOW);
		const auto expectedFore = systemColor(COLOR_WINDOWTEXT);
		CUI_EXPECT_EQ(expectedBack.r, back.r);
		CUI_EXPECT_EQ(expectedBack.g, back.g);
		CUI_EXPECT_EQ(expectedBack.b, back.b);
		CUI_EXPECT_EQ(expectedFore.r, fore.r);
		CUI_EXPECT_EQ(expectedFore.g, fore.g);
		CUI_EXPECT_EQ(expectedFore.b, fore.b);

		CUI_EXPECT_TRUE(toggle->Invoke());
		CUI_EXPECT_TRUE(toggle->Checked);
		CUI_EXPECT_FALSE(toggle->IsAnimationRunning());

		preferences.HighContrast = false;
		preferences.AnimationsEnabled = true;
		preferences.KeyboardCuesAlwaysVisible = false;
		preferences.TextScalePercent = 100;
		form.ApplySystemVisualPreferences(preferences);
		CUI_EXPECT_EQ(10.0f, editor->Font->FontSize);
		CUI_EXPECT_EQ(0.8f, editor->BackColor.r);
		CUI_EXPECT_EQ(0.7f, editor->ForeColor.g);
		::DestroyWindow(form.Handle);
	});

	runner.Add("ObservableObject exposes and enforces source property metadata", []
	{
		MetadataObservableObject source;
		source.SetValue(L"Count", 12);
		source.SetValue(L"Name", std::wstring(L"CUI"));

		const auto properties = source.GetProperties();
		CUI_EXPECT_EQ(2ULL, properties.size());
		CUI_EXPECT_EQ(std::wstring(L"Count"), properties[0].Name);
		CUI_EXPECT_EQ(BindingValueKind::Int, properties[0].ValueKind);
		CUI_EXPECT_EQ(std::type_index(typeid(int)), properties[0].ValueType);
		CUI_EXPECT_EQ(std::wstring(L"Name"), properties[1].Name);

		BindingSourcePropertyMetadata metadata;
		CUI_EXPECT_TRUE(source.TryGetPropertyMetadata(L"Name", metadata));
		CUI_EXPECT_TRUE(metadata.CanRead);
		CUI_EXPECT_TRUE(metadata.CanWrite);
		CUI_EXPECT_TRUE(metadata.CanObserve);

		CUI_EXPECT_TRUE(source.DefineProperty(
			L"ReadOnly", std::wstring(L"initial"), true, false, true));
		CUI_EXPECT_FALSE(source.TrySetValue(L"ReadOnly", BindingValue(L"external")));
		CUI_EXPECT_EQ(std::wstring(L"initial"),
			source.GetValue<std::wstring>(L"ReadOnly"));

		int notifications = 0;
		auto connection = source.PropertyChanged().Subscribe(
			[&](const PropertyChangedEventArgs&) { ++notifications; });
		CUI_EXPECT_TRUE(source.SetCurrentValue(
			L"ReadOnly", std::wstring(L"internal")));
		CUI_EXPECT_EQ(std::wstring(L"internal"),
			source.GetValue<std::wstring>(L"ReadOnly"));
		CUI_EXPECT_EQ(1, notifications);

		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"Silent", BindingValueKind::Int,
				std::type_index(typeid(int)), true, true, false },
			BindingValue(1)));
		CUI_EXPECT_TRUE(source.SetValue(L"Silent", 2));
		CUI_EXPECT_EQ(1, notifications);

		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"Hidden", BindingValueKind::String,
				std::type_index(typeid(std::wstring)), false, true, false },
			BindingValue(L"secret")));
		BindingValue hidden;
		CUI_EXPECT_FALSE(source.TryGetValue(L"Hidden", hidden));
		CUI_EXPECT_TRUE(source.RemoveProperty(L"ReadOnly"));
		CUI_EXPECT_EQ(2, notifications);
	});

	runner.Add("ObservableObject maintains field validation state", []
	{
		MetadataObservableObject source;
		source.SetValue(L"Name", std::wstring(L"CUI"));

		int notifications = 0;
		std::wstring changedProperty;
		auto* validationChanged = source.ValidationChanged();
		CUI_EXPECT_TRUE(validationChanged != nullptr);
		auto connection = validationChanged->Subscribe(
			[&](const BindingValidationChangedEventArgs& e)
			{
				++notifications;
				changedProperty = e.PropertyName;
			});

		CUI_EXPECT_TRUE(source.SetValidationIssues(L" Name ", {
			{ L" Required ", BindingValidationSeverity::Error, L" required " },
			{ L"Required", BindingValidationSeverity::Error, L"required" },
			{ L"Please use a display name", BindingValidationSeverity::Warning, L"" },
			{ L"   ", BindingValidationSeverity::Error, L"ignored" }
		}));
		CUI_EXPECT_EQ(1, notifications);
		CUI_EXPECT_EQ(std::wstring(L"Name"), changedProperty);

		const auto issues = source.GetValidationIssues(L"Name");
		CUI_EXPECT_EQ(2ULL, issues.size());
		CUI_EXPECT_EQ(std::wstring(L"Required"), issues[0].Message);
		CUI_EXPECT_EQ(std::wstring(L"required"), issues[0].Code);
		CUI_EXPECT_EQ(BindingValidationSeverity::Warning, issues[1].Severity);
		CUI_EXPECT_TRUE(source.HasValidationIssues());
		CUI_EXPECT_TRUE(source.HasValidationErrors());
		CUI_EXPECT_TRUE(source.HasValidationErrors(L"Name"));

		CUI_EXPECT_FALSE(source.SetValidationIssues(L"Name", issues));
		CUI_EXPECT_EQ(1, notifications);
		CUI_EXPECT_TRUE(source.SetValidationError(
			L"", L"The object is incomplete", L"incomplete"));
		CUI_EXPECT_EQ(2, notifications);
		CUI_EXPECT_EQ(std::wstring(L""), changedProperty);
		CUI_EXPECT_EQ(3ULL,
			GetBindingValidationIssuesForPath(source, L"Name").size());

		CUI_EXPECT_TRUE(source.ClearValidationIssues(L"Name"));
		CUI_EXPECT_FALSE(source.HasValidationErrors(L"Name"));
		CUI_EXPECT_TRUE(source.HasValidationErrors());
		CUI_EXPECT_TRUE(source.ClearAllValidationIssues());
		CUI_EXPECT_FALSE(source.HasValidationIssues());
		CUI_EXPECT_FALSE(source.ClearAllValidationIssues());

		CUI_EXPECT_TRUE(source.SetValidationError(L"Name", L"Removed with property"));
		CUI_EXPECT_TRUE(source.RemoveProperty(L"Name"));
		CUI_EXPECT_TRUE(source.GetValidationIssues(L"Name").empty());
	});

	runner.Add("Binding publishes nested source validation to its control", []
	{
		auto profile = std::make_shared<MetadataObservableObject>();
		profile->SetValue(L"DisplayName", std::wstring(L"Alice"));
		MetadataObservableObject source;
		source.SetValue(L"Profile", BindingSourceReference(profile));

		Control target;
		Binding* binding = target.DataBindings.Add(
			L"Text", source, L"Profile.DisplayName", BindingMode::OneWay);
		CUI_EXPECT_TRUE(binding != nullptr);
		CUI_EXPECT_FALSE(binding->HasValidationIssues());
		CUI_EXPECT_FALSE(target.DataBindings.HasValidationErrors());
		CUI_EXPECT_TRUE(target.ShowValidationBorder);
		CUI_EXPECT_TRUE(target.ShowValidationToolTip);
		CUI_EXPECT_FALSE(target.HasValidationIssues());
		CUI_EXPECT_TRUE(target.GetValidationSummary().empty());
		target.AccessibleDescription = L"Display name input";

		int bindingNotifications = 0;
		int controlNotifications = 0;
		int presentationNotifications = 0;
		std::wstring changedTarget;
		std::wstring changedPresentationTarget;
		auto bindingConnection = binding->ValidationChanged().Subscribe(
			[&](const BindingValidationChangedEventArgs& e)
			{
				++bindingNotifications;
				CUI_EXPECT_EQ(std::wstring(L"Profile.DisplayName"), e.PropertyName);
			});
		auto controlConnection = target.DataBindings.ValidationChanged().Subscribe(
			[&](const BindingValidationChangedEventArgs& e)
			{
				++controlNotifications;
				changedTarget = e.PropertyName;
			});
		auto presentationConnection = target.OnValidationStateChanged.Subscribe(
			[&](const BindingValidationChangedEventArgs& e)
			{
				++presentationNotifications;
				changedPresentationTarget = e.PropertyName;
			});

		CUI_EXPECT_TRUE(profile->SetValidationError(
			L"DisplayName", L"A display name is required", L"required"));
		CUI_EXPECT_TRUE(binding->HasValidationErrors());
		CUI_EXPECT_TRUE(target.DataBindings.HasValidationErrors());
		CUI_EXPECT_EQ(1, bindingNotifications);
		CUI_EXPECT_EQ(1, controlNotifications);
		CUI_EXPECT_EQ(std::wstring(L"Text"), changedTarget);
		CUI_EXPECT_TRUE(target.HasValidationIssues());
		CUI_EXPECT_TRUE(target.HasValidationErrors());
		BindingValidationSeverity severity = BindingValidationSeverity::Info;
		CUI_EXPECT_TRUE(target.TryGetValidationSeverity(severity));
		CUI_EXPECT_EQ(BindingValidationSeverity::Error, severity);
		CUI_EXPECT_TRUE(target.ShouldShowValidationToolTip());
		CUI_EXPECT_TRUE(target.GetValidationSummary().find(
			L"A display name is required") != std::wstring::npos);
		CUI_EXPECT_TRUE(target.GetEffectiveAccessibleDescription().find(
			L"Display name input") != std::wstring::npos);
		CUI_EXPECT_TRUE(target.GetEffectiveAccessibleDescription().find(
			L"A display name is required") != std::wstring::npos);
		CUI_EXPECT_EQ(1, presentationNotifications);
		CUI_EXPECT_EQ(std::wstring(L"Text"), changedPresentationTarget);

		CUI_EXPECT_TRUE(source.SetValidationIssues(L"Profile", {
			{ L"Profile is still loading", BindingValidationSeverity::Warning, L"loading" }
		}));
		const auto results = target.DataBindings.GetValidationResults();
		CUI_EXPECT_EQ(2ULL, results.size());
		CUI_EXPECT_EQ(std::wstring(L"Text"), results[0].TargetProperty);
		CUI_EXPECT_EQ(std::wstring(L"Profile.DisplayName"), results[0].SourceProperty);
		CUI_EXPECT_EQ(2, presentationNotifications);

		profile->SetValidationError(L"Unrelated", L"Ignored by this binding");
		CUI_EXPECT_EQ(2, bindingNotifications);
		CUI_EXPECT_EQ(2, controlNotifications);
		CUI_EXPECT_EQ(2, presentationNotifications);

		auto replacement = std::make_shared<MetadataObservableObject>();
		replacement->SetValue(L"DisplayName", std::wstring(L"Bob"));
		replacement->SetValidationIssues(L"DisplayName", {
			{ L"Name should be longer", BindingValidationSeverity::Info, L"length" }
		});
		source.SetValue(L"Profile", BindingSourceReference(replacement));
		CUI_EXPECT_EQ(std::wstring(L"Bob"), target.Text);
		CUI_EXPECT_EQ(2ULL, target.DataBindings.GetValidationResults().size());
		CUI_EXPECT_EQ(0ULL, profile->ValidationChanged()->Count());
		CUI_EXPECT_EQ(1ULL, replacement->ValidationChanged()->Count());
		CUI_EXPECT_EQ(3, presentationNotifications);

		target.DataBindings.Clear();
		CUI_EXPECT_EQ(0ULL, source.ValidationChanged()->Count());
		CUI_EXPECT_EQ(0ULL, replacement->ValidationChanged()->Count());
		CUI_EXPECT_FALSE(target.DataBindings.HasValidationIssues());
		CUI_EXPECT_FALSE(target.HasValidationIssues());
		CUI_EXPECT_FALSE(target.ShouldShowValidationToolTip());
		CUI_EXPECT_EQ(4, presentationNotifications);
		CUI_EXPECT_EQ(std::wstring(L""), changedPresentationTarget);

		target.ValidationBorderThickness = -5.0f;
		CUI_EXPECT_EQ(0.0f, target.ValidationBorderThickness);
		target.ValidationBorderThickness = 99.0f;
		CUI_EXPECT_EQ(16.0f, target.ValidationBorderThickness);
		target.ValidationCornerRadius = -1.0f;
		CUI_EXPECT_EQ(0.0f, target.ValidationCornerRadius);
		target.ValidationToolTipMaxWidth = 10.0f;
		CUI_EXPECT_EQ(120.0f, target.ValidationToolTipMaxWidth);
		target.ShowValidationBorder = false;
		target.ShowValidationToolTip = false;
		CUI_EXPECT_FALSE(target.ShowValidationBorder);
		CUI_EXPECT_FALSE(target.ShowValidationToolTip);
	});

	runner.Add("OneTime binding keeps validation attached to a replaced path", []
	{
		auto first = std::make_shared<MetadataObservableObject>();
		first->SetValue(L"Name", std::wstring(L"first"));
		MetadataObservableObject source;
		source.SetValue(L"Profile", BindingSourceReference(first));
		Control target;
		Binding* binding = target.DataBindings.Add(
			L"Text", source, L"Profile.Name", BindingMode::OneTime);
		CUI_EXPECT_TRUE(binding != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"first"), target.Text);

		auto second = std::make_shared<MetadataObservableObject>();
		second->SetValue(L"Name", std::wstring(L"second"));
		second->SetValidationError(L"Name", L"second is invalid");
		source.SetValue(L"Profile", BindingSourceReference(second));

		CUI_EXPECT_EQ(std::wstring(L"first"), target.Text);
		CUI_EXPECT_TRUE(binding->HasValidationErrors());
		CUI_EXPECT_EQ(0ULL, first->ValidationChanged()->Count());
		CUI_EXPECT_EQ(1ULL, second->ValidationChanged()->Count());
	});

	runner.Add("Binding validates discoverable source capabilities", []
	{
		MetadataObservableObject source;
		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"ReadOnly", BindingValueKind::String,
				std::type_index(typeid(std::wstring)), true, false, true },
			BindingValue(L"value")));
		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"Silent", BindingValueKind::String,
				std::type_index(typeid(std::wstring)), true, true, false },
			BindingValue(L"snapshot")));
		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"Hidden", BindingValueKind::String,
				std::type_index(typeid(std::wstring)), false, true, true },
			BindingValue(L"hidden")));

		Control target;
		CUI_EXPECT_TRUE(target.DataBindings.Add(
			L"Text", source, L"ReadOnly", BindingMode::TwoWay) == nullptr);
		CUI_EXPECT_EQ(BindingError::SourceNotWritable,
			target.DataBindings.LastError());
		CUI_EXPECT_TRUE(target.DataBindings.Add(
			L"Text", source, L"Silent", BindingMode::OneWay) == nullptr);
		CUI_EXPECT_EQ(BindingError::SourceNotObservable,
			target.DataBindings.LastError());
		CUI_EXPECT_TRUE(target.DataBindings.Add(
			L"Text", source, L"Hidden", BindingMode::OneWay) == nullptr);
		CUI_EXPECT_EQ(BindingError::SourceNotReadable,
			target.DataBindings.LastError());

		Binding* oneTime = target.DataBindings.Add(
			L"Text", source, L"Silent", BindingMode::OneTime);
		CUI_EXPECT_TRUE(oneTime != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"snapshot"), target.Text);

		auto profile = std::make_shared<ObservableObject>();
		profile->SetValue(L"Name", std::wstring(L"nested"));
		MetadataObservableObject nestedSource;
		CUI_EXPECT_TRUE(nestedSource.DefineProperty(
			{ L"Profile", BindingValueKind::Object,
				std::type_index(typeid(BindingSourceReference)), true, false, false },
			BindingValue(BindingSourceReference(profile))));
		Control nestedTarget;
		CUI_EXPECT_TRUE(nestedTarget.DataBindings.Add(
			L"Text", nestedSource, L"Profile.Name", BindingMode::OneWay) == nullptr);
		CUI_EXPECT_EQ(BindingError::SourceNotObservable,
			nestedTarget.DataBindings.LastError());
	});

    runner.Add("Binding numeric conversions reject out of range values", []
    {
        BindingValue value(255);
        uint8_t byte = 0;
        CUI_EXPECT_TRUE(value.TryGet(byte));
        CUI_EXPECT_EQ(255, static_cast<int>(byte));

        CUI_EXPECT_FALSE(BindingValue(-1).TryGet(byte));
        CUI_EXPECT_FALSE(BindingValue(256).TryGet(byte));

        double finite = 0.0;
        CUI_EXPECT_FALSE(BindingValue(L"1e999").TryGetDouble(finite));
        CUI_EXPECT_FALSE(BindingValue(L"NaN").TryGetDouble(finite));

        ObservableObject source;
        source.SetValue(L"Width", (std::numeric_limits<long long>::max)());
        Control target;
        target.Width = 17;
        Binding* binding = target.DataBindings.Add(L"Width", source, L"Width");

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ(BindingError::TargetConversionFailed, binding->LastError());
        CUI_EXPECT_EQ(17, target.Width);

        source.SetValue(L"Width", 42LL);
        CUI_EXPECT_EQ(42, target.Width);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("TwoWay binding preserves source types after conversion failures", []
    {
        ObservableObject source;
        source.SetValue(L"Count", 12);
        Control target;
        Binding* binding = target.DataBindings.Add(
            L"Text", source, L"Count", BindingMode::TwoWay);

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ(std::wstring(L"12"), target.Text);

        target.Text = L"not a number";
        CUI_EXPECT_EQ(BindingError::SourceConversionFailed, binding->LastError());
        CUI_EXPECT_EQ(12, source.GetValue<int>(L"Count"));

        BindingValue stored;
        CUI_EXPECT_TRUE(source.TryGetValue(L"Count", stored));
        CUI_EXPECT_EQ(BindingValueKind::Int, stored.Kind());

        target.Text = L"27";
        CUI_EXPECT_EQ(27, source.GetValue<int>(L"Count"));
        CUI_EXPECT_TRUE(source.TryGetValue(L"Count", stored));
        CUI_EXPECT_EQ(BindingValueKind::Int, stored.Kind());
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("Binding supports owned bidirectional value converters", []
    {
        auto converter = std::make_shared<DelegateBindingValueConverter>(
            [](const BindingValue& value, BindingValue& out)
            {
                int amount = 0;
                if (!value.TryGetInt(amount)) return false;
                out = BindingValue(L"#" + std::to_wstring(amount));
                return true;
            },
            [](const BindingValue& value, BindingValue& out)
            {
                std::wstring text;
                if (!value.TryGetString(text) || text.size() < 2 || text.front() != L'#')
                    return false;
                int amount = 0;
                if (!BindingValue(text.substr(1)).TryGetInt(amount)) return false;
                out = BindingValue(amount);
                return true;
            });

        ObservableObject source;
        source.SetValue(L"Amount", 12);
        Control target;
        Binding* binding = target.DataBindings.Add(
            L"Text",
            source,
            L"Amount",
            BindingMode::TwoWay,
            DataSourceUpdateMode::OnPropertyChanged,
            converter);

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_TRUE(binding->Converter() != nullptr);
        CUI_EXPECT_EQ(std::wstring(L"#12"), target.Text);

        converter.reset();
        source.SetValue(L"Amount", 18);
        CUI_EXPECT_EQ(std::wstring(L"#18"), target.Text);

        target.Text = L"#31";
        CUI_EXPECT_EQ(31, source.GetValue<int>(L"Amount"));

        target.Text = L"invalid";
        CUI_EXPECT_EQ(BindingError::SourceConversionFailed, binding->LastError());
        CUI_EXPECT_EQ(31, source.GetValue<int>(L"Amount"));

        source.SetValue(L"Amount", 44);
        CUI_EXPECT_EQ(std::wstring(L"#44"), target.Text);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("Binding converter registry exposes built-in converters", []
    {
        const auto converters = BindingValueConverterRegistry::GetConverters();
        const auto findConverter = [&](const wchar_t* name)
        {
            return std::find_if(converters.begin(), converters.end(),
                [name](const BindingValueConverterMetadata& metadata)
                {
                    return _wcsicmp(metadata.Name.c_str(), name) == 0;
                });
        };

        const auto booleanNegation = findConverter(L"BooleanNegation");
        const auto stringIsNotEmpty = findConverter(L"StringIsNotEmpty");
        const auto stringTrim = findConverter(L"StringTrim");
        CUI_EXPECT_TRUE(booleanNegation != converters.end());
        CUI_EXPECT_TRUE(stringIsNotEmpty != converters.end());
        CUI_EXPECT_TRUE(stringTrim != converters.end());
        CUI_EXPECT_EQ(BindingValueKind::Bool, booleanNegation->SourceKind);
        CUI_EXPECT_EQ(BindingValueKind::Bool, booleanNegation->TargetKind);
        CUI_EXPECT_TRUE(booleanNegation->CanConvertBack);
        CUI_EXPECT_FALSE(stringIsNotEmpty->CanConvertBack);

        const auto metadata = BindingValueConverterRegistry::Find(L"booleannegation");
        CUI_EXPECT_TRUE(metadata.has_value());
        CUI_EXPECT_EQ(std::wstring(L"BooleanNegation"), metadata->Name);

        const auto negate = BindingValueConverterRegistry::Create(L"BOOLEANNEGATION");
        BindingValue converted;
        bool booleanValue = true;
        CUI_EXPECT_TRUE(negate != nullptr);
        CUI_EXPECT_TRUE(negate->Convert(BindingValue(true), converted));
        CUI_EXPECT_TRUE(converted.TryGetBool(booleanValue));
        CUI_EXPECT_FALSE(booleanValue);
        CUI_EXPECT_TRUE(negate->ConvertBack(BindingValue(false), converted));
        CUI_EXPECT_TRUE(converted.TryGetBool(booleanValue));
        CUI_EXPECT_TRUE(booleanValue);

        const auto trim = BindingValueConverterRegistry::Create(L"StringTrim");
        std::wstring stringValue;
        CUI_EXPECT_TRUE(trim != nullptr);
        CUI_EXPECT_TRUE(trim->Convert(BindingValue(L"  value \t"), converted));
        CUI_EXPECT_TRUE(converted.TryGetString(stringValue));
        CUI_EXPECT_EQ(std::wstring(L"value"), stringValue);
        CUI_EXPECT_TRUE(trim->ConvertBack(BindingValue(L"  source  "), converted));
        CUI_EXPECT_TRUE(converted.TryGetString(stringValue));
        CUI_EXPECT_EQ(std::wstring(L"source"), stringValue);

        CUI_EXPECT_FALSE(BindingValueConverterRegistry::Find(L"Missing").has_value());
        CUI_EXPECT_TRUE(BindingValueConverterRegistry::Create(L"Missing") == nullptr);

        for (size_t i = 1; i < converters.size(); ++i)
        {
            CUI_EXPECT_TRUE(_wcsicmp(
                converters[i - 1].Name.c_str(),
                converters[i].Name.c_str()) < 0);
        }
    });

    runner.Add("Binding converter registry supports custom factories", []
    {
        constexpr const wchar_t* converterName = L"Tests.BracketText";
        BindingValueConverterRegistry::Unregister(converterName);

        const BindingValueConverterMetadata metadata{
            converterName,
            BindingValueKind::String,
            BindingValueKind::String,
            true };
        const auto factory = []
        {
            return std::make_shared<DelegateBindingValueConverter>(
                [](const BindingValue& value, BindingValue& out)
                {
                    std::wstring text;
                    if (!value.TryGetString(text)) return false;
                    out = BindingValue(L"[" + text + L"]");
                    return true;
                },
                [](const BindingValue& value, BindingValue& out)
                {
                    std::wstring text;
                    if (!value.TryGetString(text)
                        || text.size() < 2
                        || text.front() != L'['
                        || text.back() != L']')
                    {
                        return false;
                    }
                    out = BindingValue(text.substr(1, text.size() - 2));
                    return true;
                });
        };

        CUI_EXPECT_TRUE(BindingValueConverterRegistry::Register(metadata, factory));
        CUI_EXPECT_FALSE(BindingValueConverterRegistry::Register(metadata, factory));

        const auto found = BindingValueConverterRegistry::Find(L"tests.brackettext");
        CUI_EXPECT_TRUE(found.has_value());
        CUI_EXPECT_EQ(std::wstring(converterName), found->Name);

        const auto converter = BindingValueConverterRegistry::Create(converterName);
        BindingValue converted;
        std::wstring text;
        CUI_EXPECT_TRUE(converter != nullptr);
        CUI_EXPECT_TRUE(converter->Convert(BindingValue(L"hello"), converted));
        CUI_EXPECT_TRUE(converted.TryGetString(text));
        CUI_EXPECT_EQ(std::wstring(L"[hello]"), text);
        CUI_EXPECT_TRUE(converter->ConvertBack(converted, converted));
        CUI_EXPECT_TRUE(converted.TryGetString(text));
        CUI_EXPECT_EQ(std::wstring(L"hello"), text);

        CUI_EXPECT_TRUE(BindingValueConverterRegistry::Unregister(L"TESTS.BRACKETTEXT"));
        CUI_EXPECT_FALSE(BindingValueConverterRegistry::Unregister(converterName));
        CUI_EXPECT_TRUE(BindingValueConverterRegistry::Create(converterName) == nullptr);
    });

    runner.Add("Named converter participates in metadata binding", []
    {
        ObservableObject source;
        source.SetValue(L"Enabled", true);
        Control target;
        const auto converter = BindingValueConverterRegistry::Create(L"BooleanNegation");
        Binding* binding = target.DataBindings.Add(
            L"Checked",
            source,
            L"Enabled",
            BindingMode::OneWay,
            DataSourceUpdateMode::OnPropertyChanged,
            converter);

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_FALSE(target.Checked);
        source.SetValue(L"Enabled", false);
        CUI_EXPECT_TRUE(target.Checked);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("Binding observes and rebinds dotted source paths", []
    {
        auto firstProfile = std::make_shared<ObservableObject>();
        firstProfile->SetValue(L"DisplayName", std::wstring(L"Alice"));

        ObservableObject source;
        source.SetValue(L"Profile", BindingSourceReference(firstProfile));
        Control target;
        Binding* binding = target.DataBindings.Add(
            L"Text", source, L"Profile.DisplayName");

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ(std::wstring(L"Alice"), target.Text);
        CUI_EXPECT_EQ(1ULL, source.PropertyChanged().Count());
        CUI_EXPECT_EQ(1ULL, firstProfile->PropertyChanged().Count());

        firstProfile->SetValue(L"DisplayName", std::wstring(L"Bob"));
        CUI_EXPECT_EQ(std::wstring(L"Bob"), target.Text);

        auto secondProfile = std::make_shared<ObservableObject>();
        secondProfile->SetValue(L"DisplayName", std::wstring(L"Carol"));
        source.SetValue(L"Profile", BindingSourceReference(secondProfile));

        CUI_EXPECT_EQ(std::wstring(L"Carol"), target.Text);
        CUI_EXPECT_EQ(0ULL, firstProfile->PropertyChanged().Count());
        CUI_EXPECT_EQ(1ULL, secondProfile->PropertyChanged().Count());

        firstProfile->SetValue(L"DisplayName", std::wstring(L"stale"));
        CUI_EXPECT_EQ(std::wstring(L"Carol"), target.Text);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("Binding source paths recover when intermediates appear", []
    {
        ObservableObject source;
        Control target;
        Binding* binding = target.DataBindings.Add(
            L"Text", source, L"Profile.DisplayName");

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_TRUE(binding->IsValid());
        CUI_EXPECT_EQ(BindingError::SourcePathUnresolved, binding->LastError());
        CUI_EXPECT_EQ(1ULL, source.PropertyChanged().Count());

        auto profile = std::make_shared<ObservableObject>();
        profile->SetValue(L"DisplayName", std::wstring(L"available"));
        source.SetValue(L"Profile", BindingSourceReference(profile));

        CUI_EXPECT_EQ(std::wstring(L"available"), target.Text);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
        CUI_EXPECT_EQ(1ULL, profile->PropertyChanged().Count());
    });

    runner.Add("TwoWay binding writes through dotted source paths", []
    {
        auto profile = std::make_shared<ObservableObject>();
        profile->SetValue(L"Score", 7);
        ObservableObject source;
        source.SetValue(L"Profile", BindingSourceReference(profile));

        Control target;
        Binding* binding = target.DataBindings.Add(
            L"Text", source, L"Profile.Score", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ(std::wstring(L"7"), target.Text);

        target.Text = L"19";
        CUI_EXPECT_EQ(19, profile->GetValue<int>(L"Score"));

        target.Text = L"invalid";
        CUI_EXPECT_EQ(BindingError::SourceConversionFailed, binding->LastError());
        CUI_EXPECT_EQ(19, profile->GetValue<int>(L"Score"));

        profile->SetValue(L"Score", 23);
        CUI_EXPECT_EQ(std::wstring(L"23"), target.Text);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

	runner.Add("Binding rejects malformed source property paths", []
    {
        ObservableObject source;
        Control target;
        CUI_EXPECT_EQ(nullptr, target.DataBindings.Add(
            L"Text", source, L"Profile..DisplayName"));
        CUI_EXPECT_EQ(
            BindingError::InvalidSourcePropertyPath,
            target.DataBindings.LastError());
        CUI_EXPECT_EQ(0ULL, source.PropertyChanged().Count());
	});

	runner.Add("Designer binding paths and enum names round trip", []
	{
		CUI_EXPECT_TRUE(DesignerBindingUtils::IsValidSourcePath(L"Profile.DisplayName"));
		CUI_EXPECT_TRUE(DesignerBindingUtils::IsValidSourcePath(L" Profile . DisplayName "));
		CUI_EXPECT_FALSE(DesignerBindingUtils::IsValidSourcePath(L"Profile..DisplayName"));
		CUI_EXPECT_FALSE(DesignerBindingUtils::IsValidSourcePath(L".Profile"));
		CUI_EXPECT_FALSE(DesignerBindingUtils::IsValidSourcePath(L"Profile."));

		BindingMode mode = BindingMode::OneWay;
		CUI_EXPECT_TRUE(DesignerBindingUtils::TryParseBindingMode(L"twoWAY", mode));
		CUI_EXPECT_EQ(BindingMode::TwoWay, mode);
		CUI_EXPECT_EQ(std::wstring(L"TwoWay"),
			std::wstring(DesignerBindingUtils::BindingModeName(mode)));

		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Never;
		CUI_EXPECT_TRUE(DesignerBindingUtils::TryParseUpdateMode(
			L"OnValidation", updateMode));
		CUI_EXPECT_EQ(DataSourceUpdateMode::OnValidation, updateMode);
	});

	runner.Add("Designer DataContext schema canonicalizes nested paths", []
	{
		DesignerDataContextSchema schema{
			{ L" Profile . DisplayName ", BindingValueKind::String, true, true, true },
			{ L"Profile", BindingValueKind::Object, true, false, true },
			{ L"Count", BindingValueKind::Int, true, false, true }
		};
		DesignerDataContextSchemaUtils::Canonicalize(schema);
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerDataContextSchemaUtils::Validate(schema, &error));
		CUI_EXPECT_TRUE(error.empty());
		CUI_EXPECT_EQ(std::wstring(L"Count"), schema[0].Path);
		CUI_EXPECT_EQ(std::wstring(L"Profile"), schema[1].Path);
		CUI_EXPECT_EQ(std::wstring(L"Profile.DisplayName"), schema[2].Path);

		const auto* displayName = DesignerDataContextSchemaUtils::Find(
			schema, L"profile.displayname");
		CUI_EXPECT_TRUE(displayName != nullptr);
		CUI_EXPECT_EQ(BindingValueKind::String, displayName->ValueKind);
		CUI_EXPECT_EQ(std::wstring(L"Profile.DisplayName : String  [RWO]"),
			DesignerDataContextSchemaUtils::Describe(*displayName));

		auto duplicate = schema;
		duplicate.push_back({ L"PROFILE.DISPLAYNAME", BindingValueKind::String });
		CUI_EXPECT_FALSE(DesignerDataContextSchemaUtils::Validate(duplicate, &error));
		CUI_EXPECT_TRUE(error.find(L"重复") != std::wstring::npos);

		auto invalidParent = schema;
		invalidParent[1].ValueKind = BindingValueKind::String;
		CUI_EXPECT_FALSE(DesignerDataContextSchemaUtils::Validate(invalidParent, &error));
		CUI_EXPECT_TRUE(error.find(L"Object") != std::wstring::npos);

		DesignerDataContextSchema invalidPath{
			{ L"Profile..Name", BindingValueKind::String }
		};
		CUI_EXPECT_FALSE(DesignerDataContextSchemaUtils::Validate(invalidPath, &error));
	});

	runner.Add("Designer DataContext schema imports runtime source metadata", []
	{
		auto profile = std::make_shared<MetadataObservableObject>();
		profile->SetValue(L"DisplayName", std::wstring(L"Alice"));
		CUI_EXPECT_TRUE(profile->DefineProperty(
			{ L"Id", BindingValueKind::Int,
				std::type_index(typeid(int)), true, false, false },
			BindingValue(7)));

		MetadataObservableObject source;
		source.SetValue(L"Count", 3);
		source.SetValue(L"Profile", BindingSourceReference(profile));
		CUI_EXPECT_TRUE(source.DefineProperty(
			{ L"Title", BindingValueKind::String,
				std::type_index(typeid(std::wstring)), true, false, true },
			BindingValue(L"Customer")));

		DesignerDataContextSchema schema;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerDataContextSchemaUtils::BuildFromBindingSource(
			source, schema, &error));
		CUI_EXPECT_EQ(5ULL, schema.size());
		const auto* count = DesignerDataContextSchemaUtils::Find(schema, L"Count");
		const auto* profileNode = DesignerDataContextSchemaUtils::Find(schema, L"Profile");
		const auto* displayName = DesignerDataContextSchemaUtils::Find(
			schema, L"Profile.DisplayName");
		const auto* id = DesignerDataContextSchemaUtils::Find(schema, L"Profile.Id");
		const auto* title = DesignerDataContextSchemaUtils::Find(schema, L"Title");
		CUI_EXPECT_TRUE(count != nullptr);
		CUI_EXPECT_TRUE(profileNode != nullptr);
		CUI_EXPECT_TRUE(displayName != nullptr);
		CUI_EXPECT_TRUE(id != nullptr);
		CUI_EXPECT_TRUE(title != nullptr);
		CUI_EXPECT_EQ(BindingValueKind::Object, profileNode->ValueKind);
		CUI_EXPECT_FALSE(id->CanWrite);
		CUI_EXPECT_FALSE(id->CanObserve);
		CUI_EXPECT_FALSE(title->CanWrite);

		auto cyclic = std::make_shared<ObservableObject>();
		cyclic->SetValue(L"Self", BindingSourceReference(cyclic));
		DesignerDataContextSchema cyclicSchema;
		CUI_EXPECT_TRUE(DesignerDataContextSchemaUtils::BuildFromBindingSource(
			*cyclic, cyclicSchema, &error));
		CUI_EXPECT_EQ(1ULL, cyclicSchema.size());
		CUI_EXPECT_EQ(std::wstring(L"Self"), cyclicSchema.front().Path);

		ObservableObject empty;
		CUI_EXPECT_FALSE(DesignerDataContextSchemaUtils::BuildFromBindingSource(
			empty, schema, &error));
		CUI_EXPECT_TRUE(error.find(L"没有公开") != std::wstring::npos);
	});

	runner.Add("Designer DataContext schema participates in binding validation", []
	{
		Control target;
		DesignerDataContextSchema schema{
			{ L"Profile", BindingValueKind::Object, true, false, true },
			{ L"Profile.Name", BindingValueKind::String, true, true, true },
			{ L"ReadOnly", BindingValueKind::String, true, false, true },
			{ L"Snapshot", BindingValueKind::String, true, false, false }
		};
		DesignerDataBinding binding{
			L"Profile.Name", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"StringTrim" };
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));

		schema[0].CanObserve = false;
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));
		CUI_EXPECT_TRUE(error.find(L"中间属性") != std::wstring::npos);
		schema[0].CanObserve = true;

		binding.SourceProperty = L"Missing";
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));
		CUI_EXPECT_TRUE(error.find(L"未在") != std::wstring::npos);

		binding.SourceProperty = L"Snapshot";
		binding.Converter.clear();
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));
		CUI_EXPECT_TRUE(error.find(L"通知") != std::wstring::npos);
		binding.Mode = BindingMode::OneTime;
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));

		binding.SourceProperty = L"ReadOnly";
		binding.Mode = BindingMode::TwoWay;
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &schema));
		CUI_EXPECT_TRUE(error.find(L"不可写") != std::wstring::npos);

		binding.SourceProperty = L"Profile.Name";
		binding.Mode = BindingMode::OneWay;
		binding.Converter = L"BooleanNegation";
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Checked", binding, nullptr, &error, &schema));
		CUI_EXPECT_TRUE(error.find(L"源值类型") != std::wstring::npos);

		DesignerDataContextSchema noSchema;
		binding.SourceProperty = L"Any.Free.Path";
		binding.Converter.clear();
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"Text", binding, nullptr, &error, &noSchema));
	});

	runner.Add("Controls support stable design-id lookup across nested ownership", []
	{
		Panel root(0, 0, 320, 240);
		root.DesignId = 10;
		auto containerOwner = std::make_unique<Panel>(0, 0, 200, 160);
		auto* container = containerOwner.get();
		container->DesignId = 20;
		auto buttonOwner = std::make_unique<Button>(L"Nested", 0, 0);
		auto* button = buttonOwner.get();
		button->DesignId = 30;
		container->AddOwned(std::move(buttonOwner));
		root.AddOwned(std::move(containerOwner));

		CUI_EXPECT_EQ(&root, root.FindControlByDesignId(10));
		CUI_EXPECT_EQ(container, root.FindControlByDesignId(20));
		CUI_EXPECT_EQ(button, root.FindControlByDesignId(30));
		CUI_EXPECT_EQ(nullptr, root.FindControlByDesignId(0));
		CUI_EXPECT_EQ(nullptr, root.FindControlByDesignId(999));
		const Control& constRoot = root;
		CUI_EXPECT_EQ(static_cast<const Control*>(button),
			constRoot.FindControlByDesignId(30));
	});

	runner.Add("Designer document graph centralizes identity parent and order resolution", []
	{
		DesignerModel::DesignDocument document;
		document.NextStableId = 40;
		DesignerModel::DesignNode lateChild;
		lateChild.Id = 30;
		lateChild.ParentId = 10;
		lateChild.ParentRef = L"container";
		lateChild.Name = L"lateChild";
		lateChild.Type = UIClass::UI_Button;
		lateChild.Order = 2;
		document.Nodes.push_back(std::move(lateChild));
		DesignerModel::DesignNode parent;
		parent.Id = 10;
		parent.Name = L"container";
		parent.Type = UIClass::UI_Panel;
		parent.Order = 0;
		document.Nodes.push_back(std::move(parent));
		DesignerModel::DesignNode earlyChild;
		earlyChild.Id = 20;
		earlyChild.ParentId = 10;
		earlyChild.ParentRef = L"container";
		earlyChild.Name = L"earlyChild";
		earlyChild.Type = UIClass::UI_Button;
		earlyChild.Order = 1;
		document.Nodes.push_back(std::move(earlyChild));

		DesignerModel::DesignDocumentGraph graph;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentGraph::Build(
			document, graph, &error));
		CUI_EXPECT_EQ(30, graph.MaxStableId());
		CUI_EXPECT_EQ(1ULL, graph.Roots().size());
		CUI_EXPECT_EQ(1ULL, graph.Roots()[0]);
		const auto children = graph.ChildrenOf(L"container");
		CUI_EXPECT_EQ(2ULL, children.size());
		CUI_EXPECT_EQ(2ULL, children[0]);
		CUI_EXPECT_EQ(0ULL, children[1]);
		CUI_EXPECT_EQ(0ULL, graph.FindById(30)->SourceIndex);
		CUI_EXPECT_EQ(2ULL,
			graph.FindByName(L"earlyChild")->SourceIndex);
		CUI_EXPECT_EQ(std::wstring(L"container"),
			graph.Nodes()[0].ParentKey);

		auto missingParentId = document;
		missingParentId.Nodes.front().ParentId = 0;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentGraph::Build(
			missingParentId, graph, &error));
		auto negativeParentId = document;
		negativeParentId.Nodes[1].ParentId = -1;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentGraph::Build(
			negativeParentId, graph, &error));
	});

	runner.Add("Designer materialization pool owns rollback and transfers controls", []
	{
		struct TrackedControl final : Control
		{
			explicit TrackedControl(int& destroyed) : Destroyed(&destroyed) {}
			~TrackedControl() override { ++*Destroyed; }
			int* Destroyed;
		};

		DesignerModel::DesignDocument document;
		for (int id = 1; id <= 3; ++id)
		{
			DesignerModel::DesignNode node;
			node.Id = id;
			node.Name = L"control" + std::to_wstring(id);
			node.Type = UIClass::UI_Button;
			document.Nodes.push_back(std::move(node));
		}
		document.NextStableId = 4;
		DesignerModel::DesignDocumentGraph graph;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentGraph::Build(
			document, graph, &error));

		int destroyed = 0;
		{
			DesignerModel::DesignDocumentControlPool pool;
			CUI_EXPECT_TRUE(DesignerModel::DesignDocumentControlPool::Build(
				document, graph,
				[&](const DesignerModel::DesignNode&)
				{ return std::make_unique<TrackedControl>(destroyed); },
				pool, &error));
			CUI_EXPECT_EQ(3ULL, pool.PendingCount());
			CUI_EXPECT_EQ(2, pool.FindByName(L"control2")->DesignId);
			CUI_EXPECT_EQ(pool.FindById(2), pool.FindByName(L"control2"));

			Control runtimeRoot;
			auto attached = pool.TakeByName(L"control2");
			CUI_EXPECT_TRUE(attached != nullptr);
			runtimeRoot.AddOwned(std::move(attached));
			CUI_EXPECT_EQ(2ULL, pool.PendingCount());
			CUI_EXPECT_EQ(runtimeRoot.GetChild(0),
				runtimeRoot.FindControlByDesignId(2));
		}
		CUI_EXPECT_EQ(3, destroyed);

		DesignerModel::DesignDocumentControlPool rejected;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentControlPool::Build(
			document, graph,
			[](const DesignerModel::DesignNode&)
			{ return std::unique_ptr<Control>{}; },
			rejected, &error));
		CUI_EXPECT_EQ(0ULL, rejected.PendingCount());
	});

	runner.Add("Designer XML v5 persists code-behind and upgrades older versions", []
	{
		DesignerModel::DesignDocument document;
		document.Form.Name = L"SchemaForm";
		document.CodeBehind.ClassName = L"Acme::Views::SchemaWindow";
		document.CodeBehind.RelativeBasePath = L"generated/SchemaWindow";
		document.DataContextSchema = {
			{ L"Profile", BindingValueKind::Object, true, false, true },
			{ L"Profile.DisplayName", BindingValueKind::String, true, true, true }
		};
		DesignerDataContextSchemaUtils::Canonicalize(document.DataContextSchema);
		document.StyleSheet.Resources.push_back({
			L"Accent", { DesignerStyleValueKind::Color, L"#FF336699" } });
		DesignerStyleRule styleRule;
		styleRule.HasType = true;
		styleRule.Type = UIClass::UI_TextBox;
		styleRule.Classes = { L"primary" };
		styleRule.RequiredStates = ControlStyleState::Focused;
		styleRule.Setters = {
			{ L"FocusedColor", true, L"Accent", {} },
			{ L"CornerRadius", false, L"", { DesignerStyleValueKind::Float, L"6" } }
		};
		document.StyleSheet.Rules.push_back(std::move(styleRule));
		DesignerStyleSheetUtils::Canonicalize(document.StyleSheet);
		DesignerModel::DesignNode node;
		node.Id = document.AllocateNodeId();
		node.Name = L"nameInput";
		node.Type = UIClass::UI_TextBox;
		node.Props["showValidationBorder"] = false;
		node.Props["showValidationToolTip"] = true;
		node.Props["validationBorderThickness"] = 3.5;
		node.Props["validationCornerRadius"] = 6.0;
		node.Props["validationToolTipMaxWidth"] = 420.0;
		node.Props["accessibleDescription"] = "Display name input";
		node.Props["styleId"] = "primaryInput";
		DesignerModel::DesignValue styleClasses = DesignerModel::DesignValue::array();
		styleClasses.push_back("input");
		styleClasses.push_back("primary");
		node.Props["styleClasses"] = std::move(styleClasses);
		DesignerModel::DesignValue metadataProperties =
			DesignerModel::DesignValue::object();
		metadataProperties["Round"] = DesignerModel::DesignValue{
			{ "kind", "Float" },
			{ "value", "9.25" }
		};
		node.Props["metadata"] = std::move(metadataProperties);
		document.Nodes.push_back(std::move(node));
		DesignerModel::DesignNode child;
		child.Id = document.AllocateNodeId();
		child.ParentId = document.Nodes.front().Id;
		child.ParentRef = document.Nodes.front().Name;
		child.Name = L"saveButton";
		child.Type = UIClass::UI_Button;
		document.Nodes.push_back(std::move(child));
		// Persist the high-water mark independently of the current maximum ID.
		document.NextStableId = 17;

		const auto xml = DesignerModel::DesignDocumentSerializer::ToXml(document);
		CUI_EXPECT_TRUE(xml.find("version=\"5\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("nextId=\"17\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find(
			"<codeBehind class=\"Acme::Views::SchemaWindow\" relativeBasePath=\"generated/SchemaWindow\"")
			!= std::string::npos);
		CUI_EXPECT_TRUE(xml.find("id=\"1\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("parentId=\"1\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("<dataContext>") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("<styleSheet>") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("requiredStates=\"Focused\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("resource=\"Accent\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("path=\"Profile.DisplayName\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("styleId") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("styleClasses") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("name=\"metadata\"") != std::string::npos);
		CUI_EXPECT_TRUE(xml.find("name=\"Round\"") != std::string::npos);

		DesignerModel::DesignDocument loaded;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			xml, loaded, &error));
		CUI_EXPECT_EQ(DesignerModel::DesignDocument::CurrentSchemaVersion,
			loaded.SchemaVersion);
		CUI_EXPECT_EQ(document, loaded);

		const std::string versionOne =
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
			"<designDocument schema=\"cui.designer\" version=\"1\">"
			"<controls></controls></designDocument>";
		DesignerModel::DesignDocument upgraded;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			versionOne, upgraded, &error));
		CUI_EXPECT_EQ(DesignerModel::DesignDocument::CurrentSchemaVersion,
			upgraded.SchemaVersion);
		CUI_EXPECT_TRUE(upgraded.DataContextSchema.empty());
		CUI_EXPECT_TRUE(upgraded.StyleSheet.Empty());
		CUI_EXPECT_TRUE(upgraded.CodeBehind.Empty());

		const std::string versionTwo =
			"<designDocument schema=\"cui.designer\" version=\"2\">"
			"<dataContext><property path=\"Name\" kind=\"String\" "
			"read=\"true\" write=\"true\" observe=\"true\"/></dataContext>"
			"<controls></controls></designDocument>";
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			versionTwo, upgraded, &error));
		CUI_EXPECT_EQ(1ULL, upgraded.DataContextSchema.size());
		CUI_EXPECT_TRUE(upgraded.StyleSheet.Empty());

		const std::string versionThree =
			"<designDocument schema=\"cui.designer\" version=\"3\">"
			"<controls>"
			"<control name=\"legacyPanel\" type=\"Panel\" order=\"0\"><props/></control>"
			"<control name=\"legacyButton\" type=\"Button\" parent=\"legacyPanel\" order=\"0\"><props/></control>"
			"</controls></designDocument>";
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			versionThree, upgraded, &error));
		CUI_EXPECT_EQ(2ULL, upgraded.Nodes.size());
		CUI_EXPECT_EQ(1, upgraded.Nodes[0].Id);
		CUI_EXPECT_EQ(2, upgraded.Nodes[1].Id);
		CUI_EXPECT_EQ(upgraded.Nodes[0].Id, upgraded.Nodes[1].ParentId);
		CUI_EXPECT_EQ(3, upgraded.NextStableId);

		const std::string unsupported =
			"<designDocument schema=\"cui.designer\" version=\"6\">"
			"<controls></controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			unsupported, upgraded, &error));

		const std::string duplicateId =
			"<designDocument schema=\"cui.designer\" version=\"4\" nextId=\"3\">"
			"<controls>"
			"<control id=\"1\" name=\"a\" type=\"Panel\" order=\"0\"><props/></control>"
			"<control id=\"1\" name=\"b\" type=\"Button\" order=\"1\"><props/></control>"
			"</controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			duplicateId, upgraded, &error));
		const std::string danglingParent =
			"<designDocument schema=\"cui.designer\" version=\"4\" nextId=\"3\">"
			"<controls>"
			"<control id=\"1\" parentId=\"2\" name=\"a\" type=\"Button\" order=\"0\"><props/></control>"
			"</controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			danglingParent, upgraded, &error));
		const std::string parentCycle =
			"<designDocument schema=\"cui.designer\" version=\"4\" nextId=\"3\">"
			"<controls>"
			"<control id=\"1\" parentId=\"2\" name=\"a\" type=\"Panel\" order=\"0\"><props/></control>"
			"<control id=\"2\" parentId=\"1\" name=\"b\" type=\"Panel\" order=\"0\"><props/></control>"
			"</controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			parentCycle, upgraded, &error));
		const std::string regressedHighWater =
			"<designDocument schema=\"cui.designer\" version=\"4\" nextId=\"1\">"
			"<controls>"
			"<control id=\"1\" name=\"a\" type=\"Button\" order=\"0\"><props/></control>"
			"</controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			regressedHighWater, upgraded, &error));

		const std::string absoluteCodeBehind =
			"<designDocument schema=\"cui.designer\" version=\"5\" nextId=\"1\">"
			"<codeBehind class=\"WindowCode\" relativeBasePath=\"C:/outside/WindowCode\"/>"
			"<controls></controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			absoluteCodeBehind, upgraded, &error));
		const std::string missingCodeClass =
			"<designDocument schema=\"cui.designer\" version=\"5\" nextId=\"1\">"
			"<codeBehind relativeBasePath=\"WindowCode\"/>"
			"<controls></controls></designDocument>";
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			missingCodeClass, upgraded, &error));
		DesignerModel::DesignCodeBehindModel invalidClass;
		invalidClass.ClassName = L"bad::class";
		CUI_EXPECT_FALSE(invalidClass.Validate(&error));
		std::wstring normalizedClass;
		CUI_EXPECT_TRUE(
			DesignerModel::DesignCodeBehindModel::TryNormalizeClassName(
				L"Acme.Views.MainWindow", normalizedClass, &error));
		CUI_EXPECT_EQ(std::wstring(L"Acme::Views::MainWindow"), normalizedClass);
		CUI_EXPECT_FALSE(
			DesignerModel::DesignCodeBehindModel::TryNormalizeClassName(
				L"Acme::::MainWindow", normalizedClass, &error));
	});

	runner.Add("Designer event catalog owns signatures names and legacy migration", []
	{
		auto click = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_Button, L"OnMouseClick");
		CUI_EXPECT_TRUE(click.has_value());
		CUI_EXPECT_EQ(std::string("OnMouseClick"), click->EventField);
		CUI_EXPECT_EQ(std::string("Control* sender, MouseEventArgs e"),
			click->ParameterList);
		CUI_EXPECT_TRUE(click->MatchesEventMember(&Control::OnMouseClick));
		CUI_EXPECT_FALSE(click->MatchesEventMember(&Control::OnMouseMove));
		CUI_EXPECT_TRUE(click->IsDefault);
		CUI_EXPECT_EQ(DesignerEventCategory::Mouse, click->Category);
		auto checkDefault = DesignerEventCatalog::GetDefaultControlEvent(
			UIClass::UI_CheckBox);
		CUI_EXPECT_TRUE(checkDefault.has_value());
		CUI_EXPECT_EQ(std::wstring(L"OnChecked"), checkDefault->Name);
		CUI_EXPECT_EQ(DesignerEventCategory::Value, checkDefault->Category);
		auto listDefault = DesignerEventCatalog::GetDefaultControlEvent(
			UIClass::UI_ListView);
		CUI_EXPECT_TRUE(listDefault.has_value());
		CUI_EXPECT_EQ(std::wstring(L"OnItemDoubleClick"), listDefault->Name);
		auto formClose = DesignerEventCatalog::FindFormEvent(L"OnClose");
		CUI_EXPECT_TRUE(formClose.has_value());
		CUI_EXPECT_EQ(std::string("OnClosing"), formClose->EventField);
		CUI_EXPECT_EQ(std::string("Form* sender, bool& cancel"),
			formClose->ParameterList);
		CUI_EXPECT_TRUE(formClose->MatchesEventMember(&Form::OnClosing));
		auto formDefault = DesignerEventCatalog::GetDefaultFormEvent();
		CUI_EXPECT_TRUE(formDefault.has_value());
		CUI_EXPECT_EQ(std::wstring(L"OnShown"), formDefault->Name);
		CUI_EXPECT_TRUE(formDefault->MatchesEventMember(&Form::OnShown));
		CUI_EXPECT_EQ(DesignerEventCategory::Lifecycle, formDefault->Category);
		Form shownProbe(L"shown probe", POINT{ 10, 10 }, SIZE{ 80, 60 });
		int shownCount = 0;
		auto shownConnection = shownProbe.OnShown.Subscribe(
			[&shownCount](Form*) { ++shownCount; });
		shownProbe.Visible = true;
		shownProbe.Show();
		shownProbe.Visible = false;
		CUI_EXPECT_TRUE(shownConnection.Connected());
		CUI_EXPECT_EQ(1, shownCount);
		for (int value = static_cast<int>(UIClass::UI_Base);
			value <= static_cast<int>(UIClass::UI_CUSTOM); ++value)
		{
			const auto events = DesignerEventCatalog::GetControlEvents(
				static_cast<UIClass>(value));
			CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(std::count_if(
				events.begin(), events.end(), [](const auto& event)
				{ return event.IsDefault; })));
		}
		auto addingRow = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_GridView, L"OnUserAddingRow");
		CUI_EXPECT_TRUE(addingRow.has_value());
		auto gridCombo = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_GridView, L"OnGridViewComboBoxSelectionChanged");
		CUI_EXPECT_TRUE(gridCombo.has_value());
		CUI_EXPECT_EQ(std::string(
			"GridView* sender, int c, int r, int selectedIndex, std::wstring selectedText"),
			gridCombo->ParameterList);
		auto chartPoint = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_ChartView, L"OnPointClick");
		CUI_EXPECT_TRUE(chartPoint.has_value());
		CUI_EXPECT_EQ(std::string(
			"ChartView* sender, int seriesIndex, int pointIndex"),
			chartPoint->ParameterList);
		auto filterQuery = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_FilterBar, L"OnQueryChanged");
		CUI_EXPECT_TRUE(filterQuery.has_value());
		CUI_EXPECT_EQ(std::string(
			"FilterBar* sender, const std::wstring& query"),
			filterQuery->ParameterList);
		auto browserStarting = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_WebBrowser, L"OnNavigationStarting");
		CUI_EXPECT_TRUE(browserStarting.has_value());
		auto mediaState = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_MediaPlayer, L"OnStateChanged");
		CUI_EXPECT_TRUE(mediaState.has_value());
		auto formTheme = DesignerEventCatalog::FindFormEvent(L"OnThemeChanged");
		CUI_EXPECT_TRUE(formTheme.has_value());
		auto dropFile = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_Button, L"OnDropFile");
		CUI_EXPECT_TRUE(dropFile.has_value());
		CUI_EXPECT_EQ(std::string(
			"Control* sender, std::vector<std::wstring> files"),
			dropFile->ParameterList);
		auto formDropFile = DesignerEventCatalog::FindFormEvent(L"OnDropFile");
		CUI_EXPECT_TRUE(formDropFile.has_value());
		CUI_EXPECT_EQ(std::string(
			"Form* sender, std::vector<std::wstring> files"),
			formDropFile->ParameterList);
		auto propertyChanged = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_Button, L"OnPropertyValueChanged");
		CUI_EXPECT_TRUE(propertyChanged.has_value());
		CUI_EXPECT_EQ(std::string(
			"Control* sender, const ControlPropertyChangedEventArgs& e"),
			propertyChanged->ParameterList);
		auto validationChanged = DesignerEventCatalog::FindControlEvent(
			UIClass::UI_Button, L"OnValidationStateChanged");
		CUI_EXPECT_TRUE(validationChanged.has_value());
		CUI_EXPECT_EQ(std::string(
			"const BindingValidationChangedEventArgs& e"),
			validationChanged->ParameterList);
		CUI_EXPECT_EQ(DesignerEventCategory::Diagnostics,
			validationChanged->Category);
		GridView eventGrid(0, 0, 200, 100);
		bool addingRowObserved = false;
		auto addingRowConnection = eventGrid.OnUserAddingRow.Subscribe(
			[&addingRowObserved](GridView*, bool& cancel)
			{
				addingRowObserved = true;
				cancel = true;
			});
		bool cancelAddingRow = false;
		eventGrid.OnUserAddingRow.Invoke(&eventGrid, cancelAddingRow);
		CUI_EXPECT_TRUE(addingRowConnection.Connected());
		CUI_EXPECT_TRUE(addingRowObserved && cancelAddingRow);
		CUI_EXPECT_EQ(std::wstring(L"saveButton_OnMouseClick"),
			DesignerEventCatalog::MakeDefaultHandlerName(
				L"saveButton", L"OnMouseClick"));
		CUI_EXPECT_EQ(std::wstring(L"saveButton_OnMouseClick"),
			DesignerEventCatalog::ResolveHandlerName(
				L"1", L"saveButton", L"OnMouseClick"));
		CUI_EXPECT_EQ(std::wstring(L"HandleSave"),
			DesignerEventCatalog::ResolveHandlerName(
				L"HandleSave", L"saveButton", L"OnMouseClick"));
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerEventCatalog::ValidateHandlerName(L"HandleSave", &error));
		CUI_EXPECT_FALSE(DesignerEventCatalog::ValidateHandlerName(L"bad::name", &error));
		CUI_EXPECT_FALSE(DesignerEventCatalog::ValidateHandlerName(L"class", &error));
		CUI_EXPECT_FALSE(DesignerEventCatalog::ValidateHandlerName(L"__reserved", &error));
	});

	runner.Add("Designer event index validates shares and renames handler references", []
	{
		DesignerModel::DesignDocument document;
		document.Form.Name = L"EventForm";
		document.Form.EventHandlers[L"OnCommand"] = L"HandleCommand";

		DesignerModel::DesignNode save;
		save.Id = 10;
		save.Name = L"saveButton";
		save.Type = UIClass::UI_Button;
		save.Events["OnMouseClick"] = true;
		save.Events["OnMouseDoubleClick"] = "HandleSharedMouse";
		document.Nodes.push_back(save);

		DesignerModel::DesignNode cancel;
		cancel.Id = 11;
		cancel.Name = L"cancelButton";
		cancel.Type = UIClass::UI_Button;
		cancel.Order = 1;
		cancel.Events["OnMouseClick"] = "HandleSharedMouse";
		document.Nodes.push_back(cancel);
		document.NextStableId = 12;

		DesignerModel::DesignDocumentEventIndex index;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentEventIndex::Build(
			document, index, &error));
		CUI_EXPECT_EQ(4ULL, index.References().size());
		CUI_EXPECT_EQ(3ULL, index.Handlers().size());
		const auto* shared = index.FindHandler(L"HandleSharedMouse");
		CUI_EXPECT_TRUE(shared != nullptr);
		CUI_EXPECT_EQ(2ULL, shared->ReferenceIndices.size());
		const auto* conventional = index.FindHandler(L"saveButton_OnMouseClick");
		CUI_EXPECT_TRUE(conventional != nullptr);
		CUI_EXPECT_TRUE(index.References()[
			conventional->ReferenceIndices.front()].UsedConventionalName);

		size_t renamed = 0;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentEventIndex::RenameHandler(
			document, L"HandleSharedMouse", L"HandleMouse", &renamed, &error));
		CUI_EXPECT_EQ(2ULL, renamed);
		CUI_EXPECT_EQ(std::string("HandleMouse"),
			document.Nodes[0].Events["OnMouseDoubleClick"].get<std::string>());
		CUI_EXPECT_EQ(std::string("HandleMouse"),
			document.Nodes[1].Events["OnMouseClick"].get<std::string>());

		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentEventIndex::RenameHandler(
			document, L"saveButton_OnMouseClick", L"HandleMouse", &renamed, &error));
		CUI_EXPECT_EQ(1ULL, renamed);
		CUI_EXPECT_EQ(std::string("HandleMouse"),
			document.Nodes[0].Events["OnMouseClick"].get<std::string>());
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentEventIndex::Build(
			document, index, &error));
		CUI_EXPECT_EQ(2ULL, index.Handlers().size());
		CUI_EXPECT_EQ(3ULL,
			index.FindHandler(L"HandleMouse")->ReferenceIndices.size());

		const auto beforeConflict = document;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentEventIndex::RenameHandler(
			document, L"HandleCommand", L"HandleMouse", &renamed, &error));
		CUI_EXPECT_TRUE(document == beforeConflict);
		CUI_EXPECT_TRUE(!error.empty());

		DesignerModel::DesignDocument sameTypeDifferentNames;
		sameTypeDifferentNames.Form.Name = L"SharedSignatureForm";
		sameTypeDifferentNames.Form.EventHandlers[L"OnTextChanged"] =
			L"HandleStringPair";
		sameTypeDifferentNames.Form.EventHandlers[L"OnThemeChanged"] =
			L"HandleStringPair";
		DesignerModel::DesignDocumentEventIndex sharedSignatureIndex;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentEventIndex::Build(
			sameTypeDifferentNames, sharedSignatureIndex, &error));
		const auto* sharedStringPair =
			sharedSignatureIndex.FindHandler(L"HandleStringPair");
		CUI_EXPECT_TRUE(sharedStringPair != nullptr);
		CUI_EXPECT_EQ(2ULL, sharedStringPair->ReferenceIndices.size());

		auto invalid = document;
		invalid.Nodes[0].Events["UnknownEvent"] = "HandleUnknown";
		const auto previousReferenceCount = index.References().size();
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentEventIndex::Build(
			invalid, index, &error));
		CUI_EXPECT_EQ(previousReferenceCount, index.References().size());
	});

	runner.Add("Designer event generation preserves user code across regeneration", []
	{
		namespace fs = std::filesystem;
		const fs::path outputDir = fs::temp_directory_path()
			/ (L"cui-event-codegen-" + std::to_wstring(GetCurrentProcessId())
				+ L"-" + std::to_wstring(GetTickCount64()));
		fs::create_directories(outputDir);
		const auto headerPath = outputDir / L"PersistedEventForm.h";
		const auto cppPath = outputDir / L"PersistedEventForm.cpp";
		auto readText = [](const fs::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::string(
				std::istreambuf_iterator<char>(stream),
				std::istreambuf_iterator<char>());
		};
		auto countText = [](const std::string& text, const std::string& needle)
		{
			size_t count = 0;
			for (size_t position = 0;
				(position = text.find(needle, position)) != std::string::npos;
				position += needle.size()) ++count;
			return count;
		};

		Button button(L"Save", 0, 0);
		auto designButton = std::make_shared<DesignerControl>(
			&button, L"saveButton", UIClass::UI_Button, nullptr, 42);
		designButton->EventHandlers[L"OnMouseClick"] = L"HandleSave";
		designButton->EventHandlers[L"OnMouseDoubleClick"] = L"HandleSave";
		CodeGenInput input;
		input.FormName = L"PersistedEventForm";
		input.Controls = { designButton };
		input.FormEventHandlers[L"OnCommand"] = L"HandleCommand";
		input.FormEventHandlers[L"OnClose"] = L"HandleClose";
		input.FormEventHandlers[L"OnTextChanged"] = L"HandleFormValueChange";
		input.FormEventHandlers[L"OnThemeChanged"] = L"HandleFormValueChange";
		input.FormEventHandlers[L"OnShown"] = L"HandleShown";
		DesignerStyleRule generatedStyleRule;
		generatedStyleRule.HasType = true;
		generatedStyleRule.Type = UIClass::UI_Button;
		generatedStyleRule.Setters.push_back({
			L"Round", false, {}, { DesignerStyleValueKind::Float, L"6" } });
		input.StyleSheet.Rules.push_back(std::move(generatedStyleRule));

		CodeGenerator generator(L"PersistedEventForm", input);
		CUI_EXPECT_TRUE(generator.GenerateFiles(
			headerPath.wstring(), cppPath.wstring()));
		const auto generatedHeader = readText(outputDir / L"PersistedEventForm.g.h");
		const auto generatedCpp = readText(outputDir / L"PersistedEventForm.g.cpp");
		const auto generatedHandlerInclude = readText(
			outputDir / L"PersistedEventForm.handlers.g.inc");
		const auto userHeader = readText(headerPath);
		auto userCpp = readText(cppPath);
		CUI_EXPECT_TRUE(generatedHeader.find(
			"class PersistedEventFormGenerated : public Form") != std::string::npos);
		CUI_EXPECT_TRUE(generatedHeader.find("Button* saveButton = nullptr;")
			< generatedHeader.find("std::vector<EventConnection> _generatedEventConnections;"));
		CUI_EXPECT_TRUE(generatedHeader.find(
			"static constexpr int saveButton = 42;") != std::string::npos);
		CUI_EXPECT_TRUE(generatedHeader.find(
			"Button* GetSaveButton() noexcept { return saveButton; }")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedHeader.find(
			"const Button* GetSaveButton() const noexcept { return saveButton; }")
			!= std::string::npos);
		CUI_EXPECT_EQ(1ULL, countText(generatedHeader,
			"virtual void HandleSave(Control* sender, MouseEventArgs e);"));
		CUI_EXPECT_EQ(2ULL, countText(generatedCpp,
			"std::bind_front(&PersistedEventFormGenerated::HandleSave, this)"));
		CUI_EXPECT_EQ(1ULL, countText(generatedHeader,
			"virtual void HandleFormValueChange("));
		CUI_EXPECT_EQ(2ULL, countText(generatedCpp,
			"std::bind_front(&PersistedEventFormGenerated::HandleFormValueChange, this)"));
		CUI_EXPECT_TRUE(generatedHeader.find(
			"virtual void HandleShown(Form* sender);") != std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find(
			"this->OnShown.Subscribe(std::bind_front(&PersistedEventFormGenerated::HandleShown, this))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find("saveButton->DesignId = 42;")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find(
			"saveButton->SetStyleSheet(__styleSheet, true);")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find(".Subscribe(std::bind_front(")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find(
			"this->OnClosing.Subscribe(std::bind_front(&PersistedEventFormGenerated::HandleClose")
			!= std::string::npos);
		CUI_EXPECT_TRUE(generatedCpp.find(" += std::bind_front(")
			== std::string::npos);
		CUI_EXPECT_TRUE(userHeader.find("<cui-designer-user-header>")
			!= std::string::npos);
		CUI_EXPECT_TRUE(userCpp.find("<cui-designer-user-source>")
			!= std::string::npos);
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::HandleSave("));

		// Sanitized names must remain globally unique even when one source name
		// already looks like another source name's numeric fallback.
		Button collisionOne(L"One", 0, 0);
		Button collisionTwo(L"Two", 0, 0);
		Button collisionThree(L"Three", 0, 0);
		CodeGenInput collisionInput;
		collisionInput.Controls = {
			std::make_shared<DesignerControl>(
				&collisionOne, L"button", UIClass::UI_Button),
			std::make_shared<DesignerControl>(
				&collisionTwo, L"Button", UIClass::UI_Button),
			std::make_shared<DesignerControl>(
				&collisionThree, L"button2", UIClass::UI_Button),
		};
		const auto collisionHeader = CodeGenerator(
			L"CollisionForm", collisionInput).GenerateHeader();
		CUI_EXPECT_EQ(1ULL, countText(collisionHeader, "Button* button = nullptr;"));
		CUI_EXPECT_EQ(1ULL, countText(collisionHeader, "Button* button2 = nullptr;"));
		CUI_EXPECT_EQ(1ULL, countText(collisionHeader, "Button* button22 = nullptr;"));
		CUI_EXPECT_TRUE(collisionHeader.find("GetButton22() noexcept")
			!= std::string::npos);

		CodeGenerator mismatchedClassGenerator(L"RenamedEventForm", input);
		CUI_EXPECT_FALSE(mismatchedClassGenerator.GenerateFiles(
			headerPath.wstring(), cppPath.wstring()));
		CUI_EXPECT_TRUE(!mismatchedClassGenerator.GetLastError().empty());
		CUI_EXPECT_EQ(generatedHeader,
			readText(outputDir / L"PersistedEventForm.g.h"));
		CUI_EXPECT_EQ(generatedCpp,
			readText(outputDir / L"PersistedEventForm.g.cpp"));
		CUI_EXPECT_EQ(generatedHandlerInclude,
			readText(outputDir / L"PersistedEventForm.handlers.g.inc"));
		CUI_EXPECT_EQ(userHeader, readText(headerPath));
		CUI_EXPECT_EQ(userCpp, readText(cppPath));

		// Lock the third batch target against rename. The first two generated
		// files will already have committed and must be restored byte-for-byte.
		const auto handlerIncludePath =
			outputDir / L"PersistedEventForm.handlers.g.inc";
		const auto generatedHeaderPath =
			outputDir / L"PersistedEventForm.g.h";
		CUI_EXPECT_TRUE(fs::remove(generatedHeaderPath));
		const HANDLE lockedHandlerInclude = CreateFileW(
			handlerIncludePath.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		CUI_EXPECT_TRUE(lockedHandlerInclude != INVALID_HANDLE_VALUE);
		if (lockedHandlerInclude != INVALID_HANDLE_VALUE)
		{
			designButton->EventHandlers[L"OnMouseMove"] =
				L"HandleBatchFailure";
			CodeGenerator rejectedBatch(L"PersistedEventForm", input);
			CUI_EXPECT_FALSE(rejectedBatch.GenerateFiles(
				headerPath.wstring(), cppPath.wstring()));
			CUI_EXPECT_TRUE(!rejectedBatch.GetLastError().empty());
			CloseHandle(lockedHandlerInclude);
			designButton->EventHandlers.erase(L"OnMouseMove");

			CUI_EXPECT_FALSE(fs::exists(generatedHeaderPath));
			CUI_EXPECT_EQ(generatedCpp,
				readText(outputDir / L"PersistedEventForm.g.cpp"));
			CUI_EXPECT_EQ(generatedHandlerInclude,
				readText(handlerIncludePath));
			CUI_EXPECT_EQ(userHeader, readText(headerPath));
			CUI_EXPECT_EQ(userCpp, readText(cppPath));
			CUI_EXPECT_TRUE(DesignerModel::AtomicFile::Write(
				generatedHeaderPath.wstring(), generatedHeader));

			bool foundBatchArtifact = false;
			for (const auto& item : fs::directory_iterator(outputDir))
				if (item.path().filename().wstring().find(L".~cui-batch-")
					!= std::wstring::npos)
					foundBatchArtifact = true;
			CUI_EXPECT_FALSE(foundBatchArtifact);
		}

		{
			std::ofstream append(cppPath, std::ios::binary | std::ios::app);
			append
				<< "\n// USER_SENTINEL: must survive regeneration.\n"
				<< "// PersistedEventForm::HandleComment is only a comment.\n"
				<< "static constexpr auto USER_HANDLER_TEXT = "
					"\"PersistedEventForm::HandleString\";\n"
				<< "static constexpr auto USER_HANDLER_RAW = "
					"R\"(PersistedEventForm::HandleRaw)\";\n";
		}
		// "Handle" is also a strict prefix of the existing HandleSave body.
		designButton->EventHandlers[L"OnMouseDown"] = L"Handle";
		designButton->EventHandlers[L"OnMouseUp"] = L"HandleComment";
		designButton->EventHandlers[L"OnMouseEnter"] = L"HandleString";
		designButton->EventHandlers[L"OnMouseLeave"] = L"HandleRaw";
		// Same Event function type, different display parameter names. Removing
		// the first route must not reject the retained user declaration/body.
		input.FormEventHandlers.erase(L"OnTextChanged");
		CodeGenerator regenerated(L"PersistedEventForm", input);
		CUI_EXPECT_TRUE(regenerated.GenerateFiles(
			headerPath.wstring(), cppPath.wstring()));
		userCpp = readText(cppPath);
		CUI_EXPECT_TRUE(userCpp.find("USER_SENTINEL") != std::string::npos);
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::HandleSave("));
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::Handle("));
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::HandleComment("));
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::HandleString("));
		CUI_EXPECT_EQ(1ULL, countText(userCpp,
			"void PersistedEventForm::HandleRaw("));

		designButton->EventHandlers.erase(L"OnMouseClick");
		designButton->EventHandlers.erase(L"OnMouseDoubleClick");
		input.FormEventHandlers.erase(L"OnShown");
		CodeGenerator afterRemoval(L"PersistedEventForm", input);
		CUI_EXPECT_TRUE(afterRemoval.GenerateFiles(
			headerPath.wstring(), cppPath.wstring()));
		const auto retained = readText(
			outputDir / L"PersistedEventForm.handlers.g.inc");
		CUI_EXPECT_TRUE(retained.find("void HandleSave(") != std::string::npos);
		CUI_EXPECT_TRUE(retained.find("void HandleShown(") != std::string::npos);
		const auto afterRemovalGeneratedCpp = readText(
			outputDir / L"PersistedEventForm.g.cpp");
		CUI_EXPECT_TRUE(afterRemovalGeneratedCpp.find(
			"OnMouseClick.Subscribe(std::bind_front(&PersistedEventFormGenerated::HandleSave")
			== std::string::npos);
		CUI_EXPECT_TRUE(afterRemovalGeneratedCpp.find(
			"OnShown.Subscribe(std::bind_front(&PersistedEventFormGenerated::HandleShown")
			== std::string::npos);
		CUI_EXPECT_TRUE(readText(cppPath).find("USER_SENTINEL") != std::string::npos);

		// Existing user bodies are matched by parameter types, not only by name.
		// Formatting and parameter-name changes remain valid, while a type drift
		// must fail before any member of the five-file batch is modified.
		Button signatureButton(L"Signature", 0, 0);
		auto signatureDesignButton = std::make_shared<DesignerControl>(
			&signatureButton, L"signatureButton", UIClass::UI_Button);
		signatureDesignButton->EventHandlers[L"OnMouseClick"] =
			L"HandleSignatureDrift";
		CodeGenInput signatureInput;
		signatureInput.FormName = L"SignatureDriftForm";
		signatureInput.Controls = { signatureDesignButton };
		const auto signatureHeaderPath = outputDir / L"SignatureDriftForm.h";
		const auto signatureCppPath = outputDir / L"SignatureDriftForm.cpp";
		CodeGenerator signatureGenerator(L"SignatureDriftForm", signatureInput);
		CUI_EXPECT_TRUE(signatureGenerator.GenerateFiles(
			signatureHeaderPath.wstring(), signatureCppPath.wstring()));
		auto signatureUserCpp = readText(signatureCppPath);
		const std::string generatedSignature =
			"void SignatureDriftForm::HandleSignatureDrift(Control* sender, MouseEventArgs e)";
		const std::string reformattedSignature =
			"void SignatureDriftForm::HandleSignatureDrift( Control * origin, MouseEventArgs args )";
		auto signaturePosition = signatureUserCpp.find(generatedSignature);
		CUI_EXPECT_TRUE(signaturePosition != std::string::npos);
		if (signaturePosition != std::string::npos)
			signatureUserCpp.replace(signaturePosition,
				generatedSignature.size(), reformattedSignature);
		CUI_EXPECT_TRUE(DesignerModel::AtomicFile::Write(
			signatureCppPath.wstring(), signatureUserCpp));
		CodeGenerator reformattedSignatureGenerator(
			L"SignatureDriftForm", signatureInput);
		CUI_EXPECT_TRUE(reformattedSignatureGenerator.GenerateFiles(
			signatureHeaderPath.wstring(), signatureCppPath.wstring()));
		CUI_EXPECT_EQ(1ULL, countText(readText(signatureCppPath),
			"void SignatureDriftForm::HandleSignatureDrift("));

		signatureUserCpp = readText(signatureCppPath);
		signaturePosition = signatureUserCpp.find("MouseEventArgs args");
		CUI_EXPECT_TRUE(signaturePosition != std::string::npos);
		if (signaturePosition != std::string::npos)
			signatureUserCpp.replace(signaturePosition,
				std::string("MouseEventArgs args").size(), "KeyEventArgs args");
		CUI_EXPECT_TRUE(DesignerModel::AtomicFile::Write(
			signatureCppPath.wstring(), signatureUserCpp));
		const std::vector<fs::path> signatureBatchPaths{
			outputDir / L"SignatureDriftForm.g.h",
			outputDir / L"SignatureDriftForm.g.cpp",
			outputDir / L"SignatureDriftForm.handlers.g.inc",
			signatureHeaderPath,
			signatureCppPath,
		};
		std::vector<std::string> signatureBatchBefore;
		for (const auto& path : signatureBatchPaths)
			signatureBatchBefore.push_back(readText(path));
		CodeGenerator rejectedSignatureGenerator(
			L"SignatureDriftForm", signatureInput);
		CUI_EXPECT_FALSE(rejectedSignatureGenerator.GenerateFiles(
			signatureHeaderPath.wstring(), signatureCppPath.wstring()));
		CUI_EXPECT_TRUE(rejectedSignatureGenerator.GetLastError().find(
			L"HandleSignatureDrift") != std::wstring::npos);
		for (size_t index = 0; index < signatureBatchPaths.size(); ++index)
			CUI_EXPECT_EQ(signatureBatchBefore[index],
				readText(signatureBatchPaths[index]));

		Button namespaceButton(L"Namespaced", 0, 0);
		auto namespaceDesignButton = std::make_shared<DesignerControl>(
			&namespaceButton, L"namespaceButton", UIClass::UI_Button,
			nullptr, 77);
		namespaceDesignButton->EventHandlers[L"OnMouseClick"] =
			L"HandleNamespacedClick";
		namespaceDesignButton->EventHandlers[L"OnDropFile"] =
			L"HandleNamespacedDrop";
		namespaceDesignButton->EventHandlers[L"OnPropertyValueChanged"] =
			L"HandleNamespacedPropertyChanged";
		namespaceDesignButton->EventHandlers[L"OnValidationStateChanged"] =
			L"HandleNamespacedValidationChanged";
		CodeGenInput namespaceInput;
		namespaceInput.FormName = L"NamespacedRuntimeForm";
		namespaceInput.Controls = { namespaceDesignButton };
		const auto namespaceHeaderPath = outputDir / L"NamespacedWindow.h";
		const auto namespaceCppPath = outputDir / L"NamespacedWindow.cpp";
		CodeGenerator namespaceGenerator(
			L"Acme::Views::MainWindow", namespaceInput);
		CUI_EXPECT_TRUE(namespaceGenerator.GenerateFiles(
			namespaceHeaderPath.wstring(), namespaceCppPath.wstring()));
		const auto namespaceGeneratedHeader = readText(
			outputDir / L"NamespacedWindow.g.h");
		const auto namespaceGeneratedCpp = readText(
			outputDir / L"NamespacedWindow.g.cpp");
		const auto namespaceUserHeader = readText(namespaceHeaderPath);
		const auto namespaceUserCpp = readText(namespaceCppPath);
		CUI_EXPECT_TRUE(namespaceGeneratedHeader.find(
			"namespace Acme::Views") != std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedHeader.find(
			"class MainWindowGenerated : public Form") != std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedHeader.find(
			"static constexpr int namespaceButton = 77;") != std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedHeader.find(
			"Button* GetNamespaceButton() noexcept { return namespaceButton; }")
			!= std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedCpp.find(
			"#include \"NamespacedWindow.g.h\"") != std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedCpp.find(
			"Acme::Views::MainWindowGenerated::MainWindowGenerated()")
			!= std::string::npos);
		CUI_EXPECT_TRUE(namespaceGeneratedCpp.find(
			"&Acme::Views::MainWindowGenerated::HandleNamespacedClick")
			!= std::string::npos);
		CUI_EXPECT_TRUE(namespaceUserHeader.find(
			"<cui-designer-class>Acme::Views::MainWindow</cui-designer-class>")
			!= std::string::npos);
		CUI_EXPECT_TRUE(namespaceUserHeader.find(
			"class MainWindow : public MainWindowGenerated")
			!= std::string::npos);
		CUI_EXPECT_TRUE(namespaceUserCpp.find(
			"Acme::Views::MainWindow::MainWindow()") != std::string::npos);
		CUI_EXPECT_TRUE(namespaceUserCpp.find(
			"void Acme::Views::MainWindow::HandleNamespacedClick(")
			!= std::string::npos);
		CodeGenerator namespaceRegenerator(
			L"Acme::Views::MainWindow", namespaceInput);
		CUI_EXPECT_TRUE(namespaceRegenerator.GenerateFiles(
			namespaceHeaderPath.wstring(), namespaceCppPath.wstring()));
		CUI_EXPECT_EQ(1ULL, countText(readText(namespaceCppPath),
			"void Acme::Views::MainWindow::HandleNamespacedClick("));
		CodeGenerator wrongNamespace(
			L"Other::Views::MainWindow", namespaceInput);
		CUI_EXPECT_FALSE(wrongNamespace.GenerateFiles(
			namespaceHeaderPath.wstring(), namespaceCppPath.wstring()));
		CUI_EXPECT_EQ(namespaceGeneratedCpp,
			readText(outputDir / L"NamespacedWindow.g.cpp"));
		CUI_EXPECT_EQ(namespaceUserCpp, readText(namespaceCppPath));

		const auto legacyHeader = outputDir / L"Legacy.h";
		CUI_EXPECT_TRUE(DesignerModel::AtomicFile::Write(
			legacyHeader.wstring(), "// existing hand-written file\n"));
		CodeGenerator guarded(L"Legacy", input);
		CUI_EXPECT_FALSE(guarded.GenerateFiles(
			legacyHeader.wstring(), (outputDir / L"Legacy.cpp").wstring()));
		CUI_EXPECT_TRUE(readText(legacyHeader).find("existing hand-written file")
			!= std::string::npos);

		if (GetEnvironmentVariableW(
			L"CUI_KEEP_CODEGEN_TEST_OUTPUT", nullptr, 0) == 0)
			fs::remove_all(outputDir);
	});

	runner.Add("Headless design code generation resolves XAML and XML associations", []
	{
		namespace fs = std::filesystem;
		const fs::path outputDir = fs::temp_directory_path()
			/ (L"cui-headless-codegen-" + std::to_wstring(GetCurrentProcessId())
				+ L"-" + std::to_wstring(GetTickCount64()));
		fs::create_directories(outputDir);
		auto findRepositoryRoot = []
		{
			std::vector<fs::path> seeds{ fs::current_path() };
			try
			{
				seeds.push_back(fs::absolute(fs::path(__FILE__)).parent_path());
			}
			catch (...) {}
			for (auto seed : seeds)
				for (int depth = 0; depth < 8 && !seed.empty(); ++depth)
				{
					if (fs::exists(seed / L"CUI.sln")) return seed;
					const auto parent = seed.parent_path();
					if (parent == seed) break;
					seed = parent;
				}
			return fs::path{};
		};
		auto readText = [](const fs::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::string(
				std::istreambuf_iterator<char>(stream),
				std::istreambuf_iterator<char>());
		};
		auto canonicalText = [](std::string value)
		{
			value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
			return value;
		};

		const auto repositoryRoot = findRepositoryRoot();
		CUI_EXPECT_TRUE(!repositoryRoot.empty());
		const auto xamlPath = repositoryRoot
			/ L"CuiStaticGeneratedSample" / L"NamespacedWindow.cui.xaml";
		DesignerModel::DesignCodeGenerationOptions xamlOptions;
		xamlOptions.OutputBasePath = (outputDir / L"NamespacedWindow").wstring();
		DesignerModel::DesignCodeGenerationResult xamlResult;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::DesignCodeGenerationService::GenerateFile(
			xamlPath.wstring(), xamlOptions, &xamlResult, &error));
		CUI_EXPECT_EQ(std::wstring(L"Acme::Views::MainWindow"),
			xamlResult.ClassName);
		CUI_EXPECT_EQ(5ULL, xamlResult.OutputFiles().size());
		for (const auto& file : xamlResult.OutputFiles())
			CUI_EXPECT_TRUE(fs::exists(file));
		CUI_EXPECT_TRUE(readText(xamlResult.GeneratedSourcePath).find(
			"&Acme::Views::MainWindowGenerated::HandleNamespacedClick")
			!= std::string::npos);
		const auto staticSample = repositoryRoot / L"CuiStaticGeneratedSample";
		for (const auto* fileName : {
			L"NamespacedWindow.g.h", L"NamespacedWindow.g.cpp",
			L"NamespacedWindow.handlers.g.inc", L"NamespacedWindow.h",
			L"NamespacedWindow.cpp" })
		{
			CUI_EXPECT_EQ(
				canonicalText(readText(outputDir / fileName)),
				canonicalText(readText(staticSample / fileName)));
		}
		const auto preservedTime = fs::file_time_type::clock::now()
			- std::chrono::hours(2);
		std::vector<fs::file_time_type> preservedTimes;
		for (const auto& file : xamlResult.OutputFiles())
		{
			fs::last_write_time(file, preservedTime);
			preservedTimes.push_back(fs::last_write_time(file));
		}
		CUI_EXPECT_TRUE(DesignerModel::DesignCodeGenerationService::GenerateFile(
			xamlPath.wstring(), xamlOptions, &xamlResult, &error));
		const auto regeneratedFiles = xamlResult.OutputFiles();
		for (size_t index = 0; index < regeneratedFiles.size(); ++index)
			CUI_EXPECT_EQ(preservedTimes[index],
				fs::last_write_time(regeneratedFiles[index]));

		DesignerModel::DesignDocument xmlDocument;
		xmlDocument.Form.Name = L"XmlWindow";
		xmlDocument.CodeBehind.ClassName = L"Acme::Tools::XmlWindow";
		xmlDocument.CodeBehind.RelativeBasePath = L"generated/XmlWindow";
		DesignerModel::DesignNode button;
		button.Id = 1;
		button.Name = L"xmlButton";
		button.Type = UIClass::UI_Button;
		button.Props["Text"] = "XML";
		button.Props["Width"] = 100;
		button.Props["Height"] = 28;
		button.Events["OnMouseClick"] = "HandleXmlClick";
		xmlDocument.Nodes.push_back(std::move(button));
		xmlDocument.RecalculateNextStableId();
		const auto xmlPath = outputDir / L"XmlWindow.xml";
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::SaveToFile(
			xmlDocument, xmlPath.wstring(), &error));
		DesignerModel::DesignCodeGenerationResult xmlResult;
		CUI_EXPECT_TRUE(DesignerModel::DesignCodeGenerationService::GenerateFile(
			xmlPath.wstring(), {}, &xmlResult, &error));
		CUI_EXPECT_EQ(std::wstring(L"Acme::Tools::XmlWindow"),
			xmlResult.ClassName);
		CUI_EXPECT_EQ((outputDir / L"generated" / L"XmlWindow").wstring(),
			xmlResult.OutputBasePath);
		for (const auto& file : xmlResult.OutputFiles())
			CUI_EXPECT_TRUE(fs::exists(file));

		DesignerModel::DesignCodeGenerationOptions invalidOptions;
		invalidOptions.ClassName = L"Invalid.Window";
		invalidOptions.OutputBasePath = (outputDir / L"Invalid.cpp").wstring();
		CUI_EXPECT_FALSE(DesignerModel::DesignCodeGenerationService::Generate(
			xmlDocument, L"", invalidOptions, nullptr, &error));
		CUI_EXPECT_TRUE(error.find(L"扩展名") != std::wstring::npos);
		CUI_EXPECT_FALSE(fs::exists(outputDir / L"Invalid.cpp.g.h"));

		if (GetEnvironmentVariableW(
			L"CUI_KEEP_CODEGEN_TEST_OUTPUT", nullptr, 0) == 0)
			fs::remove_all(outputDir);
	});

	runner.Add("Designer code generation emits document style resources and rules", []
	{
		CodeGenInput input;
		input.FormName = L"StyledForm";
		input.FormText = L"Styled";
		input.StyleSheet.Resources.push_back({
			L"Accent", { DesignerStyleValueKind::Color, L"#FF0078D4" } });
		DesignerStyleRule rule;
		rule.HasType = true;
		rule.Type = UIClass::UI_Button;
		rule.Classes = { L"primary" };
		rule.RequiredStates = ControlStyleState::Hovered;
		rule.Setters = {
			{ L"UnderMouseColor", true, L"Accent", {} },
			{ L"Round", false, L"", { DesignerStyleValueKind::Float, L"8.5" } }
		};
		input.StyleSheet.Rules.push_back(std::move(rule));
		Button styledRoot(L"Styled", 0, 0, 120, 32);
		input.Controls.push_back(std::make_shared<DesignerControl>(
			&styledRoot, L"styledRoot", UIClass::UI_Button, nullptr, 1));

		CodeGenerator generator(L"StyledForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find("#include \"Style.h\"") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("SetResource(L\"Accent\"") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("UIClass::UI_Button") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("RequiredStates = static_cast<ControlStyleState>(1u)")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("ControlStyleSetter::Resource(L\"UnderMouseColor\", L\"Accent\")")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("BindingValue(8.5f)") != std::string::npos);
		// Form is not a Control. The shared document sheet belongs on each root
		// control tree and then propagates recursively.
		CUI_EXPECT_TRUE(cpp.find("->SetStyleSheet(__styleSheet, true);")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("this->SetStyleSheet(__styleSheet);")
			== std::string::npos);
	});

	runner.Add("Designer metadata properties generate through runtime property metadata", []
	{
		Button button(L"Metadata", 0, 0);
		std::wstring canonicalName;
		DesignerStyleValue effectiveValue;
		std::wstring error;
		auto designerControl = std::make_shared<DesignerControl>(
			&button, L"metadataButton", UIClass::UI_Button);
		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ApplyAndTrackValue(
			button,
			designerControl->MetadataProperties,
			L"Round",
			{ DesignerStyleValueKind::Float, L"9.25" },
			&canonicalName,
			&effectiveValue,
			&error));

		CodeGenInput input;
		input.Controls.push_back(designerControl);
		CodeGenerator generator(L"MetadataForm", input);
		const auto header = generator.GenerateHeader();
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(header.find("#include \"Binding.h\"") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"(void)metadataButton->TrySetPropertyValue(L\"Round\", BindingValue(9.25f));")
			!= std::string::npos);

		CUI_EXPECT_TRUE(DesignerPropertyCatalog::ResetAndUntrackValue(
			button, designerControl->MetadataProperties, L"Round"));
		CodeGenerator resetGenerator(L"MetadataForm", input);
		const auto resetCpp = resetGenerator.GenerateCpp();
		CUI_EXPECT_TRUE(resetCpp.find(
			"metadataButton->TrySetPropertyValue(L\"Round\"") == std::string::npos);
	});

	runner.Add("Designer binding code generation preserves and restores Local values", []
	{
		Button captionButton(L"Fallback caption", 0, 0);
		auto captionTarget = std::make_shared<DesignerControl>(
			&captionButton, L"captionButton", UIClass::UI_Button);
		captionTarget->DataBindings[L"Text"] = {
			L"Caption", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"StringTrim"
		};
		CheckBox stateCheckBox(L"State", 0, 0);
		auto stateTarget = std::make_shared<DesignerControl>(
			&stateCheckBox, L"stateCheckBox", UIClass::UI_CheckBox);
		stateTarget->DataBindings[L"Checked"] = {
			L"Enabled", BindingMode::OneWayToSource,
			DataSourceUpdateMode::OnPropertyChanged, L""
		};
		CodeGenInput input;
		input.Controls = { captionTarget, stateTarget };
		CodeGenerator generator(L"BoundForm", input);
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"TryGetPropertyValue(L\"Text\", ControlPropertyValueSource::Local")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"ClearPropertyValue(L\"Text\", ControlPropertyValueSource::Local")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"TrySetPropertyValue(L\"Text\", __previousLocal, ControlPropertyValueSource::Local")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"BindingValueConverterRegistry::Create(L\"StringTrim\")")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"ClearPropertyValue(L\"Checked\", ControlPropertyValueSource::Local")
			== std::string::npos);
	});

    runner.Add("Designer binding validation uses target metadata capabilities", []
	{
		Control target;
		DesignerDataBinding binding{
			L"Profile.Width", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged };
		const BindingPropertyMetadata* metadata = nullptr;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"LayoutWidth", binding, &metadata, &error));
		CUI_EXPECT_TRUE(metadata != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"LayoutWidth"), metadata->Name());
		CUI_EXPECT_TRUE(error.empty());

		binding.Mode = BindingMode::TwoWay;
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"LayoutWidth", binding, nullptr, &error));
		CUI_EXPECT_TRUE(error.find(L"变更通知") != std::wstring::npos);

		binding.UpdateMode = DataSourceUpdateMode::Never;
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"LayoutWidth", binding, nullptr, &error));

		binding.SourceProperty = L"Profile..Width";
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"LayoutWidth", binding, nullptr, &error));
		CUI_EXPECT_TRUE(error.find(L"源路径") != std::wstring::npos);

		binding = DesignerDataBinding{
			L"Profile.Name", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"StringIsNotEmpty" };
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"Checked", binding, &metadata, &error));
		CUI_EXPECT_EQ(BindingValueKind::Bool, metadata->ValueKind());

		binding.Mode = BindingMode::TwoWay;
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Checked", binding, nullptr, &error));
		CUI_EXPECT_TRUE(error.find(L"ConvertBack") != std::wstring::npos);

		binding.Mode = BindingMode::OneWay;
		binding.Converter = L"StringTrim";
		CUI_EXPECT_FALSE(DesignerBindingUtils::Validate(
			target, L"Checked", binding, nullptr, &error));
		CUI_EXPECT_TRUE(error.find(L"类型") != std::wstring::npos);

		binding.Converter = L"Application.CustomConverter";
		CUI_EXPECT_TRUE(DesignerBindingUtils::Validate(
			target, L"Checked", binding, nullptr, &error));
	});

	runner.Add("Binding metadata discovery returns effective properties", []
    {
        DateTimePicker picker;
        const auto properties = BindingPropertyRegistry::GetProperties(picker);
        CUI_EXPECT_TRUE(!properties.empty());

        auto findProperty = [&](const wchar_t* name)
        {
            auto it = std::find_if(properties.begin(), properties.end(),
                [name](const BindingPropertyMetadata* property)
                {
                    return _wcsicmp(property->Name().c_str(), name) == 0;
                });
            return it == properties.end() ? nullptr : *it;
        };

		const auto* text = findProperty(L"Text");
		const auto* value = findProperty(L"Value");
		const auto* mode = findProperty(L"Mode");
		const auto* validationBorder = findProperty(L"ShowValidationBorder");
		const auto* validationToolTip = findProperty(L"ShowValidationToolTip");
		const auto* accessibleDescription = findProperty(L"AccessibleDescription");
		CUI_EXPECT_TRUE(text != nullptr);
		CUI_EXPECT_TRUE(value != nullptr);
		CUI_EXPECT_TRUE(mode != nullptr);
		CUI_EXPECT_TRUE(validationBorder != nullptr);
		CUI_EXPECT_TRUE(validationToolTip != nullptr);
		CUI_EXPECT_TRUE(accessibleDescription != nullptr);
		CUI_EXPECT_TRUE(value->CanRead());
        CUI_EXPECT_TRUE(value->CanWrite());
		CUI_EXPECT_TRUE(value->CanObserve());
		CUI_EXPECT_FALSE(mode->CanObserve());
		CUI_EXPECT_TRUE(validationBorder->CanRead());
		CUI_EXPECT_TRUE(validationBorder->CanWrite());
		CUI_EXPECT_FALSE(validationBorder->CanObserve());
		CUI_EXPECT_EQ(BindingValueKind::String,
			accessibleDescription->ValueKind());

        for (size_t i = 1; i < properties.size(); ++i)
        {
            CUI_EXPECT_TRUE(_wcsicmp(
                properties[i - 1]->Name().c_str(),
                properties[i]->Name().c_str()) < 0);
        }
    });

	runner.Add("Portable custom binding metadata uses the shared validator", []
	{
		DesignerBindingUtils::TargetMetadata target{
			L"Severity", BindingValueKind::Int64,
			false, true, false };
		DesignerDataContextSchema schema{
			{ L"Status.Severity", BindingValueKind::Int64,
				true, true, true },
			{ L"Status", BindingValueKind::Object,
				true, false, true }
		};
		DesignerDataBinding oneWay{
			L"Status.Severity", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged };
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerBindingUtils::ValidateTarget(
			target, oneWay, &error, &schema));

		auto twoWay = oneWay;
		twoWay.Mode = BindingMode::TwoWay;
		CUI_EXPECT_FALSE(DesignerBindingUtils::ValidateTarget(
			target, twoWay, &error, &schema));

		target.CanRead = true;
		target.CanObserve = true;
		CUI_EXPECT_TRUE(DesignerBindingUtils::ValidateTarget(
			target, twoWay, &error, &schema));
	});

    runner.Add("Interactive state metadata supports TwoWay binding", []
    {
        ObservableObject source;
        source.SetValue(L"Expanded", false);
        Expander expander;
        Binding* expandedBinding = expander.DataBindings.Add(
            L"IsExpanded", source, L"Expanded", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(expandedBinding != nullptr);
        CUI_EXPECT_FALSE(expander.IsExpanded);
        expander.Toggle();
        CUI_EXPECT_TRUE(source.GetValue<bool>(L"Expanded"));

        NavigationView navigation;
        navigation.AddItem(L"First");
        navigation.AddItem(L"Second");
        source.SetValue(L"NavigationIndex", 1);
        Binding* navigationBinding = navigation.DataBindings.Add(
            L"SelectedIndex", source, L"NavigationIndex", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(navigationBinding != nullptr);
        CUI_EXPECT_EQ(1, navigation.SelectedIndex);
        CUI_EXPECT_TRUE(navigation.SelectItem(0));
        CUI_EXPECT_EQ(0, source.GetValue<int>(L"NavigationIndex"));

        TabControl tabs(0, 0, 320, 200);
        tabs.AddPage(L"First");
        tabs.AddPage(L"Second");
        source.SetValue(L"TabIndex", 1);
        Binding* tabBinding = tabs.DataBindings.Add(
            L"SelectedIndex", source, L"TabIndex", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(tabBinding != nullptr);
        CUI_EXPECT_EQ(1, tabs.SelectedIndex);
        CUI_EXPECT_TRUE(tabs.SelectPage(0));
        CUI_EXPECT_EQ(0, source.GetValue<int>(L"TabIndex"));
    });

    runner.Add("Date and color metadata preserve arbitrary value types", []
    {
        SYSTEMTIME initial{};
        initial.wYear = 2026;
        initial.wMonth = 7;
        initial.wDay = 14;
        ObservableObject source;
        source.SetValue(L"Date", initial);

        DateTimePicker datePicker;
        Binding* dateBinding = datePicker.DataBindings.Add(
            L"Value", source, L"Date", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(dateBinding != nullptr);
        CUI_EXPECT_EQ(2026, static_cast<int>(datePicker.Value.wYear));
        CUI_EXPECT_EQ(7, static_cast<int>(datePicker.Value.wMonth));

        SYSTEMTIME changed = initial;
        changed.wDay = 21;
        datePicker.Value = changed;
        CUI_EXPECT_EQ(
            21,
            static_cast<int>(source.GetValue<SYSTEMTIME>(L"Date").wDay));

        CalendarView calendar;
        source.SetValue(L"CalendarDate", initial);
        Binding* calendarBinding = calendar.DataBindings.Add(
            L"SelectedDate", source, L"CalendarDate", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(calendarBinding != nullptr);
        changed.wDay = 22;
        calendar.SetSelectedDate(changed);
        CUI_EXPECT_EQ(
            22,
            static_cast<int>(source.GetValue<SYSTEMTIME>(L"CalendarDate").wDay));

        const D2D1_COLOR_F initialColor{ 0.1f, 0.2f, 0.3f, 1.0f };
        source.SetValue(L"Color", initialColor);
        ColorPicker colorPicker;
        Binding* colorBinding = colorPicker.DataBindings.Add(
            L"SelectedColor", source, L"Color", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(colorBinding != nullptr);
        CUI_EXPECT_NEAR(0.2f, colorPicker.SelectedColor.g, 0.0001f);

        colorPicker.SelectedColor = D2D1_COLOR_F{ 0.8f, 0.7f, 0.6f, 1.0f };
        const auto storedColor = source.GetValue<D2D1_COLOR_F>(L"Color");
        CUI_EXPECT_NEAR(0.8f, storedColor.r, 0.0001f);
        CUI_EXPECT_NEAR(0.6f, storedColor.b, 0.0001f);
    });

    runner.Add("Scroll offsets expose observable metadata", []
    {
        ObservableObject source;
        source.SetValue(L"OffsetX", 30);
        source.SetValue(L"OffsetY", 40);

        ScrollView scroll(0, 0, 100, 80);
        scroll.AutoContentSize = false;
        scroll.ContentSize = SIZE{ 500, 400 };
        Binding* xBinding = scroll.DataBindings.Add(
            L"ScrollXOffset", source, L"OffsetX", BindingMode::TwoWay);
        Binding* yBinding = scroll.DataBindings.Add(
            L"ScrollYOffset", source, L"OffsetY", BindingMode::TwoWay);
        CUI_EXPECT_TRUE(xBinding != nullptr);
        CUI_EXPECT_TRUE(yBinding != nullptr);
        CUI_EXPECT_EQ(30, scroll.ScrollXOffset);
        CUI_EXPECT_EQ(40, scroll.ScrollYOffset);

        scroll.SetScrollOffset(55, 65);
        CUI_EXPECT_EQ(55, source.GetValue<int>(L"OffsetX"));
        CUI_EXPECT_EQ(65, source.GetValue<int>(L"OffsetY"));
    });

    runner.Add("Binding clear disconnects source and target events", []
    {
        ObservableObject source;
        source.SetValue(L"Name", std::wstring(L"before"));
        Control target;

        CUI_EXPECT_EQ(0ULL, source.PropertyChanged().Count());
        CUI_EXPECT_EQ(0ULL, target.OnTextChanged.Count());
        Binding* binding = target.DataBindings.Add(
            L"Text", source, L"Name", BindingMode::TwoWay);

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_EQ(1ULL, source.PropertyChanged().Count());
        CUI_EXPECT_EQ(1ULL, target.OnTextChanged.Count());

        target.DataBindings.Clear();
        CUI_EXPECT_EQ(0ULL, source.PropertyChanged().Count());
        CUI_EXPECT_EQ(0ULL, target.OnTextChanged.Count());
    });

    runner.Add("Binding target destruction disconnects the source", []
    {
        ObservableObject source;
        source.SetValue(L"Name", std::wstring(L"before"));
        {
            Control target;
            CUI_EXPECT_TRUE(target.DataBindings.Add(
                L"Text", source, L"Name", BindingMode::TwoWay) != nullptr);
            CUI_EXPECT_EQ(1ULL, source.PropertyChanged().Count());
        }
        CUI_EXPECT_EQ(0ULL, source.PropertyChanged().Count());
    });

    runner.Add("Binding tolerates source destruction before target", []
    {
        Control target;
        Binding* binding = nullptr;
        {
            auto source = std::make_unique<MetadataObservableObject>();
            source->SetValue(L"Name", std::wstring(L"before"));
            source->SetValidationError(L"Name", L"temporary issue");
            binding = target.DataBindings.Add(
                L"Text", *source, L"Name", BindingMode::TwoWay);
            CUI_EXPECT_TRUE(binding != nullptr);
            CUI_EXPECT_EQ(std::wstring(L"before"), target.Text);
            CUI_EXPECT_TRUE(binding->HasValidationErrors());
        }

        CUI_EXPECT_FALSE(binding->HasValidationIssues());
        CUI_EXPECT_FALSE(target.DataBindings.HasValidationErrors());
        target.Text = L"after source destruction";
        CUI_EXPECT_EQ(BindingError::SourceUnavailable, binding->LastError());
        target.DataBindings.Clear();
        CUI_EXPECT_EQ(0ULL, target.OnTextChanged.Count());
    });

    runner.Add("Copied binding sources do not share event subscribers", []
    {
        MetadataObservableObject source;
        source.SetValue(L"Name", std::wstring(L"original"));
        source.SetValidationError(L"Name", L"source issue");
        Control target;
        Binding* binding = target.DataBindings.Add(L"Text", source, L"Name");
        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_TRUE(binding->HasValidationErrors());

        MetadataObservableObject copy = source;
        CUI_EXPECT_EQ(1ULL, source.PropertyChanged().Count());
        CUI_EXPECT_EQ(0ULL, copy.PropertyChanged().Count());
        CUI_EXPECT_EQ(1ULL, source.ValidationChanged()->Count());
        CUI_EXPECT_EQ(0ULL, copy.ValidationChanged()->Count());
        CUI_EXPECT_TRUE(copy.HasValidationErrors());
        copy.ClearAllValidationIssues();
        CUI_EXPECT_TRUE(binding->HasValidationErrors());
        copy.SetValue(L"Name", std::wstring(L"copy"));
        CUI_EXPECT_EQ(std::wstring(L"original"), target.Text);

        source.SetValue(L"Name", std::wstring(L"updated"));
        CUI_EXPECT_EQ(std::wstring(L"updated"), target.Text);
    });

    runner.Add("Binding rejects invalid target configurations", []
    {
        ObservableObject source;
        source.SetValue(L"Value", true);
        Control target;

        CUI_EXPECT_EQ(nullptr, target.DataBindings.Add(L"Missing", source, L"Value"));
        CUI_EXPECT_EQ(BindingError::TargetPropertyNotFound, target.DataBindings.LastError());
        CUI_EXPECT_EQ(0ULL, target.DataBindings.Count());

        CUI_EXPECT_EQ(nullptr, target.DataBindings.Add(
            L"Visible", source, L"Value", BindingMode::TwoWay));
        CUI_EXPECT_EQ(BindingError::TargetNotObservable, target.DataBindings.LastError());
        CUI_EXPECT_EQ(0ULL, target.DataBindings.Count());

        Binding* oneWay = target.DataBindings.Add(
            L"Visible", source, L"Value", BindingMode::OneWay);
        CUI_EXPECT_TRUE(oneWay != nullptr);
        CUI_EXPECT_TRUE(oneWay->IsValid());
        CUI_EXPECT_EQ(BindingError::None, target.DataBindings.LastError());
    });

    runner.Add("Binding exposes runtime source read failures", []
    {
        ObservableObject source;
        Control target;
        Binding* binding = target.DataBindings.Add(L"Text", source, L"Missing");

        CUI_EXPECT_TRUE(binding != nullptr);
        CUI_EXPECT_TRUE(binding->IsValid());
        CUI_EXPECT_EQ(BindingError::SourceReadFailed, binding->LastError());

        source.SetValue(L"Missing", std::wstring(L"available later"));
        CUI_EXPECT_EQ(std::wstring(L"available later"), target.Text);
        CUI_EXPECT_EQ(BindingError::None, binding->LastError());
    });

    runner.Add("Layout size metadata binds Auto and fractional DIPs", []
    {
        using namespace cui::layout;

        ObservableObject source;
        source.SetValue(L"Width", Length::Fixed(51.375f));

        Control target;
        target.DataBindings.Add(L"LayoutWidth", source, L"Width");
        CUI_EXPECT_TRUE(target.GetLayoutWidth().IsFixed());
        CUI_EXPECT_NEAR(51.375f, target.GetLayoutWidth().value, 0.0001f);

        source.SetValue(L"Width", Length::Auto());
        CUI_EXPECT_TRUE(target.GetLayoutWidth().IsAuto());
    });

    runner.Add("Panel-specific layout metadata binds without core switches", []
    {
        ObservableObject source;
        source.SetValue(L"Spacing", 7.25f);
        source.SetValue(L"ContentAlignment", HorizontalAlignment::Center);
        source.SetValue(L"ItemWidth", 64.5f);
        source.SetValue(L"LastChildFill", false);

        StackPanel stack;
        WrapPanel wrap;
        DockPanel dock;
        CUI_EXPECT_TRUE(stack.DataBindings.Add(L"Spacing", source, L"Spacing") != nullptr);
        CUI_EXPECT_TRUE(stack.DataBindings.Add(
            L"HorizontalContentAlignment", source, L"ContentAlignment") != nullptr);
        CUI_EXPECT_TRUE(wrap.DataBindings.Add(L"ItemWidth", source, L"ItemWidth") != nullptr);
        CUI_EXPECT_TRUE(dock.DataBindings.Add(L"LastChildFill", source, L"LastChildFill") != nullptr);

        CUI_EXPECT_NEAR(7.25f, stack.GetSpacing(), 0.0001f);
        CUI_EXPECT_EQ(HorizontalAlignment::Center, stack.GetHorizontalContentAlignment());
        CUI_EXPECT_NEAR(64.5f, wrap.GetItemWidth(), 0.0001f);
        CUI_EXPECT_FALSE(dock.GetLastChildFill());

        source.SetValue(L"Spacing", 3.5f);
        source.SetValue(L"ItemWidth", 72.75f);
        CUI_EXPECT_NEAR(3.5f, stack.GetSpacing(), 0.0001f);
        CUI_EXPECT_NEAR(72.75f, wrap.GetItemWidth(), 0.0001f);
    });

    runner.Add("Default text metadata updates TwoWay sources", []
    {
        ObservableObject source;
        source.SetValue(L"Name", std::wstring(L"before"));

        Control target;
        target.DataBindings.Add(L"Text", source, L"Name", BindingMode::TwoWay);
        CUI_EXPECT_EQ(std::wstring(L"before"), target.Text);

        target.Text = L"after";
        CUI_EXPECT_EQ(std::wstring(L"after"), source.GetValue<std::wstring>(L"Name"));
    });

    runner.Add("Changed measure constraints invalidate arrange", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LayoutState state;
        state.CommitMeasure(Size{ 80.0f, 20.0f }, Constraints{ Size{ 100.0f, 40.0f } });
        state.CommitArrange(Rect{ 0.0f, 0.0f, 80.0f, 20.0f });
        state.CommitPaint();
        CUI_EXPECT_FALSE(state.NeedsArrange());

        state.CommitMeasure(Size{ 80.0f, 20.0f }, Constraints{ Size{ 200.0f, 40.0f } });
        CUI_EXPECT_TRUE(state.NeedsArrange());
        CUI_EXPECT_TRUE(state.NeedsPaint());
    });

    runner.Add("Built-in Stack measurement preserves fractional DIPs", []
    {
        using namespace cui::core;

        Control container;
        auto* first = container.AddControl(new FractionalMeasureControl(Size{ 20.25f, 10.5f }));
        auto* second = container.AddControl(new FractionalMeasureControl(Size{ 30.75f, 11.125f }));
        first->Margin = Thickness(0.25f, 0.5f, 0.75f, 1.0f);
        second->Margin = Thickness(1.25f, 0.0f, 0.5f, 0.25f);

        StackLayoutEngine engine;
        engine.SetSpacing(2.375f);
        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints{ Size{ 200.5f, 100.25f } });

        CUI_EXPECT_NEAR(32.5f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(25.75f, desired.height, 0.0001f);
    });

    runner.Add("Stack measurement deflates cross axis margins", []
    {
        using namespace cui::core;

        Control container;
        auto* child = container.AddControl(new WidthAwareMeasureControl());
        child->Margin = Thickness(5.0f, 0.0f, 7.0f, 0.0f);

        StackLayoutEngine engine;
        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints{ Size{ 60.0f, Infinity } });

        CUI_EXPECT_NEAR(48.0f, child->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, desired.height, 0.0001f);
    });

    runner.Add("Vertical Stack aligns its content band and children", []
    {
        using namespace cui::core;

        Control container;
        auto* first = container.AddControl(new FractionalMeasureControl(Size{ 40.0f, 10.0f }));
        auto* second = container.AddControl(new FractionalMeasureControl(Size{ 20.0f, 10.0f }));
        second->HAlign = HorizontalAlignment::Center;

        StackLayoutEngine engine;
        engine.SetSpacing(5.0f);
        engine.SetHorizontalContentAlignment(HorizontalAlignment::Center);
        engine.SetVerticalContentAlignment(VerticalAlignment::Center);
        LayoutContext context(&container);
        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 200.0f, 100.0f });

        CUI_EXPECT_NEAR(80.0f, first->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(37.5f, first->GetActualLocationDip().y, 0.0001f);
        CUI_EXPECT_NEAR(90.0f, second->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(52.5f, second->GetActualLocationDip().y, 0.0001f);
    });

    runner.Add("Horizontal Stack aligns the full row and cross axis", []
    {
        using namespace cui::core;

        Control container;
        auto* first = container.AddControl(new FractionalMeasureControl(Size{ 10.0f, 20.0f }));
        auto* second = container.AddControl(new FractionalMeasureControl(Size{ 20.0f, 10.0f }));
        second->VAlign = VerticalAlignment::Center;

        StackLayoutEngine engine;
        engine.SetOrientation(Orientation::Horizontal);
        engine.SetSpacing(5.0f);
        engine.SetHorizontalContentAlignment(HorizontalAlignment::Right);
        engine.SetVerticalContentAlignment(VerticalAlignment::Bottom);
        LayoutContext context(&container);
        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 100.0f, 80.0f });

        CUI_EXPECT_NEAR(65.0f, first->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, first->GetActualLocationDip().y, 0.0001f);
        CUI_EXPECT_NEAR(80.0f, second->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(65.0f, second->GetActualLocationDip().y, 0.0001f);
    });

    runner.Add("StackPanel exposes normalized content settings", []
    {
        StackPanel panel;
        panel.SetSpacing(-5.0f);
        panel.SetHorizontalContentAlignment(HorizontalAlignment::Right);
        panel.SetVerticalContentAlignment(VerticalAlignment::Bottom);

        CUI_EXPECT_NEAR(0.0f, panel.GetSpacing(), 0.0001f);
        CUI_EXPECT_EQ(HorizontalAlignment::Right, panel.GetHorizontalContentAlignment());
        CUI_EXPECT_EQ(VerticalAlignment::Bottom, panel.GetVerticalContentAlignment());
    });

    runner.Add("Wrap fixed item width drives width-aware measurement", []
    {
        using namespace cui::core;

        Control container;
        auto* first = container.AddControl(new WidthAwareMeasureControl());
        auto* second = container.AddControl(new WidthAwareMeasureControl());
        WrapLayoutEngine engine;
        engine.SetItemWidth(60.0f);
        LayoutContext context(&container);

        const auto desired = engine.Measure(context, Constraints{ Size{ 120.0f, 100.0f } });
        CUI_EXPECT_NEAR(60.0f, first->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, second->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(120.0f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, desired.height, 0.0001f);

        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 120.0f, 100.0f });
        CUI_EXPECT_NEAR(60.0f, first->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, first->GetActualSizeDip().height, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, second->GetActualLocationDip().x, 0.0001f);
    });

    runner.Add("Wrap auto items deflate margins from constraints", []
    {
        using namespace cui::core;

        Control container;
        auto* child = container.AddControl(new WidthAwareMeasureControl());
        child->Margin = Thickness(5.0f, 0.0f, 7.0f, 0.0f);
        WrapLayoutEngine engine;
        LayoutContext context(&container);

        const auto desired = engine.Measure(context, Constraints{ Size{ 60.0f, 100.0f } });
        CUI_EXPECT_NEAR(48.0f, child->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, desired.height, 0.0001f);
    });

    runner.Add("WrapPanel normalizes invalid item dimensions", []
    {
        WrapPanel panel;
        panel.SetItemWidth(-10.0f);
        panel.SetItemHeight((std::numeric_limits<float>::quiet_NaN)());
        CUI_EXPECT_NEAR(0.0f, panel.GetItemWidth(), 0.0001f);
        CUI_EXPECT_NEAR(0.0f, panel.GetItemHeight(), 0.0001f);
    });

    runner.Add("Dock measurement includes previously consumed axes", []
    {
        using namespace cui::core;

        Control container;
        auto* left = container.AddControl(new FractionalMeasureControl(Size{ 30.0f, 20.0f }));
        auto* top = container.AddControl(new FractionalMeasureControl(Size{ 50.0f, 10.0f }));
        auto* fill = container.AddControl(new FractionalMeasureControl(Size{ 40.0f, 15.0f }));
        left->DockPosition = Dock::Left;
        top->DockPosition = Dock::Top;
        fill->DockPosition = Dock::Fill;

        DockLayoutEngine engine;
        engine.SetLastChildFill(false);
        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints::Unbounded());

        CUI_EXPECT_NEAR(80.0f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(25.0f, desired.height, 0.0001f);
    });

    runner.Add("Dock LastChildFill uses the last visible child", []
    {
        using namespace cui::core;

        Control container;
        auto* first = container.AddControl(new FractionalMeasureControl(Size{ 30.0f, 20.0f }));
        auto* lastVisible = container.AddControl(new FractionalMeasureControl(Size{ 40.0f, 20.0f }));
        auto* hidden = container.AddControl(new FractionalMeasureControl(Size{ 10.0f, 10.0f }));
        first->DockPosition = Dock::Left;
        lastVisible->DockPosition = Dock::Left;
        hidden->Visible = false;

        DockLayoutEngine engine;
        LayoutContext context(&container);
        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 100.0f, 20.0f });

        CUI_EXPECT_NEAR(30.0f, first->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(30.0f, lastVisible->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(70.0f, lastVisible->GetActualSizeDip().width, 0.0001f);
    });

    runner.Add("Dock deflates margins before measuring children", []
    {
        using namespace cui::core;

        Control container;
        auto* child = container.AddControl(new WidthAwareMeasureControl());
        child->DockPosition = Dock::Top;
        child->Margin = Thickness(5.0f, 0.0f, 7.0f, 0.0f);

        DockLayoutEngine engine;
        engine.SetLastChildFill(false);
        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints{ Size{ 60.0f, 100.0f } });

        CUI_EXPECT_NEAR(48.0f, child->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(60.0f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, desired.height, 0.0001f);
    });

    runner.Add("Grid redistributes bounded Percent and Star tracks", []
    {
        using namespace cui::core;

        Control container;
        auto* percentChild = container.AddControl(new FractionalMeasureControl(Size{ 1.0f, 1.0f }));
        auto* flexibleChild = container.AddControl(new FractionalMeasureControl(Size{ 1.0f, 1.0f }));
        auto* cappedChild = container.AddControl(new FractionalMeasureControl(Size{ 1.0f, 1.0f }));
        percentChild->GridColumn = 0;
        flexibleChild->GridColumn = 1;
        cappedChild->GridColumn = 2;
        percentChild->HAlign = HorizontalAlignment::Stretch;
        flexibleChild->HAlign = HorizontalAlignment::Stretch;
        cappedChild->HAlign = HorizontalAlignment::Stretch;
        percentChild->VAlign = VerticalAlignment::Stretch;
        flexibleChild->VAlign = VerticalAlignment::Stretch;
        cappedChild->VAlign = VerticalAlignment::Stretch;

        GridLayoutEngine engine;
        engine.AddColumn(ColumnDefinition(GridLength::Percent(25.0f)));
        engine.AddColumn(ColumnDefinition(GridLength::Star(1.0f), 80.0f));
        engine.AddColumn(ColumnDefinition(GridLength::Star(1.0f), 0.0f, 40.0f));
        engine.AddRow(RowDefinition(GridLength::Pixels(20.0f)));

        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints{ Size{ 200.0f, 20.0f } });
        CUI_EXPECT_NEAR(200.0f, desired.width, 0.0001f);
        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 200.0f, 20.0f });

        CUI_EXPECT_NEAR(50.0f, percentChild->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(110.0f, flexibleChild->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, cappedChild->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(50.0f, flexibleChild->GetActualLocationDip().x, 0.0001f);
        CUI_EXPECT_NEAR(160.0f, cappedChild->GetActualLocationDip().x, 0.0001f);
    });

    runner.Add("Grid Auto tracks include spanning content", []
    {
        using namespace cui::core;

        Control container;
        auto* spanning = container.AddControl(new FractionalMeasureControl(Size{ 101.5f, 20.25f }));
        auto* firstTrack = container.AddControl(new FractionalMeasureControl(Size{}));
        auto* secondTrack = container.AddControl(new FractionalMeasureControl(Size{}));
        spanning->GridColumnSpan = 2;
        spanning->Margin = Thickness(1.0f, 0.0f, 1.0f, 0.0f);
        firstTrack->GridColumn = 0;
        secondTrack->GridColumn = 1;
        firstTrack->HAlign = HorizontalAlignment::Stretch;
        secondTrack->HAlign = HorizontalAlignment::Stretch;

        GridLayoutEngine engine;
        engine.AddColumn(ColumnDefinition(GridLength::Auto()));
        engine.AddColumn(ColumnDefinition(GridLength::Auto()));
        engine.AddRow(RowDefinition(GridLength::Auto()));

        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints::Unbounded());
        CUI_EXPECT_NEAR(103.5f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(20.25f, desired.height, 0.0001f);

        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, desired.width, desired.height });
        CUI_EXPECT_NEAR(51.75f, firstTrack->GetActualSizeDip().width, 0.0001f);
        CUI_EXPECT_NEAR(51.75f, secondTrack->GetActualSizeDip().width, 0.0001f);
    });

    runner.Add("Grid resolves Star before growing mixed Auto spans", []
    {
        using namespace cui::core;

        Control container;
        auto* autoChild = container.AddControl(new FractionalMeasureControl(Size{ 20.0f, 10.0f }));
        auto* spanning = container.AddControl(new FractionalMeasureControl(Size{ 100.0f, 10.0f }));
        autoChild->GridColumn = 0;
        autoChild->HAlign = HorizontalAlignment::Stretch;
        spanning->GridColumnSpan = 2;

        GridLayoutEngine engine;
        engine.AddColumn(ColumnDefinition(GridLength::Auto()));
        engine.AddColumn(ColumnDefinition(GridLength::Star()));
        engine.AddRow(RowDefinition(GridLength::Pixels(10.0f)));

        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints{ Size{ 200.0f, 10.0f } });
        CUI_EXPECT_NEAR(200.0f, desired.width, 0.0001f);
        engine.Arrange(context, D2D1_RECT_F{ 0.0f, 0.0f, 200.0f, 10.0f });
        CUI_EXPECT_NEAR(20.0f, autoChild->GetActualSizeDip().width, 0.0001f);
    });

    runner.Add("Grid Auto rows measure content with resolved column width", []
    {
        using namespace cui::core;

        Control container;
        auto* child = container.AddControl(new WidthAwareMeasureControl());
        GridLayoutEngine engine;
        engine.AddColumn(ColumnDefinition(GridLength::Pixels(60.0f)));
        engine.AddRow(RowDefinition(GridLength::Auto()));

        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints::Unbounded());
        CUI_EXPECT_NEAR(60.0f, child->LastConstraints.maximum.width, 0.0001f);
        CUI_EXPECT_NEAR(40.0f, desired.height, 0.0001f);
    });

    runner.Add("Grid treats Percent as content sized on unbounded axes", []
    {
        using namespace cui::core;

        Control container;
        container.AddControl(new FractionalMeasureControl(Size{ 42.25f, 11.5f }));
        GridLayoutEngine engine;
        engine.AddColumn(ColumnDefinition(GridLength::Percent(50.0f)));
        engine.AddRow(RowDefinition(GridLength::Auto()));

        LayoutContext context(&container);
        const auto desired = engine.Measure(context, Constraints::Unbounded());
        CUI_EXPECT_NEAR(42.25f, desired.width, 0.0001f);
        CUI_EXPECT_NEAR(11.5f, desired.height, 0.0001f);
    });

    runner.Add("Grid cell hit testing uses half open track bounds", []
    {
        GridPanel panel(0, 0, 100, 20);
        panel.AddColumn(GridLength::Pixels(50.0f));
        panel.AddColumn(GridLength::Pixels(50.0f));
        panel.AddRow(GridLength::Pixels(20.0f));

        int row = -1;
        int column = -1;
        CUI_EXPECT_TRUE(panel.TryGetCellAtPoint(POINT{ 49, 10 }, row, column));
        CUI_EXPECT_EQ(0, column);
        CUI_EXPECT_TRUE(panel.TryGetCellAtPoint(POINT{ 50, 10 }, row, column));
        CUI_EXPECT_EQ(1, column);
        CUI_EXPECT_TRUE(panel.TryGetCellAtPoint(POINT{ 100, 10 }, row, column));
        CUI_EXPECT_EQ(1, column);
    });

    runner.Add("Out of order commits preserve dirty dependencies", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LayoutState state;
        state.CommitArrange(Rect{ 0.0f, 0.0f, 80.0f, 20.0f });
        state.CommitPaint();

        CUI_EXPECT_TRUE(state.NeedsMeasure());
        CUI_EXPECT_TRUE(state.NeedsArrange());
        CUI_EXPECT_TRUE(state.NeedsPaint());
    });

    runner.Add("Arrange state preserves fractional DIPs", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LayoutState state;
        state.CommitMeasure(Size{ 20.0f, 10.0f }, Constraints{ Size{ 100.0f, 100.0f } });
        state.CommitArrange(Rect{ 0.25f, 1.5f, 20.75f, 10.125f });

        CUI_EXPECT_NEAR(0.25f, state.arrangedRect.x, 0.0001f);
        CUI_EXPECT_NEAR(1.5f, state.arrangedRect.y, 0.0001f);
        CUI_EXPECT_NEAR(20.75f, state.arrangedRect.width, 0.0001f);
        CUI_EXPECT_NEAR(10.125f, state.arrangedRect.height, 0.0001f);
        CUI_EXPECT_FALSE(state.NeedsArrange());
    });

    runner.Add("Layout context supports new and legacy engines", []
    {
        auto* legacyContainer = reinterpret_cast<Control*>(0x1000);
        auto* hostForm = reinterpret_cast<Form*>(0x4000);
        Control* children[] = {
            reinterpret_cast<Control*>(0x2000),
            reinterpret_cast<Control*>(0x3000)
        };
        LayoutContext context(
            legacyContainer,
            std::span<Control* const>{ children, 2 },
            hostForm,
            true);

        CUI_EXPECT_EQ(2, context.ChildCount());
        CUI_EXPECT_EQ(children[0], context.ChildAt(0));
        CUI_EXPECT_EQ(children[1], context.ChildAt(1));
        CUI_EXPECT_EQ(nullptr, context.ChildAt(-1));
        CUI_EXPECT_EQ(nullptr, context.ChildAt(2));
        CUI_EXPECT_TRUE(context.IsWindowRoot());
        CUI_EXPECT_EQ(hostForm, context.HostForm());

        LegacyLayoutProbe legacyProbe;
        LayoutEngine& legacyEngine = legacyProbe;
        const SIZE legacyDesired = legacyEngine.Measure(context, SIZE{ 200, 100 });
        CUI_EXPECT_EQ(100L, legacyDesired.cx);
        CUI_EXPECT_EQ(50L, legacyDesired.cy);
        CUI_EXPECT_EQ(legacyContainer, legacyProbe.ReceivedContainer);

        ContextLayoutProbe contextProbe;
        LayoutEngine& contextEngine = contextProbe;
        const SIZE contextDesired = contextEngine.Measure(context, SIZE{ 200, 100 });
        CUI_EXPECT_EQ(200L, contextDesired.cx);
        CUI_EXPECT_EQ(100L, contextDesired.cy);
        CUI_EXPECT_TRUE(contextProbe.ReceivedWindowRoot);
        CUI_EXPECT_EQ(2, contextProbe.ReceivedChildCount);

        FractionalLayoutProbe fractionalProbe;
        LayoutEngine& fractionalEngine = fractionalProbe;
        const cui::core::Constraints fractionalAvailable{
            cui::core::Size{ 200.75f, 100.5f } };
        const auto fractionalDesired = fractionalEngine.Measure(context, fractionalAvailable);
        CUI_EXPECT_NEAR(17.625f, fractionalDesired.width, 0.0001f);
        CUI_EXPECT_NEAR(9.375f, fractionalDesired.height, 0.0001f);
        CUI_EXPECT_EQ(fractionalAvailable, fractionalProbe.ReceivedConstraints);

		class NonOwningLayoutHost final : public Control
		{
		public:
			std::vector<Control*> View;
			std::span<Control* const> GetLayoutChildrenView() noexcept override
			{
				return { View.data(), View.size() };
			}
		};
		Control externalChild;
		NonOwningLayoutHost viewHost;
		viewHost.View.push_back(&externalChild);
		LayoutContext projectedContext(&viewHost, true);
		CUI_EXPECT_EQ(1, projectedContext.ChildCount());
		CUI_EXPECT_TRUE(projectedContext.ChildAt(0) == &externalChild);
		CUI_EXPECT_TRUE(viewHost.Children.empty());
		CUI_EXPECT_TRUE(externalChild.Parent == nullptr);
    });

    runner.Add("Legacy Canvas preserves absolute Location semantics", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.location = { 30.0f, 40.0f };
        slot.margin = Insets{ 7.0f };

        const Rect result = ArrangeLegacyCanvasItem(
            Rect{ 10.0f, 20.0f, 300.0f, 200.0f },
            Size{ 80.0f, 25.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 40.0f, 60.0f, 80.0f, 25.0f }), result);
    });

    runner.Add("Legacy Canvas supports edge anchors", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.location = { 20.0f, 15.0f };
        slot.margin = Insets{ 0.0f, 0.0f, 12.0f, 8.0f };
        slot.anchorLeft = true;
        slot.anchorTop = true;
        slot.anchorRight = true;
        slot.anchorBottom = true;

        const Rect result = ArrangeLegacyCanvasItem(
            Rect{ 5.0f, 10.0f, 200.0f, 100.0f },
            Size{ 30.0f, 20.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 25.0f, 25.0f, 168.0f, 77.0f }), result);
    });

    runner.Add("Any legacy anchor disables alignment on both axes", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.location = { 17.0f, 9.0f };
        slot.horizontalAlignment = Alignment::Center;
        slot.verticalAlignment = Alignment::End;
        slot.anchorTop = true;

        const Rect result = ArrangeLegacyCanvasItem(
            Rect{ 5.0f, 10.0f, 200.0f, 100.0f },
            Size{ 30.0f, 20.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 22.0f, 19.0f, 30.0f, 20.0f }), result);
    });

    runner.Add("Unknown legacy anchor bits still select anchor mode", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.location = { 17.0f, 9.0f };
        slot.horizontalAlignment = Alignment::Center;
        slot.verticalAlignment = Alignment::Stretch;
        slot.useAnchorMode = true;

        const Rect result = ArrangeLegacyCanvasItem(
            Rect{ 5.0f, 10.0f, 200.0f, 100.0f },
            Size{ 30.0f, 20.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 22.0f, 19.0f, 30.0f, 20.0f }), result);
    });

    runner.Add("Legacy Canvas preserves Form status-bar anchor bounds", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.location = { 10.0f, 10.0f };
        slot.margin = Insets{ 0.0f, 0.0f, 5.0f, 5.0f };
        slot.useAnchorMode = true;
        slot.anchorTop = true;
        slot.anchorBottom = true;

        const Rect stretched = ArrangeLegacyCanvasItem(
            Rect{ 0.0f, 0.0f, 200.0f, 100.0f },
            Rect{ 0.0f, 0.0f, 200.0f, 80.0f },
            Size{ 30.0f, 20.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 10.0f, 10.0f, 30.0f, 65.0f }), stretched);

        slot.anchorTop = false;
        const Rect bottomOnly = ArrangeLegacyCanvasItem(
            Rect{ 0.0f, 0.0f, 200.0f, 100.0f },
            Rect{ 0.0f, 0.0f, 200.0f, 80.0f },
            Size{ 30.0f, 20.0f },
            slot);
        CUI_EXPECT_EQ((Rect{ 10.0f, 75.0f, 30.0f, 20.0f }), bottomOnly);
    });

    runner.Add("Legacy Canvas supports alignment and fractional DIPs", []
    {
        using namespace cui::core;
        using namespace cui::layout;

        LegacyCanvasSlot slot;
        slot.margin = Insets{ 3.0f, 2.0f, 5.0f, 4.0f };
        slot.horizontalAlignment = Alignment::Center;
        slot.verticalAlignment = Alignment::Stretch;

        const Rect result = ArrangeLegacyCanvasItem(
            Rect{ 0.5f, 1.5f, 101.0f, 50.0f },
            Size{ 20.0f, 10.0f },
            slot);
        CUI_EXPECT_NEAR(40.0f, result.x, 0.0001f);
        CUI_EXPECT_NEAR(3.5f, result.y, 0.0001f);
        CUI_EXPECT_NEAR(20.0f, result.width, 0.0001f);
        CUI_EXPECT_NEAR(44.0f, result.height, 0.0001f);
    });

	runner.Add("Custom XAML controls round-trip portable type metadata", []
	{
		const std::string xaml = R"(<Form xmlns="urn:cui"
			xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
			xmlns:d="urn:cui:designer"
			xmlns:local="urn:cui:tests"
			x:Name="CustomForm" Width="640" Height="480">
			<local:FancyButton x:Name="fancy" DesignId="41"
				d:CppType="Acme.Controls.FancyButton"
				d:Header="Controls/FancyButton.h"
				d:BaseType="Button" d:Constructor="Default"
				Text="Custom" Canvas.Left="13" Canvas.Top="17"
				Width="140" Height="36" />
		</Form>)";
		DesignerModel::DesignDocument document;
		std::wstring error;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			xaml, document, &error));
		CUI_EXPECT_EQ(1ULL, document.Nodes.size());
		const auto& node = document.Nodes.front();
		CUI_EXPECT_EQ(UIClass::UI_Button, node.Type);
		CUI_EXPECT_EQ(std::wstring(L"local"), node.CustomType.XamlPrefix);
		CUI_EXPECT_EQ(std::wstring(L"urn:cui:tests"),
			node.CustomType.XamlNamespace);
		CUI_EXPECT_EQ(std::wstring(L"Acme::Controls::FancyButton"),
			node.CustomType.CppType);
		CUI_EXPECT_EQ(DesignerCustomControlConstructor::Default,
			node.CustomType.Constructor);

		const auto canonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(document);
		CUI_EXPECT_TRUE(canonical.find("xmlns:local=\"urn:cui:tests\"")
			!= std::string::npos);
		CUI_EXPECT_TRUE(canonical.find("<local:FancyButton")
			!= std::string::npos);
		CUI_EXPECT_TRUE(canonical.find(
			"d:CppType=\"Acme::Controls::FancyButton\"") != std::string::npos);
		DesignerModel::DesignDocument fromXaml;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			canonical, fromXaml, &error));
		CUI_EXPECT_EQ(document, fromXaml);

		const auto xml = DesignerModel::DesignDocumentSerializer::ToXml(document);
		DesignerModel::DesignDocument fromXml;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			xml, fromXml, &error));
		CUI_EXPECT_EQ(document, fromXml);
	});

	runner.Add("Custom controls require registration at runtime and allow tool proxies", []
	{
		class FancyButton final : public Button
		{
		public:
			FancyButton() : Button(L"", 0, 0, 1, 1) {}
		};

		DesignerModel::DesignDocument document;
		document.Form.Name = L"CustomRuntimeForm";
		DesignerModel::DesignNode node;
		node.Id = 7;
		node.Name = L"fancy";
		node.Type = UIClass::UI_Button;
		node.CustomType = {
			L"local", L"FancyButton", L"urn:cui:tests",
			L"Acme::Controls::FancyButton", L"Controls/FancyButton.h",
			DesignerCustomControlConstructor::Default };
		node.Props["metadata"]["Text"] = {
			{ "kind", "String" }, { "value", "Registered" } };
		document.Nodes.push_back(node);
		document.NextStableId = 8;

		std::wstring error;
		DesignerModel::RuntimeDocument rejected;
		CUI_EXPECT_FALSE(DesignerModel::RuntimeDocumentLoader::Load(
			document, rejected, {}, &error));

		auto registry =
			std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
		CUI_EXPECT_TRUE(registry->Register(
			L"urn:cui:tests", L"FancyButton",
			[](const DesignerModel::DesignNode&)
			{ return std::make_unique<FancyButton>(); }, &error));
		DesignerModel::RuntimeDocumentLoadOptions registeredOptions;
		registeredOptions.CustomControls = registry;
		DesignerModel::RuntimeDocument runtime;
		CUI_EXPECT_TRUE(DesignerModel::RuntimeDocumentLoader::Load(
			document, runtime, registeredOptions, &error));
		CUI_EXPECT_TRUE(dynamic_cast<FancyButton*>(
			runtime.FindControlByDesignId(7)) != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Registered"),
			runtime.FindControlByDesignId(7)->Text);

		DesignerModel::RuntimeDocumentLoadOptions proxyOptions;
		proxyOptions.AllowCustomControlProxy = true;
		DesignerModel::RuntimeDocument proxy;
		CUI_EXPECT_TRUE(DesignerModel::RuntimeDocumentLoader::Load(
			document, proxy, proxyOptions, &error));
		CUI_EXPECT_TRUE(dynamic_cast<FancyButton*>(
			proxy.FindControlByDesignId(7)) == nullptr);
		CUI_EXPECT_EQ(std::wstring(L"Acme::Controls::FancyButton"),
			proxy.Controls().front()->CustomType.CppType);
	});

	runner.Add("Custom control code generation uses declared include type and constructor", []
	{
		DesignerModel::DesignDocument document;
		document.Form.Name = L"CustomCodeForm";
		DesignerModel::DesignNode node;
		node.Id = 9;
		node.Name = L"fancy";
		node.Type = UIClass::UI_Button;
		node.Order = 0;
		node.CustomType = {
			L"local", L"FancyButton", L"urn:cui:tests",
			L"Acme::Controls::FancyButton", L"Controls/FancyButton.h",
			DesignerCustomControlConstructor::Default };
		node.Props["location"] = { { "x", 11 }, { "y", 12 } };
		node.Props["size"] = { { "w", 130 }, { "h", 31 } };
		node.Props["metadata"]["Text"] = {
			{ "kind", "String" }, { "value", "Generated" } };
		node.Props["metadata"]["Severity"] = {
			{ "kind", "Int" }, { "value", "4" } };
		node.CustomEvents.push_back({
			L"OnSeverityInvoked", L"Severity invoked", "OnSeverityInvoked",
			DesignerEventCategory::Action,
			DesignerCustomEventSignature::SenderInt, 5, true });
		node.Events["OnSeverityInvoked"] = "HandleSeverityInvoked";
		document.Nodes.push_back(std::move(node));
		document.NextStableId = 10;

		CodeGenInput input;
		std::wstring error;
		const auto xml = DesignerModel::DesignDocumentSerializer::ToXml(document);
		DesignerModel::DesignDocument xmlRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			xml, xmlRoundTrip, &error));
		CUI_EXPECT_EQ(document, xmlRoundTrip);
		const auto xaml = DesignerModel::XamlDocumentSerializer::ToXaml(document);
		DesignerModel::DesignDocument xamlRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			xaml, xamlRoundTrip, &error));
		CUI_EXPECT_EQ(document, xamlRoundTrip);
		auto invalidXamlContract = xaml;
		const auto xamlSignature = invalidXamlContract.find(
			" Signature=\"SenderInt\"");
		CUI_EXPECT_TRUE(xamlSignature != std::string::npos);
		invalidXamlContract.insert(xamlSignature,
			" Unsupported=\"manifest-cpp\"");
		DesignerModel::DesignDocument rejectedXamlContract;
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			invalidXamlContract, rejectedXamlContract, &error));
		auto invalidXmlContract = xml;
		const auto xmlSignature = invalidXmlContract.find(
			" signature=\"SenderInt\"");
		CUI_EXPECT_TRUE(xmlSignature != std::string::npos);
		invalidXmlContract.insert(xmlSignature,
			" unsupported=\"manifest-cpp\"");
		DesignerModel::DesignDocument rejectedXmlContract;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			invalidXmlContract, rejectedXmlContract, &error));
		auto builtInContract = document;
		builtInContract.Nodes.front().CustomType = {};
		DesignerModel::DesignDocumentEventIndex rejectedBuiltInContract;
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentEventIndex::Build(
			builtInContract, rejectedBuiltInContract, &error));
		CUI_EXPECT_TRUE(
			DesignerModel::DesignDocumentCodeGenInputBuilder::Build(
				document, input, &error));
		CodeGenerator generator(L"CustomCodeForm", input);
		const auto header = generator.GenerateHeader();
		const auto cpp = generator.GenerateCpp();
		CUI_EXPECT_TRUE(header.find(
			"#include \"Controls/FancyButton.h\"") != std::string::npos);
		CUI_EXPECT_TRUE(header.find(
			"Acme::Controls::FancyButton* fancy") != std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"std::make_unique<Acme::Controls::FancyButton>()")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("fancy->Location = {11, 12};")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find("fancy->Size = {130, 31};")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"fancy->TrySetPropertyValue(L\"Severity\", BindingValue(4))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(header.find(
			"virtual void HandleSeverityInvoked(Control* sender, int value);")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"fancy->OnSeverityInvoked.Subscribe(std::bind_front(&CustomCodeFormGenerated::HandleSeverityInvoked, this))")
			!= std::string::npos);
	});

	runner.Add("Registered custom probes validate direct properties and canonicalize tool metadata", []
	{
		class PropertyBadge final : public Button
		{
		public:
			PropertyBadge() : Button(L"", 0, 0, 120, 30) {}
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
					ControlPropertyOptions<PropertyBadge, int> options;
					options.DefaultValue = 0;
					options.Design.Category = L"Custom";
					options.Design.Persistence =
						ControlPropertyPersistence::Metadata;
					BindingPropertyRegistry::Register<PropertyBadge, int>(
						L"Severity",
						[](PropertyBadge& target) { return target.Severity(); },
						[](PropertyBadge& target, const int& value)
						{ target.SetSeverity(value); }, {}, std::move(options));
					return true;
				}();
				(void)registered;
			}

		private:
			int _severity = 0;
		};

		auto registry =
			std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
		std::wstring error;
		CUI_EXPECT_TRUE(registry->Register(
			L"urn:cui:tests", L"PropertyBadge",
			[](const DesignerModel::DesignNode&)
			{ return std::make_unique<PropertyBadge>(); }, &error));
		DesignerModel::XamlDocumentParseOptions parseOptions;
		parseOptions.CustomControlFactory = [registry](
			const DesignerModel::DesignNode& node)
			{ return registry->Create(node); };
		const std::string xaml = R"(<Form xmlns="urn:cui"
			xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
			xmlns:d="urn:cui:designer" xmlns:local="urn:cui:tests"
			x:Name="PropertyForm">
			<local:PropertyBadge x:Name="badge" DesignId="51"
				d:CppType="Acme.Controls.PropertyBadge"
				d:Header="Controls/PropertyBadge.h" d:BaseType="Button"
				Severity="6" />
		</Form>)";
		DesignerModel::DesignDocument document;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			xaml, document, parseOptions, &error));
		CUI_EXPECT_EQ(std::string("6"), document.Nodes.front().Props
			["metadata"]["Severity"]["value"].get<std::string>());
		document.Nodes.front().Bindings["Severity"] = {
			{ "source", "View.Severity" },
			{ "mode", static_cast<int>(BindingMode::OneWay) },
			{ "updateMode", static_cast<int>(
				DataSourceUpdateMode::OnPropertyChanged) } };

		const auto canonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(document);
		CUI_EXPECT_TRUE(canonical.find("d:DesignProps") != std::string::npos);
		CUI_EXPECT_TRUE(canonical.find("d:DesignBindings") != std::string::npos);
		CUI_EXPECT_TRUE(canonical.find(" Severity=\"") == std::string::npos);
		DesignerModel::DesignDocument headless;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			canonical, headless, &error));
		CUI_EXPECT_EQ(document, headless);
		CodeGenInput input;
		CUI_EXPECT_TRUE(
			DesignerModel::DesignDocumentCodeGenInputBuilder::Build(
				headless, input, &error));
		const auto cpp = CodeGenerator(L"PropertyForm", input).GenerateCpp();
		CUI_EXPECT_TRUE(cpp.find(
			"badge->TrySetPropertyValue(L\"Severity\", BindingValue(6))")
			!= std::string::npos);
		CUI_EXPECT_TRUE(cpp.find(
			"badge->DataBindings.Add(L\"Severity\", dataContext, L\"View.Severity\"")
			!= std::string::npos);
	});

    return runner.RunAll();
}

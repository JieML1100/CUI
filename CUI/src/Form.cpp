#include "Form.h"
#include "NotifyIcon.h"
#include "DCompLayeredHost.h"
#include "Layout/LegacyCanvasAdapter.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <cwctype>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <new>
#include <oleidl.h>
#include <oleacc.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <uiautomationclient.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <windowsx.h>

#pragma comment(lib, "Oleacc.lib")
#pragma comment(lib, "Uiautomationcore.lib")

namespace accessibility_detail
{
	bool SupportsDefaultAction(AccessibleRole role)
	{
		switch (role)
		{
		case AccessibleRole::Button:
		case AccessibleRole::Link:
		case AccessibleRole::CheckBox:
		case AccessibleRole::RadioButton:
		case AccessibleRole::Switch:
			return true;
		default:
			return false;
		}
	}

	LONG ToMsaaRole(AccessibleRole role)
	{
		switch (role)
		{
		case AccessibleRole::Window: return ROLE_SYSTEM_WINDOW;
		case AccessibleRole::Pane: return ROLE_SYSTEM_PANE;
		case AccessibleRole::Group: return ROLE_SYSTEM_GROUPING;
		case AccessibleRole::Text: return ROLE_SYSTEM_STATICTEXT;
		case AccessibleRole::Link: return ROLE_SYSTEM_LINK;
		case AccessibleRole::Button: return ROLE_SYSTEM_PUSHBUTTON;
		case AccessibleRole::CheckBox:
		case AccessibleRole::Switch: return ROLE_SYSTEM_CHECKBUTTON;
		case AccessibleRole::RadioButton: return ROLE_SYSTEM_RADIOBUTTON;
		case AccessibleRole::TextBox:
		case AccessibleRole::PasswordBox: return ROLE_SYSTEM_TEXT;
		case AccessibleRole::ComboBox: return ROLE_SYSTEM_COMBOBOX;
		case AccessibleRole::List: return ROLE_SYSTEM_LIST;
		case AccessibleRole::ListItem: return ROLE_SYSTEM_LISTITEM;
		case AccessibleRole::Table: return ROLE_SYSTEM_TABLE;
		case AccessibleRole::Tree: return ROLE_SYSTEM_OUTLINE;
		case AccessibleRole::TreeItem: return ROLE_SYSTEM_OUTLINEITEM;
		case AccessibleRole::DataItem: return ROLE_SYSTEM_CELL;
		case AccessibleRole::HeaderItem: return ROLE_SYSTEM_COLUMNHEADER;
		case AccessibleRole::Tab: return ROLE_SYSTEM_PAGETABLIST;
		case AccessibleRole::TabItem: return ROLE_SYSTEM_PAGETAB;
		case AccessibleRole::Menu: return ROLE_SYSTEM_MENUPOPUP;
		case AccessibleRole::MenuItem: return ROLE_SYSTEM_MENUITEM;
		case AccessibleRole::ToolBar: return ROLE_SYSTEM_TOOLBAR;
		case AccessibleRole::StatusBar: return ROLE_SYSTEM_STATUSBAR;
		case AccessibleRole::Slider: return ROLE_SYSTEM_SLIDER;
		case AccessibleRole::ProgressBar: return ROLE_SYSTEM_PROGRESSBAR;
		case AccessibleRole::ScrollBar: return ROLE_SYSTEM_SCROLLBAR;
		case AccessibleRole::Image: return ROLE_SYSTEM_GRAPHIC;
		case AccessibleRole::Document: return ROLE_SYSTEM_DOCUMENT;
		default: return ROLE_SYSTEM_CLIENT;
		}
	}

	HRESULT CopyBstr(const std::wstring& value, BSTR* result)
	{
		if (!result) return E_POINTER;
		*result = nullptr;
		if (value.empty()) return S_FALSE;
		*result = ::SysAllocStringLen(value.data(), static_cast<UINT>(value.size()));
		return *result ? S_OK : E_OUTOFMEMORY;
	}

	std::wstring DefaultActionName(AccessibleRole role)
	{
		switch (role)
		{
		case AccessibleRole::Link: return L"Open";
		case AccessibleRole::CheckBox: return L"Check";
		case AccessibleRole::RadioButton: return L"Select";
		case AccessibleRole::Switch: return L"Toggle";
		default: return L"Press";
		}
	}

	HRESULT SetVariantString(VARIANT* value, const std::wstring& text)
	{
		if (!value) return E_POINTER;
		::VariantInit(value);
		value->vt = VT_BSTR;
		value->bstrVal = ::SysAllocStringLen(
			text.data(), static_cast<UINT>(text.size()));
		return value->bstrVal ? S_OK : E_OUTOFMEMORY;
	}

	void SetVariantBool(VARIANT* value, bool state)
	{
		::VariantInit(value);
		value->vt = VT_BOOL;
		value->boolVal = state ? VARIANT_TRUE : VARIANT_FALSE;
	}

	void SetVariantInt(VARIANT* value, int number)
	{
		::VariantInit(value);
		value->vt = VT_I4;
		value->lVal = number;
	}

	int ToUiaControlType(AccessibleRole role)
	{
		switch (role)
		{
		case AccessibleRole::Window: return UIA_WindowControlTypeId;
		case AccessibleRole::Pane: return UIA_PaneControlTypeId;
		case AccessibleRole::Group: return UIA_GroupControlTypeId;
		case AccessibleRole::Text: return UIA_TextControlTypeId;
		case AccessibleRole::Link: return UIA_HyperlinkControlTypeId;
		case AccessibleRole::Button: return UIA_ButtonControlTypeId;
		case AccessibleRole::CheckBox:
		case AccessibleRole::Switch: return UIA_CheckBoxControlTypeId;
		case AccessibleRole::RadioButton: return UIA_RadioButtonControlTypeId;
		case AccessibleRole::TextBox:
		case AccessibleRole::PasswordBox: return UIA_EditControlTypeId;
		case AccessibleRole::ComboBox: return UIA_ComboBoxControlTypeId;
		case AccessibleRole::List: return UIA_ListControlTypeId;
		case AccessibleRole::ListItem: return UIA_ListItemControlTypeId;
		case AccessibleRole::Table: return UIA_DataGridControlTypeId;
		case AccessibleRole::Tree: return UIA_TreeControlTypeId;
		case AccessibleRole::TreeItem: return UIA_TreeItemControlTypeId;
		case AccessibleRole::DataItem: return UIA_DataItemControlTypeId;
		case AccessibleRole::HeaderItem: return UIA_HeaderItemControlTypeId;
		case AccessibleRole::Tab: return UIA_TabControlTypeId;
		case AccessibleRole::TabItem: return UIA_TabItemControlTypeId;
		case AccessibleRole::Menu: return UIA_MenuControlTypeId;
		case AccessibleRole::MenuItem: return UIA_MenuItemControlTypeId;
		case AccessibleRole::ToolBar: return UIA_ToolBarControlTypeId;
		case AccessibleRole::StatusBar: return UIA_StatusBarControlTypeId;
		case AccessibleRole::Slider: return UIA_SliderControlTypeId;
		case AccessibleRole::ProgressBar: return UIA_ProgressBarControlTypeId;
		case AccessibleRole::ScrollBar: return UIA_ScrollBarControlTypeId;
		case AccessibleRole::Image: return UIA_ImageControlTypeId;
		case AccessibleRole::Document: return UIA_DocumentControlTypeId;
		default: return UIA_CustomControlTypeId;
		}
	}

	std::wstring UiaClassName(UIClass type)
	{
		switch (type)
		{
		case UIClass::UI_Label: return L"CUI.Label";
		case UIClass::UI_LinkLabel: return L"CUI.LinkLabel";
		case UIClass::UI_Button: return L"CUI.Button";
		case UIClass::UI_TextBox: return L"CUI.TextBox";
		case UIClass::UI_RichTextBox: return L"CUI.RichTextBox";
		case UIClass::UI_PasswordBox: return L"CUI.PasswordBox";
		case UIClass::UI_ComboBox: return L"CUI.ComboBox";
		case UIClass::UI_CheckBox: return L"CUI.CheckBox";
		case UIClass::UI_RadioBox: return L"CUI.RadioBox";
		case UIClass::UI_Switch: return L"CUI.Switch";
		case UIClass::UI_Slider: return L"CUI.Slider";
		case UIClass::UI_NumericUpDown: return L"CUI.NumericUpDown";
		case UIClass::UI_ProgressBar: return L"CUI.ProgressBar";
		case UIClass::UI_ProgressRing: return L"CUI.ProgressRing";
		case UIClass::UI_ListView: return L"CUI.ListView";
		case UIClass::UI_ListBox: return L"CUI.ListBox";
		case UIClass::UI_GridView: return L"CUI.GridView";
		case UIClass::UI_TreeView: return L"CUI.TreeView";
		case UIClass::UI_TabControl: return L"CUI.TabControl";
		case UIClass::UI_TabPage: return L"CUI.TabPage";
		case UIClass::UI_Expander: return L"CUI.Expander";
		case UIClass::UI_GroupBox: return L"CUI.GroupBox";
		case UIClass::UI_Panel: return L"CUI.Panel";
		default: return L"CUI.Control";
		}
	}
}

class FormAccessibleObject final : public IAccessible
{
public:
	explicit FormAccessibleObject(Form* form) : _form(form) {}

	void DetachForm() noexcept { _form = nullptr; }

	long ChildIdFor(Control* control) const
	{
		if (!_form || !control) return CHILDID_SELF;
		auto controls = _form->GetAccessibleControls();
		auto position = std::find(controls.begin(), controls.end(), control);
		return position == controls.end()
			? CHILDID_SELF
			: static_cast<long>(position - controls.begin()) + 1;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IAccessible)
			*object = static_cast<IAccessible*>(this);
		else
			return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++_references;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --_references;
		if (remaining == 0) delete this;
		return remaining;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override
	{
		if (!count) return E_POINTER;
		*count = 0;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD,
		DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override { return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** parent) override
	{
		if (!parent) return E_POINTER;
		*parent = nullptr;
		return Connected() ? S_FALSE : CO_E_OBJNOTCONNECTED;
	}

	HRESULT STDMETHODCALLTYPE get_accChildCount(long* count) override
	{
		if (!count) return E_POINTER;
		if (!Connected()) return CO_E_OBJNOTCONNECTED;
		const auto controls = _form->GetAccessibleControls();
		*count = controls.size() > static_cast<size_t>(LONG_MAX)
			? LONG_MAX : static_cast<long>(controls.size());
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accChild(VARIANT child, IDispatch** dispatch) override
	{
		if (!dispatch) return E_POINTER;
		*dispatch = nullptr;
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return S_FALSE; // Children are simple MSAA elements addressed by child id.
	}

	HRESULT STDMETHODCALLTYPE get_accName(VARIANT child, BSTR* name) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return accessibility_detail::CopyBstr(
			self ? static_cast<std::wstring>(_form->Text)
				: control->GetAccessibilitySnapshot().Name, name);
	}

	HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child, BSTR* value) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return accessibility_detail::CopyBstr(
			self ? std::wstring{} : control->GetAccessibilitySnapshot().Value, value);
	}

	HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT child, BSTR* description) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return accessibility_detail::CopyBstr(self ? std::wstring{}
			: control->GetAccessibilitySnapshot().Description, description);
	}

	HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child, VARIANT* role) override
	{
		if (!role) return E_POINTER;
		::VariantInit(role);
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		role->vt = VT_I4;
		role->lVal = self ? ROLE_SYSTEM_CLIENT
			: accessibility_detail::ToMsaaRole(
				control->GetAccessibilitySnapshot().Role);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accState(VARIANT child, VARIANT* state) override
	{
		if (!state) return E_POINTER;
		::VariantInit(state);
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		LONG flags = 0;
		if (self)
		{
			if (!::IsWindowEnabled(_form->Handle)) flags |= STATE_SYSTEM_UNAVAILABLE;
			if (!::IsWindowVisible(_form->Handle)) flags |= STATE_SYSTEM_INVISIBLE;
			if (::GetFocus() == _form->Handle) flags |= STATE_SYSTEM_FOCUSED;
		}
		else
		{
			const auto snapshot = control->GetAccessibilitySnapshot();
			if (!snapshot.Enabled) flags |= STATE_SYSTEM_UNAVAILABLE;
			if (!snapshot.Visible || !::IsWindowVisible(_form->Handle))
				flags |= STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN;
			if (snapshot.Focusable) flags |= STATE_SYSTEM_FOCUSABLE;
			if (snapshot.Focused) flags |= STATE_SYSTEM_FOCUSED;
			if (snapshot.Selected) flags |= STATE_SYSTEM_SELECTED;
			if (snapshot.Checked) flags |= STATE_SYSTEM_CHECKED;
			if (snapshot.Password) flags |= STATE_SYSTEM_PROTECTED;
			if (snapshot.ReadOnly) flags |= STATE_SYSTEM_READONLY;
			if (snapshot.Role == AccessibleRole::ComboBox) flags |= STATE_SYSTEM_HASPOPUP;
		}
		state->vt = VT_I4;
		state->lVal = flags;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT child, BSTR* help) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return accessibility_detail::CopyBstr(self ? std::wstring{}
			: control->GetAccessibilitySnapshot().HelpText, help);
	}

	HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* helpFile, VARIANT, long* topic) override
	{
		if (!helpFile || !topic) return E_POINTER;
		*helpFile = nullptr;
		*topic = 0;
		return Connected() ? S_FALSE : CO_E_OBJNOTCONNECTED;
	}

	HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(
		VARIANT child, BSTR* shortcut) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		return accessibility_detail::CopyBstr(self ? std::wstring{}
			: control->GetAccessibilitySnapshot().KeyboardShortcut, shortcut);
	}

	HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* child) override
	{
		if (!child) return E_POINTER;
		::VariantInit(child);
		if (!Connected()) return CO_E_OBJNOTCONNECTED;
		const long id = ChildIdFor(_form->Selected);
		if (id == CHILDID_SELF && ::GetFocus() != _form->Handle) return S_FALSE;
		child->vt = VT_I4;
		child->lVal = id;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* children) override
	{
		if (!children) return E_POINTER;
		::VariantInit(children);
		if (!Connected()) return CO_E_OBJNOTCONNECTED;
		const long id = ChildIdFor(_form->Selected);
		if (id == CHILDID_SELF) return S_FALSE;
		children->vt = VT_I4;
		children->lVal = id;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child, BSTR* action) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		if (self) return accessibility_detail::CopyBstr({}, action);
		const auto role = control->GetAccessibilitySnapshot().Role;
		return accessibility_detail::SupportsDefaultAction(role)
			? accessibility_detail::CopyBstr(
				accessibility_detail::DefaultActionName(role), action)
			: accessibility_detail::CopyBstr({}, action);
	}

	HRESULT STDMETHODCALLTYPE accSelect(long flags, VARIANT child) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		if (self)
		{
			if ((flags & SELFLAG_TAKEFOCUS) && _form->Handle)
				::SetFocus(_form->Handle);
			return S_OK;
		}
		if (flags & SELFLAG_REMOVESELECTION)
		{
			if (_form->Selected == control) _form->SetSelectedControl(nullptr, true);
			return S_OK;
		}
		if (flags & (SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION | SELFLAG_ADDSELECTION))
			return control->Focus() ? S_OK : S_FALSE;
		return S_FALSE;
	}

	HRESULT STDMETHODCALLTYPE accLocation(long* left, long* top, long* width,
		long* height, VARIANT child) override
	{
		if (!left || !top || !width || !height) return E_POINTER;
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		RECT rectangle{};
		if (self)
		{
			if (!::GetClientRect(_form->Handle, &rectangle)) return HRESULT_FROM_WIN32(::GetLastError());
		}
		else
		{
			rectangle = _form->ContentDipRectToClientPixels(control->AbsRect);
		}
		POINT points[2]{ { rectangle.left, rectangle.top }, { rectangle.right, rectangle.bottom } };
		::MapWindowPoints(_form->Handle, nullptr, points, 2);
		*left = points[0].x;
		*top = points[0].y;
		*width = points[1].x - points[0].x;
		*height = points[1].y - points[0].y;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT start,
		VARIANT* destination) override
	{
		if (!destination) return E_POINTER;
		::VariantInit(destination);
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(start, self, control);
		if (FAILED(resolved)) return resolved;
		auto controls = _form->GetAccessibleControls();
		long id = self ? CHILDID_SELF : ChildIdFor(control);
		if (self && direction == NAVDIR_FIRSTCHILD && !controls.empty()) id = 1;
		else if (self && direction == NAVDIR_LASTCHILD && !controls.empty())
			id = static_cast<long>(controls.size());
		else if (!self && (direction == NAVDIR_NEXT || direction == NAVDIR_DOWN || direction == NAVDIR_RIGHT)
			&& id < static_cast<long>(controls.size())) ++id;
		else if (!self && (direction == NAVDIR_PREVIOUS || direction == NAVDIR_UP || direction == NAVDIR_LEFT)
			&& id > 1) --id;
		else return S_FALSE;
		destination->vt = VT_I4;
		destination->lVal = id;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, VARIANT* child) override
	{
		if (!child) return E_POINTER;
		::VariantInit(child);
		if (!Connected()) return CO_E_OBJNOTCONNECTED;
		auto controls = _form->GetAccessibleControls();
		for (size_t index = controls.size(); index > 0; --index)
		{
			Control* control = controls[index - 1];
			if (!control) continue;
			VARIANT id{};
			id.vt = VT_I4;
			id.lVal = static_cast<long>(index);
			long left = 0, top = 0, width = 0, height = 0;
			if (SUCCEEDED(accLocation(&left, &top, &width, &height, id))
				&& x >= left && y >= top && x < left + width && y < top + height)
			{
				child->vt = VT_I4;
				child->lVal = static_cast<long>(index);
				return S_OK;
			}
		}
		VARIANT self{};
		self.vt = VT_I4;
		self.lVal = CHILDID_SELF;
		long left = 0, top = 0, width = 0, height = 0;
		if (SUCCEEDED(accLocation(&left, &top, &width, &height, self))
			&& x >= left && y >= top && x < left + width && y < top + height)
		{
			child->vt = VT_I4;
			child->lVal = CHILDID_SELF;
			return S_OK;
		}
		return S_FALSE;
	}

	HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		if (self || !accessibility_detail::SupportsDefaultAction(
			control->GetAccessibilitySnapshot().Role)) return S_FALSE;
		return control->Invoke() ? S_OK : S_FALSE;
	}

	HRESULT STDMETHODCALLTYPE put_accName(VARIANT child, BSTR name) override
	{
		bool self = false;
		Control* control = nullptr;
		const HRESULT resolved = Resolve(child, self, control);
		if (FAILED(resolved)) return resolved;
		if (self) return E_NOTIMPL;
		control->AccessibleName = name ? name : L"";
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, BSTR) override
	{
		return Connected() ? E_NOTIMPL : CO_E_OBJNOTCONNECTED;
	}

private:
	bool Connected() const noexcept
	{
		return _form && _form->Handle && ::IsWindow(_form->Handle);
	}

	HRESULT Resolve(VARIANT child, bool& self, Control*& control) const
	{
		self = false;
		control = nullptr;
		if (!Connected()) return CO_E_OBJNOTCONNECTED;
		if (child.vt != VT_I4) return E_INVALIDARG;
		if (child.lVal == CHILDID_SELF)
		{
			self = true;
			return S_OK;
		}
		auto controls = _form->GetAccessibleControls();
		if (child.lVal < 1 || static_cast<size_t>(child.lVal) > controls.size())
			return E_INVALIDARG;
		control = controls[static_cast<size_t>(child.lVal - 1)];
		return control ? S_OK : E_INVALIDARG;
	}

	std::atomic<ULONG> _references{ 1 };
	Form* _form = nullptr;
};

class ControlUiaProvider;
class VirtualUiaProvider;

class FormUiaProvider final :
	public IRawElementProviderSimple,
	public IRawElementProviderFragment,
	public IRawElementProviderFragmentRoot
{
public:
	explicit FormUiaProvider(Form* form) : _form(form) {}

	void DetachForm() noexcept { _form.store(nullptr, std::memory_order_release); }
	bool Connected() const noexcept
	{
		auto* form = _form.load(std::memory_order_acquire);
		return form && form->Handle && ::IsWindow(form->Handle);
	}
	Form* GetForm() const noexcept
	{
		return _form.load(std::memory_order_acquire);
	}
	Control* ResolveControl(Control* candidate, uint32_t runtimeId) const;
	ControlUiaProvider* ProviderFor(Control* control);
	VirtualUiaProvider* VirtualProviderFor(Control* owner, uint32_t virtualId);
	void UnregisterProvider(uint32_t runtimeId, ControlUiaProvider* provider);
	void UnregisterVirtualProvider(
		uint64_t key, VirtualUiaProvider* provider);
	std::vector<Control*> DirectChildren(Control* parent) const;
	bool TryGetVirtualBoundaryChild(Control* owner, uint32_t parentId,
		bool last, uint32_t& result) const;
	bool TryGetVirtualSibling(Control* owner, uint32_t parentId,
		uint32_t id, bool next, uint32_t& result) const;
	bool ResolveVirtualNode(Control* owner, uint32_t ownerRuntimeId,
		uint32_t virtualId, AccessibilityVirtualNode& node,
		IAccessibilityVirtualizedControl** source = nullptr) const;
	Control* ParentOf(Control* control) const;
	Control* SiblingOf(Control* control, bool next) const;
	void RaiseEvent(Control* control, AccessibilityChange change);
	void RaiseVirtualEvent(
		Control* owner, uint32_t virtualId, AccessibilityChange change);
	void SetVirtualFocus(Control* owner, uint32_t virtualId);
	bool IsVirtualFocused(Control* owner, uint32_t virtualId);
	uint32_t VirtualFocusFor(Control* owner);

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == IID_IUnknown || iid == IID_IRawElementProviderSimple)
			*object = static_cast<IRawElementProviderSimple*>(this);
		else if (iid == IID_IRawElementProviderFragment)
			*object = static_cast<IRawElementProviderFragment*>(this);
		else if (iid == IID_IRawElementProviderFragmentRoot)
			*object = static_cast<IRawElementProviderFragmentRoot*>(this);
		else
			return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++_references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --_references;
		if (remaining == 0) delete this;
		return remaining;
	}

	HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override
	{
		if (!value) return E_POINTER;
		*value = ProviderOptions_ServerSideProvider;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return Connected() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* value) override
	{
		if (!value) return E_POINTER;
		::VariantInit(value);
		auto* form = GetForm();
		if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
		switch (propertyId)
		{
		case UIA_NamePropertyId:
			return accessibility_detail::SetVariantString(value,
				static_cast<std::wstring>(form->Text));
		case UIA_ControlTypePropertyId:
			accessibility_detail::SetVariantInt(value, UIA_WindowControlTypeId);
			return S_OK;
		case UIA_ClassNamePropertyId:
			return accessibility_detail::SetVariantString(value, L"CUI.Form");
		case UIA_FrameworkIdPropertyId:
			return accessibility_detail::SetVariantString(value, L"CUI");
		case UIA_ProviderDescriptionPropertyId:
			return accessibility_detail::SetVariantString(
				value, L"CUI native UI Automation provider");
		case UIA_ProcessIdPropertyId:
			accessibility_detail::SetVariantInt(
				value, static_cast<int>(::GetCurrentProcessId()));
			return S_OK;
		case UIA_NativeWindowHandlePropertyId:
			accessibility_detail::SetVariantInt(
				value, static_cast<int>(reinterpret_cast<INT_PTR>(form->Handle)));
			return S_OK;
		case UIA_IsEnabledPropertyId:
			accessibility_detail::SetVariantBool(value, form->Enable);
			return S_OK;
		case UIA_IsKeyboardFocusablePropertyId:
			accessibility_detail::SetVariantBool(value, true);
			return S_OK;
		case UIA_HasKeyboardFocusPropertyId:
			accessibility_detail::SetVariantBool(value, ::GetFocus() == form->Handle);
			return S_OK;
		case UIA_IsOffscreenPropertyId:
			accessibility_detail::SetVariantBool(
				value, !form->Visible || !::IsWindowVisible(form->Handle));
			return S_OK;
		case UIA_IsControlElementPropertyId:
		case UIA_IsContentElementPropertyId:
			accessibility_detail::SetVariantBool(value, true);
			return S_OK;
		default:
			return S_OK;
		}
	}
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		auto* form = GetForm();
		if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
		return ::UiaHostProviderFromHwnd(form->Handle, value);
	}

	HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
		IRawElementProviderFragment** value) override;
	HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return Connected() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* value) override
	{
		if (!value) return E_POINTER;
		*value = UiaRect{};
		auto* form = GetForm();
		if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
		RECT rectangle{};
		if (!::GetClientRect(form->Handle, &rectangle))
			return HRESULT_FROM_WIN32(::GetLastError());
		POINT points[2]{ { rectangle.left, rectangle.top },
			{ rectangle.right, rectangle.bottom } };
		::MapWindowPoints(form->Handle, nullptr, points, 2);
		value->left = points[0].x;
		value->top = points[0].y;
		value->width = points[1].x - points[0].x;
		value->height = points[1].y - points[0].y;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return Connected() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE SetFocus() override
	{
		auto* form = GetForm();
		if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
		(void)::SetFocus(form->Handle);
		return ::GetFocus() == form->Handle
			? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_FragmentRoot(
		IRawElementProviderFragmentRoot** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
		*value = static_cast<IRawElementProviderFragmentRoot*>(this);
		AddRef();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y,
		IRawElementProviderFragment** value) override;
	HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** value) override;

private:
	~FormUiaProvider() = default;
	std::atomic<ULONG> _references{ 1 };
	std::atomic<Form*> _form{ nullptr };
	std::mutex _providerMutex;
	std::unordered_map<uint32_t, ControlUiaProvider*> _providers;
	std::unordered_map<uint64_t, VirtualUiaProvider*> _virtualProviders;
	std::unordered_map<uint32_t, uint32_t> _focusedVirtualByOwner;
};

class ControlUiaProvider final :
	public IRawElementProviderSimple,
	public IRawElementProviderFragment,
	public IInvokeProvider,
	public IToggleProvider,
	public IValueProvider,
	public IRangeValueProvider,
	public IExpandCollapseProvider,
	public ISelectionItemProvider,
	public ISelectionProvider,
	public IGridProvider,
	public ITableProvider,
	public IScrollProvider
{
public:
	ControlUiaProvider(FormUiaProvider* root, Control* control) :
		_root(root), _control(control),
		_runtimeId(control ? control->GetAccessibilityRuntimeId() : 0),
		_type(control ? control->Type() : UIClass::UI_Base),
		_role(control ? control->GetEffectiveAccessibleRole()
			: AccessibleRole::Custom)
	{
		if (_root) _root->AddRef();
	}

	uint32_t RuntimeId() const noexcept { return _runtimeId; }
	int GetAccessibilityTypeForEvent() const noexcept
	{
		return SupportsRange() ? 1 : 0;
	}
	bool SupportsToggleForEvent() const noexcept { return SupportsToggle(); }
	bool SupportsSelectionItemForEvent() const noexcept
	{
		return SupportsSelectionItem();
	}
	bool Matches(Control* control) const noexcept
	{
		return control == _control && control
			&& control->GetAccessibilityRuntimeId() == _runtimeId;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == IID_IUnknown || iid == IID_IRawElementProviderSimple)
			*object = static_cast<IRawElementProviderSimple*>(this);
		else if (iid == IID_IRawElementProviderFragment)
			*object = static_cast<IRawElementProviderFragment*>(this);
		else if (iid == IID_IInvokeProvider && SupportsInvoke())
			*object = static_cast<IInvokeProvider*>(this);
		else if (iid == IID_IToggleProvider && SupportsToggle())
			*object = static_cast<IToggleProvider*>(this);
		else if (iid == IID_IValueProvider && SupportsValue())
			*object = static_cast<IValueProvider*>(this);
		else if (iid == IID_IRangeValueProvider && SupportsRange())
			*object = static_cast<IRangeValueProvider*>(this);
		else if (iid == IID_IExpandCollapseProvider && SupportsExpandCollapse())
			*object = static_cast<IExpandCollapseProvider*>(this);
		else if (iid == IID_ISelectionItemProvider && SupportsSelectionItem())
			*object = static_cast<ISelectionItemProvider*>(this);
		else if (iid == IID_ISelectionProvider && SupportsSelection())
			*object = static_cast<ISelectionProvider*>(this);
		else if (iid == IID_IGridProvider && SupportsGrid())
			*object = static_cast<IGridProvider*>(this);
		else if (iid == IID_ITableProvider && SupportsTable())
			*object = static_cast<ITableProvider*>(this);
		else if (iid == IID_IScrollProvider && SupportsScroll())
			*object = static_cast<IScrollProvider*>(this);
		else
			return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++_references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --_references;
		if (remaining == 0) delete this;
		return remaining;
	}

	HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override
	{
		if (!value) return E_POINTER;
		*value = ProviderOptions_ServerSideProvider;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,
		IUnknown** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
		auto queryPattern = [this, value](REFIID iid)
		{
			const HRESULT result = QueryInterface(
				iid, reinterpret_cast<void**>(value));
			return result == E_NOINTERFACE ? S_OK : result;
		};
		if (patternId == UIA_InvokePatternId)
			return queryPattern(IID_IInvokeProvider);
		if (patternId == UIA_TogglePatternId)
			return queryPattern(IID_IToggleProvider);
		if (patternId == UIA_ValuePatternId)
			return queryPattern(IID_IValueProvider);
		if (patternId == UIA_RangeValuePatternId)
			return queryPattern(IID_IRangeValueProvider);
		if (patternId == UIA_ExpandCollapsePatternId)
			return queryPattern(IID_IExpandCollapseProvider);
		if (patternId == UIA_SelectionItemPatternId)
			return queryPattern(IID_ISelectionItemProvider);
		if (patternId == UIA_SelectionPatternId)
			return queryPattern(IID_ISelectionProvider);
		if (patternId == UIA_GridPatternId)
			return queryPattern(IID_IGridProvider);
		if (patternId == UIA_TablePatternId)
			return queryPattern(IID_ITableProvider);
		if (patternId == UIA_ScrollPatternId)
			return queryPattern(IID_IScrollProvider);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,
		VARIANT* value) override
	{
		if (!value) return E_POINTER;
		::VariantInit(value);
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		const auto snapshot = control->GetAccessibilitySnapshot();
		switch (propertyId)
		{
		case UIA_NamePropertyId:
			return accessibility_detail::SetVariantString(value, snapshot.Name);
		case UIA_AutomationIdPropertyId:
			return accessibility_detail::SetVariantString(value, snapshot.AutomationId);
		case UIA_ControlTypePropertyId:
			accessibility_detail::SetVariantInt(value,
				accessibility_detail::ToUiaControlType(snapshot.Role));
			return S_OK;
		case UIA_ClassNamePropertyId:
			return accessibility_detail::SetVariantString(
				value, accessibility_detail::UiaClassName(_type));
		case UIA_FrameworkIdPropertyId:
			return accessibility_detail::SetVariantString(value, L"CUI");
		case UIA_ProviderDescriptionPropertyId:
			return accessibility_detail::SetVariantString(
				value, L"CUI native control provider");
		case UIA_HelpTextPropertyId:
			return accessibility_detail::SetVariantString(value, snapshot.HelpText);
		case UIA_FullDescriptionPropertyId:
			return accessibility_detail::SetVariantString(value, snapshot.Description);
		case UIA_AcceleratorKeyPropertyId:
		case UIA_AccessKeyPropertyId:
			return accessibility_detail::SetVariantString(
				value, snapshot.KeyboardShortcut);
		case UIA_ProcessIdPropertyId:
			accessibility_detail::SetVariantInt(
				value, static_cast<int>(::GetCurrentProcessId()));
			return S_OK;
		case UIA_IsEnabledPropertyId:
			accessibility_detail::SetVariantBool(value, snapshot.Enabled);
			return S_OK;
		case UIA_IsKeyboardFocusablePropertyId:
			accessibility_detail::SetVariantBool(value, snapshot.Focusable);
			return S_OK;
		case UIA_HasKeyboardFocusPropertyId:
			accessibility_detail::SetVariantBool(value, snapshot.Focused);
			return S_OK;
		case UIA_IsOffscreenPropertyId:
			accessibility_detail::SetVariantBool(value,
				!snapshot.Visible || !IsOnscreen(control));
			return S_OK;
		case UIA_IsPasswordPropertyId:
			accessibility_detail::SetVariantBool(value, snapshot.Password);
			return S_OK;
		case UIA_IsControlElementPropertyId:
		case UIA_IsContentElementPropertyId:
			accessibility_detail::SetVariantBool(value, true);
			return S_OK;
		case UIA_IsInvokePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsInvoke()); return S_OK;
		case UIA_IsTogglePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsToggle()); return S_OK;
		case UIA_IsValuePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsValue()); return S_OK;
		case UIA_IsRangeValuePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsRange()); return S_OK;
		case UIA_IsExpandCollapsePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsExpandCollapse()); return S_OK;
		case UIA_IsSelectionItemPatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsSelectionItem()); return S_OK;
		case UIA_IsSelectionPatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsSelection()); return S_OK;
		case UIA_IsGridPatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsGrid()); return S_OK;
		case UIA_IsTablePatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsTable()); return S_OK;
		case UIA_IsScrollPatternAvailablePropertyId:
			accessibility_detail::SetVariantBool(value, SupportsScroll()); return S_OK;
		case UIA_ScrollHorizontalScrollPercentPropertyId:
			if (!SupportsScroll()) return S_OK;
			value->vt = VT_R8;
			return get_HorizontalScrollPercent(&value->dblVal);
		case UIA_ScrollHorizontalViewSizePropertyId:
			if (!SupportsScroll()) return S_OK;
			value->vt = VT_R8;
			return get_HorizontalViewSize(&value->dblVal);
		case UIA_ScrollVerticalScrollPercentPropertyId:
			if (!SupportsScroll()) return S_OK;
			value->vt = VT_R8;
			return get_VerticalScrollPercent(&value->dblVal);
		case UIA_ScrollVerticalViewSizePropertyId:
			if (!SupportsScroll()) return S_OK;
			value->vt = VT_R8;
			return get_VerticalViewSize(&value->dblVal);
		case UIA_ScrollHorizontallyScrollablePropertyId:
			if (!SupportsScroll()) return S_OK;
			{
				BOOL scrollable = FALSE;
				const HRESULT result = get_HorizontallyScrollable(&scrollable);
				if (SUCCEEDED(result)) accessibility_detail::SetVariantBool(
					value, scrollable != FALSE);
				return result;
			}
		case UIA_ScrollVerticallyScrollablePropertyId:
			if (!SupportsScroll()) return S_OK;
			{
				BOOL scrollable = FALSE;
				const HRESULT result = get_VerticallyScrollable(&scrollable);
				if (SUCCEEDED(result)) accessibility_detail::SetVariantBool(
					value, scrollable != FALSE);
				return result;
			}
		case UIA_ValueValuePropertyId:
			if (!SupportsValue()) return S_OK;
			return accessibility_detail::SetVariantString(value,
				snapshot.Password ? std::wstring{} : snapshot.Value);
		case UIA_ValueIsReadOnlyPropertyId:
			if (!SupportsValue()) return S_OK;
			accessibility_detail::SetVariantBool(value, snapshot.ReadOnly); return S_OK;
		case UIA_RangeValueValuePropertyId:
			if (!SupportsRange()) return S_OK;
			value->vt = VT_R8;
			return get_Value(&value->dblVal);
		case UIA_RangeValueMinimumPropertyId:
			if (!SupportsRange()) return S_OK;
			value->vt = VT_R8;
			return RangeMinimum(&value->dblVal);
		case UIA_RangeValueMaximumPropertyId:
			if (!SupportsRange()) return S_OK;
			value->vt = VT_R8;
			return RangeMaximum(&value->dblVal);
		case UIA_RangeValueSmallChangePropertyId:
			if (!SupportsRange()) return S_OK;
			value->vt = VT_R8;
			return get_SmallChange(&value->dblVal);
		case UIA_RangeValueLargeChangePropertyId:
			if (!SupportsRange()) return S_OK;
			value->vt = VT_R8;
			return get_LargeChange(&value->dblVal);
		case UIA_RangeValueIsReadOnlyPropertyId:
			if (!SupportsRange()) return S_OK;
			{
				BOOL readOnly = FALSE;
				(void)RangeIsReadOnly(&readOnly);
				accessibility_detail::SetVariantBool(value, readOnly != FALSE);
			}
			return S_OK;
		case UIA_ToggleToggleStatePropertyId:
			if (!SupportsToggle()) return S_OK;
			accessibility_detail::SetVariantInt(value,
				control->Checked ? ToggleState_On : ToggleState_Off);
			return S_OK;
		case UIA_ExpandCollapseExpandCollapseStatePropertyId:
			if (!SupportsExpandCollapse()) return S_OK;
			{
				ExpandCollapseState state = ExpandCollapseState_LeafNode;
				(void)get_ExpandCollapseState(&state);
				accessibility_detail::SetVariantInt(value, state);
			}
			return S_OK;
		case UIA_SelectionItemIsSelectedPropertyId:
			if (!SupportsSelectionItem()) return S_OK;
			{
				BOOL selected = FALSE;
				(void)get_IsSelected(&selected);
				accessibility_detail::SetVariantBool(value, selected != FALSE);
			}
			return S_OK;
		default:
			return S_OK;
		}
	}
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return CurrentControl() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}

	HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
		IRawElementProviderFragment** value) override;
	HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
		SAFEARRAY* result = ::SafeArrayCreateVector(VT_I4, 0, 2);
		if (!result) return E_OUTOFMEMORY;
		LONG first = 0;
		LONG second = 1;
		int append = UiaAppendRuntimeId;
		int id = static_cast<int>(_runtimeId);
		HRESULT hr = ::SafeArrayPutElement(result, &first, &append);
		if (SUCCEEDED(hr)) hr = ::SafeArrayPutElement(result, &second, &id);
		if (FAILED(hr))
		{
			::SafeArrayDestroy(result);
			return hr;
		}
		*value = result;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* value) override
	{
		if (!value) return E_POINTER;
		*value = UiaRect{};
		auto* control = CurrentControl();
		auto* form = _root ? _root->GetForm() : nullptr;
		if (!control || !form) return UIA_E_ELEMENTNOTAVAILABLE;
		RECT rectangle = form->ContentDipRectToClientPixels(control->AbsRect);
		POINT points[2]{ { rectangle.left, rectangle.top },
			{ rectangle.right, rectangle.bottom } };
		::MapWindowPoints(form->Handle, nullptr, points, 2);
		value->left = points[0].x;
		value->top = points[0].y;
		value->width = points[1].x - points[0].x;
		value->height = points[1].y - points[0].y;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return CurrentControl() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE SetFocus() override
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		return control->Focus() ? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_FragmentRoot(
		IRawElementProviderFragmentRoot** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentControl() || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
		*value = static_cast<IRawElementProviderFragmentRoot*>(_root);
		_root->AddRef();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Invoke() override
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsInvoke()) return UIA_E_NOTSUPPORTED;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		return control->Invoke() ? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE Toggle() override
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsToggle()) return UIA_E_NOTSUPPORTED;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		return control->Invoke() ? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsToggle()) return UIA_E_NOTSUPPORTED;
		*value = control->Checked ? ToggleState_On : ToggleState_Off;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsValue()) return UIA_E_NOTSUPPORTED;
		const auto snapshot = control->GetAccessibilitySnapshot();
		if (!snapshot.Enabled) return UIA_E_ELEMENTNOTENABLED;
		if (snapshot.ReadOnly) return UIA_E_INVALIDOPERATION;
		return control->TrySetCurrentPropertyValue(
			L"Text", BindingValue(value ? value : L""))
			? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_Value(BSTR* value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsValue()) return UIA_E_NOTSUPPORTED;
		const auto snapshot = control->GetAccessibilitySnapshot();
		const std::wstring exposed = snapshot.Password
			? std::wstring{} : snapshot.Value;
		*value = ::SysAllocStringLen(
			exposed.data(), static_cast<UINT>(exposed.size()));
		return *value ? S_OK : E_OUTOFMEMORY;
	}
	HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (SupportsValue())
		{
			*value = control->GetAccessibilitySnapshot().ReadOnly ? TRUE : FALSE;
			return S_OK;
		}
		return RangeIsReadOnly(value);
	}

	HRESULT STDMETHODCALLTYPE SetValue(double value) override
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsRange()) return UIA_E_NOTSUPPORTED;
		if (!std::isfinite(value)) return E_INVALIDARG;
		BOOL readOnly = TRUE;
		HRESULT hr = RangeIsReadOnly(&readOnly);
		if (FAILED(hr)) return hr;
		if (readOnly) return UIA_E_INVALIDOPERATION;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		double minimum = 0.0, maximum = 0.0;
		if (FAILED(RangeMinimum(&minimum)) || FAILED(RangeMaximum(&maximum)))
			return UIA_E_INVALIDOPERATION;
		if (value < minimum || value > maximum) return E_INVALIDARG;
		if (auto* slider = dynamic_cast<Slider*>(control))
			return slider->TrySetCurrentPropertyValue(
				L"Value", BindingValue(static_cast<float>(value)))
				? S_OK : UIA_E_INVALIDOPERATION;
		if (auto* numeric = dynamic_cast<NumericUpDown*>(control))
			return numeric->TrySetCurrentPropertyValue(L"Value", BindingValue(value))
				? S_OK : UIA_E_INVALIDOPERATION;
		return UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_Value(double* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* slider = dynamic_cast<Slider*>(control)) *value = slider->Value;
		else if (auto* numeric = dynamic_cast<NumericUpDown*>(control)) *value = numeric->Value;
		else if (auto* progress = dynamic_cast<ProgressBar*>(control)) *value = progress->Value;
		else if (auto* ring = dynamic_cast<ProgressRing*>(control)) *value = ring->PercentageValue;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_Maximum(double* value) override { return RangeMaximum(value); }
	HRESULT STDMETHODCALLTYPE get_Minimum(double* value) override { return RangeMinimum(value); }
	HRESULT STDMETHODCALLTYPE get_LargeChange(double* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* slider = dynamic_cast<Slider*>(control)) *value = slider->Step * 10.0;
		else if (auto* numeric = dynamic_cast<NumericUpDown*>(control)) *value = numeric->Step * 10.0;
		else if (SupportsRange()) *value = 0.0;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_SmallChange(double* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* slider = dynamic_cast<Slider*>(control)) *value = slider->Step;
		else if (auto* numeric = dynamic_cast<NumericUpDown*>(control)) *value = numeric->Step;
		else if (SupportsRange()) *value = 0.0;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Expand() override { return SetExpandedState(true); }
	HRESULT STDMETHODCALLTYPE Collapse() override { return SetExpandedState(false); }
	HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(
		ExpandCollapseState* value) override
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* combo = dynamic_cast<ComboBox*>(control))
			*value = combo->Expand ? ExpandCollapseState_Expanded
				: ExpandCollapseState_Collapsed;
		else if (auto* expander = dynamic_cast<Expander*>(control))
			*value = expander->IsExpanded ? ExpandCollapseState_Expanded
				: ExpandCollapseState_Collapsed;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Select() override { return SelectItem(); }
	HRESULT STDMETHODCALLTYPE AddToSelection() override { return SelectItem(); }
	HRESULT STDMETHODCALLTYPE RemoveFromSelection() override
	{
		return CurrentControl() ? UIA_E_INVALIDOPERATION
			: UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* value) override
	{
		if (!value) return E_POINTER;
		*value = FALSE;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (_type == UIClass::UI_RadioBox)
			*value = control->Checked ? TRUE : FALSE;
		else if (_type == UIClass::UI_TabPage)
		{
			auto* tabs = dynamic_cast<TabControl*>(control->Parent);
			if (!tabs) return UIA_E_INVALIDOPERATION;
			auto& pages = tabs->Pages;
			auto position = std::find(pages.begin(), pages.end(), control);
			*value = position != pages.end()
				&& static_cast<int>(position - pages.begin()) == tabs->SelectedIndex
				? TRUE : FALSE;
		}
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_SelectionContainer(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		auto* control = CurrentControl();
		if (!control || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
		if (control->Parent)
		{
			auto* provider = _root->ProviderFor(control->Parent);
			if (!provider) return UIA_E_ELEMENTNOTAVAILABLE;
			*value = static_cast<IRawElementProviderSimple*>(provider);
		}
		else
		{
			*value = static_cast<IRawElementProviderSimple*>(_root);
			_root->AddRef();
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY** value) override;
	HRESULT STDMETHODCALLTYPE get_CanSelectMultiple(BOOL* value) override
	{
		if (!value) return E_POINTER;
		if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsSelection()) return UIA_E_NOTSUPPORTED;
		*value = _type == UIClass::UI_TabControl
			? FALSE : (VirtualContainerInfo().CanSelectMultiple ? TRUE : FALSE);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_IsSelectionRequired(BOOL* value) override
	{
		if (!value) return E_POINTER;
		if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsSelection()) return UIA_E_NOTSUPPORTED;
		*value = _type == UIClass::UI_TabControl
			? TRUE : (VirtualContainerInfo().IsSelectionRequired ? TRUE : FALSE);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetItem(
		int row, int column, IRawElementProviderSimple** value) override;
	HRESULT STDMETHODCALLTYPE get_RowCount(int* value) override;
	HRESULT STDMETHODCALLTYPE get_ColumnCount(int* value) override;
	HRESULT STDMETHODCALLTYPE GetRowHeaders(SAFEARRAY** value) override;
	HRESULT STDMETHODCALLTYPE GetColumnHeaders(SAFEARRAY** value) override;
	HRESULT STDMETHODCALLTYPE get_RowOrColumnMajor(
		RowOrColumnMajor* value) override;
	HRESULT STDMETHODCALLTYPE Scroll(
		ScrollAmount horizontalAmount, ScrollAmount verticalAmount) override;
	HRESULT STDMETHODCALLTYPE SetScrollPercent(
		double horizontalPercent, double verticalPercent) override;
	HRESULT STDMETHODCALLTYPE get_HorizontalScrollPercent(double* value) override;
	HRESULT STDMETHODCALLTYPE get_HorizontalViewSize(double* value) override;
	HRESULT STDMETHODCALLTYPE get_VerticalScrollPercent(double* value) override;
	HRESULT STDMETHODCALLTYPE get_VerticalViewSize(double* value) override;
	HRESULT STDMETHODCALLTYPE get_HorizontallyScrollable(BOOL* value) override;
	HRESULT STDMETHODCALLTYPE get_VerticallyScrollable(BOOL* value) override;

private:
	~ControlUiaProvider()
	{
		if (_root)
		{
			_root->UnregisterProvider(_runtimeId, this);
			_root->Release();
		}
	}
	Control* CurrentControl() const
	{
		return _root ? _root->ResolveControl(_control, _runtimeId) : nullptr;
	}
	bool SupportsInvoke() const
	{
		return accessibility_detail::SupportsDefaultAction(_role)
			&& _role != AccessibleRole::CheckBox
			&& _role != AccessibleRole::Switch;
	}
	bool SupportsToggle() const
	{
		return _role == AccessibleRole::CheckBox
			|| _role == AccessibleRole::Switch;
	}
	bool SupportsValue() const
	{
		return _type == UIClass::UI_TextBox
			|| _type == UIClass::UI_RichTextBox
			|| _type == UIClass::UI_PasswordBox;
	}
	bool SupportsRange() const
	{
		return _type == UIClass::UI_Slider
			|| _type == UIClass::UI_NumericUpDown
			|| _type == UIClass::UI_ProgressBar
			|| _type == UIClass::UI_ProgressRing;
	}
	bool SupportsExpandCollapse() const
	{
		return _type == UIClass::UI_ComboBox || _type == UIClass::UI_Expander;
	}
	bool SupportsSelectionItem() const
	{
		return _type == UIClass::UI_RadioBox || _type == UIClass::UI_TabPage;
	}
	AccessibilityVirtualContainerInfo VirtualContainerInfo() const
	{
		auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(
			CurrentControl());
		return source ? source->GetAccessibilityVirtualContainerInfo()
			: AccessibilityVirtualContainerInfo{};
	}
	IAccessibilityVirtualizedControl* VirtualSource() const
	{
		return dynamic_cast<IAccessibilityVirtualizedControl*>(CurrentControl());
	}
	bool SupportsSelection() const
	{
		return _type == UIClass::UI_TabControl
			|| HasAccessibilityVirtualPattern(
				VirtualContainerInfo().Patterns,
				AccessibilityVirtualPattern::Selection);
	}
	bool SupportsGrid() const
	{
		return HasAccessibilityVirtualPattern(
			VirtualContainerInfo().Patterns,
			AccessibilityVirtualPattern::Grid);
	}
	bool SupportsTable() const
	{
		return HasAccessibilityVirtualPattern(
			VirtualContainerInfo().Patterns,
			AccessibilityVirtualPattern::Table);
	}
	bool SupportsScroll() const
	{
		return HasAccessibilityVirtualPattern(
			VirtualContainerInfo().Patterns,
			AccessibilityVirtualPattern::Scroll);
	}
	bool IsOnscreen(Control* control) const
	{
		auto* form = _root ? _root->GetForm() : nullptr;
		if (!form || !form->Handle) return false;
		RECT controlRect = form->ContentDipRectToClientPixels(control->AbsRect);
		RECT client{};
		return ::GetClientRect(form->Handle, &client)
			&& controlRect.right > client.left && controlRect.left < client.right
			&& controlRect.bottom > client.top && controlRect.top < client.bottom;
	}
	HRESULT RangeIsReadOnly(BOOL* value)
	{
		if (!value) return E_POINTER;
		if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!SupportsRange()) return UIA_E_NOTSUPPORTED;
		*value = (_type == UIClass::UI_ProgressBar
			|| _type == UIClass::UI_ProgressRing) ? TRUE : FALSE;
		return S_OK;
	}
	HRESULT RangeMaximum(double* value)
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* slider = dynamic_cast<Slider*>(control)) *value = slider->Max;
		else if (auto* numeric = dynamic_cast<NumericUpDown*>(control)) *value = numeric->Max;
		else if (auto* progress = dynamic_cast<ProgressBar*>(control)) *value = progress->MaxValue;
		else if (dynamic_cast<ProgressRing*>(control)) *value = 1.0;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}
	HRESULT RangeMinimum(double* value)
	{
		if (!value) return E_POINTER;
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (auto* slider = dynamic_cast<Slider*>(control)) *value = slider->Min;
		else if (auto* numeric = dynamic_cast<NumericUpDown*>(control)) *value = numeric->Min;
		else if (dynamic_cast<ProgressBar*>(control)
			|| dynamic_cast<ProgressRing*>(control)) *value = 0.0;
		else return UIA_E_NOTSUPPORTED;
		return S_OK;
	}
	HRESULT SetExpandedState(bool expanded)
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		if (auto* combo = dynamic_cast<ComboBox*>(control))
		{
			combo->SetExpanded(expanded);
			return S_OK;
		}
		if (auto* expander = dynamic_cast<Expander*>(control))
		{
			return expander->TrySetCurrentPropertyValue(
				L"IsExpanded", BindingValue(expanded))
				? S_OK : UIA_E_INVALIDOPERATION;
		}
		return UIA_E_NOTSUPPORTED;
	}
	HRESULT SelectItem()
	{
		auto* control = CurrentControl();
		if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!control->GetAccessibilitySnapshot().Enabled)
			return UIA_E_ELEMENTNOTENABLED;
		if (_type == UIClass::UI_RadioBox)
			return control->Invoke() ? S_OK : UIA_E_INVALIDOPERATION;
		if (_type == UIClass::UI_TabPage)
		{
			auto* tabs = dynamic_cast<TabControl*>(control->Parent);
			if (!tabs) return UIA_E_INVALIDOPERATION;
			auto& pages = tabs->Pages;
			auto position = std::find(pages.begin(), pages.end(), control);
			if (position == pages.end()) return UIA_E_INVALIDOPERATION;
			const int index = static_cast<int>(position - pages.begin());
			return tabs->SelectedIndex == index || tabs->SelectPage(index)
				? S_OK : UIA_E_INVALIDOPERATION;
		}
		return UIA_E_NOTSUPPORTED;
	}

	std::atomic<ULONG> _references{ 1 };
	FormUiaProvider* _root = nullptr;
	Control* _control = nullptr;
	uint32_t _runtimeId = 0;
	UIClass _type = UIClass::UI_Base;
	AccessibleRole _role = AccessibleRole::Custom;
};

class VirtualUiaProvider final :
	public IRawElementProviderSimple,
	public IRawElementProviderFragment,
	public IInvokeProvider,
	public IToggleProvider,
	public IValueProvider,
	public IExpandCollapseProvider,
	public ISelectionItemProvider,
	public IScrollItemProvider,
	public IVirtualizedItemProvider,
	public IGridItemProvider,
	public ITableItemProvider
{
public:
	VirtualUiaProvider(FormUiaProvider* root, Control* owner, uint32_t id) :
		_root(root), _owner(owner),
		_ownerRuntimeId(owner ? owner->GetAccessibilityRuntimeId() : 0),
		_virtualId(id),
		_key((static_cast<uint64_t>(_ownerRuntimeId) << 32) | id)
	{
		if (_root) _root->AddRef();
	}

	bool Matches(Control* owner, uint32_t id) const noexcept
	{
		return owner == _owner && id == _virtualId
			&& owner && owner->GetAccessibilityRuntimeId() == _ownerRuntimeId;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == IID_IUnknown || iid == IID_IRawElementProviderSimple)
			*object = static_cast<IRawElementProviderSimple*>(this);
		else if (iid == IID_IRawElementProviderFragment)
			*object = static_cast<IRawElementProviderFragment*>(this);
		else if (iid == IID_IInvokeProvider && Supports(
			AccessibilityVirtualPattern::Invoke))
			*object = static_cast<IInvokeProvider*>(this);
		else if (iid == IID_IToggleProvider && Supports(
			AccessibilityVirtualPattern::Toggle))
			*object = static_cast<IToggleProvider*>(this);
		else if (iid == IID_IValueProvider && Supports(
			AccessibilityVirtualPattern::Value))
			*object = static_cast<IValueProvider*>(this);
		else if (iid == IID_IExpandCollapseProvider && Supports(
			AccessibilityVirtualPattern::ExpandCollapse))
			*object = static_cast<IExpandCollapseProvider*>(this);
		else if (iid == IID_ISelectionItemProvider && Supports(
			AccessibilityVirtualPattern::SelectionItem))
			*object = static_cast<ISelectionItemProvider*>(this);
		else if (iid == IID_IScrollItemProvider && Supports(
			AccessibilityVirtualPattern::ScrollItem))
			*object = static_cast<IScrollItemProvider*>(this);
		else if (iid == IID_IVirtualizedItemProvider && Supports(
			AccessibilityVirtualPattern::VirtualizedItem))
			*object = static_cast<IVirtualizedItemProvider*>(this);
		else if (iid == IID_IGridItemProvider && Supports(
			AccessibilityVirtualPattern::GridItem))
			*object = static_cast<IGridItemProvider*>(this);
		else if (iid == IID_ITableItemProvider && Supports(
			AccessibilityVirtualPattern::TableItem))
			*object = static_cast<ITableItemProvider*>(this);
		else
			return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++_references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --_references;
		if (remaining == 0) delete this;
		return remaining;
	}

	HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* value) override
	{
		if (!value) return E_POINTER;
		*value = ProviderOptions_ServerSideProvider;
		return CurrentNode(nullptr) ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE GetPatternProvider(
		PATTERNID patternId, IUnknown** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentNode(nullptr)) return UIA_E_ELEMENTNOTAVAILABLE;
		const IID* iid = nullptr;
		if (patternId == UIA_InvokePatternId) iid = &IID_IInvokeProvider;
		else if (patternId == UIA_TogglePatternId) iid = &IID_IToggleProvider;
		else if (patternId == UIA_ValuePatternId) iid = &IID_IValueProvider;
		else if (patternId == UIA_ExpandCollapsePatternId) iid = &IID_IExpandCollapseProvider;
		else if (patternId == UIA_SelectionItemPatternId) iid = &IID_ISelectionItemProvider;
		else if (patternId == UIA_ScrollItemPatternId) iid = &IID_IScrollItemProvider;
		else if (patternId == UIA_VirtualizedItemPatternId) iid = &IID_IVirtualizedItemProvider;
		else if (patternId == UIA_GridItemPatternId) iid = &IID_IGridItemProvider;
		else if (patternId == UIA_TableItemPatternId) iid = &IID_ITableItemProvider;
		else return S_OK;
		const HRESULT result = QueryInterface(*iid, reinterpret_cast<void**>(value));
		return result == E_NOINTERFACE ? S_OK : result;
	}
	HRESULT STDMETHODCALLTYPE GetPropertyValue(
		PROPERTYID propertyId, VARIANT* value) override
	{
		if (!value) return E_POINTER;
		::VariantInit(value);
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		switch (propertyId)
		{
		case UIA_NamePropertyId:
			return accessibility_detail::SetVariantString(value, node.Name);
		case UIA_AutomationIdPropertyId:
			return accessibility_detail::SetVariantString(value, node.AutomationId);
		case UIA_ControlTypePropertyId:
			accessibility_detail::SetVariantInt(value,
				accessibility_detail::ToUiaControlType(node.Role)); return S_OK;
		case UIA_ClassNamePropertyId:
			return accessibility_detail::SetVariantString(value, L"CUI.VirtualItem");
		case UIA_FrameworkIdPropertyId:
			return accessibility_detail::SetVariantString(value, L"CUI");
		case UIA_ProviderDescriptionPropertyId:
			return accessibility_detail::SetVariantString(
				value, L"CUI native virtual item provider");
		case UIA_HelpTextPropertyId:
		case UIA_FullDescriptionPropertyId:
			return accessibility_detail::SetVariantString(value, node.Description);
		case UIA_IsEnabledPropertyId:
			accessibility_detail::SetVariantBool(value, node.Enabled); return S_OK;
		case UIA_IsKeyboardFocusablePropertyId:
			accessibility_detail::SetVariantBool(value,
				HasAccessibilityVirtualPattern(node.Patterns,
					AccessibilityVirtualPattern::SelectionItem));
			return S_OK;
		case UIA_HasKeyboardFocusPropertyId:
			accessibility_detail::SetVariantBool(value,
				_owner && _root && _owner->GetAccessibilitySnapshot().Focused
				&& _root->IsVirtualFocused(_owner, _virtualId));
			return S_OK;
		case UIA_IsOffscreenPropertyId:
			accessibility_detail::SetVariantBool(value, !node.Visible); return S_OK;
		case UIA_IsControlElementPropertyId:
		case UIA_IsContentElementPropertyId:
			accessibility_detail::SetVariantBool(value, true); return S_OK;
		case UIA_IsInvokePatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::Invoke);
		case UIA_IsTogglePatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::Toggle);
		case UIA_IsValuePatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::Value);
		case UIA_IsExpandCollapsePatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::ExpandCollapse);
		case UIA_IsSelectionItemPatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::SelectionItem);
		case UIA_IsScrollItemPatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::ScrollItem);
		case UIA_IsVirtualizedItemPatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::VirtualizedItem);
		case UIA_IsGridItemPatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::GridItem);
		case UIA_IsTableItemPatternAvailablePropertyId:
			return SetPatternAvailability(value, node, AccessibilityVirtualPattern::TableItem);
		case UIA_ToggleToggleStatePropertyId:
			if (!Supports(AccessibilityVirtualPattern::Toggle)) return S_OK;
			accessibility_detail::SetVariantInt(
				value, node.Checked ? ToggleState_On : ToggleState_Off); return S_OK;
		case UIA_ValueValuePropertyId:
			if (!Supports(AccessibilityVirtualPattern::Value)) return S_OK;
			return accessibility_detail::SetVariantString(value, node.Value);
		case UIA_ValueIsReadOnlyPropertyId:
			if (!Supports(AccessibilityVirtualPattern::Value)) return S_OK;
			accessibility_detail::SetVariantBool(value, node.ReadOnly); return S_OK;
		case UIA_ExpandCollapseExpandCollapseStatePropertyId:
			if (!Supports(AccessibilityVirtualPattern::ExpandCollapse)) return S_OK;
			accessibility_detail::SetVariantInt(value, node.Expanded
				? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed); return S_OK;
		case UIA_SelectionItemIsSelectedPropertyId:
			if (!Supports(AccessibilityVirtualPattern::SelectionItem)) return S_OK;
			accessibility_detail::SetVariantBool(value, node.Selected); return S_OK;
		case UIA_GridItemRowPropertyId:
			accessibility_detail::SetVariantInt(value, node.Row); return S_OK;
		case UIA_GridItemColumnPropertyId:
			accessibility_detail::SetVariantInt(value, node.Column); return S_OK;
		case UIA_GridItemRowSpanPropertyId:
			accessibility_detail::SetVariantInt(value, node.RowSpan); return S_OK;
		case UIA_GridItemColumnSpanPropertyId:
			accessibility_detail::SetVariantInt(value, node.ColumnSpan); return S_OK;
		case UIA_LevelPropertyId:
			accessibility_detail::SetVariantInt(value, node.Level); return S_OK;
		default: return S_OK;
		}
	}
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return CurrentNode(nullptr) ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}

	HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
		IRawElementProviderFragment** value) override;
	HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentNode(nullptr)) return UIA_E_ELEMENTNOTAVAILABLE;
		SAFEARRAY* result = ::SafeArrayCreateVector(VT_I4, 0, 3);
		if (!result) return E_OUTOFMEMORY;
		int ids[3]{ UiaAppendRuntimeId, static_cast<int>(_ownerRuntimeId),
			static_cast<int>(_virtualId) };
		HRESULT hr = S_OK;
		for (LONG index = 0; index < 3 && SUCCEEDED(hr); ++index)
			hr = ::SafeArrayPutElement(result, &index, &ids[index]);
		if (FAILED(hr)) { ::SafeArrayDestroy(result); return hr; }
		*value = result;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* value) override
	{
		if (!value) return E_POINTER;
		*value = UiaRect{};
		AccessibilityVirtualNode node;
		auto* form = _root ? _root->GetForm() : nullptr;
		if (!CurrentNode(&node) || !form || !_owner)
			return UIA_E_ELEMENTNOTAVAILABLE;
		const auto owner = _owner->GetAbsoluteLocationDip();
		const D2D1_RECT_F absolute = D2D1::RectF(
			owner.x + node.BoundsDip.left, owner.y + node.BoundsDip.top,
			owner.x + node.BoundsDip.right, owner.y + node.BoundsDip.bottom);
		RECT rectangle = form->ContentDipRectToClientPixels(absolute);
		POINT points[2]{ { rectangle.left, rectangle.top },
			{ rectangle.right, rectangle.bottom } };
		::MapWindowPoints(form->Handle, nullptr, points, 2);
		value->left = points[0].x;
		value->top = points[0].y;
		value->width = points[1].x - points[0].x;
		value->height = points[1].y - points[0].y;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		return CurrentNode(nullptr) ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
	}
	HRESULT STDMETHODCALLTYPE SetFocus() override
	{
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!node.Enabled) return UIA_E_ELEMENTNOTENABLED;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::SelectionItem))
			return UIA_E_INVALIDOPERATION;
		if (!_owner || !_root || !_owner->Focus())
			return UIA_E_INVALIDOPERATION;
		_root->SetVirtualFocus(_owner, _virtualId);
		_root->RaiseVirtualEvent(
			_owner, _virtualId, AccessibilityChange::Focus);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_FragmentRoot(
		IRawElementProviderFragmentRoot** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentNode(nullptr) || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
		*value = static_cast<IRawElementProviderFragmentRoot*>(_root);
		_root->AddRef();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Invoke() override
	{
		return PerformNodeAction(AccessibilityVirtualPattern::Invoke,
			[](IAccessibilityVirtualizedControl& source, uint32_t id)
			{ return source.InvokeAccessibilityVirtualNode(id); });
	}
	HRESULT STDMETHODCALLTYPE Toggle() override
	{
		return PerformNodeAction(AccessibilityVirtualPattern::Toggle,
			[](IAccessibilityVirtualizedControl& source, uint32_t id)
			{ return source.ToggleAccessibilityVirtualNode(id); });
	}
	HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* value) override
	{
		if (!value) return E_POINTER;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Toggle)) return UIA_E_NOTSUPPORTED;
		*value = node.Checked ? ToggleState_On : ToggleState_Off;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override
	{
		if (!value) return E_INVALIDARG;
		AccessibilityVirtualNode node;
		IAccessibilityVirtualizedControl* source = nullptr;
		if (!CurrentNode(&node, &source)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Value)) return UIA_E_NOTSUPPORTED;
		if (!node.Enabled) return UIA_E_ELEMENTNOTENABLED;
		if (node.ReadOnly) return UIA_E_NOTSUPPORTED;
		return source->SetAccessibilityVirtualNodeValue(_virtualId, value)
			? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT STDMETHODCALLTYPE get_Value(BSTR* value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Value)) return UIA_E_NOTSUPPORTED;
		return accessibility_detail::CopyBstr(node.Value, value);
	}
	HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* value) override
	{
		if (!value) return E_POINTER;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Value)) return UIA_E_NOTSUPPORTED;
		*value = node.ReadOnly ? TRUE : FALSE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Expand() override { return SetExpanded(true); }
	HRESULT STDMETHODCALLTYPE Collapse() override { return SetExpanded(false); }
	HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(
		ExpandCollapseState* value) override
	{
		if (!value) return E_POINTER;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::ExpandCollapse))
			return UIA_E_NOTSUPPORTED;
		*value = node.Expanded ? ExpandCollapseState_Expanded
			: ExpandCollapseState_Collapsed;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Select() override
	{
		return SelectNode(AccessibilitySelectionAction::Select);
	}
	HRESULT STDMETHODCALLTYPE AddToSelection() override
	{
		return SelectNode(AccessibilitySelectionAction::Add);
	}
	HRESULT STDMETHODCALLTYPE RemoveFromSelection() override
	{
		return SelectNode(AccessibilitySelectionAction::Remove);
	}
	HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* value) override
	{
		if (!value) return E_POINTER;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::SelectionItem))
			return UIA_E_NOTSUPPORTED;
		*value = node.Selected ? TRUE : FALSE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE get_SelectionContainer(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentNode(nullptr) || !_root || !_owner)
			return UIA_E_ELEMENTNOTAVAILABLE;
		auto* provider = _root->ProviderFor(_owner);
		if (!provider) return E_OUTOFMEMORY;
		*value = static_cast<IRawElementProviderSimple*>(provider);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE ScrollIntoView() override
	{
		return PerformNodeAction(AccessibilityVirtualPattern::ScrollItem,
			[](IAccessibilityVirtualizedControl& source, uint32_t id)
			{ return source.ScrollAccessibilityVirtualNodeIntoView(id); });
	}
	HRESULT STDMETHODCALLTYPE Realize() override
	{
		return PerformNodeAction(AccessibilityVirtualPattern::VirtualizedItem,
			[](IAccessibilityVirtualizedControl& source, uint32_t id)
			{ return source.ScrollAccessibilityVirtualNodeIntoView(id); });
	}
	HRESULT STDMETHODCALLTYPE get_Row(int* value) override
	{
		return GetGridInt(value, [](const AccessibilityVirtualNode& node) { return node.Row; });
	}
	HRESULT STDMETHODCALLTYPE get_Column(int* value) override
	{
		return GetGridInt(value, [](const AccessibilityVirtualNode& node) { return node.Column; });
	}
	HRESULT STDMETHODCALLTYPE get_RowSpan(int* value) override
	{
		return GetGridInt(value, [](const AccessibilityVirtualNode& node) { return node.RowSpan; });
	}
	HRESULT STDMETHODCALLTYPE get_ColumnSpan(int* value) override
	{
		return GetGridInt(value, [](const AccessibilityVirtualNode& node) { return node.ColumnSpan; });
	}
	HRESULT STDMETHODCALLTYPE get_ContainingGrid(
		IRawElementProviderSimple** value) override
	{
		if (!value) return E_POINTER;
		*value = nullptr;
		if (!CurrentNode(nullptr) || !_root || !_owner)
			return UIA_E_ELEMENTNOTAVAILABLE;
		auto* provider = _root->ProviderFor(_owner);
		if (!provider) return E_OUTOFMEMORY;
		*value = static_cast<IRawElementProviderSimple*>(provider);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetRowHeaderItems(SAFEARRAY** value) override
	{
		return CreateProviderArray({}, value);
	}
	HRESULT STDMETHODCALLTYPE GetColumnHeaderItems(SAFEARRAY** value) override;

private:
	~VirtualUiaProvider()
	{
		if (_root)
		{
			_root->UnregisterVirtualProvider(_key, this);
			_root->Release();
		}
	}
	bool CurrentNode(AccessibilityVirtualNode* result,
		IAccessibilityVirtualizedControl** source = nullptr) const
	{
		AccessibilityVirtualNode node;
		IAccessibilityVirtualizedControl* resolved = nullptr;
		if (!_root || !_root->ResolveVirtualNode(
			_owner, _ownerRuntimeId, _virtualId, node, &resolved)) return false;
		if (result) *result = std::move(node);
		if (source) *source = resolved;
		return true;
	}
	bool Supports(AccessibilityVirtualPattern pattern) const
	{
		AccessibilityVirtualNode node;
		return CurrentNode(&node)
			&& HasAccessibilityVirtualPattern(node.Patterns, pattern);
	}
	static HRESULT SetPatternAvailability(VARIANT* value,
		const AccessibilityVirtualNode& node,
		AccessibilityVirtualPattern pattern)
	{
		accessibility_detail::SetVariantBool(value,
			HasAccessibilityVirtualPattern(node.Patterns, pattern));
		return S_OK;
	}
	template<typename Action>
	HRESULT PerformNodeAction(
		AccessibilityVirtualPattern pattern, Action&& action)
	{
		AccessibilityVirtualNode node;
		IAccessibilityVirtualizedControl* source = nullptr;
		if (!CurrentNode(&node, &source)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(node.Patterns, pattern))
			return UIA_E_NOTSUPPORTED;
		if (!node.Enabled) return UIA_E_ELEMENTNOTENABLED;
		return action(*source, _virtualId) ? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT SelectNode(AccessibilitySelectionAction action)
	{
		AccessibilityVirtualNode node;
		IAccessibilityVirtualizedControl* source = nullptr;
		if (!CurrentNode(&node, &source)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::SelectionItem))
			return UIA_E_NOTSUPPORTED;
		if (!node.Enabled) return UIA_E_ELEMENTNOTENABLED;
		const auto container = source->GetAccessibilityVirtualContainerInfo();
		if (action == AccessibilitySelectionAction::Remove
			&& node.Selected && container.IsSelectionRequired)
			return UIA_E_INVALIDOPERATION;
		if (action == AccessibilitySelectionAction::Add
			&& !node.Selected && !container.CanSelectMultiple)
		{
			std::vector<uint32_t> selection;
			source->GetAccessibilityVirtualSelection(selection);
			if (!selection.empty()) return UIA_E_INVALIDOPERATION;
		}
		return source->SelectAccessibilityVirtualNode(_virtualId, action)
			? S_OK : UIA_E_INVALIDOPERATION;
	}
	HRESULT SetExpanded(bool expanded)
	{
		AccessibilityVirtualNode node;
		IAccessibilityVirtualizedControl* source = nullptr;
		if (!CurrentNode(&node, &source)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::ExpandCollapse))
			return UIA_E_NOTSUPPORTED;
		if (!node.Enabled) return UIA_E_ELEMENTNOTENABLED;
		return source->SetAccessibilityVirtualNodeExpanded(_virtualId, expanded)
			? S_OK : UIA_E_INVALIDOPERATION;
	}
	template<typename Getter>
	HRESULT GetGridInt(int* value, Getter&& getter)
	{
		if (!value) return E_POINTER;
		AccessibilityVirtualNode node;
		if (!CurrentNode(&node)) return UIA_E_ELEMENTNOTAVAILABLE;
		if (!HasAccessibilityVirtualPattern(node.Patterns,
			AccessibilityVirtualPattern::GridItem)) return UIA_E_NOTSUPPORTED;
		*value = getter(node);
		return S_OK;
	}
	HRESULT CreateProviderArray(
		const std::vector<uint32_t>& ids, SAFEARRAY** value);

	std::atomic<ULONG> _references{ 1 };
	FormUiaProvider* _root = nullptr;
	Control* _owner = nullptr;
	uint32_t _ownerRuntimeId = 0;
	uint32_t _virtualId = 0;
	uint64_t _key = 0;
};

Control* FormUiaProvider::ResolveControl(
	Control* candidate, uint32_t runtimeId) const
{
	auto* form = GetForm();
	if (!form || !Connected() || !candidate || runtimeId == 0) return nullptr;
	const auto controls = form->GetAccessibleControls();
	const auto position = std::find(controls.begin(), controls.end(), candidate);
	if (position == controls.end()) return nullptr;
	return (*position)->GetAccessibilityRuntimeId() == runtimeId
		? *position : nullptr;
}

ControlUiaProvider* FormUiaProvider::ProviderFor(Control* control)
{
	if (!control || !ResolveControl(
		control, control->GetAccessibilityRuntimeId())) return nullptr;
	const uint32_t runtimeId = control->GetAccessibilityRuntimeId();
	std::lock_guard lock(_providerMutex);
	if (const auto position = _providers.find(runtimeId);
		position != _providers.end())
	{
		if (position->second && position->second->Matches(control))
		{
			position->second->AddRef();
			return position->second;
		}
		_providers.erase(position);
	}
	auto* provider = new (std::nothrow) ControlUiaProvider(this, control);
	if (!provider) return nullptr;
	_providers.emplace(runtimeId, provider);
	return provider;
}

bool FormUiaProvider::ResolveVirtualNode(
	Control* owner, uint32_t ownerRuntimeId, uint32_t virtualId,
	AccessibilityVirtualNode& node,
	IAccessibilityVirtualizedControl** source) const
{
	if (source) *source = nullptr;
	auto* resolvedOwner = ResolveControl(owner, ownerRuntimeId);
	if (!resolvedOwner || virtualId == 0) return false;
	auto* virtualized = dynamic_cast<IAccessibilityVirtualizedControl*>(
		resolvedOwner);
	if (!virtualized
		|| !virtualized->TryGetAccessibilityVirtualNode(virtualId, node)
		|| node.Id != virtualId) return false;
	if (source) *source = virtualized;
	return true;
}

VirtualUiaProvider* FormUiaProvider::VirtualProviderFor(
	Control* owner, uint32_t virtualId)
{
	if (!owner || virtualId == 0) return nullptr;
	AccessibilityVirtualNode node;
	const uint32_t ownerRuntimeId = owner->GetAccessibilityRuntimeId();
	if (!ResolveVirtualNode(owner, ownerRuntimeId, virtualId, node)) return nullptr;
	const uint64_t key = (static_cast<uint64_t>(ownerRuntimeId) << 32)
		| virtualId;
	std::lock_guard lock(_providerMutex);
	if (const auto position = _virtualProviders.find(key);
		position != _virtualProviders.end())
	{
		if (position->second && position->second->Matches(owner, virtualId))
		{
			position->second->AddRef();
			return position->second;
		}
		_virtualProviders.erase(position);
	}
	auto* provider = new (std::nothrow) VirtualUiaProvider(
		this, owner, virtualId);
	if (!provider) return nullptr;
	_virtualProviders.emplace(key, provider);
	return provider;
}

void FormUiaProvider::UnregisterProvider(
	uint32_t runtimeId, ControlUiaProvider* provider)
{
	std::lock_guard lock(_providerMutex);
	const auto position = _providers.find(runtimeId);
	if (position != _providers.end() && position->second == provider)
		_providers.erase(position);
}

void FormUiaProvider::UnregisterVirtualProvider(
	uint64_t key, VirtualUiaProvider* provider)
{
	std::lock_guard lock(_providerMutex);
	const auto position = _virtualProviders.find(key);
	if (position != _virtualProviders.end() && position->second == provider)
		_virtualProviders.erase(position);
}

void FormUiaProvider::SetVirtualFocus(Control* owner, uint32_t virtualId)
{
	if (!owner || virtualId == 0) return;
	std::lock_guard lock(_providerMutex);
	_focusedVirtualByOwner[owner->GetAccessibilityRuntimeId()] = virtualId;
}

bool FormUiaProvider::IsVirtualFocused(
	Control* owner, uint32_t virtualId)
{
	if (!owner || virtualId == 0) return false;
	std::lock_guard lock(_providerMutex);
	const auto position = _focusedVirtualByOwner.find(
		owner->GetAccessibilityRuntimeId());
	return position != _focusedVirtualByOwner.end()
		&& position->second == virtualId;
}

uint32_t FormUiaProvider::VirtualFocusFor(Control* owner)
{
	if (!owner) return 0;
	std::lock_guard lock(_providerMutex);
	const auto position = _focusedVirtualByOwner.find(
		owner->GetAccessibilityRuntimeId());
	return position == _focusedVirtualByOwner.end() ? 0 : position->second;
}

std::vector<Control*> FormUiaProvider::DirectChildren(Control* parent) const
{
	std::vector<Control*> result;
	auto* form = GetForm();
	if (!form || !Connected()) return result;
	const auto& source = parent ? parent->Children : form->Controls;
	result.reserve(source.size());
	for (auto* child : source)
	{
		if (child && ResolveControl(child, child->GetAccessibilityRuntimeId()))
			result.push_back(child);
	}
	return result;
}

bool FormUiaProvider::TryGetVirtualBoundaryChild(
	Control* owner, uint32_t parentId, bool last, uint32_t& result) const
{
	result = 0;
	if (!owner || !ResolveControl(
		owner, owner->GetAccessibilityRuntimeId())) return false;
	auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(owner);
	if (!source) return false;
	const size_t count = source->GetAccessibilityVirtualChildCount(parentId);
	for (size_t offset = 0; offset < count; ++offset)
	{
		const size_t index = last ? count - offset - 1 : offset;
		uint32_t candidate = 0;
		if (!source->TryGetAccessibilityVirtualChildAt(
			parentId, index, candidate) || candidate == 0) continue;
		AccessibilityVirtualNode node;
		if (!ResolveVirtualNode(owner, owner->GetAccessibilityRuntimeId(),
			candidate, node)) continue;
		result = candidate;
		return true;
	}
	return false;
}

bool FormUiaProvider::TryGetVirtualSibling(
	Control* owner, uint32_t parentId, uint32_t id,
	bool next, uint32_t& result) const
{
	result = 0;
	if (!owner || !ResolveControl(
		owner, owner->GetAccessibilityRuntimeId())) return false;
	auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(owner);
	if (!source || !source->TryGetAccessibilityVirtualSibling(
		parentId, id, next, result) || result == 0) return false;
	AccessibilityVirtualNode node;
	if (!ResolveVirtualNode(owner, owner->GetAccessibilityRuntimeId(), result, node))
	{
		result = 0;
		return false;
	}
	return true;
}

Control* FormUiaProvider::ParentOf(Control* control) const
{
	if (!control || !ResolveControl(
		control, control->GetAccessibilityRuntimeId())) return nullptr;
	return control->Parent;
}

Control* FormUiaProvider::SiblingOf(Control* control, bool next) const
{
	if (!control) return nullptr;
	const auto siblings = DirectChildren(control->Parent);
	const auto position = std::find(siblings.begin(), siblings.end(), control);
	if (position == siblings.end()) return nullptr;
	if (next)
		return position + 1 != siblings.end() ? *(position + 1) : nullptr;
	return position != siblings.begin() ? *(position - 1) : nullptr;
}

HRESULT FormUiaProvider::Navigate(NavigateDirection direction,
	IRawElementProviderFragment** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	if (!Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (direction != NavigateDirection_FirstChild
		&& direction != NavigateDirection_LastChild) return S_OK;
	const auto children = DirectChildren(nullptr);
	if (children.empty()) return S_OK;
	auto* provider = ProviderFor(direction == NavigateDirection_FirstChild
		? children.front() : children.back());
	if (!provider) return E_OUTOFMEMORY;
	*value = static_cast<IRawElementProviderFragment*>(provider);
	return S_OK;
}

HRESULT FormUiaProvider::ElementProviderFromPoint(double x, double y,
	IRawElementProviderFragment** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* form = GetForm();
	if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
	POINT clientPoint{ static_cast<LONG>(std::floor(x)),
		static_cast<LONG>(std::floor(y)) };
	if (!::ScreenToClient(form->Handle, &clientPoint))
	{
		const DWORD error = ::GetLastError();
		return HRESULT_FROM_WIN32(error != ERROR_SUCCESS
			? error : ERROR_INVALID_WINDOW_HANDLE);
	}
	float dpiScale = form->GetDpiScale();
	if (dpiScale <= 0.0f) dpiScale = 1.0f;
	const float contentX = static_cast<float>(clientPoint.x) / dpiScale;
	const float contentY = static_cast<float>(clientPoint.y
		- (form->VisibleHead ? form->HeadHeight : 0)) / dpiScale;
	auto controls = form->GetAccessibleControls();
	for (auto position = controls.rbegin(); position != controls.rend(); ++position)
	{
		auto* control = *position;
		if (!control || !control->GetAccessibilitySnapshot().Visible) continue;
		auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(control);
		if (source)
		{
			const auto owner = control->GetAbsoluteLocationDip();
			uint32_t virtualId = 0;
			if (source->TryHitTestAccessibilityVirtualNode(
				contentX - owner.x, contentY - owner.y, virtualId)
				&& virtualId != 0)
			{
				AccessibilityVirtualNode node;
				if (ResolveVirtualNode(control,
					control->GetAccessibilityRuntimeId(), virtualId, node))
				{
					auto* provider = VirtualProviderFor(control, virtualId);
					if (!provider) return E_OUTOFMEMORY;
					*value = static_cast<IRawElementProviderFragment*>(provider);
					return S_OK;
				}
			}
		}
		RECT rectangle = form->ContentDipRectToClientPixels(control->AbsRect);
		POINT points[2]{ { rectangle.left, rectangle.top },
			{ rectangle.right, rectangle.bottom } };
		::MapWindowPoints(form->Handle, nullptr, points, 2);
		if (x < points[0].x || y < points[0].y
			|| x >= points[1].x || y >= points[1].y) continue;
		auto* provider = ProviderFor(control);
		if (!provider) return E_OUTOFMEMORY;
		*value = static_cast<IRawElementProviderFragment*>(provider);
		return S_OK;
	}
	*value = static_cast<IRawElementProviderFragment*>(this);
	AddRef();
	return S_OK;
}

HRESULT FormUiaProvider::GetFocus(IRawElementProviderFragment** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* form = GetForm();
	if (!form || !Connected()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!form->Selected) return S_OK;
	if (const uint32_t focused = VirtualFocusFor(form->Selected);
		focused != 0)
	{
		if (auto* provider = VirtualProviderFor(form->Selected, focused))
		{
			*value = static_cast<IRawElementProviderFragment*>(provider);
			return S_OK;
		}
	}
	if (auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(
		form->Selected))
	{
		std::vector<uint32_t> selection;
		source->GetAccessibilityVirtualSelection(selection);
		if (!selection.empty())
		{
			auto* provider = VirtualProviderFor(
				form->Selected, selection.front());
			if (!provider) return E_OUTOFMEMORY;
			*value = static_cast<IRawElementProviderFragment*>(provider);
			return S_OK;
		}
	}
	auto* provider = ProviderFor(form->Selected);
	if (!provider) return E_OUTOFMEMORY;
	*value = static_cast<IRawElementProviderFragment*>(provider);
	return S_OK;
}

HRESULT ControlUiaProvider::Navigate(NavigateDirection direction,
	IRawElementProviderFragment** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* control = CurrentControl();
	if (!control || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
	Control* destination = nullptr;
	uint32_t virtualDestination = 0;
	Control* virtualOwner = nullptr;
	switch (direction)
	{
	case NavigateDirection_Parent:
		destination = _root->ParentOf(control);
		if (!destination)
		{
			*value = static_cast<IRawElementProviderFragment*>(_root);
			_root->AddRef();
			return S_OK;
		}
		break;
	case NavigateDirection_FirstChild:
	case NavigateDirection_LastChild:
		{
			const auto children = _root->DirectChildren(control);
			if (direction == NavigateDirection_FirstChild)
			{
				if (!children.empty()) destination = children.front();
				else if (_root->TryGetVirtualBoundaryChild(
					control, 0, false, virtualDestination))
				{
					virtualOwner = control;
				}
			}
			else
			{
				if (_root->TryGetVirtualBoundaryChild(
					control, 0, true, virtualDestination))
				{
					virtualOwner = control;
				}
				else if (!children.empty()) destination = children.back();
			}
		}
		break;
	case NavigateDirection_NextSibling:
		destination = _root->SiblingOf(control, true);
		if (!destination && control->Parent)
		{
			if (_root->TryGetVirtualBoundaryChild(
				control->Parent, 0, false, virtualDestination))
			{
				virtualOwner = control->Parent;
			}
		}
		break;
	case NavigateDirection_PreviousSibling:
		destination = _root->SiblingOf(control, false);
		break;
	default:
		return E_INVALIDARG;
	}
	if (virtualDestination != 0)
	{
		auto* provider = _root->VirtualProviderFor(
			virtualOwner, virtualDestination);
		if (!provider) return E_OUTOFMEMORY;
		*value = static_cast<IRawElementProviderFragment*>(provider);
		return S_OK;
	}
	if (!destination) return S_OK;
	auto* provider = _root->ProviderFor(destination);
	if (!provider) return E_OUTOFMEMORY;
	*value = static_cast<IRawElementProviderFragment*>(provider);
	return S_OK;
}

HRESULT VirtualUiaProvider::Navigate(NavigateDirection direction,
	IRawElementProviderFragment** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	AccessibilityVirtualNode node;
	if (!CurrentNode(&node) || !_root || !_owner)
		return UIA_E_ELEMENTNOTAVAILABLE;
	auto returnVirtual = [&](uint32_t id) -> HRESULT
	{
		if (id == 0) return S_OK;
		auto* provider = _root->VirtualProviderFor(_owner, id);
		if (!provider) return E_OUTOFMEMORY;
		*value = static_cast<IRawElementProviderFragment*>(provider);
		return S_OK;
	};
	switch (direction)
	{
	case NavigateDirection_Parent:
		if (node.ParentId != 0) return returnVirtual(node.ParentId);
		{
			auto* provider = _root->ProviderFor(_owner);
			if (!provider) return E_OUTOFMEMORY;
			*value = static_cast<IRawElementProviderFragment*>(provider);
			return S_OK;
		}
	case NavigateDirection_FirstChild:
	case NavigateDirection_LastChild:
		{
			uint32_t child = 0;
			if (!_root->TryGetVirtualBoundaryChild(_owner, _virtualId,
				direction == NavigateDirection_LastChild, child)) return S_OK;
			return returnVirtual(child);
		}
	case NavigateDirection_NextSibling:
	case NavigateDirection_PreviousSibling:
		{
			uint32_t sibling = 0;
			if (_root->TryGetVirtualSibling(_owner, node.ParentId, _virtualId,
				direction == NavigateDirection_NextSibling, sibling))
				return returnVirtual(sibling);
			if (direction == NavigateDirection_NextSibling) return S_OK;
			if (node.ParentId == 0)
			{
				const auto controls = _root->DirectChildren(_owner);
				if (!controls.empty())
				{
					auto* provider = _root->ProviderFor(controls.back());
					if (!provider) return E_OUTOFMEMORY;
					*value = static_cast<IRawElementProviderFragment*>(provider);
				}
			}
			return S_OK;
		}
	default:
		return E_INVALIDARG;
	}
}

HRESULT ControlUiaProvider::GetSelection(SAFEARRAY** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* control = CurrentControl();
	if (!control || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* tabs = dynamic_cast<TabControl*>(control);
	if (tabs)
	{
		Control* selected = nullptr;
		auto& pages = tabs->Pages;
		if (tabs->SelectedIndex >= 0
			&& static_cast<size_t>(tabs->SelectedIndex) < pages.size())
			selected = pages[static_cast<size_t>(tabs->SelectedIndex)];
		SAFEARRAY* result = ::SafeArrayCreateVector(
			VT_UNKNOWN, 0, selected ? 1 : 0);
		if (!result) return E_OUTOFMEMORY;
		if (selected)
		{
			auto* provider = _root->ProviderFor(selected);
			if (!provider)
			{
				::SafeArrayDestroy(result);
				return E_OUTOFMEMORY;
			}
			LONG index = 0;
			IUnknown* unknown = static_cast<IRawElementProviderSimple*>(provider);
			const HRESULT hr = ::SafeArrayPutElement(result, &index, unknown);
			provider->Release();
			if (FAILED(hr))
			{
				::SafeArrayDestroy(result);
				return hr;
			}
		}
		*value = result;
		return S_OK;
	}
	auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(control);
	if (!source || !SupportsSelection()) return UIA_E_NOTSUPPORTED;
	std::vector<uint32_t> selection;
	source->GetAccessibilityVirtualSelection(selection);
	SAFEARRAY* result = ::SafeArrayCreateVector(
		VT_UNKNOWN, 0, static_cast<ULONG>(selection.size()));
	if (!result) return E_OUTOFMEMORY;
	for (LONG index = 0; index < static_cast<LONG>(selection.size()); ++index)
	{
		auto* provider = _root->VirtualProviderFor(
			control, selection[static_cast<size_t>(index)]);
		if (!provider)
		{
			::SafeArrayDestroy(result);
			return E_OUTOFMEMORY;
		}
		IUnknown* unknown = static_cast<IRawElementProviderSimple*>(provider);
		const HRESULT hr = ::SafeArrayPutElement(result, &index, unknown);
		provider->Release();
		if (FAILED(hr))
		{
			::SafeArrayDestroy(result);
			return hr;
		}
	}
	*value = result;
	return S_OK;
}

HRESULT ControlUiaProvider::GetItem(
	int row, int column, IRawElementProviderSimple** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* control = CurrentControl();
	auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(control);
	if (!control || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!source || !SupportsGrid()) return UIA_E_NOTSUPPORTED;
	uint32_t id = 0;
	if (!source->GetAccessibilityVirtualItemAt(row, column, id))
		return E_INVALIDARG;
	auto* provider = _root->VirtualProviderFor(control, id);
	if (!provider) return E_OUTOFMEMORY;
	*value = static_cast<IRawElementProviderSimple*>(provider);
	return S_OK;
}

HRESULT ControlUiaProvider::get_RowCount(int* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!SupportsGrid()) return UIA_E_NOTSUPPORTED;
	*value = VirtualContainerInfo().RowCount;
	return S_OK;
}

HRESULT ControlUiaProvider::get_ColumnCount(int* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!SupportsGrid()) return UIA_E_NOTSUPPORTED;
	*value = VirtualContainerInfo().ColumnCount;
	return S_OK;
}

HRESULT ControlUiaProvider::GetRowHeaders(SAFEARRAY** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!SupportsTable()) return UIA_E_NOTSUPPORTED;
	*value = ::SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
	return *value ? S_OK : E_OUTOFMEMORY;
}

HRESULT ControlUiaProvider::GetColumnHeaders(SAFEARRAY** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	auto* control = CurrentControl();
	auto* source = dynamic_cast<IAccessibilityVirtualizedControl*>(control);
	if (!control || !_root) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!source || !SupportsTable()) return UIA_E_NOTSUPPORTED;
	std::vector<uint32_t> headers;
	source->GetAccessibilityVirtualColumnHeaders(headers);
	SAFEARRAY* result = ::SafeArrayCreateVector(
		VT_UNKNOWN, 0, static_cast<ULONG>(headers.size()));
	if (!result) return E_OUTOFMEMORY;
	for (LONG index = 0; index < static_cast<LONG>(headers.size()); ++index)
	{
		auto* provider = _root->VirtualProviderFor(
			control, headers[static_cast<size_t>(index)]);
		if (!provider)
		{
			::SafeArrayDestroy(result);
			return E_OUTOFMEMORY;
		}
		IUnknown* unknown = static_cast<IRawElementProviderSimple*>(provider);
		const HRESULT hr = ::SafeArrayPutElement(result, &index, unknown);
		provider->Release();
		if (FAILED(hr))
		{
			::SafeArrayDestroy(result);
			return hr;
		}
	}
	*value = result;
	return S_OK;
}

HRESULT ControlUiaProvider::get_RowOrColumnMajor(RowOrColumnMajor* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!SupportsTable()) return UIA_E_NOTSUPPORTED;
	*value = RowOrColumnMajor_RowMajor;
	return S_OK;
}

HRESULT ControlUiaProvider::Scroll(
	ScrollAmount horizontalAmount, ScrollAmount verticalAmount)
{
	auto* control = CurrentControl();
	auto* source = VirtualSource();
	if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!source || !SupportsScroll()) return UIA_E_NOTSUPPORTED;
	if (!control->GetAccessibilitySnapshot().Enabled)
		return UIA_E_ELEMENTNOTENABLED;
	auto convert = [](ScrollAmount amount,
		AccessibilityScrollAmount& result) -> bool
	{
		switch (amount)
		{
		case ScrollAmount_LargeDecrement:
			result = AccessibilityScrollAmount::LargeDecrement; return true;
		case ScrollAmount_SmallDecrement:
			result = AccessibilityScrollAmount::SmallDecrement; return true;
		case ScrollAmount_NoAmount:
			result = AccessibilityScrollAmount::NoAmount; return true;
		case ScrollAmount_LargeIncrement:
			result = AccessibilityScrollAmount::LargeIncrement; return true;
		case ScrollAmount_SmallIncrement:
			result = AccessibilityScrollAmount::SmallIncrement; return true;
		default: return false;
		}
	};
	AccessibilityScrollAmount horizontal{};
	AccessibilityScrollAmount vertical{};
	if (!convert(horizontalAmount, horizontal)
		|| !convert(verticalAmount, vertical)) return E_INVALIDARG;
	AccessibilityScrollInfo info;
	if (!source->GetAccessibilityScrollInfo(info))
		return UIA_E_INVALIDOPERATION;
	if ((!info.HorizontallyScrollable
			&& horizontal != AccessibilityScrollAmount::NoAmount)
		|| (!info.VerticallyScrollable
			&& vertical != AccessibilityScrollAmount::NoAmount))
		return UIA_E_INVALIDOPERATION;
	return source->ScrollAccessibility(horizontal, vertical)
		? S_OK : UIA_E_INVALIDOPERATION;
}

HRESULT ControlUiaProvider::SetScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	auto* control = CurrentControl();
	auto* source = VirtualSource();
	if (!control) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!source || !SupportsScroll()) return UIA_E_NOTSUPPORTED;
	if (!control->GetAccessibilitySnapshot().Enabled)
		return UIA_E_ELEMENTNOTENABLED;
	auto valid = [](double value)
	{
		return value == UIA_ScrollPatternNoScroll
			|| (std::isfinite(value) && value >= 0.0 && value <= 100.0);
	};
	if (!valid(horizontalPercent) || !valid(verticalPercent))
		return E_INVALIDARG;
	AccessibilityScrollInfo info;
	if (!source->GetAccessibilityScrollInfo(info))
		return UIA_E_INVALIDOPERATION;
	if ((!info.HorizontallyScrollable
			&& horizontalPercent != UIA_ScrollPatternNoScroll)
		|| (!info.VerticallyScrollable
			&& verticalPercent != UIA_ScrollPatternNoScroll))
		return UIA_E_INVALIDOPERATION;
	return source->SetAccessibilityScrollPercent(
		horizontalPercent, verticalPercent)
		? S_OK : UIA_E_INVALIDOPERATION;
}

HRESULT ControlUiaProvider::get_HorizontalScrollPercent(double* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.HorizontalScrollPercent;
	return S_OK;
}

HRESULT ControlUiaProvider::get_HorizontalViewSize(double* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.HorizontalViewSize;
	return S_OK;
}

HRESULT ControlUiaProvider::get_VerticalScrollPercent(double* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.VerticalScrollPercent;
	return S_OK;
}

HRESULT ControlUiaProvider::get_VerticalViewSize(double* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.VerticalViewSize;
	return S_OK;
}

HRESULT ControlUiaProvider::get_HorizontallyScrollable(BOOL* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.HorizontallyScrollable ? TRUE : FALSE;
	return S_OK;
}

HRESULT ControlUiaProvider::get_VerticallyScrollable(BOOL* value)
{
	if (!value) return E_POINTER;
	if (!CurrentControl()) return UIA_E_ELEMENTNOTAVAILABLE;
	auto* source = VirtualSource();
	AccessibilityScrollInfo info;
	if (!source || !SupportsScroll()
		|| !source->GetAccessibilityScrollInfo(info)) return UIA_E_NOTSUPPORTED;
	*value = info.VerticallyScrollable ? TRUE : FALSE;
	return S_OK;
}

HRESULT VirtualUiaProvider::CreateProviderArray(
	const std::vector<uint32_t>& ids, SAFEARRAY** value)
{
	if (!value) return E_POINTER;
	*value = nullptr;
	if (!CurrentNode(nullptr) || !_root || !_owner)
		return UIA_E_ELEMENTNOTAVAILABLE;
	SAFEARRAY* result = ::SafeArrayCreateVector(
		VT_UNKNOWN, 0, static_cast<ULONG>(ids.size()));
	if (!result) return E_OUTOFMEMORY;
	for (LONG index = 0; index < static_cast<LONG>(ids.size()); ++index)
	{
		auto* provider = _root->VirtualProviderFor(
			_owner, ids[static_cast<size_t>(index)]);
		if (!provider)
		{
			::SafeArrayDestroy(result);
			return E_OUTOFMEMORY;
		}
		IUnknown* unknown = static_cast<IRawElementProviderSimple*>(provider);
		const HRESULT hr = ::SafeArrayPutElement(result, &index, unknown);
		provider->Release();
		if (FAILED(hr))
		{
			::SafeArrayDestroy(result);
			return hr;
		}
	}
	*value = result;
	return S_OK;
}

HRESULT VirtualUiaProvider::GetColumnHeaderItems(SAFEARRAY** value)
{
	AccessibilityVirtualNode node;
	IAccessibilityVirtualizedControl* source = nullptr;
	if (!CurrentNode(&node, &source)) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!HasAccessibilityVirtualPattern(
		node.Patterns, AccessibilityVirtualPattern::TableItem))
		return UIA_E_NOTSUPPORTED;
	std::vector<uint32_t> headers;
	source->GetAccessibilityVirtualColumnHeaders(headers);
	if (node.Column < 0 || static_cast<size_t>(node.Column) >= headers.size())
		return CreateProviderArray({}, value);
	return CreateProviderArray(
		{ headers[static_cast<size_t>(node.Column)] }, value);
}

void FormUiaProvider::RaiseEvent(
	Control* control, AccessibilityChange change)
{
	if (!Connected() || !::UiaClientsAreListening()) return;
	if (change == AccessibilityChange::Structure)
	{
		(void)::UiaRaiseStructureChangedEvent(
			static_cast<IRawElementProviderSimple*>(this),
			StructureChangeType_ChildrenInvalidated, nullptr, 0);
		return;
	}
	if (!control) return;
	auto* provider = ProviderFor(control);
	if (!provider) return;
	auto* simple = static_cast<IRawElementProviderSimple*>(provider);
	if (change == AccessibilityChange::Focus)
	{
		(void)::UiaRaiseAutomationEvent(
			simple, UIA_AutomationFocusChangedEventId);
		provider->Release();
		return;
	}
	if (change == AccessibilityChange::Scroll)
	{
		constexpr PROPERTYID properties[]{
			UIA_ScrollHorizontalScrollPercentPropertyId,
			UIA_ScrollHorizontalViewSizePropertyId,
			UIA_ScrollVerticalScrollPercentPropertyId,
			UIA_ScrollVerticalViewSizePropertyId,
			UIA_ScrollHorizontallyScrollablePropertyId,
			UIA_ScrollVerticallyScrollablePropertyId
		};
		for (const auto propertyId : properties)
		{
			VARIANT oldValue{};
			VARIANT newValue{};
			::VariantInit(&oldValue);
			::VariantInit(&newValue);
			if (SUCCEEDED(provider->GetPropertyValue(propertyId, &newValue)))
				(void)::UiaRaiseAutomationPropertyChangedEvent(
					simple, propertyId, oldValue, newValue);
			::VariantClear(&newValue);
		}
		provider->Release();
		return;
	}
	PROPERTYID propertyId = 0;
	switch (change)
	{
	case AccessibilityChange::Name: propertyId = UIA_NamePropertyId; break;
	case AccessibilityChange::Description:
		propertyId = UIA_FullDescriptionPropertyId; break;
	case AccessibilityChange::Help: propertyId = UIA_HelpTextPropertyId; break;
	case AccessibilityChange::Value:
		propertyId = provider->GetAccessibilityTypeForEvent() == 1
			? UIA_RangeValueValuePropertyId : UIA_ValueValuePropertyId;
		break;
	case AccessibilityChange::Invoke:
		(void)::UiaRaiseAutomationEvent(simple, UIA_Invoke_InvokedEventId);
		break;
	case AccessibilityChange::Toggle:
		propertyId = UIA_ToggleToggleStatePropertyId; break;
	case AccessibilityChange::ExpandCollapse:
		propertyId = UIA_ExpandCollapseExpandCollapseStatePropertyId; break;
	case AccessibilityChange::Selection:
		propertyId = UIA_SelectionItemIsSelectedPropertyId; break;
	case AccessibilityChange::State:
		if (provider->SupportsToggleForEvent())
			propertyId = UIA_ToggleToggleStatePropertyId;
		else if (provider->SupportsSelectionItemForEvent())
			propertyId = UIA_SelectionItemIsSelectedPropertyId;
		else
			propertyId = UIA_IsEnabledPropertyId;
		break;
	case AccessibilityChange::Scroll: break;
	default: break;
	}
	if (propertyId != 0)
	{
		VARIANT oldValue{};
		VARIANT newValue{};
		::VariantInit(&oldValue);
		::VariantInit(&newValue);
		if (SUCCEEDED(provider->GetPropertyValue(propertyId, &newValue)))
			(void)::UiaRaiseAutomationPropertyChangedEvent(
				simple, propertyId, oldValue, newValue);
		::VariantClear(&newValue);
	}
	provider->Release();
}

void FormUiaProvider::RaiseVirtualEvent(
	Control* owner, uint32_t virtualId, AccessibilityChange change)
{
	if (!Connected() || !::UiaClientsAreListening()
		|| !owner || virtualId == 0) return;
	auto* provider = VirtualProviderFor(owner, virtualId);
	if (!provider) return;
	auto* simple = static_cast<IRawElementProviderSimple*>(provider);
	if (change == AccessibilityChange::Focus)
	{
		(void)::UiaRaiseAutomationEvent(
			simple, UIA_AutomationFocusChangedEventId);
		provider->Release();
		return;
	}
	if (change == AccessibilityChange::Structure)
	{
		(void)::UiaRaiseStructureChangedEvent(simple,
			StructureChangeType_ChildrenInvalidated, nullptr, 0);
		provider->Release();
		return;
	}
	AccessibilityVirtualNode node;
	PROPERTYID propertyId = 0;
	if (ResolveVirtualNode(owner, owner->GetAccessibilityRuntimeId(),
		virtualId, node))
	{
		switch (change)
		{
		case AccessibilityChange::Name:
			propertyId = UIA_NamePropertyId; break;
		case AccessibilityChange::Description:
		case AccessibilityChange::Help:
			propertyId = UIA_FullDescriptionPropertyId; break;
		case AccessibilityChange::Value:
			propertyId = UIA_ValueValuePropertyId; break;
		case AccessibilityChange::Invoke:
			(void)::UiaRaiseAutomationEvent(simple, UIA_Invoke_InvokedEventId);
			break;
		case AccessibilityChange::Toggle:
			propertyId = UIA_ToggleToggleStatePropertyId; break;
		case AccessibilityChange::ExpandCollapse:
			propertyId = UIA_ExpandCollapseExpandCollapseStatePropertyId; break;
		case AccessibilityChange::Selection:
			propertyId = UIA_SelectionItemIsSelectedPropertyId;
			(void)::UiaRaiseAutomationEvent(simple, node.Selected
				? UIA_SelectionItem_ElementSelectedEventId
				: UIA_SelectionItem_ElementRemovedFromSelectionEventId);
			break;
		case AccessibilityChange::State:
			if (HasAccessibilityVirtualPattern(
				node.Patterns, AccessibilityVirtualPattern::Toggle))
				propertyId = UIA_ToggleToggleStatePropertyId;
			else if (HasAccessibilityVirtualPattern(
				node.Patterns, AccessibilityVirtualPattern::SelectionItem))
				propertyId = UIA_SelectionItemIsSelectedPropertyId;
			else
				propertyId = UIA_IsEnabledPropertyId;
			break;
		default: break;
		}
	}
	if (propertyId != 0)
	{
		VARIANT oldValue{};
		VARIANT newValue{};
		::VariantInit(&oldValue);
		::VariantInit(&newValue);
		if (SUCCEEDED(provider->GetPropertyValue(propertyId, &newValue)))
			(void)::UiaRaiseAutomationPropertyChangedEvent(
				simple, propertyId, oldValue, newValue);
		::VariantClear(&newValue);
	}
	provider->Release();
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

namespace
{
	constexpr int DCompSceneLayerBand = 1000;

	D2D1_COLOR_F FromSystemColor(int index)
	{
		const COLORREF color = ::GetSysColor(index);
		return D2D1_COLOR_F{
			static_cast<float>(GetRValue(color)) / 255.0f,
			static_cast<float>(GetGValue(color)) / 255.0f,
			static_cast<float>(GetBValue(color)) / 255.0f,
			1.0f };
	}

	RECT GetPrimaryWorkArea()
	{
		RECT workArea{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
		return workArea;
	}

	RECT GetWindowWorkArea(HWND hWnd, POINT fallbackPoint)
	{
		RECT workArea = GetPrimaryWorkArea();
		HMONITOR monitor = nullptr;
		if (hWnd)
		{
			monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		}
		else
		{
			monitor = MonitorFromPoint(fallbackPoint, MONITOR_DEFAULTTONEAREST);
		}
		if (!monitor)
			return workArea;

		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(monitor, &monitorInfo))
			return monitorInfo.rcWork;
		return workArea;
	}

	POINT ClampWindowOriginToWorkArea(POINT origin, SIZE size, const RECT& workArea)
	{
		POINT clamped = origin;
		const int maxX = (std::max)(workArea.left, workArea.right - size.cx);
		const int maxY = (std::max)(workArea.top, workArea.bottom - size.cy);
		clamped.x = (std::clamp)(clamped.x, workArea.left, (LONG)maxX);
		clamped.y = (std::clamp)(clamped.y, workArea.top, (LONG)maxY);
		return clamped;
	}

	float LayoutMainTopBar(Form* form, const SIZE& clientSize)
	{
		auto* menu = form ? form->MainMenu : nullptr;
		LONG width = (std::max)(0L, clientSize.cx);
		LONG top = 0;

		if (menu && menu->Visible)
		{
			LONG height = (std::max)(0, menu->BarHeight);
			if (height > clientSize.cy)
				height = (std::max)(0L, clientSize.cy);
			menu->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
			top += height;
		}

		auto* toolBar = form ? form->MainToolBar : nullptr;
		if (toolBar && toolBar->Visible)
		{
			LONG availableHeight = clientSize.cy - top;
			if (availableHeight < 0)
				availableHeight = 0;
			SIZE measured = toolBar->Measure(SIZE{ width, availableHeight });
			LONG height = (std::max)(0L, measured.cy);
			if (height > availableHeight)
				height = availableHeight;
			toolBar->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
			top += height;
		}

		return (float)top;
	}

	float LayoutMainStatusBar(Form* form, const SIZE& clientSize)
	{
		auto* statusBar = form ? form->MainStatusBar : nullptr;
		if (!statusBar || !statusBar->TopMost || !statusBar->Visible)
		{
			return 0.0f;
		}

		SIZE measured = statusBar->Measure(clientSize);
		LONG width = (std::max)(0L, clientSize.cx);
		LONG height = (std::max)(0L, measured.cy);
		LONG top = clientSize.cy - height;
		if (top < 0)
		{
			top = 0;
			height = (std::min)(height, clientSize.cy);
		}

		statusBar->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
		return (float)height;
	}

	bool IsFormManagedChrome(Form* form, Control* control)
	{
		if (!form || !control) return false;
		return control->Type() == UIClass::UI_Menu
			|| control == form->MainToolBar
			|| control == form->MainStatusBar;
	}

	std::vector<Control*> GetFormLayoutControls(Form* form)
	{
		std::vector<Control*> result;
		if (!form) return result;
		result.reserve(form->Controls.size());
		for (auto* control : form->Controls)
		{
			if (!control || IsFormManagedChrome(form, control)) continue;
			result.push_back(control);
		}
		return result;
	}

	// Compatibility bridge for the existing LayoutEngine(Control*) contract.
	// It exposes Form's non-chrome children without taking ownership or changing
	// their real Parent/ParentForm relationships.
	class FormLayoutRootHost final : public Control
	{
		std::vector<Control*> _layoutChildren;

	public:
		FormLayoutRootHost(Form* form, std::vector<Control*> children, SIZE size)
			: _layoutChildren(std::move(children))
		{
			this->ParentForm = form;
			this->_size = size;
			this->UpdateLayoutBaseSize(size);
			this->_layoutState.CommitArrange(cui::core::Rect{
				0.0f, 0.0f, (float)size.cx, (float)size.cy });
		}

		std::span<Control* const> GetLayoutChildrenView() noexcept override
		{
			return std::span<Control* const>{
				_layoutChildren.data(), _layoutChildren.size() };
		}

		FormLayoutRootHost(const FormLayoutRootHost&) = delete;
		FormLayoutRootHost& operator=(const FormLayoutRootHost&) = delete;
	};

	HICON LoadProcessIcon(bool wantSmall)
	{
		static HICON largeIcon = nullptr;
		static HICON smallIcon = nullptr;
		HICON& cached = wantSmall ? smallIcon : largeIcon;
		if (cached) return cached;

		wchar_t exePath[MAX_PATH]{};
		if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
		{
			HICON large = nullptr;
			HICON smallHandle = nullptr;
			if (ExtractIconExW(exePath, 0, &large, &smallHandle, 1) > 0)
			{
				if (wantSmall)
				{
					cached = smallHandle ? smallHandle : large;
					if (large && large != cached) DestroyIcon(large);
				}
				else
				{
					cached = large ? large : smallHandle;
					if (smallHandle && smallHandle != cached) DestroyIcon(smallHandle);
				}
			}
		}

		if (!cached)
		{
			cached = (HICON)LoadImageW(
				nullptr,
				IDI_APPLICATION,
				IMAGE_ICON,
				wantSmall ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON),
				wantSmall ? GetSystemMetrics(SM_CYSMICON) : GetSystemMetrics(SM_CYICON),
				LR_SHARED);
		}

		return cached;
	}
}

HCURSOR Form::GetSystemCursor(CursorKind kind)
{
	static std::unordered_map<CursorKind, HCURSOR> cache;
	auto it = cache.find(kind);
	if (it != cache.end() && it->second) return it->second;

	LPCWSTR id = IDC_ARROW;
	switch (kind)
	{
	case CursorKind::Arrow: id = IDC_ARROW; break;
	case CursorKind::Cross: id = IDC_CROSS; break;
	case CursorKind::Hand: id = IDC_HAND; break;
	case CursorKind::IBeam: id = IDC_IBEAM; break;
	case CursorKind::SizeWE: id = IDC_SIZEWE; break;
	case CursorKind::SizeNS: id = IDC_SIZENS; break;
	case CursorKind::SizeNWSE: id = IDC_SIZENWSE; break;
	case CursorKind::SizeNESW: id = IDC_SIZENESW; break;
	case CursorKind::SizeAll: id = IDC_SIZEALL; break;
	case CursorKind::No: id = IDC_NO; break;
	default: id = IDC_ARROW; break;
	}
	HCURSOR h = LoadCursorW(nullptr, id);
	cache.emplace(kind, h);
	return h;
}

void Form::SetImeCompositionWindowFromLogicalRect(const D2D1_RECT_F& logicalRect)
{
	if (!this->Handle || !::IsWindow(this->Handle)) return;

	HIMC hImc = ImmGetContext(this->Handle);
	if (!hImc) return;

	float dpiScale = GetDpiScale();
	if (dpiScale <= 0.0f) dpiScale = 1.0f;
	float headLogical = this->VisibleHead ? ((float)this->HeadHeight / dpiScale) : 0.0f;

	LONG left = (LONG)std::lround(logicalRect.left * dpiScale);
	LONG top = (LONG)std::lround((logicalRect.top + headLogical) * dpiScale);
	LONG right = (LONG)std::lround(logicalRect.right * dpiScale);
	LONG bottom = (LONG)std::lround((logicalRect.bottom + headLogical) * dpiScale);
	if (right < left) right = left;
	if (bottom < top) bottom = top;

	POINT anchor{ left, bottom };
	COMPOSITIONFORM composition{};
	composition.dwStyle = CFS_POINT;
	composition.ptCurrentPos = anchor;
	ImmSetCompositionWindow(hImc, &composition);

	CANDIDATEFORM candidate{};
	candidate.dwStyle = CFS_EXCLUDE;
	candidate.ptCurrentPos = anchor;
	candidate.rcArea = RECT{
		left,
		top,
		(std::max)(left + 1, right),
		(std::max)(top + 1, bottom)
	};
	for (DWORD index = 0; index < 4; ++index)
	{
		candidate.dwIndex = index;
		ImmSetCandidateWindow(hImc, &candidate);
	}

	ImmReleaseContext(this->Handle, hImc);
}

void Form::ApplyCursor(CursorKind kind)
{
	HCURSOR desired = GetSystemCursor(kind);
	if (kind == _currentCursor && ::GetCursor() == desired) return;
	_currentCursor = kind;
	::SetCursor(desired);
}

bool Form::ApplySystemCursorId(UINT32 cursorId)
{
	if (cursorId == 0) return false;
	HCURSOR cursor = LoadCursorW(nullptr, MAKEINTRESOURCEW((ULONG_PTR)cursorId));
	if (!cursor) return false;
	::SetCursor(cursor);
	return true;
}

static int ToLegacyLocalCoordinate(float value)
{
	return static_cast<int>(std::floor(value));
}

static Control* HitTestDeepestChild(Control* root, POINT contentMouse)
{
	if (!root) return nullptr;
	if (!root->Visible || !root->Enable) return nullptr;
	const auto rootAbs = root->GetAbsoluteLocationDip();
	const int localX = ToLegacyLocalCoordinate((float)contentMouse.x - rootAbs.x);
	const int localY = ToLegacyLocalCoordinate((float)contentMouse.y - rootAbs.y);
	if (!root->ShouldHitTestChildrenAt(localX, localY))
		return root;

	for (auto child : root->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		const auto childRect = child->GetAbsoluteRectDip();
		if (childRect.Contains(cui::core::Point{
			(float)contentMouse.x, (float)contentMouse.y }))
		{
			auto deeperChild = HitTestDeepestChild(child, contentMouse);
			return deeperChild ? deeperChild : child;
		}
	}
	return root;
}

static bool PointInControlRect(Control* control, POINT contentMouse)
{
	if (!control) return false;
	if (!control->Visible || !control->Enable) return false;
	const auto location = control->GetAbsoluteLocationDip();
	return control->ContainsPoint(
		ToLegacyLocalCoordinate((float)contentMouse.x - location.x),
		ToLegacyLocalCoordinate((float)contentMouse.y - location.y));
}

static bool PointInForegroundControlRect(Control* control, POINT contentMouse)
{
	if (!control) return false;
	if (!control->Visible || !control->Enable) return false;
	const auto location = control->GetAbsoluteLocationDip();
	return control->ContainsForegroundPoint(
		ToLegacyLocalCoordinate((float)contentMouse.x - location.x),
		ToLegacyLocalCoordinate((float)contentMouse.y - location.y));
}

static bool IsControlOrDescendantOf(Control* control, Control* ancestor)
{
	for (auto current = control; current; current = current->Parent)
	{
		if (current == ancestor)
			return true;
	}
	return false;
}

static void SyncFormWindowStyles(HWND hWnd, bool showInTaskBar, bool minBox, bool maxBox, bool closeBox, bool allowResize)
{
	if (!hWnd)
		return;

	LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
	style &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME);
	if (minBox) style |= WS_MINIMIZEBOX;
	if (maxBox) style |= WS_MAXIMIZEBOX;
	if (closeBox) style |= WS_SYSMENU;
	if (allowResize) style |= WS_THICKFRAME;
	SetWindowLongPtrW(hWnd, GWL_STYLE, style);

	LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
	if (showInTaskBar)
	{
		exStyle &= ~WS_EX_TOOLWINDOW;
		exStyle |= WS_EX_APPWINDOW;
	}
	else
	{
		exStyle &= ~WS_EX_APPWINDOW;
		exStyle |= WS_EX_TOOLWINDOW;
	}
	SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);

	SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	const int cornerPreference = 1;
	DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
}

static int GetCustomFrameInset()
{
	return GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static std::vector<Control*> GetRootControlsInZOrder(Form* form)
{
	std::vector<Control*> result;
	if (!form) return result;
	result = form->Controls;
	std::stable_sort(result.begin(), result.end(), [](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	return result;
}

static std::vector<Control*> GetRootControlsInReverseZOrder(Form* form)
{
	auto result = GetRootControlsInZOrder(form);
	std::reverse(result.begin(), result.end());
	return result;
}

Control* Form::HitTestControlAt(POINT contentMouse)
{
	// 1) 置顶控件优先命中（ComboBox 下拉等）
	if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl->Enable)
	{
		auto* foregroundControl = this->ForegroundControl;
		if (PointInForegroundControlRect(foregroundControl, contentMouse))
		{
			return HitTestDeepestChild(foregroundControl, contentMouse);
		}
	}

	// 2) 主菜单单独优先命中（包含下拉区域）
	if (this->MainMenu && this->MainMenu->Visible && this->MainMenu->Enable)
	{
		auto* mainMenu = this->MainMenu;
		if (PointInControlRect(mainMenu, contentMouse))
		{
			return HitTestDeepestChild(mainMenu, contentMouse);
		}
	}

	// 3) 状态栏：置顶于普通控件（但优先级低于主菜单与前景控件）
	if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible && this->MainStatusBar->Enable)
	{
		auto* statusBar = this->MainStatusBar;
		if (PointInControlRect(statusBar, contentMouse))
		{
			return HitTestDeepestChild(statusBar, contentMouse);
		}
	}

	// 4) 普通控件：按绘制顺序倒序命中（后绘制者优先）
	for (auto control : GetRootControlsInReverseZOrder(this))
	{
		if (!control || !control->Visible || !control->Enable) continue;
		if (control == this->ForegroundControl && !control->RenderNormalWhenForeground()) continue;
		if (control == this->MainMenu) continue;
		if (this->MainStatusBar && this->MainStatusBar->TopMost && control == this->MainStatusBar) continue;
		if (!PointInControlRect(control, contentMouse)) continue;
		return HitTestDeepestChild(control, contentMouse);
	}
	return nullptr;
}

static bool IsScrollViewFallbackKey(WPARAM key)
{
	switch (key)
	{
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}

static Control* FindAncestorScrollViewForFallback(Control* start, WPARAM key)
{
	if (!start) return nullptr;
	for (Control* parent = start->Parent; parent; parent = parent->Parent)
	{
		if (parent->Type() == UIClass::UI_ScrollView && parent->HandlesNavigationKey(key))
			return parent;
	}
	return nullptr;
}

static Control* GetScrollViewFallbackTarget(Control* selected, WPARAM key)
{
	if (!selected) return nullptr;
	if (!selected->IsVisual) return nullptr;
	if (!IsScrollViewFallbackKey(key)) return nullptr;
	if (selected->HandlesNavigationKey(key)) return nullptr;
	return FindAncestorScrollViewForFallback(selected, key);
}

static bool IsKeyboardInvokable(Control* control)
{
	if (!control) return false;
	switch (control->GetEffectiveAccessibleRole())
	{
	case AccessibleRole::Button:
	case AccessibleRole::Link:
	case AccessibleRole::CheckBox:
	case AccessibleRole::RadioButton:
	case AccessibleRole::Switch:
		return true;
	default:
		return false;
	}
}

static bool DataObjectHasFormat(IDataObject* pDataObj, CLIPFORMAT cf)
{
	if (!pDataObj) return false;
	FORMATETC fmt{};
	fmt.cfFormat = cf;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	return SUCCEEDED(pDataObj->QueryGetData(&fmt));
}

static std::optional<std::vector<std::wstring>> TryExtractDroppedFiles(IDataObject* pDataObj)
{
	if (!pDataObj) return std::nullopt;
	FORMATETC fmt{};
	fmt.cfFormat = CF_HDROP;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	STGMEDIUM stg{};
	if (FAILED(pDataObj->GetData(&fmt, &stg))) return std::nullopt;

	std::vector<std::wstring> files;
	HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
	if (hDrop)
	{
		UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
		WCHAR buf[MAX_PATH];
		for (UINT i = 0; i < count; i++)
		{
			buf[0] = 0;
			DragQueryFileW(hDrop, i, buf, MAX_PATH);
			files.push_back(buf);
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	if (files.size() <= 0) return std::nullopt;
	return files;
}

static std::optional<std::wstring> TryExtractDroppedText(IDataObject* pDataObj)
{
	if (!pDataObj) return std::nullopt;
	CLIPFORMAT fmtText = CF_UNICODETEXT;
	if (!DataObjectHasFormat(pDataObj, fmtText))
	{
		fmtText = CF_TEXT;
		if (!DataObjectHasFormat(pDataObj, fmtText))
			return std::nullopt;
	}

	FORMATETC fmt{};
	fmt.cfFormat = fmtText;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	STGMEDIUM stg{};
	if (FAILED(pDataObj->GetData(&fmt, &stg))) return std::nullopt;

	std::optional<std::wstring> result;
	void* p = GlobalLock(stg.hGlobal);
	if (p)
	{
		if (fmtText == CF_UNICODETEXT)
		{
			result = std::wstring((const wchar_t*)p);
		}
		else
		{
			// ANSI -> UTF-16
			const char* s = (const char*)p;
			int len = (int)strlen(s);
			int wlen = MultiByteToWideChar(CP_ACP, 0, s, len, nullptr, 0);
			std::wstring ws;
			ws.resize(wlen);
			if (wlen > 0)
				MultiByteToWideChar(CP_ACP, 0, s, len, ws.data(), wlen);
			result = std::move(ws);
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	if (result && result->empty()) return std::nullopt;
	return result;
}

class FormDropTarget final : public IDropTarget
{
public:
	explicit FormDropTarget(Form* f) : _ref(1), _form(f) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject) return E_POINTER;
		*ppvObject = nullptr;
		if (riid == IID_IUnknown || riid == IID_IDropTarget)
		{
			*ppvObject = static_cast<IDropTarget*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef(void) override { return InterlockedIncrement(&_ref); }
	ULONG STDMETHODCALLTYPE Release(void) override
	{
		ULONG r = InterlockedDecrement(&_ref);
		if (r == 0) delete this;
		return r;
	}

	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		_lastDataObj = pDataObj;
		return DragOver(grfKeyState, pt, pdwEffect);
	}
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		if (!pdwEffect) return E_POINTER;
		*pdwEffect = DROPEFFECT_NONE;
		if (!_form || !_form->Handle) return S_OK;

		POINT client{ pt.x, pt.y };
		ScreenToClient(_form->Handle, &client);
		// physical→logical: OS gives physical px; controls live in logical (96-DPI) coords
		const float sc_ = _form->GetDpiScale();
		POINT contentMouse{ (LONG)(client.x / sc_), (LONG)((client.y - _form->ClientTop()) / sc_) };
		if (_form->VisibleHead && client.y < _form->ClientTop())
			return S_OK;

		auto* target = _form->HitTestControlAt(contentMouse);
		bool hasFiles = DataObjectHasFormat(_lastDataObj, CF_HDROP);
		bool hasText = DataObjectHasFormat(_lastDataObj, CF_UNICODETEXT) || DataObjectHasFormat(_lastDataObj, CF_TEXT);

		auto canAcceptFiles = [&](Control* c) -> bool { return c && c->OnDropFile.Count() > 0; };
		auto canAcceptText = [&](Control* c) -> bool { return c && c->OnDropText.Count() > 0; };

		bool accept = false;
		if (target)
		{
			if (hasFiles && canAcceptFiles(target)) accept = true;
			else if (hasText && canAcceptText(target)) accept = true;
		}
		else
		{
			if (hasFiles && _form->OnDropFile.Count() > 0) accept = true;
			else if (hasText && _form->OnDropText.Count() > 0) accept = true;
		}

		if (accept)
			*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DragLeave(void) override
	{
		_lastDataObj = nullptr;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		if (!pdwEffect) return E_POINTER;
		*pdwEffect = DROPEFFECT_NONE;
		if (!_form || !_form->Handle || !pDataObj) return S_OK;

		POINT client{ pt.x, pt.y };
		ScreenToClient(_form->Handle, &client);
		const float sc_ = _form->GetDpiScale();
		POINT contentMouse{ (LONG)(client.x / sc_), (LONG)((client.y - _form->ClientTop()) / sc_) };
		if (_form->VisibleHead && client.y < _form->ClientTop())
			return S_OK;

		auto* target = _form->HitTestControlAt(contentMouse);

		if (auto files = TryExtractDroppedFiles(pDataObj))
		{
			if (target && target->OnDropFile.Count() > 0)
			{
				target->OnDropFile(target, *files);
				*pdwEffect = DROPEFFECT_COPY;
			}
			else if (!target && _form->OnDropFile.Count() > 0)
			{
				_form->OnDropFile(_form, *files);
				*pdwEffect = DROPEFFECT_COPY;
			}
			return S_OK;
		}

		if (auto text = TryExtractDroppedText(pDataObj))
		{
			if (target && target->OnDropText.Count() > 0)
			{
				target->OnDropText(target, *text);
				*pdwEffect = DROPEFFECT_COPY;
			}
			else if (!target && _form->OnDropText.Count() > 0)
			{
				_form->OnDropText(_form, *text);
				*pdwEffect = DROPEFFECT_COPY;
			}
			return S_OK;
		}

		return S_OK;
	}

private:
	volatile LONG _ref;
	Form* _form;
	IDataObject* _lastDataObj = nullptr;
};

static Control* HitTestRootControlAt(Form* form, POINT contentMouse)
{
	if (!form) return nullptr;

	// 1) ForegroundControl 顶层优先
	if (form->ForegroundControl && form->ForegroundControl->Visible && form->ForegroundControl->Enable)
	{
		auto* foregroundControl = form->ForegroundControl;
		if (PointInForegroundControlRect(foregroundControl, contentMouse))
			return foregroundControl;
	}

	// 2) 主菜单次优先
	if (form->MainMenu && form->MainMenu->Visible && form->MainMenu->Enable)
	{
		auto* mainMenu = form->MainMenu;
		if (PointInControlRect(mainMenu, contentMouse))
			return mainMenu;
	}

	// 3) 状态栏（TopMost=true）
	if (form->MainStatusBar && form->MainStatusBar->TopMost && form->MainStatusBar->Visible && form->MainStatusBar->Enable)
	{
		auto* statusBar = form->MainStatusBar;
		if (PointInControlRect(statusBar, contentMouse))
			return statusBar;
	}

	// 4) 普通控件按绘制顺序倒序命中
	for (auto control : GetRootControlsInReverseZOrder(form))
	{
		if (!control || !control->Visible || !control->Enable) continue;
		if (control == form->ForegroundControl && !control->RenderNormalWhenForeground()) continue;
		if (control == form->MainMenu) continue;
		if (form->MainStatusBar && form->MainStatusBar->TopMost && control == form->MainStatusBar) continue;
		if (!PointInControlRect(control, contentMouse)) continue;
		return control;
	}
	return nullptr;
}

static void DismissForegroundOnOutsideMouseDown(Form* form, Control* hitControl, POINT contentMouse, UINT message)
{
	if (!form) return;
	if (message != WM_LBUTTONDOWN && message != WM_RBUTTONDOWN && message != WM_MBUTTONDOWN) return;
	bool wasDismissed = false;
	if (form->ForegroundControl && form->ForegroundControl->Visible && form->ForegroundControl->Enable)
	{
		if (!IsControlOrDescendantOf(hitControl, form->ForegroundControl) && form->ForegroundControl->AutoCloseOnOutsideClick())
		{
			form->ForegroundControl->ClosePopup();
			wasDismissed = true;
		}
	}
	if (form->MainMenu && form->MainMenu->Visible && form->MainMenu->Enable)
	{
		if (!PointInControlRect(form->MainMenu, contentMouse) && form->MainMenu->AutoCloseOnOutsideClick())
		{
			form->MainMenu->ClosePopup();
			wasDismissed = true;
		}
	}
	if (wasDismissed)
		form->Invalidate(true);
}

CursorKind Form::QueryCursorAt(POINT mouseClient, POINT contentMouse)
{
	const int titleBarHeight = ClientTop();
	if (this->VisibleHead && mouseClient.y < titleBarHeight)
	{
		return CursorKind::Arrow;
	}

	auto hitControl = HitTestControlAt(contentMouse);

	if (this->Selected && this->Selected->IsVisual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
	{
		bool keepSelectedCursor = (::GetCapture() == this->Handle);
		if (!keepSelectedCursor)
		{
			keepSelectedCursor = (hitControl == this->Selected) || PointInControlRect(this->Selected, contentMouse);
		}
		if (keepSelectedCursor)
		{
			const auto selectedLocation = this->Selected->GetAbsoluteLocationDip();
			int localX = ToLegacyLocalCoordinate((float)contentMouse.x - selectedLocation.x);
			int localY = ToLegacyLocalCoordinate((float)contentMouse.y - selectedLocation.y);
			return this->Selected->QueryCursor(localX, localY);
		}
	}

	if (!hitControl) return CursorKind::Arrow;
	const auto hitLocation = hitControl->GetAbsoluteLocationDip();
	int localX = ToLegacyLocalCoordinate((float)contentMouse.x - hitLocation.x);
	int localY = ToLegacyLocalCoordinate((float)contentMouse.y - hitLocation.y);
	return hitControl->QueryCursor(localX, localY);
}

void Form::UpdateCursor(POINT mouseClient, POINT contentMouse)
{
	const int titleBarHeight = ClientTop();
	if (!(this->VisibleHead && mouseClient.y < titleBarHeight))
	{
		auto hitControl = HitTestControlAt(contentMouse);

		if (this->Selected && this->Selected->IsVisual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			bool keepSelectedCursor = (::GetCapture() == this->Handle);
			if (!keepSelectedCursor)
			{
				keepSelectedCursor = (hitControl == this->Selected) || PointInControlRect(this->Selected, contentMouse);
			}
			if (keepSelectedCursor)
			{
				UINT32 cursorId = 0;
				if (this->Selected->TryGetSystemCursorId(cursorId) && ApplySystemCursorId(cursorId))
					return;
			}
		}

		for (Control* target = hitControl; target; target = target->Parent)
		{
			UINT32 cursorId = 0;
			if (target->TryGetSystemCursorId(cursorId) && ApplySystemCursorId(cursorId))
				return;
		}
	}

	ApplyCursor(QueryCursorAt(mouseClient, contentMouse));
}

void Form::UpdateCursorFromCurrentMouse()
{
	if (!this->Handle) return;
	POINT mouse{};
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	const float dpiScale = GetDpiScale();
	POINT contentMouse{ (LONG)(mouse.x / dpiScale), (LONG)((mouse.y - ClientTop()) / dpiScale) };
	UpdateCursor(mouse, contentMouse);
}

void Form::SetSelectedControl(Control* value, bool invalidateVisual)
{
	auto* previousSelection = this->Selected;
	if (previousSelection == value) return;
	this->Selected = value;
	if (previousSelection)
	{
		previousSelection->OnLostFocus(previousSelection);
		if (invalidateVisual) previousSelection->InvalidateVisual();
		NotifyAccessibilityEvent(previousSelection, AccessibilityChange::State);
	}
	if (value)
	{
		value->OnGotFocus(value);
		if (invalidateVisual) value->InvalidateVisual();
		NotifyAccessibilityEvent(value, AccessibilityChange::Focus);
	}
	this->_focusNotifiedSelected = this->Selected;
}

std::vector<Control*> Form::BuildTabOrder(std::span<Control* const> roots)
{
	struct Entry
	{
		Control* Value = nullptr;
		int TabIndex = 0;
		size_t TreeOrder = 0;
	};
	std::vector<Entry> entries;
	size_t treeOrder = 0;
	auto visit = [&](Control* control, const auto& self) -> void
	{
		if (!control) return;
		const size_t currentOrder = treeOrder++;
		if (control->CanReceiveKeyboardFocus())
			entries.push_back(Entry{ control, control->TabIndex, currentOrder });
		for (auto* child : control->Children)
			self(child, self);
	};
	for (auto* root : roots)
		visit(root, visit);

	std::stable_sort(entries.begin(), entries.end(),
		[](const Entry& left, const Entry& right)
		{
			if (left.TabIndex != right.TabIndex)
				return left.TabIndex < right.TabIndex;
			return left.TreeOrder < right.TreeOrder;
		});
	std::vector<Control*> result;
	result.reserve(entries.size());
	for (const auto& entry : entries)
		result.push_back(entry.Value);
	return result;
}

std::vector<Control*> Form::GetTabOrder() const
{
	return BuildTabOrder(std::span<Control* const>(Controls.data(), Controls.size()));
}

std::vector<Control*> Form::GetAccessibleControls() const
{
	std::vector<Control*> result;
	auto visit = [&](Control* control, const auto& self) -> void
	{
		if (!control) return;
		result.push_back(control);
		for (auto* child : control->Children)
			self(child, self);
	};
	for (auto* root : Controls)
		visit(root, visit);
	return result;
}

void Form::NotifyAccessibilityEvent(Control* control, AccessibilityChange change)
{
	if (!Handle || !::IsWindow(Handle)) return;
	if (_uiaProvider)
		_uiaProvider->RaiseEvent(control, change);
	DWORD eventId = EVENT_OBJECT_STATECHANGE;
	switch (change)
	{
	case AccessibilityChange::Name: eventId = EVENT_OBJECT_NAMECHANGE; break;
	case AccessibilityChange::Description: eventId = EVENT_OBJECT_DESCRIPTIONCHANGE; break;
	case AccessibilityChange::Help: eventId = EVENT_OBJECT_HELPCHANGE; break;
	case AccessibilityChange::Value: eventId = EVENT_OBJECT_VALUECHANGE; break;
	case AccessibilityChange::Focus: eventId = EVENT_OBJECT_FOCUS; break;
	case AccessibilityChange::Structure: eventId = EVENT_OBJECT_REORDER; break;
	case AccessibilityChange::Invoke:
	case AccessibilityChange::Toggle:
	case AccessibilityChange::ExpandCollapse:
	case AccessibilityChange::Selection:
	case AccessibilityChange::State:
	default: eventId = EVENT_OBJECT_STATECHANGE; break;
	}
	long childId = CHILDID_SELF;
	if (control)
	{
		if (_accessibleObject)
			childId = _accessibleObject->ChildIdFor(control);
		else
		{
			auto controls = GetAccessibleControls();
			auto position = std::find(controls.begin(), controls.end(), control);
			if (position != controls.end())
				childId = static_cast<long>(position - controls.begin()) + 1;
		}
	}
	::NotifyWinEvent(eventId, Handle, OBJID_CLIENT, childId);
}

void Form::NotifyAccessibilityVirtualEvent(
	Control* owner, uint32_t virtualId, AccessibilityChange change)
{
	if (!Handle || !::IsWindow(Handle) || !owner || virtualId == 0) return;
	if (_uiaProvider)
		_uiaProvider->RaiseVirtualEvent(owner, virtualId, change);
	// MSAA retains its compatibility simple-child model; notify the owning child.
	NotifyAccessibilityEvent(owner, change);
}

LRESULT Form::HandleAccessibleObjectRequest(WPARAM wParam, LPARAM lParam)
{
	if (static_cast<LONG>(lParam) == UiaRootObjectId)
	{
		if (!_uiaProvider)
			_uiaProvider = new (std::nothrow) FormUiaProvider(this);
		if (!_uiaProvider) return 0;
		return ::UiaReturnRawElementProvider(
			Handle, wParam, lParam,
			static_cast<IRawElementProviderSimple*>(_uiaProvider));
	}
	if (static_cast<LONG>(lParam) != OBJID_CLIENT) return 0;
	if (!_accessibleObject)
		_accessibleObject = new (std::nothrow) FormAccessibleObject(this);
	if (!_accessibleObject) return 0;
	return ::LresultFromObject(
		IID_IAccessible, wParam, static_cast<IAccessible*>(_accessibleObject));
}

bool Form::MoveFocus(bool forward)
{
	auto tabOrder = GetTabOrder();
	if (tabOrder.empty()) return false;
	auto current = std::find(tabOrder.begin(), tabOrder.end(), Selected);
	size_t index = 0;
	if (current == tabOrder.end())
		index = forward ? 0 : tabOrder.size() - 1;
	else
	{
		const size_t currentIndex = static_cast<size_t>(current - tabOrder.begin());
		index = forward
			? (currentIndex + 1) % tabOrder.size()
			: (currentIndex + tabOrder.size() - 1) % tabOrder.size();
	}
	if (Handle && ::GetFocus() != Handle)
		::SetFocus(Handle);
	SetSelectedControl(tabOrder[index], true);
	return Selected == tabOrder[index];
}

bool Form::ProcessAccessKey(wchar_t key)
{
	key = static_cast<wchar_t>(std::towupper(key));
	if (key == L'\0') return false;
	auto tabOrder = GetTabOrder();
	if (tabOrder.empty()) return false;
	const auto current = std::find(tabOrder.begin(), tabOrder.end(), Selected);
	const size_t start = current == tabOrder.end()
		? 0
		: (static_cast<size_t>(current - tabOrder.begin()) + 1) % tabOrder.size();
	for (size_t offset = 0; offset < tabOrder.size(); ++offset)
	{
		auto* candidate = tabOrder[(start + offset) % tabOrder.size()];
		if (!candidate || candidate->GetEffectiveAccessKey() != key) continue;
		if (Handle && ::GetFocus() != Handle)
			::SetFocus(Handle);
		SetSelectedControl(candidate, true);
		(void)candidate->Invoke();
		return true;
	}
	return false;
}

bool Form::SetDefaultButton(Button* button)
{
	if (button && (button->ParentForm != this || button->Type() != UIClass::UI_Button))
		return false;
	_defaultButton = button;
	return true;
}

bool Form::SetCancelButton(Button* button)
{
	if (button && (button->ParentForm != this || button->Type() != UIClass::UI_Button))
		return false;
	_cancelButton = button;
	return true;
}

static void RaiseControlMouseEnterLeave(Form* form, Control* previousHover, Control* newHover, POINT contentMouse)
{
	if (!form) return;
	if (previousHover == newHover) return;
	const bool validationToolTipChanged =
		(previousHover && previousHover->ShouldShowValidationToolTip())
		|| (newHover && newHover->ShouldShowValidationToolTip());

	auto makeArgs = [&](Control* control) -> MouseEventArgs
		{
			if (!control) return MouseEventArgs(MouseButtons::None, 0, 0, 0, 0);
			const auto controlLocation = control->GetAbsoluteLocationDip();
			return MouseEventArgs(
				MouseButtons::None, 0,
				ToLegacyLocalCoordinate((float)contentMouse.x - controlLocation.x),
				ToLegacyLocalCoordinate((float)contentMouse.y - controlLocation.y), 0);
		};

	if (previousHover)
	{
		auto args = makeArgs(previousHover);
		previousHover->OnMouseLeave(previousHover, args);
		previousHover->InvalidateVisual();
	}
	if (newHover)
	{
		auto args = makeArgs(newHover);
		newHover->OnMouseEnter(newHover, args);
		newHover->InvalidateVisual();
	}
	if (validationToolTipChanged)
		form->Invalidate(false);
}

bool Form::TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	const float dpiScale = GetDpiScale();
	int rightEdge = (int)(this->Size.cx / dpiScale);  // logical width
	int buttonHeight = (int)(this->HeadHeight / dpiScale);    // logical = _headHeightBase96
	int buttonWidth = buttonHeight;

	auto place = [&](CaptionButtonKind k, bool enabled) -> std::optional<RECT>
		{
			if (!enabled) return std::nullopt;
			RECT rect{ rightEdge - buttonWidth, 0, rightEdge, buttonHeight };
			rightEdge -= buttonWidth;
			return rect;
		};

	auto closeR = place(CaptionButtonKind::Close, this->CloseBox);
	auto maxR = place(CaptionButtonKind::Maximize, this->MaxBox);
	auto minR = place(CaptionButtonKind::Minimize, this->MinBox);

	auto pick = [&](CaptionButtonKind k) -> std::optional<RECT>
		{
			if (k == CaptionButtonKind::Close) return closeR;
			if (k == CaptionButtonKind::Maximize) return maxR;
			return minR;
		};

	auto rect = pick(kind);
	if (!rect.has_value()) return false;
	out = rect.value();
	return true;
}

bool Form::HitTestCaptionButtons(POINT clientPoint, CaptionButtonKind& outKind)
{
	// clientPoint is in physical pixels (from OS); TryGetCaptionButtonRect returns logical rects.
	const float dpiScale = GetDpiScale();
	POINT logicalPoint{ (LONG)(clientPoint.x / dpiScale), (LONG)(clientPoint.y / dpiScale) };
	RECT rect{};
	if (TryGetCaptionButtonRect(CaptionButtonKind::Close, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Close;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Maximize, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Maximize;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Minimize, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Minimize;
		return true;
	}
	return false;
}

bool Form::HitTestCaptionButtonResizeExclusion(POINT clientPoint)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	const float dpiScale = GetDpiScale();
	POINT logicalPoint{ (LONG)(clientPoint.x / dpiScale), (LONG)(clientPoint.y / dpiScale) };
	const int padding = (std::max)(2, (int)std::ceil((float)GetCustomFrameInset() / dpiScale));

	RECT unionRect{};
	bool hasRect = false;
	const CaptionButtonKind kinds[] = { CaptionButtonKind::Close, CaptionButtonKind::Maximize, CaptionButtonKind::Minimize };
	for (auto kind : kinds)
	{
		RECT rect{};
		if (!TryGetCaptionButtonRect(kind, rect)) continue;
		if (!hasRect)
		{
			unionRect = rect;
			hasRect = true;
		}
		else
		{
			unionRect.left = (std::min)(unionRect.left, rect.left);
			unionRect.top = (std::min)(unionRect.top, rect.top);
			unionRect.right = (std::max)(unionRect.right, rect.right);
			unionRect.bottom = (std::max)(unionRect.bottom, rect.bottom);
		}
	}
	if (!hasRect) return false;

	InflateRect(&unionRect, padding, padding);
	return PtInRect(&unionRect, logicalPoint) != FALSE;
}

void Form::ClearCaptionStates()
{
	_capMinState = CaptionButtonState::None;
	_capMaxState = CaptionButtonState::None;
	_capCloseState = CaptionButtonState::None;
	_capPressed = false;
	_capTracking = false;
}

void Form::UpdateCaptionHover(POINT clientPoint)
{
	if (!this->VisibleHead) return;
	CaptionButtonKind hit{};
	bool isButtonHovered = HitTestCaptionButtons(clientPoint, hit);

	auto previousMinState = _capMinState;
	auto previousMaxState = _capMaxState;
	auto previousCloseState = _capCloseState;

	_capMinState = (isButtonHovered && hit == CaptionButtonKind::Minimize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capMaxState = (isButtonHovered && hit == CaptionButtonKind::Maximize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capCloseState = (isButtonHovered && hit == CaptionButtonKind::Close) ? CaptionButtonState::Hover : CaptionButtonState::None;

	if (_capPressed)
	{
		if (_capPressedKind == CaptionButtonKind::Minimize) _capMinState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Maximize) _capMaxState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Close) _capCloseState = CaptionButtonState::Pressed;
	}

	if (previousMinState != _capMinState || previousMaxState != _capMaxState || previousCloseState != _capCloseState)
	{
		RECT titleBarRect = TitleBarRectClient();
		Invalidate(titleBarRect, false);
	}
}

void Form::ExecuteCaptionButton(CaptionButtonKind kind)
{
	switch (kind)
	{
	case CaptionButtonKind::Minimize:
		ShowWindow(this->Handle, SW_MINIMIZE);
		break;
	case CaptionButtonKind::Maximize:
		if (!this->AllowResize)
			break;
		if (IsZoomed(this->Handle))
			ShowWindow(this->Handle, SW_RESTORE);
		else
			ShowWindow(this->Handle, SW_MAXIMIZE);
		break;
	case CaptionButtonKind::Close:
		this->Close();
		break;
	}
	this->Invalidate(true);
}


void Form::Invalidate(bool immediate)
{
	if (_layoutDeferral.IsSuspended())
	{
		this->ControlChanged = true;
		_layoutDeferral.QueueFullVisual(immediate);
		return;
	}
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, nullptr, FALSE);
	// When the window is disabled/hidden (e.g. during a modal dialog), forcing
	// UpdateWindow can create excessive WM_PAINT churn. Let the system schedule paint.
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(const RECT& rect, bool immediate)
{
	if (_layoutDeferral.IsSuspended())
	{
		this->ControlChanged = true;
		_layoutDeferral.QueueVisual(cui::core::Rect::FromLTRB(
			(float)rect.left, (float)rect.top,
			(float)rect.right, (float)rect.bottom), immediate);
		return;
	}
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, &rect, FALSE);
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(D2D1_RECT_F rect, bool immediate)
{
	RECT clientRect = ToRECT(rect, 2);
	Invalidate(clientRect, immediate);
}

RECT Form::ContentDipRectToClientPixels(const D2D1_RECT_F& contentRect, float inflateDip) const
{
	float dpiScale = GetDpiScale();
	if (dpiScale <= 0.0f) dpiScale = 1.0f;

	const float padding = (std::max)(0.0f, inflateDip);
	const LONG contentTop = this->VisibleHead ? this->HeadHeight : 0;

	RECT clientRect{};
	clientRect.left = (LONG)std::floor((contentRect.left - padding) * dpiScale);
	clientRect.top = (LONG)std::floor((contentRect.top - padding) * dpiScale) + contentTop;
	clientRect.right = (LONG)std::ceil((contentRect.right + padding) * dpiScale);
	clientRect.bottom = (LONG)std::ceil((contentRect.bottom + padding) * dpiScale) + contentTop;
	return clientRect;
}

bool Form::RectIntersects(const RECT& a, const RECT& b)
{
	RECT out{};
	return ::IntersectRect(&out, &a, &b) != 0;
}

RECT Form::ToRECT(D2D1_RECT_F rect, int inflatePx)
{
	RECT result{};
	result.left = (LONG)std::floor(rect.left) - inflatePx;
	result.top = (LONG)std::floor(rect.top) - inflatePx;
	result.right = (LONG)std::ceil(rect.right) + inflatePx;
	result.bottom = (LONG)std::ceil(rect.bottom) + inflatePx;
	return result;
}

void Form::InvalidateControl(Control* control, float inflateDip, bool immediate)
{
	if (!control || !this->Handle) return;
	if (!control->IsVisual) return;
	RECT physicalRect = ContentDipRectToClientPixels(control->AbsRect, inflateDip);
	Invalidate(physicalRect, immediate);
}

void Form::RefreshAnimationTimer()
{
	if (!this->Handle) return;

	bool hasActiveAnimation = false;
	UINT desiredIntervalMs = 0;

	std::function<void(Control*)> consider;
	consider = [&](Control* control)
		{
			if (!control || !control->Visible || !control->IsVisual) return;
			if (control->IsAnimationRunning())
			{
				hasActiveAnimation = true;
				UINT interval = control->GetAnimationIntervalMs();
				if (interval == 0) interval = 16;
				desiredIntervalMs = desiredIntervalMs == 0 ? interval : (std::min)(desiredIntervalMs, interval);
			}
			for (int i = 0; i < control->Count; i++)
				consider(control->operator[](i));
		};

	for (auto control : this->Controls) consider(control);
	if (this->ForegroundControl) consider(this->ForegroundControl);
	if (this->MainMenu) consider(this->MainMenu);
	if (this->MainStatusBar) consider(this->MainStatusBar);

	if (!hasActiveAnimation)
	{
		if (_animIntervalMs != 0)
		{
			::KillTimer(this->Handle, _animTimerId);
			_animIntervalMs = 0;
		}
		return;
	}

	if (_animIntervalMs != desiredIntervalMs)
	{
		if (_animIntervalMs != 0)
			::KillTimer(this->Handle, _animTimerId);
		_animIntervalMs = desiredIntervalMs;
		::SetTimer(this->Handle, _animTimerId, _animIntervalMs, nullptr);
	}
}

void Form::InvalidateAnimatedControls(bool immediate)
{
	std::function<void(Control*)> consider;
	consider = [&](Control* control)
		{
			if (!control) return;
			if (!control->Visible || !control->IsVisual) return;
			if (control->IsAnimationRunning())
			{
				D2D1_RECT_F rect{};
				if (control->GetAnimatedInvalidRect(rect))
				{
					RECT physicalRect = ContentDipRectToClientPixels(rect, 2.0f);
					Invalidate(physicalRect, false);
				}
				else
				{
					InvalidateControl(control, 2, false);
				}
			}
			for (int i = 0; i < control->Count; i++)
				consider(control->operator[](i));
		};
	for (auto control : this->Controls) consider(control);
	// 单一置顶控件 / 主菜单（有可能不在 Controls 容器里，保险起见单独考虑）
	if (this->ForegroundControl) consider(this->ForegroundControl);
	if (this->MainMenu) consider(this->MainMenu);
	if (this->MainStatusBar) consider(this->MainStatusBar);
	RefreshAnimationTimer();
	if (immediate)
		::UpdateWindow(this->Handle);
}
GET_CPP(Form, POINT, Location)
{
	if (this->Handle)
	{
		RECT rect;
		GetWindowRect(this->Handle, &rect);
		POINT point = { rect.left,rect.top };
		return point;
	}
	else
	{
		return this->_initialLocation;
	}
}
SET_CPP(Form, POINT, Location)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, nullptr, value.x, value.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	this->_initialLocation = value;
}
GET_CPP(Form, SIZE, Size)
{
	if (this->Handle)
	{
		RECT rect;
		GetClientRect(this->Handle, &rect);
		SIZE size = { rect.right - rect.left,rect.bottom - rect.top };
		return size;
	}
	else
	{
		return this->_initialSize;
	}
}
SET_CPP(Form, SIZE, Size)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, nullptr, 0, 0, value.cx, value.cy, SWP_NOMOVE | SWP_NOZORDER);
	}
	this->_initialSize = value;
	this->ControlChanged = true;
	InvalidateLayout();
}

GET_CPP(Form, SIZE, ClientSize)
{
	auto clientSize = this->Size;
	clientSize.cy -= this->HeadHeight;
	return clientSize;
}
GET_CPP(Form, std::wstring, Text) {
	return _text;
}
SET_CPP(Form, std::wstring, Text) {
	_text = value;
	this->ControlChanged = true;
}

GET_CPP(Form, std::shared_ptr<BitmapSource>, Image)
{
	return _imageSource;
}

SET_CPP(Form, std::shared_ptr<BitmapSource>, Image)
{
	if (value == _imageSource)
		return;
	_imageSource = std::move(value);
	ResetImageCache();
	this->ControlChanged = true;
}

class Font* Form::GetFont()
{
	return GetScaledDefaultFont();
}

void Form::SetFont(class Font* value)
{
	this->SetFontEx(value, true);
}

void Form::SetFontEx(class Font* value, bool takeOwnership)
{
	if (value == GetDefaultFontObject())
	{
		value = nullptr;
		takeOwnership = false;
	}

	if (value == this->_font)
	{
		this->_ownsFont = takeOwnership;
		return;
	}

	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = value;
	this->_ownsFont = takeOwnership;
	this->_systemScaledFont.reset();
	this->_systemScaledFontSource = nullptr;
	for (auto* control : this->Controls)
	{
		if (control)
			control->InvalidateMeasureSubtree();
	}
	this->ControlChanged = true;
	this->InvalidateLayout();
}

FormThemeFrame Form::GetThemeFrame() const
{
	FormThemeFrame theme;
	theme.WindowBackColor = this->BackColor;
	theme.WindowForeColor = this->ForeColor;
	theme.WindowBorderLightColor = this->BorderLightColor;
	theme.WindowBorderDarkColor = this->BorderDarkColor;
	theme.TitleBarBackColor = this->HeadBackColor;
	theme.CaptionHoverColor = this->CaptionHoverColor;
	theme.CaptionPressedColor = this->CaptionPressedColor;
	theme.CloseHoverColor = this->CloseHoverColor;
	theme.ClosePressedColor = this->ClosePressedColor;
	theme.ValidationErrorColor = this->ValidationErrorColor;
	theme.ValidationWarningColor = this->ValidationWarningColor;
	theme.ValidationInfoColor = this->ValidationInfoColor;
	theme.ValidationToolTipBackColor = this->ValidationToolTipBackColor;
	theme.ValidationToolTipTextColor = this->ValidationToolTipTextColor;
	return theme;
}

FormThemeFrame Form::GetEffectiveThemeFrame() const
{
	auto theme = GetThemeFrame();
	if (!_systemVisualPreferences.HighContrast)
		return theme;
	theme.WindowBackColor = FromSystemColor(COLOR_WINDOW);
	theme.WindowForeColor = FromSystemColor(COLOR_WINDOWTEXT);
	theme.WindowBorderLightColor = FromSystemColor(COLOR_WINDOWTEXT);
	theme.WindowBorderDarkColor = FromSystemColor(COLOR_WINDOWTEXT);
	theme.TitleBarBackColor = FromSystemColor(COLOR_HIGHLIGHT);
	theme.CaptionHoverColor = FromSystemColor(COLOR_HIGHLIGHT);
	theme.CaptionPressedColor = FromSystemColor(COLOR_HOTLIGHT);
	theme.CloseHoverColor = FromSystemColor(COLOR_HIGHLIGHT);
	theme.ClosePressedColor = FromSystemColor(COLOR_HOTLIGHT);
	theme.ValidationErrorColor = FromSystemColor(COLOR_HOTLIGHT);
	theme.ValidationWarningColor = FromSystemColor(COLOR_HIGHLIGHT);
	theme.ValidationInfoColor = FromSystemColor(COLOR_HIGHLIGHT);
	theme.ValidationToolTipBackColor = FromSystemColor(COLOR_INFOBK);
	theme.ValidationToolTipTextColor = FromSystemColor(COLOR_INFOTEXT);
	return theme;
}

D2D1_COLOR_F Form::GetEffectiveControlBackColor(
	D2D1_COLOR_F configured) const
{
	return _systemVisualPreferences.HighContrast
		? FromSystemColor(COLOR_WINDOW) : configured;
}

D2D1_COLOR_F Form::GetEffectiveControlForeColor(
	D2D1_COLOR_F configured) const
{
	return _systemVisualPreferences.HighContrast
		? FromSystemColor(COLOR_WINDOWTEXT) : configured;
}

D2D1_COLOR_F Form::GetEffectiveFocusColor(
	D2D1_COLOR_F configured) const
{
	return _systemVisualPreferences.HighContrast
		? FromSystemColor(COLOR_HIGHLIGHT) : configured;
}

void Form::ApplySystemVisualPreferences(SystemVisualPreferences preferences)
{
	preferences = Application::NormalizeSystemVisualPreferences(preferences);
	const bool changed =
		preferences.HighContrast != _systemVisualPreferences.HighContrast
		|| preferences.AnimationsEnabled != _systemVisualPreferences.AnimationsEnabled
		|| preferences.KeyboardCuesAlwaysVisible
			!= _systemVisualPreferences.KeyboardCuesAlwaysVisible
		|| preferences.TextScalePercent != _systemVisualPreferences.TextScalePercent;
	if (!changed) return;
	const bool textScaleChanged = preferences.TextScalePercent
		!= _systemVisualPreferences.TextScalePercent;
	_systemVisualPreferences = preferences;
	if (_systemVisualPreferences.KeyboardCuesAlwaysVisible)
		_keyboardFocusVisualRequested = true;
	_systemScaledFont.reset();
	_systemScaledFontSource = nullptr;
	if (textScaleChanged)
	{
		for (auto* control : Controls)
			if (control) control->InvalidateMeasureSubtree();
		InvalidateLayout();
	}
	if (Selected) Selected->InvalidateVisual();
	ControlChanged = true;
	Invalidate(false);
	RefreshAnimationTimer();
}

void Form::RefreshSystemVisualPreferences()
{
	ApplySystemVisualPreferences(Application::QuerySystemVisualPreferences());
}

void Form::ApplyThemeFrame(const FormThemeFrame& theme, const std::wstring& themeName)
{
	std::wstring oldTheme = this->_themeName;
	if (!themeName.empty())
	{
		this->_themeName = themeName;
	}

	this->BackColor = theme.WindowBackColor;
	this->ForeColor = theme.WindowForeColor;
	this->BorderLightColor = theme.WindowBorderLightColor;
	this->BorderDarkColor = theme.WindowBorderDarkColor;
	this->HeadBackColor = theme.TitleBarBackColor;
	this->CaptionHoverColor = theme.CaptionHoverColor;
	this->CaptionPressedColor = theme.CaptionPressedColor;
	this->CloseHoverColor = theme.CloseHoverColor;
	this->ClosePressedColor = theme.ClosePressedColor;
	this->ValidationErrorColor = theme.ValidationErrorColor;
	this->ValidationWarningColor = theme.ValidationWarningColor;
	this->ValidationInfoColor = theme.ValidationInfoColor;
	this->ValidationToolTipBackColor = theme.ValidationToolTipBackColor;
	this->ValidationToolTipTextColor = theme.ValidationToolTipTextColor;
	this->ControlChanged = true;

	if (oldTheme != this->_themeName)
	{
		this->OnThemeChanged(this, oldTheme, this->_themeName);
	}

	this->Invalidate(true);
}

D2D1_COLOR_F Form::GetValidationColor(
	BindingValidationSeverity severity) const noexcept
{
	const auto theme = GetEffectiveThemeFrame();
	switch (severity)
	{
	case BindingValidationSeverity::Info: return theme.ValidationInfoColor;
	case BindingValidationSeverity::Warning: return theme.ValidationWarningColor;
	case BindingValidationSeverity::Error:
	default: return theme.ValidationErrorColor;
	}
}

void Form::RenderValidationToolTip()
{
	auto* target = UnderMouse;
	if (!target || target->ParentForm != this || !target->Visible
		|| !target->ShouldShowValidationToolTip() || !Render)
		return;

	const auto summary = target->GetValidationSummary(3);
	if (summary.empty()) return;
	BindingValidationSeverity severity;
	if (!target->TryGetValidationSeverity(severity)) return;

	const float dpiScale = GetDpiScale();
	const float contentWidth = Size.cx / dpiScale;
	const float contentHeight = Size.cy / dpiScale - ClientTop() / dpiScale;
	if (!(contentWidth > 20.0f) || !(contentHeight > 20.0f)) return;

	auto* font = target->Font;
	if (!font) font = GetDefaultFontObject();
	constexpr float margin = 8.0f;
	constexpr float paddingX = 10.0f;
	constexpr float paddingY = 7.0f;
	const float availableWidth = (std::max)(40.0f, contentWidth - margin * 2.0f);
	const float popupMaxWidth = (std::min)(
		availableWidth, target->ValidationToolTipMaxWidth);
	const float textMaxWidth = (std::max)(20.0f, popupMaxWidth - paddingX * 2.0f);
	const float textMaxHeight = (std::max)(20.0f, contentHeight - margin * 2.0f - paddingY * 2.0f);
	const auto textSize = font->GetTextSize(summary, textMaxWidth, textMaxHeight);
	const float popupWidth = (std::min)(availableWidth,
		(std::max)(40.0f, textSize.width + paddingX * 2.0f));
	const float popupHeight = (std::min)(contentHeight - margin * 2.0f,
		(std::max)(font->FontHeight + paddingY * 2.0f,
			textSize.height + paddingY * 2.0f));
	if (!(popupWidth > 0.0f) || !(popupHeight > 0.0f)) return;

	const auto location = target->GetAbsoluteLocationDip();
	const auto targetSize = target->GetActualSizeDip();
	float x = location.x;
	float y = location.y + targetSize.height + 6.0f;
	if (x + popupWidth > contentWidth - margin)
		x = contentWidth - margin - popupWidth;
	if (y + popupHeight > contentHeight - margin)
		y = location.y - popupHeight - 6.0f;
	x = (std::clamp)(x, margin, (std::max)(margin, contentWidth - margin - popupWidth));
	y = (std::clamp)(y, margin, (std::max)(margin, contentHeight - margin - popupHeight));

	const auto accent = GetValidationColor(severity);
	const auto effectiveTheme = GetEffectiveThemeFrame();
	Render->FillRoundRect(x + 1.0f, y + 3.0f, popupWidth, popupHeight,
		D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.22f }, 7.0f);
	Render->FillRoundRect(x, y, popupWidth, popupHeight,
		effectiveTheme.ValidationToolTipBackColor, 7.0f);
	Render->DrawRoundRect(x, y, popupWidth, popupHeight, accent, 1.5f, 7.0f);
	Render->FillRoundRect(x + 3.0f, y + 4.0f, 3.0f,
		(std::max)(0.0f, popupHeight - 8.0f), accent, 1.5f);
	Render->DrawString(summary,
		x + paddingX, y + paddingY,
		(std::max)(0.0f, popupWidth - paddingX * 2.0f),
		(std::max)(0.0f, popupHeight - paddingY * 2.0f),
		effectiveTheme.ValidationToolTipTextColor, font);
}

GET_CPP(Form, bool, TopMost)
{
	return (GetWindowLong(this->Handle, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}
SET_CPP(Form, bool, TopMost)
{
	if (value)
	{
		SetWindowPos(this->Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos(this->Handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
}
GET_CPP(Form, bool, Enable)
{
	return IsWindowEnabled(this->Handle);
}
SET_CPP(Form, bool, Enable)
{
	EnableWindow(this->Handle, value);
}
GET_CPP(Form, bool, Visible)
{
	return IsWindowVisible(this->Handle);
}
SET_CPP(Form, bool, Visible)
{
	ShowWindow(this->Handle, value ? SW_SHOW : SW_HIDE);
	if (value) RaiseShownOnce();
}

GET_CPP(Form, bool, AllowResize)
{
	return this->_allowResize;
}

SET_CPP(Form, bool, AllowResize)
{
	if (this->_allowResize == value)
		return;

	this->_allowResize = value;
	if (!value)
	{
		this->_maxBoxBeforeAllowResize = this->MaxBox;
		this->MaxBox = false;

		if (this->Handle && IsZoomed(this->Handle))
			ShowWindow(this->Handle, SW_RESTORE);
	}
	else
	{
		this->MaxBox = this->_maxBoxBeforeAllowResize;
	}

	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);

	ClearCaptionStates();
	Invalidate(TitleBarRectClient(), true);
}

GET_CPP(Form, bool, ShowInTaskBar)
{
	return this->_showInTaskBar;
}
SET_CPP(Form, bool, ShowInTaskBar)
{
	this->_showInTaskBar = value;
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
}

Form::Form(std::wstring text, POINT _location, SIZE _size)
{
	Application::EnsureDpiAwareness();
	this->_systemVisualPreferences =
		Application::QuerySystemVisualPreferences();
	this->_keyboardFocusVisualRequested =
		this->_systemVisualPreferences.KeyboardCuesAlwaysVisible;

	this->_text = text;
	this->_autoCenterOnCreate = (_location.x == 0 && _location.y == 0);
	static bool ClassInited = false;
	this->Location = _location;
	this->Size = _size;
	this->_headHeightBase96 = this->HeadHeight;
	WNDCLASSW wndclass = { 0 };
	if (!ClassInited)
	{
		wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.lpfnWndProc = WINMSG_PROCESS;
		wndclass.hInstance = GetModuleHandleA(nullptr);
		wndclass.hIcon = LoadProcessIcon(false);
		wndclass.hCursor = LoadCursorW(GetModuleHandle(nullptr), IDC_ARROW);
		wndclass.lpszMenuName = nullptr;
		wndclass.lpszClassName = L"CoreNativeWindow";
		if (!RegisterClassW(&wndclass))
		{
			return;
		}
		ClassInited = true;
	}
	RECT workArea = GetWindowWorkArea(nullptr, _location);
	POINT initialOrigin = _location;
	if (this->_autoCenterOnCreate)
	{
		initialOrigin.x = workArea.left + ((workArea.right - workArea.left) - this->Size.cx) / 2;
		initialOrigin.y = workArea.top + ((workArea.bottom - workArea.top) - this->Size.cy) / 2;
	}
	initialOrigin = ClampWindowOriginToWorkArea(initialOrigin, this->Size, workArea);
	this->Handle = CreateWindowExW(
		0L,
		L"CoreNativeWindow",
		_text.c_str(),
		WS_POPUP,
		initialOrigin.x,
		initialOrigin.y,
		this->Size.cx,
		this->Size.cy,
		nullptr,
		nullptr,
		GetModuleHandleW(0),
		0);
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	SetWindowLongPtrW(this->Handle, GWLP_USERDATA, (LONG_PTR)this ^ 0xFFFFFFFFFFFFFFFF);

	DragAcceptFiles(this->Handle, TRUE);
	EnsureDropTargetRegistered();


	Application::Forms[this->Handle] = this;

	Render = new HwndGraphics(this->Handle);
	OverlayRender = nullptr;
	ResetImageCache();
	ClearCaptionStates();
	ApplyWindowIcon();
}

Form::~Form()
{
	CleanupResources();
}

void Form::CleanupResources()
{
	if (_resourcesCleaned)
		return;
	_resourcesCleaned = true;
	if (_uiaProvider)
	{
		_uiaProvider->DetachForm();
		_uiaProvider->Release();
		_uiaProvider = nullptr;
	}
	if (_accessibleObject)
	{
		_accessibleObject->DetachForm();
		_accessibleObject->Release();
		_accessibleObject = nullptr;
	}
	if (this->Handle && _dropRegistered)
	{
		RevokeDragDrop(this->Handle);
		_dropRegistered = false;
	}
	if (_dropTarget)
	{
		_dropTarget->Release();
		_dropTarget = nullptr;
	}

	auto isDescendant = [&](Control* root, Control* node, const auto& self) -> bool
		{
			if (!root || !node) return false;
			for (int i = 0; i < root->Count; i++)
			{
				auto child = root->operator[](i);
				if (child == node) return true;
				if (self(child, node, self)) return true;
			}
			return false;
		};

	auto isOwnedByRootControls = [&](Control* node) -> bool
		{
		SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
			if (!node) return false;
			for (auto c : this->Controls)
			{
				if (c == node) return true;
				if (isDescendant(c, node, isDescendant)) return true;
			}
			for (Control* parent = node->Parent; parent; parent = parent->Parent)
			{
				for (auto c : this->Controls)
				{
					if (c == parent) return true;
					if (isDescendant(c, parent, isDescendant)) return true;
				}
			}
			return false;
		};

	if (this->ForegroundControl && !isOwnedByRootControls(this->ForegroundControl))
	{
		delete this->ForegroundControl;
	}
	this->ForegroundControl = nullptr;
	// Layout engines may keep non-owning per-child metadata. Destroy the engine
	// while the control tree is still alive.
	_layoutEngine.reset();

	for (auto c : this->Controls)
	{
		delete c;
	}
	this->Controls.clear();

	this->Selected = nullptr;
	this->UnderMouse = nullptr;
	this->MainMenu = nullptr;
	this->MainToolBar = nullptr;
	this->MainStatusBar = nullptr;
	this->_defaultButton = nullptr;
	this->_cancelButton = nullptr;

	this->_imageSource.reset();
	ResetImageCache();

	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = nullptr;
	this->_ownsFont = false;

	if (OverlayRender)
	{
		delete OverlayRender;
		OverlayRender = nullptr;
	}
	ReleaseDCompD2DLayers();
	if (Render)
	{
		delete Render;
		Render = nullptr;
	}
	if (_dcompHost)
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
}

bool Form::EnsureDCompInitialized()
{
#ifdef CUI_ENABLE_WEBVIEW2
		if (_dcompHost) return _dcompHost->IsInitialized();
		if (!this->Handle || !::IsWindow(this->Handle)) return false;

	RECT rc{};
	::GetClientRect(this->Handle, &rc);
	UINT w = (UINT)std::max<LONG>(1, rc.right - rc.left);
	UINT h = (UINT)std::max<LONG>(1, rc.bottom - rc.top);

	_dcompHost = new DCompLayeredHost();
	if (_dcompHost->Initialize(this->Handle, w, h))
	{
		auto* swapChain = static_cast<IDXGISwapChain1*>(_dcompHost->GetSwapChain());
		if (swapChain)
		{
			if (Render)
			{
				delete Render;
				Render = nullptr;
			}
			Render = new CompositionSwapChainGraphics(swapChain);
			Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
			Render->ReSize(w, h);
			auto* overlaySwapChain = static_cast<IDXGISwapChain1*>(_dcompHost->GetOverlaySwapChain());
			if (overlaySwapChain)
			{
				if (OverlayRender)
				{
					delete OverlayRender;
					OverlayRender = nullptr;
				}
				OverlayRender = new CompositionSwapChainGraphics(overlaySwapChain);
				OverlayRender->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
				OverlayRender->ReSize(w, h);
			}
		}
	}
	else
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
		return _dcompHost && _dcompHost->IsInitialized();
#else
		return false;
#endif
}

D2DGraphics* Form::GetDCompD2DLayerRender(size_t index, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!_dcompHost)
		return nullptr;
	while (_dcompD2DLayers.size() <= index)
	{
		void* swapChainPtr = nullptr;
		IDCompositionVisual* visual = nullptr;
		if (!_dcompHost->CreateD2DLayer(&swapChainPtr, &visual, layer, order))
			return nullptr;
		auto* swapChain = static_cast<IDXGISwapChain1*>(swapChainPtr);
		auto* graphics = new CompositionSwapChainGraphics(swapChain);
		if (swapChain)
			swapChain->Release();
		graphics->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
		_dcompD2DLayers.push_back({ visual, graphics });
	}

	auto& item = _dcompD2DLayers[index];
	if (item.Visual)
		_dcompHost->UpdateVisualOrder(item.Visual, layer, order);
	if (item.Render)
		item.Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	return item.Render;
#else
	(void)index;
	(void)layer;
	(void)order;
	return nullptr;
#endif
}

void Form::ReleaseDCompD2DLayers()
{
#ifdef CUI_ENABLE_WEBVIEW2
	for (auto& item : _dcompD2DLayers)
	{
		if (item.Render)
		{
			delete item.Render;
			item.Render = nullptr;
		}
		if (item.Visual)
		{
			if (_dcompHost)
				_dcompHost->DestroyD2DLayer(item.Visual);
			item.Visual->Release();
			item.Visual = nullptr;
		}
	}
	_dcompD2DLayers.clear();
#endif
}

void Form::ClearUnusedDCompD2DLayers(size_t usedCount, float logW, float logH)
{
#ifdef CUI_ENABLE_WEBVIEW2
	for (size_t i = usedCount; i < _dcompD2DLayers.size(); i++)
	{
		auto* layerRender = _dcompD2DLayers[i].Render;
		if (!layerRender) continue;
		layerRender->BeginRender();
		layerRender->ClearTransform();
		layerRender->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		layerRender->PushDrawRect(0.0f, 0.0f, logW, logH);
		layerRender->PopDrawRect();
		layerRender->EndRender();
	}
#else
	(void)usedCount;
	(void)logW;
	(void)logH;
#endif
}

void Form::RenderDCompRootLayers(const RECT& contentDirty, int titleBarOffset, float dpiScale)
{
#ifdef CUI_ENABLE_WEBVIEW2
	(void)titleBarOffset;
	if (!_dcompHost || contentDirty.right <= contentDirty.left || contentDirty.bottom <= contentDirty.top)
		return;

	RECT fullClient{};
	::GetClientRect(this->Handle, &fullClient);
	const float logW = (fullClient.right - fullClient.left) / dpiScale;
	const float logH = (fullClient.bottom - fullClient.top) / dpiScale;
	const auto roots = GetRootControlsInZOrder(this);
	DCompSceneBuildState state{};
	state.ContentDirty = contentDirty;
	state.LogW = logW;
	state.LogH = logH;
	state.OldRender = this->Render;
	_dcompSceneOrderCounter = 0;
	_dcompSceneRenderActive = true;

	for (auto control : roots)
	{
		if (ShouldSkipRootDCompSceneControl(control))
			continue;
		RenderDCompControlTree(control, state);
	}

	EndDCompD2DSegment(state);
	_dcompSceneRenderActive = false;
	ClearUnusedDCompD2DLayers(state.LayerIndex, logW, logH);
#else
	(void)contentDirty;
	(void)titleBarOffset;
	(void)dpiScale;
#endif
}

int Form::NextDCompSceneOrder()
{
	return _dcompSceneOrderCounter++;
}

bool Form::ShouldSkipRootDCompSceneControl(Control* control) const
{
	if (!control || !control->Visible)
		return true;
	if (control == this->ForegroundControl && !control->RenderNormalWhenForeground())
		return true;
	if (control == this->MainMenu)
		return true;
	if (this->MainStatusBar && this->MainStatusBar->TopMost && control == this->MainStatusBar)
		return true;
	return false;
}

bool Form::IsNativeDCompControl(Control* control) const
{
	return control && control->Type() == UIClass::UI_WebBrowser;
}

bool Form::GetDCompSceneClientClip(Control* control, const RECT& contentDirty, RECT& outClip)
{
	if (!control)
		return false;
	const int top = (int)(ClientTop() / GetDpiScale());
	outClip = contentDirty;
	outClip.top += top;
	outClip.bottom += top;

	Control* current = control->Parent;
	while (current)
	{
		if (current->ClipsChildren())
		{
			auto clip = current->GetChildrenClipRect();
			const auto parentAbs = current->GetAbsoluteLocationDip();
			RECT clipRect{
				(LONG)std::floor(clip.left + parentAbs.x),
				(LONG)std::floor(clip.top + parentAbs.y + top),
				(LONG)std::ceil(clip.right + parentAbs.x),
				(LONG)std::ceil(clip.bottom + parentAbs.y + top)
			};
			RECT intersection{};
			if (!::IntersectRect(&intersection, &outClip, &clipRect))
				return false;
			outClip = intersection;
		}
		current = current->Parent;
	}
	return outClip.right > outClip.left && outClip.bottom > outClip.top;
}

std::vector<Control*> Form::GetDCompSceneChildren(Control* control)
{
	std::vector<Control*> children;
	if (!control)
		return children;

	if (control->Type() == UIClass::UI_TabControl)
	{
		auto* tab = static_cast<TabControl*>(control);
		for (auto child : tab->GetVisibleScenePages())
		{
			if (child && child->Visible)
				children.push_back(child);
		}
		return children;
	}

	children = control->GetChildrenInZOrder();
	children.erase(std::remove_if(children.begin(), children.end(), [](Control* child)
		{
			return !child || !child->Visible;
		}), children.end());
	return children;
}

void Form::BeginDCompD2DSegment(DCompSceneBuildState& state, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (state.SegmentOpen)
		return;
	auto* layerRender = GetDCompD2DLayerRender(state.LayerIndex++, DCompSceneLayerBand, order);
	if (!layerRender)
		return;
	state.SegmentRender = layerRender;
	state.SegmentOrder = order;
	state.SegmentOpen = true;
	layerRender->BeginRender();
	layerRender->ClearTransform();
	layerRender->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
	layerRender->PushDrawRect(0.0f, 0.0f, state.LogW, state.LogH);
	this->Render = layerRender;
#else
	(void)state;
	(void)order;
#endif
}

void Form::EndDCompD2DSegment(DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!state.SegmentOpen || !state.SegmentRender)
		return;
	state.SegmentRender->ClearTransform();
	state.SegmentRender->PopDrawRect();
	state.SegmentRender->EndRender();
	this->Render = state.OldRender;
	state.SegmentRender = nullptr;
	state.SegmentOpen = false;
#else
	(void)state;
#endif
}

void Form::RenderDCompD2DControlInSegment(Control* control, DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!control || !state.SegmentOpen || !state.SegmentRender)
		return;
	RECT crc = ToRECT(control->AbsRect, 2);
	const int top = (int)(ClientTop() / GetDpiScale());
	RECT clientControlRc = crc;
	clientControlRc.top += top;
	clientControlRc.bottom += top;
	RECT clientClip{};
	if (!GetDCompSceneClientClip(control, state.ContentDirty, clientClip) || !RectIntersects(clientClip, clientControlRc))
		return;

	state.SegmentRender->PushDrawRect((float)clientClip.left, (float)clientClip.top, (float)(clientClip.right - clientClip.left), (float)(clientClip.bottom - clientClip.top));
	control->Update();
	state.SegmentRender->PopDrawRect();
#else
	(void)control;
	(void)state;
#endif
}

void Form::RenderDCompControlTree(Control* control, DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!control || !control->Visible)
		return;

	const int order = NextDCompSceneOrder();
	if (IsNativeDCompControl(control))
	{
		EndDCompD2DSegment(state);
		control->SetDCompSceneOrderOverride(order);
		control->Update();
		control->ClearDCompSceneOrderOverride();
	}
	else
	{
		if (!state.SegmentOpen)
			BeginDCompD2DSegment(state, order);
		RenderDCompD2DControlInSegment(control, state);
	}

	for (auto child : GetDCompSceneChildren(control))
	{
		RenderDCompControlTree(child, state);
	}
#else
	(void)control;
	(void)state;
#endif
}

IDCompositionDevice* Form::GetDCompDevice() const
{
#ifdef CUI_ENABLE_WEBVIEW2
		auto* self = const_cast<Form*>(this);
		if (!self->_dcompHost && !self->EnsureDCompInitialized()) return nullptr;
		return self->_dcompHost ? self->_dcompHost->GetDCompDevice() : nullptr;
#else
	return nullptr;
#endif
}

IDCompositionVisual* Form::GetWebContainerVisual() const
{
#ifdef CUI_ENABLE_WEBVIEW2
		auto* self = const_cast<Form*>(this);
		if (!self->_dcompHost && !self->EnsureDCompInitialized()) return nullptr;
		return self->_dcompHost ? self->_dcompHost->GetWebContainerVisual() : nullptr;
#else
	return nullptr;
#endif
}

bool Form::RegisterDCompVisual(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
		if (!_dcompHost && !EnsureDCompInitialized()) return false;
	return _dcompHost ? _dcompHost->RegisterVisual(visual, layer, order) : false;
#else
	(void)visual;
	(void)layer;
	(void)order;
	return false;
#endif
}

void Form::UpdateDCompVisualOrder(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->UpdateVisualOrder(visual, layer, order);
#else
	(void)visual;
	(void)layer;
	(void)order;
#endif
}

void Form::UnregisterDCompVisual(IDCompositionVisual* visual)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->UnregisterVisual(visual);
#else
	(void)visual;
#endif
}

int Form::GetDCompVisualOrder(Control* control) const
{
	if (!control)
		return 0;

	int order = 0;
	int result = -1;
	std::function<bool(Control*)> visit = [&](Control* current) -> bool
		{
			if (!current)
				return false;
			if (current == control)
			{
				result = order++;
				return true;
			}
			order++;
			for (auto child : current->GetChildrenInZOrder())
			{
				if (visit(child))
					return true;
			}
			return false;
		};

	auto roots = GetRootControlsInZOrder(const_cast<Form*>(this));
	for (auto root : roots)
	{
		if (visit(root))
			return result;
	}
	return order;
}

void Form::CommitComposition()
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->CommitComposition();
#endif
}

Font* Form::GetScaledDefaultFont()
{
	Font* source = _font ? _font : GetDefaultFontObject();
	const float factor = GetTextScaleFactor();
	if (!(factor > 1.0001f)) return source;
	const float sourceSize = source->FontSize;
	if (!_systemScaledFont || _systemScaledFontSource != source
		|| std::fabs(_systemScaledFontSourceSize - sourceSize) > 0.001f
		|| std::fabs(_systemScaledFontFactor - factor) > 0.001f)
	{
		_systemScaledFont = std::make_unique<::Font>(
			source->FontName, sourceSize * factor);
		_systemScaledFontSource = source;
		_systemScaledFontSourceSize = sourceSize;
		_systemScaledFontFactor = factor;
	}
	return _systemScaledFont.get();
}

void Form::ApplyDpiChange(UINT newDpi)
{
	if (newDpi == 0) newDpi = 96;
	if (this->_dpi == newDpi) return;
	this->_dpi = newDpi;

	// 标题栏高度保持物理像素（OS 命中测试使用），从 96-DPI 基准重新计算避免累积误差
	this->HeadHeight = Application::ScaleInt(this->_headHeightBase96, 96, newDpi);

	// 通过 D2D SetDpi 让渲染引擎在逻辑坐标系（96-DPI 设计值）中工作，
	// 无需再对控件树的位置/大小/字体进行缩放——D2D 内部映射到正确的物理像素。
	if (this->Render)        this->Render->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);
	if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);

	this->InvalidateLayout();
	this->_hasRenderedOnce = false;
	this->Invalidate(false);
}

void Form::SyncRenderSizeToClient()
{
	if (!this->Handle || !this->Render) return;
	RECT rc{};
	::GetClientRect(this->Handle, &rc);
	UINT width = (UINT)std::max<LONG>(1, rc.right - rc.left);
	UINT height = (UINT)std::max<LONG>(1, rc.bottom - rc.top);
	if (this->_dcompHost) this->_dcompHost->UpdateD2DLayerSize(width, height);
	this->Render->ReSize(width, height);
	if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->ReSize(width, height);
	if (this->Render)        this->Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
}

void Form::EnsureInitialDpiApplied()
{
	if (_initialDpiApplied) return;
	_initialDpiApplied = true;
	if (!this->Handle) return;

	Application::EnsureDpiAwareness();
	UINT dpi = Application::GetDpiForWindow(this->Handle);
	if (dpi == 0) dpi = 96;
	if (this->_headHeightBase96 <= 0) this->_headHeightBase96 = 24;

	// 窗口物理尺寸：按 96→dpi 缩放，使窗口在屏幕上占据与设计値相同的视角
	// 控件树保持 96-DPI 逻辑坐标，D2D 通过 SetDpi 完成物理像素映射，无需缩放控件树
	if (!this->_initialWindowRectApplied)
	{
		RECT wr{};
		GetWindowRect(this->Handle, &wr);
		const int newW = Application::ScaleInt(this->_initialSize.cx, 96, dpi);
		const int newH = Application::ScaleInt(this->_initialSize.cy, 96, dpi);
		RECT workArea = GetWindowWorkArea(this->Handle, POINT{ wr.left, wr.top });
		POINT origin{};
		if (this->_autoCenterOnCreate)
		{
			origin.x = workArea.left + ((workArea.right - workArea.left) - newW) / 2;
			origin.y = workArea.top + ((workArea.bottom - workArea.top) - newH) / 2;
		}
		else
		{
			origin.x = wr.left;
			origin.y = wr.top;
		}
		origin = ClampWindowOriginToWorkArea(origin, SIZE{ newW, newH }, workArea);
		SetWindowPos(this->Handle, nullptr, origin.x, origin.y, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
		SyncRenderSizeToClient();
		this->_hasRenderedOnce = false;
		this->Invalidate(false);
	}

	// 更新 HeadHeight（物理像素）并为渲染目标设置 DPI
	ApplyDpiChange(dpi);
}

void Form::EnsureOleInitialized()
{
	static bool inited = false;
	if (inited) return;
	inited = true;
	OleInitialize(nullptr);
}

void Form::EnsureDropTargetRegistered()
{
	if (!this->Handle) return;
	if (_dropRegistered) return;
	EnsureOleInitialized();
	if (!_dropTarget)
	{
		_dropTarget = new FormDropTarget(this);
	}
	HRESULT hr = RegisterDragDrop(this->Handle, _dropTarget);
	if (SUCCEEDED(hr))
	{
		_dropRegistered = true;
		DragAcceptFiles(this->Handle, FALSE);
	}
}

ID2D1Bitmap* Form::EnsureImageCache()
{
	if (!_imageSource || !this->Render)
		return nullptr;
	auto* target = this->Render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (_imageCache && _imageCacheTarget == target)
		return _imageCache.Get();
	_imageCache.Reset();
	_imageCacheTarget = target;
	auto* bmp = this->Render->CreateBitmap(_imageSource);
	if (!bmp)
		return nullptr;
	_imageCache.Attach(bmp);
	return _imageCache.Get();
}

void Form::ResetImageCache()
{
	_imageCache.Reset();
	_imageCacheTarget = nullptr;
}

void Form::RecoverRenderIfNeeded()
{
	if (_recoveringDeviceLost)
		return;
	_recoveringDeviceLost = true;

	// 只有在句柄存在时才尝试恢复
	if (!this->Handle || !::IsWindow(this->Handle))
	{
		_recoveringDeviceLost = false;
		return;
	}

	bool need = false;
	if (this->Render && this->Render->IsDeviceLost()) need = true;
	if (this->OverlayRender && this->OverlayRender->IsDeviceLost()) need = true;
	for (const auto& layer : this->_dcompD2DLayers)
	{
		if (layer.Render && layer.Render->IsDeviceLost()) need = true;
	}
	if (!need)
	{
		_recoveringDeviceLost = false;
		return;
	}

	// 先释放旧渲染对象
	if (this->OverlayRender)
	{
		delete this->OverlayRender;
		this->OverlayRender = nullptr;
	}
	ReleaseDCompD2DLayers();
	if (this->Render)
	{
		delete this->Render;
		this->Render = nullptr;
	}

		bool hadDCompHost = _dcompHost != nullptr;
		if (_dcompHost)
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
		if (hadDCompHost)
		{
				EnsureDCompInitialized();
		}
	if (!Render)
	{
		Render = new HwndGraphics(this->Handle);
		OverlayRender = nullptr;
	}

	SyncRenderSizeToClient();
	this->_hasRenderedOnce = false;
	this->Invalidate(false);
	_recoveringDeviceLost = false;
}

void Form::SetLayoutEngine(class LayoutEngine* engine)
{
	if (_layoutEngine.get() == engine)
	{
		InvalidateLayout();
		return;
	}
	_layoutEngine.reset(engine);
	InvalidateLayout();
}

void Form::InvalidateLayout()
{
	_needsLayout = true;
	if (_layoutEngine)
	{
		_layoutEngine->Invalidate();
	}
	if (_layoutDeferral.IsSuspended())
	{
		_layoutDeferral.QueueLayout();
		return;
	}

	// Layout is committed immediately before painting. Marking a layout engine
	// dirty without scheduling a frame used to leave geometry stale until an
	// unrelated visual change happened.
	Invalidate(false);
}

void Form::SuspendLayout()
{
	_layoutDeferral.Suspend();
}

void Form::ResumeLayout(bool performLayout)
{
	const auto work = _layoutDeferral.Resume();
	if (!work.ready)
		return;

	if (work.layoutRequested && performLayout &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformLayout();
	}

	if (work.layoutRequested || work.fullVisual)
	{
		Invalidate(work.immediate);
		return;
	}

	if (work.visualRequested && !work.visualBounds.IsEmpty())
	{
		const auto bounds = work.visualBounds;
		const RECT rect{
			(LONG)std::floor(bounds.Left()),
			(LONG)std::floor(bounds.Top()),
			(LONG)std::ceil(bounds.Right()),
			(LONG)std::ceil(bounds.Bottom()) };
		Invalidate(rect, work.immediate);
	}
}

void Form::PerformLayout()
{
	SIZE clientSize = this->ClientSize;
	// physical -> logical: layout coordinates stay in 96-DPI DIPs.
	const float dpiScale = GetDpiScale();
	const float clientWidthDip = (float)clientSize.cx / dpiScale;
	const float clientHeightDip = (float)clientSize.cy / dpiScale;
	clientSize.cx = (LONG)clientWidthDip;
	clientSize.cy = (LONG)clientHeightDip;
	LayoutMainTopBar(this, clientSize);
	const float statusBarHeight = LayoutMainStatusBar(this, clientSize);
	const float contentHeight = (std::max)(
		0.0f, clientHeightDip - statusBarHeight);

	if (!_layoutEngine)
	{
		const float contentLeft = 0.0f;
		const float contentTop = 0.0f;
		const float contentWidth = clientWidthDip;
		const cui::core::Rect clientBounds{
			contentLeft, contentTop, contentWidth, clientHeightDip };
		const cui::core::Rect anchoredStretchBounds{
			contentLeft, contentTop, contentWidth, contentHeight };
		const cui::core::Constraints measureAvailable{ cui::core::Size{
			contentWidth, contentHeight } };
		
		for (auto* control : GetFormLayoutControls(this))
		{
			if (!control || !control->Visible) continue;
			cui::layout::compat::ArrangeLegacyCanvasChild(
				*control,
				clientBounds,
				anchoredStretchBounds,
				measureAvailable);
		}
	}
	else
	{
		SIZE contentSize{ clientSize.cx, (LONG)contentHeight };
		FormLayoutRootHost rootHost(
			this, GetFormLayoutControls(this), contentSize);
		LayoutContext context(
			&rootHost, rootHost.GetLayoutChildrenView(), this, true);
		_layoutEngine->Measure(context, cui::core::Constraints{ cui::core::Size{
			clientWidthDip, contentHeight } });
		_layoutEngine->Arrange(context, D2D1_RECT_F{
			0.0f, 0.0f,
			clientWidthDip,
			contentHeight });
	}
	
	_needsLayout = false;
}

void Form::ApplyWindowIcon()
{
	if (!this->Handle) return;
	HICON largeIcon = this->Icon ? this->Icon : LoadProcessIcon(false);
	HICON smallIcon = this->Icon ? this->Icon : LoadProcessIcon(true);
	if (largeIcon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)largeIcon);
	if (smallIcon) SendMessage(this->Handle, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
}

void Form::RaiseShownOnce()
{
	if (this->_shownRaised) return;
	this->_shownRaised = true;
	this->OnShown(this);
}

void Form::Show()
{
	EnsureInitialDpiApplied();
	ApplyWindowIcon();
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	RaiseShownOnce();
	this->OnSizeChanged(this);
	this->Invalidate(true);
}
static HWND GetBestOwnerWindowInCurrentProcess(HWND exclude = nullptr)
{
	HWND fg = GetForegroundWindow();
	if (fg && fg != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(fg, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(fg))
			return fg;
	}

	HWND active = GetActiveWindow();
	if (active && active != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(active, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(active))
			return active;
	}

	for (auto& kv : Application::Forms)
	{
		HWND h = kv.first;
		if (h && h != exclude && IsWindow(h) && IsWindowVisible(h))
			return h;
	}

	return nullptr;
}

void Form::ShowDialog(HWND parent)
{
	EnsureInitialDpiApplied();
	HWND owner = parent;
	if (!owner)
		owner = GetBestOwnerWindowInCurrentProcess(this->Handle);

	if (owner && IsWindow(owner))
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, (LONG_PTR)owner);
	else
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, 0);

	if (owner && IsWindow(owner))
	{
		EnableWindow(owner, FALSE);
	}

	ApplyWindowIcon();
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	RaiseShownOnce();
	this->OnSizeChanged(this);
	this->Invalidate(true);
	SetForegroundWindow(this->Handle);
	SetActiveWindow(this->Handle);

	MSG messageRecord;
	while (IsWindow(this->Handle))
	{
		BOOL r = GetMessageW(&messageRecord, nullptr, 0, 0);
		if (r <= 0) break;
		TranslateMessage(&messageRecord);
		DispatchMessageW(&messageRecord);
	}

	if (owner && IsWindow(owner))
	{
		EnableWindow(owner, TRUE);
		SetForegroundWindow(owner);
		SetActiveWindow(owner);
	}
}
void Form::Close()
{
	if (!this->Handle) return;
	bool canceled = false;
	this->OnClosing(this, canceled);
	if (!canceled)
		PostMessageW(this->Handle, WM_CLOSE, 0, 0);
}
bool Form::DoEvent()
{
	bool hasMessage = false;
	MSG messageRecord;
	while (PeekMessage(&messageRecord, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&messageRecord);
		DispatchMessage(&messageRecord);
		hasMessage = true;
	}
	if (!hasMessage && Application::Forms.size() > 0)
	{
		WaitMessage();
	}
	return hasMessage;
}
bool Form::WaitEvent()
{
	MSG messageRecord;
	if (GetMessageW(&messageRecord, nullptr, 0, 0))
	{
		TranslateMessage(&messageRecord);
		DispatchMessageW(&messageRecord);
		return true;
	}
	return false;
}
bool Form::Update(bool force)
{
	if (!IsWindow(this->Handle)) return false;

	if (!force && !ControlChanged) return false;

	RECT dirty{};
	if (!GetUpdateRect(this->Handle, &dirty, FALSE))
		return false;
	return UpdateDirtyRect(dirty, force);
}

bool Form::UpdateDirtyRect(const RECT& dirty, bool force)
{
	if (!IsWindow(this->Handle) || !this->Render) return false;

	if (dirty.right <= dirty.left || dirty.bottom <= dirty.top)
		return false;

	RECT clientRc{};
	::GetClientRect(this->Handle, &clientRc);

	// 在渲染前执行一次布局：否则直接挂在 Form 上的控件不会应用 Margin/Anchor 等布局属性
	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformLayout();
	}
	// dirty rect from OS is in physical pixels; D2D after SetDpi works in logical coords
	const float dpiSc = GetDpiScale();
	auto toLogical = [&](RECT r) -> RECT {
		return RECT{ (LONG)std::floor(r.left / dpiSc), (LONG)std::floor(r.top / dpiSc),
		             (LONG)std::ceil (r.right / dpiSc), (LONG)std::ceil (r.bottom / dpiSc) };
	};
	RECT logClientRc = toLogical(clientRc);
	RECT drawRc = toLogical(dirty);
	if (force || !this->_hasRenderedOnce)
	{
		drawRc = logClientRc;
	}
	const auto effectiveTheme = GetEffectiveThemeFrame();

	this->Render->BeginRender();
	this->Render->ClearTransform();
	this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
	this->Render->FillRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), effectiveTheme.WindowBackColor);
	this->Render->DrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), effectiveTheme.WindowBorderLightColor, 2.0f);
	this->Render->DrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), effectiveTheme.WindowBorderDarkColor, 1.0f);

	if (this->Image)
	{
		this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
		this->RenderImage();
		this->Render->PopDrawRect();
	}

	if (VisibleHead)
	{
		const float logW = this->Size.cx / dpiSc;
		const float logH = this->HeadHeight / dpiSc;
		RECT headRc{ 0, 0, (LONG)logW, (LONG)logH };
		if (RectIntersects(drawRc, headRc))
		{
			this->Render->FillRect(0, 0, logW, logH, effectiveTheme.TitleBarBackColor);
			auto font = this->GetFont();
			float headTextTop = (logH - font->FontHeight) * 0.5f;
			if (headTextTop < 0.0f)
				headTextTop = 0.0f;
			this->Render->PushDrawRect(0, 0, logW, logH);
			if (this->CenterTitle)
			{
				auto tSize = font->GetTextSize(this->Text);
				float textRangeWidth = logW;
				int buttonCount = 0;
				if (this->MinBox) buttonCount++;
				if (this->MaxBox) buttonCount++;
				if (this->CloseBox) buttonCount++;
				textRangeWidth -= (logH * buttonCount);
				float headTextLeft = (textRangeWidth - tSize.width) * 0.5f;
				if (headTextLeft < 0.0f)
					headTextLeft = 0.0f;
				this->Render->DrawString(this->Text, headTextLeft, headTextTop, effectiveTheme.WindowForeColor, font);
			}
			else
			{
				this->Render->DrawString(this->Text, 5.0f, headTextTop, effectiveTheme.WindowForeColor, font);
			}

			auto drawBtn = [&](CaptionButtonKind kind, CaptionButtonState st, D2D1_COLOR_F hover, D2D1_COLOR_F pressed)
				{
					RECT r{};
					if (!TryGetCaptionButtonRect(kind, r)) return;
					if (st == CaptionButtonState::Hover)
						this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), hover);
					else if (st == CaptionButtonState::Pressed)
						this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), pressed);

					const float left = (float)r.left;
					const float top = (float)r.top;
					const float bw = (float)(r.right - r.left);
					const float bh = (float)(r.bottom - r.top);
					const float s = (bw < bh) ? bw : bh;
					const float cx = left + bw * 0.5f;
					const float cy = top + bh * 0.5f;

					const float icon = s * 0.42f;
					const float half = icon * 0.5f;
					float stroke = s * 0.08f;
					if (stroke < 1.0f) stroke = 1.0f;

					auto drawMinimize = [&]()
						{
							const float y = cy + half * 0.35f;
							this->Render->DrawLine({ cx - half, y }, { cx + half, y }, effectiveTheme.WindowForeColor, stroke);
						};
					auto drawMaximize = [&]()
						{
							const float x = cx - half;
							const float y = cy - half;
							this->Render->DrawRect(x, y, icon, icon, effectiveTheme.WindowForeColor, stroke);
						};
					auto drawRestore = [&]()
						{
							const float restoreStroke = (std::min)(stroke, (std::max)(1.0f, s * 0.055f));
							const float total = s * 0.46f;
							const float rect = total * 0.68f;
							const float off = total - rect;
							const float xFront = cx - total * 0.5f;
							const float yFront = cy - total * 0.5f + off;
							const float xBack = xFront + off;
							const float yBack = yFront - off;
							this->Render->DrawLine({ xBack, yBack }, { xBack + rect, yBack }, effectiveTheme.WindowForeColor, restoreStroke);
							this->Render->DrawLine({ xBack + rect, yBack }, { xBack + rect, yBack + rect }, effectiveTheme.WindowForeColor, restoreStroke);
							this->Render->DrawLine({ xBack, yBack }, { xBack, yFront }, effectiveTheme.WindowForeColor, restoreStroke);
							this->Render->DrawLine({ xFront + rect, yBack + rect }, { xBack + rect, yBack + rect }, effectiveTheme.WindowForeColor, restoreStroke);
							this->Render->DrawRect(xFront, yFront, rect, rect, effectiveTheme.WindowForeColor, restoreStroke);
						};
					auto drawClose = [&]()
						{
							this->Render->DrawLine({ cx - half, cy - half }, { cx + half, cy + half }, effectiveTheme.WindowForeColor, stroke);
							this->Render->DrawLine({ cx + half, cy - half }, { cx - half, cy + half }, effectiveTheme.WindowForeColor, stroke);
						};

					switch (kind)
					{
					case CaptionButtonKind::Minimize:
						drawMinimize();
						break;
					case CaptionButtonKind::Maximize:
						if (IsZoomed(this->Handle))
							drawRestore();
						else
							drawMaximize();
						break;
					case CaptionButtonKind::Close:
						drawClose();
						break;
					}
				};

			drawBtn(CaptionButtonKind::Close, _capCloseState, effectiveTheme.CloseHoverColor, effectiveTheme.ClosePressedColor);
			drawBtn(CaptionButtonKind::Maximize, _capMaxState, effectiveTheme.CaptionHoverColor, effectiveTheme.CaptionPressedColor);
			drawBtn(CaptionButtonKind::Minimize, _capMinState, effectiveTheme.CaptionHoverColor, effectiveTheme.CaptionPressedColor);

			this->Render->PopDrawRect();
		}
	}
	const int top = (int)(ClientTop() / dpiSc);  // logical head height
	const int logContentW = (int)(this->Size.cx / dpiSc);
	const int logContentH = (int)(this->Size.cy / dpiSc);
	RECT contentDirty = drawRc;
	contentDirty.top -= top;
	contentDirty.bottom -= top;
	if (contentDirty.top < 0) contentDirty.top = 0;
	if (contentDirty.left < 0) contentDirty.left = 0;
	if (contentDirty.right > logContentW) contentDirty.right = logContentW;
	if (contentDirty.bottom > (logContentH - top)) contentDirty.bottom = (logContentH - top);

	if (contentDirty.right > contentDirty.left && contentDirty.bottom > contentDirty.top)
	{
		if (_dcompHost)
		{
			RenderDCompRootLayers(contentDirty, top, dpiSc);
			if (!this->OverlayRender)
			{
				this->Render->SetTransform(
					D2D1::Matrix3x2F::Translation(0.0f, (float)top));
				this->Render->PushDrawRect(
					(float)contentDirty.left, (float)contentDirty.top,
					(float)(contentDirty.right - contentDirty.left),
					(float)(contentDirty.bottom - contentDirty.top));
				RenderValidationToolTip();
				this->Render->PopDrawRect();
				this->Render->ClearTransform();
			}
		}
		else
		{
			this->Render->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)top));
			this->Render->PushDrawRect((float)contentDirty.left, (float)contentDirty.top, (float)(contentDirty.right - contentDirty.left), (float)(contentDirty.bottom - contentDirty.top));

			for (auto c : GetRootControlsInZOrder(this))
			{
				if (!c || !c->Visible) continue;
				if (c == this->ForegroundControl && !c->RenderNormalWhenForeground()) continue;
				if (c == this->MainMenu) continue;
				if (this->MainStatusBar && this->MainStatusBar->TopMost && c == this->MainStatusBar)
					continue;
				RECT crc = ToRECT(c->AbsRect, 2);
				if (!RectIntersects(contentDirty, crc)) continue;
				if (c->ParentForm->Render == nullptr)
					c->ParentForm->Render = this->Render;
				c->Update();
			}

			if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible)
			{
				this->MainStatusBar->Update();
			}

			if (!this->OverlayRender)
			{
				if (this->MainMenu && this->MainMenu->Visible)
				{
					this->MainMenu->Update();
				}
				if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl != (Control*)this->MainMenu)
				{
					this->ForegroundControl->UpdateForeground();
				}
				RenderValidationToolTip();
			}
			this->Render->PopDrawRect();
			this->Render->ClearTransform();
		}
	}

	this->OnPaint(this);

	this->Render->PopDrawRect();
	this->Render->EndRender();
	CommitComposition();
	RecoverRenderIfNeeded();

	if (this->OverlayRender)
	{
		auto* oldRender = this->Render;
		RECT fullClient{};
		::GetClientRect(this->Handle, &fullClient);
		// physical→logical for D2D overlay
		const float ovLogW = (fullClient.right  - fullClient.left) / dpiSc;
		const float ovLogH = (fullClient.bottom - fullClient.top)  / dpiSc;

		this->OverlayRender->BeginRender();
		this->OverlayRender->ClearTransform();
		this->OverlayRender->Clear(D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f });
		this->OverlayRender->PushDrawRect(0.0f, 0.0f, ovLogW, ovLogH);

		const int ovTop = (int)(ClientTop() / dpiSc);  // logical head height
		RECT overlayContent{};
		overlayContent.left   = 0;
		overlayContent.top    = 0;
		overlayContent.right  = (LONG)ovLogW;
		overlayContent.bottom = (LONG)ovLogH - ovTop;

		if (overlayContent.right > overlayContent.left && overlayContent.bottom > overlayContent.top)
		{
			this->OverlayRender->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)ovTop));
			this->OverlayRender->PushDrawRect((float)overlayContent.left, (float)overlayContent.top, (float)(overlayContent.right - overlayContent.left), (float)(overlayContent.bottom - overlayContent.top));

			this->Render = this->OverlayRender;
			if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible)
			{
				this->MainStatusBar->Update();
			}
			if (this->MainMenu && this->MainMenu->Visible)
			{
				this->MainMenu->Update();
			}
			if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl != (Control*)this->MainMenu)
			{
				this->ForegroundControl->UpdateForeground();
			}
			RenderValidationToolTip();
			this->Render = oldRender;

			this->OverlayRender->PopDrawRect();
			this->OverlayRender->ClearTransform();
		}

		this->OverlayRender->PopDrawRect();
		this->OverlayRender->EndRender();
		RecoverRenderIfNeeded();
	}

	this->ControlChanged = false;
	this->_hasRenderedOnce = true;
	RefreshAnimationTimer();
	return true;
}
bool Form::ForceUpdate()
{
	this->Invalidate(true);
	return true;
}

void Form::ClearDetachedControlReferences(Control* root)
{
	if (!root)
		return;
	auto belongsToDetachedTree = [root](Control* control)
	{
		return IsControlOrDescendantOf(control, root);
	};

	if (belongsToDetachedTree(this->ForegroundControl))
		this->ForegroundControl = nullptr;
	if (belongsToDetachedTree(this->MainMenu))
		this->MainMenu = nullptr;
	if (belongsToDetachedTree(this->MainToolBar))
		this->MainToolBar = nullptr;
	if (belongsToDetachedTree(this->MainStatusBar))
		this->MainStatusBar = nullptr;
	if (belongsToDetachedTree(this->_defaultButton))
		this->_defaultButton = nullptr;
	if (belongsToDetachedTree(this->_cancelButton))
		this->_cancelButton = nullptr;
	if (belongsToDetachedTree(this->UnderMouse))
		this->UnderMouse = nullptr;
	if (belongsToDetachedTree(this->Selected))
		this->SetSelectedControl(nullptr, true);
	if (belongsToDetachedTree(this->_focusNotifiedSelected))
	{
		auto* previousSelection = this->_focusNotifiedSelected;
		this->_focusNotifiedSelected = nullptr;
		previousSelection->OnLostFocus(previousSelection);
		previousSelection->InvalidateVisual();
	}
	if (belongsToDetachedTree(this->_hoverControl))
		this->_hoverControl = nullptr;
	if (belongsToDetachedTree(this->_mouseCaptureControl))
	{
		this->_mouseCaptureControl = nullptr;
		if (this->Handle && GetCapture() == this->Handle)
			ReleaseCapture();
	}
}

std::unique_ptr<Control> Form::DetachControl(Control* control)
{
	if (!control)
		return {};
	auto position = std::find(this->Controls.begin(), this->Controls.end(), control);
	if (position == this->Controls.end())
		return {};

	ClearDetachedControlReferences(control);
	this->Controls.erase(position);
	control->Parent = nullptr;
	control->_isFormRoot = false;
	Control::SetChildrenParentForm(control, nullptr);
	this->InvalidateLayout();
	NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
	return std::unique_ptr<Control>(control);
}

bool Form::TryInsertOwned(
	int index, std::unique_ptr<Control>& control) noexcept
{
	if (!control) return false;
	auto* raw = control.get();
	if (index < 0 || static_cast<size_t>(index) > Controls.size()
		|| raw->Parent || raw->ParentForm || IndexOfControl(raw) >= 0)
		return false;
	try
	{
		InsertControl(index, raw);
		control.release();
		return true;
	}
	catch (...)
	{
		if (IndexOfControl(raw) >= 0)
		{
			try
			{
				auto detached = DetachControl(raw);
				if (detached) control = std::move(detached);
			}
			catch (...) {}
		}
		return false;
	}
}

int Form::IndexOfControl(const Control* control) const noexcept
{
	if (!control) return -1;
	const auto found = std::find(
		Controls.begin(), Controls.end(), control);
	return found == Controls.end()
		? -1 : static_cast<int>(found - Controls.begin());
}

bool Form::DeleteControl(Control* control)
{
	auto detached = DetachControl(control);
	return detached != nullptr;
}

bool Form::RemoveControl(Control* control)
{
	auto detached = DetachControl(control);
	if (!detached)
		return false;
	detached.release();
	return true;
}
bool Form::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	_lastKeyboardMessageHandled = false;
	const bool keyboardInput = message == WM_KEYDOWN || message == WM_SYSKEYDOWN
		|| message == WM_CHAR || message == WM_SYSCHAR;
	const bool pointerInput = message == WM_LBUTTONDOWN
		|| message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
	if (keyboardInput && !_keyboardFocusVisualRequested)
	{
		_keyboardFocusVisualRequested = true;
		if (Selected) Selected->InvalidateVisual();
	}
	else if (pointerInput && _keyboardFocusVisualRequested
		&& !_systemVisualPreferences.KeyboardCuesAlwaysVisible)
	{
		_keyboardFocusVisualRequested = false;
		if (Selected) Selected->InvalidateVisual();
	}
	if (!this->Enable || !this->Visible) return true;
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	const int titleBarHeight = ClientTop();  // physical HeadHeight, for title bar (OS-level) comparisons
	const float dpiScale = GetDpiScale();
	POINT contentMouse{ (LONG)(mouse.x / dpiScale), (LONG)((mouse.y - titleBarHeight) / dpiScale) };
	Control* hitControl = nullptr;
	auto anyMouseButtonDown = []() -> bool
		{
			return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
				(GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
				(GetAsyncKeyState(VK_MBUTTON) & 0x8000);
		};
	auto forwardToCapturedControl = [&](UINT messageId, WPARAM wParamValue, LPARAM lParamValue) -> bool
		{
			if (!this->_mouseCaptureControl || !this->_mouseCaptureControl->IsVisual)
				return false;
			hitControl = this->_mouseCaptureControl;
			const auto location = this->_mouseCaptureControl->GetAbsoluteLocationDip();
			this->_mouseCaptureControl->ProcessMessage(
				messageId, wParamValue, lParamValue,
				ToLegacyLocalCoordinate((float)contentMouse.x - location.x),
				ToLegacyLocalCoordinate((float)contentMouse.y - location.y));
			return true;
		};
	auto releaseCapturedControlIfIdle = [&]()
		{
			if (this->_mouseCaptureControl && !anyMouseButtonDown())
			{
				this->_mouseCaptureControl = nullptr;
				if (this->Handle && GetCapture() == this->Handle)
					ReleaseCapture();
			}
		};
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
			auto* target = HitTestControlAt(contentMouse);
			if (target)
			{
				target->OnDropFile(target, files);
			}
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		if (!this->_mouseLeaveTracking && this->Handle)
		{
			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = this->Handle;
			::TrackMouseEvent(&tme);
			this->_mouseLeaveTracking = true;
		}

		if (this->VisibleHead && mouse.y < this->HeadHeight)
		{
			UpdateCaptionHover(mouse);
		}
		else if (this->_capMinState != CaptionButtonState::None || this->_capMaxState != CaptionButtonState::None || this->_capCloseState != CaptionButtonState::None)
		{
			if (!this->_capPressed)
			{
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
			}
		}

		if (this->VisibleHead && mouse.y < titleBarHeight)
		{
			if (this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
			{
				UpdateCursor(mouse, contentMouse);
				break;
			}
			RaiseControlMouseEnterLeave(this, this->_hoverControl, nullptr, contentMouse);
			this->_hoverControl = nullptr;
			this->UnderMouse = nullptr;
			this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
			ApplyCursor(CursorKind::Arrow);
			break;
		}

		if (this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
		{
			UpdateCursor(mouse, contentMouse);
			break;
		}

		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				RaiseControlMouseEnterLeave(this, this->_hoverControl, this->Selected, contentMouse);
				this->_hoverControl = this->Selected;
				this->UnderMouse = this->Selected;
				hitControl = this->Selected;
				const auto location = this->Selected->GetAbsoluteLocationDip();
				this->Selected->ProcessMessage(
					message, wParam, lParam,
					ToLegacyLocalCoordinate((float)contentMouse.x - location.x),
					ToLegacyLocalCoordinate((float)contentMouse.y - location.y));
				UpdateCursor(mouse, contentMouse);
				break;
			}
		}

		Control* newHover = HitTestControlAt(contentMouse);
		RaiseControlMouseEnterLeave(this, this->_hoverControl, newHover, contentMouse);
		this->_hoverControl = newHover;
		this->UnderMouse = newHover;

		auto hit = HitTestRootControlAt(this, contentMouse);
		if (hit)
		{
			hitControl = hit;
			const auto hitLocation = hit->GetAbsoluteLocationDip();
			hit->ProcessMessage(
				message, wParam, lParam,
				ToLegacyLocalCoordinate((float)contentMouse.x - hitLocation.x),
				ToLegacyLocalCoordinate((float)contentMouse.y - hitLocation.y));
		}
		this->UnderMouse = this->_hoverControl;
		UpdateCursor(mouse, contentMouse);
		this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
	}
	break;
	case WM_MOUSELEAVE:
	{
		this->_mouseLeaveTracking = false;
		if (this->_mouseCaptureControl)
		{
			UpdateCursorFromCurrentMouse();
			break;
		}
		RaiseControlMouseEnterLeave(this, this->_hoverControl, nullptr, contentMouse);
		this->_hoverControl = nullptr;
		this->UnderMouse = nullptr;
		UpdateCursorFromCurrentMouse();
	}
	break;
	case WM_MOUSEWHEEL:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONDBLCLK:
	{
		Control* selectedBeforeLeftDown = (message == WM_LBUTTONDOWN) ? this->Selected : nullptr;
		Control* pointerHover = nullptr;
		if (!(this->VisibleHead && mouse.y < titleBarHeight))
		{
			pointerHover = HitTestControlAt(contentMouse);
		}
		RaiseControlMouseEnterLeave(this, this->_hoverControl, pointerHover, contentMouse);
		this->_hoverControl = pointerHover;
		this->UnderMouse = pointerHover;

		DismissForegroundOnOutsideMouseDown(this, pointerHover, contentMouse, message);

		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			if (!(this->VisibleHead && mouse.y < titleBarHeight))
			{
				Control* hit = HitTestControlAt(contentMouse);
				if (::GetFocus() != this->Handle)
					::SetFocus(this->Handle);
			}
		}

		if (WM_LBUTTONDOWN == message)
		{
			if (VisibleHead)
			{
				CaptionButtonKind kind{};
				if (HitTestCaptionButtons(mouse, kind))
				{
					_capPressed = true;
					_capPressedKind = kind;
					_capTracking = true;
					UpdateCaptionHover(mouse);
					SetCapture(this->Handle);
					break;
				}

				if (mouse.y < titleBarHeight)
				{
					ReleaseCapture();
					PostMessage(this->Handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
				}
			}
		}
		else if (WM_LBUTTONUP == message)
		{
			if (_capTracking)
			{
				ReleaseCapture();
				_capTracking = false;
				CaptionButtonKind kind{};
				bool hit = HitTestCaptionButtons(mouse, kind);
				if (_capPressed && hit && kind == _capPressedKind)
				{
					_capPressed = false;
					ClearCaptionStates();
					ExecuteCaptionButton(kind);
					UpdateCursor(mouse, contentMouse);
					break;
				}
				_capPressed = false;
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
				UpdateCursor(mouse, contentMouse);
				break;
			}

			if (this->Selected)
			{
				if (this->Selected->IsVisual)
				{
					hitControl = this->Selected;
					const auto location = this->Selected->GetAbsoluteLocationDip();
					this->Selected->ProcessMessage(
						message, wParam, lParam,
						ToLegacyLocalCoordinate((float)contentMouse.x - location.x),
						ToLegacyLocalCoordinate((float)contentMouse.y - location.y));
					UpdateCursor(mouse, contentMouse);
					break;
				}
			}
		}
		else if (WM_LBUTTONDBLCLK == message)
		{
			if (VisibleHead && mouse.y < this->HeadHeight)
			{
				CaptionButtonKind kind{};
				if (!HitTestCaptionButtons(mouse, kind))
				{
					ExecuteCaptionButton(CaptionButtonKind::Maximize);
					break;
				}
			}
		}
		if (this->VisibleHead && mouse.y < titleBarHeight)
		{
			if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) &&
				this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
			{
				releaseCapturedControlIfIdle();
				UpdateCursor(mouse, contentMouse);
				break;
			}
			break;
		}

		if ((message == WM_MOUSEWHEEL || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) &&
			this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
		{
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
				releaseCapturedControlIfIdle();
			UpdateCursor(mouse, contentMouse);
			break;
		}

		if (message == WM_MOUSEWHEEL)
		{
			const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			Control* wheelHit = HitTestControlAt(contentMouse);
			for (Control* target = wheelHit; target; target = target->Parent)
			{
				if (!target->HandlesMouseWheel()) continue;
				const auto targetAbs = target->GetAbsoluteLocationDip();
				const int targetX = ToLegacyLocalCoordinate((float)contentMouse.x - targetAbs.x);
				const int targetY = ToLegacyLocalCoordinate((float)contentMouse.y - targetAbs.y);
				if (!target->CanHandleMouseWheel(delta, targetX, targetY)) continue;
				if (target->ProcessMessage(message, wParam, lParam, targetX, targetY))
				{
					hitControl = target;
					break;
				}
			}
			this->OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, delta));
			break;
		}

		auto hit = HitTestRootControlAt(this, contentMouse);
		if (hit)
		{
			hitControl = hit;
			const auto hitLocation = hit->GetAbsoluteLocationDip();
			if (message == WM_MOUSEWHEEL)
			{
				const int controlLocalX = ToLegacyLocalCoordinate((float)contentMouse.x - hitLocation.x);
				const int controlLocalY = ToLegacyLocalCoordinate((float)contentMouse.y - hitLocation.y);
				const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				if (!hit->CanHandleMouseWheel(delta, controlLocalX, controlLocalY))
					hitControl = nullptr;
				else if (!hit->ProcessMessage(message, wParam, lParam, controlLocalX, controlLocalY))
					hitControl = nullptr;
			}
			else
			{
				hit->ProcessMessage(
					message, wParam, lParam,
					ToLegacyLocalCoordinate((float)contentMouse.x - hitLocation.x),
					ToLegacyLocalCoordinate((float)contentMouse.y - hitLocation.y));
			}
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
			{
				this->_mouseCaptureControl = hit;
				if (this->Handle)
					SetCapture(this->Handle);
			}
		}
		if (message == WM_MOUSEWHEEL)
		{
			this->OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, GET_WHEEL_DELTA_WPARAM(wParam)));
		}
		else if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			this->OnMouseDown(this, MouseEventArgs(FromParamToMouseButtons(message), 0, contentMouse.x, contentMouse.y, HIWORD(wParam)));
		}
		else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
		{
			this->OnMouseUp(this, MouseEventArgs(FromParamToMouseButtons(message), 0, contentMouse.x, contentMouse.y, HIWORD(wParam)));
		}

		if (message == WM_LBUTTONDOWN && selectedBeforeLeftDown && this->Selected == selectedBeforeLeftDown && pointerHover != selectedBeforeLeftDown)
		{
			this->SetSelectedControl(nullptr, true);
		}

		if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
		{
			releaseCapturedControlIfIdle();
			UpdateCursor(mouse, contentMouse);
		}
	}
	break;
	case WM_CAPTURECHANGED:
	{
		if ((HWND)lParam != this->Handle)
			this->_mouseCaptureControl = nullptr;
	}
	break;
	case WM_KEYDOWN:
	{
		const bool firstPress = (static_cast<unsigned long long>(lParam)
			& (1ull << 30)) == 0;
		const bool selectedHandles = this->Selected
			&& this->Selected->HandlesNavigationKey(wParam);
		if (wParam == VK_TAB && !selectedHandles)
		{
			_lastKeyboardMessageHandled = MoveFocus(
				(::GetKeyState(VK_SHIFT) & 0x8000) == 0);
			if (_lastKeyboardMessageHandled)
			{
				_suppressedCharacter = L'\t';
				this->OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
				break;
			}
		}

		Control* invocationTarget = nullptr;
		if (!selectedHandles && wParam == VK_ESCAPE)
			invocationTarget = _cancelButton;
		else if (!selectedHandles && wParam == VK_RETURN)
		{
			if (IsKeyboardInvokable(this->Selected))
			{
				_lastKeyboardMessageHandled = true;
				_suppressedCharacter = L'\r';
				this->Selected->OnKeyDown(
					this->Selected, KeyEventArgs((Keys)(wParam | 0)));
				this->OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
				if (firstPress) (void)this->Selected->Invoke();
				break;
			}
			invocationTarget = _defaultButton;
		}
		else if (!selectedHandles && wParam == VK_SPACE && this->Selected)
		{
			if (IsKeyboardInvokable(this->Selected))
			{
				_lastKeyboardMessageHandled = true;
				_suppressedCharacter = L' ';
				this->Selected->OnKeyDown(
					this->Selected, KeyEventArgs((Keys)(wParam | 0)));
				this->OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
				if (firstPress) (void)this->Selected->Invoke();
				break;
			}
		}

		const auto invocationSnapshot = invocationTarget
			? invocationTarget->GetAccessibilitySnapshot()
			: AccessibilitySnapshot{};
		if (invocationTarget && invocationTarget->ParentForm == this
			&& invocationSnapshot.Enabled && invocationSnapshot.Visible)
		{
			_lastKeyboardMessageHandled = true;
			_suppressedCharacter = wParam == VK_ESCAPE ? L'\x1b' : L'\r';
			if (this->Selected)
				this->Selected->OnKeyDown(
					this->Selected, KeyEventArgs((Keys)(wParam | 0)));
			this->OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
			if (firstPress) (void)invocationTarget->Invoke();
			break;
		}

		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				if (this->Selected->IsVisual)
				{
					hitControl = this->Selected;
					KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
					this->OnKeyDown(this, eventArgs);
				}
			}
			else
			{
				auto fallbackTarget = GetScrollViewFallbackTarget(this->Selected, wParam);
				if (fallbackTarget && fallbackTarget->ProcessMessage(message, wParam, lParam, localX, localY))
				{
					hitControl = fallbackTarget;
					KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
					this->OnKeyDown(this, eventArgs);
				}
			}
		}
		else
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, eventArgs);
		}
	}
	break;
	case WM_SYSCHAR:
	{
		_lastKeyboardMessageHandled = ProcessAccessKey(static_cast<wchar_t>(wParam));
		if (_lastKeyboardMessageHandled)
			this->OnKeyDown(this, KeyEventArgs((Keys)(std::towupper((wchar_t)wParam) | 0)));
	}
	break;
	case WM_SETFOCUS:
	{
		this->OnGotFocus(this);
	}
	break;
	case WM_KILLFOCUS:
	{
		if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl->AutoCloseOnFormFocusLoss())
		{
			this->ForegroundControl->ClosePopup();
			this->Invalidate(true);
		}
		if (this->MainMenu && this->MainMenu->Visible && this->MainMenu->AutoCloseOnFormFocusLoss())
		{
			this->MainMenu->ClosePopup();
			this->Invalidate(true);
		}
		this->OnLostFocus(this);
	}
	break;
	case WM_KEYUP:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				hitControl = this->Selected;
				KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
				this->OnKeyUp(this, eventArgs);
			}
		}
		else
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyUp(this, eventArgs);
		}
	}
	break;
	case WM_SIZE:
	{
		RECT rec;
		GetClientRect(this->Handle, &rec);
		UINT width = (UINT)std::max<LONG>(1, rec.right - rec.left);
		UINT height = (UINT)std::max<LONG>(1, rec.bottom - rec.top);
		if (this->_dcompHost) this->_dcompHost->UpdateD2DLayerSize(width, height);
		this->Render->ReSize(width, height);
		if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
		for (auto& layer : this->_dcompD2DLayers)
			if (layer.Render) layer.Render->ReSize(width, height);
		if (this->Render)        this->Render->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
		if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
		for (auto& layer : this->_dcompD2DLayers)
			if (layer.Render) layer.Render->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
		this->InvalidateLayout();
		this->_hasRenderedOnce = false;
		this->OnSizeChanged(this);
		this->Invalidate(false);
	}
	break;
	case WM_MOVE:
	{
		RECT client_rectangle;
		GetClientRect(this->Handle, &client_rectangle);
		this->OnMoved(this);
	}
	break;
	case WM_PAINT:
	{

	}
	break;
	case WM_CHAR:
	{
		if (_suppressedCharacter != L'\0'
			&& static_cast<wchar_t>(wParam) == _suppressedCharacter)
		{
			_suppressedCharacter = L'\0';
			_lastKeyboardMessageHandled = true;
			break;
		}
		_suppressedCharacter = L'\0';
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				hitControl = this->Selected;
				this->OnCharInput(this, (wchar_t)(wParam));
			}
		}
		else
		{
			hitControl = nullptr;
			this->OnCharInput(this, (wchar_t)(wParam));
		}
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (this->Selected)
		{
			hitControl = this->Selected;
			this->Selected->ProcessMessage(message, wParam, lParam, localX, localY);
		}
	}
	break;
	case WM_CLOSE:
	{
		this->OnFormClosing(this);
		delete this->Render;
		this->Render = nullptr;
		return true;
	}
	break;
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		int additionalInfo = HIWORD(wParam);
		this->OnCommand(this, id, additionalInfo);
	}
	break;
	};
	// 兼容：旧控件代码路径直接写 Selected，这里补齐焦点事件
	if (this->_focusNotifiedSelected != this->Selected)
	{
		auto* old = this->_focusNotifiedSelected;
		auto* now = this->Selected;
		this->_focusNotifiedSelected = now;
		if (old)
		{
			old->OnLostFocus(old);
			old->InvalidateVisual();
		}
		if (now)
		{
			now->OnGotFocus(now);
			now->InvalidateVisual();
		}
	}
	if (WM_LBUTTONDOWN == message && hitControl == nullptr && this->Selected && hitControl != this->Selected)
	{
		this->SetSelectedControl(nullptr, true);
		UpdateCursor(mouse, contentMouse);
	}
	return true;
}
void Form::RenderImage()
{
	auto* bmp = EnsureImageCache();
	if (bmp)
	{
		auto size = bmp->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			// 自绘标题栏属于 client 区域的一部分：背景图应铺满整个窗口区域（D2D 逻辑坐标）
			const float rSc = GetDpiScale();
			const struct { float cx, cy; } asize = { this->Size.cx / rSc, this->Size.cy / rSc };
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->Render->DrawBitmap(bmp, 0, 0, size.width, size.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				float xf = (asize.cx - size.width) / 2.0f;
				float yf = (asize.cy - size.height) / 2.0f;
				this->Render->DrawBitmap(bmp, xf, yf, size.width, size.height);
			}
			break;
			case ImageSizeMode::StretchImage:
			{
				this->Render->DrawBitmap(bmp, 0, 0, (float)asize.cx, (float)asize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float xp = asize.cx / size.width, yp = asize.cy / size.height;
				float tp = xp < yp ? xp : yp;
				float tw = size.width * tp, th = size.height * tp;
				float xf = (asize.cx - tw) / 2.0f, yf = (asize.cy - th) / 2.0f;
				this->Render->DrawBitmap(bmp, xf, yf, tw, th);
			}
			break;
			default:
				break;
			}
		}
	}
}
Control* Form::LastChild()
{
	if (this->Controls.size())
	{
		return this->Controls.back();
	}
	return nullptr;
}
D2D1_RECT_F Form::ChildRect()
{
	if (this->Controls.size() == 0)
		return D2D1_RECT_F{ 0,0,0,0 };
	cui::core::Rect bounds{};
	bool hasBounds = false;
	for (auto control : this->Controls)
	{
		if (!control || !control->Visible) continue;
		const cui::core::Rect childRect{
			control->GetActualLocationDip(), control->GetActualSizeDip() };
		bounds = hasBounds ? bounds.Union(childRect) : childRect;
		hasBounds = true;
	}
	return hasBounds
		? D2D1_RECT_F{ bounds.Left(), bounds.Top(), bounds.Right(), bounds.Bottom() }
		: D2D1_RECT_F{ 0,0,0,0 };
}
LRESULT CustomFrameHitTest(HWND _hWnd, WPARAM wParam, LPARAM lParam, int captionHeight, UINT dpi)
{
	const int scalerWidth = (std::max)(1, Application::ScaleInt(8, 96, dpi));
	RECT wr, cr;
	const POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	GetWindowRect(_hWnd, &wr);
	cr.left = wr.left + scalerWidth;
	cr.right = wr.right - scalerWidth;
	cr.bottom = wr.bottom - scalerWidth;
	cr.top = wr.top + scalerWidth;

	uint8_t pos_code = 0;
	if (ptMouse.x < wr.left || ptMouse.x > wr.right || ptMouse.y < wr.top || ptMouse.y > wr.bottom)
		return HTNOWHERE;

	if (ptMouse.x < cr.left)
		pos_code |= 0b01;
	else if (ptMouse.x > cr.right)
		pos_code |= 0b11;
	else
		pos_code |= 0b10;

	if (ptMouse.y < cr.top)
		pos_code |= 0b0100;
	else if (captionHeight > 0 && ptMouse.y < wr.top + captionHeight)
		return HTCAPTION;
	else if (ptMouse.y > cr.bottom)
		pos_code |= 0b1100;
	else
		pos_code |= 0b1000;

	switch (pos_code)
	{
	case 0b0101:
		return HTTOPLEFT;
	case 0b0110:
		return HTTOP;
	case 0b0111:
		return HTTOPRIGHT;
	case 0b1001:
		return HTLEFT;
	case 0b1010:
		return HTCLIENT;
	case 0b1011:
		return HTRIGHT;
	case 0b1101:
		return HTBOTTOMLEFT;
	case 0b1110:
		return HTBOTTOM;
	case 0b1111:
		return HTBOTTOMRIGHT;
	}
	return HTNOWHERE;
}
LRESULT CALLBACK Form::WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (NotifyIcon::DispatchWindowMessage(hWnd, message, wParam, lParam))
		return 0;

	Form* form = (Form*)(GetWindowLongPtrW(hWnd, GWLP_USERDATA) ^ 0xFFFFFFFFFFFFFFFF);
	if ((ULONG64)form != 0xFFFFFFFFFFFFFFFF && Application::Forms.find(form->Handle) != Application::Forms.end())
	{
		if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED
			|| message == WM_SYSCOLORCHANGE)
		{
			form->RefreshSystemVisualPreferences();
			form->Invalidate(true);
		}
		if (message == WM_DPICHANGED)
		{
			UINT newDpi = HIWORD(wParam);
			RECT* suggested = (RECT*)lParam;
			if (suggested)
			{
				SetWindowPos(hWnd, nullptr,
					suggested->left,
					suggested->top,
					suggested->right - suggested->left,
					suggested->bottom - suggested->top,
					SWP_NOZORDER | SWP_NOACTIVATE);
				form->_initialWindowRectApplied = !form->_initialDpiApplied;
			}
			// 尺寸/DPI 变化后，强制同步渲染目标尺寸并安排一次重绘，避免出现新区域未刷新。
			form->SyncRenderSizeToClient();
			form->_hasRenderedOnce = false;
			form->Invalidate(false);
			// 若窗口尚未首次显示，控件树可能还未构造完成：此时只记录 DPI，真正缩放留到 Show 前。
			if (!form->_initialDpiApplied)
			{
				form->_dpi = newDpi;
				return 0;
			}
			form->ApplyDpiChange(newDpi);
			return 0;
		}
		if (message == WM_GETOBJECT)
		{
			const LRESULT accessible = form->HandleAccessibleObjectRequest(wParam, lParam);
			if (accessible != 0) return accessible;
		}

		form->ProcessMessage(message, wParam, lParam, 0, 0);
		if (form->_lastKeyboardMessageHandled)
			return 0;

		// After any button-up, release lingering mouse capture from child controls
		// (TabControl / GridView call SetCapture for drag tracking; if the mouse moves
		// to the resize border before the button is released, the hit-test in
		// ProcessMessage finds no control there, so the child never receives its
		// WM_LBUTTONUP and never calls ReleaseCapture — leaving capture stuck and
		// blocking the OS from starting a window-resize drag via WM_NCHITTEST).
		if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			&& GetCapture() == hWnd && !form->_capTracking)
		{
			ReleaseCapture();
		}

		switch (message)
		{
		case WM_NCCALCSIZE:
		{
			if (wParam)
			{
				NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				if (params)
				{
					if (IsZoomed(hWnd))
					{
						const int inset = GetCustomFrameInset();
						params->rgrc[0].left += inset;
						params->rgrc[0].right -= inset;
						params->rgrc[0].top += inset;
						params->rgrc[0].bottom -= inset;
					}
					return 0;
				}
			}
			break;
		}
		case WM_SETCURSOR:
		{
			if (LOWORD(lParam) == HTCLIENT)
			{
				form->UpdateCursorFromCurrentMouse();
				return TRUE;
			}
		}
		break;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hWnd, &ps);
			if (form->Render)
			{
				if (!::IsWindowEnabled(hWnd))
				{
					EndPaint(hWnd, &ps);
					return 0;
				}

				if (form->ControlChanged || !form->_hasRenderedOnce)
					form->UpdateDirtyRect(ps.rcPaint, true);
			}
			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_TIMER:
		{
			if (wParam == form->_animTimerId)
			{
				form->InvalidateAnimatedControls(true);
				return 0;
			}
		}
		break;
		case WM_ACTIVATE:
		{
			constexpr MARGINS margins{ 1, 1, 1, 1 };
			DwmExtendFrameIntoClientArea(hWnd, &margins);
		}
		break;
		case WM_NCHITTEST:
		{
			LRESULT hitTestResult;
			if (!DwmDefWindowProc(hWnd, message, wParam, lParam, &hitTestResult))
			{
				POINT ptClient{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				ScreenToClient(hWnd, &ptClient);
				if (form->HitTestCaptionButtonResizeExclusion(ptClient))
					return HTCLIENT;

				hitTestResult = CustomFrameHitTest(hWnd, wParam, lParam, form->ClientTop(), form->_dpi);
				if (hitTestResult == HTCAPTION)
				{
					CaptionButtonKind k{};
					if (form->HitTestCaptionButtons(ptClient, k))
						return HTCLIENT;
				}
				if (IsZoomed(hWnd))
				{
					// 最大化状态下禁止鼠标拖拽边缘/角落调整窗口大小
					if (hitTestResult == HTLEFT || hitTestResult == HTRIGHT || hitTestResult == HTTOP || hitTestResult == HTBOTTOM ||
						hitTestResult == HTTOPLEFT || hitTestResult == HTTOPRIGHT || hitTestResult == HTBOTTOMLEFT || hitTestResult == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
				}
				if (!form->AllowResize)
				{
					// 禁用边缘/角落 resize，只保留标题栏拖动与正常客户区
					if (hitTestResult == HTLEFT || hitTestResult == HTRIGHT || hitTestResult == HTTOP || hitTestResult == HTBOTTOM ||
						hitTestResult == HTTOPLEFT || hitTestResult == HTTOPRIGHT || hitTestResult == HTBOTTOMLEFT || hitTestResult == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
				}
				if (hitTestResult != HTCAPTION)
				{
					return hitTestResult;
				}
			}
		}
		break;
		case WM_NCDESTROY:
		{
			form->OnFormClosed(form);
			Application::Forms.erase(form->Handle);
			form->CleanupResources();
		}
		break;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

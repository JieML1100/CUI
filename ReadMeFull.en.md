# CUI - Modern Windows GUI Framework Complete Documentation

## Table of Contents

1. [Project Overview](#project-overview)
2. [Core Features](#core-features)
3. [Project Architecture](#project-architecture)
4. [Quick Start](#quick-start)
5. [Controls Guide](#controls-guide)
6. [Layout System](#layout-system)
7. [Events and Input](#events-and-input)
8. [Advanced Features](#advanced-features)
9. [Visual Designer](#visual-designer)
10. [Code Examples](#code-examples)
11. [FAQ](#faq)
12. [Dependencies and Requirements](#dependencies-and-requirements)
13. [Community and Support](#community-and-support)

---

## Project Overview

CUI is a modern native Windows GUI framework based on **Direct2D** and **DirectComposition**, written in C++20. It provides a complete high-performance solution for Windows desktop application development, along with a powerful visual UI designer.

### Core Design Principles

- **High-Performance Rendering**: Leveraging Direct2D hardware acceleration and DirectComposition compositor engine
- **Modern API**: Using C++20 features with a clean and easy-to-use object-oriented interface
- **WYSIWYG**: Visual designer supporting drag-and-drop UI development
- **Native Experience**: Fully based on Windows graphics stack for native application experience

### Application Scenarios

CUI is suitable for developing various Windows desktop applications:

- Professional tools software
- Multimedia applications
- Data visualization tools
- Industrial control systems
- Embedded device interfaces
- Web hybrid applications

---

## Core Features

### High-Performance Rendering Engine

CUI uses Direct2D as the underlying rendering engine, fully utilizing GPU hardware acceleration:

```cpp
// Direct2D hardware-accelerated rendering
auto render = new D2DGraphics1(hwnd);
render->BeginRender();
render->Clear(D2D1::ColorF(0, 0, 0, 0));
// Execute rendering operations
render->EndRender();
```

**Rendering Features:**
- Hardware-accelerated 2D graphics rendering
- DirectComposition window compositing
- Smooth animation support
- High DPI-aware rendering
- Anti-aliased text rendering

### Rich Control Library

CUI provides 33+ common UI controls covering various development needs:

**Basic Controls:**
- `Button` - Button control
- `Label` - Text label
- `LinkLabel` - Hyperlink label
- `TextBox` - Single-line text input
- `RichTextBox` - Rich text editing
- `PasswordBox` - Password input box
- `ComboBox` - Dropdown selection box
- `CheckBox` - Checkbox
- `RadioBox` - Radio button
- `Switch` - Switch control
- `Slider` - Slider control
- `ProgressBar` - Progress bar

**Container Controls:**
- `Panel` - General panel
- `TabControl` / `TabPage` - Tab control
- `GroupBox` - Group box (extensible)

**Data Display:**
- `TreeView` - Tree view
- `GridView` - Data grid
- `ListView` - List view (extensible)

**Media Controls:**
- `PictureBox` - Image display
- `MediaPlayer` - Media player
- `WebBrowser` - WebView2 browser

**System Integration:**
- `Menu` / `MenuItem` - Menu system
- `ToolBar` - Toolbar
- `StatusBar` - Status bar
- `NotifyIcon` - Tray icon
- `Taskbar` - Taskbar integration

### Flexible Layout System

CUI provides five powerful layout containers to meet various interface requirements:

| Layout Type | Features | Suitable Scenarios |
|-------------|----------|-------------------|
| `StackPanel` | Linear arrangement of child controls | Toolbars, forms |
| `GridPanel` | Grid row/column layout | Complex table interfaces |
| `DockPanel` | Docking layout | Window docking, IDE interfaces |
| `WrapPanel` | Auto-wrapping layout | Toolboxes, icon grids |
| `RelativePanel` | Relative constraint layout | Precise position control |

### Complete Event System

CUI provides a comprehensive event handling mechanism:

```cpp
// Button click event
button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // Handle click
};

// Text change event
textBox->OnTextChanged += [](Control* sender, const std::wstring& oldText, 
                              const std::wstring& newText) {
    // Handle text change
};

// Keyboard event
control->OnKeyDown += [](Control* sender, KeyEventArgs e) {
    if (e.Key == VK_RETURN) {
        // Return key handling
    }
};
```

**Supported Event Types:**
- Mouse events (click, move, scroll, drag-drop)
- Keyboard events (key press, character input)
- Focus events (gain/lose focus)
- Control events (select, change, scroll)
- Window events (size change, move, close)

### International Input Support

CUI natively supports IME (Input Method Editor) for Chinese, Japanese, Korean and other complex text input:

```cpp
// Enable Chinese input
textBox->ImeMode = ImeMode::On;
```

### SVG Vector Graphics

Built-in nanosvg library supporting SVG vector graphics rendering:

```cpp
// Load bitmap from SVG data
ID2D1Bitmap* svgBitmap = ToBitmapFromSvg(graphics, svgContent);
pictureBox->SetImageEx(svgBitmap, false);
```

### WebView2 Integration

Embed Web content and implement JS/C++ interop through WebView2:

```cpp
// Create WebBrowser control
WebBrowser* web = new WebBrowser(10, 40, 800, 500);

// Register JS-to-C++ handler
web->RegisterJsInvokeHandler(L"native.echo", [](const std::wstring& payload) {
    return L"echo: " + payload;
});

// C++ calls JS
web->ExecuteScriptAsync(L"window.setFromNative('Hello from C++');");

// Set HTML content
web->SetHtml(L"<html><body><h1>Hello World</h1></body></html>");
```

### Media Playback Function

Built-in MediaPlayer control supporting multiple media formats:

```cpp
// Create media player
MediaPlayer* player = new MediaPlayer(10, 40, 800, 500);

// Load and play video
player->Load(L"C:\\video\\demo.mp4");
player->Play();

// Control playback
player->Pause();
player->Stop();
player->Volume = 0.8f;
player->PlaybackRate = 1.5f;
```

---

## Project Architecture

### Solution Structure

```
CUI.sln
├── CUI/                    # Main framework project (Windows 8+)
│   ├── CUI/CUI.vcxproj    # Core project configuration
│   ├── GUI/               # Framework source code
│   │   ├── Controls/      # Control implementations
│   │   ├── Layout/        # Layout system
│   │   └── ...
│   └── nanosvg.cpp/h      # SVG rendering library
├── CUI_Legacy/            # Legacy version (Windows 7)
├── CuiDesigner/           # Visual designer
├── CUITest/               # Samples and test program
├── CUITest_Legacy/        # Legacy version samples
└── CppUtils/              # Utility library dependencies
    ├── Graphics/          # Graphics utility library
    └── Utils/             # General utility library
```

### Core Class Hierarchy

```
Control (Base Class)
├── Form                    # Top-level window
├── Button                  # Button
├── Label                   # Label
├── TextBox                 # Text box
├── ComboBox                # Dropdown box
├── CheckBox/RadioBox       # Selection controls
├── Slider                  # Slider
├── ProgressBar             # Progress bar
├── PictureBox              # Picture box
├── Panel                   # Panel
├── TabControl              # Tab control
├── TreeView                # Tree view
├── GridView                # Data grid
├── Menu/ToolBar/StatusBar  # Menu bar
├── WebBrowser              # Web browser
├── MediaPlayer             # Media player
└── NotifyIcon              # Tray icon
```

### Layout Container Hierarchy

```
Control
├── StackPanel              # Stack layout
├── GridPanel               # Grid layout
├── DockPanel               # Dock layout
├── WrapPanel               # Wrap layout
└── RelativePanel           # Relative layout
```

---

## Quick Start

### Environment Requirements

- **Operating System**: Windows 8 / Windows 10 / Windows 11
- **Development Tools**: Visual Studio 2022 or higher
- **C++ Standard**: C++20
- **Dependencies**: WebView2 NuGet package

### Building the Project

1. Open `CUI.sln` with Visual Studio
2. Select Release|x64 configuration
3. Build the solution

### Creating Your First Application

```cpp
#include "CUI/GUI/Form.h"

using namespace CUI;

class MainWindow : public Form
{
public:
    MainWindow() : Form(L"My First CUI App", { 100, 100 }, { 800, 600 })
    {
        // Set window background color
        BackColor = Colors::DarkGray;
        
        // Add label
        auto label = AddControl(new Label(L"Welcome to CUI!", 50, 50));
        label->Font = new Font(L"Microsoft YaHei", 24.0f);
        label->ForeColor = Colors::White;
        
        // Add button
        auto button = AddControl(new Button(L"Click Me", 50, 100, 120, 40));
        button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
            MessageBoxW(sender->GetForm()->Handle, 
                       L"You clicked the button!", L"CUI", MB_OK);
        };
        
        // Add picture box
        auto picture = AddControl(new PictureBox(50, 160, 200, 150));
        picture->SizeMode = ImageSizeMode::StretchIamge;
        
        // Status bar
        auto statusBar = AddControl(new StatusBar(0, 0, 800, 26));
        statusBar->AddPart(L"Ready", -1);
        statusBar->AddPart(L"CUI Demo", 100);
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    Application::Initialize(hInstance);
    
    auto window = new MainWindow();
    window->Show();
    
    return Application::Run();
}
```

### Running Result

The above code will create a window with the following elements:
- Title bar with minimize, maximize, close buttons
- "Welcome to CUI!" label centered
- Button that shows a message box when clicked
- Picture display area supporting drag-and-drop
- Status bar showing "Ready" and "CUI Demo"

---

## Controls Guide

### Basic Controls

#### Label

Used to display static text:

```cpp
// Create label
auto label = AddControl(new Label(L"This is a label", 20, 20));

// Set style
label->Font = new Font(L"Microsoft YaHei", 16.0f);
label->ForeColor = D2D1::ColorF(Colors::LightBlue);
```

#### Button

Standard button control:

```cpp
auto button = AddControl(new Button(L"OK", 20, 50, 100, 30));

button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // Handle click event
    sender->Text = L"Clicked";
    sender->PostRender();
};
```

Button with icon:

```cpp
auto iconButton = AddControl(new Button(L"", 20, 90, 40, 40));
iconButton->Image = myBitmap;
iconButton->SizeMode = ImageSizeMode::CenterImage;
iconButton->BackColor = D2D1::ColorF(0, 0, 0, 0);
```

#### TextBox

Single-line text input:

```cpp
auto textBox = AddControl(new TextBox(L"Default text", 20, 120, 200, 26));

textBox->OnTextChanged += [](Control* s, 
                             const std::wstring& oldText,
                             const std::wstring& newText) {
    // Text change handling
};
```

#### PasswordBox

Masked password input:

```cpp
auto password = AddControl(new PasswordBox(L"", 20, 150, 200, 26));
```

#### RichTextBox

Multi-line text with drag-drop support:

```cpp
auto richText = AddControl(new RichTextBox(L"Multi-line text area\n", 20, 180, 400, 150));
richText->AllowMultiLine = true;
richText->ScrollToEnd();

richText->OnDropText += [](Control* sender, std::wstring text) {
    auto rtb = (RichTextBox*)sender;
    rtb->AppendText(text);
    rtb->ScrollToEnd();
};
```

#### ComboBox

```cpp
auto combo = AddControl(new ComboBox(L"Please select", 20, 340, 180, 28));
combo->ExpandCount = 8;

// Add options
for (int i = 0; i < 20; i++) {
    combo->Items.Add(StringHelper::Format(L"Option %d", i));
}

combo->OnSelectionChanged += [](Control* sender) {
    auto cb = (ComboBox*)sender;
    // Handle selection change
};
```

#### CheckBox

```cpp
auto check = AddControl(new CheckBox(L"I agree to terms", 20, 380));
check->Checked = true;

check->OnChecked += [](Control* sender) {
    auto cb = (CheckBox*)sender;
    if (cb->Checked) {
        // Checked
    }
};
```

#### RadioBox

```cpp
auto radio1 = AddControl(new RadioBox(L"Option A", 20, 410));
auto radio2 = AddControl(new RadioBox(L"Option B", 100, 410));

radio1->Checked = true;

radio1->OnChecked += radio2->OnChecked += [](Control* sender) {
    // Ensure radio behavior
    if (sender->Checked) {
        // Handle selection
    }
};
```

#### Switch

```cpp
auto switchControl = AddControl(new Switch(20, 440));
switchControl->Checked = true;

switchControl->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    auto sw = (Switch*)sender;
    // Toggle handling
};
```

#### Slider

```cpp
auto slider = AddControl(new Slider(20, 480, 200, 30));
slider->Min = 0;
slider->Max = 100;
slider->Value = 50;

slider->OnValueChanged += [](Control* sender, float oldValue, float newValue) {
    // Slider value change handling
};
```

#### ProgressBar

```cpp
auto progress = AddControl(new ProgressBar(20, 520, 200, 24));
progress->PercentageValue = 0.5f;  // 50%
```

### Container Controls

#### Panel

```cpp
auto panel = AddControl(new Panel(300, 20, 400, 300));
panel->BackColor = D2D1::ColorF(1, 1, 1, 0.1f);
panel->BolderColor = D2D1::ColorF(1, 1, 1, 0.2f);

// Add child controls to panel
panel->AddControl(new Label(L"Label in panel", 10, 10));
```

#### TabControl

```cpp
auto tabControl = AddControl(new TabControl(20, 20, 600, 400));

// Add tab pages
auto page1 = tabControl->AddPage(L"Page 1");
auto page2 = tabControl->AddPage(L"Page 2");

// Add content to pages
page1->AddControl(new Label(L"Page 1 content", 10, 10));
page2->AddControl(new Label(L"Page 2 content", 10, 10));
```

### Data Display Controls

#### TreeView

```cpp
auto tree = AddControl(new TreeView(20, 20, 250, 400));

// Add root node
auto root = new TreeNode(L"Root", nullptr);
tree->Root->Children.push_back(root);

// Add child nodes
for (int i = 0; i < 5; i++) {
    auto child = new TreeNode(StringHelper::Format(L"Child %d", i), nullptr);
    root->Children.push_back(child);
}
```

#### GridView

```cpp
auto grid = AddControl(new GridView(290, 20, 500, 400));

// Add columns
grid->Columns.Add(GridViewColumn(L"Name", 150, ColumnType::Text));
grid->Columns.Add(GridViewColumn(L"Type", 100, ColumnType::Text));
grid->Columns.Add(GridViewColumn(L"Action", 80, ColumnType::Button));

// Add row data
for (int i = 0; i < 20; i++) {
    GridViewRow row;
    row.Cells = { L"Item " + std::to_wstring(i), L"Type A", L"" };
    grid->Rows.Add(row);
}
```

### Media Controls

#### PictureBox

```cpp
auto picture = AddControl(new PictureBox(20, 20, 300, 200));
picture->SizeMode = ImageSizeMode::StretchIamge;

// Load image
auto img = BitmapSource::FromFile(L"C:\\image.png");
picture->SetImageEx(graphics->CreateBitmap(img->GetWicBitmap()), false);

// Drag-drop support
picture->OnDropFile += [](Control* sender, List<std::wstring> files) {
    // Handle dropped files
};
```

#### MediaPlayer

```cpp
auto player = AddControl(new MediaPlayer(20, 240, 400, 300));
player->AutoPlay = true;
player->Loop = false;

// Load media
player->Load(L"C:\\video.mp4");

// Event handling
player->OnMediaOpened += [](Control* sender) {
    // Media opened
};

player->OnMediaEnded += [](Control* sender) {
    // Media playback ended
};

player->OnPositionChanged += [](Control* sender, double position) {
    // Playback progress update
};
```

### System Controls

#### Menu

```cpp
auto menu = AddControl(new Menu(0, 0, 800, 28));
menu->BarBackColor = D2D1::ColorF(1, 1, 1, 0.08f);

auto fileMenu = menu->AddItem(L"File");
fileMenu->AddSubItem(L"Open", 1001);
fileMenu->AddSubItem(L"Save", 1002);
fileMenu->AddSeparator();
fileMenu->AddSubItem(L"Exit", 1003);

auto helpMenu = menu->AddItem(L"Help");
helpMenu->AddSubItem(L"About", 2001);

menu->OnMenuCommand += [](Control* sender, int id) {
    switch (id) {
        case 1001: /* Open handling */ break;
        case 1003: /* Exit handling */ break;
    }
};
```

#### ToolBar

```cpp
auto toolbar = AddControl(new ToolBar(0, 28, 800, 32));

auto btnNew = toolbar->AddToolButton(L"New", 24);
auto btnOpen = toolbar->AddToolButton(L"Open", 24);
auto btnSave = toolbar->AddToolButton(L"Save", 24);

btnNew->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // New handling
};
```

#### StatusBar

```cpp
auto statusBar = AddControl(new StatusBar(0, 550, 800, 26));
statusBar->AddPart(L"Ready", -1);
statusBar->AddPart(L"User: admin", 120);
statusBar->AddPart(L"Encoding: UTF-8", 100);

// Update status bar text
statusBar->SetPartText(0, L"Processing...");
```

#### NotifyIcon

```cpp
auto notify = new NotifyIcon();
notify->InitNotifyIcon(hwnd, 1);
notify->SetIcon(LoadIcon(NULL, IDI_APPLICATION));
notify->SetToolTip(L"My App");

notify->ClearMenu();
notify->AddMenuItem(L"Show Window", 1);
notify->AddMenuSeparator();
notify->AddMenuItem(L"Exit", 3);

notify->OnNotifyIconMenuClick += [](NotifyIcon* sender, int menuId) {
    switch (menuId) {
        case 1: ShowWindow(sender->hWnd, SW_SHOWNORMAL); break;
        case 3: PostMessage(sender->hWnd, WM_CLOSE, 0, 0); break;
    }
};

notify->ShowNotifyIcon();
```

#### Taskbar

```cpp
auto taskbar = new Taskbar(hwnd);

// Set taskbar progress
taskbar->SetValue(current, total);

// Set taskbar thumbnail buttons
taskbar->AddThumbnailButton(/* button config */);
```

### WebBrowser Control

```cpp
auto web = AddControl(new WebBrowser(20, 20, 760, 500));

// Register JS-to-C++ handler
web->RegisterJsInvokeHandler(L"native.log", [](const std::wstring& msg) {
    // Receive log from JS
    return L"logged";
});

// C++ calls JS
web->ExecuteScriptAsync(L"console.log('Hello from C++');");

// Set HTML
web->SetHtml(L"<html><body><h1>Embedded Content</h1></body></html>");

// Navigate to URL
web->Navigate(L"https://www.example.com");
```

---

## Layout System

### StackPanel

Child controls arranged sequentially, supporting horizontal and vertical directions:

```cpp
auto stack = AddControl(new StackPanel(20, 20, 200, 300));
stack->SetOrientation(Orientation::Vertical);
stack->SetSpacing(8);

stack->AddControl(new Button(L"Button 1", 0, 0, 180, 30));
stack->AddControl(new Button(L"Button 2", 0, 0, 180, 30));
stack->AddControl(new Button(L"Button 3", 0, 0, 180, 30));
```

### GridPanel

Flexible grid layout with row/column definitions:

```cpp
auto grid = AddControl(new GridPanel(240, 20, 300, 300));

// Define rows
grid->AddRow(GridLength::Auto());          // Auto height
grid->AddRow(GridLength::Star(1.0f));       // Remaining space
grid->AddRow(GridLength::Pixels(50));       // Fixed height 50px

// Define columns
grid->AddColumn(GridLength::Star(1.0f));    // 50% width
grid->AddColumn(GridLength::Star(1.0f));    // 50% width

// Add controls with positions
auto title = new Label(L"Title", 0, 0);
title->GridRow = 0;
title->GridColumn = 0;
title->GridColumnSpan = 2;
grid->AddControl(title);

auto leftPanel = new Button(L"Left", 0, 0, 80, 80);
leftPanel->GridRow = 1;
leftPanel->GridColumn = 0;
grid->AddControl(leftPanel);

auto rightPanel = new Button(L"Right", 0, 0, 80, 80);
rightPanel->GridRow = 1;
rightPanel->GridColumn = 1;
grid->AddControl(rightPanel);
```

### DockPanel

Child controls docked to container edges:

```cpp
auto dock = AddControl(new DockPanel(560, 20, 300, 300));
dock->SetLastChildFill(true);

// Top dock
auto top = new Label(L"Top Title Bar", 0, 0);
top->Size = SIZE{ 300, 40 };
top->DockPosition = Dock::Top;
dock->AddControl(top);

// Left dock
auto left = new Label(L"Left Sidebar", 0, 0);
left->Size = SIZE{ 60, 200 };
left->DockPosition = Dock::Left;
dock->AddControl(left);

// Bottom dock
auto bottom = new Label(L"Bottom Status", 0, 0);
bottom->Size = SIZE{ 300, 30 };
bottom->DockPosition = Dock::Bottom;
dock->AddControl(bottom);

// Fill remaining space
auto fill = new Label(L"Main Content", 0, 0);
fill->DockPosition = Dock::Fill;
dock->AddControl(fill);
```

### WrapPanel

Child controls automatically wrap:

```cpp
auto wrap = AddControl(new WrapPanel(880, 20, 300, 300));
wrap->SetOrientation(Orientation::Horizontal);
wrap->SetSpacing(8);

for (int i = 1; i <= 15; i++) {
    wrap->AddControl(new Button(StringHelper::Format(L"Button %d", i), 
                                0, 0, 80, 30));
}
```

### RelativePanel

Position child controls through relative constraints:

```cpp
auto relative = AddControl(new RelativePanel(20, 340, 400, 300));

// Center button
auto centerBtn = new Button(L"Center", 0, 0, 100, 40);
RelativeConstraints constraints;
constraints.CenterHorizontal = true;
constraints.CenterVertical = true;
relative->AddControl(centerBtn);
relative->SetConstraints(centerBtn, constraints);

// Top-left label
auto topLeft = new Label(L"Top Left", 0, 0);
RelativeConstraints tlConstraints;
tlConstraints.AlignLeftWithPanel = true;
tlConstraints.AlignTopWithPanel = true;
relative->AddControl(topLeft);
relative->SetConstraints(topLeft, tlConstraints);

// Bottom-right label
auto bottomRight = new Label(L"Bottom Right", 0, 0);
RelativeConstraints brConstraints;
brConstraints.AlignRightWithPanel = true;
brConstraints.AlignBottomWithPanel = true;
relative->AddControl(bottomRight);
relative->SetConstraints(bottomRight, brConstraints);
```

### Combined Layout Usage

```cpp
// Create main panel
auto mainPanel = AddControl(new Panel(20, 20, 760, 500));

// Use GridPanel to split into left/right sections
auto gridPanel = mainPanel->AddControl(new GridPanel(10, 10, 740, 480));
gridPanel->AddColumn(GridLength::Star(1.0f));
gridPanel->AddColumn(GridLength::Pixels(250));

// Left StackPanel
auto leftStack = new StackPanel(0, 0, 300, 400);
leftStack->GridRow = 0;
leftStack->GridColumn = 0;
leftStack->SetOrientation(Orientation::Vertical);
leftStack->SetSpacing(10);
gridPanel->AddControl(leftStack);

// Right WrapPanel
auto rightWrap = new WrapPanel(0, 0, 230, 400);
rightWrap->GridRow = 0;
rightWrap->GridColumn = 1;
rightWrap->SetOrientation(Orientation::Horizontal);
rightWrap->SetSpacing(8);
gridPanel->AddControl(rightWrap);
```

---

## Events and Input

### Mouse Events

```cpp
// Mouse click
control->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // e.Button - mouse button
    // e.X, e.Y - mouse position
    // e.Clicks - click count
};

// Mouse double click
control->OnMouseDoubleClick += [](Control* sender, MouseEventArgs e) {
    // Double click handling
};

// Mouse move
control->OnMouseMove += [](Control* sender, MouseEventArgs e) {
    // Mouse move handling
};

// Mouse wheel
control->OnMouseWheel += [](Control* sender, MouseEventArgs e) {
    // e.Delta - wheel delta
};

// Mouse enter/leave
control->OnMouseEnter += [](Control* sender, MouseEventArgs e) {
    // Mouse enter
};

control->OnMouseLeave += [](Control* sender, MouseEventArgs e) {
    // Mouse leave
};
```

### Keyboard Events

```cpp
// Key down
control->OnKeyDown += [](Control* sender, KeyEventArgs e) {
    // e.Key - virtual key code
    // e.Modifiers - modifier key state
};

// Key up
control->OnKeyUp += [](Control* sender, KeyEventArgs e) {
    // Key up handling
};

// Character input
control->OnCharInput += [](Control* sender, wchar_t ch) {
    // Character input handling
};
```

### Focus Events

```cpp
control->OnGotFocus += [](Control* sender) {
    // Gain focus
};

control->OnLostFocus += [](Control* sender) {
    // Lose focus
};
```

### Drag and Drop Events

```cpp
// Accept drag-drop
control->AllowDrop = true;

// File drop
control->OnDropFile += [](Control* sender, List<std::wstring> files) {
    for (const auto& file : files) {
        // Handle dropped files
    }
};

// Text drop
control->OnDropText += [](Control* sender, std::wstring text) {
    // Handle dropped text
};
```

### Window Events

```cpp
// Window size change
form->OnSizeChanged += [](Form* sender) {
    // Adjust layout
};

// Window move
form->OnMoved += [](Form* sender) {
    // Window move handling
};

// Window close
form->OnClosing += [](Form* sender) {
    // Ask to close
    // Set sender->Cancel = true to cancel close
};

form->OnClosed += [](Form* sender) {
    // Window closed, cleanup resources
};
```

### Combined Event Handling

```cpp
// Using lambda expressions to capture context
class MyWindow : public Form
{
private:
    Label* _statusLabel;
    int _clickCount = 0;

public:
    MyWindow() : Form(L"Event Demo", { 100, 100 }, { 600, 400 })
    {
        auto button = AddControl(new Button(L"Click Count", 50, 50, 120, 40));
        
        _statusLabel = AddControl(new Label(L"Clicks: 0", 50, 100));
        
        // Capture this pointer
        button->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
            (void)sender;
            (void)e;
            _clickCount++;
            _statusLabel->Text = StringHelper::Format(L"Clicks: %d", _clickCount);
            _statusLabel->PostRender();
        };
    }
};
```

---

## Advanced Features

### Custom Controls

Create custom versions by inheriting existing controls:

```cpp
// CustomControls.h
#pragma once
#include "CUI/GUI/Button.h"
#include "CUI/GUI/TextBox.h"

class CustomLabel1 : public Label
{
public:
    CustomLabel1(const std::wstring& text, int x, int y) : Label(text, x, y) {}
    
    void OnPaint(Graphics* g) override
    {
        // Gradient drawing
        auto gradient = g->CreateLinearGradientBrush(
            D2D1::Point2F(0, 0),
            D2D1::Point2F(Width, 0),
            { {0, D2D1::ColorF(Colors::Cyan)}, {1, D2D1::ColorF(Colors::Blue)} }
        );
        
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->FillRectangle(rect, gradient);
        
        // Draw text
        DrawText(g, Text.c_str(), rect, GetTextFormat(), ForeColor);
    }
};

class CustomTextBox1 : public TextBox
{
public:
    CustomTextBox1(const std::wstring& text, int x, int y, int w, int h) 
        : TextBox(text, x, y, w, h) {}
    
    void OnPaint(Graphics* g) override
    {
        // Custom border drawing
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->DrawRoundedRectangle(rect, 8.0f, D2D1::ColorF(Colors::Gold));
        
        // Internal fill
        g->FillRectangle(
            D2D1::RoundedRect(D2D1::RectF(1, 1, Width - 1, Height - 1), 7, 7),
            BackColor
        );
        
        // Draw text
        DrawText(g, Text.c_str(), 
                 D2D1::RectF(5, 5, Width - 5, Height - 5),
                 GetTextFormat(), ForeColor);
    }
};
```

### DirectComposition Integration

```cpp
// Use DCompLayeredHost for layered rendering
auto dcompHost = new DCompLayeredHost(hwnd);
dcompHost->Initialize();

// Create composition visual effects
IDCompositionVisual* visual = nullptr;
dcompDevice->CreateVisual(&visual);
dcompSurface = // Get Direct3D surface
visual->SetContent(dcompSurface);
dcompTarget->AddVisual(visual);
```

### Resource Management

```cpp
// Font resource management
Font* font = new Font(L"Microsoft YaHei", 16.0f);
control->Font = font;  // Control takes ownership

// Image resource management
ID2D1Bitmap* bitmap = graphics->CreateBitmap(image);
pictureBox->Image = bitmap;  // Control takes ownership

// Custom resource release
void CleanupResources()
{
    for (auto& b : _bitmaps) {
        if (b) {
            b->Release();
            b = nullptr;
        }
    }
}
```

### Internationalization

```cpp
// Enable IME input
textBox->ImeMode = ImeMode::On;

// Set IME mode
textBox->ImeMode = ImeMode::AlphaNumeric;

// Get IME status
auto imeStatus = textBox->GetImeStatus();
```

---

## Visual Designer

### Starting the Designer

```
CuiDesigner.exe
```

### Designer Features

1. **Drag-and-Drop Interface Design**
   - Drag controls from toolbox to canvas
   - Real-time preview of interface effects
   - Adjust control position and size

2. **Property Editing**
   - Modify control properties in property grid
   - Support property grouping and search
   - Real-time property application

3. **Layout Container Support**
   - StackPanel, GridPanel, DockPanel
   - WrapPanel, RelativePanel
   - Row/column definition editor

4. **Design File Management**
   - JSON format for saving design files
   - Automatic C++ code generation
   - Support for design file version control

### Creating Interface with Designer

1. **Create New Project**
   - Launch CuiDesigner.exe
   - Select "New Project"

2. **Add Controls**
   - Select control type from toolbox
   - Drag to design canvas
   - Adjust position and size

3. **Set Properties**
   - Select control
   - Edit properties in property grid
   - Real-time preview

4. **Use Layout Containers**
   - Add layout container from toolbox
   - Set container properties
   - Add child controls to container

5. **Save and Generate Code**
   - Save JSON design file
   - Generate C++ code
   - Integrate code into project

### Generated Code Example

C++ code structure generated by designer:

```cpp
// Form1.h - Auto-generated interface definition
#pragma once
#include "Form.h"

class Form1 : public Form
{
public:
    Form1() : Form(L"Form1", { 100, 100 }, { 800, 600 })
    {
        InitializeComponents();
    }

private:
    void InitializeComponents()
    {
        // Label
        auto label1 = AddControl(new Label(L"Welcome to CUI", 50, 50));
        label1->Font = new Font(L"Microsoft YaHei", 24.0f);
        
        // Button
        auto button1 = AddControl(new Button(L"OK", 50, 100, 100, 36));
        button1->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            // Event handling
        };
        
        // TextBox
        auto textBox1 = AddControl(new TextBox(L"", 50, 150, 200, 28));
        
        // Layout container
        auto stackPanel = AddControl(new StackPanel(300, 50, 200, 300));
        stackPanel->SetOrientation(Orientation::Vertical);
        stackPanel->AddControl(new Button(L"Button1", 0, 0, 180, 30));
        stackPanel->AddControl(new Button(L"Button2", 0, 0, 180, 30));
    }
};
```

---

## Code Examples

### Complete Example: Media Player

```cpp
#include "Form.h"
#include "MediaPlayer.h"
#include "Button.h"
#include "Slider.h"
#include "Label.h"
#include "OpenFileDialog.h"

using namespace CUI;

class MediaPlayerWindow : public Form
{
private:
    MediaPlayer* _player;
    Label* _timeLabel;
    Slider* _progressSlider;
    Slider* _volumeSlider;
    bool _progressUpdating = false;

public:
    MediaPlayerWindow() : Form(L"Media Player", { 100, 100 }, { 900, 600 })
    {
        // Create media player
        _player = AddControl(new MediaPlayer(10, 40, 880, 450));
        _player->Margin = Thickness(10, 40, 10, 120);
        _player->AutoPlay = false;
        _player->Loop = false;

        // Media events
        _player->OnMediaOpened += [this](Control* sender) {
            auto player = (MediaPlayer*)sender;
            int total = (int)player->Duration;
            _timeLabel->Text = StringHelper::Format(L"00:00 / %02d:%02d", 
                                                     total / 60, total % 60);
            _progressSlider->Max = 1000;
            _progressSlider->Value = 0;
        };

        _player->OnPositionChanged += [this](Control* sender, double position) {
            if (_progressUpdating) return;
            auto player = (MediaPlayer*)sender;
            if (player->Duration > 0) {
                int cur = (int)position;
                int total = (int)player->Duration;
                _timeLabel->Text = StringHelper::Format(
                    L"%02d:%02d / %02d:%02d", 
                    cur / 60, cur % 60, total / 60, total % 60
                );
                _progressUpdating = true;
                _progressSlider->Value = (float)(position / player->Duration * 1000.0);
                _progressUpdating = false;
            }
        };

        _player->OnMediaEnded += [this](Control* sender) {
            (void)sender;
            _timeLabel->Text = L"Playback ended";
        };

        // Control panel
        auto controlPanel = AddControl(new Panel(10, 500, 880, 80));
        controlPanel->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        // Open button
        auto btnOpen = controlPanel->AddControl(new Button(L"Open", 10, 20, 80, 36));
        btnOpen->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            OpenFileDialog ofd;
            ofd.Filter = MakeDialogFilterString("Media Files", 
                                                "*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.mp3");
            ofd.Title = "Select Media File";
            if (ofd.ShowDialog(this->Handle) == DialogResult::OK && 
                !ofd.SelectedPaths.empty()) {
                _player->Load(ofd.SelectedPaths[0]);
                _player->Play();
            }
        };

        // Playback control buttons
        auto btnPlay = controlPanel->AddControl(new Button(L"Play", 100, 20, 70, 36));
        btnPlay->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Play();
        };

        auto btnPause = controlPanel->AddControl(new Button(L"Pause", 180, 20, 70, 36));
        btnPause->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Pause();
        };

        auto btnStop = controlPanel->AddControl(new Button(L"Stop", 260, 20, 70, 36));
        btnStop->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Stop();
        };

        // Volume control
        controlPanel->AddControl(new Label(L"Volume", 340, 26));
        _volumeSlider = controlPanel->AddControl(new Slider(390, 22, 100, 30));
        _volumeSlider->Min = 0;
        _volumeSlider->Max = 100;
        _volumeSlider->Value = 80;
        _volumeSlider->OnValueChanged += [this](Control* s, float old, float ne) {
            (void)s;
            (void)old;
            _player->Volume = ne / 100.0;
        };
        _player->Volume = 0.8;

        // Progress bar
        _progressSlider = controlPanel->AddControl(new Slider(500, 22, 300, 30));
        _progressSlider->OnValueChanged += [this](Control* s, float old, float ne) {
            (void)s;
            (void)old;
            if (_progressUpdating) return;
            if (_player->Duration > 0) {
                _player->Position = (ne / 1000.0) * _player->Duration;
            }
        };

        // Time label
        _timeLabel = controlPanel->AddControl(new Label(L"00:00 / 00:00", 810, 26));
        _timeLabel->ForeColor = Colors::LightGray;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    Application::Initialize(hInstance);
    
    auto window = new MediaPlayerWindow();
    window->Show();
    
    return Application::Run();
}
```

### Web Integration Example

```cpp
#include "Form.h"
#include "WebBrowser.h"

using namespace CUI;

class WebIntegrationWindow : public Form
{
private:
    WebBrowser* _web;

public:
    WebIntegrationWindow() : Form(L"Web Integration Demo", { 100, 100 }, { 1000, 700 })
    {
        _web = AddControl(new WebBrowser(10, 10, 980, 600));
        _web->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                             AnchorStyles::Right | AnchorStyles::Bottom;

        // Register JS-to-C++ handlers
        _web->RegisterJsInvokeHandler(L"native.getTime", 
            [](const std::wstring& payload) {
                (void)payload;
                SYSTEMTIME st;
                GetLocalTime(&st);
                return StringHelper::Format(
                    L"%04d-%02d-%02d %02d:%02d:%02d",
                    st.wYear, st.wMonth, st.wDay, 
                    st.wHour, st.wMinute, st.wSecond
                );
            });

        _web->RegisterJsInvokeHandler(L"native.calculate", 
            [](const std::wstring& payload) {
                // Simple calculation example
                return L"Result: " + payload;
            });

        // Generate HTML
        std::wstring html =
            L"<!DOCTYPE html>"
            L"<html><head><meta charset='utf-8'/>"
            L"<style>"
            L"body{font-family:Segoe UI,Arial; padding:20px; background:#1e1e1e; color:#fff;}"
            L"button{padding:10px 20px; margin:5px; background:#0078d4; color:#fff; border:none; border-radius:4px; cursor:pointer;}"
            L"button:hover{background:#1084d8;}"
            L".output{padding:15px; background:#2d2d2d; border-radius:4px; margin-top:15px;}"
            L"input{padding:10px; border-radius:4px; border:1px solid #444; background:#333; color:#fff;}"
            L"</style></head><body>"
            L"<h2>CUI WebBrowser Interop Demo</h2>"
            L"<div>"
            L"<button id='btnTime'>Get Server Time</button>"
            L"<button id='btnCalc'>Calculate 2+2</button>"
            L"</div>"
            L"<div class='output'>JS to C++ Result: <span id='jsResult'>(none)</span></div>"
            L"<div class='output'>C++ to JS: <span id='nativeResult'>(waiting)</span></div>"
            L"<script>"
            L"// JS to C++"
            L"document.getElementById('btnTime').onclick = async function() {"
            L"  try {"
            L"    const r = await window.CUI.invoke('native.getTime', '');"
            L"    document.getElementById('jsResult').textContent = r;"
            L"  } catch(e) {"
            L"    document.getElementById('jsResult').textContent = 'Error: ' + e;"
            L"  }"
            L"};"
            L""
            L"document.getElementById('btnCalc').onclick = async function() {"
            L"  try {"
            L"    const r = await window.CUI.invoke('native.calculate', '2+2');"
            L"    document.getElementById('jsResult').textContent = r;"
            L"  } catch(e) {"
            L"    document.getElementById('jsResult').textContent = 'Error: ' + e;"
            L"  }"
            L"};"
            L""
            L"// C++ to JS callback function"
            L"window.setFromNative = function(text) {"
            L"  document.getElementById('nativeResult').textContent = text;"
            L"  return 'ok';"
            L"};"
            L"</script></body></html>";

        _web->SetHtml(html);

        // Add button to demonstrate C++ calling JS
        auto btnCallJs = AddControl(new Button(L"C++ Call JS", 10, 630, 150, 36));
        btnCallJs->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            SYSTEMTIME st;
            GetLocalTime(&st);
            std::wstring msg = StringHelper::Format(
                L"Hello from C++ - %02d:%02d:%02d",
                st.wHour, st.wMinute, st.wSecond
            );
            _web->ExecuteScriptAsync(
                L"window.setFromNative(\"" + msg + L"\");"
            );
        };
    }
};
```

### Complete Example: Data Management System

```cpp
#include "Form.h"
#include "GridView.h"
#include "TreeView.h"
#include "Button.h"
#include "TextBox.h"
#include "Panel.h"

using namespace CUI;

class DataManagerWindow : public Form
{
private:
    GridView* _dataGrid;
    TreeView* _categoryTree;
    TextBox* _searchBox;
    std::vector<GridViewRow> _allData;

public:
    DataManagerWindow() : Form(L"Data Management System", { 100, 100 }, { 1200, 800 })
    {
        BackColor = Colors::grey31;

        // Top search bar
        auto searchPanel = AddControl(new Panel(10, 10, 1180, 50));
        searchPanel->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        searchPanel->AddControl(new Label(L"Search:", 15, 15));
        _searchBox = searchPanel->AddControl(new TextBox(L"", 60, 14, 300, 28));
        _searchBox->OnTextChanged += [this](Control* s, 
                                            const std::wstring& oldText,
                                            const std::wstring& newText) {
            (void)s;
            (void)oldText;
            FilterData(newText);
        };

        auto btnAdd = searchPanel->AddControl(new Button(L"Add", 380, 12, 80, 32));
        auto btnEdit = searchPanel->AddControl(new Button(L"Edit", 470, 12, 80, 32));
        auto btnDelete = searchPanel->AddControl(new Button(L"Delete", 560, 12, 80, 32));
        auto btnRefresh = searchPanel->AddControl(new Button(L"Refresh", 650, 12, 80, 32));

        // Left category tree
        _categoryTree = AddControl(new TreeView(10, 70, 250, 650));
        _categoryTree->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                                       AnchorStyles::Bottom;
        _categoryTree->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        // Initialize categories
        auto root = new TreeNode(L"All Categories", nullptr);
        _categoryTree->Root->Children.push_back(root);

        auto category1 = new TreeNode(L"Electronics", nullptr);
        category1->Expand = true;
        root->Children.push_back(category1);
        for (int i = 1; i <= 3; i++) {
            category1->Children.push_back(
                new TreeNode(StringHelper::Format(L"Subcategory %d", i), nullptr)
            );
        }

        root->Children.push_back(new TreeNode(L"Office Supplies", nullptr));
        root->Children.push_back(new TreeNode(L"Home Goods", nullptr));
        root->Children.push_back(new TreeNode(L"Food & Beverages", nullptr));

        // Right data grid
        _dataGrid = AddControl(new GridView(280, 70, 910, 650));
        _dataGrid->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                                   AnchorStyles::Right | AnchorStyles::Bottom;
        _dataGrid->BackColor = D2D1::ColorF(0, 0, 0, 0);
        _dataGrid->AllowUserToAddRows = false;

        // Set columns
        _dataGrid->Columns.Add(GridViewColumn(L"ID", 60, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"Name", 200, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"Category", 120, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"Price", 100, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"Stock", 80, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"Status", 100, ColumnType::ComboBox));
        
        // Set column combo options
        auto statusCol = _dataGrid->Columns[5];
        statusCol.ComboBoxItems = { L"In Stock", L"Out of Stock", L"Discontinued" };

        // Add sample data
        GenerateSampleData();
        RefreshGrid();

        // Bottom status bar
        auto statusBar = AddControl(new StatusBar(0, 740, 1200, 30));
        statusBar->AddPart(L"Ready", -1);
        statusBar->AddPart(L"Total " + std::to_wstring(_allData.size()) + " records", 200);
    }

private:
    void GenerateSampleData()
    {
        const wchar_t* categories[] = { L"Electronics", L"Office Supplies", L"Home Goods", L"Food & Beverages" };
        const wchar_t* names[][4] = {
            { L"Smartphone", L"Laptop", L"Tablet", L"Smart Watch" },
            { L"Printer", L"Scanner", L"Shredder", L"Projector" },
            { L"Sofa", L"Dining Table", L"Bookshelf", L"Bed" },
            { L"Coffee", L"Tea", L"Snacks", L"Beverages" }
        };

        for (int cat = 0; cat < 4; cat++) {
            for (int i = 0; i < 8; i++) {
                GridViewRow row;
                row.Cells = {
                    std::to_wstring(cat * 8 + i + 1),
                    names[cat][i % 4],
                    categories[cat],
                    std::to_wstring(100 + (cat * 100) + (i * 10)),
                    std::to_wstring(10 + (i * 5)),
                    i % 2 == 0 ? L"In Stock" : (i % 3 == 0 ? L"Out of Stock" : L"Discontinued")
                };
                _allData.push_back(row);
            }
        }
    }

    void RefreshGrid()
    {
        _dataGrid->Rows.Clear();
        for (const auto& row : _allData) {
            _dataGrid->Rows.Add(row);
        }
    }

    void FilterData(const std::wstring& keyword)
    {
        _dataGrid->Rows.Clear();
        for (const auto& row : _allData) {
            bool match = false;
            for (const auto& cell : row.Cells) {
                if (StringHelper::Contains(cell, keyword)) {
                    match = true;
                    break;
                }
            }
            if (match || keyword.empty()) {
                _dataGrid->Rows.Add(row);
            }
        }
    }
};
```

---

## FAQ

### Q1: Which Windows versions does CUI support?

**A**: The main CUI version supports Windows 8 and above. If you need Windows 7 support, please use the `CUI_Legacy` project (WebBrowser not included).

### Q2: How to handle high DPI displays?

**A**: CUI supports high DPI rendering by default. Windows scale windows automatically based on system DPI settings. For finer control, use `SetDpiAwareness()` function.

### Q3: How to debug rendering issues?

**A**:
1. Enable debug output: `graphics->SetDebugMode(true)`
2. Check Direct2D factory status
3. Verify resource creation success
4. Use Visual Studio's Graphics Diagnostics tool

### Q4: How to handle navigation errors in WebBrowser?

**A**: Use `NavigationCompleted` and `NavigationStarting` events:

```cpp
_web->OnNavigationStarting += [](Control* sender, NavigationStartingArgs& args) {
    // Can cancel navigation
    // args.Cancel = true;
};

_web->OnNavigationCompleted += [](Control* sender, NavigationCompletedArgs args) {
    if (!args.IsSuccess) {
        // Handle navigation error
        // args.WebErrorStatus contains error type
    }
};
```

### Q5: How to implement custom drawing?

**A**: Inherit control and override `OnPaint` method:

```cpp
class CustomControl : public Control
{
public:
    void OnPaint(Graphics* g) override
    {
        // Custom drawing logic
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->FillRectangle(rect, BackColor);
        // ... more drawing
    }
};
```

### Q6: How to share data between different controls?

**A**: Several ways:
1. Share data pointer through parent window
2. Use observer pattern publish-subscribe
3. Use static variables (use with caution)
4. Use external data model class

### Q7: What are CUI's advantages compared to MFC/WTL?

**A**:
- Hardware-accelerated rendering, higher performance
- Modern C++ API, easier to use
- Complete visual designer
- Built-in WebView2 integration
- Better high DPI support
- Richer control library

### Q8: How to contribute code?

**A**:
1. Fork the project repository
2. Create feature branch
3. Submit changes
4. Create Pull Request
5. Join QQ group 522222570 for discussion

---

## Dependencies and Requirements

### System Requirements

| Component | Requirement |
|-----------|-------------|
| Operating System | Windows 8 / 10 / 11 |
| Processor | x64 architecture |
| Memory | 4GB+ recommended |
| Graphics | Direct2D support |

### Development Environment

| Component | Version Requirement |
|-----------|-------------------|
| Visual Studio | 2022 or higher |
| Windows SDK | 10.0.17763.0 or higher |
| C++ Standard | C++20 |
| WebView2 SDK | 1.0.x or higher |

### NuGet Dependencies

```
Microsoft.Web.WebView2
```

### CppUtils Dependencies

CUI depends on the `CppUtils/Graphics` library, with source code included in the `CppUtils/` directory:

- Graphics rendering base classes
- Font management
- Color system
- Bitmap processing
- Utility functions

### Third-Party Components

| Component | Purpose | License |
|-----------|---------|---------|
| nanosvg | SVG rendering | MIT |
| WebView2 | Web content embedding | Microsoft EULA |

---

## Community and Support

### Community

- **QQ Group**: 522222570
- **GitHub Issues**: Report bugs and feature requests

### License

This project is open source under the **AFL 3.0** (Academic Free License v3.0) license.

### Acknowledgments

Thanks to all contributors and community members for your support!

---

## Appendix

### A. Control Properties Quick Reference

| Control | Core Properties | Core Events |
|---------|----------------|-------------|
| Button | Text, Image, SizeMode | OnMouseClick |
| Label | Text, Font, ForeColor | - |
| TextBox | Text, MaxLength, ReadOnly | OnTextChanged, OnCharInput |
| ComboBox | Text, Items, SelectedIndex | OnSelectionChanged |
| CheckBox | Checked, Text | OnChecked |
| Slider | Value, Min, Max | OnValueChanged |
| GridView | Columns, Rows, Selection | OnSelectionChanged |
| TreeView | Root, SelectedNode | OnSelectionChanged |
| WebBrowser | Url, Html | OnNavigationCompleted |
| MediaPlayer | Volume, Position, Duration | OnMediaOpened, OnPositionChanged |

### B. Layout Properties Quick Reference

| Property | Description | Applicable Controls |
|----------|-------------|-------------------|
| Margin | Outer margin | All controls |
| Padding | Inner margin | Container controls |
| DockPosition | Dock position | DockPanel |
| GridRow/GridColumn | Grid position | GridPanel |
| AnchorStyles | Anchor styles | All controls |
| HAlign/VAlign | Alignment | Layout containers |
| RelativeConstraints | Relative constraints | RelativePanel |

### C. Color Constants

```cpp
namespace Colors {
    // Basic colors
    ColorF White;
    ColorF Black;
    ColorF Red;
    ColorF Green;
    ColorF Blue;
    
    // System colors
    ColorF LightGray;
    ColorF DarkGray;
    ColorF Gray;
    ColorF Silver;
    
    // Extended colors
    ColorF Transparent;
    ColorF Accent;
    ColorF Error;
    ColorF Warning;
    ColorF Success;
}
```

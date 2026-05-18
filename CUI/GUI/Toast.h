#pragma once
#include "Control.h"

enum class ToastKind
{
	Info,
	Success,
	Warning,
	Error
};

class ToastItem
{
public:
	std::wstring Title;
	std::wstring Message;
	std::wstring ActionText;
	ToastKind Kind = ToastKind::Info;
	UINT DurationMs = 3200;
	UINT64 CreatedTick = 0;
	UINT64 Tag = 0;
	bool Persistent = false;
	bool Dismissing = false;
	UINT64 DismissStartTick = 0;
	UINT DismissDurationMs = 180;

	ToastItem() = default;
	ToastItem(std::wstring title, std::wstring message, ToastKind kind = ToastKind::Info, UINT durationMs = 3200);
};

typedef Event<void(class ToastHost*, int index)> ToastHostEvent;

class ToastHost : public Control
{
public:
	UIClass Type() override;
	ToastHost(int x = 0, int y = 0, int width = 340, int height = 260);

	std::vector<ToastItem> Toasts;

	float ToastWidth = 320.0f;
	float ToastHeight = 88.0f;
	float Gap = 10.0f;
	float Padding = 10.0f;
	float CornerRadius = 8.0f;
	float AccentWidth = 4.0f;
	float AccentInsetY = 10.0f;
	UINT DefaultDurationMs = 3200;
	int MaxVisible = 4;
	bool NewestOnTop = true;
	bool ShowCloseButton = true;

	D2D1_COLOR_F ToastBackColor = D2D1_COLOR_F{ 0.98f, 0.985f, 0.995f, 0.96f };
	D2D1_COLOR_F ToastBorderColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.35f };
	D2D1_COLOR_F TitleColor = D2D1_COLOR_F{ 0.08f, 0.10f, 0.14f, 1.0f };
	D2D1_COLOR_F MessageColor = D2D1_COLOR_F{ 0.32f, 0.35f, 0.42f, 1.0f };
	D2D1_COLOR_F CloseHoverColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 0.16f };
	D2D1_COLOR_F InfoColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 1.0f };
	D2D1_COLOR_F SuccessColor = D2D1_COLOR_F{ 0.10f, 0.68f, 0.48f, 1.0f };
	D2D1_COLOR_F WarningColor = D2D1_COLOR_F{ 0.95f, 0.62f, 0.18f, 1.0f };
	D2D1_COLOR_F ErrorColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };

	int HoveredIndex = -1;
	int PressedCloseIndex = -1;

	ToastHostEvent OnToastClick;
	ToastHostEvent OnToastDismissed;

	int ShowToast(const std::wstring& title, const std::wstring& message,
		ToastKind kind = ToastKind::Info, UINT durationMs = 0);
	bool DismissToast(int index);
	void ClearToasts();
	size_t ToastCount() const;

	CursorKind QueryCursor(int xof, int yof) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 33; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	D2D1_COLOR_F KindColor(ToastKind kind) const;
	D2D1_RECT_F GetToastRect(int visibleOrdinal) const;
	D2D1_RECT_F GetCloseRect(const D2D1_RECT_F& toastRect) const;
	std::vector<int> VisibleIndices() const;
	int HitTestToast(int xof, int yof) const;
	int HitTestClose(int xof, int yof) const;
	bool RemoveExpired();
};

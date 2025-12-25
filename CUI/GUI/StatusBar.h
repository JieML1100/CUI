#pragma once
#include "Panel.h"
#include "Label.h"

class StatusBar : public Panel
{
private:
	struct Part
	{
		Label* LabelCtrl = nullptr;
		int Width = 0;
	};

	std::vector<Part> _parts;
	std::vector<float> _separatorsX;

	Label* _leftLabel = nullptr;
	Label* _rightLabel = nullptr;

	int _partInnerPadding = 6;
	D2D1_COLOR_F _separatorColor = D2D1_COLOR_F{ 1, 1, 1, 0.12f };

	void EnsureDefaultParts();
	void UpdateCompatPointers();

public:
	virtual UIClass Type() override;

	// When true, Form will treat this StatusBar like MainMenu: rendered and hit-tested
	// through a dedicated top-most channel (independent from normal control z-order).
	bool TopMost = true;

	int Padding = 6;
	int Gap = 10;

	StatusBar(int x, int y, int width, int height = 26);

	int AddPart(const std::wstring& text = L"", int width = 0);
	void ClearParts();
	int PartCount() const;
	void SetPartText(int index, const std::wstring& text);
	std::wstring GetPartText(int index) const;
	int GetPartWidth(int index) const;
	void SetPartWidth(int index, int width);

	void SetLeftText(const std::wstring& text);
	void SetRightText(const std::wstring& text);
	std::wstring GetLeftText() const;
	std::wstring GetRightText() const;

	Label* LeftLabel() const { return _leftLabel; }
	Label* RightLabel() const { return _rightLabel; }

	void LayoutItems();
	void Update() override;
};

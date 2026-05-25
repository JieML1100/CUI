#include "TreeView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

static float EffectiveItemHeight(TreeView* tree)
{
	const float fontHeight = (tree && tree->Font) ? tree->Font->FontHeight : 16.0f;
	const float configuredHeight = tree ? tree->ItemHeight : 28.0f;
	return (std::max)(configuredHeight, fontHeight + 8.0f);
}

static D2D1_POINT_2F RotateAround(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = (std::clamp)(progress, 0.0f, 1.0f);
	const float angle = progress * 1.57079632679f;
	const float halfW = size * 0.28f;
	const float halfH = size * 0.46f;
	D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	D2D1_POINT_2F p2 = D2D1::Point2F(cx + halfW, cy);
	D2D1_POINT_2F p3 = D2D1::Point2F(cx - halfW, cy + halfH);
	p1 = RotateAround(p1, cx, cy, angle);
	p2 = RotateAround(p2, cx, cy, angle);
	p3 = RotateAround(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.8f);
	d2d->DrawLine(p2, p3, color, 1.8f);
}

static float measureNodes(std::vector<TreeNode*>& children)
{
	float count = 0.0f;
	for (auto* child : children)
	{
		if (child) count += child->AnimatedVisibleCount();
	}
	return count;
}

static void renderNodes(TreeView* tree, D2DGraphics* d2d, float w, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children)
{
	if (!tree || !d2d) return;
	const float fontHeight = tree->Font ? tree->Font->FontHeight : 16.0f;
	const float rowInsetX = (std::max)(0.0f, tree->ItemHorizontalPadding);
	const float rowInsetY = (std::max)(0.0f, tree->ItemVerticalPadding);
	const float chevronSize = (std::max)(6.0f, tree->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	const float textSpacing = (std::max)(2.0f, tree->TextLeftSpacing);
	for (auto* c : children)
	{
		if (!c) continue;
		const float renderTop = cursorY - scrollOffsetY;
		const float renderBottom = renderTop + itemHeight;
		const float baseLeft = rowInsetX + (sunLevel * (std::max)(8.0f, tree->IndentWidth));

		if (renderBottom >= 0.0f && renderTop < h)
		{
			auto foreColor = (c == tree->SelectedNode) ? tree->SelectedForeColor : tree->ForeColor;
			const float pillRight = (std::max)(rowInsetX + 1.0f, w - rowInsetX - 10.0f);
			const D2D1_RECT_F itemRect = D2D1::RectF(rowInsetX, renderTop + rowInsetY, pillRight, renderBottom - rowInsetY);
			if (c == tree->SelectedNode)
			{
				d2d->FillRoundRect(itemRect, tree->SelectedBackColor, tree->ItemCornerRadius);
				const float accentW = (std::max)(2.0f, tree->SelectedAccentWidth);
				const float accentTop = itemRect.top + 5.0f;
				const float accentH = (std::max)(6.0f, (itemRect.bottom - itemRect.top) - 10.0f);
				d2d->FillRoundRect(itemRect.left, accentTop, accentW, accentH, tree->AccentColor, accentW * 0.5f);
			}
			else if (c == tree->HoveredNode)
			{
				d2d->FillRoundRect(itemRect, tree->UnderMouseItemBackColor, tree->ItemCornerRadius);
			}
			if (c->Children.size() > 0)
			{
				const float chevronCx = baseLeft + chevronSlot * 0.5f;
				const float chevronCy = renderTop + itemHeight * 0.5f;
				DrawChevron(d2d, chevronCx, chevronCy, chevronSize, c->CurrentExpandProgress(), foreColor);
			}

			float contentLeft = baseLeft + chevronSlot + textSpacing;
			if (auto* bmp = c->GetImageBitmap(d2d))
			{
				const float imageSize = (std::max)(12.0f, (std::min)(18.0f, itemHeight - 8.0f));
				const float imageTop = renderTop + (itemHeight - imageSize) * 0.5f;
				d2d->DrawBitmap(bmp, contentLeft, imageTop, imageSize, imageSize);
				contentLeft += imageSize + textSpacing;
			}
			const float textTop = renderTop + (std::max)(0.0f, (itemHeight - fontHeight) * 0.5f);
			d2d->DrawString(c->Text, contentLeft, textTop, foreColor, tree->Font);
		}

		cursorY += itemHeight;

		if (c->Children.size() > 0)
		{
			const float progress = c->CurrentExpandProgress();
			if (progress > 0.001f)
			{
				const float childFullHeight = measureNodes(c->Children) * itemHeight;
				const float childVisibleHeight = childFullHeight * progress;
				if (childVisibleHeight > 0.001f)
				{
					const float clipTop = cursorY - scrollOffsetY;
					const float clipBottom = clipTop + childVisibleHeight;
					if (clipBottom > 0.0f && clipTop < h)
					{
						const float clipY = (std::max)(0.0f, clipTop);
						const float clipH = (std::min)(h, clipBottom) - clipY;
						if (clipH > 0.0f)
						{
							float childCursor = cursorY;
							d2d->PushDrawRect(0.0f, clipY, w, clipH);
							renderNodes(tree, d2d, w, h, itemHeight, scrollOffsetY, childCursor, sunLevel + 1, c->Children);
							d2d->PopDrawRect();
						}
					}
					cursorY += childVisibleHeight;
				}
			}
		}
	}
}

static TreeNode* findNode(TreeView* tree, float posX, float posY, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children, bool& isHitEx)
{
	if (!tree) return nullptr;
	const float rowInsetX = (std::max)(0.0f, tree->ItemHorizontalPadding);
	const float chevronSize = (std::max)(6.0f, tree->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	for (auto* c : children)
	{
		if (!c) continue;
		const float currTop = cursorY - scrollOffsetY;
		const float currBottom = currTop + itemHeight;
		if (currBottom >= 0.0f && currTop < h)
		{
			if (posY >= currTop && posY <= currBottom)
			{
				float exLeft = rowInsetX + (sunLevel * (std::max)(8.0f, tree->IndentWidth));
				if (posX >= (exLeft - 3.0f) && posX <= (exLeft + chevronSlot + 3.0f) && c->Children.size() > 0)
					isHitEx = true;
				else
					isHitEx = false;
				return c;
			}
		}
		cursorY += itemHeight;
		if (c->Children.size() > 0)
		{
			const float progress = c->CurrentExpandProgress();
			if (progress > 0.001f)
			{
				const float childVisibleHeight = measureNodes(c->Children) * itemHeight * progress;
				const float childTop = cursorY - scrollOffsetY;
				const float childBottom = childTop + childVisibleHeight;
				if (posY >= childTop && posY <= childBottom)
				{
					float childCursor = cursorY;
					auto result = findNode(tree, posX, posY, h, itemHeight, scrollOffsetY, childCursor, sunLevel + 1, c->Children, isHitEx);
					if (result)
						return result;
				}
				cursorY += childVisibleHeight;
			}
		}
	}

	return nullptr;
}

TreeNode::TreeNode(std::wstring text, std::shared_ptr<BitmapSource> image)
{
	this->Text = text;
	this->Image = std::move(image);
	this->Expand = false;
	this->ExpandProgress = 0.0f;
	this->Children = std::vector<TreeNode*>();
}

float TreeNode::CurrentExpandProgress()
{
	if (this->Children.size() <= 0)
	{
		this->Animating = false;
		this->ExpandProgress = 0.0f;
		return 0.0f;
	}
	if (!this->Animating)
	{
		this->ExpandProgress = this->Expand ? 1.0f : 0.0f;
		return this->ExpandProgress;
	}
	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= this->AnimStartTick ? (now - this->AnimStartTick) : 0;
	float t = this->AnimDurationMs > 0 ? (float)elapsed / (float)this->AnimDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		this->ExpandProgress = this->AnimTargetProgress;
		this->Animating = false;
		return this->ExpandProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	this->ExpandProgress = this->AnimStartProgress + (this->AnimTargetProgress - this->AnimStartProgress) * t;
	return this->ExpandProgress;
}

void TreeNode::SetExpanded(bool expanded)
{
	const bool wantExpand = expanded && this->Children.size() > 0;
	const float current = CurrentExpandProgress();
	this->Expand = wantExpand;
	this->AnimStartProgress = current;
	this->AnimTargetProgress = wantExpand ? 1.0f : 0.0f;
	if (std::fabs(this->AnimTargetProgress - this->AnimStartProgress) < 0.001f)
	{
		this->ExpandProgress = this->AnimTargetProgress;
		this->Animating = false;
	}
	else
	{
		this->AnimStartTick = ::GetTickCount64();
		this->Animating = true;
	}
}

bool TreeNode::IsAnimationRunning()
{
	CurrentExpandProgress();
	if (this->Animating) return true;
	for (auto* child : this->Children)
	{
		if (child && child->IsAnimationRunning()) return true;
	}
	return false;
}

float TreeNode::AnimatedVisibleCount()
{
	float count = 1.0f;
	if (this->Children.size() > 0)
	{
		float childCount = 0.0f;
		for (auto* child : this->Children)
		{
			if (child) childCount += child->AnimatedVisibleCount();
		}
		count += CurrentExpandProgress() * childCount;
	}
	return count;
}

ID2D1Bitmap* TreeNode::GetImageBitmap(D2DGraphics* render)
{
	if (!render || !Image)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (ImageCache && ImageCacheTarget == target && ImageCacheSource == Image.get())
		return ImageCache.Get();
	ImageCache.Reset();
	ImageCacheTarget = target;
	ImageCacheSource = Image.get();
	auto* bmp = render->CreateBitmap(Image);
	if (!bmp)
		return nullptr;
	ImageCache.Attach(bmp);
	return ImageCache.Get();
}
TreeNode::~TreeNode()
{
	for (auto& c : this->Children)
		delete c;
	this->Children.clear();
}
int TreeNode::UnfoldedCount()
{
	int count = 1;
	for (auto& c : this->Children)
	{
		if (c->Expand)
			count += c->UnfoldedCount();
	}
	return count;
}
UIClass TreeView::Type() { return UIClass::UI_TreeView; }

bool TreeView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	const float itemHeight = EffectiveItemHeight(this);
	const int renderItemCount = itemHeight > 0.0f ? std::max(1, (int)((float)this->Height / itemHeight)) : 1;
	int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
	if (maxScroll < 0) maxScroll = 0;
	if (maxScroll <= 0) return false;
	return delta > 0
		? this->ScrollIndex > 0
		: this->ScrollIndex < maxScroll;
}

CursorKind TreeView::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;

	const float itemHeight = EffectiveItemHeight(this);
	if (itemHeight > 0.0f)
	{
		const int renderCount = std::max(1, (int)((float)this->Height / itemHeight));
		const bool hasVScroll = (_contentRenderItems > (float)renderCount + 0.001f);
		if (hasVScroll && localX >= (this->Width - 8))
			return CursorKind::SizeNS;
	}
	return CursorKind::Arrow;
}

TreeView::TreeView(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->Root = new TreeNode(L"");
	this->SelectedNode = nullptr;
}
TreeView::~TreeView()
{
	delete this->Root;
}

bool TreeView::IsAnimationRunning()
{
	return this->Root && this->Root->IsAnimationRunning();
}

bool TreeView::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

void TreeView::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	if (_contentRenderItems <= 0.0f) return;

	const float itemHeight = EffectiveItemHeight(this);
	if (itemHeight <= 0.0f) return;

	const float height = (float)this->Height;
	int renderItemCount = (int)(height / itemHeight);
	if (renderItemCount <= 0) renderItemCount = 1;

	if ((float)renderItemCount >= _contentRenderItems)
	{
		if (this->ScrollIndex != 0)
		{
			this->ScrollIndex = 0;
			InvalidateVisual();
		}
		return;
	}

	int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
	if (maxScroll < 0) maxScroll = 0;

	float scrollBlockHeight = (renderItemCount / _contentRenderItems) * height;
	if (scrollBlockHeight < height * 0.1f) scrollBlockHeight = height * 0.1f;
	if (scrollBlockHeight > height) scrollBlockHeight = height;

	const float scrollHeight = height - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - grab;
	float per = targetTop / scrollHeight;
	if (per < 0.0f) per = 0.0f;
	if (per > 1.0f) per = 1.0f;

	int newScroll = (int)(per * (float)maxScroll);
	if (newScroll < 0) newScroll = 0;
	if (newScroll > maxScroll) newScroll = maxScroll;

	if (this->ScrollIndex != newScroll)
	{
		this->ScrollIndex = newScroll;
		InvalidateVisual();
	}
}
void TreeView::DrawScroll() {
	float width = this->Width - 8.0f;
	float height = static_cast<float>(this->Height);
	float itemHeight = EffectiveItemHeight(this);
	if (_contentRenderItems > 0.0f) {
		int renderItemCount = static_cast<int>(height / itemHeight);
		if ((float)renderItemCount < _contentRenderItems) {
			int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
			float scrollBlockHeight = (renderItemCount / _contentRenderItems) * height;
			if (scrollBlockHeight < height * 0.1f) scrollBlockHeight = height * 0.1f;
			if (scrollBlockHeight > height) scrollBlockHeight = height;
			float scrollPer = (float)this->ScrollIndex / (float)maxScroll;
			float scrollBlockTop = scrollPer * (height - scrollBlockHeight);
			this->ParentForm->Render->FillRoundRect(width, 0, 8.0f, height, this->ScrollBackColor, 4.0f);
			this->ParentForm->Render->FillRoundRect(width, scrollBlockTop, 8.0f, scrollBlockHeight, this->ScrollForeColor, 4.0f);
		}
	}
}
void TreeView::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	bool isSelected = this->ParentForm->Selected == this;
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}

		{
			const float itemHeight = EffectiveItemHeight(this);
			_contentRenderItems = measureNodes(this->Root->Children);
			this->MaxRenderItems = (int)std::ceil(_contentRenderItems);
			int maxScroll = (int)std::ceil(_contentRenderItems - ((float)this->Height / itemHeight));
			if (maxScroll < 0)maxScroll = 0;
			if (this->ScrollIndex > maxScroll) this->ScrollIndex = maxScroll;
			float cursorY = 0.0f;
			renderNodes(this, d2d, actualWidth, actualHeight, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children);
			this->DrawScroll();
			d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BorderColor);

		}
		if (!this->Enable)
		{
			d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
bool TreeView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < fileCount; i++)
		{
			DragQueryFile(hDropInfo, i, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		const float itemHeight = EffectiveItemHeight(this);
		const int renderItemCount = itemHeight > 0.0f ? std::max(1, (int)((float)this->Height / itemHeight)) : 1;
		int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
		if (maxScroll < 0) maxScroll = 0;
		if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
		{
			if (this->ScrollIndex < maxScroll)
			{
				this->ScrollIndex += 1;
				this->ScrollChanged(this);
				this->InvalidateVisual();
			}
		}
		else
		{
			if (this->ScrollIndex > 0)
			{
				this->ScrollIndex -= 1;
				this->ScrollChanged(this);
				this->InvalidateVisual();
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (isDraggingScroll) {
			UpdateScrollDrag(static_cast<float>(localY));
		}
		else
		{
			if (localX < 0 || localX > this->Width || localY < 0 || localY > this->Height)
			{
				if (this->HoveredNode != nullptr)
				{
					this->HoveredNode = nullptr;
					this->InvalidateVisual();
				}
			}
			else
			{
				auto size = this->ActualSize();
				const float itemHeight = EffectiveItemHeight(this);
				float cursorY = 0.0f;
				bool isHit = false;
				auto newHoveredNode = findNode(this, (float)localX, (float)localY, (float)size.cy, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
				bool needUpdate = this->HoveredNode != newHoveredNode;
				this->HoveredNode = newHoveredNode;
				if (needUpdate) this->InvalidateVisual();
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			if (this->ParentForm->Selected != this)
			{
				auto lse = this->ParentForm->Selected;
				this->ParentForm->Selected = this;
				if (lse) lse->InvalidateVisual();
			}
			if (localX >= Width - 8 && localX <= Width)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float itemHeight = EffectiveItemHeight(this);
				if (itemHeight > 0.0f && this->MaxRenderItems > 0)
				{
					const float height = (float)this->Height;
					int renderItemCount = (int)(height / itemHeight);
					if (renderItemCount <= 0) renderItemCount = 1;
					if (renderItemCount < this->MaxRenderItems)
					{
						int maxScroll = this->MaxRenderItems - renderItemCount;
						if (maxScroll < 0) maxScroll = 0;
						float thumbH = (renderItemCount / (float)this->MaxRenderItems) * height;
						if (thumbH < height * 0.1f) thumbH = height * 0.1f;
						if (thumbH > height) thumbH = height;
						const float moveSpace = std::max(0.0f, height - thumbH);
						float per = 0.0f;
						if (maxScroll > 0) per = std::clamp((float)this->ScrollIndex / (float)maxScroll, 0.0f, 1.0f);
						const float thumbTop = per * moveSpace;
						const float pointerY = (float)localY;
						const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbH));
						_scrollThumbGrabOffsetY = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
					}
					else
					{
						_scrollThumbGrabOffsetY = 0.0f;
					}
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)localY);
			}
			else
			{
				auto size = this->ActualSize();
				const float itemHeight = EffectiveItemHeight(this);
				float cursorY = 0.0f;
				bool isHit = false;
				auto node = findNode(this, (float)localX, (float)localY, (float)size.cy, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
				if (node)
				{
					if (isHit)
					{
						node->SetExpanded(!node->Expand);
					}
					else
					{
						bool isChanged = this->SelectedNode != node;
						this->SelectedNode = node;
						if (isChanged)this->SelectionChanged(this);
					}
				}
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message)
		{
			if (isDraggingScroll) {
				isDraggingScroll = false;
			}
			if (this->ParentForm->Selected == this)
			{
				MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
				this->OnMouseClick(this, eventArgs);
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->Selected = this;
		auto size = this->ActualSize();
		const float itemHeight = EffectiveItemHeight(this);
		float cursorY = 0.0f;
		bool isHit = false;
		auto node = findNode(this, (float)localX, (float)localY, (float)size.cy, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
		if (node)
		{
			if (node->Children.size() > 0)
				node->SetExpanded(!node->Expand);
			if (!isHit)
			{
				bool isChanged = this->SelectedNode != node;
				this->SelectedNode = node;
				if (isChanged)this->SelectionChanged(this);
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->ParentForm)
		{
			auto pos = this->AbsLocation;
			float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
			this->ParentForm->SetImeCompositionWindowFromLogicalRect(
				D2D1_RECT_F{ (float)pos.x, (float)pos.y, (float)pos.x + 1.0f, (float)pos.y + caretH });
		}
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}

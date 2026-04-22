#include "TreeView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

static D2D1_POINT_2F LerpPoint(const D2D1_POINT_2F& from, const D2D1_POINT_2F& to, float t)
{
	return D2D1::Point2F(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t);
}

static D2D1_TRIANGLE BuildExpandTriangle(float left, float top, float itemHeight, float progress)
{
	const float triSize = itemHeight * 0.5f;
	const float triCenterX = left + triSize * 0.5f;
	const float triCenterY = top + (itemHeight * 0.2f) + triSize * 0.5f;
	D2D1_TRIANGLE collapsed{};
	collapsed.point1 = D2D1::Point2F(triCenterX - triSize * 0.4f, triCenterY - triSize * 0.5f);
	collapsed.point2 = D2D1::Point2F(triCenterX - triSize * 0.4f, triCenterY + triSize * 0.5f);
	collapsed.point3 = D2D1::Point2F(triCenterX + triSize * 0.4f, triCenterY);
	D2D1_TRIANGLE expanded{};
	expanded.point1 = D2D1::Point2F(triCenterX - triSize * 0.5f, triCenterY - triSize * 0.4f);
	expanded.point2 = D2D1::Point2F(triCenterX + triSize * 0.5f, triCenterY - triSize * 0.4f);
	expanded.point3 = D2D1::Point2F(triCenterX, triCenterY + triSize * 0.4f);
	progress = (std::clamp)(progress, 0.0f, 1.0f);
	D2D1_TRIANGLE tri{};
	tri.point1 = LerpPoint(collapsed.point1, expanded.point1, progress);
	tri.point2 = LerpPoint(collapsed.point2, expanded.point2, progress);
	tri.point3 = LerpPoint(collapsed.point3, expanded.point3, progress);
	return tri;
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
	for (auto* c : children)
	{
		if (!c) continue;
		const float renderTop = cursorY - scrollOffsetY;
		const float renderBottom = renderTop + itemHeight;
		const float baseLeft = (sunLevel * itemHeight) + 3.5f;

		if (renderBottom >= 0.0f && renderTop < h)
		{
			float renderLeft = baseLeft;
			float exTop = renderTop + (itemHeight * 0.2f);
			auto foreColor = (c == tree->SelectedNode) ? tree->SelectedForeColor : tree->ForeColor;
			if (c == tree->SelectedNode)
			{
				d2d->FillRect(0, renderTop, w, itemHeight, tree->SelectedBackColor);
			}
			else if (c == tree->HoveredNode)
			{
				d2d->FillRect(0, renderTop, w, itemHeight, tree->UnderMouseItemBackColor);
			}
			if (c->Children.size() > 0)
			{
				D2D1_TRIANGLE tri = BuildExpandTriangle(renderLeft, renderTop, itemHeight, c->CurrentExpandProgress());
				d2d->FillTriangle(tri, foreColor);

				if (auto* bmp = c->GetImageBitmap(d2d))
				{
					d2d->DrawBitmap(bmp, renderLeft + (itemHeight * 0.8f), renderTop, itemHeight, itemHeight);
					renderLeft += itemHeight;
				}
				d2d->DrawString(c->Text, renderLeft + (itemHeight * 0.8f), renderTop, foreColor, tree->Font);
			}
			else
			{
				if (auto* bmp = c->GetImageBitmap(d2d))
				{
					d2d->DrawBitmap(bmp, renderLeft, renderTop, itemHeight, itemHeight);
					renderLeft += itemHeight;
				}
				d2d->DrawString(c->Text, renderLeft, renderTop, foreColor, tree->Font);
			}
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

static TreeNode* findNode(float posX, float posY, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children, bool& isHitEx)
{
	for (auto* c : children)
	{
		if (!c) continue;
		const float currTop = cursorY - scrollOffsetY;
		const float currBottom = currTop + itemHeight;
		if (currBottom >= 0.0f && currTop < h)
		{
			if (posY >= currTop && posY <= currBottom)
			{
				float exLeft = (sunLevel * itemHeight) + 3.5f;
				if (posX >= exLeft && posX <= (exLeft + (itemHeight * 0.6f)) && c->Children.size() > 0)
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
					auto result = findNode(posX, posY, h, itemHeight, scrollOffsetY, childCursor, sunLevel + 1, c->Children, isHitEx);
					if (result)
						return result;
				}
				cursorY += childVisibleHeight;
			}
		}
	}

	return NULL;
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

CursorKind TreeView::QueryCursor(int xof, int yof)
{
	(void)yof;
	if (!this->Enable) return CursorKind::Arrow;

	const float fontHeight = this->Font ? this->Font->FontHeight : 0.0f;
	if (fontHeight > 0.0f)
	{
		const int renderCount = std::max(1, (int)((float)this->Height / fontHeight));
		const bool hasVScroll = (_contentRenderItems > (float)renderCount + 0.001f);
		if (hasVScroll && xof >= (this->Width - 8))
			return CursorKind::SizeNS;
	}
	return CursorKind::Arrow;
}

TreeView::TreeView(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->Root = new TreeNode(L"");
	this->SelectedNode = NULL;
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

	const float fontHeight = this->Font->FontHeight;
	if (fontHeight <= 0.0f) return;

	const float height = (float)this->Height;
	int renderItemCount = (int)(height / fontHeight);
	if (renderItemCount <= 0) renderItemCount = 1;

	if ((float)renderItemCount >= _contentRenderItems)
	{
		if (this->ScrollIndex != 0)
		{
			this->ScrollIndex = 0;
			PostRender();
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
		PostRender();
	}
}
void TreeView::DrawScroll() {
	float width = this->Width - 8.0f;
	float height = this->Height;
	float fontHeight = this->Font->FontHeight;
	if (_contentRenderItems > 0.0f) {
		int renderItemCount = height / fontHeight;
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
	bool isSelected = this->ParentForm->Selected == this;
	this->BeginRender();
	{
		d2d->FillRect(0, 0, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}

		{
			const float itemHeight = font->FontHeight;
			_contentRenderItems = measureNodes(this->Root->Children);
			this->MaxRenderItems = (int)std::ceil(_contentRenderItems);
			int maxScroll = (int)std::ceil(_contentRenderItems - ((float)this->Height / itemHeight));
			if (maxScroll < 0)maxScroll = 0;
			if (this->ScrollIndex > maxScroll) this->ScrollIndex = maxScroll;
			float cursorY = 0.0f;
			renderNodes(this, d2d, (float)size.cx, (float)size.cy, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children);
			this->DrawScroll();
			d2d->DrawRect(0, 0, size.cx, size.cy, this->ForeColor);

		}
		if (!this->Enable)
		{
			d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
bool TreeView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.push_back(strFileName);
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
		const float fontHeight = this->Font ? this->Font->FontHeight : 0.0f;
		const int renderItemCount = fontHeight > 0.0f ? std::max(1, (int)((float)this->Height / fontHeight)) : 1;
		int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
		if (maxScroll < 0) maxScroll = 0;
		if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
		{
			if (this->ScrollIndex < maxScroll)
			{
				this->ScrollIndex += 1;
				this->ScrollChanged(this);
				this->PostRender();
			}
		}
		else
		{
			if (this->ScrollIndex > 0)
			{
				this->ScrollIndex -= 1;
				this->ScrollChanged(this);
				this->PostRender();
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (isDraggingScroll) {
			UpdateScrollDrag(yof);
		}
		else
		{
			if (xof < 0 || xof > this->Width || yof < 0 || yof > this->Height)
			{
				if (this->HoveredNode != nullptr)
				{
					this->HoveredNode = nullptr;
					this->PostRender();
				}
			}
			else
			{
				auto font = this->Font;
				auto size = this->ActualSize();
				float cursorY = 0.0f;
				bool isHit = false;
				auto newHoveredNode = findNode((float)xof, (float)yof, (float)size.cy, font->FontHeight, (float)this->ScrollIndex * font->FontHeight, cursorY, 0, this->Root->Children, isHit);
				bool needUpdate = this->HoveredNode != newHoveredNode;
				this->HoveredNode = newHoveredNode;
				if (needUpdate) this->PostRender();
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
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
				if (lse) lse->PostRender();
			}
			if (xof >= Width - 8 && xof <= Width)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float fontHeight = this->Font ? this->Font->FontHeight : 0.0f;
				if (fontHeight > 0.0f && this->MaxRenderItems > 0)
				{
					const float height = (float)this->Height;
					int renderItemCount = (int)(height / fontHeight);
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
						const float localY = (float)yof;
						const bool hitThumb = (localY >= thumbTop && localY <= (thumbTop + thumbH));
						_scrollThumbGrabOffsetY = hitThumb ? (localY - thumbTop) : (thumbH * 0.5f);
					}
					else
					{
						_scrollThumbGrabOffsetY = 0.0f;
					}
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)yof);
			}
			else
			{
				auto font = this->Font;
				auto size = this->ActualSize();
				float cursorY = 0.0f;
				bool isHit = false;
				auto node = findNode((float)xof, (float)yof, (float)size.cy, font->FontHeight, (float)this->ScrollIndex * font->FontHeight, cursorY, 0, this->Root->Children, isHit);
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
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
		this->PostRender();
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
				MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
				this->OnMouseClick(this, event_obj);
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->Selected = this;
		auto font = this->Font;
		auto size = this->ActualSize();
		float cursorY = 0.0f;
		bool isHit = false;
		auto node = findNode((float)xof, (float)yof, (float)size.cy, font->FontHeight, (float)this->ScrollIndex * font->FontHeight, cursorY, 0, this->Root->Children, isHit);
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
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
		this->PostRender();
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
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
		this->PostRender();
	}
	break;
	}
	return true;
}
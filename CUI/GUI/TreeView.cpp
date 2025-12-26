#include "TreeView.h"
#include "Form.h"
static void renderNodes(TreeView* tree, D2DGraphics1* d2d, float x, float y, float w, float h, float itemHeight, int scrollIndex, int& currindex, int sunLevel, List<TreeNode*>& Children)
{
	float baseLeft = sunLevel * itemHeight + 3.5f + x;
	for (auto c : Children)
	{
		int renderIndex = currindex - scrollIndex;
		float renderTop = itemHeight * renderIndex;

		if (renderTop >= 0.0f && renderTop < h)
		{
			float renderLeft = baseLeft;
			float exTop = renderTop + (itemHeight * 0.2f) + y;
			auto foreColor = (c == tree->SelectedNode) ? tree->SelectedForeColor : tree->ForeColor;
			if (c == tree->SelectedNode)
			{
				d2d->FillRect(x, renderTop + y, w, itemHeight, tree->SelectedBackColor);
			}
			else if (c == tree->HoveredNode)
			{
				d2d->FillRect(x, renderTop + y, w, itemHeight, tree->UnderMouseItemBackColor);
			}
			if (c->Children.Count > 0)
			{
				d2d->DrawRect(renderLeft, exTop, itemHeight * 0.6f, itemHeight * 0.6f, foreColor);


				d2d->DrawLine(
					{ renderLeft + (itemHeight * 0.1f), exTop + (itemHeight * 0.3f) },
					{ renderLeft + (itemHeight * 0.5f), exTop + (itemHeight * 0.3f) },
					foreColor);
				if (!c->Expand)
				{

					d2d->DrawLine(
						{ renderLeft + (itemHeight * 0.3f), exTop + (itemHeight * 0.1f) },
						{ renderLeft + (itemHeight * 0.3f), exTop + (itemHeight * 0.5f) },
						foreColor);
				}

				if (c->Image)
				{
					d2d->DrawBitmap(c->Image, renderLeft + (itemHeight * 0.8f), renderTop + y, itemHeight, itemHeight);
					renderLeft += itemHeight;
				}
				d2d->DrawString(c->Text, renderLeft + (itemHeight * 0.8f), renderTop + y, foreColor, tree->Font);
			}
			else
			{
				if (c->Image)
				{
					d2d->DrawBitmap(c->Image, renderLeft, renderTop + y, itemHeight, itemHeight);
					renderLeft += itemHeight;
				}
				d2d->DrawString(c->Text, renderLeft, renderTop + y, foreColor, tree->Font);
			}
		}

		currindex += 1;

		if (c->Expand)
		{
			renderNodes(tree, d2d, x, y, w, h, itemHeight, scrollIndex, currindex, sunLevel + 1, c->Children);
		}
	}
}

static TreeNode* findNode(float posX, float posY, float w, float h, float itemHeight, int scrollIndex, int& currindex, int sunLevel, List<TreeNode*>& Children, bool& isHitEx)
{
	for (auto c : Children)
	{
		int renderIndex = currindex - scrollIndex;
		float currTop = itemHeight * renderIndex;
		float currBottom = currTop + itemHeight;
		if (currTop >= 0.0f && currTop < h)
		{
			if (posY >= currTop && posY <= currBottom)
			{
				float exLeft = (sunLevel * itemHeight) + 3.5f;
				if (posX >= exLeft && posX <= (exLeft + (itemHeight * 0.6f)) && c->Children.Count > 0)
					isHitEx = true;
				else
					isHitEx = false;
				return c;
			}
		}
		currindex += 1;
		if (c->Expand)
		{
			auto result = findNode(posX, posY, w, h, itemHeight, scrollIndex, currindex, sunLevel + 1, c->Children, isHitEx);
			if (result)
				return result;
		}
	}

	return NULL;
}

TreeNode::TreeNode(std::wstring text, ID2D1Bitmap* image)
{
	this->Text = text;
	this->Image = image;
	this->Expand = false;
	this->Children = List<TreeNode*>();
}
TreeNode::~TreeNode()
{
	for (auto& c : this->Children)
		delete c;
	this->Children.Clear();
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
		const bool hasVScroll = (this->MaxRenderItems > renderCount);
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

void TreeView::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	if (this->MaxRenderItems <= 0) return;

	const float fontHeight = this->Font->FontHeight;
	if (fontHeight <= 0.0f) return;

	const float height = (float)this->Height;
	int renderItemCount = (int)(height / fontHeight);
	if (renderItemCount <= 0) renderItemCount = 1;

	if (renderItemCount >= this->MaxRenderItems)
	{
		if (this->ScrollIndex != 0)
		{
			this->ScrollIndex = 0;
			PostRender();
		}
		return;
	}

	int maxScroll = this->MaxRenderItems - renderItemCount;
	if (maxScroll < 0) maxScroll = 0;

	float scrollBlockHeight = (renderItemCount / (float)this->MaxRenderItems) * height;
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
	if (this->MaxRenderItems > 0) {
		int renderItemCount = height / fontHeight;
		if (renderItemCount < this->MaxRenderItems) {
			int maxScroll = this->MaxRenderItems - renderItemCount;
			float scrollBlockHeight = (renderItemCount / (float)this->MaxRenderItems) * height;
			if (scrollBlockHeight < height * 0.1f) scrollBlockHeight = height * 0.1f;
			float scrollPer = (float)this->ScrollIndex / (float)maxScroll;
			float scrollBlockTop = scrollPer * (height - scrollBlockHeight);
			this->ParentForm->Render->FillRoundRect(this->AbsLocation.x + width, this->AbsLocation.y, 8.0f, height, this->ScrollBackColor, 4.0f);
			this->ParentForm->Render->FillRoundRect(this->AbsLocation.x + width, this->AbsLocation.y + scrollBlockTop, 8.0f, scrollBlockHeight, this->ScrollForeColor, 4.0f);
		}
	}
}
void TreeView::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	bool isSelected = this->ParentForm->Selected == this;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}

		{
			int curr = 0;
			renderNodes(this, d2d, abslocation.x, abslocation.y, size.cx, size.cy, font->FontHeight, ScrollIndex, curr, 0, this->Root->Children);
			this->MaxRenderItems = curr;
			int maxScroll = this->MaxRenderItems - (this->Height / (this->Font->FontHeight)) + 1;
			if (maxScroll < 0)maxScroll = 0;
			if (this->ScrollIndex > maxScroll) this->ScrollIndex = maxScroll;
			this->DrawScroll();
			d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->ForeColor);

		}
		if (!this->Enable)
		{
			d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
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
		List<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.Add(strFileName);
		}
		DragFinish(hDropInfo);
		if (files.Count > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		bool need_update = false;
		if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
		{
			auto d2d = this->ParentForm->Render;
			auto font = this->Font;
			float font_height = font->FontHeight;
			int pre_render_item_count = (this->MaxRenderItems - this->ScrollIndex) - 1;
			float pre_render_items_height = (pre_render_item_count * font_height);
			if (pre_render_items_height + font_height >= this->Height)
			{
				need_update = true;
				this->ScrollIndex += 1;
				this->ScrollChanged(this);
				this->PostRender();
			}
		}
		else
		{
			if (this->ScrollIndex > 0)
			{
				need_update = true;
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
			auto font = this->Font;
			auto abslocation = this->AbsLocation;
			auto size = this->ActualSize();
			int curr = 0;
			bool isHit = false;
			auto newHoveredNode = findNode(xof, yof, size.cx, size.cy, font->FontHeight, ScrollIndex, curr, 0, this->Root->Children, isHit);
			bool needUpdate = this->HoveredNode == newHoveredNode;
			this->HoveredNode = newHoveredNode;
			if (needUpdate) this->PostRender();
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
				auto abslocation = this->AbsLocation;
				auto size = this->ActualSize();
				int curr = 0;
				bool isHit = false;
				auto node = findNode(xof, yof, size.cx, size.cy, font->FontHeight, ScrollIndex, curr, 0, this->Root->Children, isHit);
				if (node)
				{
					if (isHit)
					{
						node->Expand = !node->Expand;
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
		auto abslocation = this->AbsLocation;
		auto size = this->ActualSize();
		int curr = 0;
		bool isHit = false;
		auto node = findNode(xof, yof, size.cx, size.cy, font->FontHeight, ScrollIndex, curr, 0, this->Root->Children, isHit);
		if (node)
		{
			if (node->Children.Count > 0)
				node->Expand = !node->Expand;
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
		auto pos = this->AbsLocation;
		HIMC hImc = ImmGetContext(this->ParentForm->Handle);
		COMPOSITIONFORM form;
		form.dwStyle = CFS_RECT;
		form.ptCurrentPos = pos;
		form.rcArea = RECT{ pos.x, pos.y + this->Height, pos.x + 300, pos.y + 240 };
		ImmSetCompositionWindow(hImc, &form);
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
#include "DesignDocument.h"

namespace DesignerModel
{
bool DesignFormModel::operator==(const DesignFormModel& other) const
{
	return Name == other.Name
		&& Text == other.Text
		&& FontName == other.FontName
		&& FontSize == other.FontSize
		&& Size.cx == other.Size.cx
		&& Size.cy == other.Size.cy
		&& Location.x == other.Location.x
		&& Location.y == other.Location.y
		&& BackColor.r == other.BackColor.r
		&& BackColor.g == other.BackColor.g
		&& BackColor.b == other.BackColor.b
		&& BackColor.a == other.BackColor.a
		&& ForeColor.r == other.ForeColor.r
		&& ForeColor.g == other.ForeColor.g
		&& ForeColor.b == other.ForeColor.b
		&& ForeColor.a == other.ForeColor.a
		&& ShowInTaskBar == other.ShowInTaskBar
		&& TopMost == other.TopMost
		&& Enable == other.Enable
		&& Visible == other.Visible
		&& VisibleHead == other.VisibleHead
		&& HeadHeight == other.HeadHeight
		&& MinBox == other.MinBox
		&& MaxBox == other.MaxBox
		&& CloseBox == other.CloseBox
		&& CenterTitle == other.CenterTitle
		&& AllowResize == other.AllowResize
		&& EventHandlers == other.EventHandlers;
}

bool DesignNode::operator==(const DesignNode& other) const
{
	return Id == other.Id
		&& ParentId == other.ParentId
		&& ParentRef == other.ParentRef
		&& Name == other.Name
		&& Type == other.Type
		&& Order == other.Order
		&& Props == other.Props
		&& Extra == other.Extra
		&& Events == other.Events;
}

int DesignDocument::AllocateNodeId()
{
	return NextStableId++;
}

void DesignDocument::RecalculateNextStableId()
{
	int maxId = 0;
	for (const auto& node : Nodes)
	{
		maxId = (std::max)(maxId, node.Id);
	}
	NextStableId = maxId + 1;
	if (NextStableId < 1)
	{
		NextStableId = 1;
	}
}

void DesignDocument::Clear()
{
	*this = DesignDocument();
}

bool DesignDocument::operator==(const DesignDocument& other) const
{
	return Schema == other.Schema
		&& SchemaVersion == other.SchemaVersion
		&& NextStableId == other.NextStableId
		&& Form == other.Form
		&& Nodes == other.Nodes;
}
}

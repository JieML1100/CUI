#include "DesignDocument.h"

namespace DesignerModel
{
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
}
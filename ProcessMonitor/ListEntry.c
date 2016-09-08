#include "ListEntry.h"

PPROCESSNODE InitListNode()
{
	PPROCESSNODE pNode = NULL;

	pNode = (PPROCESSNODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(PROCESSNODE), MEM_TAG);
	if (pNode == NULL)
	{
		return NULL;
	}

	return pNode;
}

VOID DestroyList()
{
	// 释放链表所有内存
	while (TRUE)
	{
		// 从链表中取出一个节点
		PPROCESSNODE pNode = (PPROCESSNODE)ExInterlockedRemoveHeadList(&g_ListHead, &g_Lock);
		if (NULL != pNode)
		{
			if (NULL != pNode->pProcessInfo)
			{
				ExFreePoolWithTag(pNode->pProcessInfo, MEM_TAG);
			}
			ExFreePoolWithTag(pNode, MEM_TAG);
		}
		else
		{
			break;
		}
	};
}

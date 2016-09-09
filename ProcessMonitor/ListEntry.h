#ifndef __LISTENTRY_H__
#define __LISTENTRY_H__

#include <ntifs.h>
#include "NtStructDef.h"

KSPIN_LOCK	g_Lock;			// 用于链表的锁
KEVENT		g_Event;		// 用于通知的事件
LIST_ENTRY	g_ListHead;		// 连表头

// 储存进程的链表
typedef struct
{
	LIST_ENTRY		list_entry;
	PPROCESSINFO	pProcessInfo;
} PROCESSNODE, *PPROCESSNODE;


PPROCESSNODE	InitListNode();
VOID			DestroyList();

#endif // !__LISTENTRY_H__



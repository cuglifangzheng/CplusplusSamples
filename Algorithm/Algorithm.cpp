// Algorithm.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ListReverse.h"

int main()
{
	int a[] = {1,2,3,4,5};
	Node* head = InitList(a, sizeof(a) / sizeof(int));
	PrintList(head);

	getchar();
    return 0;
}


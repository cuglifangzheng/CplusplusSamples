#pragma once
#include <iostream>
using namespace std;

typedef struct Node_
{
	int val;
	struct Node_* next;
}Node;

void PrintList(Node* head)
{
	Node* p = head;
	while (p != NULL)
	{
		cout << p->val << " ";
		p = p->next;
	}
	cout << endl;
}

Node* InitList(int a[], int len)
{
	if (len <= 0)
	{
		return NULL;
	}

	Node* head = new Node;
	head->val = a[0];
	head->next = NULL;

	Node* cur = head;
	for (int i = 1; i < len; i++)
	{
		Node* p = new Node;
		p->val = a[i];
		p->next = NULL;
		cur->next = p;

		cur = p;
	}


	return head;
}

// 遍历方式逆转链表
Node* ReverseByCircle(Node* head)
{
	if (head == NULL || head->next == NULL)
	{
		return head;
	}

	Node* pCur = head;
	Node* pPrev = NULL;
	while (pCur)
	{
		Node* pTmp = pCur;
		pCur = pCur->next;
		pTmp->next = pPrev;
		pPrev = pTmp;
	}
	
	return pPrev;
	
}

Node* ReverseByRecuse(Node* head)
{
	if (head == NULL)
	{
		return head;
	}
	Node* p = head;
	Node* q = head->next;
	if (q == NULL)
	{
		return head;
	}
	else
	{
		head = ReverseByRecuse(q);
	}
	q->next = p;
	p->next = NULL;
	
	return head;
}
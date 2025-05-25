/*
Copyright (©) 2022-2025  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>

#include <util.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/LinkedList.hpp>

#include <HAL/HAL.hpp>

bool operator==(const LinkedList::Node& left, const LinkedList::Node& right) {
    return ((left.data == right.data) && (left.next == right.next) && (left.previous == right.previous));
}

namespace LinkedList {

    Node nodePool[POOL_SIZE];
	uint64_t nodePool_UsedCount;
	uint8_t nodePool_BitmapData[POOL_SIZE / 8];
	RawBitmap nodePool_Bitmap;
	bool nodePoolHasBeenInitialised = false;

	void NodePool_Init() {
		nodePool_UsedCount = 0;
		memset(nodePool_BitmapData, 0, POOL_SIZE / 8);
		nodePool_Bitmap.SetSize(POOL_SIZE);
		nodePool_Bitmap.SetBuffer(&(nodePool_BitmapData[0]));
		nodePoolHasBeenInitialised = true;
	}

	void NodePool_Destroy() {
		nodePool_Bitmap.~RawBitmap();
		nodePoolHasBeenInitialised = false;
	}

	Node* NodePool_AllocateNode() {
		if (nodePool_UsedCount == POOL_SIZE - 1) return nullptr;
		for (uint64_t i = 0; i < POOL_SIZE; i++) {
			if (nodePool_Bitmap.Get(i) == 0) {
				nodePool_Bitmap.Set(i, true);
				nodePool_UsedCount++;
				return &(nodePool[i]);
			}
		}
		return nullptr;
	}

	bool NodePool_FreeNode(Node*& node) {
		for (uint64_t i = 0; i < POOL_SIZE; i++) {
			if ((uint64_t)node == (uint64_t)(&(nodePool[i]))) {
				nodePool_Bitmap.Set(i, false);
				nodePool_UsedCount--;
				node = nullptr;
				return true;
			}
		}
		return false;
	}

	bool NodePool_HasBeenInitialised() {
		return nodePoolHasBeenInitialised;
	}

	bool NodePool_IsInPool(Node* obj) {
        return (((uint64_t)obj >= (uint64_t)nodePool) && ((uint64_t)obj < ((uint64_t)nodePool + POOL_SIZE * sizeof(Node))));
    }

    uint64_t length(Node* head) {
        if (head == nullptr)
            return 0;

        Node* current = head;
        uint64_t count = 0;
        while (true) {
            count++;
            if (current->next == nullptr)
                break;
            current = current->next;
        }

        current = nullptr; // protects the node that current points to from potential deletion

        return count;
    }

    Node* newNode(uint64_t data, bool vmm, bool usePool) {
        Node* node;

        if (usePool)
            node = NodePool_AllocateNode();
        else if (vmm)
            node = (Node*)kcalloc_vmm(1, sizeof(Node));
        else
            node = new Node();

        node->data = data;
        node->previous = nullptr;
        node->next = nullptr;
        return node;
    }

    void insertNode(Node*& head, uint64_t data, bool vmm, bool usePool) {
        // check if head is NULL
        if (head == nullptr) {
            head = newNode(data, vmm, usePool);
            return;
        }

        // move to last node
        Node* current = head;
        while (true) {
            if (current->next == nullptr)
                break;
            current = current->next;
        }

        // get new node and set last node's next to it
        current->next = newNode(data, vmm, usePool);

        // update newly created node's previous to the last node
        current->next->previous = current;

        // clear the value of current to protect the node it is pointing to from possible deletion
        current = nullptr;
    }

    Node* findNode(Node* head, uint64_t data) {
        Node* current = head;
        while (current != nullptr) {
            if (current->data == data)
                return current;
            current = current->next;
        }
        return nullptr;
    }

    void deleteNode(Node*& head, uint64_t key, bool vmm, bool usePool) {
        Node* temp = head;
        if (temp != nullptr && temp->data == key) {
            head = temp->next;
            if (head != nullptr) {
                head->previous = nullptr;
                if (head->next != nullptr)
                    head->next->previous = head;
            }
            if (usePool && NodePool_IsInPool(temp))
                NodePool_FreeNode(temp);
            else if (vmm)
                kfree_vmm(temp);
            else
                delete temp;
            return;
        }
        if (temp == nullptr)
            return;
        while (temp->data != key) {
            if (temp->next == nullptr)
                return;
            temp = temp->next;
        }
        if (temp->next != nullptr)
            temp->next->previous = temp->previous;
        if (temp->previous != nullptr)
            temp->previous->next = temp->next;
        if (usePool && NodePool_IsInPool(temp))
            NodePool_FreeNode(temp);
        else if (vmm)
            kfree_vmm(temp);
        else
            delete temp;
    }

    void deleteNode(Node*& head, Node* node, bool vmm, bool usePool) {
        if (node == nullptr || head == nullptr)
            return;
        if (node->next != nullptr)
            node->next->previous = node->previous;
        if (node->previous != nullptr)
            node->previous->next = node->next;
        if (head == node)
            head = node->next;
        if (usePool && NodePool_IsInPool(node))
            NodePool_FreeNode(node);
        else if (vmm)
            kfree_vmm(node);
        else
            delete node;
    }

    void fprint(fd_t file, Node* head) {
        fprintf(file, "Linked list order: ");

        Node* current = head;
        while (current != nullptr) {
            fprintf(file, " %lu ", current->data);
            current = current->next;
        }

        fprintf(file, "\n");

        // clear the value of current to protect the node it is pointing to from possible deletion
        current = nullptr;
    }

    void panic(const char* str) {
        PANIC(str);
    }

} // namespace LinkedList

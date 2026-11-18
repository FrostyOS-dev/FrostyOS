/*
Copyright (©) 2022-2026  Frosty515

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

#include <stdio.h>
#include <util.h>

#include <DataStructures/Bitmap.hpp>
#include <DataStructures/LinkedList.hpp>

#include <HAL/HAL.hpp>

#include <Memory/Heap.hpp>

bool operator==(const LinkedList::Node& left, const LinkedList::Node& right) {
    return ((left.data == right.data) && (left.next == right.next) && (left.previous == right.previous));
}

namespace LinkedList {

    KMemCache* g_VMMNodeCache;
    KMemCache* g_KNodeCache;

    bool InitNodeCache(bool vmm) {
        if (vmm) {
            g_VMMNodeCache = g_VMMHeap.CacheCreate("LinkedList::VMMNodeCache", sizeof(Node), 8, nullptr, nullptr, nullptr, 0);
            return g_VMMNodeCache != nullptr;
        } else {
            g_KNodeCache = g_KHeap.CacheCreate("LinkedList::KNodeCache", sizeof(Node), 8, nullptr, nullptr, nullptr, 0);
            return g_KNodeCache != nullptr;
        }
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

    Node* newNode(uint64_t data, bool vmm) {
        Node* node = AllocNode(vmm);
        if (node == nullptr)
            return nullptr;

        node->data = data;
        node->previous = nullptr;
        node->next = nullptr;
        return node;
    }

    void insertNode(Node*& head, uint64_t data, bool vmm) {
        // check if head is NULL
        if (head == nullptr) {
            head = newNode(data, vmm);
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
        current->next = newNode(data, vmm);

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

    bool deleteNode(Node*& head, uint64_t key, bool vmm) {
        Node* temp = head;
        if (temp != nullptr && temp->data == key) {
            head = temp->next;
            if (head != nullptr) {
                head->previous = nullptr;
                if (head->next != nullptr)
                    head->next->previous = head;
            }
            FreeNode(vmm, temp);
            return true;
        }
        if (temp == nullptr)
            return false;
        while (temp->data != key) {
            if (temp->next == nullptr)
                return false;
            temp = temp->next;
        }
        if (temp->next != nullptr)
            temp->next->previous = temp->previous;
        if (temp->previous != nullptr)
            temp->previous->next = temp->next;
        FreeNode(vmm, temp);
        return true;
    }

    bool deleteNode(Node*& head, Node* node, bool vmm) {
        if (node == nullptr || head == nullptr)
            return false;
        if (node->next != nullptr)
            node->next->previous = node->previous;
        if (node->previous != nullptr)
            node->previous->next = node->next;
        if (head == node)
            head = node->next;
        FreeNode(vmm, node);
        return true;
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

} // namespace LinkedList

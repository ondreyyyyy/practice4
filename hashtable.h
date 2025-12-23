#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>

using namespace std;

template<typename K, typename V>
class Node {
private:
    V value;
    Node* prev;
    K key;
    Node* next;

public:
    Node(const K& key, const V& value) {
        this->key = key;
        this->value = value;
        next = nullptr;
        prev = nullptr;
    }

    const V& getValue() const { 
        return value; 
    }

    V& getValue() { 
        return value; 
    }

    Node* getNext() const { 
        return next; 
    }

    Node* getPrev() const { 
        return prev; 
    }

    const K& getKey() const { 
        return key; 
    }

    template<typename KK, typename VV>
    friend class HashTable;
};

template<typename K>
class HashFunction {
private:
    const int hashBase = 256;
    int partSize = 3;

public:
    size_t operator()(const K& key, size_t capacity) const {
        int total = 0;
        const string& s = key;

        for (size_t i = 0; i < s.length(); i += partSize) {
            int partValue = 0;

            for (size_t j = i; j < i + partSize && j < s.length(); j++) {
                partValue = partValue * hashBase + static_cast<unsigned char>(s[j]);
            }

            total += partValue;
        }

        return abs(total) % capacity;
    }
};

template<typename K, typename V>
class HashTable {
private:
    HashFunction<K> hf;
    const size_t initial_capacity = 8;
    size_t size;
    Node<K,V>** table;
    size_t capacity;
    const double loadFactor = 0.75;

    void rehash() {
        size_t oldCapacity = capacity;
        Node<K,V>** oldTable = table;

        capacity = oldCapacity * 2;
        table = new Node<K,V>*[capacity];
        size = 0;

        for (size_t i = 0; i < capacity; i++) {
            table[i] = nullptr;
        }

        for (size_t i = 0; i < oldCapacity; i++) {
            Node<K,V>* current = oldTable[i];
            while (current != nullptr) {
                Node<K,V>* next = current->next;

                size_t newIndex = hf(current->key, capacity);

                current->next = table[newIndex];
                current->prev = nullptr;
                if (table[newIndex] != nullptr) {
                    table[newIndex]->prev = current;
                }
                table[newIndex] = current;

                size++;
                current = next;
            }
        }

        delete[] oldTable;
    }

    bool needRehash() const {
        return static_cast<double>(size) / capacity > loadFactor;
    }

public:
    HashTable() {
        size = 0;
        capacity = initial_capacity;
        table = new Node<K,V>*[capacity];
        for (size_t i = 0; i < capacity; i++) {
            table[i] = nullptr;
        }
    }

    ~HashTable() {
        for (size_t i = 0; i < capacity; ++i) {
            Node<K,V>* current = table[i];
            while (current != nullptr) {
                Node<K,V>* temp = current;
                current = current->next;
                delete temp;
            }
        }
        delete[] table;
    }

    void insert(const K& key, const V& value) {
        if (needRehash()) {
            rehash();
        }

        size_t index = hf(key, capacity);
        Node<K,V>* current = table[index];

        while (current != nullptr) {
            if (current->key == key) {
                current->value = value;
                return;
            }
            current = current->next;
        }

        Node<K,V>* newNode = new Node<K,V>(key, value);
        newNode->next = table[index];
        newNode->prev = nullptr;

        if (table[index] != nullptr) {
            table[index]->prev = newNode;
        }

        table[index] = newNode;
        size++;
    }

    V& at(const K& key) {
        size_t index = hf(key, capacity);
        Node<K,V>* current = table[index];

        while (current != nullptr) {
            if (current->key == key) {
                return current->value;
            }
            current = current->next;
        }

        throw out_of_range("Ключ не найден");
    }

    const V& at(const K& key) const {
        size_t index = hf(key, capacity);
        Node<K,V>* current = table[index];

        while (current != nullptr) {
            if (current->key == key) {
                return current->value;
            }
            current = current->next;
        }

        throw out_of_range("Ключ не найден");
    }

    void erase(const K& key) {
        size_t index = hf(key, capacity);
        Node<K,V>* nodeToDelete = table[index];

        while (nodeToDelete != nullptr) {
            if (nodeToDelete->key == key) {
                if (nodeToDelete->prev != nullptr) {
                    nodeToDelete->prev->next = nodeToDelete->next;
                } else {
                    table[index] = nodeToDelete->next;
                }
                if (nodeToDelete->next != nullptr) {
                    nodeToDelete->next->prev = nodeToDelete->prev;
                }

                delete nodeToDelete;
                size--;
                return;
            }
            nodeToDelete = nodeToDelete->next;
        }
    }

    size_t getSize() const {
        return size;
    }

    Node<K,V>* getChain(size_t index) const {
        return table[index];
    }

    size_t getCapacity() const {
        return capacity;
    }

    void print() const {
        cout << "Хеш-таблица (элементов: " << size << ", вместимость: " << capacity << "):\n";

        for (size_t i = 0; i < capacity; i++) {
            cout << "Цепочка " << i << ": ";

            Node<K,V>* current = table[i];
            if (current == nullptr) {
                cout << "пусто";
            } else {
                while (current != nullptr) {
                    cout << "[" << current->key << "]";
                    if (current->next != nullptr) {
                        cout << " -> ";
                    }
                    current = current->next;
                }
            }
            cout << "\n";
        }
    }
};

#endif
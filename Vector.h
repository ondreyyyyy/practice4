#ifndef VECTOR_H
#define VECTOR_H  

#include <initializer_list>
#include <stdexcept>

template<typename T>
class Vector {
private:
    T* data = nullptr;
    size_t capacity = 0;
    size_t size = 0;
    
public:
    Vector() = default;

    explicit Vector(size_t count) : size(count), capacity(count) {
        data = new T[capacity];
    }

    Vector(size_t count, const T& value) : size(count), capacity(count) {
        data = new T[capacity];
        for (size_t i = 0; i < size; i++) {
            data[i] = value;
        }
    }

    Vector(const Vector& other) : size(other.size), capacity(other.capacity) {
        data = new T[capacity];
        for (size_t i = 0; i < size; i++) {
            data[i] = other.data[i];
        }
    } 

    Vector(std::initializer_list<T> init) : size(init.size()), capacity(init.size()) {
        data = new T[capacity];
        size_t i = 0;
        for (const T& element : init) {
            data[i++] = element;
        }
    }

    ~Vector() {
        delete[] data;
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            delete[] data;
            size = other.size;
            capacity = other.capacity;
            data = new T[capacity];
            for (size_t i = 0; i < size; i++) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    T& operator[](size_t index) {
        return data[index];
    }

    const T& operator[](size_t index) const {
        return data[index];
    }

    T& at(size_t index) {
        if (index >= size) {
            throw std::out_of_range("Индекс вне диапазона.");
        }
        return data[index];
    }  
    
    const T& at(size_t index) const {
        if (index >= size) {
            throw std::out_of_range("Индекс вне диапазона.");
        }
        return data[index];
    }
    
    T* begin() noexcept {
        return data;
    }

    const T* begin() const noexcept {
        return data;
    }

    T* end() noexcept {
        return data + size;
    }

    const T* end() const noexcept {
        return data + size;
    }

    bool empty() const noexcept {
        return size == 0;
    }

    size_t get_size() const noexcept {
        return size;
    }

    size_t get_capacity() const noexcept {
        return capacity;
    }

    void reserve(size_t newCapacity) {
        if (newCapacity > capacity) {
            T* new_data = new T[newCapacity];
            for (size_t i = 0; i < size; i++) {
                new_data[i] = std::move(data[i]);
            }
            delete[] data;
            data = new_data;
            capacity = newCapacity;
        }
    }

    void push_back(const T& value) {
        if (size >= capacity) {
            size_t newCapacity = (capacity == 0) ? 1 : capacity * 2;
            reserve(newCapacity);
        }
        data[size] = value;
        size++;
    }

    void push_back(T&& value) {
        if (size >= capacity) {
            size_t newCapacity = (capacity == 0) ? 1 : capacity * 2;
            reserve(newCapacity);
        }
        data[size] = std::move(value);
        size++;
    }

    void pop_back() {
        if (size > 0) {
            size--;
        }
    }

    void clear() {
        size = 0;
    }

    T* insert(const T* pos, const T& value) {
        return insert(pos, 1, value);
    }

    T* insert(const T* pos, T&& value) {
        size_t index = pos - data;
        if (size + 1 > capacity) {
            size_t new_capacity = (capacity == 0) ? 1 : capacity * 2;
            reserve(new_capacity);
        }
        
        for (size_t i = size; i > index; --i) {
            data[i] = std::move(data[i - 1]);
        }
        
        data[index] = std::move(value);
        size++;
        
        return data + index;
    }

    T* insert(const T* pos, size_t count, const T& value) {
        size_t index = pos - data;
        
        if (size + count > capacity) {
            size_t new_capacity = capacity;
            while (new_capacity < size + count) {
                new_capacity = (new_capacity == 0) ? 1 : new_capacity * 2;
            }
            reserve(new_capacity);
        }
        
        for (size_t i = size + count - 1; i >= index + count; --i) {
            data[i] = std::move(data[i - count]);
        }
        
        for (size_t i = 0; i < count; ++i) {
            data[index + i] = value;
        }
        
        size += count;
        return data + index;
    }

    T* erase(const T* pos) {
        return erase(pos, pos + 1);
    }

    T* erase(const T* first, const T* last) {
        size_t start_index = first - data;
        size_t end_index = last - data;
        size_t count = end_index - start_index;
        
        if (first < data || last > data + size || first > last) {
            throw std::out_of_range("Индекс вне диапазона.");
        }
        
        if (count == 0) return const_cast<T*>(last);
        
        for (size_t i = start_index; i < size - count; ++i) {
            data[i] = std::move(data[i + count]);
        }
        
        size -= count;
        return data + start_index;
    }

    bool operator==(const Vector& other) const {
        if (size != other.size) {
            return false;
        }
        
        for (size_t i = 0; i < size; ++i) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Vector& other) const {
        return !(*this == other);
    }
};

#endif
#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace titans {

template <typename T>
class Container {
private:
    struct Node {
        T data;
        Node* prev{nullptr};
        Node* next{nullptr};

        template <typename... Args>
        explicit Node(Args&&... args)
            : data(std::forward<Args>(args)...), prev(nullptr), next(nullptr) {}
    };

    Node* head_{nullptr};
    Node* tail_{nullptr};
    std::size_t size_{0};

    // Unlinks and deletes a node, updating head_, tail_, and size_ in one place.
    void unlink_and_delete(Node* node) noexcept {
        if (!node) {
            return;
        }

        if (node->prev) {
            node->prev->next = node->next;
        } else {
            head_ = node->next;
        }

        if (node->next) {
            node->next->prev = node->prev;
        } else {
            tail_ = node->prev;
        }

        delete node;
        --size_;
    }

    // Traverses from the nearer end (head if pos < size_ / 2, else tail).
    Node* get_node_at(std::size_t pos) const noexcept {
        if (pos < size_ / 2) {
            Node* curr = head_;
            for (std::size_t i = 0; i < pos; ++i) {
                curr = curr->next;
            }
            return curr;
        } else {
            Node* curr = tail_;
            for (std::size_t i = size_ - 1; i > pos; --i) {
                curr = curr->prev;
            }
            return curr;
        }
    }

    template <typename U>
    void push_back_impl(U&& value) {
        Node* new_node = new Node(std::forward<U>(value));
        if (!tail_) {
            head_ = new_node;
            tail_ = new_node;
        } else {
            tail_->next = new_node;
            new_node->prev = tail_;
            tail_ = new_node;
        }
        ++size_;
    }

    template <typename U>
    void push_front_impl(U&& value) {
        Node* new_node = new Node(std::forward<U>(value));
        if (!head_) {
            head_ = new_node;
            tail_ = new_node;
        } else {
            new_node->next = head_;
            head_->prev = new_node;
            head_ = new_node;
        }
        ++size_;
    }

    template <typename U>
    void insert_impl(std::size_t pos, U&& value) {
        if (pos > size_) {
            throw std::out_of_range("Container::insert: index out of range");
        }
        if (pos == 0) {
            push_front_impl(std::forward<U>(value));
        } else if (pos == size_) {
            push_back_impl(std::forward<U>(value));
        } else {
            Node* curr = get_node_at(pos);
            Node* new_node = new Node(std::forward<U>(value));
            new_node->prev = curr->prev;
            new_node->next = curr;
            curr->prev->next = new_node;
            curr->prev = new_node;
            ++size_;
        }
    }

public:
    // O(1)
    Container() noexcept : head_(nullptr), tail_(nullptr), size_(0) {}

    // O(n)
    ~Container() {
        clear();
    }

    // O(n)
    Container(const Container& other) : head_(nullptr), tail_(nullptr), size_(0) {
        try {
            for (Node* curr = other.head_; curr != nullptr; curr = curr->next) {
                push_back_impl(curr->data);
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    // O(1)
    Container(Container&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    // O(n)
    Container& operator=(const Container& other) {
        if (this != &other) {
            Container temp(other);
            swap(temp);
        }
        return *this;
    }

    // O(n)
    Container& operator=(Container&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = nullptr;
            other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // O(1)
    std::size_t size() const noexcept {
        return size_;
    }

    // O(1)
    bool empty() const noexcept {
        return size_ == 0;
    }

    // O(1)
    void push_back(const T& value) {
        push_back_impl(value);
    }

    // O(1)
    void push_back(T&& value) {
        push_back_impl(std::move(value));
    }

    // O(1)
    void push_front(const T& value) {
        push_front_impl(value);
    }

    // O(1)
    void push_front(T&& value) {
        push_front_impl(std::move(value));
    }

    // O(n)
    void insert(std::size_t pos, const T& value) {
        insert_impl(pos, value);
    }

    // O(n)
    void insert(std::size_t pos, T&& value) {
        insert_impl(pos, std::move(value));
    }

    // O(n)
    void remove_at(std::size_t pos) {
        if (pos >= size_) {
            throw std::out_of_range("Container::remove_at: index out of range");
        }
        Node* target = get_node_at(pos);
        unlink_and_delete(target);
    }

    // O(n)
    bool remove_value(const T& value) {
        for (Node* curr = head_; curr != nullptr; curr = curr->next) {
            if (curr->data == value) {
                unlink_and_delete(curr);
                return true;
            }
        }
        return false;
    }

    // O(n)
    void clear() noexcept {
        Node* curr = head_;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

    // O(1)
    T& front() {
        if (empty()) {
            throw std::out_of_range("Container::front: container is empty");
        }
        return head_->data;
    }

    // O(1)
    const T& front() const {
        if (empty()) {
            throw std::out_of_range("Container::front: container is empty");
        }
        return head_->data;
    }

    // O(1)
    T& back() {
        if (empty()) {
            throw std::out_of_range("Container::back: container is empty");
        }
        return tail_->data;
    }

    // O(1)
    const T& back() const {
        if (empty()) {
            throw std::out_of_range("Container::back: container is empty");
        }
        return tail_->data;
    }

    // O(n)
    T& at(std::size_t pos) {
        if (pos >= size_) {
            throw std::out_of_range("Container::at: index out of range");
        }
        return get_node_at(pos)->data;
    }

    // O(n)
    const T& at(std::size_t pos) const {
        if (pos >= size_) {
            throw std::out_of_range("Container::at: index out of range");
        }
        return get_node_at(pos)->data;
    }

    // O(1)
    void swap(Container& other) noexcept {
        using std::swap;
        swap(head_, other.head_);
        swap(tail_, other.tail_);
        swap(size_, other.size_);
    }
};

// O(1)
template <typename T>
void swap(Container<T>& a, Container<T>& b) noexcept {
    a.swap(b);
}

} // namespace titans

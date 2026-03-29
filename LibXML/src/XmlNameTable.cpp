#include "../include/XmlNameTable.h"

namespace System::Xml {

XmlNameTable::XmlNameTable() = default;

XmlNameTable::XmlNameTable(XmlNameTable&& other) noexcept
    : values_(std::move(other.values_)),
      firstBlock_(other.firstBlock_),
      currentBlock_(other.currentBlock_) {
    other.firstBlock_ = nullptr;
    other.currentBlock_ = nullptr;
}

XmlNameTable& XmlNameTable::operator=(XmlNameTable&& other) noexcept {
    if (this != &other) {
        auto* current = firstBlock_;
        while (current != nullptr) {
            for (std::size_t i = 0; i < current->used; ++i) {
                reinterpret_cast<std::string*>(current->storage + i * sizeof(std::string))->~basic_string();
            }
            auto* next = current->next;
            delete current;
            current = next;
        }

        values_ = std::move(other.values_);
        firstBlock_ = other.firstBlock_;
        currentBlock_ = other.currentBlock_;

        other.firstBlock_ = nullptr;
        other.currentBlock_ = nullptr;
    }
    return *this;
}

XmlNameTable::~XmlNameTable() {
    auto* current = firstBlock_;
    while (current != nullptr) {
        for (std::size_t i = 0; i < current->used; ++i) {
            reinterpret_cast<std::string*>(current->storage + i * sizeof(std::string))->~basic_string();
        }
        auto* next = current->next;
        delete current;
        current = next;
    }
}

const std::string& XmlNameTable::AllocateString(std::string_view value) {
    if (currentBlock_ == nullptr || currentBlock_->used == ArenaBlock::Capacity) {
        auto* nextBlock = new ArenaBlock();
        if (currentBlock_ != nullptr) {
            currentBlock_->next = nextBlock;
        } else {
            firstBlock_ = nextBlock;
        }
        currentBlock_ = nextBlock;
    }

    std::string* newString = new (currentBlock_->storage + currentBlock_->used * sizeof(std::string)) std::string(value);
    ++currentBlock_->used;
    return *newString;
}

const std::string& XmlNameTable::Add(std::string_view value) {
    auto it = values_.find(value);
    if (it != values_.end()) {
        return **it;
    }
    const std::string& newStr = AllocateString(value);
    values_.insert(&newStr);
    return newStr;
}

const std::string* XmlNameTable::Get(std::string_view value) const {
    auto it = values_.find(value);
    if (it != values_.end()) {
        return *it;
    }
    return nullptr;
}

std::size_t XmlNameTable::Count() const noexcept {
    return values_.size();
}

}  // namespace System::Xml

#ifndef BITSET_H
#define BITSET_H

#include <ostream>
#include <vector>

class MyBitSet {
 private:
  unsigned n_;
  std::vector<unsigned> data_;

 public:
  MyBitSet(unsigned n) : n_(n) { data_.resize((n + 31) / 32, 0); }

  void Set(unsigned i) { data_[i >> 5] |= 1 << (i & 0x1f); }

  void Reset(unsigned i) { data_[i >> 5] &= ~(1 << (i & 0x1f)); }

  bool Test(unsigned i) { return data_[i >> 5] & (1 << (i & 0x1f)); }

  bool TestAndSet(unsigned i) {
    auto& d = data_[i >> 5];
    bool ret = d & (1 << (i & 0x1f));
    d |= 1 << (i & 0x1f);
    return ret;
  }

  unsigned Size() { return n_; }

  void BitAnd(MyBitSet& other) {
    for (unsigned i = 0; i < data_.size(); i++) {
      data_[i] &= other.data_[i];
    }
  }

  void BitOr(MyBitSet& other) {
    for (unsigned i = 0; i < data_.size(); i++) {
      data_[i] |= other.data_[i];
    }
  }

  void Clear() { memset(data_.data(), 0, data_.size() * sizeof(unsigned)); }

  void Copy(MyBitSet& other) { 
    for (int i = 0; i < data_.size(); ++i) {
      data_[i] = other.data_[i];
    }
  }

  bool HaveIntersection(MyBitSet& other) {
    for (unsigned i = 0; i < data_.size(); i++) {
      if (data_[i] & other.data_[i]) {
        return true;
      }
    }
    return false;
  }

  friend MyBitSet operator|(MyBitSet& a, MyBitSet& b) {
    MyBitSet ret(a.Size());
    for (unsigned i = 0; i < a.data_.size(); i++) {
      ret.data_[i] = a.data_[i] | b.data_[i];
    }
    return ret;
  }
  
  void BitOr(MyBitSet& a, MyBitSet& b) {
    for (unsigned i = 0; i < a.data_.size(); i++) {
      data_[i] = a.data_[i] | b.data_[i];
    }
  }

  friend MyBitSet operator&(MyBitSet& a, MyBitSet& b) {
    MyBitSet ret(a.Size());
    for (unsigned i = 0; i < a.data_.size(); i++) {
      ret.data_[i] = a.data_[i] & b.data_[i];
    }
    return ret;
  }

  MyBitSet& operator|=(MyBitSet& other) {
    for (unsigned i = 0; i < data_.size(); i++) {
      data_[i] |= other.data_[i];
    }
    return *this;
  }

  MyBitSet& operator&=(MyBitSet& other) {
    for (unsigned i = 0; i < data_.size(); i++) {
      data_[i] &= other.data_[i];
    }
    return *this;
  }

  friend MyBitSet GetIntersection(MyBitSet& a, MyBitSet& b) {
    MyBitSet ret(a.Size());
    for (unsigned i = 0; i < a.data_.size(); i++) {
      ret.data_[i] = a.data_[i] & b.data_[i];
    }
    return ret;
  }

  friend std::ostream& operator<<(std::ostream& os, MyBitSet& bs) {
    os << "{";
    for (unsigned i = 0; i < bs.Size(); i++) {
      if (bs.Test(i)) {
        os << i << ", ";
      }
    }
    os << "}";
    return os;
  }

  int BitCount() {
    int ret = 0;
    for (unsigned i = 0; i < data_.size(); i++) {
      ret += __builtin_popcount(data_[i]);
    }
    return ret;
  }

  bool Empty() {
    for (unsigned i = 0; i < data_.size(); i++) {
      if (data_[i]) {
        return false;
      }
    }
    return true;
  }
};

#define BIT_TEST(x, i) ((x) & (1 << (i)))
#define BIT_SET(x, i) ((x) |= (1 << (i)))

#endif
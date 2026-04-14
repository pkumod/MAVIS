#ifndef COMBINATION_H
#define COMBINATION_H

#include <memory>

typedef unsigned int ui;

long double Perm(int n, int k);

class Combination {
  int n_, k_;
  std::unique_ptr<ui[]> data_;

public:
  Combination(int n, int k) : n_(n), k_(k) {
    data_.reset(new ui[k]);
  }

  void Init();
  void Next();
  void Next(int pos);
  bool End() const {
    return data_[0] == n_;
  }
  const ui* Data() const {
    return data_.get();
  }
};

#endif
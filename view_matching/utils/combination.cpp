#include "combination.h"

long double Perm(int n, int k) {
  if (n < k) return 0;
  long double res = 1;
  for (int i = 0; i < k; i++) {
    res *= (n - i);
  }
  return res;
}

void Combination::Init() {
  for (int i = 0; i < k_; ++i) {
    data_[i] = i;
  }
}

void Combination::Next() {
  int i = k_ - 1;
  while (i >= 0 && data_[i] == n_ - k_ + i) {
    --i;
  }
  if (i < 0) {
    data_[0] = n_;
    return;
  }
  ++data_[i];
  for (int j = i + 1; j < k_; ++j) {
    data_[j] = data_[j - 1] + 1;
  }
}

void Combination::Next(int pos) {
  int i = pos;
  ui ori_val = data_[i];
  while (i >= 0 && data_[i] == n_ - k_ + i) {
    --i;
  }
  if (i < 0) {
    data_[0] = n_;
    return;
  }
  ++data_[i];
  for (int j = i + 1; j < k_; ++j) {
    data_[j] = data_[j - 1] + 1;
  }
  if (!End() && ori_val == data_[pos]) {
    Next(pos);
  }
}

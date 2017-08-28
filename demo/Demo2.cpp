#include "Delegate.h"

using namespace delly;

// Smaller demo for compiler explorer

struct A
{
  A(int v) : val(v) {}
  int GetMyNumber(int c) const { return c + val; }
  virtual int GetMyNumber2(int c) = 0;

  int val;
};

struct B : public A
{
  B(int v) : A(v) {}
  int GetMyNumber2(int c) override { return c + GetMyNumber(c); }
};

int main() {

  using D = Delegate<int(int)>;

  B b1(3);
  B b2(7);

  D d1, d2, d3;
  d1 = Delegate<int(int)>([](int i){ return i + 42; });
  d2 = MakeDelegate(b1, &A::GetMyNumber);
  d3 = MakeDelegate(b2, &A::GetMyNumber2);

  int x = 1000;
  return d1(x) + d2(x) + d3(x);
}

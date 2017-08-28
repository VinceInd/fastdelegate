#pragma once

struct A {};
struct B : virtual A {};
struct C {};
struct D : A, C {};
struct E;

int main(int argc, char ** argv) {

  unsigned short size[] = {
    0x1a1a,
    sizeof(void (A::*)()),
    0x2b2b,
    sizeof(void (B::*)()),
    0x3c3c,
    sizeof(void (C::*)()),
    0x4d4d,
    sizeof(void (D::*)()),
    0x5e5e,
    sizeof(void (E::*)()),
    0x6f6f };

  return size[argc];
}

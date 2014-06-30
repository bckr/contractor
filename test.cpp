#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    return 0;
}

///\invariant height_greater_zero: height > 0
///\invariant width_greater_zero: width > 0
class Rectangle {

    int width, height;

  public:

    ///\pre a_not_zero: a != 0
    ///\post b_not_zero: b != 0
    void setEdges (int a, int b) {
        this->width = a;
        this->height = b;
   }

    ///\pre c_less_d: c < d
    ///\post d_less_c: d < c
    virtual int doSomething(int c, int d) {
       return c * d;
    }

} rect;

///\invariant some_inv: a > 0
///\invariant another_inv: a != c
class Test : public Rectangle {

    int a;
    int c;

  public:

    ///\pre a_not_zero: c != 0
    virtual int doSomething(int c, int d) {
        int b = c * 2;
        return b;
    }

} test;

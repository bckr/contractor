#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

int main() {
  return 0;
}

///\pre b_not_zero: b != 0
///\pre b_not_a: b != a
///\post b_not_null: b != NULL
///\post b_nt_null: b != NULL
int devide(int a, int b)
{
  return a / b;
}

///\pre some_precondition: bar != 0
///\post bar_greater_zero: bar > 0
///\post ba_greater_zero: bar > 0
void foo(int bar) {
  int baz = bar * 3;
}

///\pre sorted_asc: (A i: 0 <= i < arraySize - 1, values[i] <= values[i+1])
///\pre valid_array_size: arraySize > 0
int bar(int values[], int arraySize) {
  return values[0];
}

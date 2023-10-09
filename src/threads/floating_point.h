#ifndef THREADS_FLOATING_POINT_H
#define THREADS_FLOATING_POINT_H

/* Constants */
const int F = (1 << 14);

/* Functions */
/* Convert int to fp*/
int int_to_fp (int n) {
  return n * F;
}

/* Convert fp to int (rounding toward zero) */
int fp_to_int_zero (int x) {
  return x / F;
}

/* Convert fp to int (rounding to nearest) */
int fp_to_int_nearest (int x) {
  if (x >= 0) {
    return (x + F / 2) / F;
  } else {
    return (x - F / 2) / F;
  }
}

/* Add two fp */
int fp_add (int x, int y) {
  return x + y;
}

/* Add fp and int */
int fp_add_int (int x, int n) {
  return x + n * F;
}

/* Subtract int n from fp x */
int fp_sub_int (int x, int n) {
  return x - n * F;
}

/* Subtract fp y from fp x */
int fp_sub (int x, int y) {
  return x - y;
}

/* Multiply two fp */
int fp_mul (int x, int y) {
  return ((int64_t) x) * y / F;
}

/* Multiply fp x and int n */
int fp_mul_int (int x, int n) {
  return x * n;
}

/* Divide fp x by fp y */
int fp_div (int x, int y) {
  return ((int64_t) x) * F / y;
}

/* Divide fp x by int n */
int fp_div_int (int x, int n) {
  return x / n;
}

#endif /* threads/floating_point.h */

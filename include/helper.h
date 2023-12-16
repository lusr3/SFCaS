#include <immintrin.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>

#if !defined(HELPER_H)
#define HELPER_H

// 宏定义中的 # 相当于把之后的变量当作字符串
#define COUT_THIS(this) std::cout << this << std::endl
#define COUT_VAR(this) std::cout << #this << ": " << this << std::endl;
#define COUT_POS() COUT_THIS("at " << __FILE__ << ":" << __LINE__)
#define INVARIANT(cond)            \
  if (!(cond)) {                   \
    COUT_THIS(#cond << " failed"); \
    COUT_POS();                    \
    abort();                       \
  }
#define COUT_N_EXIT(msg) \
  COUT_THIS(msg);        \
  COUT_POS();            \
  abort();

#if defined(NDEBUGGING)
#define DEBUG_THIS(this)
#else
#define DEBUG_THIS(this) std::cerr << this << std::endl
#endif

inline void print_error(const char *format, ...) {
  va_list my_args;
  va_start(my_args, format);
  vfprintf(stderr, format, my_args);
  va_end(my_args);
}

#endif  // HELPER_H

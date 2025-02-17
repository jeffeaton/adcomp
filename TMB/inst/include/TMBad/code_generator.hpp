#ifndef HAVE_CODE_GENERATOR_HPP
#define HAVE_CODE_GENERATOR_HPP
// Autogenerated - do not edit by hand !
#define GLOBAL_HASH_TYPE unsigned int
#define GLOBAL_COMPRESS_TOL 16
#define GLOBAL_UNION_OR_STRUCT union
#define stringify(s) #s
#define xstringify(s) stringify(s)
#define THREAD_NUM 0
#define GLOBAL_INDEX_VECTOR std::vector<GLOBAL_INDEX_TYPE>
#define GLOBAL_INDEX_TYPE unsigned int
#define CONSTEXPR constexpr
#define ASSERT2(x, msg)                          \
  if (!(x)) {                                    \
    Rcerr << "ASSERTION FAILED: " << #x << "\n"; \
    Rcerr << "POSSIBLE REASON: " << msg << "\n"; \
    abort();                                     \
  }
#define GLOBAL_MAX_NUM_THREADS 48
#define INDEX_OVERFLOW(x) \
  ((size_t)(x) >= (size_t)std::numeric_limits<GLOBAL_INDEX_TYPE>::max())
#define ASSERT(x)                                \
  if (!(x)) {                                    \
    Rcerr << "ASSERTION FAILED: " << #x << "\n"; \
    abort();                                     \
  }
#define GLOBAL_REPLAY_TYPE ad_aug
#define GLOBAL_MIN_PERIOD_REP 10
#define INHERIT_CTOR(A, B)                                       \
  A() {}                                                         \
  template <class T1>                                            \
  A(const T1 &x1) : B(x1) {}                                     \
  template <class T1, class T2>                                  \
  A(const T1 &x1, const T2 &x2) : B(x1, x2) {}                   \
  template <class T1, class T2, class T3>                        \
  A(const T1 &x1, const T2 &x2, const T3 &x3) : B(x1, x2, x3) {} \
  template <class T1, class T2, class T3, class T4>              \
  A(const T1 &x1, const T2 &x2, const T3 &x3, const T4 &x4)      \
      : B(x1, x2, x3, x4) {}
#define GLOBAL_SCALAR_TYPE double
#include <fstream>
#include <iostream>
#include <sstream>
#include "global.hpp"

namespace TMBad {

void searchReplace(std::string &str, const std::string &oldStr,
                   const std::string &newStr);

struct code_config {
  bool asm_comments;
  bool gpu;
  std::string indent;
  std::string header_comment;
  std::string float_str;
  std::ostream *cout;
  std::string float_ptr();
  std::string void_str();
  void init_code();
  void write_header_comment();
  code_config();
};

void write_common(std::ostringstream &buffer, code_config cfg, size_t node);

void write_forward(global &glob, code_config cfg = code_config());

void write_reverse(global &glob, code_config cfg = code_config());

void write_all(global glob, code_config cfg = code_config());

}  // namespace TMBad
#endif  // HAVE_CODE_GENERATOR_HPP

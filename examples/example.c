// Example input for the sign analysis.  It uses every rule the analysis has and
// nothing it lacks, so whatever comes out unannotated is the domain's limit,
// not a missing rule.  README.md shows how to run it.
//
// Constants live in variables because clang folds `3 * -5` before the analysis
// ever sees it.  Signed `int` arithmetic is what makes clang emit `nsw`.  There
// is no main(): a call is an operation the analysis has no rule for.  The only
// unhandled operation left is `icmp`, which `if` and `for` need.

int add(int x) {
  int pos = 3, neg = -5;
  int pp = pos + pos; // positive
  int nn = neg + neg; // negative
  int pz = pos + 0;   // positive
  int pn = pos + neg; // unknown: 3 + -5 is negative, 5 + -3 is positive
  return x + pos;     // unknown, since x is
}

int sub(int x) {
  int pos = 3, neg = -5;
  int pn = pos - neg; // positive
  int np = neg - pos; // negative
  int mp = -pos;      // negative: clang emits this as 0 - pos
  int mn = -neg;      // positive
  int pp = pos - pos; // unknown: 3 - 5, 3 - 3, 5 - 3
  return x - 0;       // unknown, since x is
}

int mul(int x) {
  int pos = 3, neg = -5;
  int pp = pos * pos; // positive
  int nn = neg * neg; // positive
  int pn = pos * neg; // negative
  int xz = x * 0;     // zero, whatever x is
  return x * pos;     // unknown
}

int divide(int x) {
  int pos = 7, neg = -2;
  int pp = pos / pos; // zero or positive: division truncates, and 2 / 7 is 0
  int nn = neg / neg; // zero or positive
  int pn = pos / neg; // zero or negative
  int zx = 0 / x;     // zero: x == 0 would be undefined behaviour
  return x / pos;     // unknown
}

int bitwise(int x) {
  int pos = 6, neg = -3;
  int a1 = x & 0;     // zero
  int a2 = x & 7;     // zero or positive: a positive mask clears the sign bit
  int a3 = neg & neg; // negative: both sign bits are set
  int a4 = neg & pos; // zero or positive: -2 & 1 is 0
  int o1 = x | neg;   // negative, whatever x is
  int o2 = pos | pos; // positive
  int o3 = pos | 0;   // positive
  int x1 = neg ^ pos; // negative: exactly one sign bit is set
  int x2 = pos ^ pos; // zero or positive
  int x3 = neg ^ neg; // zero or positive
  return x ^ 0;       // unknown
}

// Facts joining where control flow merges.
int merge(int c) {
  int r;
  if (c)
    r = 1;
  else
    r = 0;
  // r is zero or positive
  int s;
  if (c)
    s = 4;
  else
    s = -4;
  // s is positive or negative
  int t = s * 2;      // positive or negative
  return r + r;       // zero or positive
}

// A loop: test fixpoint
int count(int n) {
  int sum = 0;
  for (int i = 0; i < n; ++i)
    sum = sum + i;
  return sum;
}

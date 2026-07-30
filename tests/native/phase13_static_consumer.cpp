#include "phase13_math.h"

int main() {
  if (rocket_phase13_add(20, 22) != 42) return 1;
  if (rocket_phase13_truth(1) != 1 || rocket_phase13_truth(0) != 0) return 2;
  if (rocket_phase13_float(2.5) != 2.5) return 3;
  if (rocket_phase13_char('R') != 'R') return 4;
  rocket_phase13_noop();
  return 0;
}

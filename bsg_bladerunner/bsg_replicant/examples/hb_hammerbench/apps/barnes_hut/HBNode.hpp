#include <stdint.h>

typedef struct HBNode_ {
  uint32_t child[8];
  float co_mass;
  float co_pos[3];
  float diamsq;
} HBNode;

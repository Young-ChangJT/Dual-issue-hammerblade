typedef struct Node_ {
  int is_leaf[8];
  int child[8];
  float co_mass;
  float co_pos[3];
  float diamsq;
} Node;

typedef struct HBListNode_ {
  int col_idx;
  float nnz;
  HBListNode_* next;
} HBListNode;


typedef struct HBList_ {
  int count;
  HBListNode* head;
  HBListNode* tail;
} HBList;

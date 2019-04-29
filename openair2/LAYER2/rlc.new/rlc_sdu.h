#ifndef _RLC_SDU_H_
#define _RLC_SDU_H_

typedef struct rlc_sdu_t {
  char *data;
  int size;
  /* next_byte indicates the starting byte to use to construct a new PDU */
  int next_byte;
  struct rlc_sdu_t *next;
} rlc_sdu_t;

rlc_sdu_t *rlc_new_sdu(char *buffer, int size);
void rlc_sdu_list_add(rlc_sdu_t **list, rlc_sdu_t **end, rlc_sdu_t *sdu);

#endif /* _RLC_SDU_H_ */

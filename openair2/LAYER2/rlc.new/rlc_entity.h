#ifndef _RLC_ENTITY_H_
#define _RLC_ENTITY_H_

typedef struct rlc_entity_t {
  void (*recv)(struct rlc_entity_t *entity, char *buffer, int size);
} rlc_entity_t;

rlc_entity_t *new_rlc_entity_am(
    int rx_maxsize,
    int t_reordering,
    int t_status_prohibit,
    int t_poll_retransmit,
    int poll_pdu,
    int poll_byte,
    int max_retx_threshold);

#endif /* _RLC_ENTITY_H_ */

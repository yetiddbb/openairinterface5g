#include "rlc_entity.h"

#include <stdio.h>
#include <stdlib.h>

#include "rlc_entity_am.h"

rlc_entity_t *new_rlc_entity_am(
    int rx_maxsize,
    void (*deliver_sdu)(void *deliver_sdu_data, struct rlc_entity_t *entity,
                      char *buf, int size),
    void *deliver_sdu_data,
    int t_reordering,
    int t_status_prohibit,
    int t_poll_retransmit,
    int poll_pdu,
    int poll_byte,
    int max_retx_threshold)
{
  rlc_entity_am_t *ret;

  ret = calloc(1, sizeof(rlc_entity_am_t));
  if (ret == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ret->common.recv      = rlc_entity_am_recv;
  ret->common.send_size = rlc_entity_am_send_size;
  ret->common.send      = rlc_entity_am_send;

  ret->common.deliver_sdu      = deliver_sdu;
  ret->common.deliver_sdu_data = deliver_sdu_data;

  ret->rx_maxsize         = rx_maxsize;
  ret->t_reordering       = t_reordering;
  ret->t_status_prohibit  = t_status_prohibit;
  ret->t_poll_retransmit  = t_poll_retransmit;
  ret->poll_pdu           = poll_pdu;
  ret->poll_byte          = poll_byte;
  ret->max_retx_threshold = max_retx_threshold;

  return (rlc_entity_t *)ret;
}

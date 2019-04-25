#include "rlc_entity.h"

#include <stdio.h>
#include <stdlib.h>

#include "rlc_entity_am.h"

rlc_entity_t *new_rlc_entity_am(
    int rx_maxsize,
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

  ret->common.recv = rlc_entity_am_recv;

  ret->rx_maxsize         = rx_maxsize;
  ret->t_reordering       = t_reordering;
  ret->t_status_prohibit  = t_status_prohibit;
  ret->t_poll_retransmit  = t_poll_retransmit;
  ret->poll_pdu           = poll_pdu;
  ret->poll_byte          = poll_byte;
  ret->max_retx_threshold = max_retx_threshold;

  return (rlc_entity_t *)ret;
}

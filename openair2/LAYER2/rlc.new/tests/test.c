#include "../rlc_entity.h"
#include "../rlc_entity_am.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/*
 * ENB <rx_maxsize> <tx_maxsize> <t_reordering> <t_status_prohibit>
 *     <t_poll_retransmit> <poll_pdu> <poll_byte> <max_retx_threshold>
 *     create the eNB RLC AM entity with given parameters
 *
 * UE <rx_maxsize> <tx_maxsize> <t_reordering> <t_status_prohibit>
 *    <t_poll_retransmit> <poll_pdu> <poll_byte> <max_retx_threshold>
 *     create the UE RLC AM entity with given parameters
 *
 * TIME <time>
 *     following actions to be performed at time <time>
 *     <time> starts at 1
 *     You must end your test definition with a line 'TIME, -1'.
 *
 * ENB_SDU <id> <size>
 *     send an SDU to eNB with id <i> and size <size>
 *     the SDU is [00 01 ... ff 01 ...]
 *     (ie. start byte is 00 then we increment for each byte, loop if needed)
 *
 * UE_SDU <id> <size>
 *     same as ENB_SDU but the SDU is sent to the UE
 *
 * ENB_PDU_SIZE <size>
 *     set 'enb_pdu_size'
 *
 * UE_PDU_SIZE <size>
 *     set 'ue_pdu_size'
 *
 * ENB_RECV_FAILS <fails>
 *     set the 'enb_recv_fails' flag to <fails>
 *     (1: recv will fail, 0: recv will succeed)
 *
 * UE_RECV_FAILS <fails>
 *     same as ENB_RECV_FAILS but for 'ue_recv_fails'
 */

enum action {
  ENB, UE,
  TIME, ENB_SDU, UE_SDU,
  ENB_PDU_SIZE, UE_PDU_SIZE,
  ENB_RECV_FAILS, UE_RECV_FAILS,
};

int test[] = {
/* TEST is defined at compilation time */
#include TEST
};

void deliver_sdu_enb(void *deliver_sdu_data, struct rlc_entity_t *_entity,
                     char *buf, int size)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: ENB: %"PRIu64": deliver SDU size %d [",
         entity->t_current, size);
  for (int i = 0; i < size; i++) printf(" %2.2x", (unsigned char)buf[i]);
  printf("]\n");
}

void successful_delivery_enb(void *successful_delivery_data,
                             rlc_entity_t *_entity, int sdu_id)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: ENB: %"PRIu64": SDU %d was successfully delivered.\n",
         entity->t_current, sdu_id);
}

void max_retx_reached_enb(void *max_retx_reached_data, rlc_entity_t *_entity)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: ENB: %"PRIu64": max RETX reached! radio link failure!\n",
         entity->t_current);
}

void deliver_sdu_ue(void *deliver_sdu_data, struct rlc_entity_t *_entity,
                    char *buf, int size)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: UE: %"PRIu64": deliver SDU size %d [",
         entity->t_current, size);
  for (int i = 0; i < size; i++) printf(" %2.2x", (unsigned char)buf[i]);
  printf("]\n");
}

void successful_delivery_ue(void *successful_delivery_data,
                            rlc_entity_t *_entity, int sdu_id)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: UE: %"PRIu64": SDU %d was successfully delivered.\n",
         entity->t_current, sdu_id);
}

void max_retx_reached_ue(void *max_retx_reached_data, rlc_entity_t *_entity)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  printf("TEST: UE: %"PRIu64", max RETX reached! radio link failure!\n",
         entity->t_current);
}

int main(void)
{
  rlc_entity_t *enb = NULL;
  rlc_entity_t *ue = NULL;
  int i;
  int k;
  char sdu[16000];
  char pdu[1000];
  int size;
  int pos;
  int enb_recv_fails = 0;
  int ue_recv_fails = 0;
  int enb_pdu_size = 1000;
  int ue_pdu_size = 1000;

  for (i = 0; i < 16000; i++)
    sdu[i] = i & 255;

  pos = 0;
  if (test[pos] != TIME) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  for (i = 1; i < 1000; i++) {
    if (i == test[pos+1]) {
      pos += 2;
      while (test[pos] != TIME)
        switch (test[pos]) {
        default: printf("fatal: unknown action\n"); exit(1);
        case ENB:
          enb = new_rlc_entity_am(test[pos+1], test[pos+2],
                                  deliver_sdu_enb, NULL,
                                  successful_delivery_enb, NULL,
                                  max_retx_reached_enb, NULL,
                                  test[pos+3], test[pos+4], test[pos+5],
                                  test[pos+6], test[pos+7], test[pos+8]);
          pos += 9;
          break;
        case UE:
          ue = new_rlc_entity_am(test[pos+1], test[pos+2],
                                 deliver_sdu_ue, NULL,
                                 successful_delivery_ue, NULL,
                                 max_retx_reached_ue, NULL,
                                 test[pos+3], test[pos+4], test[pos+5],
                                 test[pos+6], test[pos+7], test[pos+8]);
          pos += 9;
          break;
        case ENB_SDU:
          printf("TEST: ENB: %d: recv_sdu (id %d): size %d: [",
                 i, test[pos+1], test[pos+2]);
          for (k = 0; k < test[pos+2]; k++)
            printf(" %2.2x", (unsigned char)sdu[k]);
          printf("]\n");
          enb->recv_sdu(enb, sdu, test[pos+2], test[pos+1]);
          pos += 3;
          break;
        case UE_SDU:
          printf("TEST: UE: %d: recv_sdu (id %d): size %d: [",
                 i, test[pos+1], test[pos+2]);
          for (k = 0; k < test[pos+2]; k++)
            printf(" %2.2x", (unsigned char)sdu[k]);
          printf("]\n");
          ue->recv_sdu(ue, sdu, test[pos+2], test[pos+1]);
          pos += 3;
          break;
        case ENB_PDU_SIZE:
          enb_pdu_size = test[pos+1];
          pos += 2;
          break;
        case UE_PDU_SIZE:
          ue_pdu_size = test[pos+1];
          pos += 2;
          break;
        case ENB_RECV_FAILS:
          enb_recv_fails = test[pos+1];
          pos += 2;
          break;
        case UE_RECV_FAILS:
          ue_recv_fails = test[pos+1];
          break;
        }
    }

    enb->set_time(enb, i);
    ue->set_time(ue, i);

    size = enb->generate_pdu(enb, pdu, enb_pdu_size);
    if (size) {
      printf("TEST: ENB: %d: generate_pdu: size %d: [", i, size);
      for (k = 0; k < size; k++) printf(" %2.2x", (unsigned char)pdu[k]);
      printf("]\n");
      if (!ue_recv_fails)
        ue->recv_pdu(ue, pdu, size);
    }

    size = ue->generate_pdu(ue, pdu, ue_pdu_size);
    if (size) {
      printf("TEST: UE: %d: generate_pdu: size %d: [", i, size);
      for (k = 0; k < size; k++) printf(" %2.2x", (unsigned char)pdu[k]);
      printf("]\n");
      if (!enb_recv_fails)
        enb->recv_pdu(enb, pdu, size);
    }
  }

  return 0;
}

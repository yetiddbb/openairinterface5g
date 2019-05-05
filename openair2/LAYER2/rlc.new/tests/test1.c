/* test1:
 * - eNB gets one SDU and transmits it to UE
 * - UE gets one SDU and transmits it to eNB
 * - no transmission failure
 */

#include "rlc_entity.h"

#include <stdio.h>
#include <inttypes.h>

void deliver_sdu_enb(void *deliver_sdu_data, struct rlc_entity_t *entity,
                     char *buf, int size)
{
  printf("TEST: ENB: %"PRIu64": deliver SDU size %d [",
         entity->t_current, size);
  for (int i = 0; i < size; i++) printf(" %2.2x", (unsigned char)buf[i]);
  printf("]\n");
}

void successful_delivery_enb(void *successful_delivery_data,
                             rlc_entity_t *entity, int sdu_id)
{
  printf("TEST: ENB: %"PRIu64": SDU %d was successfully delivered.\n",
         entity->t_current, sdu_id);
}

void max_retx_reached_enb(void *max_retx_reached_data, rlc_entity_t *entity)
{
  printf("TEST: ENB: %"PRIu64": max RETX reached! radio link failure!\n",
         entity->t_current);
}

void deliver_sdu_ue(void *deliver_sdu_data, struct rlc_entity_t *entity,
                    char *buf, int size)
{
  printf("TEST: UE: %"PRIu64": deliver SDU size %d [",
         entity->t_current, size);
  for (int i = 0; i < size; i++) printf(" %2.2x", (unsigned char)buf[i]);
  printf("]\n");
}

void successful_delivery_ue(void *successful_delivery_data,
                            rlc_entity_t *entity, int sdu_id)
{
  printf("TEST: UE: %"PRIu64": SDU %d was successfully delivered.\n",
         entity->t_current, sdu_id);
}

void max_retx_reached_ue(void *max_retx_reached_data, rlc_entity_t *entity)
{
  printf("TEST: UE: %"PRIu64", max RETX reached! radio link failure!\n",
         entity->t_current);
}

typedef struct {
  int size;
  char *data;
  int time;
  int id;
} upper_layer_packet_to_send;

int main(void)
{
  rlc_entity_t *enb;
  rlc_entity_t *ue;
  int i;
  int k;
  upper_layer_packet_to_send to_enb[] = {
    { size: 10,
      data: (char []){ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      time: 1,
      id: 0 },
    { size: 0 }
  };
  int enb_next_packet = 0;
  upper_layer_packet_to_send to_ue[] = {
    { size: 5,
      data: (char []){ 5, 4, 3, 2, 1 },
      time: 10,
      id: 0 },
    { size: 0 }
  };
  int ue_next_packet = 0;
  char pdu[1000];
  int size;

  enb = new_rlc_entity_am(100000, 100000,
                          deliver_sdu_enb, NULL,
                          successful_delivery_enb, NULL,
                          max_retx_reached_enb, NULL,
                          35, 0, 45, -1, -1, 4);

  ue = new_rlc_entity_am(100000, 100000,
                         deliver_sdu_ue, NULL,
                         successful_delivery_ue, NULL,
                         max_retx_reached_ue, NULL,
                         35, 0, 45, -1, -1, 4);

  for (i = 1; i < 1000; i++) {
    enb->t_current = i;
    ue->t_current = i;

    if (to_enb[enb_next_packet].size && to_enb[enb_next_packet].time == i) {
      printf("TEST: ENB: %d: recv_sdu (id %d): size %d: [",
             i, to_enb[enb_next_packet].id, to_enb[enb_next_packet].size);
      for (k = 0; k < to_enb[enb_next_packet].size; k++)
        printf(" %2.2x", (unsigned char)to_enb[enb_next_packet].data[k]);
      printf("]\n");
      enb->recv_sdu(enb, to_enb[enb_next_packet].data,
                    to_enb[enb_next_packet].size,
                    to_enb[enb_next_packet].id);
      enb_next_packet++;
    }

    if (to_ue[ue_next_packet].size && to_ue[ue_next_packet].time == i) {
      printf("TEST: UE: %d: recv_sdu (id %d): size %d: [",
             i, to_ue[ue_next_packet].id, to_ue[ue_next_packet].size);
      for (k = 0; k < to_ue[ue_next_packet].size; k++)
        printf(" %2.2x", (unsigned char)to_ue[ue_next_packet].data[k]);
      printf("]\n");
      ue->recv_sdu(ue, to_ue[ue_next_packet].data,
                   to_ue[ue_next_packet].size,
                   to_ue[ue_next_packet].id);
      ue_next_packet++;
    }

    size = enb->generate_pdu(enb, pdu, 1000);
    if (size) {
      printf("TEST: ENB: %d: generate_pdu: size %d: [", i, size);
      for (k = 0; k < size; k++) printf(" %2.2x", (unsigned char)pdu[k]);
      printf("]\n");
      ue->recv_pdu(ue, pdu, size);
    }

    size = ue->generate_pdu(ue, pdu, 1000);
    if (size) {
      printf("TEST: UE: %d: generate_pdu: size %d: [", i, size);
      for (k = 0; k < size; k++) printf(" %2.2x", (unsigned char)pdu[k]);
      printf("]\n");
      enb->recv_pdu(enb, pdu, size);
    }
  }

  return 0;
}

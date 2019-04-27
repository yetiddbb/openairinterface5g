#include "rlc_entity_am.h"
#include "rlc_pdu.h"

#include <stdio.h>
#include <stdlib.h>

static int modulus(rlc_entity_am_t *entity, int a)
{
  /* as per 36.322 7.1, modulus base is vr(r) and modulus is 1024 */
  int r = a - entity->vr_r;
  if (r < 0) r += 1024;
  return r;
}

static int sn_in_recv_window(void *_entity, int sn)
{
  rlc_entity_am_t *entity = _entity;
  int mod_sn = modulus(entity, sn);
  /* we simplify vr(r)<=sn<vr(mr). base is vr(r) and vr(mr) = vr(r) + 512 */
  return mod_sn < 512;
}

static int sn_compare(void *_entity, int a, int b)
{
  rlc_entity_am_t *entity = _entity;
  return modulus(entity, a) - modulus(entity, b);
}

static int segment_already_received(rlc_entity_am_t *entity,
    int sn, int so, int data_size)
{
  /* TODO: optimize */
  rlc_pdu_segment_list_t *l = entity->rx_list;

  while (l != NULL) {
    if (l->s->sn == sn && l->s->so <= so &&
        l->s->so + l->s->size - l->s->data_offset >= so + data_size)
      return 1;
    l = l->next;
  }

  return 0;
}

static int rlc_am_segment_full(rlc_entity_am_t *entity, int sn)
{
  rlc_pdu_segment_list_t *l = entity->rx_list;
  int last_byte;
  int new_last_byte;

  last_byte = -1;
  while (l != NULL) {
    if (l->s->sn == sn) break;
    l = l->next;
  }
  while (l != NULL && l->s->sn == sn) {
    if (l->s->so > last_byte + 1) return 0;
    if (l->s->is_last) return 1;
    new_last_byte = l->s->so + l->s->size - l->s->data_offset - 1;
    if (new_last_byte > last_byte)
      last_byte = new_last_byte;
    l = l->next;
  }
  return 0;
}

/* return 1 if the new segment has some data to consume, 0 if not */
static int rlc_am_reassemble_next_segment(rlc_am_reassemble_t *r)
{
  int rf;
  int sn;

  r->sdu_offset = r->start->s->data_offset;

  rlc_pdu_decoder_init(&r->dec, r->start->s->data, r->start->s->size);

  rlc_pdu_decoder_get_bits(&r->dec, 1);            /* dc */
  rf    = rlc_pdu_decoder_get_bits(&r->dec, 1);
  rlc_pdu_decoder_get_bits(&r->dec, 1);            /* p */
  r->fi = rlc_pdu_decoder_get_bits(&r->dec, 2);
  r->e  = rlc_pdu_decoder_get_bits(&r->dec, 1);
  sn    = rlc_pdu_decoder_get_bits(&r->dec, 10);
  if (rf) {
    rlc_pdu_decoder_get_bits(&r->dec, 1);          /* lsf */
    r->so = rlc_pdu_decoder_get_bits(&r->dec, 15);
  } else {
    r->so = 0;
  }

  if (r->e) {
    r->e       = rlc_pdu_decoder_get_bits(&r->dec, 1);
    r->sdu_len = rlc_pdu_decoder_get_bits(&r->dec, 11);
  } else
    r->sdu_len = r->start->s->size - r->sdu_offset;

  /* new sn: read starts from PDU byte 0 */
  if (sn != r->sn) {
    r->pdu_byte = 0;
    r->sn = sn;
  }

  r->data_pos = r->start->s->data_offset + r->pdu_byte - r->so;

  /* TODO: remove this check, it is useless, data has been validated before */
  if (r->pdu_byte < r->so) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  /* if pdu_byte is not in [so .. so+len-1] then all bytes from this segment
   * have already been consumed
   */
  if (r->pdu_byte >= r->so + r->start->s->size - r->start->s->data_offset)
    return 0;

  /* go to correct SDU */
  while (r->pdu_byte >= r->so + (r->sdu_offset - r->start->s->data_offset) + r->sdu_len) {
    r->sdu_offset += r->sdu_len;
    if (r->e) {
      r->e       = rlc_pdu_decoder_get_bits(&r->dec, 1);
      r->sdu_len = rlc_pdu_decoder_get_bits(&r->dec, 11);
    } else {
      r->sdu_len = r->start->s->size - r->sdu_offset;
    }
  }

  return 1;
}

static void rlc_am_reassemble(rlc_entity_am_t *entity)
{
  rlc_am_reassemble_t *r = &entity->reassemble;

  while (r->start != NULL) {
    if (r->sdu_pos >= SDU_MAX) {
      /* TODO: proper error handling (discard PDUs with current sn from
       * reassembly queue? something else?)
       */
      printf("%s:%d:%s: bad RLC PDU\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }
    r->sdu[r->sdu_pos] = r->start->s->data[r->data_pos];
    r->sdu_pos++;
    r->data_pos++;
    r->pdu_byte++;
    if (r->data_pos == r->sdu_offset + r->sdu_len) {
      /* all bytes of SDU are consumed, check if SDU is fully there.
       * It is if the data pointer is not at the end of the PDU segment
       * or if 'fi' & 1 == 0
       */
      if (r->data_pos != r->start->s->size || (r->fi & 1) == 0) {
        entity->common.deliver_sdu(entity->common.deliver_sdu_data,
                                   (rlc_entity_t *)entity,
                                   r->sdu, r->sdu_pos);
        r->sdu_pos = 0;
      }
      if (r->data_pos != r->start->s->size) {
        /* not at the end, process next SDU */
        r->sdu_offset += r->sdu_len;
        if (r->e) {
          r->e       = rlc_pdu_decoder_get_bits(&r->dec, 1);
          r->sdu_len = rlc_pdu_decoder_get_bits(&r->dec, 11);
        } else
          r->sdu_len = r->start->s->size - r->sdu_offset;
      } else {
        /* all bytes are consumend, go to next segment not already fully
         * processed, if any
         */
        do {
          rlc_pdu_segment_list_t *e = r->start;
          entity->rx_size -= e->s->size;
          r->start = r->start->next;
          rlc_free_pdu_segment(e->s);
          free(e);
        } while (r->start != NULL && !rlc_am_reassemble_next_segment(r));
      }
    }
  }
}

static void rlc_am_reception_actions(rlc_entity_am_t *entity,
    rlc_pdu_segment_t *pdu_segment)
{
  int x = pdu_segment->sn;
  int vr_ms;
  int vr_r;

  printf("begin of rlc_am_reception_actions: x %d vr(h) %d vr(ms) %d vr(r) %d\n", x, entity->vr_h, entity->vr_ms, entity->vr_r);

  if (modulus(entity, x) >= modulus(entity, entity->vr_h))
    entity->vr_h = (x + 1) % 1024;

  vr_ms = entity->vr_ms;
  while (rlc_am_segment_full(entity, vr_ms))
    vr_ms = (vr_ms + 1) % 1024;
  entity->vr_ms = vr_ms;

  if (x == entity->vr_r) {
    vr_r = entity->vr_r;
    while (rlc_am_segment_full(entity, vr_r)) {
      /* move segments with sn=vr(r) from rx list to end of reassembly list */
      while (entity->rx_list != NULL && entity->rx_list->s->sn == vr_r) {
        rlc_pdu_segment_list_t *e = entity->rx_list;
        entity->rx_list = e->next;
        e->next = NULL;
        if (entity->reassemble.start == NULL) {
          entity->reassemble.start = e;
          /* the list was empty, we need to init decoder */
          entity->reassemble.sn = -1;
          if (!rlc_am_reassemble_next_segment(&entity->reassemble)) {
            /* TODO: proper error recovery (or remove the test, it should not happen */
            printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
            exit(1);
          }
        } else {
          entity->reassemble.end->next = e;
        }
        entity->reassemble.end = e;
      }

      /* update vr_r */
      vr_r = (vr_r + 1) % 1024;
    }
    entity->vr_r = vr_r;
  }

  rlc_am_reassemble(entity);

  if (entity->t_reordering_start) {
    int vr_x = entity->vr_x;
    if (vr_x < entity->vr_r) vr_x += 1024;
    if (vr_x == entity->vr_r || vr_x > entity->vr_r + 512)
      entity->t_reordering_start = 0;
  }

  if (entity->t_reordering_start == 0) {
    if (sn_compare(entity, entity->vr_h, entity->vr_r) > 0) {
      entity->t_reordering_start = entity->common.t_current;
      entity->vr_x = entity->vr_h;
    }
  }

  printf("end of rlc_am_reception_actions: vr(h) %d vr(ms) %d vr(r) %d\n", entity->vr_h, entity->vr_ms, entity->vr_r);
}

void rlc_entity_am_recv(rlc_entity_t *_entity, char *buffer, int size)
{
#define R do { if (rlc_pdu_decoder_in_error(&decoder)) goto err; } while (0)
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  rlc_pdu_decoder_t decoder;
  rlc_pdu_decoder_t data_decoder;

  int dc;
  int rf;
  int i;
  int p = 0;
  int fi;
  int e;
  int sn;
  int lsf;
  int so;

  int data_e;
  int data_li;

  int packet_count;
  int data_size;
  int data_start;
  int indicated_data_size;

  rlc_pdu_segment_t *pdu_segment;

  printf("got packet length %d\n", size);
  for (i = 0; i < size; i++) printf("%2.2x ", (unsigned char)buffer[i]);
  printf("\n");

  rlc_pdu_decoder_init(&decoder, buffer, size);
  dc = rlc_pdu_decoder_get_bits(&decoder, 1); R;
  if (dc == 0) goto control;

  /* data PDU */
  rf = rlc_pdu_decoder_get_bits(&decoder, 1); R;
  p  = rlc_pdu_decoder_get_bits(&decoder, 1); R;
  fi = rlc_pdu_decoder_get_bits(&decoder, 2); R;
  e  = rlc_pdu_decoder_get_bits(&decoder, 1); R;
  sn = rlc_pdu_decoder_get_bits(&decoder, 10); R;

  /* dicard PDU if rx buffer is full */
  if (entity->rx_size + size > entity->rx_maxsize) {
    printf("%s:%d:%s: warning: discard PDU, RX buffer full\n",
           __FILE__, __LINE__, __FUNCTION__);
    goto discard;
  }

  if (!sn_in_recv_window(entity, sn)) {
    printf("%s:%d:%s: warning: discard PDU, sn out of window (sn %d vr_r %d)\n",
           __FILE__, __LINE__, __FUNCTION__,
           sn, entity->vr_r);
    goto discard;
  }

  if (rf) {
    lsf = rlc_pdu_decoder_get_bits(&decoder, 1); R;
    so  = rlc_pdu_decoder_get_bits(&decoder, 15); R;
  } else {
    lsf = 1;
    so = 0;
  }

  packet_count = 1;

  /* go to start of data */
  indicated_data_size = 0;
  data_decoder = decoder;
  data_e = e;
  while (data_e) {
    data_e = rlc_pdu_decoder_get_bits(&data_decoder, 1); R;
    data_li = rlc_pdu_decoder_get_bits(&data_decoder, 11); R;
    if (data_li == 0) {
      printf("%s:%d:%s: warning: discard PDU, li == 0\n",
             __FILE__, __LINE__, __FUNCTION__);
      goto discard;
    }
    indicated_data_size += data_li;
    packet_count++;
  }
  rlc_pdu_decoder_align(&data_decoder);

  data_start = data_decoder.byte;
  data_size = size - data_start;

  if (data_size <= 0) {
    printf("%s:%d:%s: warning: discard PDU, wrong data size (%d data size %d)\n",
           __FILE__, __LINE__, __FUNCTION__,
           indicated_data_size, data_size);
    goto discard;
  }
  if (indicated_data_size >= data_size) {
    printf("%s:%d:%s: warning: discard PDU, bad LIs (sum of LI %d data size %d)\n",
           __FILE__, __LINE__, __FUNCTION__,
           indicated_data_size, data_size);
    goto discard;
  }

  /* discard segment if all the bytes of the segment are already there */
  if (segment_already_received(entity, sn, so, data_size)) {
    printf("%s:%d:%s: warning: discard PDU, already received\n",
           __FILE__, __LINE__, __FUNCTION__);
    goto discard;
  }

  char *fi_str[] = {
    "first byte: YES  last byte: YES",
    "first byte: YES  last byte: NO",
    "first byte: NO   last byte: YES",
    "first byte: NO   last byte: NO",
  };

  printf("found %d packets, data size %d data start %d [fi %d %s] (sn %d) (p %d)\n",
         packet_count, data_size, data_decoder.byte, fi, fi_str[fi], sn, p);

  /* put in pdu reception list */
  entity->rx_size += size;
  pdu_segment = rlc_new_pdu_segment(sn, so, size, lsf, buffer, data_start);
  entity->rx_list = rlc_pdu_segment_list_add(sn_compare, entity,
                                             entity->rx_list, pdu_segment);

  /* do reception actions (36.322 5.1.3.2.3) */
  rlc_am_reception_actions(entity, pdu_segment);

  if (p) {
    /* 36.322 5.2.3 says status triggering should be delayed
     * until x < VR(MS) or x >= VR(MR). This is not clear (what
     * is x then? we keep the same?). So let's trigger no matter what.
     */
    int vr_mr = (entity->vr_r + 512) % 1024;
    entity->status_triggered = 1;
    if (!(sn_compare(entity, sn, entity->vr_ms) < 0 ||
          sn_compare(entity, sn, vr_mr) >= 0)) {
      printf("%s:%d:%s: warning: STATUS trigger should be delayed, according to specs\n",
             __FILE__, __LINE__, __FUNCTION__);
    }
  }

  return;

control:
  printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__); exit(1);
  return;

err:
  printf("%s:%d:%s: error decoding PDU, discarding\n", __FILE__, __LINE__, __FUNCTION__);
  goto discard;

discard:
  if (p)
    entity->status_triggered = 1;

#undef R
}

static int rlc_entity_am_status_size(rlc_entity_am_t *entity, int maxsize)
{
  /* let's count bits */
  int bits = 15;               /* minimum size is 15 (header+ack_sn+e1) */
  int sn;

  maxsize *= 8;

  if (bits > maxsize) {
    printf("%s:%d:%s: warning: cannot generate status PDU, not enough room\n",
           __FILE__, __LINE__, __FUNCTION__);
    return 0;
  }

  /* each NACK adds 12 bits */
  sn = entity->vr_r;
  while (bits + 12 <= maxsize && sn_compare(entity, sn, entity->vr_ms) < 0) {
    if (!(rlc_am_segment_full(entity, sn)))
      bits += 12;
    sn = (sn + 1) % 1024;
  }

  return (bits + 7) / 8;
}

static int rlc_entity_am_send_status(rlc_entity_am_t *entity, char *buffer, int size)
{
  /* let's count bits */
  int bits = 15;               /* minimum size is 15 (header+ack_sn+e1) */
  int sn;
  rlc_pdu_encoder_t encoder;
  int has_nack = 0;
  int ack;

  rlc_pdu_encoder_init(&encoder, buffer, size);

  size *= 8;

  if (bits > size) {
    printf("%s:%d:%s: warning: cannot generate status PDU, not enough room\n",
           __FILE__, __LINE__, __FUNCTION__);
    return 0;
  }

  /* header */
  rlc_pdu_encoder_put_bits(&encoder, 0, 1);   /* D/C */
  rlc_pdu_encoder_put_bits(&encoder, 0, 3);   /* CPT */

  /* reserve room for ACK (it will be set after putting the NACKs) */
  rlc_pdu_encoder_put_bits(&encoder, 0, 10);

  /* at this point, ACK is VR(R) */
  ack = entity->vr_r;

  /* each NACK adds 12 bits */
  sn = entity->vr_r;
  while (bits + 12 <= size && sn_compare(entity, sn, entity->vr_ms) < 0) {
    if (!(rlc_am_segment_full(entity, sn))) {
      /* put previous e1 (is 1) */
      rlc_pdu_encoder_put_bits(&encoder, 1, 1);
      /* if previous was NACK, put previous e2 (0, we don't do 'so' thing) */
      if (has_nack)
        rlc_pdu_encoder_put_bits(&encoder, 0, 1);
      /* put NACKed sn */
      rlc_pdu_encoder_put_bits(&encoder, sn, 10);
      has_nack = 1;
      bits += 12;
    } else {
      /* this sn is full and we put all NACKs before it, use it for ACK */
      ack = (sn + 1) % 1024;
    }
    sn = (sn + 1) % 1024;
  }

  /* go to highest full sn+1 for ACK, VR(MS) is the limit */
  while (sn_compare(entity, ack, entity->vr_ms) < 0 &&
         rlc_am_segment_full(entity, ack)) {
    ack = (ack + 1) % 1024;
  }

  /* at this point, if last put was NACK then put 2 bits else put 1 bit */
  if (has_nack)
    rlc_pdu_encoder_put_bits(&encoder, 0, 2);
  else
    rlc_pdu_encoder_put_bits(&encoder, 0, 1);

  rlc_pdu_encoder_align(&encoder);

  /* let's put the ACK */
  buffer[0] |= ack >> 6;
  buffer[1] |= (ack & 0x3f) << 2;

  /* reset the trigger */
  entity->status_triggered = 0;

  /* start t_status_prohibit */
  entity->t_status_prohibit_start = entity->common.t_current;

  printf("ack is %d, buffer: ", ack);
  for (int i = 0; i < encoder.byte; i++) printf(" %2.2x", buffer[i]);
  printf("\n");

  return encoder.byte;
}

static int rlc_am_status_to_report(rlc_entity_am_t *entity)
{
  return entity->status_triggered &&
         (entity->common.t_current - entity->t_status_prohibit_start > entity->t_status_prohibit);
}

int rlc_entity_am_send_size(rlc_entity_t *_entity, int maxsize)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;

  if (rlc_am_status_to_report(entity))
    return rlc_entity_am_status_size(entity, maxsize);

  return 0;
}

int rlc_entity_am_send(rlc_entity_t *_entity, char *buffer, int size)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;

  if (rlc_am_status_to_report(entity))
    return rlc_entity_am_send_status(entity, buffer, size);

  return 0;
}

#include "rlc_entity_am.h"
#include "rlc_pdu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************************************************************************/
/* PDU RX functions                                                      */
/*************************************************************************/

static int modulus_rx(rlc_entity_am_t *entity, int a)
{
  /* as per 36.322 7.1, modulus base is vr(r) and modulus is 1024 for rx */
  int r = a - entity->vr_r;
  if (r < 0) r += 1024;
  return r;
}

static int sn_in_recv_window(void *_entity, int sn)
{
  rlc_entity_am_t *entity = _entity;
  int mod_sn = modulus_rx(entity, sn);
  /* we simplify vr(r)<=sn<vr(mr). base is vr(r) and vr(mr) = vr(r) + 512 */
  return mod_sn < 512;
}

static int sn_compare_rx(void *_entity, int a, int b)
{
  rlc_entity_am_t *entity = _entity;
  return modulus_rx(entity, a) - modulus_rx(entity, b);
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
        /* SDU is full - deliver to higher layer */
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

  if (modulus_rx(entity, x) >= modulus_rx(entity, entity->vr_h))
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
    if (sn_compare_rx(entity, entity->vr_h, entity->vr_r) > 0) {
      entity->t_reordering_start = entity->common.t_current;
      entity->vr_x = entity->vr_h;
    }
  }

  printf("end of rlc_am_reception_actions: vr(h) %d vr(ms) %d vr(r) %d\n", entity->vr_h, entity->vr_ms, entity->vr_r);
}

void rlc_entity_am_recv_pdu(rlc_entity_t *_entity, char *buffer, int size)
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
  entity->rx_list = rlc_pdu_segment_list_add(sn_compare_rx, entity,
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
    if (!(sn_compare_rx(entity, sn, entity->vr_ms) < 0 ||
          sn_compare_rx(entity, sn, vr_mr) >= 0)) {
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

/*************************************************************************/
/* TX functions                                                          */
/*************************************************************************/

static int pdu_size(rlc_entity_am_t *entity, rlc_tx_pdu_segment_t *pdu)
{
  int size;
  int sdu_count;
  int data_size;
  int li_bits;
  rlc_sdu_t *sdu;

  size = 2;
  data_size = pdu->data_size;

  if (pdu->is_segment)
    size += 2;

  sdu = pdu->start_sdu;

  sdu_count = 1;
  data_size -= sdu->size - pdu->sdu_start_byte;
  sdu = sdu->next;

  while (data_size > 0) {
    sdu_count++;
    data_size -= sdu->size;
    sdu = sdu->next;
  }

  li_bits = 12 * sdu_count;

  return size + (li_bits + 7) / 8;
}

static int modulus_tx(rlc_entity_am_t *entity, int a)
{
  /* as per 36.322 7.1, modulus base is vt(a) and modulus is 1024 for tx */
  int r = a - entity->vt_a;
  if (r < 0) r += 1024;
  return r;
}

static int sn_compare_tx(void *_entity, int a, int b)
{
  rlc_entity_am_t *entity = _entity;
  return modulus_tx(entity, a) - modulus_tx(entity, b);
}

static int header_size(int sdu_count)
{
  int bits = 16 + 12 * (sdu_count-1);
  /* padding if we have to */
  return (bits + 7) / 8;
}

typedef struct {
  int sdu_count;
  int data_size;
  int header_size;
} tx_pdu_size_t;

static tx_pdu_size_t tx_pdu_size(rlc_entity_am_t *entity, int maxsize)
{
  tx_pdu_size_t ret;
  int sdu_count;
  int sdu_size;
  int pdu_data_size;
  rlc_sdu_t *sdu;

  int vt_ms = (entity->vt_a + 512) % 1024;

  ret.sdu_count = 0;
  ret.data_size = 0;
  ret.header_size = 0;

  /* sn out of window? nothing to do */
  if (!(sn_compare_tx(entity, entity->vt_s, entity->vt_a) >= 0 &&
        sn_compare_tx(entity, entity->vt_s, vt_ms) < 0))
    return ret;

  /* TX PDU - let's make the biggest PDU we can with the SDUs we have */
  sdu_count = 0;
  pdu_data_size = 0;
  sdu = entity->tx_list;
  while (sdu != NULL) {
    /* include SDU only if it has not been fully included in PDUs already */
    if (sdu->next_byte != sdu->size) {
      int new_header_size = header_size(sdu_count+1);
      /* if we cannot put new header + at least 1 byte of data then over */
      if (new_header_size + pdu_data_size >= maxsize)
        break;
      sdu_count++;
      /* only include the bytes of this SDU not included in PDUs already */
      sdu_size = sdu->size - sdu->next_byte;
      /* don't feed more than 'maxsize' bytes */
      if (new_header_size + pdu_data_size + sdu_size > maxsize)
        sdu_size = maxsize - new_header_size - pdu_data_size;
      pdu_data_size += sdu_size;
      /* if we put more than 2^11-1 bytes then the LI field cannot be used,
       * so this is the last SDU we can put
       */
      if (sdu_size > 2047)
        break;
    }
    sdu = sdu->next;
  }

  if (sdu_count) {
    ret.sdu_count = sdu_count;
    ret.data_size = pdu_data_size;
    ret.header_size = header_size(sdu_count);
  }

  return ret;
}

static int status_size(rlc_entity_am_t *entity, int maxsize)
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
  while (bits + 12 <= maxsize && sn_compare_rx(entity, sn, entity->vr_ms) < 0) {
    if (!(rlc_am_segment_full(entity, sn)))
      bits += 12;
    sn = (sn + 1) % 1024;
  }

  return (bits + 7) / 8;
}

static int generate_status(rlc_entity_am_t *entity, char *buffer, int size)
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
  while (bits + 12 <= size && sn_compare_rx(entity, sn, entity->vr_ms) < 0) {
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
  while (sn_compare_rx(entity, ack, entity->vr_ms) < 0 &&
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

int transmission_buffer_empty(rlc_entity_am_t *entity)
{
  rlc_sdu_t *sdu;

  /* is transmission buffer empty? */
  sdu = entity->tx_list;
  while (sdu != NULL) {
    if (sdu->next_byte != sdu->size)
      return 0;
  }
  return 1;
}

int check_poll_after_pdu_assembly(rlc_entity_am_t *entity)
{
  int retransmission_buffer_empty;
  int window_stalling;
  int vt_ms;

  /* is retransmission buffer empty? */
  if (entity->retransmit_list == NULL)
    retransmission_buffer_empty = 1;
  else
    retransmission_buffer_empty = 0;

  /* is window stalling? */
  vt_ms = (entity->vt_a + 512) % 1024;
  if (!(sn_compare_tx(entity, entity->vt_s, entity->vt_a) >= 0 &&
        sn_compare_tx(entity, entity->vt_s, vt_ms) < 0))
    window_stalling = 1;
  else
    window_stalling = 0;

  return (transmission_buffer_empty(entity) && retransmission_buffer_empty) ||
         window_stalling;
}

void include_poll(rlc_entity_am_t *entity, char *buffer)
{
  /* set the P bit to 1 */
  buffer[0] |= 0x20;

  entity->pdu_without_poll = 0;
  entity->byte_without_poll = 0;

  /* set POLL_SN to VT(S) - 1 */
  entity->poll_sn = (entity->vt_s + 1023) % 1024;

  /* start t_poll_retransmit */
  entity->t_poll_retransmit_start = entity->common.t_current;
}

static int generate_tx_pdu(rlc_entity_am_t *entity, char *buffer, int bufsize)
{
  int                  vt_ms;
  tx_pdu_size_t        pdu_size;
  int                  first_sdu_full;
  int                  last_sdu_full;
  rlc_tx_pdu_segment_t *pdu;
  rlc_sdu_t            *sdu;
  int                  i;
  int                  cursize;
  rlc_pdu_encoder_t    encoder;
  int                  fi;
  int                  e;
  int                  li;
  char                 *out;
  int                  outpos;
  int                  p;

  /* sn out of window? do nothing */
  vt_ms = (entity->vt_a + 512) % 1024;
  if (!(sn_compare_tx(entity, entity->vt_s, entity->vt_a) >= 0 &&
        sn_compare_tx(entity, entity->vt_s, vt_ms) < 0))
    return 0;

  pdu_size = tx_pdu_size(entity, bufsize);
  if (pdu_size.sdu_count == 0)
    return 0;

  pdu = rlc_tx_new_pdu();

  pdu->sn = entity->vt_s;
  entity->vt_s = (entity->vt_s + 1) % 1024;

  /* go to first SDU (skip those already fully processed) */
  sdu = entity->tx_list;
  while (sdu->next_byte == sdu->size)
    sdu = sdu->next;

  pdu->start_sdu = sdu;

  first_sdu_full = sdu->next_byte == 0;

  pdu->sdu_start_byte = sdu->next_byte;

  pdu->so = 0;
  pdu->is_segment = 0;
  pdu->retx_count = 0;

  /* reserve SDU bytes */
  cursize = 0;
  last_sdu_full = 1;
  for (i = 0; i < pdu_size.sdu_count; i++, sdu = sdu->next) {
    int sdu_size = sdu->size - sdu->next_byte;
    if (cursize + sdu_size > pdu_size.data_size) {
      sdu_size = pdu_size.data_size - cursize;
      last_sdu_full = 0;
    }
    sdu->next_byte += sdu_size;
    cursize += sdu_size;
  }

  /* generate header */
  rlc_pdu_encoder_init(&encoder, buffer, bufsize);

  rlc_pdu_encoder_put_bits(&encoder, 1, 1);         /* D/C: 1 = data */
  rlc_pdu_encoder_put_bits(&encoder, 0, 1);         /* RF: 0 = PDU */
  rlc_pdu_encoder_put_bits(&encoder, 0, 1);         /* P: reserve, set later */

  fi = 0;
  if (!first_sdu_full)
    fi |= 0x02;
  if (!last_sdu_full)
    fi |= 0x01;
  rlc_pdu_encoder_put_bits(&encoder, fi, 2);        /* FI */

  /* to understand the logic for Es and LIs:
   * If we have:
   *   1 SDU:   E=0
   *
   *   2 SDUs:  E=1
   *     then:  E=0 LI(sdu[0])
   *
   *   3 SDUs:  E=1
   *     then:  E=1 LI(sdu[0])
   *     then:  E=0 LI(sdu[1])
   *
   *   4 SDUs:  E=1
   *     then:  E=1 LI(sdu[0])
   *     then:  E=1 LI(sdu[1])
   *     then:  E=0 LI(sdu[2])
   */
  if (pdu_size.sdu_count >= 2)
    e = 1;
  else
    e = 0;
  rlc_pdu_encoder_put_bits(&encoder, e, 1);         /* E */

  rlc_pdu_encoder_put_bits(&encoder, pdu->sn, 10);  /* SN */

  /* put LIs */
  sdu = pdu->start_sdu;
  /* first SDU */
  li = sdu->size - pdu->sdu_start_byte;
  /* put E+LI only if at least 2 SDUs */
  if (pdu_size.sdu_count >= 2) {
    /* E is 1 if at least 3 SDUs */
    if (pdu_size.sdu_count >= 3)
      e = 1;
    else
      e = 0;
    rlc_pdu_encoder_put_bits(&encoder, e, 1);       /* E */
    rlc_pdu_encoder_put_bits(&encoder, li, 11);     /* LI */
  }
  /* next SDUs, but not the last (no LI for the last) */
  sdu = sdu->next;
  for (i = 2; i < pdu_size.sdu_count; i++, sdu = sdu->next) {
    if (i != pdu_size.sdu_count - 1)
      e = 1;
    else
      e = 0;
    li = sdu->size;
    rlc_pdu_encoder_put_bits(&encoder, e, 1);       /* E */
    rlc_pdu_encoder_put_bits(&encoder, li, 11);     /* LI */
  }

  rlc_pdu_encoder_align(&encoder);

  /* generate data */
  out = buffer + encoder.byte;
  sdu = pdu->start_sdu;
  /* first SDU */
  li = sdu->size - pdu->sdu_start_byte;
  memcpy(out, sdu->data + pdu->sdu_start_byte, li);
  outpos = li;
  /* next SDUs */
  sdu = sdu->next;
  for (i = 1; i < pdu_size.sdu_count; i++, sdu = sdu->next) {
    li = sdu->size;
    if (outpos + li >= pdu_size.data_size)
      li = pdu_size.data_size - outpos;
    memcpy(out+outpos, sdu->data, li);
    outpos += li;
  }

  /* put PDU at the end of the wait list */
  entity->wait_list = rlc_tx_pdu_list_append(entity->wait_list, pdu);

  /* polling actions for a new PDU */
  entity->pdu_without_poll++;
  entity->byte_without_poll += pdu_size.data_size;
  if ((entity->poll_pdu != -1 &&
       entity->pdu_without_poll >= entity->poll_pdu) ||
      (entity->poll_byte != -1 &&
       entity->byte_without_poll >= entity->poll_byte))
    p = 1;
  else
    p = check_poll_after_pdu_assembly(entity);

  if (p)
    include_poll(entity, buffer);

  return pdu_size.header_size + pdu_size.data_size;
}

static int generate_retx_pdu(rlc_entity_am_t *entity, char *buffer, int size)
{
  printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__);
  exit(1);
  return 0;
}

static int status_to_report(rlc_entity_am_t *entity)
{
  return entity->status_triggered &&
         (entity->t_status_prohibit_start == 0 ||
          entity->common.t_current - entity->t_status_prohibit_start >
              entity->t_status_prohibit);
}

static int retx_pdu_size(rlc_entity_am_t *entity, int maxsize)
{
  int size;

  if (entity->retransmit_list == NULL)
    return 0;

  size = pdu_size(entity, entity->retransmit_list);
  if (size <= maxsize)
    return size;

  /* we can segment head of retransmist list if maxsize is large enough
   * to hold a PDU segment with at least 1 data byte (so 5 bytes: 4 bytes
   * header + 1 byte data)
   */
  if (maxsize < 5)
    return 0;

  /* a later segmentation of the head of retransmit list will generate a pdu
   * of size 'maxsize'
   */
  return maxsize;
}

rlc_entity_buffer_status_t rlc_entity_am_buffer_status(
    rlc_entity_t *_entity, int maxsize)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  rlc_entity_buffer_status_t ret;
  tx_pdu_size_t tx_size;

  /* status PDU, if we have to */
  if (status_to_report(entity))
    ret.status_size = status_size(entity, maxsize);
  else
    ret.status_size = 0;

  /* TX PDU */
  tx_size = tx_pdu_size(entity, maxsize);
  ret.tx_size = tx_size.data_size + tx_size.header_size;

  /* reTX PDU */
  ret.retx_size = retx_pdu_size(entity, maxsize);

  return ret;
}

int rlc_entity_am_generate_pdu(rlc_entity_t *_entity, char *buffer, int size)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;

  if (status_to_report(entity))
    return generate_status(entity, buffer, size);

  if (entity->retransmit_list != NULL)
    return generate_retx_pdu(entity, buffer, size);

  return generate_tx_pdu(entity, buffer, size);
}

/*************************************************************************/
/* SDU RX functions                                                      */
/*************************************************************************/

void rlc_entity_am_recv_sdu(rlc_entity_t *_entity, char *buffer, int size)
{
  rlc_entity_am_t *entity = (rlc_entity_am_t *)_entity;
  rlc_sdu_t *sdu;

  if (size > SDU_MAX) {
    printf("%s:%d:%s: fatal: SDU size too big (%d bytes)\n",
           __FILE__, __LINE__, __FUNCTION__, size);
    exit(1);
  }

  if (entity->tx_size + size > entity->tx_maxsize) {
    printf("%s:%d:%s: warning: SDU rejected, SDU buffer full\n",
           __FILE__, __LINE__, __FUNCTION__);
    return;
  }

  entity->tx_size += size;

  sdu = rlc_new_sdu(buffer, size);
  rlc_sdu_list_add(&entity->tx_list, &entity->tx_end, sdu);
}

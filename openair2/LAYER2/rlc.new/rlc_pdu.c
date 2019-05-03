#include "rlc_pdu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************************************************************/
/* RX PDU segment and segment list                                        */
/**************************************************************************/

rlc_pdu_segment_t *rlc_new_pdu_segment(int sn, int so, int size,
    int is_last, char *data, int data_offset)
{
  rlc_pdu_segment_t *ret = malloc(sizeof(rlc_pdu_segment_t));
  if (ret == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  ret->sn = sn;
  ret->so = so;
  ret->size = size;
  ret->is_last = is_last;

  ret->data = malloc(size);
  if (ret->data == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  memcpy(ret->data, data, size);

  ret->data_offset = data_offset;

  return ret;
}

void rlc_free_pdu_segment(rlc_pdu_segment_t *pdu_segment)
{
  free(pdu_segment->data);
  free(pdu_segment);
}

rlc_pdu_segment_list_t *rlc_pdu_segment_list_add(
    int (*sn_compare)(void *, int, int), void *sn_compare_data,
    rlc_pdu_segment_list_t *list, rlc_pdu_segment_t *pdu_segment)
{
  rlc_pdu_segment_list_t *ret = calloc(1, sizeof(rlc_pdu_segment_list_t));
  rlc_pdu_segment_list_t *cur;
  if (ret == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  ret->s = pdu_segment;

  /* order is by 'sn', if 'sn' is the same then order is by 'so' */
  cur = list;
  if (cur == NULL) return ret;

  while (1) {
    /* check if 'ret' is before 'cur' in the list */
    if (sn_compare(sn_compare_data, cur->s->sn, ret->s->sn) > 0 ||
        (cur->s->sn == ret->s->sn && cur->s->so > ret->s->so)) {
      rlc_pdu_segment_list_t *prev = cur->prev;
      cur->prev = ret;
      ret->next = cur;
      ret->prev = prev;
      /* if 'cur' was the head of the list, then 'ret' is the new head */
      if (prev == NULL) return ret;
      prev->next = ret;
      return list;
    }
    /* if 'cur' is the last of the list then 'ret' is the new last */
    if (cur->next == NULL) {
      cur->next = ret;
      ret->prev = cur;
      return list;
    }
    cur = cur->next;
  }
}

/**************************************************************************/
/* TX PDU management                                                      */
/**************************************************************************/

rlc_tx_pdu_segment_t *rlc_tx_new_pdu(void)
{
  rlc_tx_pdu_segment_t *ret = calloc(1, sizeof(rlc_tx_pdu_segment_t));
  if (ret == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  return ret;
}

void rlc_tx_free_pdu(rlc_tx_pdu_segment_t *pdu)
{
  free(pdu);
}

rlc_tx_pdu_segment_t *rlc_tx_pdu_list_append(rlc_tx_pdu_segment_t *list,
    rlc_tx_pdu_segment_t *pdu)
{
  rlc_tx_pdu_segment_t head;
  rlc_tx_pdu_segment_t *cur;

  head.next = list;

  cur = &head;
  while (cur->next != NULL) {
    cur = cur->next;
  }
  cur->next = pdu;

  return head.next;
}

rlc_tx_pdu_segment_t *rlc_tx_pdu_list_add(
    int (*sn_compare)(void *, int, int), void *sn_compare_data,
    rlc_tx_pdu_segment_t *list, rlc_tx_pdu_segment_t *pdu_segment)
{
  rlc_tx_pdu_segment_t head;
  rlc_tx_pdu_segment_t *cur;
  rlc_tx_pdu_segment_t *prev;

  head.next = list;
  cur = list;
  prev = &head;

  /* order is by 'sn', if 'sn' is the same then order is by 'so' */
  while (cur != NULL) {
    /* check if 'pdu_segment' is before 'cur' in the list */
    if (sn_compare(sn_compare_data, cur->sn, pdu_segment->sn) > 0 ||
        (cur->sn == pdu_segment->sn && cur->so > pdu_segment->so)) {
      break;
    }
    prev = cur;
    cur = cur->next;
  }
  prev->next = pdu_segment;
  pdu_segment->next = cur;
  return head.next;
}

/**************************************************************************/
/* PDU decoder                                                            */
/**************************************************************************/

void rlc_pdu_decoder_init(rlc_pdu_decoder_t *decoder, char *buffer, int size)
{
  decoder->error = 0;
  decoder->byte = 0;
  decoder->bit = 0;
  decoder->buffer = buffer;
  decoder->size = size;
}

static int get_bit(rlc_pdu_decoder_t *decoder)
{
  int ret;

  if (decoder->byte >= decoder->size) {
    decoder->error = 1;
    return 0;
  }

  ret = (decoder->buffer[decoder->byte] >> (7 - decoder->bit)) & 1;

  decoder->bit++;
  if (decoder->bit == 8) {
    decoder->bit = 0;
    decoder->byte++;
  }

  return ret;
}

int rlc_pdu_decoder_get_bits(rlc_pdu_decoder_t *decoder, int count)
{
  int ret = 0;
  int i;

  if (count > 31) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  for (i = 0; i < count; i++) {
    ret <<= 1;
    ret |= get_bit(decoder);
    if (decoder->error) return -1;
  }

  return ret;
}

void rlc_pdu_decoder_align(rlc_pdu_decoder_t *decoder)
{
  if (decoder->bit) {
    decoder->bit = 0;
    decoder->byte++;
  }
}

/**************************************************************************/
/* PDU encoder                                                            */
/**************************************************************************/

void rlc_pdu_encoder_init(rlc_pdu_encoder_t *encoder, char *buffer, int size)
{
  encoder->byte = 0;
  encoder->bit = 0;
  encoder->buffer = buffer;
  encoder->size = size;
}

static void put_bit(rlc_pdu_encoder_t *encoder, int bit)
{
  if (encoder->byte == encoder->size) {
    printf("%s:%d:%s: fatal, buffer full\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  encoder->buffer[encoder->byte] <<= 1;
  if (bit)
    encoder->buffer[encoder->byte] |= 1;

  encoder->bit++;
  if (encoder->bit == 8) {
    encoder->bit = 0;
    encoder->byte++;
  }
}

void rlc_pdu_encoder_put_bits(rlc_pdu_encoder_t *encoder, int value, int count)
{
  int i;
  int x;

  if (count > 31) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  x = 1 << (count - 1);
  for (i = 0; i < count; i++, x >>= 1)
    put_bit(encoder, value & x);
}

void rlc_pdu_encoder_align(rlc_pdu_encoder_t *encoder)
{
  while (encoder->bit)
    put_bit(encoder, 0);
}

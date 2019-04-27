#ifndef _RLC_PDU_H_
#define _RLC_PDU_H_

/**************************************************************************/
/* PDU segment and segment list                                           */
/**************************************************************************/

typedef struct {
  int sn;
  int so;
  int size;
  int is_last;
  char *data;
  int data_offset;
} rlc_pdu_segment_t;

typedef struct rlc_pdu_segment_list_t {
  struct rlc_pdu_segment_list_t *prev;
  struct rlc_pdu_segment_list_t *next;
  rlc_pdu_segment_t *s;
} rlc_pdu_segment_list_t;

rlc_pdu_segment_t *rlc_new_pdu_segment(int sn, int so, int size,
    int is_last, char *data, int data_offset);

void rlc_free_pdu_segment(rlc_pdu_segment_t *pdu_segment);

rlc_pdu_segment_list_t *rlc_pdu_segment_list_add(
    int (*sn_compare)(void *, int, int), void *sn_compare_data,
    rlc_pdu_segment_list_t *list, rlc_pdu_segment_t *pdu_segment);

/**************************************************************************/
/* PDU decoder                                                            */
/**************************************************************************/

typedef struct {
  int error;
  int byte;           /* next byte to decode */
  int bit;            /* next bit in next byte to decode */
  char *buffer;
  int size;
} rlc_pdu_decoder_t;

void rlc_pdu_decoder_init(rlc_pdu_decoder_t *decoder, char *buffer, int size);

#define rlc_pdu_decoder_in_error(d) ((d)->error == 1)

int rlc_pdu_decoder_get_bits(rlc_pdu_decoder_t *decoder, int count);

void rlc_pdu_decoder_align(rlc_pdu_decoder_t *decoder);

/**************************************************************************/
/* PDU encoder                                                            */
/**************************************************************************/

typedef struct {
  int byte;           /* next byte to encode */
  int bit;            /* next bit in next byte to encode */
  char *buffer;
  int size;
} rlc_pdu_encoder_t;

void rlc_pdu_encoder_init(rlc_pdu_encoder_t *encoder, char *buffer, int size);

void rlc_pdu_encoder_put_bits(rlc_pdu_encoder_t *encoder, int value, int count);

void rlc_pdu_encoder_align(rlc_pdu_encoder_t *encoder);

#endif /* _RLC_PDU_H_ */

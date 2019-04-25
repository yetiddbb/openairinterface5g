#ifndef _ASN1_UTILS_H_
#define _ASN1_UTILS_H_

int decode_t_reordering(int v);
int decode_t_status_prohibit(int v);
int decode_t_poll_retransmit(int v);
int decode_poll_pdu(int v);
int decode_poll_byte(int v);
int decode_max_retx_threshold(int v);

#endif /* _ASN1_UTILS_H_ */

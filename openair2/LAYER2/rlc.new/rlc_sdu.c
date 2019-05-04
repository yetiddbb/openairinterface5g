#include "rlc_sdu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rlc_sdu_t *rlc_new_sdu(char *buffer, int size, int upper_layer_id)
{
  rlc_sdu_t *ret = calloc(1, sizeof(rlc_sdu_t));
  if (ret == NULL)
    goto oom;

  ret->upper_layer_id = upper_layer_id;

  ret->data = malloc(size);
  if (ret->data == NULL)
    goto oom;

  memcpy(ret->data, buffer, size);

  ret->size = size;

  return ret;

oom:
  printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__,  __FUNCTION__);
  exit(1);
}

void rlc_free_sdu(rlc_sdu_t *sdu)
{
  free(sdu->data);
  free(sdu);
}

void rlc_sdu_list_add(rlc_sdu_t **list, rlc_sdu_t **end, rlc_sdu_t *sdu)
{
  if (*list == NULL) {
    *list = sdu;
    *end = sdu;
    return;
  }

  (*end)->next = sdu;
  *end = sdu;
}

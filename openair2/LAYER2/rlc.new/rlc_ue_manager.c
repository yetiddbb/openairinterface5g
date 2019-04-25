#include "rlc_ue_manager.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  pthread_mutex_t lock;
  rlc_ue_t        *ue_list;
  int             ue_count;
} rlc_ue_manager_internal_t;

rlc_ue_manager_t *new_rlc_ue_manager(void)
{
  rlc_ue_manager_internal_t *ret;

  ret = calloc(1, sizeof(rlc_ue_manager_internal_t));
  if (ret == NULL) {
    printf("out of memory\n");
    exit(1);
  }

  if (pthread_mutex_init(&ret->lock, NULL)) abort();

  return ret;
}

void rlc_manager_lock(rlc_ue_manager_t *_m)
{
  rlc_ue_manager_internal_t *m = _m;
  if (pthread_mutex_lock(&m->lock)) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

void rlc_manager_unlock(rlc_ue_manager_t *_m)
{
  rlc_ue_manager_internal_t *m = _m;
  if (pthread_mutex_unlock(&m->lock)) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

/* must be called with lock acquired */
rlc_ue_t *rlc_manager_get_ue(rlc_ue_manager_t *_m, int rnti)
{
  /* TODO: optimze */
  rlc_ue_manager_internal_t *m = _m;
  int i;

  for (i = 0; i < m->ue_count; i++)
    if (m->ue_list[i].rnti == rnti)
      return &m->ue_list[i];

  m->ue_count++;
  m->ue_list = realloc(m->ue_list, sizeof(rlc_ue_t) * m->ue_count);
  if (m->ue_list == NULL) {
    printf("%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  memset(&m->ue_list[m->ue_count-1], 0, sizeof(rlc_ue_t));

  m->ue_list[m->ue_count-1].rnti = rnti;

  return &m->ue_list[m->ue_count-1];
}

/* must be called with lock acquired */
void rlc_ue_add_srb_rlc_entity(rlc_ue_t *ue, int srb_id, rlc_entity_t *entity)
{
  if (srb_id < 0 || srb_id > 2) {
    printf("%s:%d:%s: fatal, bad srb id\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  srb_id--;

  if (ue->srb[srb_id] != NULL) {
    printf("%s:%d:%s: fatal, srb already present\n",
           __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ue->srb[srb_id] = entity;
}

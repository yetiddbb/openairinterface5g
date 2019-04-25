#ifndef _RLC_UE_MANAGER_H_
#define _RLC_UE_MANAGER_H_

#include "rlc_entity.h"

typedef void rlc_ue_manager_t;

typedef struct rlc_ue_t {
  int rnti;
  rlc_entity_t *srb[2];
  rlc_entity_t *drb[5];
} rlc_ue_t;

/***********************************************************************/
/* manager functions                                                   */
/***********************************************************************/

rlc_ue_manager_t *new_rlc_ue_manager(void);

void rlc_manager_lock(rlc_ue_manager_t *m);
void rlc_manager_unlock(rlc_ue_manager_t *m);

rlc_ue_t *rlc_manager_get_ue(rlc_ue_manager_t *m, int rnti);

/***********************************************************************/
/* ue functions                                                        */
/***********************************************************************/

void rlc_ue_add_srb_rlc_entity(rlc_ue_t *ue, int srb_id, rlc_entity_t *entity);

#endif /* _RLC_UE_MANAGER_H_ */

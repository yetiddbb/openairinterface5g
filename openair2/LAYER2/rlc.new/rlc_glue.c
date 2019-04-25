#include "rlc.h"
#include "asn1_utils.h"
#include "rlc_ue_manager.h"
#include "rlc_entity.h"

/* remove include/defines */
#include <stdio.h>
#define RED "\x1b[31m"
#define RESET "\x1b[m"

static rlc_ue_manager_t *rlc_ue_manager;

void mac_rlc_data_ind     (
  const module_id_t         module_idP,
  const rnti_t              rntiP,
  const eNB_index_t         eNB_index,
  const frame_t             frameP,
  const eNB_flag_t          enb_flagP,
  const MBMS_flag_t         MBMS_flagP,
  const logical_chan_id_t   channel_idP,
  char                     *buffer_pP,
  const tb_size_t           tb_sizeP,
  num_tb_t                  num_tbP,
  crc_t                    *crcs_pP)
{
  rlc_ue_t *ue;

  if (module_idP != 0 || eNB_index != 0 || enb_flagP != 1 || MBMS_flagP != 0) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  if (channel_idP != 1) { printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__); exit(1); }

  rlc_manager_lock(rlc_ue_manager);
  ue = rlc_manager_get_ue(rlc_ue_manager, rntiP);
  ue->srb[0]->recv(ue->srb[0], buffer_pP, tb_sizeP);
  rlc_manager_unlock(rlc_ue_manager);
}

tbs_size_t mac_rlc_data_req(
  const module_id_t       module_idP,
  const rnti_t            rntiP,
  const eNB_index_t       eNB_index,
  const frame_t           frameP,
  const eNB_flag_t        enb_flagP,
  const MBMS_flag_t       MBMS_flagP,
  const logical_chan_id_t channel_idP,
  const tb_size_t         tb_sizeP,
  char             *buffer_pP
#if (LTE_RRC_VERSION >= MAKE_VERSION(14, 0, 0))
  ,const uint32_t sourceL2Id
  ,const uint32_t destinationL2Id
#endif
   )
{
  printf(RED "%s" RESET "\n", __FUNCTION__); return 0;
}

mac_rlc_status_resp_t mac_rlc_status_ind(
  const module_id_t       module_idP,
  const rnti_t            rntiP,
  const eNB_index_t       eNB_index,
  const frame_t           frameP,
  const sub_frame_t       subframeP,
  const eNB_flag_t        enb_flagP,
  const MBMS_flag_t       MBMS_flagP,
  const logical_chan_id_t channel_idP,
  const tb_size_t         tb_sizeP
#if (LTE_RRC_VERSION >= MAKE_VERSION(14, 0, 0))
  ,const uint32_t sourceL2Id
  ,const uint32_t destinationL2Id
#endif
  )
{
  mac_rlc_status_resp_t ret;
  ret.bytes_in_buffer = 0;
  ret.pdus_in_buffer = 0;
  ret.head_sdu_creation_time = 0;
  ret.head_sdu_remaining_size_to_send = 0;
  ret.head_sdu_is_segmented = 0;
  printf(RED "%s" RESET "\n", __FUNCTION__);
  return ret;
}

int oai_emulation;

rlc_op_status_t rlc_data_req     (const protocol_ctxt_t *const ctxt_pP,
                                  const srb_flag_t   srb_flagP,
                                  const MBMS_flag_t  MBMS_flagP,
                                  const rb_id_t      rb_idP,
                                  const mui_t        muiP,
                                  confirm_t    confirmP,
                                  sdu_size_t   sdu_sizeP,
                                  mem_block_t *sdu_pP
#if (LTE_RRC_VERSION >= MAKE_VERSION(14, 0, 0))
  ,const uint32_t *const sourceL2Id
  ,const uint32_t *const destinationL2Id
#endif
                                 )
{
  printf(RED "%s" RESET "\n", __FUNCTION__); return 0;
}

int rlc_module_init(void)
{
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static int inited = 0;

  if (pthread_mutex_lock(&lock)) abort();

  if (inited) {
    printf("%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  inited = 1;

  rlc_ue_manager = new_rlc_ue_manager();

  if (pthread_mutex_unlock(&lock)) abort();

  return 0;
}

void rlc_util_print_hex_octets(comp_name_t componentP, unsigned char *dataP, const signed long sizeP)
{
  printf(RED "%s" RESET "\n", __FUNCTION__);
}

static void add_srb(int rnti, struct LTE_SRB_ToAddMod *s)
{
  rlc_entity_t            *rlc_am;
  rlc_ue_t                *ue;

  struct LTE_SRB_ToAddMod__rlc_Config *r = s->rlc_Config;
  struct LTE_SRB_ToAddMod__logicalChannelConfig *l = s->logicalChannelConfig;
  int srb_id = s->srb_Identity;
  int logical_channel_group;

  int t_reordering;
  int t_status_prohibit;
  int t_poll_retransmit;
  int poll_pdu;
  int poll_byte;
  int max_retx_threshold;

  switch (l->present) {
  case LTE_SRB_ToAddMod__logicalChannelConfig_PR_explicitValue:
    logical_channel_group = *l->choice.explicitValue.ul_SpecificParameters->logicalChannelGroup;
    break;
  case LTE_SRB_ToAddMod__logicalChannelConfig_PR_defaultValue:
    /* default value from 36.331 9.2.1 */
    logical_channel_group = 0;
    break;
  default:
    printf("%s:%d:%s: fatal error\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  /* TODO: accept other values? */
  if (logical_channel_group != 0) {
    printf("%s:%d:%s: fatal error\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  switch (r->present) {
  case LTE_SRB_ToAddMod__rlc_Config_PR_explicitValue: {
    struct LTE_RLC_Config__am *am;
    if (r->choice.explicitValue.present != LTE_RLC_Config_PR_am) {
      printf("%s:%d:%s: fatal error, must be RLC AM\n",
             __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }
    am = &r->choice.explicitValue.choice.am;
    t_reordering       = decode_t_reordering(am->dl_AM_RLC.t_Reordering);
    t_status_prohibit  = decode_t_status_prohibit(am->dl_AM_RLC.t_StatusProhibit);
    t_poll_retransmit  = decode_t_poll_retransmit(am->ul_AM_RLC.t_PollRetransmit);
    poll_pdu           = decode_poll_pdu(am->ul_AM_RLC.pollPDU);
    poll_byte          = decode_poll_byte(am->ul_AM_RLC.pollByte);
    max_retx_threshold = decode_max_retx_threshold(am->ul_AM_RLC.maxRetxThreshold);
    break;
  }
  case LTE_SRB_ToAddMod__rlc_Config_PR_defaultValue:
    /* default values from 36.331 9.2.1 */
    t_reordering       = 35;
    t_status_prohibit  = 0;
    t_poll_retransmit  = 45;
    poll_pdu           = -1;
    poll_byte          = -1;
    max_retx_threshold = 4;
    break;
  default:
    printf("%s:%d:%s: fatal error\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  rlc_am = new_rlc_entity_am(t_reordering, t_status_prohibit, t_poll_retransmit,
                             poll_pdu, poll_byte, max_retx_threshold);
  rlc_manager_lock(rlc_ue_manager);
  ue = rlc_manager_get_ue(rlc_ue_manager, rnti);
  rlc_ue_add_srb_rlc_entity(ue, srb_id, rlc_am);
  rlc_manager_unlock(rlc_ue_manager);
}

rlc_op_status_t rrc_rlc_config_asn1_req (const protocol_ctxt_t   * const ctxt_pP,
    const LTE_SRB_ToAddModList_t   * const srb2add_listP,
    const LTE_DRB_ToAddModList_t   * const drb2add_listP,
    const LTE_DRB_ToReleaseList_t  * const drb2release_listP
#if (LTE_RRC_VERSION >= MAKE_VERSION(9, 0, 0))
    ,const LTE_PMCH_InfoList_r9_t * const pmch_InfoList_r9_pP
    ,const uint32_t sourceL2Id
    ,const uint32_t destinationL2Id
#endif
                                        )
{
  int rnti = ctxt_pP->rnti;
  int i;

  if (ctxt_pP->enb_flag != 1 || ctxt_pP->module_id != 0 /*||
      ctxt_pP->instance != 0 || ctxt_pP->eNB_index != 0 ||
      ctxt_pP->configured != 1 || ctxt_pP->brOption != 0 */) {
    printf("%s: ctxt_pP not handled (%d %d %d %d %d %d)\n", __FUNCTION__,
ctxt_pP->enb_flag , ctxt_pP->module_id, ctxt_pP->instance, ctxt_pP->eNB_index, ctxt_pP->configured, ctxt_pP->brOption);
    exit(1);
  }

  if (pmch_InfoList_r9_pP != NULL) {
    printf("%s: pmch_InfoList_r9_pP not handled\n", __FUNCTION__);
    exit(1);
  }

  if (drb2release_listP != NULL) {
    printf("%s:%d:%s: TODO\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  if (srb2add_listP != NULL) {
    for (i = 0; i < srb2add_listP->list.count; i++) {
      add_srb(rnti, srb2add_listP->list.array[i]);
    }
  }

  if (drb2add_listP != NULL) {
    printf("%s:%d:%s: TODO\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  return RLC_OP_STATUS_OK;
}

rlc_op_status_t rrc_rlc_config_req   (
  const protocol_ctxt_t* const ctxt_pP,
  const srb_flag_t      srb_flagP,
  const MBMS_flag_t     mbms_flagP,
  const config_action_t actionP,
  const rb_id_t         rb_idP,
  const rlc_info_t      rlc_infoP)
{
  printf(RED "%s" RESET "\n", __FUNCTION__); return 0;
}

void rrc_rlc_register_rrc (rrc_data_ind_cb_t rrc_data_indP, rrc_data_conf_cb_t rrc_data_confP)
{
  /* nothing to do */
}

rlc_op_status_t rrc_rlc_remove_ue (const protocol_ctxt_t* const x)
{
  printf(RED "%s" RESET "\n", __FUNCTION__); return 0;
}


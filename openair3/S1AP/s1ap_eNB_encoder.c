/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file s1ap_eNB_encoder.c
 * \brief s1ap pdu encode procedures for eNB
 * \author Sebastien ROUX and Navid Nikaein
 * \email navid.nikaein@eurecom.fr
 * \date 2013 - 2015
 * \version 0.1
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "assertions.h"
#include "conversions.h"
#include "intertask_interface.h"
#include "s1ap_common.h"
#include "s1ap_eNB_encoder.h"

static inline int s1ap_eNB_encode_initiating(S1AP_S1AP_PDU_t *pdu,
    uint8_t **buffer,
    uint32_t *len);

static inline int s1ap_eNB_encode_successfull_outcome(S1AP_S1AP_PDU_t *pdu,
    uint8_t **buffer, uint32_t *len);

static inline int s1ap_eNB_encode_unsuccessfull_outcome(S1AP_S1AP_PDU_t *pdu,
    uint8_t **buffer, uint32_t *len);

int s1ap_eNB_encode_pdu(S1AP_S1AP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  int ret = -1;
  DevAssert(pdu != NULL);
  DevAssert(buffer != NULL);
  DevAssert(len != NULL);

  switch(pdu->present) {
    case S1AP_S1AP_PDU_PR_initiatingMessage:
      ret = s1ap_eNB_encode_initiating(pdu, buffer, len);
      break;

    case S1AP_S1AP_PDU_PR_successfulOutcome:
      ret = s1ap_eNB_encode_successfull_outcome(pdu, buffer, len);
      break;

    case S1AP_S1AP_PDU_PR_unsuccessfulOutcome:
      ret = s1ap_eNB_encode_unsuccessfull_outcome(pdu, buffer, len);
      break;

    default:
      S1AP_DEBUG("Unknown message outcome (%d) or not implemented",
                 (int)pdu->present);
      return -1;
  }

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_S1AP_S1AP_PDU, pdu);
  return ret;
}

static inline
int s1ap_eNB_encode_initiating(S1AP_S1AP_PDU_t *pdu,
                               uint8_t **buffer, uint32_t *len)
{
  MessageDef *message_p;
  MessagesIds message_id;
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.initiatingMessage.procedureCode) {
    case S1AP_ProcedureCode_id_S1Setup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_S1_SETUP_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_s1_setup_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_s1_setup_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_uplinkNASTransport:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_UPLINK_NAS_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_uplink_nas_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_uplink_nas_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_UECapabilityInfoIndication:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_UE_CAPABILITY_IND_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_ue_capability_ind_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_ue_capability_ind_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_initialUEMessage:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_INITIAL_UE_MESSAGE_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_initial_ue_message_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_initial_ue_message_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_NASNonDeliveryIndication:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_NAS_NON_DELIVERY_IND_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_nas_non_delivery_ind_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_nas_non_delivery_ind_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_UEContextReleaseRequest:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_UE_CONTEXT_RELEASE_REQ_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_ue_context_release_req_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_ue_context_release_req_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    default:
      S1AP_DEBUG("Unknown procedure ID (%d) for initiating message\n",
                 (int)pdu->choice.initiatingMessage.procedureCode);
      return -1;
  }

  if (asn1_xer_print) {
    xer_fprint(stdout, &asn_DEF_S1AP_S1AP_PDU, (void *)pdu);
  }

  memset(&res, 0, sizeof(res));
  res = asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_S1AP_S1AP_PDU, pdu);
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

static inline
int s1ap_eNB_encode_successfull_outcome(S1AP_S1AP_PDU_t *pdu,
                                        uint8_t **buffer, uint32_t *len)
{
  MessageDef *message_p;
  MessagesIds message_id;
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.successfulOutcome.procedureCode) {
    case S1AP_ProcedureCode_id_InitialContextSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_INITIAL_CONTEXT_SETUP_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_initial_context_setup_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_initial_context_setup_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_UEContextRelease:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_UE_CONTEXT_RELEASE_COMPLETE_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_ue_context_release_complete_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_ue_context_release_complete_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    case S1AP_ProcedureCode_id_E_RABSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_E_RAB_SETUP_RESPONSE_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_e_rab_setup_response_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_e_rab_setup_response_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      S1AP_INFO("E_RABSetup successful message\n");
      break;

    case S1AP_ProcedureCode_id_E_RABModify:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_E_RAB_MODIFY_RESPONSE_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_e_rab_modify_response_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_e_rab_modify_response_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      S1AP_INFO("E_RABModify successful message\n");
      break;

    case S1AP_ProcedureCode_id_E_RABRelease:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_E_RAB_RELEASE_RESPONSE_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_e_rab_release_response_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_e_rab_release_response_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      S1AP_INFO("E_RAB Release successful message\n");
      break;

    default:
      S1AP_WARN("Unknown procedure ID (%d) for successfull outcome message\n",
                (int)pdu->choice.successfulOutcome.procedureCode);
      return -1;
  }

  if (asn1_xer_print) {
    xer_fprint(stdout, &asn_DEF_S1AP_S1AP_PDU, (void *)pdu);
  }

  memset(&res, 0, sizeof(res));
  res = asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_S1AP_S1AP_PDU, pdu);
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

static inline
int s1ap_eNB_encode_unsuccessfull_outcome(S1AP_S1AP_PDU_t *pdu,
    uint8_t **buffer, uint32_t *len)
{
  MessageDef *message_p;
  MessagesIds message_id;
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.unsuccessfulOutcome.procedureCode) {
    case S1AP_ProcedureCode_id_InitialContextSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_S1AP_S1AP_PDU, pdu);
      message_id = S1AP_INITIAL_CONTEXT_SETUP_LOG;
      message_p = itti_alloc_new_message_sized(TASK_S1AP, message_id, res.result.encoded + sizeof (IttiMsgText));
      message_p->ittiMsg.s1ap_initial_context_setup_log.size = res.result.encoded;
      memcpy(&message_p->ittiMsg.s1ap_initial_context_setup_log.text, res.buffer, res.result.encoded);
      itti_send_msg_to_task(TASK_UNKNOWN, INSTANCE_DEFAULT, message_p);
      free(res.buffer);
      break;

    default:
      S1AP_DEBUG("Unknown procedure ID (%d) for unsuccessfull outcome message\n",
                 (int)pdu->choice.unsuccessfulOutcome.procedureCode);
      return -1;
  }

  if (asn1_xer_print) {
    xer_fprint(stdout, &asn_DEF_S1AP_S1AP_PDU, (void *)pdu);
  }

  memset(&res, 0, sizeof(res));
  res = asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_S1AP_S1AP_PDU, pdu);
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
   included in this distribution in the file called "COPYING". If not,
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Compus SophiaTech 450, route des chappes, 06451 Biot, France.

 *******************************************************************************/

/*! \file enb_agent.h
 * \brief top level enb agent receive thread and itti task
 * \author Navid Nikaein and Xenofon Foukas
 * \date 2016
 * \version 0.1
 */
#include "proto_agent_common.h"
#include "common/utils/LOG/log.h"
#include "proto_agent.h"
#include "assertions.h"
#include "proto_agent_net_comm.h"
#include "proto_agent_async.h" 

#define  ENB_AGENT_MAX 9

proto_agent_instance_t proto_agent[MAX_DU];
proto_agent_instance_t proto_server[MAX_DU];

pthread_t new_thread(void *(*f)(void *), void *b);

Protocol__FlexsplitMessage *proto_agent_timeout_fsp(void* args);

proto_agent_async_channel_t *client_channel[MAX_DU], *server_channel;

#define TEST_MOD 0

#define ECHO

/*  Server side function; upon a new connection 
    reception, sends the hello packets
*/
int proto_server_start(mod_id_t mod_id, const cu_params_t *cu)
{
  int channel_id;

  DevAssert(cu->local_interface);
  DevAssert(cu->local_ipv4_address);
  DevAssert(cu->local_port > 1024); // "unprivileged" port
  
  proto_server[mod_id].mod_id = mod_id;

  /* Initialize the channel container */

  /* TODO only initialize the first time */
  proto_agent_init_channel_container();

  /*Create the async channel info*/
  proto_agent_async_channel_t *channel_info;
  channel_info = proto_server_async_channel_info(mod_id, cu->local_ipv4_address, cu->local_port);
  
  server_channel = channel_info;

  /* Create a channel using the async channel info */
  channel_id = proto_agent_create_channel((void *) channel_info,
					proto_agent_async_msg_send,
					proto_agent_async_msg_recv,
					proto_agent_async_release);
  if (channel_id <= 0) goto error;

  proto_agent_channel_t *channel = proto_agent_get_channel(channel_id);
  if (!channel) goto error;
  proto_server[mod_id].channel = channel;

  /* Register the channel for all underlying agents (use ENB_AGENT_MAX) */
  proto_agent_register_channel(mod_id, channel, ENB_AGENT_MAX);

  // Code for sending the HELLO/ECHO_REQ message once a connection is established
  //uint8_t *msg = NULL;
  //Protocol__FlexsplitMessage *init_msg=NULL;

  //if (udp == 0)
  //{
  //  // If the comm is not UDP, allow the server to send the first packet over the channel
  //  //printf( "Proto agent Server: Calling the echo_request packet constructor\n");
  //  msg_flag = proto_agent_echo_request(mod_id, NULL, &init_msg);
  //  if (msg_flag != 0)
  //  goto error;
  //
  //  int msgsize = 0;
  //  if (init_msg != NULL)
  //    msg = proto_agent_pack_message(init_msg, &msgsize);

  //  if (msg!= NULL)
  //  {
  //    LOG_D(PROTO_AGENT, "Server sending the message over the async channel\n");
  //    proto_agent_async_msg_send((void *)msg, (int) msgsize, 1, (void *) channel_info);
  //  }
  //  /* After sending the message, wait for any replies;
  //    the server thread blocks until it reads any data
  //    over the channel
  //  */

  //}

  proto_server[mod_id].recv_thread = new_thread(proto_server_receive, &proto_server[mod_id]);

  return 0;

error:
  LOG_E(PROTO_AGENT, "there was an error\n");
  return 1;

}

int proto_agent_start(mod_id_t mod_id, const du_params_t *du)
{
  int channel_id;

  DevAssert(du->remote_ipv4_address);
  DevAssert(du->remote_port > 1024); // "unprivileged" port
  
  proto_agent[mod_id].mod_id = mod_id;
  
  /* TODO only initialize the first time */
  proto_agent_init_channel_container();

  /*Create the async channel info*/
  
  proto_agent_async_channel_t *channel_info = proto_agent_async_channel_info(mod_id, du->remote_ipv4_address, du->remote_port);
  if (!channel_info) goto error;

  /*Create a channel using the async channel info*/
  channel_id = proto_agent_create_channel((void *) channel_info, 
					proto_agent_async_msg_send, 
					proto_agent_async_msg_recv,
					proto_agent_async_release);
  if (channel_id <= 0) goto error;

  proto_agent_channel_t *channel = proto_agent_get_channel(channel_id);
  if (!channel) goto error;
  proto_agent[mod_id].channel = channel;

  /* Register the channel for all underlying agents (use ENB_AGENT_MAX) */
  proto_agent_register_channel(mod_id, channel, ENB_AGENT_MAX);

  //uint8_t *msg = NULL;
  //Protocol__FlexsplitMessage *init_msg=NULL;
  //int msg_flag;
  
  // In the case of UDP comm, start the echo request from the client side; the server thread should be blocked until it reads the SRC port of the 1st packet
  //if (udp == 1)
  //{
  //  msg_flag = proto_agent_echo_request(cu_id, NULL, &init_msg);



  //  if (msg_flag != 0)
  //    goto error;
  //
  //  int msgsize = 0;
  //  if (init_msg != NULL)
  //    msg = proto_agent_pack_message(init_msg, &msgsize);

  //  if (msg!= NULL)
  //  {
  //    LOG_D(PROTO_AGENT, "Client sending the ECHO_REQUEST message over the async channel\n");
  //    proto_agent_async_msg_send((void *)msg, (int) msgsize, 1, (void *) channel_info);
  //  }
  //}
  //
  /* After sending the message, wait for any replies; 
     the server thread blocks until it reads any data 
     over the channel
  */
  proto_agent[mod_id].recv_thread = new_thread(proto_client_receive, &proto_agent[mod_id]);
  return 0;

error:
  LOG_E(PROTO_AGENT, "there was an error in proto_agent_start()\n");
  return 1;

}

//void
//proto_agent_send_hello(void)
//{
//  uint8_t *msg = NULL;
//  Protocol__FlexsplitMessage *init_msg=NULL;
//  int msg_flag = 0;
//
//
//  //printf( "PDCP agent: Calling the HELLO packet constructor\n");
//  msg_flag = proto_agent_hello(proto_agent[TEST_MOD].mod_id, NULL, &init_msg);
//
//  int msgsize = 0;
//  if (msg_flag == 0)
//  {
//    proto_agent_serialize_message(init_msg, &msg, &msgsize);
//  }
//
//  LOG_D(PROTO_AGENT, "Agent sending the message over the async channel\n");
//  proto_agent_async_msg_send((void *)msg, (int) msgsize, 1, (void *) client_channel[TEST_MOD]);
//}


void
proto_agent_send_rlc_data_req(uint8_t mod_id, uint8_t type_id, const protocol_ctxt_t* const ctxt_pP, const srb_flag_t srb_flagP, const MBMS_flag_t MBMS_flagP, const rb_id_t rb_idP, const mui_t muiP, 
			      confirm_t confirmP, sdu_size_t sdu_sizeP, mem_block_t *sdu_pP)
{
  
  //LOG_D(PROTO_AGENT, "PROTOPDCP: sending the data req over the async channel\n");
  
  uint8_t *msg = NULL;
  Protocol__FlexsplitMessage *init_msg=NULL;

  int msg_flag = 0;
  
  //printf( "PDCP agent: Calling the PDCP DATA REQ constructor\n");
 
  data_req_args *args = malloc(sizeof(data_req_args));
  
  args->ctxt = malloc(sizeof(protocol_ctxt_t));
  memcpy(args->ctxt, ctxt_pP, sizeof(protocol_ctxt_t));
  args->srb_flag = srb_flagP;
  args->MBMS_flag = MBMS_flagP;
  args->rb_id = rb_idP;
  args->mui = muiP;
  args->confirm = confirmP;
  args->sdu_size = sdu_sizeP;
  args->sdu_p = malloc(sdu_sizeP);
  memcpy(args->sdu_p, sdu_pP->data, sdu_sizeP);

  msg_flag = proto_agent_pdcp_data_req(type_id, (void *) args, &init_msg);
  if (msg_flag != 0)
    goto error;
  
  int msgsize = 0;
  if (init_msg != NULL)
  {
    
    msg = proto_agent_pack_message(init_msg, &msgsize);
  
    
    LOG_D(PROTO_AGENT, "Sending the pdcp data_req message over the async channel\n");
  
    if (msg!=NULL)
      proto_agent_async_msg_send((void *)msg, (int) msgsize, 1, (void *) client_channel[mod_id]);
  
  }
  else
  {
    goto error; 
  }
  
  return;
error:
  LOG_E(PROTO_AGENT, "PROTO_AGENT there was an error\n");
  return;
  
}

  
void
proto_agent_send_pdcp_data_ind(const protocol_ctxt_t* const ctxt_pP, const srb_flag_t srb_flagP,
			       const MBMS_flag_t MBMS_flagP, const rb_id_t rb_idP, sdu_size_t sdu_sizeP, mem_block_t *sdu_pP)
{
  //LOG_D(PROTO_AGENT, "PROTOPDCP: Sending Data Indication over the async channel\n");
  
  uint8_t *msg = NULL;
  Protocol__FlexsplitMessage *init_msg = NULL;

  
  int msg_flag = 0;
  
  //printf( "PDCP agent: Calling the PDCP_DATA_IND constructor\n");
 
  data_req_args *args = malloc(sizeof(data_req_args));
  
  args->ctxt = malloc(sizeof(protocol_ctxt_t));
  memcpy(args->ctxt, ctxt_pP, sizeof(protocol_ctxt_t));
  args->srb_flag = srb_flagP;
  args->MBMS_flag = MBMS_flagP;
  args->rb_id = rb_idP;
  args->sdu_size = sdu_sizeP;
  args->sdu_p = malloc(sdu_sizeP);
  memcpy(args->sdu_p, sdu_pP->data, sdu_sizeP);

  AssertFatal(0, "need mod_id here\n");
  msg_flag = proto_agent_pdcp_data_ind(proto_server[0].mod_id, (void *) args, &init_msg);
  if (msg_flag != 0)
    goto error;

  int msgsize = 0;
  
  if (init_msg != NULL)
  {
    msg = proto_agent_pack_message(init_msg, &msgsize);
    
    if (msg!=NULL)
    {
      LOG_D(PROTO_AGENT, "Sending the pdcp data_ind message over the async channel\n");
      proto_agent_async_msg_send((void *)msg, (int) msgsize, 1, (void *) server_channel);
    }
  }
  else
  {
    goto error; 
  }
  return;

error:
  LOG_E(PROTO_AGENT, "there was an error\n");
  return;
  
}

  


void *
proto_server_receive(void *args)
{
  proto_agent_instance_t *d = args;
  void                  *data = NULL;
  int                   size;
  int                   priority;
  err_code_t             err_code;

  Protocol__FlexsplitMessage *msg;
  uint8_t *ser_msg;
  
  while (1) {
   
    msg = NULL;
    ser_msg = NULL;
    
    if (proto_agent_async_msg_recv(&data, &size, &priority, server_channel)){
      err_code = PROTOCOL__FLEXSPLIT_ERR__MSG_ENQUEUING;
      goto error;
    }

    LOG_D(PROTO_AGENT, "Server side Received message with size %d and priority %d, calling message handle\n", size, priority);

    msg=proto_agent_handle_message(d->mod_id, data, size);

    if (msg == NULL)
    {
        LOG_D(PROTO_AGENT, "msg to send back is NULL\n");
    }
    else
    {
        ser_msg = proto_agent_pack_message(msg, &size);
    }
  
    LOG_D(PROTO_AGENT, "Server sending the reply message over the async channel\n");
    if (ser_msg != NULL){
      if (proto_agent_async_msg_send((void *)ser_msg, (int) size, 1, (void *) server_channel)){
	err_code = PROTOCOL__FLEXSPLIT_ERR__MSG_ENQUEUING;
	goto error;
      }
      LOG_D(PROTO_AGENT, "sent message with size %d\n", size);
    }

  }
  
  return NULL;

error:
  LOG_E(PROTO_AGENT, "server_receive_thread: error %d occured\n",err_code);
  return NULL;
  
}
  
void *
proto_client_receive(void *args)
{
  AssertFatal(0, "check proto_client_receive\n");
  mod_id_t 	      recv_mod = 0;

  LOG_D(PROTO_AGENT, "\n\nrecv mod is %u\n\n",recv_mod);  
  //proto_agent_instance_t         *d = &proto_agent[TEST_MOD];
  void                  *data = NULL;
  int                   size;
  int                   priority;
  err_code_t             err_code;

  Protocol__FlexsplitMessage *msg;
  uint8_t *ser_msg;


  while (1) {
      
    msg = NULL;
    ser_msg = NULL;

    while(client_channel[recv_mod] == NULL)
    {
	//just wait
    }
    LOG_D(PROTO_AGENT, "Will receive packets\n");
    if (proto_agent_async_msg_recv(&data, &size, &priority, client_channel[recv_mod])){
      err_code = PROTOCOL__FLEXSPLIT_ERR__MSG_ENQUEUING;
      goto error;
    }

    LOG_D(PROTO_AGENT, "Client Received message with size %d and priority %d, calling message handle with mod_id %u\n", size, priority, recv_mod);

    msg=proto_agent_handle_message(recv_mod, data, size);

    if (msg == NULL)
    {
        LOG_D(PROTO_AGENT, "msg to send back is NULL\n");
    }
    else
    {
      ser_msg = proto_agent_pack_message(msg, &size);
    }
    LOG_D(PROTO_AGENT, "Server sending the reply message over the async channel\n");

    if (ser_msg != NULL){
      if (proto_agent_async_msg_send((void *)ser_msg, (int) size, 1, (void *) client_channel[recv_mod])){
	err_code = PROTOCOL__FLEXSPLIT_ERR__MSG_ENQUEUING;
	goto error;
      }
      LOG_D(PROTO_AGENT, "sent message with size %d\n", size);
    }
   
  }
  
  return NULL;

error:
  LOG_E(PROTO_AGENT, " client_receive_thread: error %d occured\n",err_code);
  return NULL;
  
}

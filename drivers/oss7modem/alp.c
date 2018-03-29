/*! \file alp.c
 *

 *  \copyright (C) Copyright 2015 University of Antwerp and others (http://oss-7.cosys.be)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  \author glenn.ergeerts@uantwerpen.be
 *  \author maarten.weyn@uantwerpen.be
 *
 */

#include "stdlib.h"
#include <stdio.h>

#include "alp.h"
//#include "fs.h"
#include "fifo.h"
#include "log.h"
#include "types.h"
#include <assert.h>

#define SUCCESS 0

#define DPRINT(...) printf(__VA_ARGS__)


alp_operation_t alp_get_operation(uint8_t* alp_command)
{
    alp_control_t alp_ctrl;
    alp_ctrl.raw = (*alp_command);
    return alp_ctrl.operation;
}

static uint32_t parse_length_operand(fifo_t* cmd_fifo) {
  uint8_t len = 0;
  fifo_pop(cmd_fifo, (uint8_t*)&len, 1);
  uint8_t field_len = len >> 6;
  if(field_len == 0)
    return (uint32_t)len;

  uint32_t full_length = (len & 0x3F) << ( 8 * field_len); // mask field length specificier bits and shift before adding other length bytes
  fifo_pop(cmd_fifo, (uint8_t*)&full_length, field_len);
  return full_length;
}

void alp_append_length_operand(fifo_t* fifo, uint32_t length) {
  if(length < 64) {
    // can be coded in one byte
    assert(fifo_put(fifo, (uint8_t*)&length, 1) == SUCCESS);
    return;
  }

  uint8_t size = 1;
  if(length > 0x3FFF)
    size = 2;
  if(length > 0x3FFFFF)
    size = 3;

  uint8_t byte = 0;
  byte += (size << 6); // length specifier bits
  byte += ((uint8_t*)(&length))[size];
  assert(fifo_put(fifo, &byte, 1) == SUCCESS);
  do {
    size--;
    assert(fifo_put(fifo, (uint8_t*)&length + size, 1) == SUCCESS);
  } while(size > 0);
}


static alp_operand_file_offset_t parse_file_offset_operand(fifo_t* cmd_fifo) {
  alp_operand_file_offset_t operand;
  error_t err = fifo_pop(cmd_fifo, &operand.file_id, 1); assert(err == SUCCESS);
  operand.offset = parse_length_operand(cmd_fifo);
  return operand;
}

void alp_append_file_offset_operand(fifo_t* fifo, uint8_t file_id, uint32_t offset) {
  assert(fifo_put_byte(fifo, file_id) == SUCCESS);
  alp_append_length_operand(fifo, offset);
}

/*
void alp_append_forward_action(fifo_t* fifo, d7ap_master_session_config_t* session_config) {
  assert(session_config);
  assert(fifo_put_byte(fifo, ALP_OP_FORWARD) == SUCCESS);
  assert(fifo_put_byte(fifo, ALP_ITF_ID_D7ASP) == SUCCESS);
  assert(fifo_put_byte(fifo, session_config->qos.raw) == SUCCESS);
  assert(fifo_put_byte(fifo, session_config->dormant_timeout) == SUCCESS);
  assert(fifo_put_byte(fifo, session_config->addressee.ctrl.raw) == SUCCESS);
  uint8_t id_length = alp_addressee_id_length(session_config->addressee.ctrl.id_type);
  assert(fifo_put_byte(fifo, session_config->addressee.access_class) == SUCCESS);
  assert(fifo_put(fifo, session_config->addressee.id, id_length) == SUCCESS);
  DPRINT("FORWARD");
}
 */

void alp_append_return_file_data_action(fifo_t* fifo, uint8_t file_id, uint32_t offset, uint32_t length, uint8_t* data) {
  assert(fifo_put_byte(fifo, ALP_OP_RETURN_FILE_DATA) == SUCCESS);
  assert(fifo_put_byte(fifo, file_id) == SUCCESS);
  alp_append_length_operand(fifo, offset);
  alp_append_length_operand(fifo, length);
  assert(fifo_put(fifo, data, length) == SUCCESS);
}

/*
static void append_tag_response(fifo_t* fifo, uint8_t tag_id, bool eop, bool error) {
  // fill response with tag response
  uint8_t op_return_tag = ALP_OP_RETURN_TAG | (eop << 7);
  op_return_tag |= (error << 6);
  error_t err = fifo_put_byte(fifo, op_return_tag); assert(err == SUCCESS);
  err = fifo_put_byte(fifo, tag_id); assert(err == SUCCESS);
}
 */

/*
static void add_interface_status_action(fifo_t* alp_response_fifo, d7ap_session_result_t* d7asp_result)
{
  fifo_put_byte(alp_response_fifo, ALP_OP_RETURN_STATUS + (1 << 6));
  fifo_put_byte(alp_response_fifo, ALP_ITF_ID_D7ASP);
  fifo_put_byte(alp_response_fifo, d7asp_result->channel.channel_header_raw);
  uint16_t center_freq_index_be = __builtin_bswap16(d7asp_result->channel.center_freq_index);
  fifo_put(alp_response_fifo, (uint8_t*)&center_freq_index_be, 2);
  fifo_put_byte(alp_response_fifo, d7asp_result->rx_level);
  fifo_put_byte(alp_response_fifo, d7asp_result->link_budget);
  fifo_put_byte(alp_response_fifo, d7asp_result->target_rx_level);
  fifo_put_byte(alp_response_fifo, d7asp_result->status.raw);
  fifo_put_byte(alp_response_fifo, d7asp_result->fifo_token);
  fifo_put_byte(alp_response_fifo, d7asp_result->seqnr);
  fifo_put_byte(alp_response_fifo, d7asp_result->response_to);
  fifo_put_byte(alp_response_fifo, d7asp_result->addressee.ctrl.raw);
  fifo_put_byte(alp_response_fifo, d7asp_result->addressee.access_class);
  uint8_t address_len = alp_addressee_id_length(d7asp_result->addressee.ctrl.id_type);
  fifo_put(alp_response_fifo, d7asp_result->addressee.id, address_len);
}
*/


uint8_t alp_addressee_id_length(d7ap_addressee_id_type_t id_type)
{
    switch(id_type)
    {
        case ID_TYPE_NOID:
          return ID_TYPE_NOID_ID_LENGTH;
        case ID_TYPE_NBID:
          return ID_TYPE_NBID_ID_LENGTH;
        case ID_TYPE_UID:
          return ID_TYPE_UID_ID_LENGTH;
        case ID_TYPE_VID:
          return ID_TYPE_VID_LENGTH;
        default:
          assert(false);
    }
}

static void parse_op_return_file_data(fifo_t* fifo, alp_action_t* action) {
  action->file_data_operand.file_offset = parse_file_offset_operand(fifo);
  action->file_data_operand.provided_data_length = parse_length_operand(fifo);
  assert(action->file_data_operand.provided_data_length <= sizeof(action->file_data_operand.data));
  fifo_pop(fifo, action->file_data_operand.data, action->file_data_operand.provided_data_length);
  log_print_string("parsed file data file %i, len %i", action->file_data_operand.file_offset.file_id, action->file_data_operand.provided_data_length);
}


static void parse_op_return_tag(fifo_t* fifo, alp_action_t* action, bool b6, bool b7) {
  action->tag_response.completed = b7;
  action->tag_response.error = b6;
  assert(fifo_pop(fifo, &action->tag_response.tag_id, 1) == SUCCESS);
  log_print_string("parsed return tag %i, eop %i, err %i", action->tag_response.tag_id, action->tag_response.completed, action->tag_response.error);
}

/*
static void parse_op_return_status(fifo_t* fifo, alp_action_t* action, bool b6, bool b7) {
  assert(b6 && !b7); // TODO implement action status
  uint8_t itf_id;
  assert(fifo_pop(fifo, &itf_id, 1) == SUCCESS);
  assert(itf_id == 0xD7); // TODO only D7 supported for now
  // TODO uint32_t itf_len = parse_length_operand(fifo);
  // assert(itf_len == sizeof(d7ap_session_result_t));

  fifo_pop(fifo, &action->d7_interface_status.channel.channel_header_raw, 1);
  fifo_pop(fifo, (uint8_t*)&action->d7_interface_status.channel.center_freq_index, 2);
  action->d7_interface_status.channel.center_freq_index = __builtin_bswap16(action->d7_interface_status.channel.center_freq_index);
  fifo_pop(fifo, &action->d7_interface_status.rx_level, 1);
  fifo_pop(fifo, &action->d7_interface_status.link_budget, 1);
  fifo_pop(fifo, &action->d7_interface_status.target_rx_level, 1);
  fifo_pop(fifo, &action->d7_interface_status.status.raw, 1);
  fifo_pop(fifo, &action->d7_interface_status.fifo_token, 1);
  fifo_pop(fifo, &action->d7_interface_status.seqnr, 1);
  fifo_pop(fifo, &action->d7_interface_status.response_to, 1);
  fifo_pop(fifo, &action->d7_interface_status.addressee.ctrl.raw, 1);
  fifo_pop(fifo, &action->d7_interface_status.addressee.access_class, 1);
  uint8_t addressee_len = alp_addressee_id_length(action->d7_interface_status.addressee.ctrl.id_type);
  assert(fifo_pop(fifo, action->d7_interface_status.addressee.id, addressee_len) == SUCCESS);
  log_print_string("parsed interface status");
}
*/

void alp_parse_action(fifo_t* fifo, alp_action_t* action) {
  uint8_t op;
  assert(fifo_pop(fifo, &op, 1) == SUCCESS);
  bool b6 = (op >> 6) & 1;
  bool b7 = op >> 7;
  op &= 0x3F; // op is in b5-b0
  action->operation = op;
  switch(op) {
    case ALP_OP_RETURN_FILE_DATA:
      parse_op_return_file_data(fifo, action);
      break;
    case ALP_OP_RETURN_TAG:
      parse_op_return_tag(fifo, action, b6, b7);
      break;
    /*case ALP_OP_RETURN_STATUS:
      parse_op_return_status(fifo, action, b6, b7);
      break;*/
    default:
      log_print_string("op %x not implemented", op);
      assert(false);
  }
  log_print_string("parsed action");
}

/*
uint8_t alp_get_expected_response_length(uint8_t* alp_command, uint8_t alp_command_length) {
  uint8_t expected_response_length = 0;
  uint8_t* ptr = alp_command;

  while(ptr < alp_command + alp_command_length) {
    alp_control_t control;
    control.raw = (*ptr);
    ptr++; // skip control byte
    switch(control.operation) {
      case ALP_OP_READ_FILE_DATA:
        ptr += 2; // skip file offset operand // TODO we assume 2 bytes now but can be 2-5 bytes
        expected_response_length += *ptr; ptr++; // TODO we assume length is coded in 1 byte but can be 4
        // TODO
        break;
      case ALP_OP_REQUEST_TAG:
        ptr += 1; // skip tag ID operand
        break;
      case ALP_OP_RETURN_FILE_DATA:
      case ALP_OP_WRITE_FILE_DATA:
        ptr += 2; // skip file offset operand // TODO we assume 2 bytes now but can be 2-5 bytes
        uint8_t data_length = *ptr;
        ptr += 1; // skip data length field // TODO we assume length is coded in 1 byte but can be 4
        ptr += data_length; // skip data
        break;
      case ALP_OP_FORWARD:
        ptr += 1; // skip interface ID
        ptr += 1; // skip QoS
        ptr += 1; // skip dormant
        d7ap_addressee_ctrl_t addressee_ctrl;
        addressee_ctrl.raw = *ptr;
        ptr += 1; // skip addressee ctrl
        ptr += 1; // skip access class
        ptr += alp_addressee_id_length(addressee_ctrl.id_type); // skip address
        // TODO refactor to reuse same logic for parsing and response length counting
        break;
      case ALP_OP_WRITE_FILE_PROPERTIES:
        ptr += 1 + sizeof(fs_file_header_t); // skip file ID & header
        break;
      // TODO other operations
      default:
        assert(false);
    }
  }

  DPRINT("Expected ALP response length=%i", expected_response_length);
  return expected_response_length;
}

 */

void alp_append_tag_request_action(fifo_t* fifo, uint8_t tag_id, bool eop) {
  DPRINT("append tag %i", tag_id);
  uint8_t op = ALP_OP_REQUEST_TAG | (eop << 7);
  assert(fifo_put_byte(fifo, op) == SUCCESS);
  assert(fifo_put_byte(fifo, tag_id) == SUCCESS);
}

void alp_append_read_file_data_action(fifo_t* fifo, uint8_t file_id, uint32_t offset, uint32_t length, bool resp, bool group) {
  uint8_t op = ALP_OP_READ_FILE_DATA | (resp << 6) | (group << 7);
  assert(fifo_put_byte(fifo, op) == SUCCESS);
  alp_append_file_offset_operand(fifo, file_id, offset);
  alp_append_length_operand(fifo, length);
}

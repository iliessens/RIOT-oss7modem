/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2015 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "oss7modem.h"
#include "errors.h"
#include "fifo.h"
#include "alp.h"
#include "log.h"

#include "xtimer.h"
#include "thread.h"
#include "mutex.h"

#include <stdio.h>
#include <stdlib.h>

#define RX_BUFFER_SIZE 256
#define CMD_BUFFER_SIZE 256

#define BAUDRATE 115200
#define CMD_RESPONSE_TIMEOUT 500 //ms

typedef struct {
  uint8_t tag_id;
  bool is_active;
  fifo_t fifo;
  uint8_t buffer[256];
} command_t;

static uart_t uart_handle;
static fifo_t rx_fifo;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static command_t command; // TODO only one active command supported for now

static char rx_thread_stack[THREAD_STACKSIZE_MAIN];
// mutex that controls the rx thread
static mutex_t rx_mutex = MUTEX_INIT_LOCKED;
static mutex_t cmd_mutex = MUTEX_INIT_LOCKED;

// keeps track of the used Id's
static uint8_t next_tag_id = 0;
static bool parsed_header = false;
static uint8_t payload_len = 0;

static modem_read_result_t* file_return = NULL;

typedef struct {
	union {
		bool completed;
	} status;
} cmd_return_status;
static cmd_return_status return_status;

// RIOT adapter for clock
long timer_get_counter_value(void) {
	return xtimer_now_usec();
}

// alternative for callback
// TODO maybe remove this
void receiveFile(uint8_t file_id, uint32_t offset, uint32_t size, uint8_t* output_buffer) {
	if(command.is_active) puts("Response to command");
	printf("File received:\n");
	printf("File id: %d\n",file_id);
	
	printf("File data: ");
	log_print_data(output_buffer,size);
	(void) offset;
	
	if(file_return != NULL) { // synchronous read
	file_return->length = size;
	
			//TODO remove array copying
		for(unsigned int i =0; i < size; i++) {
			file_return->data[i] = output_buffer[i];
		}
	}
}

static void process_serial_frame(fifo_t* fifo) {  

  bool command_completed = false;
  bool completed_with_error = false;
  while(fifo_get_size(fifo)) {
	alp_action_t action;
    alp_parse_action(fifo, &action);

    switch(action.operation) {
      case ALP_OP_RETURN_TAG:
        if(action.tag_response.tag_id == command.tag_id) {
          command_completed = action.tag_response.completed;
          completed_with_error = action.tag_response.error;
        } else {
          log_print_string("received resp with unexpected tag_id (%i vs %i)", action.tag_response.tag_id, command.tag_id);
          // TODO unsolicited responses
        }
        break;
      case ALP_OP_RETURN_FILE_DATA:
		receiveFile(action.file_data_operand.file_offset.file_id,
                                               action.file_data_operand.file_offset.offset,
                                               action.file_data_operand.provided_data_length,
                                               action.file_data_operand.data);
        break;
      case ALP_OP_RETURN_STATUS: ;
        uint8_t addressee_len = alp_addressee_id_length(action.d7_interface_status.addressee.ctrl.id_type);
        log_print_string("received resp from: ");
        log_print_data(action.d7_interface_status.addressee.id, addressee_len);
        // TODO callback?
        break;
      default:
        assert(false);
    }
  }


  if(command_completed) {
    log_print_string("command with tag %i completed @ %i", command.tag_id, timer_get_counter_value());
	
	return_status.status.completed = ! completed_with_error;
    command.is_active = false;
	
	mutex_unlock(&cmd_mutex);
  }
  else {
	  return_status.status.completed = false;
  }
}


static void process_rx_fifo(void) {
  if(!parsed_header) {
    // <sync byte (0xC0)><version (0x00)><length of ALP command (1 byte)><ALP command> // TODO CRC
    if(fifo_get_size(&rx_fifo) > SERIAL_ALP_FRAME_HEADER_SIZE) {
        uint8_t header[SERIAL_ALP_FRAME_HEADER_SIZE];
        fifo_peek(&rx_fifo, header, 0, SERIAL_ALP_FRAME_HEADER_SIZE);

        if(header[0] != SERIAL_ALP_FRAME_SYNC_BYTE || header[1] != SERIAL_ALP_FRAME_VERSION) {
          fifo_skip(&rx_fifo, 1);
          log_print_string("skip");
          parsed_header = false;
          payload_len = 0;
          if(fifo_get_size(&rx_fifo) < SERIAL_ALP_FRAME_HEADER_SIZE)
            mutex_lock(&rx_mutex); // if header NOK --> wait for data

			mutex_unlock(&rx_mutex);
          return; // Enough header available --> re-run
        }

        parsed_header = true;
        fifo_skip(&rx_fifo, SERIAL_ALP_FRAME_HEADER_SIZE);
        payload_len = header[2];
        log_print_string("found header, payload size = %i", payload_len);
		
		// implicit return, task will re-run to parse payload
		mutex_unlock(&rx_mutex);
    }
  } else {
    if(fifo_get_size(&rx_fifo) < payload_len) {
      log_print_string("payload not complete yet");
	  
	  // stop task
      return;
    }

    // payload complete, start parsing
    // rx_fifo can be bigger than the current serial packet, init a subview fifo
    // which is restricted to payload_len so we can't parse past this packet.
    fifo_t payload_fifo;
    fifo_init_subview(&payload_fifo, &rx_fifo, 0, payload_len);
    process_serial_frame(&payload_fifo);

    // pop parsed bytes from original fifo
    fifo_skip(&rx_fifo, payload_len - fifo_get_size(&payload_fifo));
    parsed_header = false;
	
	// stop task
	//task will stop
  }
}

static void rx_cb(void * arg, uint8_t byte) {
	(void) arg; // keep compiler happy
	fifo_put_byte(&rx_fifo, byte);
  
	// start processing thread
	mutex_unlock(&rx_mutex);
}

void * rx_thread(void * arg) {
	(void) arg; // supress warning
	
	while(true) { // keep processing thread running forever
		//wait untill mutex available
		// if unlocked --> there is data to process
		mutex_lock(&rx_mutex);
		
		process_rx_fifo();
	}
	
	return NULL;
}


static void send(uint8_t* buffer, uint8_t len) {
  uint8_t header[] = {'A', 'T', '$', 'D', 0xC0, 0x00, len };
  uart_write(uart_handle, header, sizeof(header));
  uart_write(uart_handle, buffer, len);
  log_print_string("> %i bytes @ %i\n", len, timer_get_counter_value());
}

static bool blocking_send(uint8_t* buffer, uint8_t len) {
	
	mutex_trylock(&cmd_mutex); // make sure locked without blocking
	
	send(buffer,len);
	
	int ok = xtimer_mutex_lock_timeout(&cmd_mutex, CMD_RESPONSE_TIMEOUT); // try to lock again, should block until ready
	
	if(ok == -1) {
		command.is_active = false;
		return false; // timer expired
	}
	
	return return_status.status.completed;
}

bool test_comm(void) {
	return modem_read_file(0,0,0,NULL); // NULL pointer should not be a problem
}

/* Init specified UART for use with OSS7modem
 * 
 * returns 0 if OK
 * -1 on UART error
 * -2 when comm test failed
 */
int modem_init(uart_t uart) {
  uart_handle = uart;
  fifo_init(&rx_fifo, rx_buffer, RX_BUFFER_SIZE);

	// create thread
  kernel_pid_t pid = thread_create(rx_thread_stack, sizeof(rx_thread_stack), THREAD_PRIORITY_MAIN -1, 
		0 , rx_thread , NULL, "D7_rx_parser");
	
	assert(pid != EINVAL);
	assert(pid != EOVERFLOW);
	
	printf("OSS7 thread created: id %d\n", pid);
  
	int uart_state = uart_init(uart_handle, BAUDRATE, rx_cb, NULL);
	
	if(uart_state != UART_OK) {
		puts("Error initializing UART for modem!");
		return -1;
	}
	if(!test_comm()) {
		puts("Modem comm test failed! Check connections.");
		return -2;
	}
	
	return 0; // Everything OK
}

void modem_reinit(void) {
  command.is_active = false;
}

/*bool modem_execute_raw_alp(uint8_t* alp, uint8_t len) {
  send(alp, len);
}

*/
bool alloc_command(void) {
  if(command.is_active) {
    log_print_string("prev command still active @ %i", timer_get_counter_value());
    return false;
  }

  command.is_active = true;
  fifo_init(&command.fifo, command.buffer, CMD_BUFFER_SIZE);
  command.tag_id = next_tag_id;
  next_tag_id++;

  alp_append_tag_request_action(&command.fifo, command.tag_id, true);
  return true;
}

// TODO maybe remove async versions
bool modem_read_file_async(uint8_t file_id, uint32_t offset, uint32_t size) {
  if(!alloc_command())
    return false;

  alp_append_read_file_data_action(&command.fifo, file_id, offset, size, true, false);
  send(command.buffer, fifo_get_size(&command.fifo));

  return true;
}

bool modem_read_file(uint8_t file_id, uint32_t offset, uint32_t size, modem_read_result_t* result) {
  if(!alloc_command())
    return false;

	file_return = result; // put pointer so rx can use it

  alp_append_read_file_data_action(&command.fifo, file_id, offset, size, true, false);

  bool success = blocking_send(command.buffer, fifo_get_size(&command.fifo));
  
  // something else than expected was returned
  if(file_return->length == 0) return false;
  
	file_return = NULL; // clear internal pointer for next use
  
  return success;
}

// write file added from OSS7 commit 860d6050ac43842e24db84d45af46aa22ecb3bb1
bool modem_write_file(uint8_t file_id, uint32_t offset, uint32_t size, uint8_t* data) {
  if(!alloc_command())
    return false;

  alp_append_write_file_data_action(&command.fifo, file_id, offset, size, data, true, false);

  return blocking_send(command.buffer, fifo_get_size(&command.fifo));
}

/*
bool modem_send_unsolicited_response(uint8_t file_id, uint32_t offset, uint32_t length, uint8_t* data,
                                     d7ap_master_session_config_t* d7_interface_config) {
  if(!alloc_command())
    return false;

  alp_append_forward_action(&command.fifo, d7_interface_config);
  alp_append_return_file_data_action(&command.fifo, file_id, offset, length, data);

  send(command.buffer, fifo_get_size(&command.fifo));
  return true;
}

bool modem_send_raw_unsolicited_response(uint8_t* alp_command, uint32_t length,
                                         d7ap_master_session_config_t* d7_interface_config) {
  if(!alloc_command())
    return false;

  alp_append_forward_action(&command.fifo, d7_interface_config);
  fifo_put(&command.fifo, alp_command, length);

  send(command.buffer, fifo_get_size(&command.fifo));
  return true;
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//process data
#define RECORD_TYPE_PROCESS_DATA_RECORD 0xA0
#define DATA_TYPE_PAD 0x00
#define DATA_TYPE_BITS 0x01
#define DATA_TYPE_UNSIGNED 0x02
#define DATA_TYPE_SIGNED 0x03
#define DATA_TYPE_NONVOL_UNSIGNED 0x04
#define DATA_TYPE_NONVOL_SIGNED 0x05
#define DATA_TYPE_NONVOL_STREAM 0x06
#define DATA_TYPE_NONVOL_BOOLEAN 0x07

#define DATA_DIRECTION_INPUT 0x00
#define DATA_DIRECTION_BI_DIRECTIONAL 0x40
#define DATA_DIRECTION_OUTPUT 0x80

//modes
#define MODE_DATA_RECORD 0xB0

#define NO_MODES 2//gtoc modes
#define NO_GPD 1//gtoc process data
#define NO_PD 2//process data


#define MAX_PD_STRLEN 32 // this is the max space for both the unit and name strings in the PD descriptors
#define MAX_PD_VAL_BYTES 64 // this should be >= the sum of the data sizes of all pd vars

#define MEMPTR(p) ((uint32_t)&p-(uint32_t)&memory)

#define MEMU8(ptr) (memory.bytes[ptr])
#define MEMU16(ptr) (memory.bytes[ptr] | memory.bytes[ptr+1]<<8)
#define MEMU32(ptr) (memory.bytes[ptr] | memory.bytes[ptr+1]<<8 | memory.bytes[ptr+2]<<16 | memory.bytes[ptr+3]<<24)



typedef struct{
   uint8_t record_type;//0xa0
   uint8_t data_size;
   uint8_t data_type;
   uint8_t data_direction;
   float param_min;
   float param_max;
   uint16_t data_add;
   char names[MAX_PD_STRLEN];
} process_data_descriptor_t;

typedef struct{
   uint8_t record_type;//0xb0
   uint8_t index;
   uint8_t type;
   uint8_t unused;
   char name_string[4];
} mode_descriptor_t;

typedef struct{
	uint8_t input;
	uint8_t output;
   uint16_t ptocp;//pointer to process data table
   uint16_t gtocp;//pointer to mode data table
} discovery_rpc_t;

typedef struct{
	mode_descriptor_t md[NO_MODES];
	process_data_descriptor_t pd[NO_GPD];
   uint16_t eot;//end of table
} gtoc_t;

typedef struct{
	process_data_descriptor_t pd[NO_PD];
   uint16_t eot;//end of table
} ptoc_t;


typedef union {
	struct {
		uint8_t start;
		// the tocs are terminated by 0x0000 entries, hence the +1
		uint16_t ptocp[NO_PD+1];
		uint16_t gtocp[NO_GPD+NO_MODES+1];
		ptoc_t ptoc;
		gtoc_t gtoc;
		uint8_t pd_values[MAX_PD_VAL_BYTES];
		uint8_t end;
	};
	uint8_t bytes[1024];
} memory_t;


static memory_t memory;
static discovery_rpc_t discovery_rpc;


void add_pd(uint8_t *pd_num, uint16_t *param_addr, uint8_t data_size_in_bits, uint8_t data_type, uint8_t data_dir, 
			uint32_t param_min, uint32_t param_max, char *unit_string, char *name_string) {

	uint8_t i = *pd_num;

	printf("pd record %d\n", i);
	printf("value is stored at %04x\n", *param_addr);

	memory.ptoc.pd[i].record_type = RECORD_TYPE_PROCESS_DATA_RECORD;
	memory.ptoc.pd[i].data_size = data_size_in_bits;
	memory.ptoc.pd[i].data_type = data_type;
	memory.ptoc.pd[i].data_direction = data_dir;
	memory.ptoc.pd[i].param_min = param_min;
	memory.ptoc.pd[i].param_max = param_max;
	memory.ptoc.pd[i].data_add = *param_addr;
	strcpy(memory.ptoc.pd[i].names, unit_string);
	strcpy(memory.ptoc.pd[i].names + strlen(unit_string) + 1, name_string);

	memory.ptocp[i] = MEMPTR(memory.ptoc.pd[i]);

	*pd_num += 1;

	// increment the param address based on the size of the data in bits
	uint8_t num_bytes = data_size_in_bits / 8 + (data_size_in_bits % 8 > 0 ? 1 : 0);

	if (data_dir != 0x00) {
		discovery_rpc.input += num_bytes;
	}

	if (data_dir != 0x80) {
		discovery_rpc.output += num_bytes;
	}

	*param_addr += num_bytes;
}

int main(void) {
	printf("in main\n");

	uint16_t param_addr = MEMPTR(memory.pd_values);
	uint8_t pd_num = 0;

	// we increment these in add_pd
	discovery_rpc.input = 1; // 1 because the fault byte will always be present.
	discovery_rpc.output = 0;

	add_pd(&pd_num, &param_addr, 16, DATA_TYPE_UNSIGNED, DATA_DIRECTION_OUTPUT, 0, 0, "rps", "cmd_vel");
	add_pd(&pd_num, &param_addr, 8, DATA_TYPE_UNSIGNED, DATA_DIRECTION_INPUT, 0, 0, "rps", "fb_vel");

	memory.ptoc.eot = 0x0000;
	memory.gtoc.eot = 0x0000;

	memory.ptocp[NO_PD] = 0x0000;
	memory.gtocp[NO_GPD+NO_MODES] = 0x0000; 

	memory.gtoc.md[0].record_type = MODE_DATA_RECORD;
	memory.gtoc.md[0].index = 0x00;
	memory.gtoc.md[0].type = 0x00;
	memory.gtoc.md[0].unused = 0x00;
	strncpy(memory.gtoc.md[0].name_string,"foo",4);

	memory.gtoc.md[1].record_type = MODE_DATA_RECORD;
	memory.gtoc.md[1].index = 0x00;
	memory.gtoc.md[1].type = 0x01;
	memory.gtoc.md[1].unused = 0x00;
	strncpy(memory.gtoc.md[1].name_string,"io_",4);

	memory.gtoc.pd[0].record_type = RECORD_TYPE_PROCESS_DATA_RECORD;
	memory.gtoc.pd[0].data_size = 0x10;
	memory.gtoc.pd[0].data_type = 0x02;
	memory.gtoc.pd[0].data_direction = 0x80;
	memory.gtoc.pd[0].param_min = 0.0;
	memory.gtoc.pd[0].param_max = 0.0;
	memory.gtoc.pd[0].data_add = 0x095c;
	strcpy(memory.gtoc.pd[0].names,"non");
	strcat(memory.gtoc.pd[0].names,"swr");

	discovery_rpc.ptocp = MEMPTR(memory.ptocp);
	discovery_rpc.gtocp = MEMPTR(memory.gtocp);

	printf("memory created\n\n");

	printf("discovery_rpc:\n");
	printf("input bytes: %d.  output bytes: %d\n", discovery_rpc.input, discovery_rpc.output);
	printf("gtocp: 0x%04x  ptocp: 0x%04x\n", discovery_rpc.gtocp, discovery_rpc.ptocp);

	printf("\nptoc: (at 0x%04x)\n", discovery_rpc.ptocp);
	uint16_t ptocp = discovery_rpc.ptocp;


	uint16_t pd_ptr;

	while(MEMU16(ptocp) != 0x0000) {
		pd_ptr = MEMU16(ptocp);
		printf("pd ptr: 0x%04x\n", pd_ptr);

		printf("\trectype:  0x%02x\n", MEMU8(pd_ptr++));
		printf("\tdatasize: 0x%02x\n", MEMU8(pd_ptr++));
		printf("\tdatatype: 0x%02x\n", MEMU8(pd_ptr++));
		printf("\tdatadir:  0x%02x\n", MEMU8(pd_ptr++));
		printf("\tparmmin:  0x%08x\n", MEMU32(pd_ptr)); pd_ptr += 4;
		printf("\tparmmax:  0x%08x\n", MEMU32(pd_ptr)); pd_ptr += 4;
		printf("\tdataaddr: 0x%04x\n", MEMU16(pd_ptr)); pd_ptr += 2;
		char *unitstr = (char *)(memory.bytes + pd_ptr);
		printf("\tunitstr:  %s\n", unitstr);
		pd_ptr += strlen(unitstr) + 1;
		char *namestr = (char *)(memory.bytes + pd_ptr);
		printf("\tnamestr:  %s\n", namestr);
		pd_ptr += strlen(namestr) + 1;

		ptocp += 2;
	} 

	exit(0);
}
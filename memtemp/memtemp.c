
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
#define RECORD_TYPE_MODE_DATA_RECORD 0xB0

#define NO_MODES 2//gtoc modes
#define NO_GPD 1//gtoc process data
#define NO_PD 4//process data

#define IS_INPUT(pdr) (pdr.data_direction != 0x80)
#define IS_OUTPUT(pdr) (pdr.data_direction != 0x00)

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
	uint8_t input; // these are in BITS now.  I'll convert to bytes when I respond to the rpc.
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

	if (IS_INPUT(memory.ptoc.pd[i])) {
		discovery_rpc.input += data_size_in_bits;
	}

	if (IS_OUTPUT(memory.ptoc.pd[i])) {
		discovery_rpc.output += data_size_in_bits;
	}

	*param_addr += num_bytes;
}


// gtoc_idx is the index of the gpd or mode within the gtoc.  We can add gpds and modes in any order, and the gtoc will read them back
// in that order.   It's passed by value because the functions don't modify it.  It's passed in as the sum of the current gpd_num and mode_num.
void add_gpd(uint8_t *pd_num, uint8_t gtoc_idx, uint16_t *param_addr, uint8_t data_size_in_bits, uint8_t data_type, uint8_t data_dir, 
			uint32_t param_min, uint32_t param_max, char *unit_string, char *name_string) {

	uint8_t i = *pd_num;

	printf("gpd record %d\n", i);
	printf("value is stored at %04x\n", *param_addr);

	memory.gtoc.pd[i].record_type = RECORD_TYPE_PROCESS_DATA_RECORD;
	memory.gtoc.pd[i].data_size = data_size_in_bits;
	memory.gtoc.pd[i].data_type = data_type;
	memory.gtoc.pd[i].data_direction = data_dir;
	memory.gtoc.pd[i].param_min = param_min;
	memory.gtoc.pd[i].param_max = param_max;
	memory.gtoc.pd[i].data_add = *param_addr;
	strcpy(memory.gtoc.pd[i].names, unit_string);
	strcpy(memory.gtoc.pd[i].names + strlen(unit_string) + 1, name_string);

	memory.gtocp[gtoc_idx] = MEMPTR(memory.gtoc.pd[i]);

	*pd_num += 1;

	// increment the param address based on the size of the data in bits
	uint8_t num_bytes = data_size_in_bits / 8 + (data_size_in_bits % 8 > 0 ? 1 : 0);

	*param_addr += num_bytes;
}

void add_mode(uint8_t *mode_num, uint8_t gtoc_idx, uint8_t index, uint8_t type, char *name_string) {
	uint8_t i = *mode_num;

	printf("mode record %d\n", i);

	memory.gtoc.md[i].record_type = RECORD_TYPE_MODE_DATA_RECORD;
	memory.gtoc.md[i].index = index;
	memory.gtoc.md[i].type = type;
	memory.gtoc.md[i].unused = 0x00;
	strcpy(memory.gtoc.md[i].name_string, name_string);

	memory.gtocp[gtoc_idx] = MEMPTR(memory.gtoc.md[i]);

	*mode_num += 1;
}

#define BITSLEFT(ptr) (8-ptr)

void process_data_rpc(uint8_t *input, uint8_t *output) {
	uint8_t pd_cnt = NO_PD;

	printf("in pdrpc; ptoc contains %d entries\n", pd_cnt);

	*(input++) = 0xA5; // fault byte, just for easy recognition

	// data needs to be packed and unpacked based on its type and size
	// input is a pointer to the data that gets sent back to the host
	// need a bit pointer to keep track of partials

	uint8_t output_bit_ptr = 0;

	for(uint8_t i = 0; i < pd_cnt; i++) {
		process_data_descriptor_t pd = memory.ptoc.pd[i];

		if (IS_INPUT(pd)) {
		//	printf("pd %d data size is %d\n", i, memory.ptoc.pd[i].data_size);
			*(input++) = MEMU8(pd.data_add);
		}
		if (IS_OUTPUT(pd)) {
			printf("pd %d data size is %d\n", i, pd.data_size);
			uint16_t data_addr = pd.data_add;
			uint8_t data_size = pd.data_size;

			uint8_t val_bits_remaining = 8;
			uint8_t val = 0x00;

			while(data_size > 0) {
				// the number of bits to unpack this iteration is the number of bits remaining in the pd, or the number of bits remaining in the output byte, 
				// whichever is smaller.  Then, it can be even smaller if we have less room in the current val.

				uint8_t bits_to_unpack = data_size < BITSLEFT(output_bit_ptr) ? data_size : BITSLEFT(output_bit_ptr);
				if (val_bits_remaining < bits_to_unpack) { bits_to_unpack = val_bits_remaining; }

				printf("decoding partial byte, need to read %d bits from current output byte 0x%02x at bit position %d\n", bits_to_unpack, *output, output_bit_ptr);

				// create a bitmask the width of the bits to read, shifted to the position in the output byte that we're pointing to
				uint8_t mask = ((1<<bits_to_unpack) - 1) << (BITSLEFT(output_bit_ptr) - bits_to_unpack);
				printf("mask is 0x%02x\n", mask);

				// val is what we get when we mask off output and then shift it to the proper place.  
				val = (val << bits_to_unpack) | (*output & mask) >> (BITSLEFT(output_bit_ptr) - bits_to_unpack); 
				// this works sorta like a shift register.  We shift the existing bits in val up by the number of bits we just read,
				// and or the new bits to the bottom of the byte.

				printf("val is 0x%02x\n", val);

				val_bits_remaining -= bits_to_unpack;
				data_size -= bits_to_unpack;
				output_bit_ptr += bits_to_unpack;
			
				if((output_bit_ptr %= 8) == 0) output++;

				if(val_bits_remaining == 0 || data_size == 0) {
					MEMU8(data_addr++) = val;
					printf("adding 0x%02x to data\n", val);
					val_bits_remaining = 8;
					val = 0x00;
				}
			}
		}
	}
}

int main(void) {
	printf("in main\n");

	uint16_t param_addr = MEMPTR(memory.pd_values);
	uint8_t pd_num = 0, gpd_num = 0, mode_num = 0;

	// we increment these in add_pd
	discovery_rpc.input = 8; // 8 bits because the fault byte will always be present.
	discovery_rpc.output = 0;

	add_pd(&pd_num, &param_addr, 4, DATA_TYPE_BITS, DATA_DIRECTION_OUTPUT, 0, 0, "none", "output_pins");
	add_pd(&pd_num, &param_addr, 4, DATA_TYPE_BITS, DATA_DIRECTION_INPUT, 0, 0, "none", "input_pins");
	add_pd(&pd_num, &param_addr, 12, DATA_TYPE_UNSIGNED, DATA_DIRECTION_OUTPUT, 0, 0, "rps", "cmd_vel");
	add_pd(&pd_num, &param_addr, 12, DATA_TYPE_UNSIGNED, DATA_DIRECTION_INPUT, 0, 0, "rps", "fb_vel");

	add_gpd(&gpd_num, gpd_num + mode_num, &param_addr, 8, DATA_TYPE_UNSIGNED, DATA_DIRECTION_OUTPUT, 0, 0, "non", "swr");

	add_mode(&mode_num, gpd_num + mode_num, 0, 0, "foo");
	add_mode(&mode_num, gpd_num + mode_num, 1, 1, "io_");

	memory.ptoc.eot = 0x0000;
	memory.gtoc.eot = 0x0000;

	memory.ptocp[pd_num] = 0x0000;
	memory.gtocp[gpd_num + mode_num] = 0x0000; 


	discovery_rpc.ptocp = MEMPTR(memory.ptocp);
	discovery_rpc.gtocp = MEMPTR(memory.gtocp);

	printf("memory created\n\n");

	printf("discovery_rpc:\n");
	printf("input bytes: %d.  output bytes: %d\n", discovery_rpc.input>>3, discovery_rpc.output>>3);
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

	printf("\ngtoc: (at 0x%04x)\n", discovery_rpc.gtocp);
	uint16_t gtocp = discovery_rpc.gtocp;

	while(MEMU16(gtocp) != 0x0000) {
		pd_ptr = MEMU16(gtocp);
		printf("pd ptr: 0x%04x\n", pd_ptr);

		uint8_t rectype = MEMU8(pd_ptr++);
		printf("\trectype:  0x%02x\n", rectype);
		switch(rectype) {
			case RECORD_TYPE_PROCESS_DATA_RECORD:
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
				break;
			case RECORD_TYPE_MODE_DATA_RECORD:
				printf("\tindex:    0x%02x\n", pd_ptr++);
				printf("\ttype:     0x%02x\n", pd_ptr++);
				pd_ptr++; // skip 'unused' byte
				namestr = (char *)(memory.bytes + pd_ptr);
				printf("\tnamestr:  %s\n", namestr);
				pd_ptr += strlen(namestr);
				break;
		}

		gtocp += 2;
	}

	// now I'm going to create a couple pointers to values in the pdval space, these will represent our pins
	// I can change those vals, and then do a mock pd_rpc to set all the outputs and send all the inputs


	uint8_t output_buf[discovery_rpc.output>>3];
	uint8_t input_buf[discovery_rpc.input>>3];

	output_buf[0] = 0xF2;
	output_buf[1] = 0x31;
	output_buf[2] = 0x45;
	printf("incoming output bytes: "); for (uint8_t i = 0; i < discovery_rpc.output>>3; i++) { printf("0x%02x ", output_buf[i]); } printf("\n");
	process_data_rpc(input_buf, output_buf);
	printf("outgoing input bytes: "); for (uint8_t i = 0; i < discovery_rpc.input>>3; i++) { printf("0x%02x ", input_buf[i]); } printf("\n");


	printf("value in the pd[2] data pointer: 0x%04x\n", MEMU16(memory.ptoc.pd[2].data_add));


/*
	printf("setting fb and bidir\n");
	*fb_vel = 0xCA;

	output_buf[0] = 0x30;
	output_buf[1] = 0x40;

	printf("updated vals: cmd_vel 0x%02x   fb_vel 0x%02x   bidir 0x%02x\n", *cmd_vel, *fb_vel, *bidir);

	printf("incoming output bytes: "); for (uint8_t i = 0; i < discovery_rpc.output; i++) { printf("0x%02x ", output_buf[i]); } printf("\n");
	process_data_rpc(input_buf, output_buf);
	printf("outgoing input bytes: "); for (uint8_t i = 0; i < discovery_rpc.input; i++) { printf("0x%02x ", input_buf[i]); } printf("\n");

	printf("updated vals: cmd_vel 0x%02x   fb_vel 0x%02x   bidir 0x%02x\n", *cmd_vel, *fb_vel, *bidir);
	*/

	exit(0);
}
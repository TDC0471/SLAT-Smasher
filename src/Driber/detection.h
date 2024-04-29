#pragma once
#include "defs/common.h"

bool setup_cr3();

enum RESULT {
	NOHV = 0,
	HV,
	TLB_FLUSH,
	FAILURE
};

RESULT check_memory(uint64_t physical_address);

void unload();

void check_all_memory();
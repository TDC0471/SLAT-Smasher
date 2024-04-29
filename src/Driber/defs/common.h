#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <ntdef.h>
#include <wdm.h>
#include <intrin.h>

#include "datatype.h"

inline void print(const char* fmt, ...)
{
	va_list args;
	__va_start(&args, fmt);
	vDbgPrintExWithPrefix("[VMM] ", 0, 0, fmt, args);
}
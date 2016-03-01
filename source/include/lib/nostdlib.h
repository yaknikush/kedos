#ifndef __NOSTDLIB__
#define __NOSTDLIB__

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "../bcm2836/defines.h"
#include "../bcm2836/registers.h"

void* kmalloc(size_t size);

void* kcalloc(int n, size_t size);

void* krealloc(void* block, size_t size);

int kfree(void* block);

int kmem_full_dump();

void kdie(char* err_msg);

int hex2str(uint32_t hex, char* str);

#endif
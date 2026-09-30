#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdio.h>

// Get the checksum of a character buffer
// This will be used to uniquely identify different ROMs
fnv1a_checksum(const char *buffer, size_t length);

#endif
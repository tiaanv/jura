#pragma once
#define PROGMEM
#define pgm_read_byte(p) (*reinterpret_cast<const unsigned char *>(p))

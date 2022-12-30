#ifndef UTIL_H
#define UTIL_H

#define UTILITY_VERSION "1.4"

#define CLRSCRN printf ("%c",147) 
#define HOME printf ("%c",19) 
#define TOXY(X,Y) r.a=0;r.x=Y;r.y=X;r.flags=0;r.pc=58634L;_sys(&r)

#define WAITKEY do { r.pc=0xF13E; _sys(&r); } while (r.a == 0)

#define CRSR_UP 145
#define CRSR_DOWN 17
#define CRSR_LEFT 157
#define CRSR_RIGHT 29

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Poll persist busy bit and don't perform poke
// until it is 0
void safe_poke(long addr, char val);

// This checks the busy status flag before poking. Intended
// to safely POKE into VMEM_VAL registers while the persist
// bit it set so that we don't overflow the MCU's serial
// buffer.  Used by config programs when saving many registers
// back to back in a loop.
#define SAFE_POKE(addr, val) safe_poke(addr, val)

// Turn off hires bit
#define HIRES_OFF() POKE(53303L, PEEK(53303L) & 239)
// Turn on hires bit
#define HIRES_ON() POKE(53303L, PEEK(53303L) | 16)

#define INIT_COLORS() POKE(53280L,6); POKE(53281L,14); POKE(646,1);

#define FLASH_LOCKED !(get_lock_bits() & 8)
#define EXTRA_LOCKED (get_lock_bits() & 16)
#define SAVES_LOCKED (get_lock_bits() & 32)

// Returns 1 if Kawari was successfully enabled.
int enable_kawari(void);

// Returns 1 if we detected magic bytes for settings.
int have_magic(void);

// Wait for a key press or a change in switch or lock bit values
unsigned char wait_key_or_change(unsigned char current_switch_val,
                                 unsigned char current_lock_bits,
                                 unsigned char *new_switch_val,
                                 unsigned char *new_lock_bits);

unsigned char get_version_major(void);
unsigned char get_version_minor(void);
unsigned char get_lock_bits(void);
unsigned char get_chip_model(void);
void get_variant(unsigned char* dest);
unsigned int ascii_variant_to_board_int(unsigned char *variant);
unsigned int get_flash_page_size(void);
unsigned short get_capability_bits(void);
unsigned char get_cfg_version(void);

#endif

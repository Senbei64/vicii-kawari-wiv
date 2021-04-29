#define UTILITY_VERSION "1.0"

#define CLRSCRN printf ("%c",147) 
#define HOME printf ("%c",19) 
#define TOXY(X,Y) r.a=0;r.x=Y;r.y=X;r.flags=0;r.pc=58634L;_sys(&r)

#define WAITKEY do { r.pc=0xF13E; _sys(&r); } while (r.a == 0)

#define CRSR_UP 145
#define CRSR_DOWN 17
#define CRSR_LEFT 157
#define CRSR_RIGHT 29

// This checks the busy status flag before poking. Intended
// to safely POKE into VMEM_VAL registers while the persist
// bit it set so that we don't overflow the MCU's serial
// buffer.  Used by config programs when saving many registers
// back to back in a loop.
#define SAFE_POKE(addr, val) while (PEEK(53311L) & 16) {} POKE(addr,val);

// Turn off hires bit
#define HIRES_OFF() POKE(53303L, PEEK(53303L) & 239)
// Turn on hires bit
#define HIRES_ON() POKE(53303L, PEEK(53303L) | 16)

#define INIT_COLORS() POKE(53280L,6); POKE(53281L,14); POKE(646,1);


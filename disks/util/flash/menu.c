#include <stdio.h>
#include <6502.h>
#include <peekpoke.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "kawari.h"
#include "menu.h"
#include "flash.h"

// This is the version of eeprom data we know of. If we
// encounter a version higher than this, move downward and
// apply all initializations to newly added locations until
// we reach the currently saved version.  This value needs
// to change downward whenever we add new fields we want
// to save to the eeprom (so they only get initizlied once).
// NOTE: This memory location was initialied for 0xff before
// we started verioning config schemes. That's why we count
// down instead of up.
#define MY_CFG_VERSION 0xfe

// Program starts at 0x801.
// 0x5000-0x8fff is used for 16k flash load space
// 0x9000 is used first as scratch space for into then fast loader
// Program should not exceed ~14k to leave at least 4k heap space,
// otherwise heap may get corrupted by flash tmp writes since it
// wants to grow upwards starting from the end of the code.

#define FLASH_VERSION_MAJOR 1
#define FLASH_VERSION_MINOR 3

// Use a combination of direct SPI access and bulk
// SPI write operations provided by hardware to flash
// an updated bitstream to the device.  Flash (2Mb) is
// partitioned into header + golden master at 0x000000
// while 'current' bistream is located at 0x07d000.

// These must match what the Makefile uses to fill disks
// with flash image files that have been broken up. It
// depends on the page size we used to split the bitstream
// up.
#define MAX_FILE_PER_DISK_16K 8
#define MAX_FILE_PER_DISK_4K 34

static struct regs r;

#define TO_EXIT 0
#define TO_CONTINUE 1
#define TO_TRY_AGAIN 2
#define TO_NOTHING 3

unsigned char smp_tmp[40];
#define SMPRINTF_1(format, arg)\
        snprintf (smp_tmp, 39, format, arg); \
	mprintf(smp_tmp);
#define SMPRINTF_2(format, arg1, arg2)\
        snprintf (smp_tmp, 39, format, arg1, arg2);\
	mprintf(smp_tmp);
#define SMPRINTF_3(format, arg1, arg2, arg3)\
        snprintf (smp_tmp, 39, format, arg1, arg2, arg3);\
	mprintf(smp_tmp);

#define SCRATCH_SIZE 32
unsigned char filename[16];
unsigned char scratch[SCRATCH_SIZE];

unsigned char use_fast_loader;

void load_loader(void);
void copy_5000_0000(unsigned char num_256b_pages);
void compare(void);

void sys64738() {
    r.pc = (unsigned) 64738L;
    _sys(&r);
}

// Output a single character using kernel BSOUT routine.
void bsout(unsigned char c) {
    r.pc = (unsigned) 0xFFD2;
    r.a = c;
    _sys(&r);
}

// For some odd reason, the fast loader will not install
// if we use printf. So we have to build our own print
// routine based on bsout.
void mprintf(unsigned char* text) {
   int n;
   for (n=0;n<strlen(text);n++) {
       unsigned char c = text[n];
       bsout(c);
   }
}

void press_any_key(int code) {
   mprintf ("Press any key");
   if (code == TO_CONTINUE) mprintf (" to continue.\n");
   if (code == TO_EXIT) mprintf (" to exit.\n");
   if (code == TO_TRY_AGAIN) mprintf (" try again.\n");
   if (code == TO_NOTHING) mprintf (".\n");
   WAITKEY;
}

// Read a decimal value terminated with 0x0a from memory
// starting at addr.  Advance addr to after the 0x0a and
// return the decimal value as an unsigned long.
unsigned long read_decimal(unsigned long* addr) {
    unsigned long val = 0;
    unsigned char buf[16];
    unsigned n = 0;
    unsigned t = 0;
    unsigned long base;
    unsigned digit_val;

    do {
       buf[n] = PEEK(*addr);
       if(buf[n] == 0x0a) break;
       n++;
       *addr = *addr + 1;
    } while (n < 16);
    *addr = *addr + 1;

    base = 1;
    for (t=n;t>=1;t--) {
	    digit_val = buf[t-1] - '0';
	    val = val + digit_val * base;
	    base = base * 10;
    }
    return val;
}

// Read a petscii string from addr. Advance addr
// to after the 0x0a terminator.  Max SCRATCH_SIZE char
// string will get placed in 'scratch' global var.
void read_string(unsigned long* addr) {
    unsigned n = 0;
    unsigned char c;

    scratch[0] = '\0';
    do {
       c = PEEK(*addr);
       if (c != 0x0a && n < SCRATCH_SIZE-1) {
           scratch[n++] = c;
       }
       *addr = *addr + 1;
    } while (c != 0x0a);

    if (n < SCRATCH_SIZE)
       scratch[n] = '\0';
    else
       scratch[SCRATCH_SIZE-1] = '\0';
}

// Load the fast loader into $9000
unsigned char slow_load(void) {
    r.pc = (unsigned) &load_loader;
    r.x = (unsigned char)(&filename[0]);
    r.y = (unsigned)(&filename[0]) >> 8;
    r.a = (unsigned char)strlen(filename);
    _sys(&r);
    return r.x;
}

// Call the fast loader's initialization routine
void init_fast_loader(void) {
    r.pc = 36864L;
    _sys(&r);
}

// Fast load next 16k segment into $5000-$8FFF
// Return non-zero on failure to load file.
unsigned char fast_load() {
    r.pc = 36867L;
    r.x = (unsigned char)(&filename[0]);
    r.y = (unsigned)(&filename[0]) >> 8;
    _sys(&r);
    return r.a;
}

unsigned char load() {
   if (use_fast_loader)
      return fast_load();
   else
      return slow_load();
}

void fast_start(void) {
    if (use_fast_loader) {
       mprintf ("LOAD LOADER...");
       strcpy (filename,"loader");
       while (slow_load()) {
          printf ("Can't read fast loader prg\n");
          press_any_key(TO_TRY_AGAIN);
       }
       mprintf ("DONE\n\n");

       mprintf ("INIT LOADER...");
       init_fast_loader();
       mprintf ("DONE\n\n");
    }
}

// Zero indexed disk number
void please_insert(int disk_num) {
   SMPRINTF_1("\nInsert disk #%d in drive 8.\n", disk_num + 1);
   press_any_key(TO_CONTINUE);
}

void upgrade_eeprom(void) {
   unsigned char current_cfg_version;
   // Switch over to EEPROM
   use_device(DEVICE_TYPE_EEPROM);
   current_cfg_version = read_byte(CFG_VERSION);
   SMPRINTF_1 ("Cfg Version %02x\n",current_cfg_version);
   // As long as the current cfg version is higher than ours...
   while (current_cfg_version > MY_CFG_VERSION) {
      //while (SAVES_LOCKED) {
      //   mprintf ("\nERROR: SAVES lock bit is enabled!\n");
      //   mprintf ("Please put back the SAVES jumper\n");
      //   mprintf ("to continue.\n\n");
      //   press_any_key(TO_TRY_AGAIN);
      //}
      if (current_cfg_version == 0xff) {
          // Bring us to 0xfe from 0xff
          // Initialize DISPLAY_FLAGS2
          write_byte(DISPLAY_FLAGS2, 0);

          // Remember to bump version
          write_byte(CFG_VERSION, current_cfg_version - 1);
      }
      current_cfg_version--;
      SMPRINTF_1 ("Cfg Version updated to %02x\n", current_cfg_version);
   }
   // Switch back to FLASH before we exit
   use_device(DEVICE_TYPE_FLASH);
}

// Flash files are spread across 4 disks
// This routine erases the flash
void begin_flash(long num_to_write, unsigned long start_addr, unsigned long page_size, unsigned long num_disks) {
    unsigned long src_addr;
    unsigned char disknum;
    unsigned char filenum;
    unsigned char abs_filenum;
    unsigned int n;
    unsigned int max_file = (page_size == 16384 ? MAX_FILE_PER_DISK_16K : MAX_FILE_PER_DISK_4K);
    unsigned int max_disk = num_disks - 1;

    // If we had chosen a start address dividible by 64k, we
    // could have used 64k page erase.  Oh well.  At least
    // the address of our multiboot image is divisible by 4k.
    mprintf ("ERASE FLASH");
    for (src_addr=start_addr;
           src_addr<(start_addr+num_to_write) && src_addr < 2097152L;
              src_addr+=4096) {
        mprintf (".");
        wren();
        erase_4k(src_addr);
    }
    mprintf ("DONE\n\n");

    filenum = 0;
    abs_filenum = 0;
    disknum = 0;

    while (num_to_write > 0) {
       // Set next filename
       sprintf (filename,"i%03d", abs_filenum);
       SMPRINTF_2("%ld:READ %s,", num_to_write, filename);

       while (load()) {
            mprintf("\nFile not found.\n");
            press_any_key(TO_TRY_AGAIN);
            SMPRINTF_2("%ld:READ %s,", num_to_write, filename);
       }

       mprintf ("COPY,");

       // Pad remaining bytes
       if (num_to_write < page_size) {
           for (n=num_to_write; n < page_size; n++) {
              POKE(0x5000L+n, 0xff);
           }
       }

       // Transfer $5000 - $8fff into video memory @ $0000
       copy_5000_0000(page_size == 16384 ? 64 : 16);

       // Tell kawari to flash it from vmem 0x0000 to the flash address
       POKE(VIDEO_MEM_FLAGS, 0);
       POKE(VIDEO_MEM_1_IDX,(start_addr >> 16) & 0xff);
       POKE(VIDEO_MEM_1_HI,(start_addr >> 8) & 0xff);
       POKE(VIDEO_MEM_1_LO,(start_addr & 0xff));
       POKE(VIDEO_MEM_2_HI, 0);
       POKE(VIDEO_MEM_2_LO, 0);
       mprintf ("FLASH,");
       POKE(SPI_REG, FLASH_BULK_OP | FLASH_BULK_WRITE);

       // Wait for flash to be done and verified
       mprintf ("VERIFY,");
       if (wait_verify()) {
          mprintf("\nVERIFY ERROR\nRESTART FLASH TO TRY AGAIN.");
          break;
       } else {
          mprintf ("OK\n");
          start_addr += page_size;
          num_to_write -= page_size;

          abs_filenum++;
          filenum++;
          if (disknum < max_disk && filenum == max_file) {
	      filenum = 0;
	      disknum++;
	      SMPRINTF_1("Insert disk %d and press any key\n", disknum+1);
	      WAITKEY;
          }
       }
    }
    mprintf ("\nDone\n");
    press_any_key(TO_NOTHING);
}

void begin_verify(long num_to_read, unsigned long start_addr, unsigned long page_size, unsigned long num_disks) {
    unsigned char disknum;
    unsigned char filenum;
    unsigned char abs_filenum;
    unsigned int n;
    unsigned int max_file = (page_size == 16384 ? MAX_FILE_PER_DISK_16K : MAX_FILE_PER_DISK_4K);
    unsigned int max_disk = num_disks - 1;

    filenum = 0;
    abs_filenum = 0;
    disknum = 0;

    please_insert(disknum);

    while (num_to_read > 0) {
       // Set next filename
       sprintf (filename,"i%03d", abs_filenum);
       SMPRINTF_2("%ld:READ %s,", num_to_read, filename);

       while (load()) {
            mprintf("\nFile not found.\n");
            press_any_key(TO_TRY_AGAIN);
            SMPRINTF_2("%ld:READ %s,", num_to_read, filename);
       }

       // Tell kawari to read from flash to 0x0000
       POKE(VIDEO_MEM_FLAGS, 0);
       POKE(VIDEO_MEM_1_IDX,(start_addr >> 16) & 0xff);
       POKE(VIDEO_MEM_1_HI,(start_addr >> 8) & 0xff);
       POKE(VIDEO_MEM_1_LO,(start_addr & 0xff));
       POKE(VIDEO_MEM_2_HI, 0);
       POKE(VIDEO_MEM_2_LO, 0);
       mprintf ("READ FLASH,");
       POKE(SPI_REG, FLASH_BULK_OP | FLASH_BULK_READ);

       // Just wait for busy to be done, don't check verify bit.
       wait_verify();

       // Wait for flash to be done and verified
       mprintf ("VERIFY,");


       // Set the number of bytes to compare
       // vmem 0x0000 with mem 0x5000
       n = num_to_read >= page_size ? page_size : num_to_read;

// For testing in sim
//copy_5000_0000(page_size == 16384 ? 64 : 16);

       POKE(0xfe,(n >> 8) & 0xff);
       POKE(0xfd,(n & 0xff));

       compare();

       if (PEEK(0xfd) == 0) {
          mprintf ("OK\n");
          start_addr += page_size;
          num_to_read -= page_size;
          abs_filenum++;
          filenum++;
          if (disknum < max_disk && filenum == max_file) {
	     filenum = 0;
	     disknum++;
             please_insert(disknum);
          }
       } else {
	  SMPRINTF_1("FAIL @%d\n",
             PEEK(VIDEO_MEM_1_LO) + PEEK(VIDEO_MEM_1_HI) * 256);
          break;
       }
    }
    if (PEEK(0xfd) == 0)
       mprintf ("FINISHED\n");
    else
       mprintf ("VERIFY FAILED\n");

    press_any_key(TO_NOTHING);
}

void main_menu(void)
{
    unsigned char firmware_version_major;
    unsigned char firmware_version_minor;
    unsigned long page_size;
    unsigned long num_disks;
    long num_to_write;
    unsigned long start_addr;
    unsigned long tmp_addr;
    unsigned char variant[16];
    unsigned int current_board_int;
    unsigned int image_board_int;

    clrscr();

    mprintf ("VIC-II Kawari Update Util\n\n");

    SMPRINTF_2 ("Update Util Version: %d.%d\n",
        FLASH_VERSION_MAJOR, FLASH_VERSION_MINOR);

    // Turn on regs overlay to get version
    POKE(VIDEO_MEM_FLAGS, VMEM_FLAG_REGS_BIT);
    firmware_version_major = get_version_major();
    firmware_version_minor = get_version_minor();
    get_variant(variant);
    POKE(VIDEO_MEM_FLAGS, 0);

    SMPRINTF_2 ("Current Firmware Version: %d.%d\n",
        firmware_version_major, firmware_version_minor);
    SMPRINTF_1 ("Current Variant: %s\n", variant);
    mprintf ("\n");

    //        ----------------------------------------
    mprintf ("This utility will flash an update to\n");
    mprintf ("your VICII-Kawari. Do not turn off the\n");
    mprintf ("machine until the update is complete.\n");
    mprintf ("When using the fast loader, the garbage\n");
    mprintf ("at the top of the screen is normal. If\n");
    mprintf ("you have your own fast loader cartridge\n");
    mprintf ("select N when prompted. If you are\n");
    mprintf ("using a Pi1541, remember to add ALL\n");
    mprintf ("disks to your queue.\n\n");

    press_any_key(TO_CONTINUE);

    // Activate SPI reg.
    POKE(SPI_REG, 83);
    POKE(SPI_REG, 80);
    POKE(SPI_REG, 73);
 
    use_device(DEVICE_TYPE_FLASH);

    current_board_int = ascii_variant_to_board_int(variant);
    if (current_board_int != BOARD_SIM) {
      // Identify flash device
      for (;;) {
        mprintf ("\nIdentifying flash...");
        read_device();
        SMPRINTF_2 ("MID=%02x DID=%02x\n", data_in[0], data_in[1]);

        if (FLASH_LOCKED) {
           mprintf ("\nERROR: FLASH lock bit is enabled!\n");
           mprintf ("SPI functions are not available.\n");
           mprintf ("Please remove FLASH lock jumper\n");
           mprintf ("to continue.\n\n");
        }

        if (data_in[0] == 0xef) {
           mprintf ("WinBond: ");
           if (data_in[1] == 0x14) {
              mprintf ("W25Q16\n");
              break;
           } else if (data_in[1] == 0x15) {
              mprintf ("W25Q32\n");
              break;
           } else
              mprintf ("UNKNOWN\n");
              press_any_key(TO_TRY_AGAIN);
        } else {
           mprintf ("UNKNOWN FLASH\n");
           press_any_key(TO_TRY_AGAIN);
        }
      }
    }

    please_insert(0);

    mprintf ("\nREAD IMAGE INFO\n");
    strcpy (filename,"info");
    while (slow_load()) {
       printf ("Can't read info file.\n");
       press_any_key(TO_TRY_AGAIN);
    }

    // Info is now at $9000
    tmp_addr=0x9000;

    read_string(&tmp_addr);
    SMPRINTF_1 ("Image name   :%s\n", scratch);
    read_string(&tmp_addr);
    SMPRINTF_1 ("Image version:%s\n", scratch);
    num_to_write = read_decimal(&tmp_addr);
    SMPRINTF_1 ("Size         :%ld bytes\n", num_to_write);
    start_addr = read_decimal(&tmp_addr);
    SMPRINTF_1 ("Start Address:%lx \n", start_addr);
    read_string(&tmp_addr);
    SMPRINTF_1 ("Variant      :%s \n", scratch);
    image_board_int = ascii_variant_to_board_int(scratch);
    page_size = read_decimal(&tmp_addr);
    SMPRINTF_1 ("Page Size    :%ld \n", page_size);
    num_disks = read_decimal(&tmp_addr);
    SMPRINTF_1 ("Num Disks    :%ld \n", num_disks);

    if (page_size != 4096 && page_size != 16384) {
       mprintf ("\nERROR: Invalid page size\n");
       exit(0);
    }

    // Check variant matches
    if (image_board_int != current_board_int) {
       mprintf ("\nWARNING: This flash image does NOT\n");
       mprintf ("match the board number. The image\n");
       mprintf ("may be incompatible with this board.\n");
       mprintf ("If you are flashing a FORKED kawari,\n");
       mprintf ("this is expected. Proceed ONLY if\n");
       mprintf ("you know what you are doing.\n");
       mprintf ("Press any key to continue.\n");
       WAITKEY;
    }

    mprintf ("\nUse fast loader (Y/n) ?");

    use_fast_loader = 1;
    for (;;) {
       WAITKEY;
       if (r.a == 'y' || r.a == 0x0a) break;
       if (r.a == 'n') { use_fast_loader = 0; break; }
    }

    if (use_fast_loader)
       mprintf ("Y\n\n");
    else
       mprintf ("N\n\n");

    fast_start();

    for (;;) {
       if (use_fast_loader) {
          mprintf ("D - Disable fastloader\n");
       } else {
          mprintf ("E - Enable fastloader\n");
       }
       mprintf ("F - Perform flash\n");
       mprintf ("V - Perform verify\n");
       mprintf ("R - Reset\n");
       WAITKEY;
       if (r.a == 'f') {
          upgrade_eeprom();
          begin_flash(num_to_write, start_addr, page_size, num_disks);
       } else if (r.a == 'v') {
          begin_verify(num_to_write, start_addr, page_size, num_disks);
       } else if (use_fast_loader && r.a == 'd') {
          use_fast_loader = 0;
       } else if (!use_fast_loader && r.a == 'e') {
          use_fast_loader = 1;
          fast_start();
       } else if (r.a == 'r') {
          mprintf ("\nConfirm reset? (y/N)");
          WAITKEY;
          if (r.a == 'y') sys64738();
          mprintf ("\n");
       }
       //else if (r.a == 'q') {
       //   break;
       //}
    }
}

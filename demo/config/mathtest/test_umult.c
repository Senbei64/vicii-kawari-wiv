#include "tests.h"

#include <6502.h>
#include <peekpoke.h>
#include <kawari.h>
#include <util.h>
#include <stdio.h>

static unsigned long umult(unsigned short v1, unsigned short v2)
{
   unsigned long result;
   POKE(OP_1_HI, (v1&0xff00)>>8);
   POKE(OP_1_LO, v1&0xff);
   POKE(OP_2_HI, (v2&0xff00)>>8);
   POKE(OP_2_LO, v2&0xff);
   POKE(OPER, UMULT);
   // Result is 32 bits
   result = ((unsigned long)PEEK(OP_1_HI) << 24) | 
            ((unsigned long)PEEK(OP_1_LO) << 16) |
            ((unsigned long)PEEK(OP_2_HI) << 8) |
            (unsigned long)PEEK(OP_2_LO);
   return result;
}


int umult_1(void) {
   EXPECT_EQ(umult(65535u,65535u) , 65535u*65535u);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);

   EXPECT_EQ(umult(0,0) , 0);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);

   EXPECT_EQ(umult(1,0) , 0);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);

   EXPECT_EQ(umult(8,3) , 8*3);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);

   EXPECT_EQ(umult(253,64) , 253*64);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);

   EXPECT_EQ(umult(65535u,64) , 65535u*64);
   EXPECT_EQ(PEEK(OPER) & INF, 0);
   EXPECT_EQ(PEEK(OPER) & OVERFLOW, 0);
   EXPECT_EQ(PEEK(OPER) & UNDERFLOW, 0);
}


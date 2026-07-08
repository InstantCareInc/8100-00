#ifndef MSP430_BITACCESS_H_INCLUDED
#define MSP430_BITACCESS_H_INCLUDED

/*
 * IAR io430-style port bitfield access for TI CCS / msp430.h.
 */

typedef struct {
    unsigned char P0:1;
    unsigned char P1:1;
    unsigned char P2:1;
    unsigned char P3:1;
    unsigned char P4:1;
    unsigned char P5:1;
    unsigned char P6:1;
    unsigned char P7:1;
} sfr_port_bits_t;

#define SFR_PORT_BITS(sfr) (*(volatile sfr_port_bits_t *)&(sfr))

#define P1IN_bit   SFR_PORT_BITS(P1IN)
#define P2OUT_bit  SFR_PORT_BITS(P2OUT)
#define P3OUT_bit  SFR_PORT_BITS(P3OUT)
#define P4IN_bit   SFR_PORT_BITS(P4IN)
#define P4OUT_bit  SFR_PORT_BITS(P4OUT)
#define P5OUT_bit  SFR_PORT_BITS(P5OUT)
#define P6IN_bit   SFR_PORT_BITS(P6IN)

#endif

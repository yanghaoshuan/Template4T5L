#ifndef __T5LOS8051_H__
#define __T5LOS8051_H__


sfr	P0		=	0x80;
sbit P00    =   P0^0;
sbit P01    =   P0^1;    
sbit P02    =   P0^2;
sbit P03    =   P0^3;
sbit P04    =   P0^4;
sbit P05    =   P0^5;
sbit P06    =   P0^6;
sbit P07    =   P0^7;

sfr	SP		=	0x81;		
sfr DPL		=	0x82;		
sfr DPH		=	0x83;		
sfr PCON	=	0x87;		
sfr TCON	=	0x88;		

sbit	TF1 =	TCON^7;		
sbit	TR1	=	TCON^6;		
sbit	TF0	=	TCON^5;		
sbit	TR0	=	TCON^4;
sbit	IE1	=	TCON^3;		
sbit	IT1	=	TCON^2;		
sbit	IE0	=	TCON^1;		
sbit	IT0	=	TCON^0;		

sfr	TMOD	=	0x89;		
sfr	TH0 	=	0x8C;		
sfr TL0 	=	0x8A;
sfr TH1 	=	0x8D;
sfr TL1 	=	0x8B;

sfr CKCON	=	0x8E;		
sfr	P1		=	0x90;
sbit P10    =   P1^0;
sbit P11    =   P1^1;
sbit P12    =   P1^2;
sbit P13    =   P1^3;
sbit P14    =   P1^4;
sbit P15    =   P1^5;
sbit P16    =   P1^6;
sbit P17    =   P1^7;

sfr	DPC		=	0x93;		
sfr PAGESEL	=	0x94;		
sfr	D_PAGESEL	=	0x95;	

sfr SCON0	=	0x98;		
sbit	TI0	=	SCON0^1;
sbit	RI0	=	SCON0^0;
sfr	SBUF0	=	0x99;		
sfr	SREL0H	=	0xBA;		
sfr	SREL0L	=	0xAA;

sfr	SCON1	=	0x9B;		
sfr	SBUF1	=	0x9C;
sfr	SREL1H	=	0xBB;
sfr	SREL1L	=	0x9D;

sfr	IEN2	=	0x9A;		
sfr	P2		=	0xA0;
sbit P20    =   P2^0;
sbit P21    =   P2^1;
sbit P22    =   P2^2;
sbit P23    =   P2^3;
sbit P24    =   P2^4;
sbit P25    =   P2^5;
sbit P26    =   P2^6;
sbit P27    =   P2^7;
sfr	IEN0	=	0xA8;		
sbit	EA	=	IEN0^7;		
sbit	ET2	=	IEN0^5;		
sbit	ES0	=	IEN0^4;		
sbit	ET1	=	IEN0^3;		
sbit	EX1	=	IEN0^2;		
sbit	ET0	=	IEN0^1;		
sbit	EX0	=	IEN0^0;		

sfr	IP0		=	0xA9;				
sfr	P3		=	0xB0;
sbit P30    =   P3^0;
sbit P31    =   P3^1;
sbit P32    =   P3^2;
sbit P33    =   P3^3;
sfr	IEN1	=	0xB8;				
sbit	ES3R	=	IEN1^5;			
sbit	ES3T	=	IEN1^4;			
sbit	ES2R	=	IEN1^3;			
sbit	ES2T	=	IEN1^2;			
sbit	ECAN	=	IEN1^1;			

sfr	IP1		=	0xB9;				
sfr	IRCON2	=	0xBF;
sfr	IRCON 	=	0xC0;
sbit	TF2	=	IRCON^6;			
sfr	T2CON	=	0xC8;				
sbit	TR2	=	T2CON^0;			
sfr	TRL2H	=	0xCB;
sfr	TRL2L	=	0xCA;
sfr	TH2 	=	0xCD;
sfr	TL2 	=	0xCC;

sfr	PSW		=	0xD0;
sbit	CY	=	PSW^7;
sbit	AC	=	PSW^6;
sbit	F0	=	PSW^5;
sbit	RS1	=	PSW^4;
sbit	RS0	=	PSW^3;
sbit	OV	=	PSW^2;
sbit	F1	=	PSW^1;
sbit	P	=	PSW^0;
sfr	ADCON	=	0xD8;
sfr	ACC		=	0xE0;
sfr	B 		=	0xF0;


sfr	RAMMODE	=	0xF8;				
sbit	APP_REQ	=	RAMMODE^7;
sbit	APP_EN	=	RAMMODE^6;
sbit	APP_RW	=	RAMMODE^5;
sbit	APP_ACK	=	RAMMODE^4;
sfr ADR_H	=	0xF1;
sfr ADR_M	=	0xF2;
sfr ADR_L	=	0xF3;
sfr ADR_INC	=	0xF4;
sfr DATA3	=	0xFA;
sfr DATA2	=	0xFB;
sfr DATA1	=	0xFC;
sfr DATA0	=	0xFD;


sfr	SCON2T	=	0x96;					
sfr	SCON2R	=	0x97;					
sfr	BODE2_DIV_H	=	0xD9;				
sfr	BODE2_DIV_L	=	0xE7;
sfr	SBUF2_TX	=	0x9E;				
sfr	SBUF2_RX	=	0x9F;				

sfr	SCON3T	=	0xA7;					
sfr	SCON3R	=	0xAB;
sfr	BODE3_DIV_H	=	0xAE;
sfr	BODE3_DIV_L	=	0xAF;
sfr	SBUF3_TX	=	0xAC;
sfr	SBUF3_RX	=	0xAD;

sfr	CAN_CR	=	0x8F;
sfr	CAN_IR	=	0x91;
sfr	CAN_ET	=	0xE8;

sfr	P0MDOUT	=	0xB7;
sfr	P1MDOUT	=	0xBC;
sfr	P2MDOUT	=	0xBD;
sfr	P3MDOUT	=	0xBE;
sfr	MUX_SEL	=	0xC9;
sfr	PORTDRV	=	0xF9;				

sfr	MAC_MODE	=	0xE5;
sfr	DIV_MODE	=	0xE6;

sfr	EXADR	=	0xFE;
sfr	EXDATA	=	0xFF;



#endif



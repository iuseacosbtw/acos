#include "acos.h"

extern dword read_pci(dword bus,dword device,dword func,dword offset);

typedef struct{
	word flags;
	word prdtl;
	dword prdbc;
	dword ctba;
	dword ctbau;
	dword reserved[4];
}__attribute__((packed)) cmd_header;
typedef struct{
	byte  type;	// FIS_TYPE_REG_H2D

	byte  flags;

	byte  command;	// Command register
	byte  featurel;	// Feature register, 7:0

	byte  lba0;		// LBA low register, 7:0
	byte  lba1;		// LBA mid register, 15:8
	byte  lba2;		// LBA high register, 23:16
	byte  device;		// Device register

	byte  lba3;		// LBA register, 31:24
	byte  lba4;		// LBA register, 39:32
	byte  lba5;		// LBA register, 47:40
	byte  featureh;	// Feature register, 15:8

	byte  countl;		// Count register, 7:0
	byte  counth;		// Count register, 15:8
	byte  icc;		// Isochronous command completion
	byte  control;	// Control register

	byte  rsv1[4];	// Reserved
}__attribute__((packed)) fis_h2d;
typedef struct{
    dword dba;
    dword dbau;
    dword reserved;
    dword dbc_i;
} __attribute__((packed)) prdt_entry;
typedef volatile struct{
	dword clb;		// 0x00, command list base address, 1K-byte aligned
	dword clbu;		// 0x04, command list base address upper 32 bits
	dword fb;		// 0x08, FIS base address, 256-byte aligned
	dword fbu;		// 0x0C, FIS base address upper 32 bits
	dword is;		// 0x10, interrupt status
	dword ie;		// 0x14, interrupt enable
	dword cmd;		// 0x18, command and status
	dword rsv0;		// 0x1C, Reserved
	dword tfd;		// 0x20, task file data
	dword sig;		// 0x24, signature
	dword ssts;		// 0x28, SATA status (SCR0:SStatus)
	dword sctl;		// 0x2C, SATA control (SCR2:SControl)
	dword serr;		// 0x30, SATA error (SCR1:SError)
	dword sact;		// 0x34, SATA active (SCR3:SActive)
	dword ci;		// 0x38, command issue
	dword sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	dword fbs;		// 0x40, FIS-based switch control
	dword rsv1[11];	// 0x44 ~ 0x6F, Reserved
	dword vendor[4];	// 0x70 ~ 0x7F, vendor specific
}__attribute__((packed)) port;


dword discover_hdd(void){
	for(word bus=0;bus<256;bus++){
		for(byte dev=0;dev<32;dev++){
			for(byte func=0;func<8;func++){
				dword vendev=read_pci(bus,dev,func,0);
				if((vendev&0xFFFF)==0xFFFF) continue;

				dword class_code=read_pci(bus,dev,func,8);
				byte base_class=class_code>>24;
				byte subclass=class_code>>16;
				byte prog=class_code>>8;

				/*printw(bus);
				printh(dev);
				printh(func);
				printf(": ");

				printd(class_code);
				printc('\n');*/

				if(base_class==1&&(subclass==4||(subclass==6&&prog==1))){
					dword bar5=read_pci(bus,dev,func,0x24);
					return bar5&0xFFFFFFF0;
				}
			}
		}
	}
	return 0;
}

#define AHCI_RECEIVED_FIS 0x500
#define AHCI_BUFFER 0x600
#define AHCI_COMMAND_LIST 0x800
#define AHCI_COMMAND_TABLE 0xC00

#define FIS_TYPE_H2D 0x27

#define CMD_IDENTIFY 0xEC
#define CMD_READ 0x25
#define CMD_WRITE 0x35

port* p;
cmd_header* header;
// 1: No ABAR
dword ahci_init(void){
	dword abar=discover_hdd();
	if(!abar) return 1;

	p=(port*)(abar+0x100+4*0x80);
	p->clb=AHCI_COMMAND_LIST;
	p->fb=AHCI_RECEIVED_FIS;

	header=(cmd_header*)AHCI_COMMAND_LIST;
	header->prdtl=1;
	header->ctba=AHCI_COMMAND_TABLE;

	p->cmd&=~1;
	while(p->cmd&(1<<15)); // wait for CR to be 0
	p->serr=0xFFFFFFFF;
	p->is=0xFFFFFFFF;
	p->cmd|=(1<<4);
	p->cmd|=1;
	return 0;
}

byte ahci_read_prog(dword addr,dword size,byte SEG_GS* buffer){
	fis_h2d* fis=(fis_h2d*)AHCI_COMMAND_TABLE;
	fis->type=FIS_TYPE_H2D;
	fis->flags=0x80;
	fis->command=CMD_READ;

	dword offset=addr%512;
	dword sector=addr/512;
	fis->lba0=sector;
	fis->lba1=sector>>8;
	fis->lba2=sector>>16;
	fis->lba3=sector>>24;
	fis->lba4=0;
	fis->lba5=0;

	fis->device=0x40;

	dword sectors=(offset+size+511)/512; // cieling division
	fis->countl=sectors;
	fis->counth=sectors>>8;

	prdt_entry* prdt=(prdt_entry*)(AHCI_COMMAND_TABLE+0x80);
	dword allocation=allocate(sectors*512);
	if(!allocation) return 0;
	prdt->dba=allocation;
	prdt->dbc_i=sectors*512-1;

	header->prdbc=0;
	header->flags=5;
	p->ci=1;
	while(p->ci&1);
	if(p->is&(1<<30)){
		free(allocation,sectors*512);
		return 0;
	}

	// read succeeded, now write to the buffer
	byte* src=(byte*)allocation+offset;
	for(dword i=0;i<size;i++) buffer[i]=src[i];
	free(allocation,sectors*512);
	return 1;
}
byte ahci_read(dword addr,dword size,byte* buffer){
	fis_h2d* fis=(fis_h2d*)AHCI_COMMAND_TABLE;
	fis->type=FIS_TYPE_H2D;
	fis->flags=0x80;
	fis->command=CMD_READ;

	dword offset=addr%512;
	dword sector=addr/512;
	fis->lba0=sector;
	fis->lba1=sector>>8;
	fis->lba2=sector>>16;
	fis->lba3=sector>>24;
	fis->lba4=0;
	fis->lba5=0;

	fis->device=0x40;

	dword sectors=(offset+size+511)/512; // cieling division
	fis->countl=sectors;
	fis->counth=sectors>>8;

	prdt_entry* prdt=(prdt_entry*)(AHCI_COMMAND_TABLE+0x80);
	dword allocation=allocate(sectors*512);
	if(!allocation) return 0;
	prdt->dba=allocation;
	prdt->dbc_i=sectors*512-1;

	header->prdbc=0;
	header->flags=5;
	p->ci=1;
	while(p->ci&1);
	if(p->is&(1<<30)){
		free(allocation,sectors*512);
		return 0;
	}

	// read succeeded, now write to the buffer
	byte* src=(byte*)allocation+offset;
	for(dword i=0;i<size;i++) buffer[i]=src[i];
	free(allocation,sectors*512);
	return 1;
}



byte ahci_write(dword addr,dword size,byte* buffer){
	dword offset=addr%512;
	dword sector=addr/512;
	dword sectors=(offset+size+511)/512; // cieling division

	byte* allocation=(byte*)allocate(sectors*512);
	if(!allocation) return 0;

	ahci_read(sector*512,sectors*512,allocation);
	byte* des=allocation+offset;
	for(dword i=0;i<size;i++) des[i]=buffer[i];



	fis_h2d* fis=(fis_h2d*)AHCI_COMMAND_TABLE;
	fis->type=FIS_TYPE_H2D;
	fis->flags=0x80;
	fis->command=CMD_WRITE;

	fis->lba0=sector;
	fis->lba1=sector>>8;
	fis->lba2=sector>>16;
	fis->lba3=sector>>24;
	fis->lba4=0;
	fis->lba5=0;

	fis->device=0x40;

	fis->countl=sectors;
	fis->counth=sectors>>8;

	prdt_entry* prdt=(prdt_entry*)(AHCI_COMMAND_TABLE+0x80);
	prdt->dba=(dword)allocation;
	prdt->dbc_i=sectors*512-1;

	header->prdbc=0;
	header->flags=0x45;
	p->ci=1;
	while(p->ci&1);
	free((dword)allocation,sectors*512);
	if(p->is&(1<<30)) return 0;

	return 1;
}






// Now, filesystem stuff!

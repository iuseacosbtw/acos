#ifndef ACOS_H
#define ACOS_H

#include <stdint.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef struct{
	void* ptr;
	dword size;
}__attribute((packed)) region;

typedef dword pid;
#define SEG_GS __attribute__((address_space(256)))

#define ESUCCESS 0
// no error occured during the operation
// everything went according to plan!
#define ENOPID 1
// unable to find an available task_state ID
// increase TASK_LIMIT or don't allocate so many tasks
#define ENOMEM 2
// unable allocate enough memory for your request
// check for memory leaks in your program
#define EINVALID 3
// the arguments to the function were invalid
// check the function's definition for the valid syntax
#define EFORBIDDEN 4
// you do not have permission to perform the operation
// make sure you have permission, i guess
#define ETEMP 5
// the function failed because a resource is temporarily unavailable
// retrying it may work
extern _Noreturn void kinf_loop(void);
word getcursorpos(void);
extern void setcursorpos(byte x,byte y);
extern void printc(byte c);
extern void prints(char* str);
extern void prints_prog(char SEG_GS* str);
extern void printc_raw(byte c);
extern void prints_raw(char* str);
extern void prints_raw_prog(char SEG_GS* str);
extern void printh(byte c);
static inline void printw(word c){
	printh(c>>8);
	printh(c);
}
static inline void printd(dword c){
	printh(c>>24);
	printh(c>>16);
	printh(c>>8);
	printh(c);
}
extern void clear(void);
extern void panic(char* str);
extern void panic_prog(char SEG_GS* str);
extern void scroll(dword lines);
extern byte getchar_noint(void);
byte getchar(void);

extern void settimer(word time);
extern void beep_asm(word frequency);
extern void beep_stop(void);

extern byte cmpstr(char* str1,char* str2);
extern byte cmpstr_prog(char SEG_GS* str1,char SEG_GS* str2);

dword ahci_init(void);
byte ahci_read_prog(dword addr,dword size,void SEG_GS* buffer);
byte ahci_read(dword addr,dword size,void* buffer);
byte ahci_write(dword addr,dword size,void* buffer);

void printf(char* str);
void printf_prog(char SEG_GS* str);
dword allocate(dword size);
dword allocate_aligned(dword size,word alignment);
dword reallocate(dword ptr,dword size,dword new_size);
void free(dword ptr,dword size);
_Noreturn void exit(dword code);
char kill_program(pid program,byte urgent);
byte proc_getFlags(pid proc);

typedef struct{
	dword addr;
	dword count;
	byte flags;
	/* flags;
	 * 7-1: Reserved
	 * 0: Cached
	*/
}FILE; // used by kernel
// a FILE* points to this kernel struct
// I know, I know. This isn't in file.h because of the proc_getFiles() function
FILE** proc_getFiles(pid proc);
#define PAGE_SIZE 4096
#define FILE_ARRAY_SIZE 1024
#define HEAP_ARRAY_SIZE 1024

#endif

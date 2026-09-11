#include <stdint.h>

void *memset(void *dest, int value, unsigned int n)
{
    unsigned char *p = dest;

    while (n--)
        *p++ = (unsigned char)value;

    return dest;
}

void *memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;

    return dest;
}

void *memmove(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;

        while (n--)
            *--d = *--s;
    }

    return dest;
}

int memcmp(const void *a, const void *b, unsigned int n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;

    while (n--) {
        if (*x != *y)
            return *x - *y;

        x++;
        y++;
    }

    return 0;
}
// these functions are required by C standards. idk

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

typedef dword pid;

extern dword _finish;

typedef struct{
	dword gs;
	dword fs;
	dword es;
	dword ds;
	
	dword edi;
	dword esi;
	dword ebp;
	dword esp_old;
	
	dword ebx;
	dword edx;
	dword ecx;
	dword eax;
	
	dword eip;
	dword cs;
	dword eflags;
	
	dword esp;
	dword ss;
}task_state;
typedef struct{
	word lim_low;
	word base_low;
	byte base_mid;
	byte access;
	byte flags;
	byte base_high;
}gdt_entry;
typedef struct{
	union{
		struct{
			gdt_entry exec;
			gdt_entry data;
			gdt_entry stack;
		};
		gdt_entry entries[3];
	};
}proc_gdt;
typedef struct{
	dword delay;
	pid parent;
	proc_gdt segments;
}process;


extern void setcursorpos(byte x,byte y);
extern void printc(byte c);
extern void prints(char* str);
extern void printc_raw(byte c);
extern void prints_raw(char* str);
extern void printh(byte c);
extern void clear(void);
extern byte scancode_to_ascii(byte scancode);
extern byte getchar(void);
extern byte getscancode(void);
extern void settimer(word time);
extern void reload_gdt(proc_gdt* task_state_gdt);

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


#define PAGE_SIZE 4096
#define TASK_LIMIT 256
pid current_pid=0;
task_state tasks[TASK_LIMIT]={};
process processes[TASK_LIMIT]={};


#define GDT_ENTRY_ACCESS_USEREXEC 0b11111010
#define GDT_ENTRY_ACCESS_USERDATA 0b11110010
#define GDT_ENTRY_FLAGS_DEFAULT 0b11000000

#define GDT_ENTRY_TYPE_EXEC 0
#define GDT_ENTRY_TYPE_DATA 1
gdt_entry create_user_gdt_entry(dword start,dword size,byte type){
	// size scales by 2^12
	/* types
	 * 0 for executable
	 * 1 for data 
	*/
	gdt_entry ret={};
	
	if(type==GDT_ENTRY_TYPE_EXEC) ret.access=GDT_ENTRY_ACCESS_USEREXEC;
	else if(type==GDT_ENTRY_TYPE_DATA) ret.access=GDT_ENTRY_ACCESS_USERDATA;
	else return ret;
	
	ret.flags=GDT_ENTRY_FLAGS_DEFAULT&((size>>16)&0x0F);
	
	ret.base_low=(word)start;
	ret.base_mid=(byte)(start>>16);
	ret.base_high=(byte)(start>>24);
	
	ret.lim_low=(word)size;
	
	for(volatile byte i=0;i==0;);
	return ret;
}
void extract_gdt_entry(gdt_entry entry,dword* start,dword* size,byte* type){
	*start=((dword)entry.base_high<<24)|((dword)entry.base_mid<<16)|(dword)entry.base_low;
	*size=((dword)(entry.flags&0x0F)<<16)|(dword)entry.lim_low;
	*type=(entry.access&0b00001000)>>3;
}
/*proc_gdt allocate_program(dword code_size,dword data_size,dword stack_size)
	byte space_found=0;
	
	pid i=TASK_LIMIT;
	while(!space_found){
		for(;i<TASK_LIMIT;i++){
			if(tasks[i].esp!=0){
				i--;
				break;
			}
		}
		
	}
}*/


byte schedule_program(proc_gdt segments){
	dword new_pid;
	byte pid_found=0;
	for(dword i=0;i<TASK_LIMIT;i++){
		if(tasks[i].esp!=0){
			new_pid=i;
			pid_found=1;
			break;
		}
	}
	if(!pid_found) return ENOPID;
	dword entry_start;
	dword entry_size;
	byte entry_type;
	
	for(byte i=0;i<3;i++){
		extract_gdt_entry(segments.entries[i],&entry_start,&entry_size,&entry_type);
		
		if(i==0&&entry_type!=GDT_ENTRY_ACCESS_USEREXEC) return EINVALID;
		if(i>0&&entry_type!=GDT_ENTRY_ACCESS_USERDATA) return EINVALID;
		if(entry_start<_finish) return EFORBIDDEN;
		if((segments.entries[i].access&0b01100000)!=0b01100000) return EFORBIDDEN;
	}
	
	dword code_base;
	dword code_size;
	byte code_type;
	extract_gdt_entry(segments.exec,&code_base,&code_size,&code_type);
	
	task_state state={
		0x28,
		0x28,
		0x28,
		0x28,
		
		0,
		0,
		0,
		0,
		
		0,
		0,
		0,
		0,
		
		code_base,
		0x20,
		2,
		
		entry_size*PAGE_SIZE,
		0x30
	};
	
	reload_gdt(&segments);
	return 0;
}




void kmain(void){
	prints("Kmain jump works\n");
	gdt_entry code1=create_user_gdt_entry(0x100000,1,GDT_ENTRY_TYPE_EXEC);
	prints("\nIf you see this, nothing was corrupted");
	/*gdt_entry* data1=create_user_gdt_entry(0x101000,1,GDT_ENTRY_TYPE_DATA);
	gdt_entry* stack1=create_user_gdt_entry(0x101000,1,GDT_ENTRY_TYPE_DATA);
	
	gdt_entry* code2=create_user_gdt_entry(0x102000,1,GDT_ENTRY_TYPE_EXEC);
	gdt_entry* data2=create_user_gdt_entry(0x103000,1,GDT_ENTRY_TYPE_DATA);
	gdt_entry* stack2=create_user_gdt_entry(0x103000,1,GDT_ENTRY_TYPE_DATA);
	
	proc_gdt proc1={code1,data1,stack1};
	proc_gdt proc2={code2,data2,stack2};
	
	word* a1=(word*)0x100000;
	word* a2=(word*)0x102000;
	
	*a1=0xFEEB;
	*a2=0xFEEB;
	
	byte p1=schedule_program(proc1);
	if(p1){
		prints("Process 1 failed!: ");
		printh(p1);
	}
	byte p2=schedule_program(proc2);
	if(p1){
		prints("Process 2 failed!: ");
		printh(p1);
	}
	prints("Ultimate success..?");
	
	for(;;);*/
}

task_state* IRQ_handler(byte irq,task_state* state){
	byte privilege=(byte)(state->cs&(word)3);
	if(privilege==(byte)3&&irq==0x20){
		tasks[current_pid]=*state;
		
		for(pid i=0;i<TASK_LIMIT;i++){
			if(i!=current_pid&&tasks[i].esp!=0&&processes[i].delay!=0){
				current_pid=i;
				break;
			}
		}
		
		return &tasks[current_pid];
	}else{
		return state;
	}
	return 0;
}
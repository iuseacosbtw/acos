#include "acos.h"
#include "math.h"
#include "file.h"

extern volatile word G_Printing_TextCursor;
extern volatile byte G_Printing_TextAttribute;
void printf(char* str){
	byte preAttr=G_Printing_TextAttribute;
	for(;*str!=0;str++){
		if(*str=='\e'){
			if(*str++==0) return;

			if(*str=='C'){
				str++;
				byte attr=0;
				for(;*str!=0&&*str!=';';str++){
					if(*str=='Q') attr|=0x80;
					else if(*str=='R') attr|=0x40;
					else if(*str=='G') attr|=0x20;
					else if(*str=='B') attr|=0x10;
					else if(*str=='q') attr|=0x08;
					else if(*str=='r') attr|=0x04;
					else if(*str=='g') attr|=0x02;
					else if(*str=='b') attr|=0x01;

					else if(*str=='F') attr=0x0F;
					else if(*str=='f') attr=preAttr;
					else{
						attr=G_Printing_TextAttribute;
						for(;*str!=0&&*str!=';';str++);
						break;
					}
				}
				if(*str==0) break;
				G_Printing_TextAttribute=attr;
			}else if(*str=='F'){
				char clearArray[83];
				clearArray[0]='\r';
				for(byte i=1;i<81;i++) clearArray[i]=' ';
				clearArray[81]=1;
				clearArray[82]=0;
				printf(clearArray);
				for(;*str!=0&&*str!=';';str++);
			}else for(;*str!=0&&*str!=';';str++);
		}else printc(*str);
	}
}
void printf_prog(char SEG_GS* str){
	byte preAttr=G_Printing_TextAttribute;
	for(;*str!=0;str++){
		if(*str=='\e'){
			if(*str++==0) return;

			if(*str=='C'){
				str++;
				byte attr=0;
				for(;*str!=0&&*str!=';';str++){
					if(*str=='Q') attr|=0x80;
					else if(*str=='R') attr|=0x40;
					else if(*str=='G') attr|=0x20;
					else if(*str=='B') attr|=0x10;
					else if(*str=='q') attr|=0x08;
					else if(*str=='r') attr|=0x04;
					else if(*str=='g') attr|=0x02;
					else if(*str=='b') attr|=0x01;

					else if(*str=='F') attr=0x0F;
					else if(*str=='f') attr=preAttr;
					else{
						attr=G_Printing_TextAttribute;
						for(;*str!=0&&*str!=';';str++);
						break;
					}
				}
				if(*str==0) break;
				G_Printing_TextAttribute=attr;
			}else if(*str=='F'){
				char clearArray[83];
				clearArray[0]='\r';
				for(byte i=1;i<81;i++) clearArray[i]=' ';
				clearArray[81]=1;
				clearArray[82]=0;
				printf(clearArray);
				for(;*str!=0&&*str!=';';str++);
			}else for(;*str!=0&&*str!=';';str++);
		}else printc(*str);
	}
}
word getcursorpos(void){
	return (G_Printing_TextCursor%80)|(G_Printing_TextCursor/80<<8);
}
void text_background(byte color_byte){
	color_byte&=0xF0;
	G_Printing_TextAttribute|=color_byte;
	G_Printing_TextAttribute&=color_byte|0x0F;
	for(byte* i=(byte*)0xB8001;i<(byte*)(0xB8000+4000);i+=2){
		*i|=color_byte;
		*i&=color_byte|0x0F;
	}
}

static dword beep_time=0;
static byte beeping=0;
void beep(word frequency,word time){
	if(beeping) return;
	if(frequency==0) return;

	beep_asm(1193180/frequency);
	beeping=1;
	beep_time=time%500;
}


// THE CODE BELOW IS NOT WRITTEN BY ME ----------------------------------------

/* Set (value != 0) or clear (value == 0) bits start..end inclusive.
   Bit i lives in byte i/8, at bit position i%8 (LSB-first). */
void bitmap_fill(byte *arr, dword start, dword end, byte value)
{
    if (start > end || end >= 1<<20) return;

    dword first = start / 8;
    dword last  = end / 8;

    byte first_mask = (byte)(0xFF << (start % 8));  /* bits start%8 .. 7 */
    byte last_mask  = (byte)(0xFF >> (7 - end % 8)); /* bits 0 .. end%8   */

    if (first == last) {
        byte mask = first_mask & last_mask;
        if (value) arr[first] |= mask;
        else       arr[first] &= (byte)~mask;
        return;
    }

    /* partial first byte */
    if (value) arr[first] |= first_mask;
    else       arr[first] &= (byte)~first_mask;

    /* whole bytes in between */
    if (last > first + 1){
    	dword count=0;
     	for(byte* i=&arr[first + 1];count<last - first - 1;i++){
     		*i=value ? 0xFF : 0x00;
       		count++;
       	}
    }

    /* partial last byte */
    if (value) arr[last] |= last_mask;
    else       arr[last] &= (byte)~last_mask;
}
// THE CODE ABOVE IS NOT WRITTEN BY ME ----------------------------------------

const dword kernel_finish=0x300000;

typedef struct{
	dword gs;
	dword fs;
	dword es;
	dword ds;

	dword edi;
	dword esi;
	dword ebp;
	dword active;

	dword ebx;
	dword edx;
	dword ecx;
	dword eax;

	dword eip;
	dword cs;
	dword eflags;

	dword esp;
	dword ss;
}__attribute__((packed)) task_state;
typedef struct{
	word lim_low;
	word base_low;
	byte base_mid;
	byte access;
	byte flags;
	byte base_high;
}__attribute__((packed)) gdt_entry;
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
	FILE** files;
	region* heap;
	region* anti_heap;

	byte flags;
	/* flags;
	 * 7-4: Reserved
	 * 3: Suspended
	 * 2: "Dirty" (should not close immediately upon alt+f4)
	 * 1: Can accept keyboard input
	 * 0: Is root
	*/
}process;

extern void reload_gdt(proc_gdt* task_state_gdt);


#define STACK_SIZE 65536
#define TASK_LIMIT 256
pid current_pid;
static pid foreground_pid;
static task_state tasks[TASK_LIMIT]={};
static process processes[TASK_LIMIT]={};

static dword* alloc_map;
static dword alloc_map_size;
static byte alloc_moving;


#define GDT_ENTRY_ACCESS_USEREXEC 0b11111010
#define GDT_ENTRY_ACCESS_USERDATA 0b11110010
#define GDT_ENTRY_FLAGS_DEFAULT 0b11000000

#define GDT_ENTRY_TYPE_EXEC 1
#define GDT_ENTRY_TYPE_DATA 0
gdt_entry create_user_gdt_entry(dword start,dword size,byte type){
	// size scales by 2^12
	/* types
	 * 1 for executable
	 * 0 for data
	*/
	gdt_entry ret={};

	if(type==GDT_ENTRY_TYPE_EXEC) ret.access=GDT_ENTRY_ACCESS_USEREXEC;
	else if(type==GDT_ENTRY_TYPE_DATA) ret.access=GDT_ENTRY_ACCESS_USERDATA;
	else return ret;

	ret.flags=GDT_ENTRY_FLAGS_DEFAULT|((size>>16)&0x0F);

	ret.base_low=(word)start;
	ret.base_mid=(byte)(start>>16);
	ret.base_high=(byte)(start>>24);

	ret.lim_low=(word)size;

	return ret;
}
void extract_gdt_entry(gdt_entry entry,dword* start,dword* size,byte* type){
	*start=((dword)entry.base_high<<24)|((dword)entry.base_mid<<16)|(dword)entry.base_low;
	*size=((dword)(entry.flags&0x0F)<<16)|(dword)entry.lim_low;
	*type=(entry.access&0b00001000)>>3;
}
byte proc_getFlags(pid proc){
	return processes[proc].flags;
}
FILE** proc_getFiles(pid proc){
	return processes[proc].files;
}

dword allocate_aligned(dword size,word alignment){
	if(alignment<2)
		return allocate(size); // avoid division by zero and unecessary 1-byte alignment
	dword al=alignment;
	dword max_size=0;
	for(dword i=0;i<alloc_map_size;i+=2){
		dword offset=al*(alloc_map[i]/al+1)-alloc_map[i];
		if(alloc_map[i+1]-offset>=size){
			byte* ret=(byte*)alloc_map[i];
			alloc_map[i]+=size+offset;
			alloc_map[i+1]-=size+offset;
			for(dword i=offset;i<size+offset;i++)
				ret[i]=0;
			free((dword)ret,offset);
			return (dword)ret+offset;
		}
		if(alloc_map[i]>max_size)
			max_size=alloc_map[i];
	}
	__asm__ volatile("cli":::);
	printf("OUT OF MEMORY\nLargest contiguous region: ");
	printd(max_size);
	printf(", requested size: ");
	printd(size);
	kinf_loop();
}
dword allocate(dword size){
	dword max_size=0;
	for(dword i=1;i<alloc_map_size;i+=2){
		if(alloc_map[i]>=size){
			i--;
			byte* ret=(byte*)alloc_map[i];
			alloc_map[i]+=size;
			alloc_map[i+1]-=size;
			for(dword i=0;i<size;i++)
				ret[i]=0;
			return (dword)ret;
		}
		if(alloc_map[i]>max_size)
			max_size=alloc_map[i];
	}
	__asm__ volatile("cli":::);
	printf("OUT OF MEMORY\nLargest contiguous region: ");
	printd(max_size);
	printf(", requested size: ");
	printd(size);
	kinf_loop();
}
dword reallocate(dword ptr,dword size,dword new_size){
	if(size==new_size) return ptr;
	dword ret;

	if(new_size<size){
		free(ptr+new_size,size-new_size);
		return ptr;
	}else{
		byte* allocation=0;
		dword allo_index;
		for(dword i=0;i<alloc_map_size;i+=2){
			dword astart=alloc_map[i];
			dword asize=alloc_map[i+1];

			if(ptr+size==astart){ // free region is directly after
				if(asize>=new_size-size){ // there is enough space to grow in place
					for(dword j=0;j<new_size-size;j++)
						((byte*)astart)[j]=0;
					alloc_map[i]+=new_size-size;
					alloc_map[i+1]-=new_size-size;
					return ptr;
				}
			}
			if(asize>=new_size){
				allocation=(byte*)astart;
				allo_index=i;
			}
		}
		// there was not enough space to grow in place
		if(allocation){
			dword i=0;
			alloc_map[allo_index]+=new_size;
			alloc_map[allo_index+1]-=new_size;
			for(;i<size;i++)
				allocation[i]=((byte*)ptr)[i];
			for(;i<new_size;i++)
				allocation[i]=0;
			if(alloc_moving){
				alloc_map_size+=4;
				alloc_moving=0;
				alloc_map=(dword*)allocation;
			}
			free(ptr,size);
			return (dword)allocation;
		}else{
			__asm__ volatile("cli":::);
			panic("OUT OF MEMORY!!!");
			kinf_loop();
		}
	}
}
void free(dword ptr,dword size){
	dword lazy_addr;
	byte lazy=0;
	dword i=0;
	for(;i<alloc_map_size;i+=2){
		dword astart=alloc_map[i];
		dword asize=alloc_map[i+1];

		if(astart+asize==ptr){ // free region is directly before
			alloc_map[i+1]+=size;
			return;
		}else if(ptr+size==astart){ // free region is directly after
			alloc_map[i]-=size;
			alloc_map[i+1]+=size;
			return;
		}else if(!lazy&&asize==0){
			lazy=1;
			lazy_addr=i;
		}
	}
	if(lazy){
		alloc_map[lazy_addr]=ptr;
		alloc_map[lazy_addr+1]=size;
		return;
	}
	// we have to make a new ptr, using reallocate
	alloc_moving=1;
	reallocate((dword)alloc_map,alloc_map_size*sizeof(dword),(alloc_map_size+4)*sizeof(dword));
	if(alloc_moving){
		alloc_map_size+=4;
		alloc_moving=0;
	}
	i+=2;
	alloc_map[i]=ptr;
	alloc_map[i+1]=size;
}

void* allocate_prog(pid proc,dword size){
	if(size==0)
		return 0;
	dword gdt_start;
	dword gdt_size;
	byte gdt_type;
	extract_gdt_entry(processes[proc].segments.data,&gdt_start,&gdt_size,&gdt_type);

	byte region_found=0;
	dword heap_des;
	for(dword i=0;i<HEAP_ARRAY_SIZE;i++){
		if(processes[proc].heap[i].size==0){ // unused heap pointer
			region_found=1;
			heap_des=i;
			break;
		}
	}
	if(!region_found) // no unused heap pointer found
		return 0;

	region_found=0;
	dword free_des;
	for(dword i=0;i<HEAP_ARRAY_SIZE;i++){
		if(processes[proc].anti_heap[i].size>=size){ // sufficient anti-heap region
			region_found|=0x1;
			free_des=i;
			break;
		}
		if(!(region_found&0x2)&&processes[proc].anti_heap[i].size==0){
			region_found|=0x2;
			free_des=i;
		}
	}
	if(region_found&0x1){ // no need to expand heap to make space
		processes[proc].heap[heap_des].ptr=processes[proc].anti_heap[free_des].ptr;
		processes[proc].heap[heap_des].size=size;
		processes[proc].anti_heap[free_des].ptr+=size;
		processes[proc].anti_heap[free_des].size-=size;

		return processes[proc].heap[heap_des].ptr;
	}
	// must expand heap to make space
	if(!(region_found&0x2)){
		if(current_pid==proc) exit(0x10000000|ENOMEM);
		else kill_program(proc,0xFF);
		return 0;
	}

	dword page_size=(size+PAGE_SIZE-1)/PAGE_SIZE;
	dword old_gdt_start=gdt_start;
	gdt_start=reallocate(gdt_start,gdt_size*PAGE_SIZE,(gdt_size+page_size)*PAGE_SIZE);
	void* ret=(void*)(gdt_size*PAGE_SIZE);
	processes[proc].heap[heap_des].ptr=ret;
	processes[proc].heap[heap_des].size=size;
	processes[proc].anti_heap[free_des].ptr=ret+size;
	processes[proc].anti_heap[free_des].size=page_size-size;

	processes[proc].segments.stack=create_user_gdt_entry(gdt_start,gdt_size,gdt_type);
	gdt_size+=page_size;
	processes[proc].segments.data=create_user_gdt_entry(gdt_start,gdt_size,gdt_type);
	if(old_gdt_start!=gdt_start){
		{
			dword d1;
			extract_gdt_entry(processes[proc].segments.exec,&d1,&gdt_size,&gdt_type);
		}
		processes[proc].segments.exec=create_user_gdt_entry(gdt_start,gdt_size,gdt_type);
	}
	if(proc==current_pid)
		reload_gdt(&processes[current_pid].segments);

	return ret;
}
byte free_prog(pid proc,void* ptr){
	byte region_found=0;
	dword heap_des;
	for(dword i=0;i<HEAP_ARRAY_SIZE;i++){
		if(processes[proc].heap[i].ptr==ptr){
			region_found=1;
			heap_des=i;
			break;
		}
	}
	if(!region_found) // pointer doesn't exist
		return 0;

	region_found=0;
	dword free_des;
	for(dword i=0;i<HEAP_ARRAY_SIZE;i++){
		dword p=(dword)(processes[proc].anti_heap[i].ptr);
		dword s=processes[proc].anti_heap[i].size;
		if(p+s==(dword)ptr){ // free region is before
			region_found|=0x1;
			free_des=i;
			break;
		}
		if(s==0){
			region_found|=0x2;
			free_des=i;
		}
	}
	if(region_found&0x1){ // expand existing anti-heap region
		processes[proc].anti_heap[free_des].size+=processes[proc].heap[heap_des].size;
		processes[proc].heap[heap_des].size=0;
		return 1;
	}else if(region_found&0x2){
		processes[proc].anti_heap[free_des].ptr=processes[proc].heap[heap_des].ptr;
		processes[proc].anti_heap[free_des].size=processes[proc].heap[heap_des].size;
		processes[proc].heap[heap_des].size=0;
		return 1;
	}
	return 0;
}


byte schedule_program(proc_gdt segments,pid* result_pid){
	pid new_pid;
	byte pid_found=0;
	for(pid i=0;i<TASK_LIMIT;i++){
		if(tasks[i].active==0){
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

		if(i==0&&entry_type!=GDT_ENTRY_TYPE_EXEC) return EINVALID;
		if(i>0&&entry_type!=GDT_ENTRY_TYPE_DATA) return EINVALID;
		if(entry_start<kernel_finish) return EFORBIDDEN;
		if((segments.entries[i].access&0b01100000)!=0b01100000) return EFORBIDDEN;
	}

	dword code_base;
	dword code_size;
	byte code_type;
	extract_gdt_entry(segments.exec,&code_base,&code_size,&code_type);

	task_state state={
		0x2B,
		0x2B,
		0x2B,
		0x2B,

		0,
		0,
		0,
		1,

		0,
		0,
		0,
		0,

		0,
		0x23,
		0x202,

		entry_size*PAGE_SIZE,
		0x33
	};
	tasks[new_pid]=state;

	process proc={
		0,
		current_pid,
		segments,
		(FILE**)allocate(FILE_ARRAY_SIZE*sizeof(FILE*)),
		(region*)allocate(HEAP_ARRAY_SIZE*sizeof(region)),
		(region*)allocate(HEAP_ARRAY_SIZE*sizeof(region)),
		0
	};
	processes[new_pid]=proc;

	*result_pid=new_pid;
	return 0;
}



extern byte test_program;
extern byte test_program_end;
pid dummy_program(void){
	dword target=allocate(2*PAGE_SIZE);
	if(!target) return 0;

	gdt_entry code=create_user_gdt_entry(target,1,GDT_ENTRY_TYPE_EXEC);
	gdt_entry data=create_user_gdt_entry(target,2,GDT_ENTRY_TYPE_DATA);
	gdt_entry stack=create_user_gdt_entry(target,2,GDT_ENTRY_TYPE_DATA);

	proc_gdt proc={code,data,stack};
	for(dword i=0;i<(dword)(&test_program_end-&test_program);i++)
		((volatile byte*)target)[i]=(&test_program)[i];

	pid ret;
	schedule_program(proc,&ret);

	return ret;
}



// ================================ -PROCESS MANAGEMENT- ================================
byte kernel_prompt(char* title,char** options,byte length){
	pid pre_pid=current_pid;
	current_pid=0; // so this doesn't get interrupted

	word* buffer=(word*)allocate(4000);
	word* vga=(word*)0xB8000;
	for(word i=0;i<2000;i++)
		buffer[i]=vga[i]; // save the current screen


	word cursor=G_Printing_TextCursor;
	byte attr=G_Printing_TextAttribute;
	byte selection=0;
	byte done=0;
	while(!done){
		clear();
		printf("\eCF;");
		printf(title);
		printf("\n\eCrgb;You have ");
		printn(length);
		printf(" options:");
		for(byte i=0;(i<=selection||getcursorpos()>>8<24)&&i<length;i++){
			printc('\n');
			if(i==selection)
				printf("\eCRGB;");
			else
				printf("\eCrgb;");
			printn(i+1);
			printf(") ");
			printf(options[i]);
		}

		byte moved=0;
		while(!moved){
			byte c=getchar();
			if(c==1){
				if(selection>0){
					moved=1;
					selection--;
				}else if(length>2){
					moved=1;
					selection=length-1;
				}
			}else if(c==3){
				if(selection<length-1){
					moved=1;
					selection++;
				}else if(length>2){
					moved=1;
					selection=0;
				}
			}else if(c=='\n'){
				moved=1;
				done=1;
			}
		}
	}


	for(word i=0;i<2000;i++)
		vga[i]=buffer[i];
	free((dword)buffer,4000);
	G_Printing_TextCursor=cursor;
	G_Printing_TextAttribute=attr;
	current_pid=pre_pid;
	return selection;
}
pid exec_file(FILE* file){
	if(!file_permission(F_PERM_EXECUTE,file))
		return 0; // already checks if ptr valid

	dword size=get_file_header(file).size;
	if(size<4)
		return 0; // can't possibly contain the signature
	byte sign[4];
	if(!read_file(file,0,4,sign))
		return 0;
	if(
		sign[0]!='A'||
		sign[1]!='C'||
		sign[2]!='O'||
		sign[3]!='S'
	) return 0;
	dword code_size=PAGE_SIZE*((size-4+PAGE_SIZE-1)/PAGE_SIZE); // remove dword, round up to PAGE_SIZE
	dword mem=allocate(code_size+STACK_SIZE);
	// unfortunately, only since i reached here is when i realized that
	// allocate() can't possibly fail. if it does, that's handled specially,
	// and the caller never sees that it failed
	if(!read_file(file,4,code_size,(byte*)mem)){
		free(mem,code_size+STACK_SIZE);
		return 0;
	}

	gdt_entry code_gdt=create_user_gdt_entry(mem,code_size/PAGE_SIZE,GDT_ENTRY_TYPE_EXEC);
	gdt_entry data_gdt=create_user_gdt_entry(mem,(code_size+STACK_SIZE)/PAGE_SIZE,GDT_ENTRY_TYPE_DATA);
	gdt_entry stack_gdt=create_user_gdt_entry(mem,(code_size+STACK_SIZE)/PAGE_SIZE,GDT_ENTRY_TYPE_DATA);

	proc_gdt proc={code_gdt,data_gdt,stack_gdt};

	pid ret;
	byte err=schedule_program(proc,&ret);
	if(err)
		return 0;

	return ret;
}

byte kill_program_override=0;
char kill_program(pid program,byte urgent){
	if(current_pid==program&&!kill_program_override) return -1;
	if(current_pid>0){ // if you are not kernel
		if(!(processes[current_pid].flags&1)&&processes[program].flags&1)
			return -2; // user processes can't kill root processes
		byte is_descendant=1;

		if(current_pid!=1){ // if you are not init (ultimate ancestor)
			pid anc=program;
			while(anc!=current_pid){ // check if you are ancestor
				anc=processes[program].parent;
				if(anc==1){
					is_descendant=0;
					break;
				}
			}
		}
		// if the program is your descendant, or you are root and the program isn't, continue
		if(!(is_descendant||(processes[current_pid].flags&1&&!(processes[program].flags&1))))
			return -3; // this process isn't yours to kill
		if(processes[program].flags&4&&!urgent) // the process really wants to live
			return -4;
	}
	// proceed to kill the process

	tasks[program].active=0; // simplest way to delete the process

	dword start; // clean up memory
	dword size;
	byte type;
	extract_gdt_entry(processes[program].segments.exec,&start,&size,&type);
	free(start,size); // clean up code
	extract_gdt_entry(processes[program].segments.data,&start,&size,&type);
	free(start,size); // clean up data (just in case it's different)
	extract_gdt_entry(processes[program].segments.stack,&start,&size,&type);
	free(start,size); // clean up stack

	// clean up files
	for(fdesc i=0;i<FILE_ARRAY_SIZE;i++)
		if(processes[program].files[i])
			fclose_desc(program,i);
	free((dword)processes[program].files,FILE_ARRAY_SIZE*sizeof(FILE*));

	// kill children
	for(pid i=2;i<TASK_LIMIT;i++)
		if(processes[i].parent==program)
			kill_program(i,1);

	return 1;
}
_Noreturn void exit(dword code){
	printf("{exit: ");
	printd(code);
	printc('}');
	extern _Noreturn void kasm_select_next_process(byte irq,task_state* state);
	kill_program_override=1;
	kill_program(current_pid,0xFF);
	kill_program_override=0;
	kasm_select_next_process(0x20,&tasks[current_pid]); // jump to the next program, handled by the scheduler
}

void delay(dword time){
	return;
}




extern byte filesystem_new_root;
extern byte filesystem_new_root_end;
void system_init(void){
	printf("Hard disk");
	if(ahci_init()){
		printf(" [\eCqr;FAILED\eCf;]\n");
		for(;;);
	}
	printf(" [\eCqg;OK\eCF;]\n");


	char* options[2]={"No","Yes"};
	if(kernel_prompt("Are you booting from a USB drive?",options,2)){
		if(kernel_prompt("Would you like to overwrite your installation with the one on the USB drive?",options,2)){
			clear();
			if(ahci_write(0,0x14000,(byte*)0x7C00))
				printf("\eCqg;The write was successful!\n");
			else{
				printf("\eCqr;The write has failed!");
				for(;;);
			}
		}
	}


	printf("Filesystem");
	if(file_init()){
		printf(" [\eCQqrg;ACTION REQUIRED\eCF;]\n\n\eCrgb;This computer appears to not have an ACOS filesystem!\nYou have a few options:\n");
		byte selection=0;
		while(selection<0x80){
			if(selection==0) printf("\eCRGB;");
			else printf("\eCrgb;");
			printf("1. Do nothing");
			printf("\eC;\n");

			if(selection==1) printf("\eCRGB;");
			else printf("\eCrgb;");
			printf("2. Enter recovery mode (only select this if you know what you're doing!)");
			printf("\eC;\n");

			if(selection==2) printf("\eCRGB;");
			else printf("\eCrgb;");
			printf("3. Format the hard disk with the ACOS filesystem");

			byte c=getchar();
			if(c==1&&selection>0) selection--;
			else if(c==3&&selection<2) selection++;
			else if(c=='\n') selection|=0x80;

			printf("\eC;\x1\x1\r");
		}
		selection&=3;
		printf("\x3\x3\eF;\x1\eF;\x1\eF;\x1\eF;\x1\eF;\x1\eF;\x1\eF;\eCF;");

		if(selection==0){
			printf("Filesystem [\eCqr;FAILED\eCF;]");
			for(;;);
		}else if(selection==1){
			clear();
			printf("\eCF;RECOVERY MODE\n\n");
		}else if(selection==2){
			printf("Filesystem [\eCQqrg;ACTION REQUIRED\eCF;]\n\n\eCrgb;If you have not backed up your data, please power off the computer and do so now, otherwise press enter to proceed with the formatting.");
			while(getchar()!='\n');
			printf("\eF;\x1\eF;\x1\x1\eF;\eCF;Filesystem [\eCQg;RESOLVING\eCF;]");

			if(
				ahci_write(0x1B0,12,"\0\x10\0\0" FILESYSTEM_SIGNATURE)&&
				ahci_write(0x1000,(dword)(&filesystem_new_root_end-&filesystem_new_root),&filesystem_new_root)
			){
				if(!file_init()) printf("\eF;Filesystem [\eCg;RESOLVED\eCF;]");
				else{
					printf("\eF;Filesystem [\eCr;RESOLUTION FAILED\eCF;]");
					for(;;);
				}
			}else{
				printf("\eF;Filesystem [\eCr;RESOLUTION FAILED\eCF;]");
				for(;;);
			}
		}
	}else printf(" [\eCqg;OK\eCf;]");
	printf("\n\n");
}



task_state* emergency_shell(void){
	FILE* rooty=get_root_pointer();

	byte fileName[256];
	byte input[128]={0};
	byte p=0;
	FILE* currentDir=rooty;
	printf("\n> ");
	for(;;){
		byte c=getchar();
		if(c=='\b'){
			if(p>0){
				p--;
				input[p]=0;
				printc(c);
			}
		}else if(c=='\n'){
			printc(c);
			for(p=0;p<128;p++) input[p]=(input[p]==' ')?0:input[p];
			p=0;
			while(p<128&&input[p]!=0) p++;
			while(p<128&&input[p]==0) p++;
			dword p2=p;
			while(p2<128&&input[p2]!=0) p2++;
			while(p2<128&&input[p2]==0) p2++;

			if(cmpstr(input,"ls")){
				printf("There are ");
				byte entries=get_directory_entries(currentDir);
				printh(entries);
				printf(" files in '");
				get_file_name(currentDir,fileName);
				printf(fileName);
				printf("':\n");

				for(byte i=0;i<entries;i++){
					FILE* file=get_directory_entry_from_id(currentDir,i);
					if(file->flags&1)
						printc('*');

					printw(get_file_size(file));
					printc(' ');
					byte type=get_file_type(file);
					if(type==F_TYPE_FILE)
						printf("FILE: ");
					else if(type==F_TYPE_DIR)
						printf("FOLDER: ");
					else if(type==F_TYPE_SYS)
						printf("SYSTEM FILE: ");
					else
						printf("UNKNOWN TYPE: ");
					get_file_name(file,fileName);
					printf(fileName);
					printc('\n');
				}
			}else if(cmpstr(input,"print")){
				printf(input+p);
			}else if(cmpstr(input,"cd")){
				printf("You are now in ");
				if(cmpstr(input+p,".."))
					currentDir=get_file_parent(currentDir);
				else{
					FILE* newDir=get_directory_entry_from_name(currentDir,input+p);
					if(newDir&&get_file_type(newDir)==F_TYPE_DIR)
						currentDir=newDir;
				}
				get_file_name(currentDir,fileName);
				printf(fileName);
				printc('.');
			}else if(cmpstr(input,"create")){
				char res=create_file(currentDir,0x07,input+p);
				if(res>0){
					printf("The file '");
					printf(input+p);
					printf("' was successfully created.");
				}else{
					printf("Unable to create '");
					printf(input+p);
					printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
					printh(res);
					for(;;);
				}
			}else if(cmpstr(input,"mkdir")){
				char res=create_file(currentDir,0x47,input+p);
				if(res>0){
					printf("The directory '");
					printf(input+p);
					printf("' was successfully created.");
				}else{
					printf("Unable to create '");
					printf(input+p);
					printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
					printh(res);
					for(;;);
				}
			}else if(cmpstr(input,"rename")){
				FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					char res=rename_file(file,input+p2);
					if(res>0){
						printf("The file '");
						printf(input+p);
						printf("' was renamed to '");
						printf(input+p2);
						printc('\'');
					}else{
						printf("Unable to rename '");
						printf(input+p);
						printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
						printh(res);
						for(;;);
					}
				}
			}else if(cmpstr(input,"delete")){
				FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					char res=delete_file(file);
					if(res>0){
						printf("The file '");
						printf(input+p);
						printf("' was successfully deleted.");
					}else{
						printf("Unable to create '");
						printf(input+p);
						printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
						printh(res);
						for(;;);
					}
				}
			}else if(cmpstr(input,"write")){
				FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					dword size=0;
					for(;(input+p2)[size]!=0;size++);
					dword size2=overwrite_file(file,size,input+p2);
					printf("Wrote '");
					printf(input+p2);
					printf("' (");
					printd(size);
					printf(" bytes) to the file.");
				}
			}else if(cmpstr(input,"sync")){
				printf("Syncing files...");
				dword filesSynced=sync_files();
				if(filesSynced)
					while(sync_files()) printc('.');
				printf(" Synced ");
				printh(filesSynced);
				printf(" files!");
			}else if(cmpstr(input,"cat")){
				FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					dword size=get_file_size(file);
					dword allocation=allocate(size);
					read_file(file,0,size,(byte*)allocation);
					for(dword i=0;i<size;i++){
						printh(((byte*)allocation)[i]);
						printc(' ');
					}
					free(allocation,size);
				}
			}else if(cmpstr(input,"dump")){
				byte valid;
				dword num=str_to_dword(input+p,&valid);
				if(!valid)
					printf("Invalid number");
				else{
					byte buffer[512];
					ahci_read(num*512,512,buffer);
					clear();
					printf("Contents of sector 0x");
					printd(num);
					printf(" (offset: 0x");
					printd(num*0x200);
					printf("):");
					for(word i=0;i<512;i++){
						if(i%24==0){
							printc('\n');
							printw(i);
							printf(": ");
						}else
							printc(' ');
						printh(buffer[i]);
					}
				}
			}else if(cmpstr(input,"memdump")){
				byte valid;
				byte valid2;
				dword num=str_to_dword(input+p,&valid);
				dword num2=str_to_dword(input+p2,&valid2);
				if(!valid||!valid2)
					printf("Invalid number");
				else{
					clear();
					printf("Contents of memory at 0x");
					printd(num);
					printf(":");
					for(byte* i=(byte*)num;(dword)i<num+num2;i++){
						if((dword)i%16==0){
							printc('\n');
							printd((dword)i);
							printf(": ");
						}else
							printc(' ');
						printh(*i);
					}
				}
			}else if(cmpstr(input,"beep")){
				byte valid;
				byte valid2;
				dword num=str_to_dword(input+p,&valid);
				dword num2=str_to_dword(input+p2,&valid2);
				if(!valid||!valid2)
					printf("Invalid number");
				else{
					beep(num,num2);
				}
			}else if(cmpstr(input,"exec")){
				FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					printf("Executing ");
					printf(input+p);
					printf("\n");
					pid sid=exec_file(file);
					if(sid){
						current_pid=sid;
						reload_gdt(&processes[sid].segments);
						processes[current_pid].flags|=3;
						return &tasks[sid];
					}
				}
			}else{
				printf("\eCqrg;Unknown command!\eCF;");
			}

			printf("\n> ");
			for(p=0;p<128;p++) input[p]=0;
			p=0;
		}else if(p<128){
			input[p]=c;
			p++;
			printc(c);
		}
	}
}
extern char _linker_bss_start[];
extern char _linker_bss_end[];
#define SCHEDULER_FREQUENCY 1000
task_state* kmain(void){
	for(char* i=_linker_bss_start+0x4000;i<_linker_bss_end;i++) *i=0;
	// we do 0x4000 here because that's the size of the kernel's stack
	printf("Memory");
	current_pid=0;
	foreground_pid=1;
	{
		dword alloc_map_startoff[2]={kernel_finish,0x80000000-kernel_finish};
		alloc_map=(dword*)alloc_map_startoff;
		alloc_map_size=2;
		alloc_moving=0;
		free(0,0); // move the allocation table out of kernel memory
		for(dword i=0;i<alloc_map_size;i+=2){
			if(alloc_map[i]==(dword)alloc_map_startoff){
				alloc_map[i+1]=0;
				break;
			}
		}
	}
	settimer(1193180/SCHEDULER_FREQUENCY);

	printf(" [\eCqg;OK\eCf;]\n");
	tasks[0].active=1;
	processes[0].files=(FILE**)allocate(FILE_ARRAY_SIZE);

	/*current_pid=dummy_program();
	processes[current_pid].flags|=2;
	reload_gdt(&processes[current_pid].segments);
	return &tasks[current_pid];*/
	system_init();

	FILE* root_file=get_root_pointer();
	FILE* init_file=get_directory_entry_from_name(root_file,"init.ace");
	if(!init_file){
		panic("No init file found! Entering recovery mode.\n");
		fclose_ptr(root_file);
	}else{
		pid sid=exec_file(init_file);
		if(!sid){
			panic("Invalid init file! Entering recovery mode.\n");
			fclose_ptr(root_file);
			fclose_ptr(init_file);
		}else{
			current_pid=sid;
			processes[current_pid].flags|=3;
			reload_gdt(&processes[current_pid].segments);
			/*printf("Everything's loaded. Check 0x");
			dword start;
			dword size;
			byte type;
			extract_gdt_entry(processes[current_pid].segments.exec,&start,&size,&type);
			printn(start);
			printf(", ");
			printn(size);
			printf(" bytes.\n");*/
			return &tasks[current_pid];
		}
	}

	return emergency_shell();
}



_Noreturn void exception_handler(byte type,dword addr,dword cs){
	if((cs&3)!=3){
		if(type==8){
			panic("FATAL DOUBLE FAULT");
			for(;;);
		}else{
			if(type==0xD&&*(byte*)addr==0x65)
				exit(0x20000000|addr); // GS fault, ultimately caused by the program, not the kernel

			panic("Your computer suffered an unrecoverable error and needs to shut down!\n");
			switch(type){
				case 0:
					printf("I guess division by zero really is impossible...");
					break;
				case 1:
					printf("Apparently, this is intended..?");
					break;
				case 2:
					printf("What in the world are you doing?!");
					break;
				case 3:
					printf("SOMEBODY left a breakpoint in the kernel.");
					break;
				case 4:
					printf("A trophy to you for specifically trying to cause this!");
					break;
				case 5:
					printf("I don't know what 'BOUND' is, and apparently you don't either.");
					break;
				case 6:
					printf("Oh dear, this doesn't look too good...");
					break;
				case 7:
					printf("Yep. Time to go re-inspect every line of driver code.");
					break;
				case 9:
					printf("Why? Do you just like this blue background? (honestly I don't blame you)");
					break;
				case 10:
					printf("Oh no. Ohhh no no no... This is a nightmare...");
					break;
				case 11:
					printf("You loaded a non-present GDT segment! Happens to the best of us!");
					break;
				case 12:
					printf("If you are a developer,\ngo to the very end of kernel.asm,\nand increase the value in front of 'resb' from 0x4000 to something bigger.");
					break;
				case 13:
					printf("Is ring 0 not enough privileges for you?");
					break;
				case 14:
					printf("You know these aren't literal pages from a book, right?");
					break;
				case 15:
					printf("Ooh, I wonder if this became a real fault.");
					break;
				case 16:
					printf("Since when does a kernel need floating-point numbers?");
					break;
				case 17:
					printf("Have you seen the filesystem format? I don't care about alignment! Turn it off!");
					break;
				case 18:
					printf("I'm not lying. It's your computer that suffered the error, not the kernel.\nI'm really sorry this happened.");
					break;
				case 19:
					printf("You're too modern! Stop!");
					break;
				case 20:
					printf("Scared to install the kernel on your real computer? I won't bite, I promise.");
					break;
				default:
					printf("I'm panicking! Oh no!");
					break;
			}
			printf("\n\nPlease take a picture of this screen and send it to the developer of ACOS, then shut down the computer:\n");
			text_background(0x10); // blue background
			printf("\eCrgBq;INT ");
			printh(type);
			printf(" TRIGGERED BY 0x");
			printd(addr);
			printf(": ");
			for(byte* i=(byte*)addr;(dword)i<addr+128;i++){
				if(((dword)i-addr)%16==0){
					printc('\n');
					printd((dword)i);
					printf(": ");
				}else
					printc(' ');
				printh(*i);
			}
			kinf_loop();
		}
	}else exit(-(dword)type);
}

extern byte G_Keyboard_KeyFlags;
static volatile byte getchar_flagsbuffer;
static volatile byte getchar_keybuffer;
static volatile byte getchar_id;
byte getchar(void){
	byte id=getchar_id;
	__asm__ volatile("sti":::);
	while(id==getchar_id) __asm__ volatile("hlt":::);
	__asm__ volatile("cli":::);
	byte ret=getchar_keybuffer;

	if(getchar_flagsbuffer&1) // ctrl
		ret&=0x1F;
	if(getchar_flagsbuffer&4) // alt
		ret|=0x80;

	return ret;
}
task_state* IRQ_handler(byte irq,task_state* state){
	if(beeping){
		if(beep_time==0){
		beep_stop();
		beeping=0;
		}else beep_time--;
	}
	byte privilege=(byte)(state->cs&(word)3);
	if(privilege==(byte)3&&irq==0x20){
		/* The timer has interrupted a program and brought control here
		 * That program was running in User mode
		*/
		tasks[current_pid]=*state;
		current_pid++;

		for(;current_pid<TASK_LIMIT;current_pid++)
			if(tasks[current_pid].active&&processes[current_pid].delay==0) break;

		if(current_pid==TASK_LIMIT){
			current_pid=1;
			for(;current_pid<TASK_LIMIT;current_pid++)
				if(tasks[current_pid].active&&processes[current_pid].delay==0) break;
			if(current_pid==TASK_LIMIT){
				printf("\eCr;SCHEDULER FAULT\eCf;");
				return emergency_shell();
			}
		}
		reload_gdt(&processes[current_pid].segments);
		return &tasks[current_pid];
	}else if(irq==0x21){
		getchar_keybuffer=getchar_noint();
		getchar_flagsbuffer=G_Keyboard_KeyFlags;
		if(getchar_keybuffer) getchar_id++;
	}
	return state;
}

dword kos_handler(dword function,dword args[4]){
	dword a=args[0];
	dword b=args[1];
	dword c=args[2];
	dword d=args[3];

	dword data_offset;
	{
		dword size;
		byte type;
		extract_gdt_entry(processes[current_pid].segments.data,&data_offset,&size,&type);
	}

	switch(function){
			// I/O
		case 0:
			exit(a);
		case 1:
			if(current_pid==foreground_pid) setcursorpos(a,b);
			else return EFORBIDDEN;
			break;
		case 2:
			if(current_pid==foreground_pid) scroll(a);
			else return EFORBIDDEN;
			break;
		case 3:
			if(current_pid==foreground_pid) printc(a);
			else return EFORBIDDEN;
			break;
		case 4:
			if(current_pid==foreground_pid) printf_prog((char SEG_GS*)a);
			else return EFORBIDDEN;
			break;
		case 5:
			if(current_pid==foreground_pid) printh(a);
			else return EFORBIDDEN;
			break;
		case 6:
			if(current_pid==foreground_pid) printw(a);
			else return EFORBIDDEN;
			break;
		case 7:
			if(current_pid==foreground_pid) printd(a);
			else return EFORBIDDEN;
			break;
		case 8:
			if(current_pid==foreground_pid) printn(a);
			else return EFORBIDDEN;
			break;
		case 9:
			if(current_pid==foreground_pid) clear();
			else return EFORBIDDEN;
			break;
		case 10:
			if(current_pid==foreground_pid) panic_prog((char SEG_GS*)a);
			else return EFORBIDDEN;
			break;
		case 11:
			if(processes[current_pid].flags&2) return getchar();
			else return EFORBIDDEN;
		case 12:
			return cmpstr_prog((char SEG_GS*)a,(char SEG_GS*)b);

			// Files
		case 13:
			return get_root_pointer_prog();
		case 14:
			return read_file_prog(processes[current_pid].files[a],b,c,(byte SEG_GS*)d);
		case 15:
			return fclose_desc(current_pid,a);
		case 16:
			return get_directory_entries(processes[current_pid].files[a]);
		case 17:
			return get_directory_entry_from_id_prog(processes[current_pid].files[a],b);
		case 18:
			return get_directory_entry_from_name_prog(processes[current_pid].files[a],(char SEG_GS*)b);
		case 19:
			return get_file_name_prog(processes[current_pid].files[a],(byte SEG_GS*)b);
		case 20:
			return get_file_flags(processes[current_pid].files[a]);
		case 21:
			return get_file_size(processes[current_pid].files[a]);
		case 22:
			return get_file_parent_prog(current_pid,processes[current_pid].files[a]);
		case 23:
			return create_file_prog(processes[current_pid].files[a],b,(char SEG_GS*)c);
		case 24:
			return delete_file(processes[current_pid].files[a]);
		case 25:
			return rename_file_prog(processes[current_pid].files[a],(char SEG_GS*)b);
		case 26:
			return overwrite_file_prog(processes[current_pid].files[a],b,(char SEG_GS*)c);
		case 27:
			return write_file_prog(processes[current_pid].files[a],b,c,(char SEG_GS*)d);
		case 28:
			return trim_file(processes[current_pid].files[a],b,c);
		case 29:
			return sync_files();

			// Processes
		case 30:
			return exec_file(processes[current_pid].files[a]);
		case 31:
			return kill_program(a,b);
		case 32:
			if(current_pid==foreground_pid) beep(a,b);
			else return EFORBIDDEN;
			break;
		case 33:
			return proc_getFlags(current_pid);
		case 34:
			return (dword)allocate_prog(current_pid,a);
		case 35:
			return free_prog(current_pid,(void*)a);

		default: return EINVALID;
	}

	return 0;
}

/*
 * I/O
 * 0: void exit(void);
 * 1: void setcursorpos(byte x,byte y);
 * 2: void scroll(dword lines);
 * 3: void printc(byte c);
 * 4: void printf(char* str);
 * 5: void printh(byte c);
 * 6: void printw(word c);
 * 7: void printd(dword c);
 * 8: void printn(dword c);
 * 9 void clear(void);
 * 10: void panic(char* str);
 * 11: byte getchar(void);
 * 12: byte cmpstr(char* str1,char* str2);
 *
 * Files
 * 13: FILE* fget_root(void);
 * 14: byte read_file(FILE* file,dword size,byte* buffer);
 * 15: byte fclose(fdesc desc);
 * 16: dword get_directory_entries(FILE* directory);
 * 17: FILE* get_directory_entry_from_id(FILE* directory,dword entryNumber);
 * 18: FILE* get_directory_entry_from_name_prog(FILE* directory,char SEG_GS* name);
 * 19: byte get_file_name_prog(FILE* file,byte SEG_GS* buffer);
 *
 * Processes
 * 20: pid exec(FILE* file);
*/

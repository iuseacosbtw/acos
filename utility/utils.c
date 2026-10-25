#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>


typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

typedef dword pid;
typedef dword fdesc;

typedef struct{
	dword addr;
	dword count;
	byte flags;
	/* flags;
	 * 7-1: Reserved
	 * 0: Cached
	*/
}ACOS_FILE; // used by kernel
// a ACOS_FILE* points to this kernel struct
// I know, I know. This isn't in file.h because of the proc_getFiles() function
ACOS_FILE** proc_getFiles(pid proc);
#define PAGE_SIZE 4096
#define ACOS_FILE_ARRAY_SIZE (PAGE_SIZE/sizeof(ACOS_FILE*))

// Directory entries are structered like so:

// dword addr; // address of the file
// char* name; // null-terminated string of the file's name

typedef struct{
	dword size; // size of the file's data
	dword parent_addr; // parent's address on disk
	dword file_addr; // the file's own address on disk
	byte flags;
	/* flags;
	 * 7-6: File type. 00b=File, 01b=Directory, 10b=System File, 11b=Symbolic Link
	 * 5: Uncacheable
	 * 4-3: Reserved
	 * 2-0: User RWX
	*/
}__attribute__((packed)) file_header;

byte file_init(void);

file_header get_file_header(ACOS_FILE* file);
byte file_permission(byte req,ACOS_FILE* file);
dword cleanup_files(void);

ACOS_FILE* get_root_pointer(void);
fdesc get_root_pointer_prog(void);
dword read_file(ACOS_FILE* file,dword start,dword size,byte* buffer);
byte fclose_ptr(ACOS_FILE* ptr);
byte fclose_desc(pid proc,fdesc desc);
dword get_directory_entries(ACOS_FILE* directory);
ACOS_FILE* get_directory_entry_from_id(ACOS_FILE* directory,dword entryNumber);
fdesc get_directory_entry_from_id_prog(ACOS_FILE* directory,dword entryNumber);
ACOS_FILE* get_directory_entry_from_name(ACOS_FILE* directory,char* name);
//ACOS_FILE* resolve_path(char* str);
byte get_file_name(ACOS_FILE* file,byte* buffer);
ACOS_FILE* get_file_parent(ACOS_FILE* file);
byte get_file_type(ACOS_FILE* file);
dword get_file_size(ACOS_FILE* file);

dword overwrite_file(ACOS_FILE* file,dword size,byte* buffer);
dword write_file(ACOS_FILE* file,dword addr,dword size,byte* buffer);
dword trim_file(ACOS_FILE* file,dword start,dword size);
dword sync_files(void);

char create_file(ACOS_FILE* parent,byte permissions,char* name);
char delete_file(ACOS_FILE* file);
char rename_file(ACOS_FILE* file,char* name);

dword fileAllocate(dword size);
byte fileFree(dword addr,dword size);

ACOS_FILE* dummyFile(void);
#define FILESYSTEM_SIGNATURE "KORDOGGY"

#define F_PERM_READ 0
#define F_PERM_WRITE 1
#define F_PERM_EXECUTE 2

#define F_TYPE_FILE 0x01
#define F_TYPE_DIR 0x02
#define F_TYPE_SYS 0x03

#define F_FLAGS_TYPE_FILE 0x00
#define F_FLAGS_TYPE_DIR 0x40
#define F_FLAGS_TYPE_SYS 0x80
#define F_FLAGS_UNCACHEABLE 0x20
#define F_FLAGS_USER_R 0x04
#define F_FLAGS_USER_W 0x02
#define F_FLAGS_USER_X 0x01




static dword root;
static ACOS_FILE* allocsys;
static dword current_pid=0;

static FILE* disk=NULL;
static byte* memory=NULL;


byte ahci_read(dword addr, dword size, byte *buffer){
	fseek(disk,addr,SEEK_SET);
	fread(buffer,sizeof(char),size,disk);
	return 1;
}
byte ahci_write(dword addr, dword size, byte *buffer){
	fseek(disk,addr,SEEK_SET);
	fwrite(buffer,sizeof(char),size,disk);
	return 1;
}
/*dword allocate(dword size){
	return (dword)calloc(size,sizeof(char));
}
void acos_free(dword ptr,dword size){
	free((char*)ptr);
}*/
#define STACK_SIZE 65536
#define TASK_LIMIT 256

static dword alloc_map_startoff[2]={0x300000,0x80000000-0x300000};
static dword* alloc_map=alloc_map_startoff;
static dword alloc_map_size=2;
static byte alloc_moving=0;
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
	printf("OUT OF MEMORY");
	for(;;);
}
void acos_free(dword ptr,dword size);
dword reallocate(dword ptr,dword size,dword new_size){
	if(size==new_size) return ptr;
	dword ret;

	if(new_size<size){
		acos_free(ptr+new_size,size-new_size);
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
			acos_free(ptr,size);
			return (dword)allocation;
		}else{
			__asm__ volatile("cli":::);
			printf("OUT OF MEMORY!!!");
			for(;;);
		}
	}
}
void acos_free(dword ptr,dword size){
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






#define MAX_ACOS_FILEPTRS (1<<16)
static ACOS_FILE* fileList;

ACOS_FILE dwtofile(dword dw){
	ACOS_FILE file={dw};
	return file;
}

static const dword kernel_finish=0x300000;
static byte ptrValid(ACOS_FILE* file){
	if(!(
		file>=fileList&&
		(dword)file<(dword)fileList+MAX_ACOS_FILEPTRS*sizeof(ACOS_FILE)&&
		((dword)file-(dword)fileList)%sizeof(ACOS_FILE)==0&&
		file->addr
	)) return 0;
	if(file->flags&1)
		return (file->addr>=kernel_finish&&((file_header*)file->addr)->file_addr>0x200);
	else{
		dword buffer=0;
		ahci_read(file->addr+offsetof(file_header,file_addr),sizeof(dword),(byte*)(&buffer));
		return (buffer>0x200);
	}
}
/*static byte ptrExists(ACOS_FILE file){
	for(dword i=0;i<MAX_ACOS_FILEPTRS;i++)
		if(fileList[i].flags&1){
			if(((file_header*)(fileList[i].addr))->file_addr==file.addr)
				return 1;
		}else{
			if(fileList[i].addr==file.addr)
				return 1;
		}
	return 0;
}*/

byte file_permission(byte req,ACOS_FILE* file){
	return 1;
}

static byte cache_file(ACOS_FILE* file){
	if(!ptrValid(file)) return 0;
	if(file->flags&1) return 1;
	//printf("\eCb;C\eCf;");

	dword size=sizeof(file_header)+get_file_header(file).size;
	if(size>0x100000) for(;;);
	byte* buffer=(byte*)allocate(size);

	if(ahci_read(file->addr,size,buffer)){
		file->addr=(dword)buffer;
		file->flags|=1;
		//printf("\eCqb;C\eCf;");
		return 1;
	}else{
		//printf("\eCqb;C\eCf;");
		return 0;
	}
}
static byte uncache_file(ACOS_FILE* file){
	if(!ptrValid(file)) return 0;
	if(!(file->flags&1)) return 1;
	file_header header=*(file_header*)(file->addr);

	acos_free(file->addr,sizeof(file_header)+header.size);
	file->addr=header.file_addr;
	file->flags&=0xFE;
	return 1;
}
static char flush_cache(ACOS_FILE* file);
dword cleanup_files(void){
	dword creationLocation=0xFFFFFFFF;
	byte spaceFound=0;
	for(dword i=0;i<MAX_ACOS_FILEPTRS;i++){
		if(fileList[i].count==0){
			if(!spaceFound){
				spaceFound=1;
				creationLocation=i;
			}
			if(fileList[i].flags&1){
				flush_cache(fileList+i);
				uncache_file(fileList+i);
			}
			fileList[i].flags&=0xF8;
			fileList[i].addr=0;
		}
	}
	return creationLocation;
}
ACOS_FILE** proc_getFiles(pid proc){
	return NULL;
}
static fdesc add_ptr(ACOS_FILE* file){
	ACOS_FILE** files=proc_getFiles(current_pid);
	dword creationLocation;
	byte spaceFound=0;
	for(fdesc i=1;i<ACOS_FILE_ARRAY_SIZE;i++){
		if(!spaceFound&&!ptrValid(files[i])){
			spaceFound=1;
			creationLocation=i;
		}else{
			if(files[i]==file){
				file->count++;
				return i;
			}
		}
	}


	if(spaceFound){
		files[creationLocation]=file;
		file->count++;
		return creationLocation;
	}

	return 0;
}
static fdesc get_desc(pid proc,ACOS_FILE file){
	dword creationLocation;
	byte spaceFound=0;
	for(dword i=1;i<MAX_ACOS_FILEPTRS;i++){ //i=1 here because of 'return 0;' at the end
		if(!spaceFound&&!fileList[i].addr){
			spaceFound=1;
			creationLocation=i;
		}else{
			if(fileList[i].flags&1){
				if(((file_header*)(fileList[i].addr))->file_addr==file.addr)
					return add_ptr(fileList+i);
			}else{
				if(fileList[i].addr==file.addr)
					return add_ptr(fileList+i);
			}
		}
	}


	if(spaceFound||cleanup_files()!=0xFFFFFFFF){
		fileList[creationLocation].addr=file.addr;
		fileList[creationLocation].count=0;
		fileList[creationLocation].flags=0;

		if(!(get_file_header(fileList+creationLocation).flags&0b00100000))
			cache_file(fileList+creationLocation);


		return add_ptr(fileList+creationLocation);
	}

	return 0;
}
static ACOS_FILE* get_pointer(ACOS_FILE file){
	dword creationLocation;
	byte spaceFound=0;
	for(dword i=0;i<MAX_ACOS_FILEPTRS;i++){
		if(!spaceFound&&!fileList[i].addr){
			spaceFound=1;
			creationLocation=i;
		}else{
			if(fileList[i].flags&1){
				if(((file_header*)(fileList[i].addr))->file_addr==file.addr){
					fileList[i].count++;
					return fileList+i;
				}
			}else{
				if(fileList[i].addr==file.addr){
					fileList[i].count++;
					return fileList+i;
				}
			}
		}
	}


	if(spaceFound||cleanup_files()!=0xFFFFFFFF){
		fileList[creationLocation].addr=file.addr;
		fileList[creationLocation].count=1;
		fileList[creationLocation].flags=0;

		if(!(get_file_header(fileList+creationLocation).flags&0b00100000))
			cache_file(fileList+creationLocation);


		return fileList+creationLocation;
	}

	return (ACOS_FILE*)0;
}
byte fclose_desc(pid proc,fdesc desc){
	ACOS_FILE** files=proc_getFiles(proc);
	if(!ptrValid(files[desc])) return 0;
	if(files[desc]->count==0) return 0;

	files[desc]->count--;
	files[desc]=(ACOS_FILE*)0;

	return 1;
}
byte fclose_ptr(ACOS_FILE* ptr){
	if(!ptrValid(ptr)) return 0;
	if(ptr->count==0) return 0;

	ptr->count--;

	return 1;
}



ACOS_FILE* get_root_pointer(void){
	return get_pointer(dwtofile(root));
}


file_header get_file_header(ACOS_FILE* file){
	file_header ret={};
	if(!ptrValid(file)) return ret;

	if(file->flags&1){
		//printf("\eCr;M\eCf;");
		ret=*(file_header*)(file->addr); // read from cache
		//printf("\eCgb;M\eCf;");
	}else{
		//printf("\eCg;D\eCf;");
		ahci_read(file->addr,sizeof(file_header),(byte*)(&ret)); // read from disk instead
		//printf("\eCrb;D\eCf;");
	}

	return ret;
}
dword read_file(ACOS_FILE* file,dword start,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	file_header header=get_file_header(file);
	dword filesize=header.size;
	size=(start+size<=filesize)?size:filesize-start;

	if(file->flags&1){
		for(dword i=0;i<size;i++) // read from cache
			buffer[i]=((byte*)(file->addr))[i+sizeof(file_header)+start];
		return size;
	}else{
		if(ahci_read( // read from disk instead
			file->addr+sizeof(file_header)+start,
			size,
			buffer
		)) return size;
		else return 0;
	}
}


dword get_directory_entries(ACOS_FILE* directory){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	file_header header=get_file_header(directory);
	if(header.flags>>6!=0x01) return 0;
	dword size=header.size;
	if(!size) return 0;
	byte* allocation=(byte*)allocate(size);
	read_file(directory,0,size,(void*)allocation);

	dword ret=0;
	for(dword i=0;i<size;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		ret++;
	}
	acos_free((dword)allocation,size);

	return ret;
}
ACOS_FILE* get_directory_entry_from_id(ACOS_FILE* directory,dword entryNumber){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	file_header header=get_file_header(directory);
	if(header.flags>>6!=0x01) return 0;
	dword size=header.size;
	if(!size) return (ACOS_FILE*)0;
	byte* allocation=(byte*)allocate(size);
	read_file(directory,0,size,allocation);

	dword i=0;
	for(;i<size&&entryNumber>0;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		entryNumber--;
	}

	if(i>=size) return (ACOS_FILE*)0;
	ACOS_FILE file=dwtofile(*(dword*)(allocation+i));
	acos_free((dword)allocation,size);
	return get_pointer(file);
}
static ACOS_FILE get_directory_entry_from_nameRaw(ACOS_FILE* directory,char* name){
	ACOS_FILE ret={0};
	file_header header=get_file_header(directory);
	if(header.flags>>6!=0x01) return ret;
	dword size=header.size;
	if(!size) return ret;
	byte* allocation=(byte*)allocate(size);
	if(!allocation) return ret;
	read_file(directory,0,size,allocation);

	dword i=0;
	for(;i<size;i++){
		i+=4;
		if(!strcmp(name,(char*)allocation+i)) break;

		while(allocation[i]&&i<size) i++;
	}

	if(i>=size) return ret;
	ret=dwtofile(*(dword*)(allocation+i-4));
	acos_free((dword)allocation,size);
	return ret;
}
ACOS_FILE* get_directory_entry_from_name(ACOS_FILE* directory,char* name){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	ACOS_FILE ret=get_directory_entry_from_nameRaw(directory,name);
	if(!ret.addr) return (ACOS_FILE*)0;
	return get_pointer(ret);
}

byte get_file_name(ACOS_FILE* file,byte* buffer){
	if(!ptrValid(file)) return 0;
	file_header OLDheader=get_file_header(file);
	ACOS_FILE parent_file=dwtofile(OLDheader.parent_addr);
	dword addr=OLDheader.file_addr;
	ACOS_FILE* parent=get_pointer(parent_file);
	file_header header=get_file_header(parent);

	dword size=header.size;
	if(size==0){
		fclose_ptr(parent);
		return 0;
	}
	byte* allocation=(byte*)allocate(size);
	if(!allocation){
		fclose_ptr(parent);
		return 0;
	}
	if(!read_file(parent,0,size,allocation)){
		fclose_ptr(parent);
		acos_free((dword)allocation,size);
		return 0;
	}

	dword i=0;
	for(;i<size;i++){
		if(*(dword*)(allocation+i)==addr){
			i+=4;
			break;
		}
		for(i+=4;allocation[i]&&i<size;i++);
	}

	dword j=i;
	dword k=i+0xFF;
	byte ret=0;
	for(;i<size&&i<k&&allocation[i];i++){
		buffer[i-j]=allocation[i];
		ret++;
	}
	buffer[i-j]=0;

	fclose_ptr(parent);
	acos_free((dword)allocation,size);
	return ret;
}

/*ACOS_FILE* resolve_path(char* str){
	if(*str!='/') return (ACOS_FILE*)0;
	dword size=0;
	for(;str[size]!=0;size++);
	byte buffer[size+1]={};

	ACOS_FILE preFile=dwtofile(root);
	byte preExists=ptrExists(preFile);
	ACOS_FILE* currentDir=get_pointer(preFile);

	dword p=0;
	str++;
	for(;*str!=0;str++){
		if(*str!='/') buffer[p++]=*str;
		if(*str=='/'||*(str+1)==0){
			ACOS_FILE file=get_directory_entry_from_nameRaw(currentDir,(char*)buffer);
			if(!file.addr) return (ACOS_FILE*)0;
			if(!preExists){
				close_file(currentDir);
			}

			preExists=ptrExists(file);
			ACOS_FILE* currentDir=get_pointer(file);

			file_header header=get_file_header(currentDir);
			byte flags=header.flags>>6;
			if(flags==0x00){
				if(*(str+1)!=0){
					if(!preExists) close_file(currentDir);
					return (ACOS_FILE*)0;
				}else return currentDir;
			}else if(flags==0x01){
				if(header.size==0){
					if(!preExists) close_file(currentDir);
					return (ACOS_FILE*)0;
				}
			}else return (ACOS_FILE*)0;

			while(p!=0) buffer[--p]=0;
		}
	}
	return (ACOS_FILE*)0;
}*/

ACOS_FILE* get_file_parent(ACOS_FILE* file){
	if(ptrValid(file))
		return get_pointer(dwtofile(get_file_header(file).parent_addr));
	return 0;
}
byte get_file_flags(ACOS_FILE* file){
	if(ptrValid(file))
		return get_file_header(file).flags;
	return 0;
}
byte get_file_type(ACOS_FILE* file){
	if(ptrValid(file))
		return (get_file_header(file).flags>>6)+1;
	return 0;
}
dword get_file_size(ACOS_FILE* file){
	if(!ptrValid(file)) return 0;
	return get_file_header(file).size;
}


// if this function returns a negative value, PLEASE issue a kernel panic
static char flush_cache(ACOS_FILE* file){
	if(!ptrValid(file)) return 0;
	if(!(file->flags&1)) return 0;

	file_header* cacheHeader=(file_header*)(file->addr);
	file_header diskHeader;
	if(!ahci_read(cacheHeader->file_addr,sizeof(file_header),(byte*)(&diskHeader))) return 0;

	// need some way to mark the filesystem as tainted here because so much can go wrong (negative return values)
	dword newAddr=newAddr=diskHeader.file_addr;
	byte same=1;
	if(cacheHeader->size<diskHeader.size){
		if(!fileFree(diskHeader.file_addr+sizeof(file_header)+cacheHeader->size,diskHeader.size-cacheHeader->size)) return -2;
		cacheHeader=(file_header*)(file->addr); // in case it's alloc.sys
	}else if(cacheHeader->size>diskHeader.size){
		same=0;
		if(!fileFree(diskHeader.file_addr,sizeof(file_header)+diskHeader.size)) return -2;
		cacheHeader=(file_header*)(file->addr); // in case it's alloc.sys
		newAddr=fileAllocate(sizeof(file_header)+cacheHeader->size);
		if(!newAddr) return -1;
		if(newAddr==diskHeader.file_addr) same=1; // remove this if it's causing bugs
	}

	cacheHeader->file_addr=newAddr;

	byte written=0;
	if(!same){ // update its directory entry
		ACOS_FILE parent_file=dwtofile(cacheHeader->parent_addr);
		ACOS_FILE* parent=get_pointer(parent_file);
		dword parent_size=get_file_header(parent).size;
		byte* allocation=(byte*)allocate(parent_size);
		if(!allocation) return -4;
		if(!read_file(parent,0,parent_size,allocation)) return -5;

		dword i=0;
		for(;i<parent_size;i++){
			if(*(dword*)(allocation+i)==diskHeader.file_addr) break;
			i+=4;
			while(allocation[i]&&i<parent_size) i++;
		}
		if(i>=parent_size) return -6;

		acos_free((dword)allocation,parent_size);
		if(!write_file(parent,i,4,(byte*)(&newAddr))) return -7;
		fclose_ptr(parent);


		byte* data=(byte*)cacheHeader+sizeof(file_header);
		if(diskHeader.file_addr==root){ // update all root-related pointers
			if(!ahci_write(0x1B0,4,(byte*)(&newAddr))) return -8; // update the pre-filesystem pointer
			cacheHeader->parent_addr=newAddr; // update the root's pointer
			*(dword*)data=newAddr; // update the root's file's pointer, assuming the root's own entry is the first one here
			root=newAddr; // update the kernel representation
		}

		if(!ahci_write(newAddr,sizeof(file_header)+cacheHeader->size,(byte*)cacheHeader)) return -3;
		written=1;

		if((diskHeader.flags&0xC0)==F_FLAGS_TYPE_DIR){ // tell all of the files in the directory to use the new pointer
			for(dword i=0;i<cacheHeader->size;i++){
				ACOS_FILE f=dwtofile(*(dword*)(data+i));
				ACOS_FILE* p=get_pointer(f);
				if(p!=file){ // just in case it's the root
					if(p->flags&1)
						((file_header*)p->addr)->parent_addr=newAddr;
					else
						if(!ahci_write(p->addr+offsetof(file_header,parent_addr),4,(byte*)(&newAddr))) return -9;
				}
				fclose_ptr(p);
				i+=4;
				while(data[i]&&i<cacheHeader->size) i++;
			}
		}
	}
	if(!written&&!ahci_write(newAddr,sizeof(file_header)+cacheHeader->size,(byte*)cacheHeader)) return -3;
	// file has now been written to disk

	return 1;
}
dword overwrite_file(ACOS_FILE* file,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	file_header header=get_file_header(file); // file header header equals get file header file yes
	if(file->flags&1)
		uncache_file(file);

	file_header* allocation=(file_header*)allocate(size+sizeof(file_header));
	if(!allocation) return 0;

	*allocation=header;
	allocation->size=size;

	byte* p=(byte*)allocation+sizeof(file_header);
	for(dword i=0;i<size;i++)
		p[i]=buffer[i];

	file->addr=(dword)allocation;
	file->flags|=0x01;

	return size;
}
dword write_file(ACOS_FILE* file,dword addr,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	file_header header=get_file_header(file);
	dword dataSize=(header.size>=addr+size)?header.size:addr+size;


	byte* allocation;
	if((file->flags&1)&&dataSize==header.size)
		allocation=(byte*)(file->addr); // cache holds enough memory for the write
	else{
		allocation=(byte*)allocate(dataSize+sizeof(file_header));
		if(!allocation) return 0;

		read_file(file,0,0xFFFFFFFF,allocation+sizeof(file_header));
		if(file->flags&1)
			acos_free((dword)(file->addr),header.size+sizeof(file_header));
		*(file_header*)allocation=header;
		file->addr=(dword)allocation; // new cache has been created
	}
	// the cache now holds enough space for the new file, so simply write to where you need to

	for(dword i=0;i<size;i++)
		allocation[i+addr+sizeof(file_header)]=buffer[i];

	((file_header*)allocation)->size=dataSize;
	file->addr=(dword)allocation;
	file->flags|=0x01;

	return size;
}
dword fileAllocate(dword size){
	if(!cache_file(allocsys)) return 0;

	dword allocsys_size=get_file_header(allocsys).size;
	byte ptrFound=0;
	dword smallestPtr;
	dword smallestSize=0xFFFFFFFF;
	dword* allocsys_cache=(dword*)(allocsys->addr+sizeof(file_header));

	for(dword i=0;i<allocsys_size/4;i+=2){
		if(allocsys_cache[i+1]>=size&&(allocsys_cache[i+1]<smallestSize||allocsys_cache[i+1]==size)){
			ptrFound=1;
			smallestPtr=i;
			smallestSize=allocsys_cache[i+1];
			if(allocsys_cache[i+1]==size) break;
		}
	}

	if(!ptrFound) return 0;
	dword ret=allocsys_cache[smallestPtr];
	// DO NOT USE trim_file() HERE
	allocsys_cache[smallestPtr]+=size;
	allocsys_cache[smallestPtr+1]-=size;
	return ret;
}
byte fileFree(dword addr,dword size){
	if(!cache_file(allocsys)) return 0;

	dword allocsys_size=get_file_header(allocsys).size;
	dword* allocsys_cache=(dword*)(allocsys->addr+sizeof(file_header));

	byte ptrType=0;
	dword i=0;
	for(;i<allocsys_size/sizeof(dword);i+=2){
		if(allocsys_cache[i]+allocsys_cache[i+1]==addr){ // freeptr is before
			ptrType=1;
			break;
		}else if(addr+size==allocsys_cache[i]){ // freeptr is after
			ptrType=2;
			break;
		}
	}

	if(ptrType==0){
		dword buffer[2]={addr,size};
		if(!write_file(allocsys,allocsys_size,8,(byte*)buffer)) return 0;
	}else if(ptrType==1)
		allocsys_cache[i+1]+=size;
	else{
		allocsys_cache[i]-=size;
		allocsys_cache[i+1]+=size;
	}

	return 1;
}
char create_file(ACOS_FILE* parent,byte flags,char* name){
	if(!ptrValid(parent)) return 0;
	if(!file_permission(F_PERM_WRITE,parent)) return 0;

	if(!cache_file(parent)) return 0;
	dword allocation=fileAllocate(sizeof(file_header));
	if(!allocation) return -1;
	file_header* parent_cache=(file_header*)(parent->addr);
	file_header header={
		0,
		parent_cache->file_addr,
		allocation, // fileptr
		flags&0b01000111
	};
	if(!ahci_write(allocation,sizeof(file_header),(byte*)(&header))) return -2;

	dword nameSize=0;
	for(;name[nameSize];nameSize++);
	nameSize++;
	if(!write_file(parent,parent_cache->size,sizeof(dword),(byte*)(&allocation))) return -3;
	parent_cache=(file_header*)(parent->addr);
	if(!write_file(parent,parent_cache->size,nameSize,name)) return -4;

	return 1;
}
char delete_file(ACOS_FILE* file){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	if(file==allocsys) return 0; // we don't want this file to disappear

	file_header header=get_file_header(file);
	if(header.file_addr==root) return 0; // we also don't want this file to disappear
	dword count=1;
	if(header.flags&F_FLAGS_TYPE_DIR){
		for(dword i=0;i<get_directory_entries(file);i++){
			char r=delete_file(get_directory_entry_from_id(file,i));
			if(r<0) return -10+r;
			if(r>0) count++;
		}
	}
	if(!uncache_file(file)) return 0;
	header=get_file_header(file);
	if(!fileFree(header.file_addr,sizeof(file_header)+header.size)) return -1;

	ACOS_FILE parent_file=dwtofile(header.parent_addr);
	ACOS_FILE* parent=get_pointer(parent_file);
	byte cached=parent->flags&1;
	if(!cache_file(parent)) return -2;
	byte* allocation=(byte*)parent->addr+sizeof(file_header);
	dword parent_size=((file_header*)parent->addr)->size;

	dword found_addr;
	dword sizeToTrim=5;
	byte found=0;
	for(dword i=0;i<parent_size;i++){
		if(*(dword*)(allocation+i)==header.file_addr){
			found=1;
			found_addr=i;
			i+=4;
			for(;allocation[i]&&i<parent_size;i++)
				sizeToTrim++;
			break;
		}
		i+=4;
		while(allocation[i]&&i<parent_size) i++;
	}
	if(!found) return -6;

	if(!trim_file(parent,found_addr,sizeToTrim)) return -7;

	fclose_ptr(parent);
	ACOS_FILE filenull={};
	*file=filenull;
	return count;
}
char rename_file(ACOS_FILE* file,char* name){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	//if(file==allocsys) return 0;
	// renaming alloc.sys can be catastrophic IF the user doesn't know what they're doing
	// However, I chose to trust the user here, since renaming it might be an attempt at recovery

	file_header header=get_file_header(file);
	if(header.file_addr==root) return 0; // renaming the root breaks the assumption of its entry being the first one. do not remove this.

	ACOS_FILE parent_file=dwtofile(header.parent_addr);
	ACOS_FILE* parent=get_pointer(parent_file);
	byte cached=parent->flags&1;
	if(!cache_file(parent)) return -2;
	file_header* parent_cache=(file_header*)(parent->addr);
	byte* allocation=(byte*)parent->addr+sizeof(file_header);
	dword parent_size=((file_header*)parent->addr)->size;

	dword found_addr;
	dword sizeToTrim=5;
	byte found=0;
	for(dword i=0;i<parent_size;i++){
		if(*(dword*)(allocation+i)==header.file_addr){
			found=1;
			found_addr=i;
			i+=4;
			for(;allocation[i]&&i<parent_size;i++) sizeToTrim++;
			break;
		}
		i+=4;
		while(allocation[i]&&i<parent_size) i++;
	}
	if(!found) return -6;
	if(!trim_file(parent,found_addr,sizeToTrim)) return -7;

	dword nameSize=0;
	for(;name[nameSize];nameSize++);
	nameSize++;
	if(!write_file(parent,parent_cache->size,sizeof(dword),(byte*)(&header.file_addr))) return -3;
	parent_cache=(file_header*)(parent->addr);
	if(!write_file(parent,parent_cache->size,nameSize,name)) return -4;

	fclose_ptr(parent);
	return 1;
}
/*
 * move_file() should:
 * delete the file's current directory entry, like rename_file(), and maybe even save it for the next step
 * add the directory entry to the target directory, similar to rename_file()
 * update the file's parent pointer in-place
 *
 * this is nothing you haven't done before
 */

dword trim_file(ACOS_FILE* file,dword start,dword size){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;

	dword fileSize=get_file_header(file).size;
	if(size==0||start>=fileSize) return fileSize;
	size=(start+size>fileSize)?fileSize-start:size;
	byte* allocation=(byte*)allocate(fileSize);
	if(!allocation) return 0;
	if(!read_file(file,0,fileSize,allocation)){
		acos_free((dword)allocation,fileSize);
		return 0;
	}

	for(dword i=start;i+size<=fileSize;i++)
		allocation[i]=allocation[i+size];
	//  0  1  2  3  4  5  6  7  8
	// 11 22 33 44 00 00 00 00

	if(!overwrite_file(file,fileSize-size,allocation)){
		acos_free((dword)allocation,fileSize);
		return 0;
	}
	acos_free((dword)allocation,fileSize);
	return (fileSize-size>0)?fileSize-size:0xFFFFFFFF;
}

dword sync_files(void){
	dword count=0;
	byte name[64];
	for(dword i=0;i<MAX_ACOS_FILEPTRS;i++){
		if(ptrValid(fileList+i)&&fileList[i].flags&1&&fileList+i!=allocsys){
			count++;
			flush_cache(fileList+i);
			uncache_file(fileList+i);
		}
	}
	if(allocsys->flags&1){
		count++;
		flush_cache(allocsys);
		uncache_file(allocsys);
	}
	return count;
}


// 1: Signature mismatch
// 2: alloc.sys is missing
// 3: Disk error
byte file_init(void){
	byte buffer[12];
	if(!ahci_read(0x1B0,12,buffer))
		return 3;

	root=*(dword*)buffer;
	char* sig=FILESYSTEM_SIGNATURE;
	for(byte i=0;i<8;i++)
		if(sig[i]!=buffer[i+4]) return 1;

	ACOS_FILE* root_ptr=get_root_pointer();
	allocsys=get_directory_entry_from_name(root_ptr,"alloc.sys");
	fclose_ptr(root_ptr);
	if(!allocsys) return 2;

	return 0;
}


void printc(char c){
	putchar(c);
}
void set_terminal(void){
	struct termios old_tio, new_tio;
    int ch;

    // 1. Get the current terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;

    // 2. Disable canonical mode (ICANON) and local echo (ECHO)
    new_tio.c_lflag &= ~(ICANON | ECHO);

    // 3. Set the new settings immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    printf("Press any key to continue immediately... ");
    fflush(stdout); // Ensure the prompt prints out right away

    // 4. Read the single character instantly
    ch = getchar();

    // 5. Restore the original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}
int main(int argc,char* argv[]){
	memory=malloc(1000000000);
	alloc_map_startoff[0]+=(dword)memory;
	alloc_map_startoff[1]+=(dword)memory;
	if(!memory){
		printf("Unable to allocate enough memory!");
		return 5;
	}
	if(argc<=1){
		printf("Please.\n");
		return 1;
	}else{
		disk=fopen(argv[1],"r+b");
		if(!disk){
			printf("Oh no!\n");
			return 2;
		}
	}
	fileList=calloc(MAX_ACOS_FILEPTRS,sizeof(ACOS_FILE));
	byte success=file_init();
	if(success){
		printf("This isn't good!\n");
		return success;
	}
	printf("Hooray!\n");



	ACOS_FILE* rooty=get_root_pointer();

	byte fileName[256];
	byte input[128]={};
	byte p=0;
	ACOS_FILE* currentDir=rooty;
	printf("\n> ");
	for(;;){
		byte c=getchar();
		if(c=='\b'){
			if(p>0){
				p--;
				input[p]=0;
			}
		}else if(c=='\n'){
			for(p=0;p<128;p++) input[p]=(input[p]==' ')?0:input[p];
			p=0;
			while(p<128&&input[p]!=0) p++;
			while(p<128&&input[p]==0) p++;
			dword p2=p;
			while(p2<128&&input[p2]!=0) p2++;
			while(p2<128&&input[p2]==0) p2++;

			if(!strcmp(input,"ls")){
				printf("There are ");
				byte entries=get_directory_entries(currentDir);
				printf("%02X",entries);
				printf(" files in '");
				get_file_name(currentDir,fileName);
				printf(fileName);
				printf("':\n");

				for(byte i=0;i<entries;i++){
					ACOS_FILE* file=get_directory_entry_from_id(currentDir,i);
					if(file->flags&1)
						printc('*');

					printf("%04X",get_file_size(file));
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
					fclose_ptr(file);
				}
			}else if(!strcmp(input,"print")){
				printf(input+p);
			}else if(!strcmp(input,"cd")){
				printf("You are now in ");
				if(!strcmp(input+p,".."))
					currentDir=get_file_parent(currentDir);
				else{
					ACOS_FILE* newDir=get_directory_entry_from_name(currentDir,input+p);
					if(newDir&&get_file_type(newDir)==F_TYPE_DIR)
						currentDir=newDir;
				}
				get_file_name(currentDir,fileName);
				printf(fileName);
				printc('.');
			}else if(!strcmp(input,"create")){
				char res=create_file(currentDir,0x07,input+p);
				if(res>0){
					printf("The file '");
					printf(input+p);
					printf("' was successfully created.");
				}else{
					printf("Unable to create '");
					printf(input+p);
					printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
					printf("%02X",res);
					for(;;);
				}
			}else if(!strcmp(input,"mkdir")){
				char res=create_file(currentDir,0x47,input+p);
				if(res>0){
					printf("The directory '");
					printf(input+p);
					printf("' was successfully created.");
				}else{
					printf("Unable to create '");
					printf(input+p);
					printf("'! Stopping all operations.\nPlease turn off the computer.\n\nError code: ");
					printf("%02X",res);
					for(;;);
				}
			}else if(!strcmp(input,"rename")){
				ACOS_FILE* file=get_directory_entry_from_name(currentDir,input+p);
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
						printf("%02X",res);
						for(;;);
					}
				}
			}else if(!strcmp(input,"delete")){
				ACOS_FILE* file=get_directory_entry_from_name(currentDir,input+p);
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
						printf("%02X",res);
						for(;;);
					}
				}
			}else if(!strcmp(input,"write")){
				ACOS_FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					dword size=0;
					for(;(input+p2)[size]!=0;size++);
					dword size2=overwrite_file(file,size,input+p2);
					printf("Wrote '");
					printf(input+p2);
					printf("' (");
					printf("%08X",size);
					printf(" bytes) to the file.");
				}
			}else if(!strcmp(input,"sync")){
				printf("Syncing files...");
				dword filesSynced=sync_files();
				if(filesSynced)
					while(sync_files()) printc('.');
				printf(" Synced ");
				printf("%02X",filesSynced);
				printf(" files!");
			}else if(!strcmp(input,"exit")){
				printf("Syncing files...");
				dword filesSynced=sync_files();
				if(filesSynced)
					while(sync_files()) printc('.');
				printf(" Synced ");
				printf("%02X",filesSynced);
				printf(" files!\n");
				break;
			}else if(!strcmp(input,"cat")){
				ACOS_FILE* file=get_directory_entry_from_name(currentDir,input+p);
				if(!file)
					printf("Bad file");
				else{
					dword size=get_file_size(file);
					dword allocation=allocate(size);
					read_file(file,0,size,(byte*)allocation);
					for(dword i=0;i<size;i++){
						printf("%02X",((byte*)allocation)[i]);
						printc(' ');
					}
					acos_free(allocation,size);
				}
			}else if(!strcmp(input,"put")){
				FILE* file=fopen(input+p,"rb");
				struct stat st;
				if(stat(input+p,&st)==-1||!file){
				    printf("Unreadable file.");
					if(file) fclose(file);
				}else{
					ACOS_FILE* acos_file=get_directory_entry_from_name(currentDir,input+p);
					if(!acos_file){
						if(create_file(currentDir,0x07,input+p)<=0){
							printf("Fatal error! Oh no!\n");
							for(;;);
						}
						acos_file=get_directory_entry_from_name(currentDir,input+p);
					}
					char* buffer=malloc(st.st_size);
					fread(buffer,sizeof(char),st.st_size,file);
					overwrite_file(acos_file,st.st_size,buffer);
					fclose_ptr(acos_file);
					fclose(file);
					free(buffer);
				}
			}else printf("\eCqrg;Unknown command!\eCF;");

			printf("\n> ");
			for(p=0;p<128;p++) input[p]=0;
			p=0;
		}else if(p<128){
			input[p]=c;
			p++;
		}
	}


	fclose(disk);
    return 0;
}

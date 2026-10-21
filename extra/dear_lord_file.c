#include "file.h"
#include "acos.h"


static dword root;
static FILE* allocsys;
extern dword current_pid;






#define MAX_FILEPTRS (1<<16)
typedef dword fileListPtr;
static FILE fileList[MAX_FILEPTRS];

FILE dwtofile(dword dw){
	FILE file={};
	file.addr=dw;
	file.owner=current_pid;
	file.flags=0;
	return file;
}

file_header getFileHeader(FILE* file);
static byte ptrValid(FILE* file){
	return (
		file>=fileList&&
		(dword)file<(dword)fileList+MAX_FILEPTRS*sizeof(FILE)&&
		((dword)file-(dword)fileList)%sizeof(FILE)==0&&
		file->addr
	)?1:0;
}
static byte ptrExists(FILE file){
	for(fileListPtr i=0;i<MAX_FILEPTRS;i++)
		if(fileList[i].flags&1){
			if(((file_header*)(fileList[i].addr))->file_addr==file.addr)
				return 1;
		}else{
			if(fileList[i].addr==file.addr)
				return 1;
		}
	return 0;
}

static byte file_permission(byte req,FILE* file){
	if(current_pid==0) return 1;
	byte isRoot=proc_getFlags(current_pid)&1;
	if(isRoot) return 1;

	byte flags=getFileHeader(file).flags;
	if(req==F_PERM_READ)
		return (flags&0x04)>>2;
	if(req==F_PERM_WRITE)
		return (flags&0x02)>>1;
	if(req==F_PERM_EXECUTE)
		return flags&0x01;
	return 0;
}

static byte cacheFile(FILE* file){
	if(!ptrValid(file)) return 0;
	if(file->flags&1) return 1;

	file_header header=getFileHeader(file);
	dword size=sizeof(file_header)+header.size;
	byte* buffer=(byte*)allocate(size);
	printf("\eCg;cached \eCf;");
	printd((dword)buffer);
	printc('*');
	printd(getFileHeader(file).file_addr);

	if(ahci_read(file->addr,size,buffer)){
		file->addr=(dword)buffer;
		file->flags|=1;
		return 1;
	}else return 0;
}
static byte uncacheFile(FILE* file){
	if(!ptrValid(file)) return 0;
	if(!(file->flags&1)) return 1;
	file_header header=*(file_header*)(file->addr);

	printf("\eCg;uncached \eCf;");
	printd(file->addr);
	printc('*');
	printd(getFileHeader(file).file_addr);
	free(file->addr,sizeof(file_header)+header.size);
	file->addr=header.file_addr;
	file->flags&=0xFE;
	return 1;
}
static FILE* getPointer(FILE file){
	fileListPtr creationLocation;
	byte spaceFound=0;
	for(fileListPtr i=0;i<MAX_FILEPTRS;i++){
		if(!spaceFound&&!fileList[i].addr){
			spaceFound=1;
			creationLocation=i;
		}else{
			if(fileList[i].flags&1){
				if(((file_header*)(fileList[i].addr))->file_addr==file.addr)
					return fileList+i;
			}else{
				if(fileList[i].addr==file.addr)
					return fileList+i;
			}
		}
	}


	if(spaceFound){
		//printf("\eCrgb;{created a pointer for ");
		//printd(file.addr);
		//printf(" at ");
		//printd((dword)creationLocation);
		fileList[creationLocation].addr=file.addr;
		fileList[creationLocation].owner=current_pid;
		fileList[creationLocation].flags=0;
		printf("\eCqgb;CREATED\eCf;");
		printd((dword)(fileList+creationLocation));
		printc('(');
		printd(fileList[creationLocation].addr);
		printc(')');

		if(!(getFileHeader(fileList+creationLocation).flags&0b00100000))
			cacheFile(fileList+creationLocation);


		//printf("}\eCF;");
		return fileList+creationLocation;
	}

	return (FILE*)0;
}
byte closeFile(FILE* ptr){
	//printf("\eCrgb;{deleting the pointer for ");
	if(!ptrValid(ptr)) return EINVALID;
	if((ptr->owner==0&&current_pid!=0)&&ptr->owner!=current_pid) return EFORBIDDEN;

	if(ptr->flags&1)
		uncacheFile(ptr);
	//printd(ptr->addr);
	ptr->addr=0;
	printf("\eCqgb;CLOSED\eCf;");
	printd((dword)ptr);

	//printf("}\eCF;");
	return 0;
}


FILE* getRootPointer(void){
	return getPointer(dwtofile(root));
}


file_header getFileHeader(FILE* file){
	file_header ret={0,0x12345678};
	if(!ptrValid(file)) return ret;

	if(file->flags&1)
		ret=*(file_header*)(file->addr); // read from cache
	else
		ahci_read(file->addr,sizeof(file_header),(byte*)(&ret)); // read from disk instead

	return ret;
}
dword readFile_prog(FILE* file,dword size,byte SEG_GS* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_READ,file)) return 0;
	file_header header=getFileHeader(file);
	if(header.flags>>6!=0x00) return 0; // not a RWX file
	dword filesize=header.size;
	size=(size<=filesize)?size:filesize;

	if(file->flags&1){
		for(dword i=0;i<size;i++) // read from cache
			buffer[i]=((byte*)(file->addr))[i+sizeof(file_header)];
		return size;
	}else{
		if(ahci_read_prog( // read from disk instead
			file->addr+sizeof(file_header),
			size,
			buffer
		)) return size;
		else return 0;
	}
}
dword readFile(FILE* file,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	file_header header=getFileHeader(file);
	dword filesize=header.size;
	size=(size<=filesize)?size:filesize;

	if(file->flags&1){
		for(dword i=0;i<size;i++) // read from cache
			buffer[i]=((byte*)(file->addr))[i+sizeof(file_header)];
		return size;
	}else{
		if(ahci_read( // read from disk instead
			file->addr+sizeof(file_header),
			size,
			buffer
		)) return size;
		else return 0;
	}
}


dword getDirectoryEntries(FILE* directory){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	file_header header=getFileHeader(directory);
	if(header.flags>>6!=0x01) return 0;
	dword size=header.size;
	if(!size) return 0;
	byte* allocation=(byte*)allocate(size);
	readFile(directory,size,(void*)allocation);

	dword ret=0;
	for(dword i=0;i<size;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		ret++;
	}
	free((dword)allocation,size);

	return ret;
}
FILE* getDirectoryEntryFromId(FILE* directory,dword entryNumber){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	file_header header=getFileHeader(directory);
	if(header.flags>>6!=0x01) return 0;
	dword size=header.size;
	if(!size) return (FILE*)0;
	byte* allocation=(byte*)allocate(size);
	readFile(directory,size,allocation);

	dword i=0;
	for(;i<size&&entryNumber>0;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		entryNumber--;
	}

	if(i>=size) return (FILE*)0;
	FILE file=dwtofile(*(dword*)(allocation+i));
	free((dword)allocation,size);
	return getPointer(file);
}
static FILE getDirectoryEntryFromNameRaw(FILE* directory,char* name){
	printf("\eCqgb;GETENTRY\eCf;");
	FILE ret={0};
	file_header header=getFileHeader(directory);
	if(header.flags>>6!=0x01) return ret;
	dword size=header.size;
	if(!size) return ret;
	byte* allocation=(byte*)allocate(size);
	if(!allocation) return ret;
	readFile(directory,size,allocation);

	dword i=0;
	for(;i<size;i++){
		i+=4;
		if(cmpstr(name,(char*)allocation+i)) break;

		while(allocation[i]&&i<size) i++;
	}

	if(i>=size) return ret;
	ret=dwtofile(*(dword*)(allocation+i-4));
	free((dword)allocation,size);
	printf("\eCqr;GETENTRY END\eCf;");
	return ret;
}
FILE* getDirectoryEntryFromName(FILE* directory,char* name){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	FILE ret=getDirectoryEntryFromNameRaw(directory,name);
	if(!ret.addr) return (FILE*)0;
	return getPointer(ret);
}
FILE* getDirectoryEntryFromName_prog(FILE* directory,char SEG_GS* name){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	dword size=0;
	for(;name[size]!=0;size++); // triggers a general protection fault when necessary
	char buffer[size+1]={};
	for(dword i=0;name[i]!=0;i++) buffer[i]=name[i];
	FILE ret=getDirectoryEntryFromNameRaw(directory,buffer);
	if(!ret.addr) return (FILE*)0;
	return getPointer(ret);
}

byte getFileName_prog(FILE* file,byte SEG_GS* buffer){
	if(!file_permission(F_PERM_READ,file)) return 0;
	file_header header=getFileHeader(file);
	byte exists=ptrExists(dwtofile(header.parent_addr));
	dword addr=header.file_addr;
	FILE* parent=getPointer(dwtofile(header.parent_addr));
	header=getFileHeader(parent);

	dword size=header.size;
	byte* allocation=(byte*)allocate(size);
	if(!allocation){
		if(!exists) closeFile(parent);
		return 0;
	}
	if(!readFile(parent,size,allocation)){
		if(!exists) closeFile(parent);
		free((dword)allocation,size);
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
	buffer[i]=0;

	if(!exists) closeFile(parent);
	free((dword)allocation,size);
	return ret;
}
byte getFileName(FILE* file,byte* buffer){
	if(!ptrValid(file)){
		printf("{what are you thinking?}");
		return 0;
		/*
		 	* this doesn't execute, so there's always a valid file pointer being passed here
			* {sit1} is being executed because size=0 for some reason
			* find out why.
		 */
	}
	file_header OLDheader=getFileHeader(file);
	/*printf("\eCrgb;{size=");
	printd(header.size);
	printf(",parent=");
	printd(header.parent_addr);
	printf(",file=");
	printd(header.file_addr);
	printf(",ptr=");
	printd((dword)file);*/
	FILE parent_file=dwtofile(OLDheader.parent_addr);
	byte exists=ptrExists(parent_file);
	dword addr=OLDheader.file_addr;
	FILE* parent=getPointer(parent_file);
	file_header header=getFileHeader(parent);

	dword size=header.size;
	if(size==0){
		clear();
		printf("\eCqr;Freak out time! File is ");
		printd(file->addr);
		printc(' ');
		printd(file->owner);
		printc(' ');
		printd(file->flags);
		printf(" (");
		printd((dword)file);

		printf(")\nContents of memory at 0x");
		printd(file->addr);
		printf(":");
		for(byte* i=(byte*)file->addr;(dword)i<file->addr+0x30;i++){
			if((dword)i%16==0){
				printc('\n');
				printd((dword)i);
				printf(": ");
			}else
				printc(' ');
			printh(*i);
		}


		printf("\nParent is ");
		printd(parent->addr);
		printc(' ');
		printd(parent->owner);
		printc(' ');
		printd(parent->flags);
		printf(" (");
		printd((dword)parent);

		printf(")\nContents of memory at 0x");
		printd(parent->addr);
		printf(":");
		for(byte* i=(byte*)parent->addr;(dword)i<parent->addr+0x30;i++){
			if((dword)i%16==0){
				printc('\n');
				printd((dword)i);
				printf(": ");
			}else
				printc(' ');
			printh(*i);
		}
		for(;;);
	}
	printf("}\eCF;");
	byte* allocation=(byte*)allocate(size);
	if(!allocation){
		if(!exists) closeFile(parent);
		printf("{sit1}");
		printd(size);
		return 0;
	}
	if(!readFile(parent,size,allocation)){
		if(!exists) closeFile(parent);
		free((dword)allocation,size);
		printf("{sit2}");
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

	if(!exists){
		//printf("\eCr;{ts pointer didn't exist}\eCf;");
		closeFile(parent);
	}
	free((dword)allocation,size);
	return ret;
}

FILE* resolvePath(char* str){
	if(*str!='/') return (FILE*)0;
	dword size=0;
	for(;str[size]!=0;size++);
	byte buffer[size+1]={};

	FILE preFile=dwtofile(root);
	byte preExists=ptrExists(preFile);
	FILE* currentDir=getPointer(preFile);

	dword p=0;
	str++;
	for(;*str!=0;str++){
		if(*str!='/') buffer[p++]=*str;
		if(*str=='/'||*(str+1)==0){
			FILE file=getDirectoryEntryFromNameRaw(currentDir,(char*)buffer);
			if(!file.addr) return (FILE*)0;
			if(!preExists){
				closeFile(currentDir);
			}

			preExists=ptrExists(file);
			FILE* currentDir=getPointer(file);

			file_header header=getFileHeader(currentDir);
			byte flags=header.flags>>6;
			if(flags==0x00){
				if(*(str+1)!=0){
					if(!preExists) closeFile(currentDir);
					return (FILE*)0;
				}else return currentDir;
			}else if(flags==0x01){
				if(header.size==0){
					if(!preExists) closeFile(currentDir);
					return (FILE*)0;
				}
			}else return (FILE*)0;

			while(p!=0) buffer[--p]=0;
		}
	}
	return (FILE*)0;
}
FILE* resolvePath_prog(char SEG_GS* str){
	if(*str!='/') return (FILE*)0;
	dword size=0;
	for(;str[size]!=0;size++);
	byte buffer[size+1]={};

	FILE preFile=dwtofile(root);
	byte preExists=ptrExists(preFile);
	FILE* currentDir=getPointer(preFile);

	dword p=0;
	str++;
	for(;*str!=0;str++){
		if(*str!='/') buffer[p++]=*str;
		if(*str=='/'||*(str+1)==0){
			FILE file=getDirectoryEntryFromNameRaw(currentDir,(char*)buffer);
			if(!file.addr) return (FILE*)0;
			if(!preExists) closeFile(currentDir);

			preExists=ptrExists(file);
			FILE* currentDir=getPointer(file);

			file_header header=getFileHeader(currentDir);
			byte flags=header.flags>>6;
			if(flags==0x00){
				if(*(str+1)!=0){
					if(!preExists) closeFile(currentDir);
					return (FILE*)0;
				}else return currentDir;
			}else if(flags==0x01){
				if(header.size==0){
					if(!preExists) closeFile(currentDir);
					return (FILE*)0;
				}
			}else return (FILE*)0;

			while(p!=0) buffer[--p]=0;
		}
	}
	return (FILE*)0;
}

FILE* getFileParent(FILE* file){
	if(ptrValid(file))
		return getPointer(dwtofile(getFileHeader(file).parent_addr));
	return 0;
}
byte getFileType(FILE* file){
	if(ptrValid(file))
		return (getFileHeader(file).flags>>6)+1;
	return 0;
}
dword getFileSize(FILE* file){
	if(!ptrValid(file)) return 0;
	return getFileHeader(file).size;
}


// if this function returns a negative value, PLEASE issue a kernel panic
static void augh(char* str){
	//printf(str);
	//printf("}\eCF;");
	for(;;);
}
static char flushCache(FILE* file){
	//printf("call to cache");
	if(!ptrValid(file)) return 0;
	if(!(file->flags&1)) return 0;

	file_header* cacheHeader=(file_header*)(file->addr);
	file_header diskHeader;
	if(!ahci_read(cacheHeader->file_addr,sizeof(file_header),(byte*)(&diskHeader))) return 0;

	// need some way to mark the filesystem as tainted here because so much can go wrong (negative return values)
	dword newAddr=0;
		/*printf("\eCFB;{just allocated ");
		printd(sizeof(file_header)+cacheHeader->size);
		printf(" bytes. Here's the dump: ");
		dword buffer[64];
		dword re=readFile(allocsys,64*sizeof(dword),(byte*)buffer);
		if(!re){
			printf("... the hell? where is the dump?");
			for(;;);
		}
		for(dword i=0;i<re/sizeof(dword);i++){
			printd(buffer[i]);
			printc(' ');
		}
		printc('}');
		for(;;);*/
	if(cacheHeader->size!=diskHeader.size){ // DONT OPTIMIZE THIS
		if(!fileFree(diskHeader.file_addr,sizeof(file_header)+diskHeader.size)) return -2;
		cacheHeader=(file_header*)(file->addr); // in case it's alloc.sys
		newAddr=fileAllocate(sizeof(file_header)+cacheHeader->size);
		if(!newAddr) return -1;
	}else newAddr=diskHeader.file_addr;
	byte same=(newAddr==diskHeader.file_addr);

	cacheHeader->file_addr=newAddr;
	printf("\eCr;[writing ");
	for(byte i=0;i<sizeof(file_header)+cacheHeader->size;i++){
		printh(((byte*)cacheHeader)[i]);
		printc(' ');
	}
	printc('(');
	printd(sizeof(file_header)+cacheHeader->size);
	printf(" bytes) from ");
	printd((dword)((byte*)cacheHeader));
	printf(" to ");
	printd(newAddr);
	if(newAddr==0x2000) getchar();
	printf("]\eCF;");

	if(!ahci_write(newAddr,sizeof(file_header)+cacheHeader->size,(byte*)cacheHeader)) return -3;
	// file has now been written to disk

	if(!same){ // update its directory entry
		FILE parent_file=dwtofile(diskHeader.parent_addr);
		byte ptrExisted=ptrExists(parent_file);
		FILE* parent=getPointer(parent_file);
		dword parent_size=getFileHeader(parent).size;
		byte* allocation=(byte*)allocate(parent_size);
		if(!allocation) return -4;

		/*
			* the FINAL problem is in this if block
			* sync literally works perfectly when the filesize is the same
			* none of the negative values are being returned, so no fatal errors
			* fix it NOWWWW
		 */

		if(!readFile(parent,parent_size,allocation)) return -5;

		dword i=0;
		for(;i<parent_size;i++){
			if(*(dword*)(allocation+i)==diskHeader.file_addr) break;
			i+=4;
			while(allocation[i]&&i<parent_size) i++;
		}
		if(i>=parent_size) return -6;

		free((dword)allocation,parent_size);
		if(!writeFile(parent,i,4,(byte*)(&newAddr))) return -7;
		if(!ptrExisted) closeFile(parent);


		if(diskHeader.file_addr==root){
			if(!ahci_write(0x1B0,4,(byte*)(&newAddr))) return -8;
			root=newAddr;
		}
	}else return 2;

	return 1;
}
dword overwriteFile(FILE* file,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	printf("\eCqgb;OVERWRITE\eCf;");
	file_header header=getFileHeader(file); // file header header equals get file header file yes
	byte preCached=0;
	if(file->flags&1){
		preCached=1;
		uncacheFile(file);
	}

	file_header* allocation=(file_header*)allocate(size+sizeof(file_header));
	printf("\eCg;recached \eCf;");
	printd((dword)allocation);
	printc('*');
	printd(getFileHeader(file).file_addr);
	if(!allocation) return 0;

	*allocation=header;
	allocation->size=size;

	byte* p=(byte*)allocation+sizeof(file_header);
	for(dword i=0;i<size;i++)
		p[i]=buffer[i];

	file->addr=(dword)allocation;
	file->flags|=0x01;

	if(!preCached){
		flushCache(file);
		uncacheFile(file);
	}
	//printf("\eCrgb;{newCachePtr=");
	/*//printd(allocation->size);
	//printf(",parent=");
	//printd(allocation->parent_addr);
	//printf(",file=");
	//printd(allocation->file_addr);
	//printf(",flags=");
	//printh(allocation->flags);*/
	//printd((dword)allocation);
	//printf("}\eCF;");

	printf("\eCqr;OVERWRITE END\eCf;");
	return size;
}
dword writeFile(FILE* file,dword addr,dword size,byte* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	file_header header=getFileHeader(file);
	dword dataSize=(header.size>=addr+size)?header.size:addr+size;


	byte preCached=file->flags&1;
	byte* allocation;
	if((file->flags&1)&&dataSize==header.size)
		allocation=(byte*)(file->addr); // cache holds enough memory for the write
	else{
		allocation=(byte*)allocate(dataSize+sizeof(file_header));
		if(!allocation) return 0;
		printf("\eCg;REcached \eCf;");
		printd((dword)allocation);
		printc('*');
		printd(getFileHeader(file).file_addr);

		readFile(file,0xFFFFFFFF,allocation+sizeof(file_header));
		if(file->flags&1)
			free((dword)(file->addr),header.size+sizeof(file_header));
		*(file_header*)allocation=header;
		file->addr=(dword)allocation; // new cache has been created
	}
	// the cache now holds enough space for the new file, so simply write to where you need to

	for(dword i=0;i<size;i++)
		allocation[i+addr+sizeof(file_header)]=buffer[i];

	((file_header*)allocation)->size=dataSize;
	file->addr=(dword)allocation;
	file->flags|=0x01;

	if(!preCached){
		flushCache(file);
		uncacheFile(file);
	}
	return size;
}
dword fileAllocate(dword size){
	if(!cacheFile(allocsys)) return 0;

	dword allocsys_size=getFileHeader(allocsys).size;
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
	/*if(smallestSize==size)
		trimFile(allocsys,smallestPtr*4,8);
	else{*/
		allocsys_cache[smallestPtr]+=size;
		allocsys_cache[smallestPtr+1]-=size;
	//}
	//printf("[fileallocate success]");
	return ret;
}
byte fileFree(dword addr,dword size){
	if(!cacheFile(allocsys)) return 0;

	dword allocsys_size=getFileHeader(allocsys).size;
	dword* allocsys_cache=(dword*)(allocsys->addr+sizeof(file_header));

	byte ptrType=0;
	dword ptrSize=size;
	dword i=0;
	for(;i<allocsys_size/sizeof(dword);i+=2){
		if(allocsys_cache[i]+allocsys_cache[i+1]==addr){ // freeptr is before
			ptrType=1;
			break;
		}else if(addr+ptrSize==allocsys_cache[i]){ // freeptr is after
			ptrType=2;
			break;
		}
	}

	if(ptrType==0){
		dword buffer[2]={addr,ptrSize};
		if(!writeFile(allocsys,allocsys_size,8,(byte*)buffer)) return 0;
	}else if(ptrType==1)
		allocsys_cache[i+1]+=ptrSize;
	else{
		allocsys_cache[i]-=ptrSize;
		allocsys_cache[i+1]+=ptrSize;
	}

	return 1;
}
char createFile(FILE* parent,byte permissions,char* name,dword* al){
	if(!ptrValid(parent)) return 0;
	if(!file_permission(F_PERM_WRITE,parent)) return 0;

	dword allocation=fileAllocate(sizeof(file_header));
	if(!allocation) return -1;
	file_header parent_header=getFileHeader(parent);
	file_header header={
		0,
		parent_header.file_addr,
		allocation, // fileptr
		permissions&3
	};
	if(ahci_write(allocation,sizeof(file_header),(byte*)(&header))) return -2;

	dword nameSize=0;
	for(;name[nameSize];nameSize++);
	if(!writeFile(parent,parent_header.size,sizeof(dword),(byte*)(&allocation))) return -3;
	if(!writeFile(parent,parent_header.size,nameSize,name)) return -4;

	*al=allocation;
	return 1;
}
char deleteFile(FILE* file){
	if(!ptrValid(file)) return 0;

	file_header header=getFileHeader(file);
	if(!uncacheFile(file)) return 0;
	if(!fileFree(header.file_addr,sizeof(file_header)+header.size)) return -1;

	FILE parent_file=dwtofile(header.parent_addr);
	byte ptrExisted=ptrExists(parent_file);
	FILE* parent=getPointer(parent_file);
	dword parent_size=getFileHeader(parent).size;
	byte* allocation=(byte*)allocate(parent_size);
	if(!allocation) return -4;
	if(!readFile(parent,parent_size,allocation)) return -5;

	dword i=0;
	dword sizeToTrim=4;
	byte found=0;
	for(;i<parent_size;i++){
		if(*(dword*)(allocation+i)==header.file_addr){
			found=1;
			i+=4;
			for(;allocation[i]&&i<parent_size;i++) sizeToTrim++;
			break;
		}
		i+=4;
		while(allocation[i]&&i<parent_size) i++;
	}
	if(!found) return -6;

	if(!trimFile(parent,i,sizeToTrim)) return -7;

	if(!ptrExisted) closeFile(parent);
	return 1;
}

dword trimFile(FILE* file,dword start,dword size){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	if(size==0) return 0;

	dword fileSize=getFileHeader(file).size;
	size=(start+size>fileSize)?fileSize-start:size;
	byte* allocation=(byte*)allocate(fileSize);
	if(!allocation) return 0;
	if(!readFile(file,fileSize,allocation)){
		free((dword)allocation,fileSize);
		return 0;
	}

	for(dword i=size-1;i<=size;i--)
		allocation[i+start]=allocation[i+start*2];
	//  0  1  2  3  4  5  6  7  8
	// 11 22 33 44 00 00 00 00

	if(!overwriteFile(file,fileSize-size,allocation)){
		free((dword)allocation,fileSize);
		return 0;
	}
	free((dword)allocation,fileSize);
	return size;
}

dword syncFiles(void){
	dword count=0;
	byte name[64];
	printf("\eCqgb;");
	for(fileListPtr i=0;i<MAX_FILEPTRS;i++){
		if(ptrValid(fileList+i)&&fileList[i].flags&1&&fileList+i!=allocsys){
			count++;
			printf("[[");
			printd((dword)(fileList+i));
			printc(';');
			getFileName(fileList+i,name);
			printf("\eCqgb;");
			printf(name);
			printc('|');
			printd(getFileHeader(fileList+i).file_addr);
			printc(':');
			printd(fileList[i].addr);
			printc(',');
			printh(flushCache(fileList+i));
			printf("]]");
			uncacheFile(fileList+i);
		}
	}
	printf("[[");
	printd((dword)(allocsys));
	printc(';');
	getFileName(allocsys,name);
	printf("\eCqgb;");
	printf(name);
	printc('|');
	printd(getFileHeader(allocsys).file_addr);
	printc(':');
	printd(allocsys->addr);
	printc(',');
	printh(flushCache(allocsys));
	printf("]]");
	uncacheFile(allocsys);
	return count;
}


// 1: Signature mismatch
// 2: alloc.sys is missing
byte file_init(void){
	byte buffer[12];
	ahci_read(0x1B0,12,buffer);

	root=*(dword*)buffer;
	char* sig=FILESYSTEM_SIGNATURE;
	for(byte i=0;i<8;i++){
		if(sig[i]!=buffer[i+4]) return 1;
	}
	FILE filenull={0,0,0};
	for(fileListPtr i=0;i<MAX_FILEPTRS;i++)
		fileList[i]=filenull;

	allocsys=resolvePath("/alloc.sys");
	if(!allocsys){
		panic("No allocsys!");
		for(;;);
	}

	return 0;
}

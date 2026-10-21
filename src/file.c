#include "file.h"
#include <stddef.h>


static dword root;
static FILE* allocsys;
extern dword current_pid;






#define MAX_FILEPTRS (1<<16)
static FILE fileList[MAX_FILEPTRS]={};

FILE dwtofile(dword dw){
	FILE file={dw};
	return file;
}

extern dword kernel_finish;
file_header get_file_header(FILE* file);
static byte ptrValid(FILE* file){
	if(!(
		file>=fileList&&
		(dword)file<(dword)fileList+MAX_FILEPTRS*sizeof(FILE)&&
		((dword)file-(dword)fileList)%sizeof(FILE)==0&&
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
/*static byte ptrExists(FILE file){
	for(dword i=0;i<MAX_FILEPTRS;i++)
		if(fileList[i].flags&1){
			if(((file_header*)(fileList[i].addr))->file_addr==file.addr)
				return 1;
		}else{
			if(fileList[i].addr==file.addr)
				return 1;
		}
	return 0;
}*/

byte file_permission(byte req,FILE* file){
	if(!ptrValid(file)) return 0;
	if(current_pid==0) return 1;
	byte isRoot=proc_getFlags(current_pid)&1;
	if(isRoot) return 1;

	byte flags=get_file_header(file).flags;
	if(req==F_PERM_READ)
		return (flags&0x04)>>2;
	if(req==F_PERM_WRITE)
		return (flags&0x02)>>1;
	if(req==F_PERM_EXECUTE)
		return flags&0x01;
	return 0;
}

static byte cache_file(FILE* file){
	if(!ptrValid(file)) return 0;
	if(file->flags&1) return 1;

	file_header header=get_file_header(file);
	dword size=sizeof(file_header)+header.size;
	byte* buffer=(byte*)allocate(size);

	if(ahci_read(file->addr,size,buffer)){
		file->addr=(dword)buffer;
		file->flags|=1;
		return 1;
	}else return 0;
}
static byte uncache_file(FILE* file){
	if(!ptrValid(file)) return 0;
	if(!(file->flags&1)) return 1;
	file_header header=*(file_header*)(file->addr);

	free(file->addr,sizeof(file_header)+header.size);
	file->addr=header.file_addr;
	file->flags&=0xFE;
	return 1;
}
static char flush_cache(FILE* file);
dword cleanup_files(void){
	dword creationLocation=0xFFFFFFFF;
	byte spaceFound=0;
	for(dword i=0;i<MAX_FILEPTRS;i++){
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
static fdesc add_ptr(FILE* file){
	FILE** files=proc_getFiles(current_pid);
	dword creationLocation;
	byte spaceFound=0;
	for(fdesc i=1;i<FILE_ARRAY_SIZE;i++){
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
static fdesc get_desc(pid proc,FILE file){
	dword creationLocation;
	byte spaceFound=0;
	for(dword i=1;i<MAX_FILEPTRS;i++){ //i=1 here because of 'return 0;' at the end
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
static FILE* get_pointer(FILE file){
	dword creationLocation;
	byte spaceFound=0;
	for(dword i=0;i<MAX_FILEPTRS;i++){
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

	return (FILE*)0;
}
byte fclose_desc(pid proc,fdesc desc){
	FILE** files=proc_getFiles(proc);
	if(!ptrValid(files[desc])) return 0;
	if(files[desc]->count==0) return 0;

	files[desc]->count--;
	files[desc]=(FILE*)0;

	return 1;
}
byte fclose_ptr(FILE* ptr){
	if(!ptrValid(ptr)) return 0;
	if(ptr->count==0) return 0;

	ptr->count--;

	return 1;
}



FILE* get_root_pointer(void){
	return get_pointer(dwtofile(root));
}
fdesc get_root_pointer_prog(void){
	return get_desc(current_pid,dwtofile(root));
}


file_header get_file_header(FILE* file){
	file_header ret={};
	if(!ptrValid(file)) return ret;

	if(file->flags&1)
		ret=*(file_header*)(file->addr); // read from cache
	else
		ahci_read(file->addr,sizeof(file_header),(byte*)(&ret)); // read from disk instead

	return ret;
}
dword read_file_prog(FILE* file,dword start,dword size,byte SEG_GS* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_READ,file)) return 0;
	file_header header=get_file_header(file);
	if(header.flags>>6!=0x00) return 0; // not a RWX file
	dword filesize=header.size;
	size=(start+size<=filesize)?size:filesize-start;

	if(file->flags&1){
		for(dword i=0;i<size;i++) // read from cache
			buffer[i]=((byte*)(file->addr))[i+sizeof(file_header)+start];
		return size;
	}else{
		if(ahci_read_prog( // read from disk instead
			file->addr+sizeof(file_header)+start,
			size,
			buffer
		)) return size;
		else return 0;
	}
}
dword read_file(FILE* file,dword start,dword size,byte* buffer){
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


dword get_directory_entries(FILE* directory){
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
	free((dword)allocation,size);

	return ret;
}
FILE* get_directory_entry_from_id(FILE* directory,dword entryNumber){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	file_header header=get_file_header(directory);
	if(header.flags>>6!=0x01) return 0;
	dword size=header.size;
	if(!size) return (FILE*)0;
	byte* allocation=(byte*)allocate(size);
	read_file(directory,0,size,allocation);

	dword i=0;
	for(;i<size&&entryNumber>0;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		entryNumber--;
	}

	if(i>=size) return (FILE*)0;
	FILE file=dwtofile(*(dword*)(allocation+i));
	free((dword)allocation,size);
	return get_pointer(file);
}
fdesc get_directory_entry_from_id_prog(FILE* directory,dword entryNumber){
	FILE* file=get_directory_entry_from_id(directory,entryNumber);
	file->count--;
	return add_ptr(file);
}
static FILE get_directory_entry_from_nameRaw(FILE* directory,char* name){
	FILE ret={0};
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
		if(cmpstr(name,(char*)allocation+i)) break;

		while(allocation[i]&&i<size) i++;
	}

	if(i>=size) return ret;
	ret=dwtofile(*(dword*)(allocation+i-4));
	free((dword)allocation,size);
	return ret;
}
FILE* get_directory_entry_from_name(FILE* directory,char* name){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	FILE ret=get_directory_entry_from_nameRaw(directory,name);
	if(!ret.addr) return (FILE*)0;
	return get_pointer(ret);
}
fdesc get_directory_entry_from_name_prog(FILE* directory,char SEG_GS* name){
	if(!file_permission(F_PERM_READ,directory)) return 0;
	dword size=0;
	for(;name[size]!=0;size++); // triggers a general protection fault when necessary
	char buffer[size+1]={};
	for(dword i=0;name[i]!=0;i++) buffer[i]=name[i];
	FILE ret=get_directory_entry_from_nameRaw(directory,buffer);
	if(!ret.addr) return 0;
	return get_desc(current_pid,ret);
}

byte get_file_name_prog(FILE* file,byte SEG_GS* buffer){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_READ,file)) return 0;
	file_header header=get_file_header(file);
	dword addr=header.file_addr;
	FILE* parent=get_pointer(dwtofile(header.parent_addr));
	header=get_file_header(parent);

	dword size=header.size;
	byte* allocation=(byte*)allocate(size);
	if(!allocation){
		fclose_ptr(parent);
		return 0;
	}
	if(!read_file(parent,0,size,allocation)){
		fclose_ptr(parent);
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
	buffer[i-j]=0;

	fclose_ptr(parent);
	free((dword)allocation,size);
	return ret;
}
byte get_file_name(FILE* file,byte* buffer){
	if(!ptrValid(file)) return 0;
	file_header OLDheader=get_file_header(file);
	FILE parent_file=dwtofile(OLDheader.parent_addr);
	dword addr=OLDheader.file_addr;
	FILE* parent=get_pointer(parent_file);
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
	buffer[i-j]=0;

	fclose_ptr(parent);
	free((dword)allocation,size);
	return ret;
}

/*FILE* resolve_path(char* str){
	if(*str!='/') return (FILE*)0;
	dword size=0;
	for(;str[size]!=0;size++);
	byte buffer[size+1]={};

	FILE preFile=dwtofile(root);
	byte preExists=ptrExists(preFile);
	FILE* currentDir=get_pointer(preFile);

	dword p=0;
	str++;
	for(;*str!=0;str++){
		if(*str!='/') buffer[p++]=*str;
		if(*str=='/'||*(str+1)==0){
			FILE file=get_directory_entry_from_nameRaw(currentDir,(char*)buffer);
			if(!file.addr) return (FILE*)0;
			if(!preExists){
				close_file(currentDir);
			}

			preExists=ptrExists(file);
			FILE* currentDir=get_pointer(file);

			file_header header=get_file_header(currentDir);
			byte flags=header.flags>>6;
			if(flags==0x00){
				if(*(str+1)!=0){
					if(!preExists) close_file(currentDir);
					return (FILE*)0;
				}else return currentDir;
			}else if(flags==0x01){
				if(header.size==0){
					if(!preExists) close_file(currentDir);
					return (FILE*)0;
				}
			}else return (FILE*)0;

			while(p!=0) buffer[--p]=0;
		}
	}
	return (FILE*)0;
}*/

FILE* get_file_parent(FILE* file){
	if(ptrValid(file))
		return get_pointer(dwtofile(get_file_header(file).parent_addr));
	return 0;
}
fdesc get_file_parent_prog(pid proc,FILE* file){
	if(ptrValid(file))
		return get_desc(proc,dwtofile(get_file_header(file).parent_addr));
	return 0;
}
byte get_file_flags(FILE* file){
	if(ptrValid(file))
		return get_file_header(file).flags;
	return 0;
}
byte get_file_type(FILE* file){
	if(ptrValid(file))
		return (get_file_header(file).flags>>6)+1;
	return 0;
}
dword get_file_size(FILE* file){
	if(!ptrValid(file)) return 0;
	return get_file_header(file).size;
}


// if this function returns a negative value, PLEASE issue a kernel panic
static char flush_cache(FILE* file){
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
		FILE parent_file=dwtofile(cacheHeader->parent_addr);
		FILE* parent=get_pointer(parent_file);
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

		free((dword)allocation,parent_size);
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
				FILE f=dwtofile(*(dword*)(data+i));
				FILE* p=get_pointer(f);
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
dword overwrite_file(FILE* file,dword size,byte* buffer){
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
dword overwrite_file_prog(FILE* file,dword size,byte SEG_GS* buffer){
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
dword write_file(FILE* file,dword addr,dword size,byte* buffer){
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

	return size;
}
dword write_file_prog(FILE* file,dword addr,dword size,byte SEG_GS* buffer){
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
char create_file(FILE* parent,byte flags,char* name){
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
char create_file_prog(FILE* parent,byte flags,char SEG_GS* name){
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
	if(!write_file_prog(parent,parent_cache->size,nameSize,name)) return -4;

	return 1;
}
char delete_file(FILE* file){
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

	FILE parent_file=dwtofile(header.parent_addr);
	FILE* parent=get_pointer(parent_file);
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
	FILE filenull={};
	*file=filenull;
	return count;
}
char rename_file(FILE* file,char* name){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	//if(file==allocsys) return 0;
	// renaming alloc.sys can be catastrophic IF the user doesn't know what they're doing
	// However, I chose to trust the user here, since renaming it might be an attempt at recovery

	file_header header=get_file_header(file);
	if(header.file_addr==root) return 0; // renaming the root breaks the assumption of its entry being the first one. do not remove this.

	FILE parent_file=dwtofile(header.parent_addr);
	FILE* parent=get_pointer(parent_file);
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
char rename_file_prog(FILE* file,char SEG_GS* name){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;
	//if(file==allocsys) return 0;
	// renaming alloc.sys can be catastrophic IF the user doesn't know what they're doing
	// However, I chose to trust the user here, since renaming it might be an attempt at recovery

	file_header header=get_file_header(file);
	if(header.file_addr==root) return 0; // renaming the root breaks the assumption of its entry being the first one. do not remove this.

	FILE parent_file=dwtofile(header.parent_addr);
	FILE* parent=get_pointer(parent_file);
	if(!file_permission(F_PERM_WRITE,parent)){
		fclose_ptr(parent);
		return 0;
	}
	byte cached=parent->flags&1;
	if(!cache_file(parent)) return 0;
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
	if(!write_file_prog(parent,parent_cache->size,nameSize,name)) return -4;

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

dword trim_file(FILE* file,dword start,dword size){
	if(!ptrValid(file)) return 0;
	if(!file_permission(F_PERM_WRITE,file)) return 0;

	dword fileSize=get_file_header(file).size;
	if(size==0||start>=fileSize) return fileSize;
	size=(start+size>fileSize)?fileSize-start:size;
	byte* allocation=(byte*)allocate(fileSize);
	if(!allocation) return 0;
	if(!read_file(file,0,fileSize,allocation)){
		free((dword)allocation,fileSize);
		return 0;
	}

	for(dword i=start;i+size<=fileSize;i++)
		allocation[i]=allocation[i+size];
	//  0  1  2  3  4  5  6  7  8
	// 11 22 33 44 00 00 00 00

	if(!overwrite_file(file,fileSize-size,allocation)){
		free((dword)allocation,fileSize);
		return 0;
	}
	free((dword)allocation,fileSize);
	return (fileSize-size>0)?fileSize-size:0xFFFFFFFF;
}

dword sync_files(void){
	dword count=0;
	byte name[64];
	for(dword i=0;i<MAX_FILEPTRS;i++){
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
	for(byte i=0;i<8;i++){
		if(sig[i]!=buffer[i+4]) return 1;
	}

	FILE* root_ptr=get_root_pointer();
	allocsys=get_directory_entry_from_name(root_ptr,"alloc.sys");
	fclose_ptr(root_ptr);
	if(!allocsys) return 2;

	return 0;
}

typedef struct{
	dword addr;
	byte cached;
}FILE; // used by kernel
// a FILE* points to this kernel struct


// Directory entries are structered like so:

// dword addr; // address of the file
// char* name; // null-terminated string of the file's name


typedef struct{
	dword size; // size of the file's data
	dword parent_addr; // parent's address on disk
	dword file_addr; // the file's own address on disk
	byte flags;
	/*flags;
	 * 7-6: File type. 00b=File, 01b=Directory, 10b=Link, 11b=Reserved
	 * 5-3: User RWX
	 * 2-0: Root RWX
	*/
}__attribute__((packed)) file_header;


#define MAX_FILEPTRS 1<<16
typedef word fileListPtr;
static FILE fileList[MAX_FILEPTRS]={};


static FILE* createPointer(FILE file){
	fileListPtr creationLocation;
	byte spaceFound=0;
	for(fileListPtr i=0;i<MAX_FILEPTRS;i++){
		if(!spaceFound&&!fileList[i].addr){
			spaceFound=1
			creationLocation=i;
		}else if(fileList[i].addr==file.addr)
			return fileList+i;
	}

	if(spaceFound){
		fileList[creationLocation]=file;

		return fileList+creationLocation;
	}

	return NULL;
}
static FILE resolvePath(char* str){

}
static file_header getFileHeader(FILE* file){
	file_header ret;

	if(file->cached)
		for(dword i=0;i<sizeof(file_header);i++) // read from cache
			((byte*)ret)[i]=((byte*)(file->addr))[i];
	else
		ahci_read(file->addr,sizeof(file_header),&ret); // read from disk instead

	return ret;
}
void readFile(FILE* file,dword size,void* buffer){
	dword filesize=getFileHeader(file->addr).size;
	size=(size<=filesize)?size:filesize;

	if(file->cached)
		for(dword i=0;i<size;i++) // read from cache
			((byte*)ret)[i]=((byte*)(file->addr))[i];
	else
		ahci_read(
			file->addr+sizeof(file_header),
			size,
			buffer
		); // read from disk instead
}

void dword getDirectoryEntries(FILE* directory){
	dword size=getFileHeader(file->addr).size;
	if(!size) return 0;
	dword allocation=allocate(size);
	readFile(directory,size,(void*)allocation);

	dword ret=0;
	for(dword i=0;i<size;i++){
		i+=4;
		while(allocation[i]&&i<size) i++;
		ret++;
	}
	free(allocation);

	return ret;
}
void FILE* getDirectoryEntry(FILE* directory,dword entryNumber){
	dword size=getFileHeader(file->addr).size;
	if(!size) return NULL;
	dword allocation=allocate(size);
	readFile(directory,size,(void*)allocation);

	dword entry=0;
	for(;entry<size&&entryNumber>0;entry++){
		entry+=4;
		while(allocation[i]&&i<size) entry++;
		entryNumber--;
	}

	// still working on it!

	free(allocation);
}

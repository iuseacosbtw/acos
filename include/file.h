#ifndef FILE_H
#define FILE_H

#include "acos.h"


typedef dword fdesc;


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

file_header get_file_header(FILE* file);
byte file_permission(byte req,FILE* file);
dword cleanup_files(void);

FILE* get_root_pointer(void);
fdesc get_root_pointer_prog(void);
dword read_file_prog(FILE* file,dword start,dword size,byte SEG_GS* buffer);
dword read_file(FILE* file,dword start,dword size,byte* buffer);
byte fclose_ptr(FILE* ptr);
byte fclose_desc(pid proc,fdesc desc);
dword get_directory_entries(FILE* directory);
FILE* get_directory_entry_from_id(FILE* directory,dword entryNumber);
fdesc get_directory_entry_from_id_prog(FILE* directory,dword entryNumber);
FILE* get_directory_entry_from_name(FILE* directory,char* name);
fdesc get_directory_entry_from_name_prog(FILE* directory,char SEG_GS* name);
//FILE* resolve_path(char* str);
byte get_file_name(FILE* file,byte* buffer);
byte get_file_name_prog(FILE* file,byte SEG_GS* buffer);
FILE* get_file_parent(FILE* file);
fdesc get_file_parent_prog(pid proc,FILE* file);
byte get_file_flags(FILE* file);
byte get_file_type(FILE* file);
dword get_file_size(FILE* file);

dword overwrite_file(FILE* file,dword size,byte* buffer);
dword overwrite_file_prog(FILE* file,dword size,byte SEG_GS* buffer);
dword write_file(FILE* file,dword addr,dword size,byte* buffer);
dword write_file_prog(FILE* file,dword addr,dword size,byte SEG_GS* buffer);
dword trim_file(FILE* file,dword start,dword size);
dword sync_files(void);

char create_file(FILE* parent,byte flags,char* name);
char create_file_prog(FILE* parent,byte flags,char SEG_GS* name);
char delete_file(FILE* file);
char rename_file(FILE* file,char* name);
char rename_file_prog(FILE* file,char SEG_GS* name);

dword fileAllocate(dword size);
byte fileFree(dword addr,dword size);

FILE* dummyFile(void);


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

#endif

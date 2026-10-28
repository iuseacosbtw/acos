#ifndef ACOS_H
#define ACOS_H

#include <stdint.h>


void* memset(void* s,int c,unsigned int n){
	for(unsigned int i=0;i<n;i++)
		((char*)s)[i]=c;
	return s;
}
void memcpy(void* dest,const void* src,unsigned int num){
	for(unsigned int i=0;i<num;i++)
		((char*)dest)[i]=((char*)src)[i];
}
void memmove(void *dest, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d > s && d < s + n) {
        for (unsigned int i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    } else {
        // Copy forward safely
        for (unsigned int i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
}
int memcmp(const void *str1, const void *str2, unsigned int n) {
    const unsigned char *s1 = (const unsigned char *)str1;
    const unsigned char *s2 = (const unsigned char *)str2;

    for (unsigned int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
    }

    return 0;
}

// I/O
static inline void exit(int code){
	__asm__ volatile(
		"pushl %0\n"
		"xor %%eax,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		:
		: "g" (code)
		:
	);
}
static inline char vgatext_set(unsigned char x,unsigned char y){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $1,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (x),"g" (y)
		:
	);
	return ret;
}
static inline char vgatext_scroll(unsigned char lines){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $2,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (lines)
		:
	);
	return ret;
}
static inline char printc(char c){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $3,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (c)
		:
	);
	return ret;
}
static inline char printf(char* str){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $4,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (str)
		:
	);
	return ret;
}
static inline char print8(uint8_t value){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $5,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (value)
		:
	);
	return ret;
}
static inline char print16(uint16_t value){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $6,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (value)
		:
	);
	return ret;
}
static inline char print32(uint32_t value){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $7,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (value)
		:
	);
	return ret;
}
static inline char printn(int value){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $8,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (value)
		:
	);
	return ret;
}
static inline char clear(void){
	char ret;
	__asm__ volatile(
		"movl $9,%%eax\n"
		"int $0x30\n"
		: "=a" (ret)
		:
		:
	);
	return ret;
}
static inline char panic(char* str){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $10,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (str)
		:
	);
	return ret;
}
static inline char getchar(void){
	char ret;
	__asm__ volatile(
		"movl $11,%%eax\n"
		"int $0x30\n"
		: "=a" (ret)
		:
		:
	);
	return ret;
}
static inline char cmpstr(char* str1,char* str2){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $12,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (str1),"g" (str2)
		:
	);
	return ret;
}

// Files

#define F_TYPE 0xC0
#define F_TYPE_FILE 0x00
#define F_TYPE_DIR 0x40

#define F_FLAGS_UNCACHEABLE 0x20
#define F_FLAGS_READABLE 0x04
#define F_FLAGS_WRITABLE 0x02
#define F_FLAGS_EXECUTABLE 0x01

typedef uint32_t file_t;

static inline file_t fget_root(void){
	file_t ret;
	__asm__ volatile(
		"movl $13,%%eax\n"
		"int $0x30\n"
		: "=a" (ret)
		:
		:
	);
	return ret;
}
static inline unsigned int fread(file_t file,unsigned int start,unsigned int size,char* buffer){
	unsigned int ret;
	__asm__ volatile(
		"pushl %4\n"
		"pushl %3\n"
		"pushl %2\n"
		"pushl %1\n"
		"movl $14,%%eax\n"
		"int $0x30\n"
		"addl $16,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (start),"g" (size),"g" (buffer)
		:
	);
	return ret;
}
static inline char fclose(file_t file){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $15,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline unsigned int fdir_count(file_t directory){
	unsigned int ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $16,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (directory)
		:
	);
	return ret;
}
static inline file_t fdir_fromID(file_t directory,unsigned int id){
	file_t ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $17,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (directory),"g" (id)
		:
	);
	return ret;
}
static inline file_t fdir_fromNAME(file_t directory,char* name){
	file_t ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $18,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (directory),"g" (name)
		:
	);
	return ret;
}
static inline char fget_name(file_t file,char* buffer){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $19,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (buffer)
		:
	);
	return ret;
}
static inline unsigned char fget_flags(file_t file){
	unsigned char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $20,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline unsigned int fget_size(file_t file){
	unsigned int ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $21,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline file_t fget_parent(file_t file){
	file_t ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $22,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline char fcreate(file_t file,unsigned char flags,char* name){
	char ret;
	__asm__ volatile(
		"pushl %3\n"
		"pushl %2\n"
		"pushl %1\n"
		"movl $23,%%eax\n"
		"int $0x30\n"
		"addl $12,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (flags),"g" (name)
		:
	);
	return ret;
}
static inline char fdelete(file_t file){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $24,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline char frename(file_t file,char* name){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $25,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (name)
		:
	);
	return ret;
}
static inline unsigned int foverwrite(file_t file,unsigned int size,char* buffer){
	unsigned int ret;
	__asm__ volatile(
		"pushl %3\n"
		"pushl %2\n"
		"pushl %1\n"
		"movl $26,%%eax\n"
		"int $0x30\n"
		"addl $12,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (size),"g" (buffer)
		:
	);
	return ret;
}
static inline unsigned int fwrite(file_t file,int start,int size,char* buffer){
	unsigned int ret;
	__asm__ volatile(
		"pushl %4\n"
		"pushl %3\n"
		"pushl %2\n"
		"pushl %1\n"
		"movl $27,%%eax\n"
		"int $0x30\n"
		"addl $16,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (start),"g" (size),"g" (buffer)
		:
	);
	return ret;
}
static inline unsigned int ftrim(file_t file,unsigned int start,unsigned int size){
	unsigned int ret;
	__asm__ volatile(
		"pushl %3\n"
		"pushl %2\n"
		"pushl %1\n"
		"movl $28,%%eax\n"
		"int $0x30\n"
		"addl $12,%%esp"
		: "=a" (ret)
		: "g" (file),"g" (start),"g" (size)
		:
	);
	return ret;
}
static inline unsigned int fsync(void){
	unsigned int ret;
	__asm__ volatile(
		"movl $29,%%eax\n"
		"int $0x30\n"
		: "=a" (ret)
		:
		:
	);
	return ret;
}

// Processes

typedef uint32_t pid_t;

static inline pid_t exec(file_t file){
	pid_t ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $30,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (file)
		:
	);
	return ret;
}
static inline char pkill(pid_t process,unsigned char urgency){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $31,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (process),"g" (urgency)
		:
	);
	return ret;
}
static inline char beep(unsigned short frequency,unsigned short time){
	char ret;
	__asm__ volatile(
		"pushl %2\n"
		"pushl %1\n"
		"movl $32,%%eax\n"
		"int $0x30\n"
		"addl $8,%%esp"
		: "=a" (ret)
		: "g" (frequency),"g" (time)
		:
	);
	return ret;
}
static inline unsigned int pget_flags(void){
	unsigned int ret;
	__asm__ volatile(
		"movl $33,%%eax\n"
		"int $0x30\n"
		: "=a" (ret)
		:
		:
	);
	return ret;
}
static inline void* alloc(unsigned int size){
	void* ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $34,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (size)
		:
	);
	return ret;
}
static inline char free(void* ptr){
	char ret;
	__asm__ volatile(
		"pushl %1\n"
		"movl $35,%%eax\n"
		"int $0x30\n"
		"addl $4,%%esp"
		: "=a" (ret)
		: "g" (ptr)
		:
	);
	return ret;
}

#endif

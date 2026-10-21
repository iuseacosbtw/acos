#include "acos.h"

dword str_to_dword(char* str,byte* valid){
	*valid=0;
	dword ret=0;
	dword mul=1;
	dword size=0;

	for(;*str!=0;str++) size++;
	for(;size>0;size--){
		str--;
		byte a=*(byte*)str;
		if(a<'0'||a>'9') return 0;
		a-='0';
		ret+=a*mul;

		if(ret<a*mul) return 0xFFFFFFFF;
		mul*=10;
	}
	*valid=1;
	return ret;
}
void printn(dword number){
	if(!number){
		printc('0');
		return;
	}
	
	byte digits=0;
	for(dword n=number;n;){
		digits++;
		n/=10;
	}
	
	byte nums[digits];
	for(byte i=0;i<digits;i++){
		nums[digits-i-1]=number%10;
		number/=10;
	}
	
	for(byte i=0;i<digits;i++)
		printc(nums[i]+'0');
}
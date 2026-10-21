#include "acos.h"

#define ARG_LIMIT 16

unsigned int str_to_int(char* str,char* valid){
	*valid=0;
	unsigned int ret=0;
	unsigned int mul=1;
	unsigned int size=0;

	for(;*str!=0;str++) size++;
	for(;size>0;size--){
		str--;
		char a=*(char*)str;
		if(a<'0'||a>'9') return 0;
		a-='0';
		ret+=a*mul;

		if(ret<a*mul) return 0xFFFFFFFF;
		mul*=10;
	}
	*valid=1;
	return ret;
}

void main(){
	char input[200];
	char fileName[256];
	file_t current=fget_root();
	int p;
	goto _cleanup;
	for(;;){
		char c=getchar();
		if(c<0x5);
		else if(c=='\b'){
			if(p>0){
				p--;
				input[p]=0;
				printc(c);
			}
		}else if(c=='\n'){
			printc(c);
			if(input[0]==0) goto _cleanup;
			for(int i=0;i<200;i++)
				if(input[i]==' ') input[i]=0;
			char* argv[ARG_LIMIT]={input};
			int argc=1;

			for(int i=1;i<ARG_LIMIT;i++){
				argv[i]=argv[i-1];
				while(*argv[i]) argv[i]++;
				while(!*argv[i]&&argv[i]!=input+200) argv[i]++;
				if(argv[i]==input+200){
					for(;i<ARG_LIMIT;i++)
						argv[i]=0;
					break;
				}
				argc++;
			}

			if(cmpstr(input,"print")){
				for(int i=1;i<argc;i++){
					printf(argv[i]);
					printc(' ');
				}
			}else if(cmpstr(input,"cd")){
				if(argc!=2)
					printf("Improper set of arguments for cd.");
				else{
					file_t new_dir;
					if(cmpstr(argv[1],".")) new_dir=current;
					else if(cmpstr(argv[1],"..")) new_dir=fget_parent(current);
					else if(cmpstr(argv[1],"/")) new_dir=fget_root();
					else new_dir=fdir_fromNAME(current,argv[1]);
					if(new_dir&&new_dir!=current){
						fclose(current);
						current=new_dir;
					}
				}
			}else if(cmpstr(input,"ls")||cmpstr(input,"dir")){
				fget_name(current,fileName);
				printf(fileName);
				printf(" contains ");
				unsigned int count=fdir_count(current);
				printn(count);
				printf(" files:\n");
				for(unsigned int i=0;i<count;i++){
					file_t file=fdir_fromID(current,i);
					if(!file)
						printc('!');
					else{
						printn(fget_size(file));
						printc('B');

						unsigned char flags=fget_flags(file);
						if((flags&F_TYPE)==F_TYPE_FILE) printf(" \eCqg;FILE\eCf;");
						else if((flags&F_TYPE)==F_TYPE_DIR) printf(" \eCqgb;DIR\eCf;");

						printf(": ");
						fget_name(file,fileName);
						printf(fileName);
						printc('\n');

						fclose(file);
					}
				}
			}else if(cmpstr(input,"read")||cmpstr(input,"cat")){
				if(argc!=2)
					printf("Improper set of arguments for read.");
				else{
					file_t file=fdir_fromNAME(current,argv[1]);
					if(file&&(fget_flags(file)&F_TYPE)==F_TYPE_FILE){
						unsigned int size=fget_size(file);
						fget_name(file,fileName);
						if(size>2048){
							printc('\'');
							printf(fileName);
							printf("' is too large to read! (");
							printn(size);
							printc('>');
							printn(2048);
							printc(')');
						}else{
							char buffer[size];
							if(fread(file,0,size,buffer))
								for(unsigned int i=0;i<size;i++)
									printc(buffer[i]);
							else{
								printf("Unable to read '");
								printf(fileName);
								printf("'.");
							}
						}
						fclose(file);
					}else printf("Not a readable file.");
				}
			}else if(cmpstr(input,"exec")){
				if(argc!=2)
					printf("Improper set of arguments for exec.");
				else{
					file_t file=fdir_fromNAME(current,argv[1]);
					if(file&&(fget_flags(file)&F_TYPE)==F_TYPE_FILE&&fget_flags(file)&F_FLAGS_EXECUTABLE){
						pid_t pid=exec(file);
						if(!pid)
							printf("Unable to create process.");
						else{
							printf("File executed with process ID ");
							printn(pid);
							printc('.');
						}
						fclose(file);
					}else printf("Not an executable file.");
				}
			}else if(cmpstr(input,"end")){
				if(argc!=2)
					printf("Improper set of arguments for end.");
				else{
					char valid;
					unsigned int num=str_to_int(argv[1],&valid);
					if(!valid)
						printf("Invalid number");
					else{
						if(pkill(num,0)){
							printf("Process ");
							printn(num);
							printf(" was successfully terminated.");
						}else{
							printf("Process ");
							printn(num);
							printf(" did not terminate. Maybe try `kill`?");
						}
					}
				}
			}else if(cmpstr(input,"kill")){
				if(argc!=2)
					printf("Improper set of arguments for kill.");
				else{
					char valid;
					unsigned int num=str_to_int(argv[1],&valid);
					if(!valid)
						printf("Invalid number");
					else{
						if(pkill(num,1)){
							printf("Process ");
							printn(num);
							printf(" was successfully terminated.");
						}else{
							printf("Process ");
							printn(num);
							printf(" did not terminate.");
						}
					}
				}
			}else if(cmpstr(input,"beep")){
				if(argc!=3)
					printf("Improper set of arguments for beep.");
				else{
					char valid;
					char valid2;
					unsigned int num=str_to_int(argv[1],&valid);
					unsigned int num2=str_to_int(argv[2],&valid2);
					if(!valid||!valid2)
						printf("Invalid number");
					else{
						beep(num,num2);
					}
				}
			}else if(cmpstr(input,"exit")){
				while(fsync());
				exit(0);
			}else{
				printf("\eCqrg;Invalid command '");
				printf(input);
				printf("'.\eCF;");
			}

			_cleanup:
			for(int i=0;i<200;i++) input[i]=0;
			p=0;
			printc('\n');
			fget_name(current,fileName);
			printf(fileName);
			printf("> ");
		}else if(p<199){
			input[p]=c;
			p++;
			printc(c);
		}
	}
}

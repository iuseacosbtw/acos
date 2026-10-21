byte input[128]={0};
byte p=0;
prints("\n> ");
for(;;){
	byte c=getchar();
	if(c=='\b'&&p>0){
		p--;
		input[p]=0;
		printc(c);
	}else if(c=='\n'){
		byte val;
		dword result=str_to_dword(input,&val);
		clear();
		if(val){
			prints("Loading sector 0x");
			printd(result);
			prints("...");

			byte buffer[512];
			if(ahci_read(result*512,512,buffer)){
				clear();
				prints("Contents of sector 0x");
				printd(result);
				prints(" (offset: 0x");
				printd(result*0x200);
				prints("):\n\n");
				for(word i=0;i<512;i++){
					printh(buffer[i]);
					printc_raw(' ');
				}
				prints("\n\n> ");
			}else{
				printf("\eCqr;Everything went wrong!\n\eCF;");
			}
		}else{
			prints("Invalid number!\n\n> ");
		}
		for(p=0;p<128;p++) input[p]=0;
		p=0;
	}else if(p<128){
		input[p]=c;
		p++;
		printc(c);
	}
}

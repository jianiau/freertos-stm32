#include "fio.h"
#include <stdarg.h>
#include "clib.h"
#include "host.h"

void osDbgPrintf(const char * format, ...) {

	int i,p;

	va_list v1;
	va_start(v1, format);

	int tmpint;
	char *tmpcharp;
	char dest[500];


	for(i=0,p=0; format[i]; ++i){
		if(format[i]=='%'){
			switch(format[i+1]){
				case '%':
					dest[p++]='%'; break;
					break;
				case 'x':
				case 'X':
					tmpint = va_arg(v1, int);
					tmpcharp = utoa(format[i+1]=='x'?"0123456789abcdef":"0123456789ABCDEF",(unsigned)tmpint, 16);
					for(;*tmpcharp;++tmpcharp, ++p)
						dest[p]=*tmpcharp;
					break;
				case 'u':
				case 'd':
					tmpint = va_arg(v1, int);
					if (format[i+1]=='u')
						tmpcharp = utoa("0123456789",(unsigned)tmpint, 10);
					else
						tmpcharp = itoa("0123456789",(unsigned)tmpint, 10);
					for(;*tmpcharp;++tmpcharp, ++p)
						dest[p]=*tmpcharp;
					break;
				case 's':
					tmpcharp = va_arg(v1, char *);
					for(;*tmpcharp;++tmpcharp, ++p)
						dest[p]=*tmpcharp;
					break;
			}
			/* Skip the next character */
			++i;
		}else
			dest[p++]=format[i];
	}
	
	va_end(v1);
	dest[p]='\0';
	host_action (SYS_WRITE0,dest);	
}

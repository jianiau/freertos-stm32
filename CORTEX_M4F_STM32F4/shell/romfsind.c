#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs.h"
#include "osdebug.h"
#include "hash-djb2.h"
#include "clib.h"

extern const unsigned char _sromind;
extern uint32_t pwd_hash;

int pwd (uint32_t hash, char *buf) {
    uint8_t name_len=0;
    const uint8_t * file;
    file =romfs_get_file_by_hash(&_sromind, hash, NULL);
    name_len = (uint8_t)file[0] & 0x7f;
    if (name_len) {
        strncpy(buf,(const char *)file+1,name_len-1);
        buf[name_len-1]='\0';
        return 1;
    } else {
        buf[0]='\0';
        return 0;
    }
}


int ls (uint32_t hash) {
    uint32_t dir_len, offset=0;
    uint8_t  name_len=0;
    const uint8_t * file;
    file =romfs_get_file_by_hash(&_sromind, hash, &dir_len);

    name_len = (uint8_t)file[0] & 0x7f;
    offset = offset + name_len + 1;


    while ( dir_len>offset ) {
        name_len = (uint8_t)file[offset];
        if (name_len & 0x80) {
            name_len &= 0x7f ;
            fio_write(1, file+offset+1, name_len);
            fio_printf(1, "/");
        } else {
            fio_write(1, file+offset+1, name_len);
        }

        fio_printf(1, "\r\n");
        offset = offset + name_len + 1;

    }

    return 0;
}

int get_parent_path (char* path) {
    int i;
    for (i=strlen(path)-1; i>=0; i--) {
        if (path[i]=='/')
            break;
    }
    path[i]='\0';
    return i;
}

int get_full_path (char *path, char *buf) {
    int src_p=0, dst_p=0;

    if (path[0]=='/') {
        // abs path, "/romfs" => ""
        strncpy(buf,path,6);
        if (strcmp("/romfs",buf)==0) {
            for (dst_p=0; dst_p<6; dst_p++) {
                buf[dst_p]='\0';
            }
            dst_p=0;
            src_p=6;
        }
    } else {
        pwd(pwd_hash,buf);
        dst_p=strlen(buf);
        buf[dst_p++]='/';
    }

    while (src_p <= strlen(path)) {
        //  replace .
        if (path[src_p]=='.' && (path[src_p+1]=='/' || path[src_p+1]=='\0')) {
            if (buf[dst_p-1]=='/') {
                buf[dst_p-1]='\0';
            } else {
                buf[dst_p]='\0';
            }
            src_p++;
        }
        // replace ..
        if (path[src_p]=='.' && path[src_p+1]=='.'&& (path[src_p+2]=='/' || path[src_p+2]=='\0')) {
            // string trimright '/'
            if (buf[dst_p-1]=='/') {
                buf[dst_p-1]='\0';
            } else {
                buf[dst_p]='\0';
            }
            dst_p=get_parent_path(buf);
            if (dst_p<0) {
                // parent is root
                buf[0]='\0';
                dst_p=0;
            }
            src_p+=2;
        }
        buf[dst_p++]=path[src_p++];
    }
    buf[dst_p]='\0';

    // string trim '/'
    if (buf[0]=='/')
        strcpy(buf,buf+1);
    while (buf[strlen(buf)-1]=='/') {
        buf[strlen(buf)-1]='\0';
    }
    return strlen(buf);
}


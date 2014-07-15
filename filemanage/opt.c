
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <pwd.h> 
#include <grp.h> 


#ifdef DEBUG
#define debug_msg(format,...)   \
    fprintf(stdout,format,##__VA_ARGS__)
#else
    #define debug_msg(format,...)
#endif   /* DEBUG */

#include "str.h"

static int enum_opt_dir(int,char * ,int,char *,int,int);
static int cpfile( const char *, const char *,const int);



int isIllegalChar (char * lp,size_t size){
    int i = 0;  
	for ( ;size > 0 ;size--){
		memcpy (&i,lp,1);
		debug_msg ("%X"",",i);
		if ((i < 32 ) && (10 != i ) && (13 != i) && (9 != i)){ //32:space 10:x0A 13:x0D 09:TAB
			return 1;
		}
        lp++;
	}
	return 0;
}

int do_readText(char * lpFileName,char *lpBuf,size_t dwBufSize){
	int ret = 0;
    if (0 == access(lpFileName,F_OK)){		
		FILE* fp = fopen(lpFileName, "rb");
		if (fp){
			fseek(fp, 0L, SEEK_END);   
			int fileSize = ftell(fp);
			fseek(fp, 0L, SEEK_SET);
			if (fileSize > 0){
				if (fileSize > dwBufSize - 1){
				    ret = 5;
				}else{
					debug_msg ("open file success: size:%i \n",fileSize);
					if (fileSize != fread(lpBuf, sizeof(char),fileSize,fp)){
						debug_msg ("* fail to read edit file \n");	
						ret = 2;
					}else{
						debug_msg ("\n");
						if (isIllegalChar (lpBuf,fileSize)){
							debug_msg ("\n* fail,include illegal char,not pure text file\n");
							ret = 4;
						}
						debug_msg ("\n");
					}
				}
			}
			fclose(fp);
		}else{
		    ret = 3;
		}
	}else{
		debug_msg ("* edit file not exists \n");	
		ret = 1;
	}   
	return ret;
}


int readtext(char *lpBaseDir,char * input,char * buffer,size_t bufferSize){
	char objName[PATH_MAX] = {0};
	int e = 0;
	strcpy (objName,lpBaseDir);
	strcat (objName,"/");
	if (input){
		strcat (objName,input);
		sprintf(buffer,"$mod=edit""\x04""$b=%s""\x04""$i=%s""\x04""$c=",lpBaseDir,input);

		char * lpFileBuf = buffer + strlen(buffer);

		debug_msg ("Edit: obj: %s, bufferSize: %zu ,buff: %s \n",objName,bufferSize,buffer);

		if (0 != (e = do_readText(objName,lpFileBuf,bufferSize - strlen(buffer) - 1))){
			sprintf(buffer,"$mod=edit""\x04""$b=%s""\x04""$e=%i""\x04",objName,e);  
		}else{
			strcat (buffer,"\x04");
		}
	}else{
		sprintf(buffer,"$mod=edit""\x04""$b=%s""\x04""$e=1""\x04",objName);  
	}
	return 0;
}


int savetext(char *lpBaseDir,char * input,char * editContents,char * buffer){
	int lasterror = 0;
	char lpFileName[PATH_MAX] = {0};
	strcpy (lpFileName,lpBaseDir);
	strcat (lpFileName,"/");
	if (input){
		strcat (lpFileName,input);

		if (0 == access(lpFileName,F_OK)){	
			FILE* fp = fopen(lpFileName, "wb");
			if (fp){
				int fileSize = strlen(editContents);
				if (fileSize != fwrite(editContents, sizeof(char),fileSize,fp)){
					debug_msg ("* fail to write edit file \n");	
					lasterror = 6;
				}
				fclose(fp);
			}else{
				lasterror = 3;
			}
		}else{
			debug_msg ("* edit file not exists \n");	
			lasterror = 1;
		}
	}else{
	    lasterror = 1;
	}
	sprintf(buffer,"$mod=edit""\x04""$s=1""\x04""$e=%i""\x04",lasterror);
	debug_msg ("Edit Contents: %s \n",editContents);
	return 0;
}


//0 success
//$warning[3] = 'The uploaded file exists already';
//$warning[4] = 'The uploaded file transmission fail';
//$warning[5] = 'The uploaded file was only partially uploaded';
//$warning[6] = 'The uploaded file create fail';
int do_upload(void * streamFunc,REQUEST_HEADER * lpReqBuff,char *lpBaseDir,char * fileName,int index,int uploadSize,int overWrite){
	char buf[1024] = {0};
	strcpy (buf,lpBaseDir);
	strcat (buf,"/");
	strcat (buf,fileName);

	debug_msg ("upload: saveas %s , index: %i , size: %i ,overWrite: %i \n",buf,index,uploadSize,overWrite);

    int ret = 0;

	if (0 == overWrite){ //exists?
        if (0 == access(buf,0)){
		    return 3;
		} 
	}
	
	int (* connUpStream) (REQUEST_HEADER * , int,char *,int); 
    connUpStream = streamFunc;

	FILE * handle = fopen(buf,"wb");
	if (handle){
		ret = 4;
		while (uploadSize){
			int i = connUpStream(lpReqBuff,index,buf,1024);
			if ((i > 0)&&(i <= uploadSize)){
				fwrite (buf , sizeof(char), i, handle);
                uploadSize -= i;
				ret = 5;				
			}else{
				break;
			}
		}		
		if (0 == uploadSize){
			ret = 0;
		}
		fclose(handle);
	}else{
	    return 6;
	}

    return ret;
}

int optDelete(char *obj){
	int ret = 0;
    struct stat info;
	stat(obj,&info);
	if (S_ISDIR(info.st_mode)){ //enum child Dir
	    debug_msg ("Delete Directory... : %s \n",obj);
	    ret = enum_opt_dir(0,obj,1,0,0,0);		
	}
	if (0 == ret){
		debug_msg ("Delete File or Directory... : %s \n",obj);
		ret = remove (obj);
	}
    return ret;
}

int optChown(const char * src,const char * groupname,const char * username,const int isEnum){
    int ret = 0;
    struct group *group;  
	struct passwd *pwd;  
    
	gid_t gid;
	uid_t uid;

    if ((group = getgrnam(groupname)) == NULL) {
        gid = -1;
    }else{
	    gid = group->gr_gid;
	}
	
    if ((pwd = getpwnam(username)) == NULL) {
        uid = -1;
    }else{
	    uid = pwd->pw_uid;
	}

	struct stat info;
	stat(src,&info);
	ret = chown(src,uid,gid);
	debug_msg ("chown: objName %s group %i  user %i Enum %i ret: %i \n",src,gid,uid,isEnum,ret);    
	if (0 == ret){
		if (S_ISDIR(info.st_mode)){ //enum child Dir	
	        if (isEnum){
                ret = enum_opt_dir(0,(char *)src,4,0,uid,gid);
			}
		}	
	}
    return ret;
}

int optChmod(const char * src,const int mode,const int isEnum){
    int ret = 0;
    struct stat info;
	stat(src,&info);
	ret = chmod (src,mode);
	debug_msg ("[optChmod]chmod %s set %i , ret: %i\n",src,mode,ret);
	if (0 == ret){
		if (S_ISDIR(info.st_mode)){ //enum child Dir	
	        if (isEnum){
                ret = enum_opt_dir(0,(char *)src,3,0,mode,0);
			}
		}	
	}
	return ret;
}

int optCopyMove(char * src, char* dst,char *filename,const int overWrite,const int isMove){

    strcat (dst,"/");
	strcat (dst,filename);		

    int ret = 0;
    struct stat info;
	stat(src,&info);
	if (S_ISDIR(info.st_mode)){ //enum child Dir	
		ret = mkdir(dst,S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
		if ((0 == ret) || (overWrite)){
            ret = enum_opt_dir(0,src,2,dst,overWrite,isMove);
		}	    
	}else{
		ret = cpfile(src,dst,overWrite);		
		debug_msg ("copy File : %s  to : %s . over write %i ,ret %i \n",src,dst,overWrite,ret);		
	}
	
	if ((isMove) && (0 == ret)){
		ret =  remove (src);
		debug_msg ("Delete File or Directory... : %s . ret: %i \n",src,ret);
	}
    return ret;
}


int cpfile( const char *src, const char *dst,const int overWrite){

    char buf[4096];
    struct stat info;
	
    size_t size;

    if (!overWrite){
        if (-1 != stat(dst,&info)){
		    return -1;
		}
    }

    FILE* source = fopen(src, "rb");
	FILE* dest =  fopen(dst, "wb");
    
	if ((source) && (dest)){
		   
		while ((size = fread(buf, 1, 4096, source))) {
			fwrite(buf, 1, size, dest);
		}

		fclose(source);
		fclose(dest);

		return 0;
	}

	return -1;


}

//enum and operate Dirs
//0  success
//-1 fail,be broken
//max level = 25 最深不超过25
int enum_opt_dir(int level,char * dir,int opt,char *dst,int para,int para1){

	int ret = 0;
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;	

	level++;
	if (level > 25){
		debug_msg ("enum_fail! too deep level... : %i \n",level);
		return -1;
	}

    debug_msg ("enum_opt_dir... : %s \n",dir);

	if((dp = opendir(dir)) == NULL) {        
        return -1;
    }
	
	char * lpInsert    = dir + strlen(dir);
	char * lpDstInsert = 0;
	if (dst){
		lpDstInsert = dst + strlen(dst);
	}

	while((entry = readdir(dp)) != NULL) {	
		strcpy (lpInsert,"/");
		strcat (lpInsert,entry->d_name);

		if (lpDstInsert){
			strcpy (lpDstInsert,"/");
			strcat (lpDstInsert,entry->d_name);
		}
            
        lstat(dir,&statbuf);
		
		if(S_ISDIR(statbuf.st_mode)) {
            if(strcmp(".",entry->d_name) == 0 || 
                strcmp("..",entry->d_name) == 0)
                continue;

			if (2 == opt){
			    ret = mkdir(dst,S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
				if ((0 != ret) && (!para)){
					ret = -1;
					break;
				}	
			}
			
			if (0 != enum_opt_dir(level,dir,opt,dst,para,para1)){
			    ret = -1;
				break;
			}
			
        }else{
		    if (2 == opt){
				ret = cpfile(dir,dst,para);
		 	    debug_msg ("copy file : %s  to : %s . over write %i ,ret %i \n",dir,dst,para,ret);
				if (-1 == ret){
					break;
				}
			}
		}		
		
		if (1 == opt){
			debug_msg ("Delete ... : %s \n",dir);
		    if (0 != remove (dir)){
			    ret = -1;
				break;
			}			
		}
		if ((2 == opt) && (para1)){
			debug_msg ("Delete ... : %s \n",dir);
		    if (0 != remove (dir)){
			    ret = -1;
				break;
			}			
		}

		if (3 == opt){ //chmod
            ret = chmod (dir,para);
			debug_msg ("[enum_opt_dir]chmod %s set %i , ret: %i\n",dir,para,ret);
		}else if (4 == opt){ //chown
            ret = chown(dir,para,para1);
			debug_msg ("[enum_opt_dir]chown %s set %i:%i , ret: %i\n",dir,para,para1,ret);
		}
		

	}

	memset (lpInsert,0,1);

	if (lpDstInsert){
		memset (lpInsert,0,1);
	}

	closedir(dp);

	return ret;

}


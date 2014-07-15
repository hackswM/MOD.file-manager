#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <time.h>
#include <pwd.h> 
#include <grp.h> 

#ifdef DEBUG
#define debug_msg(format,...)   \
    fprintf(stdout,format,##__VA_ARGS__)
#else
    #define debug_msg(format,...)
#endif   /* DEBUG */

#include "str.h"

int do_upload(void*,REQUEST_HEADER *,char *,char * ,int,int,int);
int optDelete(char *);
int optCopyMove(char *,char*,char*,const int,const int);
int optChmod   (const char * ,const int ,const int );
int optChown   (const char * ,const char *,const char *,const int);
int readtext(const char *,const char *,char *,size_t);
int savetext(const char *,const char *,const char *,char *);

int init(char *buff)
{
	if (buff){	
	    char tmp[] = {0x01,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x10};
        memcpy(buff,tmp,16);
		return 0;
	}else{
	    return -1;
	}
}



void transfer_mode(mode_t mode, char mode_str[])  
{  
    memset (mode_str,'-',10);  

    if(S_ISDIR(mode))   mode_str[0] = 'd';  
    if(S_ISCHR(mode))   mode_str[0] = 'c';  
    if(S_ISBLK(mode))   mode_str[0] = 'b';  
    if(S_ISLNK(mode))   mode_str[0] = 'l';  
    if(S_ISFIFO(mode))   mode_str[0] = 'p';  
    if(S_ISSOCK(mode))   mode_str[0] = 's';  
  
    if(mode & S_IRUSR)  mode_str[1] = 'r';  
    if(mode & S_IWUSR)  mode_str[2] = 'w';  
    if(mode & S_IXUSR)  mode_str[3] = 'x';  
  
    if(mode & S_IRGRP)  mode_str[4] = 'r';  
    if(mode & S_IWGRP)  mode_str[5] = 'w';  
    if(mode & S_IXGRP)  mode_str[6] = 'x';  
  
    if(mode & S_IROTH)  mode_str[7] = 'r';  
    if(mode & S_IWOTH)  mode_str[8] = 'w';  
    if(mode & S_IXOTH)  mode_str[9] = 'x';  
}  

void uid_to_name(uid_t uid,char* user_id)  
{  
    struct passwd *pwd;    
  
    if((pwd = getpwuid(uid)) == NULL){  
        sprintf(user_id, "%08d", uid);  
    } else  {
	    strncpy (user_id,pwd->pw_name,8);
	}
	return;
}  

void gid_to_name(gid_t gid,char* group_id)  
{  
    struct group *group;  
  
    if((group = getgrgid(gid)) == NULL)  
    {  
        sprintf(group_id, "%08d", gid);   
    } 
    else  {
	    strncpy (group_id,group->gr_name,8);  
	}
	return;
        
}  
//ret :
// 0 success
// 1 malloc fail
// 2 open dir fail
// 5 主内存不足
int printdir(char *dir, char *buff, int buffSize)
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
	struct tm *local;

	int dirLen = strlen(dir);

	int error = 0;

	debug_msg ("base dir: %s \n",dir);

	char * tmpBuff = malloc(1024 * 1024);
	if (!tmpBuff){
		return 1;
	}

    if((dp = opendir(dir)) == NULL) {
        //sprintf(buff,"cannot open directory: %s\n", dir);
        return 2;
    }
    
	//chdir(dir);

	char mode_str[11]={0};
	char group_id[10]={0};
	char  user_id[10] ={0};
	char linker[100]  ={0};
    char *lplinker = 0;

	strcat (buff,"\x04");
	buffSize --;

    while((entry = readdir(dp)) != NULL) {	
		int isDir = 0;
		sprintf (tmpBuff,"%.*s/%s",dirLen,dir,entry->d_name);
		//debug_msg ("lstat: %s \n",entry->d_name);
        lstat(tmpBuff,&statbuf);
		local=localtime(&statbuf.st_mtime);
		transfer_mode(statbuf.st_mode, mode_str); 
        uid_to_name(statbuf.st_uid,(char *)&user_id);
		gid_to_name(statbuf.st_gid,(char *)&group_id);		

        if(S_ISDIR(statbuf.st_mode)) {
            /* Found a directory, but ignore . and .. */
            if(strcmp(".",entry->d_name) == 0 || 
                strcmp("..",entry->d_name) == 0)
                continue;
			isDir = 1;		
        }

		if(S_ISLNK(statbuf.st_mode)){
			//strcpy (linker," -> ");
			lplinker = linker;
			//lplinker += 4;
			readlink(tmpBuff,linker,90);
			//debug_msg ("linker file: %s -> %s -> %s \n",tmpBuff,entry->d_name,lplinker);
		}
		sprintf(tmpBuff,"$k[]=%.*s""\x04""$d[]=\x01""%s\x03""%s\x03""%s\x03""%lu""\x03""%d-%02d-%02d %02d:%02d""\x03""%s""\x03""%s""\x03""%01d""\x02\x04",\
				5,entry->d_name,mode_str,user_id,group_id,statbuf.st_size,\
				1900 + local->tm_year,1 + local->tm_mon,local->tm_mday,local->tm_hour,local->tm_min,\
				entry->d_name,linker,isDir);
		
		if (buffSize > strlen(tmpBuff)){
            strcat(buff,tmpBuff);
			buffSize -= strlen(tmpBuff);
		}else{
			error = 5;
		    break;
		}     	
		
		if (lplinker){
			memset(linker,0,100);
			lplinker = 0;
		}
    }
    //chdir("..");
    closedir(dp);

	free (tmpBuff);

	return error;
}

//ret  0  ok -> finish
//    -1  ok -> continue
//    1-9 fail
int do_opt(char *dir,uint32_t *opt,char *input,char * checked,char* box){
	int ret = 0;
	char dirName[PATH_MAX] = {0};
	char objName[PATH_MAX] = {0};

	strcpy (dirName,dir);
	strcat (dirName,"/");
	
	strcpy (objName,dirName);
    strcat (objName,checked);

	strcat (dirName,input);

	int checkbox = strlen(box);

	char * res = 0;

	int isMove = 0;

	int mode; 

	switch (* opt){	
		case 0x4477656e:
			debug_msg ("New Directory \n");
			ret = mkdir(dirName,S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
			if (-1 == ret){
				ret = 1;
			}
			break;
        case 0x4677656e:
            debug_msg ("New File : %s\n",dirName);
		    FILE * handle = fopen(dirName,"a");
            if (handle){
			    fclose(handle);
			}else{
			    ret = 1;
			}
		    break;
        case 0x656c6564:  //delete
			if (0 == strlen(checked)){
			    ret = -1;
			}else{				
				if (0 == optDelete (objName)){
					ret = -1;
				}else{
					ret = 1;
				}
			}
		    break;
		case 0x776f6863: //chown
		    res = strstr(input,":");
			if (res){
				memset (res,0,1);
				res ++;                
				ret = optChown(objName,input,res,checkbox);
				if (0 == ret){
					ret = -1;
				}else{
					ret = 1;
				}
				res --;
				memset (res,0x3A,1); // ":"
			}else{
				debug_msg ("*** %s \n",input);
				ret = 2;				
			}
			break;
        case 0x6f6d6863: //chmod
		    mode = strtol(input, 0, 8);			
            ret = optChmod(objName,mode,checkbox);
		    debug_msg ("chmod %s set %s (%i) by child: %i ret: %i\n",objName,input,mode,checkbox,ret);
            if (0 == ret){
				ret = -1;
            }else{
				ret = 1;
			}
			break;
        case 0x616e6572: //rename
		    ret = rename (objName,dirName);
		    debug_msg ("rename %s to %s , ret: %i\n",dirName,objName,ret);
		    break;
        case 0x65766f6d: //move
		    isMove = 1;
        case 0x79706f63: //copy
			res = realpath(input,dirName);
		    if (res){				
				if (0 == strlen(checked)){
					ret = - 1;
				}else{			
					if (0 == optCopyMove (objName,dirName,checked,checkbox,isMove)){
						ret = - 1;
					}else{
						ret = 1;
					}
				}
		    }else{
			    ret = 2;
			}			
			break;
    }

	return ret;
}

int exec(const char *buff){
	REQUEST_HEADER * Buf = (REQUEST_HEADER *)buff;

    debug_msg ("Mod.exec: request len: %i contents: %s  addr: %p \n",Buf->RDataSize,(char *)Buf->RData,Buf->RData);
	debug_msg ("Mod.exec: request ID: %.*s \n",32,Buf->Tid);
	debug_msg ("Mod.exec: ExecFunc: %p \n",Buf->ExecCallFunc);
	debug_msg ("Mod.exec: cJSON_GetObjectItem: %p \n",Buf->cJSON_GetObjectItem);
	debug_msg ("Mod.exec: lpJsonRoot: %p \n",Buf->lpJsonRoot);

    char * basedir = 0;
	char * opt     = 0;
	char * input   = 0;
	char * box     = 0;
	char * editContents = 0;
	cJSON * jsonRoot = 0;
	
    cJSON * (* cJSON_GetObjectItem) (cJSON *object,const char *string);
    cJSON_GetObjectItem = Buf->cJSON_GetObjectItem;
    if (Buf->lpJsonRoot){		 		
		jsonRoot = Buf->lpJsonRoot;

		cJSON * data = cJSON_GetObjectItem(jsonRoot,"basedir");
		if ((data)&&(data->valuestring)){
			basedir = data->valuestring;
		}		
		data = cJSON_GetObjectItem(jsonRoot,"opt");
		if ((data) && (data->valuestring)){
			opt = data->valuestring;
		}		
		data = cJSON_GetObjectItem(jsonRoot,"input");
		if ((data) && (data->valuestring)){
			input = data->valuestring;
		}	
		data = cJSON_GetObjectItem(jsonRoot,"box");
		if ((data) && (data->valuestring)){
			box = data->valuestring;
		}
		data = cJSON_GetObjectItem(jsonRoot,"c");
		if ((data) && (data->valuestring)){
			editContents = data->valuestring;
		}
    }
    if (basedir){
	    debug_msg ("nature path: %s \n",basedir);
	}
	
    if (opt){
	    debug_msg ("current opt: %s \n",opt);
	}
	
	if (input){
	    debug_msg ("current input: %s \n",input);
	}
	
	int bufferSize = 1024  * 1024;
	char *buffer   = 0;

	char *lpWarning = 0;

	int stream_output = 0;

	int print_dir = 1;

    while (bufferSize){
        buffer = malloc(bufferSize);
	    int  remained = bufferSize;

		if (buffer){
			memset(buffer, 0, bufferSize);
			strcpy (buffer,"$w=0""\x04""$basedir=");
			lpWarning = buffer + 3;
			char *lpBaseDir = buffer + sizeof("$w=0""\x04""$basedir=")-1;
			if (basedir){
				char *res = realpath(basedir,lpBaseDir);
				debug_msg ("real path: %s \n",buffer);
				if (res) {
					remained -= strlen(buffer); 
					int r = 0;
					if (opt){				
						if (*((uint32_t *)opt)== 0x6e776f64){       //download
							stream_output = 1;
							print_dir = 0;
							char objName[PATH_MAX] = {0};
							strcpy (objName,lpBaseDir);
							strcat (objName,"/");
							strcat (objName,input);
			                debug_msg ("download: %s\n",objName);
							if (0 == access(objName,0)){
								debug_msg ("download file exists \n");
								FILE * fp = fopen(objName,"rb");
								if (fp){
									fseek(fp, 0L, SEEK_END);   
									int fileSize = ftell(fp);
									fseek(fp, 0L, SEEK_SET);
									if (fileSize > 0){									
										debug_msg ("open file success: size:%i \n",fileSize);
										int (* responseStreamSender) (REQUEST_HEADER * ,size_t, int,char*); 
										responseStreamSender = Buf->ResponseStreamSender;
										size_t remainSize = fileSize;
										while (remainSize){
											size_t c_sendSize = remainSize;
											if (c_sendSize > bufferSize){
												c_sendSize = bufferSize;
											}
											remainSize -= c_sendSize;
											if (c_sendSize != fread(buffer, sizeof(char),c_sendSize,fp)){
												break;
											}
											if (1 != responseStreamSender(Buf,fileSize,c_sendSize,buffer)){
												break;
											}	
										}
									}
									fclose(fp);
								}								
							}
						}else if (*((uint32_t *)opt)== 0x74696465){  //edit
						    print_dir = 0;
							if (editContents){
								savetext (lpBaseDir,input,editContents,buffer);
							}else{
								readtext (lpBaseDir,input,buffer,bufferSize);
							}
							break;
						}else if (*((uint32_t *)opt)== 0x6F6C7075){ //upload File
						    cJSON * uploadArray = cJSON_GetObjectItem(jsonRoot,"_FILES");
							cJSON * uploadTmp = 0;
                            if (uploadArray){
                                 uploadArray = uploadArray->child;
								 while (uploadArray){                                     
                                     
									 uploadTmp = cJSON_GetObjectItem(uploadArray,"size");
                                     if (uploadTmp->valueint){
										 debug_msg ("upload size: %i \n",uploadTmp->valueint);
										 int uploadSize = uploadTmp->valueint;
                                         
										 cJSON * uploadName = cJSON_GetObjectItem(uploadArray,"name");
										 cJSON * uploadIndx = cJSON_GetObjectItem(uploadArray,"index");
										 
										 if ((uploadName->valuestring) && (uploadIndx)){
											 r = do_upload(Buf->ReadStreamFunc,Buf,lpBaseDir,uploadName->valuestring,uploadIndx->valueint,uploadSize,strlen(box));
										     if (0 != r){ //fail upload
											     r += 0x30;
												 memset (lpWarning,r,1);
										         break;
											 }
										 }
									 }									 
									 uploadArray = uploadArray->next;	
								 }									
                            }							
						}else{
							if ((0 == input) || (0 == box)){
								memset (lpWarning,0x32,1);
							}else{
								cJSON * checkedArray = cJSON_GetObjectItem(jsonRoot,"checked");
								if (checkedArray){
									debug_msg ("next: %p , prev: %p ,child: %p ,type: %i ,valuestring: %p ,valueint: %i \n",\
										checkedArray->next,checkedArray->prev,checkedArray->child,checkedArray->type,checkedArray->valuestring,checkedArray->valueint);
									if (checkedArray->child){
										checkedArray = checkedArray->child;
										while (checkedArray->valuestring){										
											r = do_opt(lpBaseDir,(uint32_t *)opt,input,checkedArray->valuestring,box); 
											if (0 == r){           //结束
												break;
											}else if (-1 == r){     //继续
												if (checkedArray->next){
													checkedArray = checkedArray->next;
												}else{
													break;
												}													
											}else{    //出错
												r += 0x30;
												memset (lpWarning,r,1);
												break;
											}															
										}				
									}
								}									
							}
						}
						//debug_msg ("opt ret: %s \n",buffer);
					}
				}else{
					strcpy (buffer,"$e=3\x04");
					break;
				}
			}else{ 
				strcat(buffer,"/");
			}		
			if (1 == print_dir){			
				int r = printdir(lpBaseDir,buffer,remained);
				if (1 == r){
					strcat (buffer,"\x04""$e=1\x04");
					break;
				}else if (2 == r){
					strcat (buffer,"\x04""$e=2\x04");			
					break;
				}else if (5 == r){
					strcpy (buffer,"$e=5\x04");
					break;
				}
			}
		}
		bufferSize = 0;
	}	
	

	//printf("ready: %i %s \n",bufferLen,buffer);


    if (0 == stream_output){	
		int (* responseSender) (REQUEST_HEADER * , int,char*); 
		responseSender = Buf->ResponseSender;
        if (buffer){
			responseSender(Buf,strlen(buffer),buffer);
		}else{
			responseSender(Buf,5,"$e=4\x04");
		}
	}

	if (buffer){
		free(buffer);
	}

	return 0;
}
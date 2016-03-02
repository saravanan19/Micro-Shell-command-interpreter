/*
 * Created by Saravanan Balasubramanian
 * Date: 02/20/2016
 * File contains functions to handle in built and normal commands
 * using file redirection, piping and signal handling
 */

#include "cmd_handler.h"

#include <bits/fcntl-linux.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parse.h"

/*
 * Handle pipe function Handles pipes one by one using a recursive call
 *
 */
void handlePipe(Pipe p)
{

  if ( p == NULL ){
    return;
  }
	handleCmd(p->head);
    handlePipe(p->next);
}

/*
 * Handle Command function interprets the command and execute the command
 *
 */

int handleCmd(Cmd c)
{
  int backup_in = dup(fileno(stdin));
  int backup_out = dup(fileno(stdout));
  int backup_err = dup(fileno(stderr));
  int isPipe=0;

  if( !strcmp(c->args[0], "end") ){
 		  exit(0);
   }

  if ( c ) {

	  setIOredirections(c);

	 if(c->out==Tpipe){
		isPipe =1;
		forkProcesses(c,-1);

	 }else{
		 forkProcesses(c,-1);
	 }

  }
	dup2(backup_in, fileno(stdin));
	dup2(backup_out,fileno(stdout));
	dup2(backup_err,fileno(stderr));
	close(backup_in);
	close(backup_out);
	close(backup_err);
  return isPipe;
}

/*
 * Fork Processes is used to create child processes to handle commands.
 * It is called recursively to execute multiple commands joined by | (pipes).
 *
 */
void forkProcesses(Cmd c, int in){

	int arr[2];
	if(c->out == Tpipe || c->out==TpipeErr)
		pipe(arr);
	int status;
	pid_t pid;
	int builtInCmd =isBuildInCommnad(c);
	if(builtInCmd && c->out!=Tpipe && c->out!=TpipeErr){
		pid=getpid();
	}else{
	  pid=fork();
	}

	switch(pid){

	case 0:
		  signal(SIGINT, SIG_DFL);
		  signal(SIGQUIT,signal_handler);
		 setIOredirections(c);
		if(c->out==Tpipe && (c->in==Tpipe||c->in==TpipeErr)){
			if(-1==dup2(arr[1],1)){
				printf("dup2 failure, cout\n");
			}
			if(-1==dup2(in,0)){
				printf("dup2 failure, cin\n");
			}
			close(arr[0]);
			close(arr[1]);
		}else if(c->out==TpipeErr && (c->in==Tpipe||c->in==TpipeErr)){
			if(-1==dup2(arr[1],1)){
				printf("dup2 failure, cout\n");
			}
			if(-1==dup2(arr[1],2)){
				printf("dup2 failure, cerr\n");
			}
			if(-1==dup2(in,0)){
				printf("dup2 failure, cin\n");
			}
			close(arr[0]);
			close(arr[1]);
		}
		else if(c->out==TpipeErr){
					if(-1==dup2(arr[1],1)){
						printf("dup2 failure, cout\n");
					}
					if(-1==dup2(arr[1],2)){
						printf("dup2 failure, cerr\n");
					}
					close(arr[0]);
					close(arr[1]);
		}else if(c->out==Tpipe){
			if(-1==dup2(arr[1],1)){
				printf("dup2 failure, cout1\n");
			}

			close(arr[0]);
			close(arr[1]);
		}
		else if(c->in==Tpipe || c->in==TpipeErr){
					if(-1==dup2(in,0)){
						printf("dup2 failure, cin\n");
					}
					close(in);
				}
		if(builtInCmd){
			execbuiltin(c,builtInCmd);
			exit(EXIT_SUCCESS);
		}else if(-1==execvp(c->args[0], c->args)){
			printf("command not found\n");
			exit(1);
		}
		break;



	case -1:
		 printf("Error creating process\n");
		 exit(1);
		 break;



	default:
			if(builtInCmd && c->out!=Tpipe && c->out!=TpipeErr){
				if(c->in==Tpipe || c->in ==TpipeErr){
					if(-1==dup2(in,0)){
						printf("dup2 failure, cin\n");
					}
					close(in);
				}
				setIOredirections(c);
				if(-1==execbuiltin(c,builtInCmd)){
					return;
				}
			}else if ( c->out==Tpipe || c->out==TpipeErr){

				 waitpid(pid,&status,0);
				 close(arr[1]);
				 forkProcesses(c->next,arr[0]);
			 }
		  break;
	  }



	close(arr[0]);
	close(arr[1]);
	close(in);
	waitpid(pid,&status,0);
}

/*
 * Execute built in executed built in commands, commands that are implemented as
 * a part of shell.
 *
 */

int execbuiltin(Cmd c, int builtInCmd){

	int ret=0;
	switch(builtInCmd){
	case 1:
		if(c->nargs==1){
			char* home = getenv("HOME");
			if(-1==changeDirectory(home))
						return -1;
		}
		else{
			if(-1==changeDirectory(c->args[1]))
				return -1;
		}
		break;
	case 2:
		if(c->nargs==1){
			printf("\n");
			break;
		}
		handleecho(c);
		break;
	case 3:
		exit(EXIT_SUCCESS);
		break;
	case 4:
		ret =myNice(c);
		return ret==-1?-1:1;
		break;
	case 5:
		getpwd();
		break;
	case 6:
		if(setenv1(c)==-1)
			return -1;
		break;
	case 7:
		if(unsetenv1(c)==-1)
			return -1;
		break;
	case 8:
		if(myWhere(c)==-1)
			return -1;
		break;
	case 9:
			if(killCmd(c)==-1)
				return -1;
			break;
	case 10:
			jobsCmd();
			break;
	}
	return 0;
}

/*
 * Every command joined by pipes can have various IO redirections and are
 * handled here.
 *
 */

void setIOredirections(Cmd c){
	int in, out;
	  if ( c->in == Tin ){
		      in = open(c->infile, O_RDWR, 0600);
			if (-1 == in) { printf("Error opening file\n"); return ; }
			if (-1 == dup2(in, fileno(stdin)))
			{
				perror("cannot redirect stdin\n"); return ;
			}
			fflush(stdin);
			close(in);
	    }


	    if ( c->out != Tnil )
	      switch ( c->out ) {
	      case Tout:
	    		    out = open(c->outfile, O_RDWR|O_CREAT, 0777);
	    		    if (-1 == out) {  printf("Error opening file\n"); return ; }
	    		    if (-1 == dup2(out, fileno(stdout)))
	    		    {
	    		    	perror("cannot redirect stdout\n"); return ;
	    		    }
	    			fflush(stdout);
	    			close(out);
	    	  	  break;

	    case Tapp:
			out = open(c->outfile, O_RDWR|O_CREAT|O_APPEND, 0777);
			if (-1 == out) {  printf("Error opening file\n"); return ; }
			if (-1 == dup2(out, fileno(stdout)))
			{
				perror("cannot redirect stdout\n"); return ;
			}
			fflush(stdout);
			close(out);
			break;
	      case ToutErr: // check test case
	    	    out = open(c->outfile, O_RDWR|O_CREAT, 0777);
			if (-1 == out) {  printf("Error opening file\n"); return ; }
			if (-1 == dup2(out, fileno(stdout)))
			{
				perror("cannot redirect stdout\n"); return ;
			}
			if (-1 == dup2(out, fileno(stderr)))
			{
				perror("cannot redirect stderr\n"); return ;
			}

			fflush(stdout);
			fflush(stderr);
			close(out);
		break;
	      case TappErr:
	    	  	     out = open(c->outfile, O_RDWR|O_CREAT|O_APPEND, 0777);
	    	  		if (-1 == out) {  perror("Error opening file\n"); return ; }
	    	  		if (-1 == dup2(out, fileno(stdout)))
	    	  		{
	    	  			perror("cannot redirect stdout\n"); return ;
	    	  		}
	    	  		if (-1 == dup2(out, fileno(stderr)))
	    	  		{
	    	  			perror("cannot redirect stderr\n"); return  ;
	    	  		}
	    			fflush(stdout);
	    			fflush(stderr);
	    			close(out);
		 break;
	      case Tpipe:
	    	  break;
	      case TpipeErr:
	    	  break;

	      default:
			fprintf(stderr, "Shouldn't get here\n");
			exit(-1);
	      }
}

/*
 * Function is used to check if a command is a built in command.
 * Built in commands are -> cd, echo, logout, nice, pwd,setenv, unsetenv, where
 */

int isBuildInCommnad(Cmd c){


	if(!strcmp(c->args[0],"cd"))
		return 1;
	else if(!strcmp(c->args[0],"echo"))
		return 2;
	else if(!strcmp(c->args[0],"logout"))
		return 3;
	else if(!strcmp(c->args[0],"nice"))
		return 4;
	else if(!strcmp(c->args[0],"pwd"))
		return 5;
	else if(!strcmp(c->args[0],"setenv"))
		return 6;
	else if(!strcmp(c->args[0],"unsetenv"))
		return 7;
	else if(!strcmp(c->args[0],"where"))
		return 8;
	else if(!strcmp(c->args[0],"kill"))
			return 9;
	else if(!strcmp(c->args[0],"jobs"))
			return 10;
	else
		return 0;
}

/*
 * Function used to switch to the directory mentioned in the path string
 */
int changeDirectory(char* path){

	int res = chdir(path);

	return res;
}

/*
 *  Helps setting up Environmental variable
 */

int setenv1(Cmd c)
{
	if(c->nargs==1){
		 extern char **environ;
		 char** temp = environ;
		 int i=0;
		 while(temp[i]!=NULL){
			 printf("%s\n",temp[i]);
			 i++;
		 }
		 return 0;
	}
	char* name = c->args[1];
	char* value;
	if(c->nargs==3)
		value = c->args[2];
    char *newStr;
    if(c->nargs==3)
    	newStr = (char*)malloc(strlen(name) + strlen(value) + 2);
    else
    	newStr = (char*)malloc(strlen(name) + 2);
    if (newStr == NULL)
        return -1;
    strcpy(newStr, name);
    strcat(newStr, "=");
    if(c->nargs==3)
    	strcat(newStr, value);
    else
    	strcat(newStr, "");
    int res=putenv(newStr);

    return (res != 0) ? -1 : 0;
}
/*
 * Helps unsetting the Environmental Variable
 */
int unsetenv1(Cmd c)
{
	if(c->nargs!=2)
		{
			return -1;
		}
		char* name = c->args[1];
	    char *newStr;

	    newStr = (char*)malloc(strlen(name) +1);
	    if (newStr == NULL)
	        return -1;

	    strcpy(newStr, name);
	    int res=putenv(newStr);

	    return (res != 0) ? -1 : 0;
}

/*
 * Prints the current working directory
 */

void getpwd(){

	char* buffer;
	buffer= malloc(PATH_MAX);

	getcwd(buffer,PATH_MAX);
	printf("%s\n",buffer);
	return;
}

/*
 * Handles Echo command. Both Strings and Environmental variable are printed
 */
void handleecho(Cmd c){

	int i=1;
	for(;i<c->nargs;i++){
		if(c->args[i][0]=='$' && c->args[i][1]!='\0'){
			char* str = c->args[i];
			printf("%s ",getenv(str+1));
		}else{
			printf("%s ",c->args[i]);
		}
	}
	printf("\n");
}

/*
 * Nice built in command is handled here
 */
int myNice(Cmd c){

	int which = PRIO_PROCESS;
	pid_t pid = getpid();
	int priority=4;
	int found=-1;
	if(c->nargs==1){
		setpriority(which, pid, priority);
		return 1;
	}
	else if (c->nargs>=2){
		char* temp = malloc(sizeof(c->args[1]));
		strcpy(temp,c->args[1]);
		int isnum=1;
		int isNeg=0;
		if(temp[0]=='-')
			isNeg=1;
		int i=0;
		if(isNeg)
			i=1;
		for(;i<strlen(temp);i++){
			if(temp[i]-'0'< 0 || temp[i]-'0'> 10){
				isnum=0;
				break;
			}
		}
		if(isnum)
			priority = strtol(c->args[1], (char **)NULL, 10);
		else
			priority = 4;

		//printf("priority: %d\n",priority);
		if(isnum){
			if(c->nargs>=3){
				int status;
				found = cmdExists(c->args[2]);
				if(found){
					pid_t pid = fork();
					switch(pid){
					case 0:
						signal(SIGINT, SIG_DFL);
						setpriority(which,getpid(),priority);
						if(-1==execvp(c->args[2],c->args+2)){
							printf("‘command not found\n");
							exit(EXIT_FAILURE);
						}
					break;
					case -1:
						return -1;

					default:
						waitpid(pid,&status,0);
					 return 1;
					}
				}
			  }else{
					setpriority(which,getpid(),priority);
			}
		}
		else{
			found = cmdExists(c->args[1]);
			int status;
			if(found){
				pid_t pid = fork();
				switch(pid){
				case 0:
					signal(SIGINT, SIG_DFL);
					setpriority(which,getpid(),priority);
					if(-1==execvp(c->args[1],c->args+1)){
						printf("‘command not found\n");
						exit(EXIT_FAILURE);
					}
				break;
				case -1:
					return -1;

				default:
					waitpid(pid,&status,0);
				 return 1;
				}
			}
			else
				return -1;
		}

	}


	return -1;

}

/*
 * Where builtin command is handled here
 */
int myWhere(Cmd c){

	if(c->nargs<2 || c->nargs >2)
		return -1;
	if(checkBuiltIn(c->args[1]))
		printf("%s is a builtin command\n",c->args[1]);


	char* path = malloc(4096);
	strcpy(path,getenv("PATH"));
	int len = strlen(path);
	int i=0;
	int count=0;
	int start=0;

	for(;i<=len;i++){
		if(path[i]==':' || path[i]=='\0'){
			char* temp = malloc(i-start+2);
			strncpy(temp,path+start,count);
			temp[i-start]='\0';
			if(fileExist(c->args[1],temp)){
				printf("%s/%s\n",temp,c->args[1]);
			}
			count=0;
			start=i+1;
			free(temp);
		}else{
			count++;
		}
	}
	free(path);
	return 0;
}

/*
 * Utility function helps to check if a file is present in a particular path
 */

int fileExist(char* file, char* path){
	char* f = malloc(strlen(file)+strlen(path)+2);
	strcpy(f,path);
	strcat(f,"/");
	strcat(f,file);
	FILE* fp = fopen(f,"r");
	if(fp!=NULL){
		fclose(fp);
		return 1;
	}
	return 0;
}


/*
 * Utility function to check if a string is avalid builtin command
 */

int checkBuiltIn(char* cmd){
	if(!strcmp(cmd,"cd") || !strcmp(cmd,"echo")|| !strcmp(cmd,"nice")
			|| !strcmp(cmd,"where") || !strcmp(cmd,"setenv") || !strcmp(cmd,"unsetenv")
			|| !strcmp(cmd,"pwd") || !strcmp(cmd,"logout"))
		return 1;

	return 0;
}


/*
 * Utility function checks all the paths in the PATH environmental variable to find
 * if the command is a valid command
 */

int cmdExists(char* cmd){

	if(checkBuiltIn(cmd))
			return 1;

		char* path = malloc(4096);
		strcpy(path,getenv("PATH"));
		int len = strlen(path);
		int i=0;
		int count=0;
		int start=0;

		for(;i<=len;i++){
			if(path[i]==':' || path[i]=='\0'){
				char* temp = malloc(i-start+2);
				strncpy(temp,path+start,count);
				temp[i-start]='\0';
				if(fileExist(cmd,temp))
					return 1;
				count=0;
				start=i+1;
				free(temp);
			}else{
				count++;
			}
		}
		free(path);
		return 0;
}

int killCmd(Cmd c){
	return kill(atoi(c->args[1]+1), SIGKILL);
}

void jobsCmd(){

	/*
	 * Code referred from stackoverflow
	 * http://stackoverflow.com/questions/1723002/how-to-list-all-subdirectories-in-a-given-directory-in-c
	 */

    struct dirent *subdir = NULL;
    char path[6] = "/proc";
    DIR *dir = opendir(path);
    int path_len = strlen(path);

    while ((subdir = readdir(dir)) != NULL)
    {

        struct stat fstat;
        char full_name[PATH_MAX + 1];
        strcpy(full_name, path);
        if (full_name[path_len - 1] != '/')
            strcat(full_name, "/");
        strcat(full_name, subdir->d_name);

        if (stat(full_name, &fstat) < 0)
            continue;
        if (S_ISDIR(fstat.st_mode))
        {
        	int isPid=1;
        	isPid = strtol(subdir->d_name, (char **)NULL, 10);
        	if(isPid)
        		printf("%s\n", subdir->d_name);
        }
    }
    closedir(dir);
    return ;

}

void signal_handler(){
	printf("Core Dumped\n");
	exit(EXIT_SUCCESS);
}

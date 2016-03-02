/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *  Modified By ......: Saravanan Balasubramanian
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


static int parentpid;

static void prCmd(Cmd c)
{
  int i;

  if ( c ) {
    printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    if ( c->in == Tin )
      printf("<(%s) ", c->infile);
    if ( c->out != Tnil )
      switch ( c->out ) {
      case Tout:
	printf(">(%s) ", c->outfile);
	break;
      case Tapp:
	printf(">>(%s) ", c->outfile);
	break;
      case ToutErr:
	printf(">&(%s) ", c->outfile);
	break;
      case TappErr:
	printf(">>&(%s) ", c->outfile);
	break;
      case Tpipe:
	printf("| ");
	break;
      case TpipeErr:
	printf("|& ");
	break;
      default:
	fprintf(stderr, "Shouldn't get here\n");
	exit(-1);
      }

    if ( c->nargs > 1 ) {
      printf("[");
      for ( i = 1; c->args[i] != NULL; i++ )
	printf("%d:%s,", i, c->args[i]);
      printf("\b]");
    }
    putchar('\n');
    // this driver understands one command
    if ( !strcmp(c->args[0], "end") )
      exit(0);
  }
}

static void prPipe(Pipe p)
{
  int i = 0;
  Cmd c;

  if ( p == NULL )
    return;

  printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  for ( c = p->head; c != NULL; c = c->next ) {
    printf("  Cmd #%d: ", ++i);
    prCmd(c);
  }
  printf("End pipe\n");
  prPipe(p->next);
}

static int getNumLines(char* fpath){
		FILE * fp;
	    char * cont = NULL;
	    int count=0;
	    ssize_t read;
	    size_t len = 0;

	    fp = fopen(fpath, "r");
	    if (fp == NULL)
	        return 0;

	    while (-1 != (read = getline(&cont, &len, fp)) ) {
	    	int isbackslash=0;
	    	int i=0;
	    	for(i=strlen(cont)-2;i>=0;i--){
	    		if (cont[i]==' ')
	    			continue;
	    		else if(cont[i]=='\\'){
	    			isbackslash=1;
	    			break;
	    		}else{
	    			isbackslash=0;
	    			break;
	    		}
	    	}
	    	if(!isbackslash)
	    		count++;
	    }

	    if (cont)
	        free(cont);
	    fclose(fp);

	    return count;
}




int main(int argc, char *argv[])
{
  Pipe p;
  int fp;
  parentpid = getpid();
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTERM,SIG_IGN);
 // signal(SIGTSTP, signal_stp_handler);
  int backup_in = dup(fileno(stdin));
  char* host = malloc(50);
  gethostname(host, 50);
  if(host==NULL)
	  host = "ush";
  char* filePath = malloc(4096);
  char filename[8] = "/.ushrc";
  strcpy(filePath,getenv("HOME")) ;
  strcat(filePath,filename);

  int numLines = getNumLines(filePath);
  fp = open(filePath, O_RDWR, 0600);
  dup2(fp, 0);
  while(numLines>0)
  {
	  p = parse();
	  handlePipe(p);
	  freePipe(p);
	  numLines--;
  }
  dup2(backup_in, 0);
  close(fp);
  while ( 1 ) {
     printf("%s%% ", host);
     fflush(stdout);
     p = parse();
     handlePipe(p);
     freePipe(p);
  }
}
/*
 * To do: Test Stderr redirection
 *
 */
/*........................ end of main.c ....................................*/

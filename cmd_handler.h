/*
 * Created by Saravanan Balasubramanian
 * Date: 02/20/2016
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parse.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/resource.h>
#include <limits.h>
#include <signal.h>
#include <dirent.h>

int handleCmd(Cmd c);
void handlePipe(Pipe p);
int redirectOut(Cmd c);
void forkProcesses(Cmd c, int in);
void setIOredirections(Cmd c);
int isBuildInCommnad(Cmd c);
int setenv1(Cmd c);
int unsetenv1(Cmd c);
int changeDirectory(char* path);
void getpwd();
int myNice(Cmd c);
void handleecho(Cmd c);
int execbuiltin(Cmd c, int builtInCmd);
int myWhere(Cmd c);
int cmdExists(char* cmd);
int fileExist(char* file, char* path);
int checkBuiltIn(char* cmd);
int killCmd(Cmd c);
void jobsCmd();
void signal_handler();

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <ios>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>


using namespace std;

const int stdoutfd(dup(fileno(stdout)));

void pipeCommands(vector<vector<string>>,int,bool,vector<string>);
vector<char *> mk_cstrvec(vector<string> & strvec);
void dl_cstrvec(vector<char *> & cstrvec);
void nice_exec(vector<string> args);
void help();
void change_prompt();
inline void nope_out(const string & sc_name);
void defaultSignal();
void defaultIO();
void redirc(string file);
void handleIO(vector<string>);
int argCheck(string);

struct job{
	string pstatus;
	string command;
	int jid;
	pid_t pid;
};

vector<job> jobList;
//{}
//pgid
//exit status
//head
//

int main(int argc, char * argv[]) {
  //defaultSignal();
  while (true){
    //signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
	
    //signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    change_prompt();
    string input = "", arg;
    stringstream ss;  
    char buffer[1];
    int n;
    vector<string> redirects;
    vector<string> process;
    vector<vector<string>> commands;
    //bool bg = false;
    bool redirIO = false;
    while((n = read(STDIN_FILENO,buffer,1)) > 0){
      if(buffer[0] == '\n')
	break;
      input += buffer[0];
    }
    if(input.length() != 0){
      ss << input;
      int pipes = 0;
      while (ss >> arg) {//symbol checking
	if(arg == "|"){
	  commands.push_back(process);
	  pipes++;
	  process.clear();
	}
		//output redirections
		else if(argCheck(arg) == 2)
		{
			do{
				if(argCheck(arg) != 2) break;
				redirIO = true;
				string out = "";
				redirects.push_back(arg);
				ss >> arg;
				redirects.push_back(arg);
				//continue;
			} while(ss >> arg);
			break;
		}
/**
		else if(arg == ">>")
		{
			redirIO = true;
			string out = "";
			process.push_back(arg);
			ss >> out;
			process.push_back(out);
		}

		//error redirections
		else if(arg == "e>")
		{
			//err = data.at(i+1);
			redirIO = true;
			string out = "";
			redirects.push_back(arg);
			ss >> out;
			redirects.push_back(out);
			continue;
		}

		else if(arg == "e>>")
		{
			//err = data.at(i+1);
			//err += " (append)";
		}

		//input redirections
		else if(arg == "<")
		{
			//in = data.at(i+1);
		}*/
	//else if (arg == "&")
	//	bg = true;
	//if arg == any io redirection symbols change bools?
	else
	  process.push_back(arg);
      }
      commands.push_back(process);
      if(commands.size() != 0){
	if(commands[0][0] == "cd"){
	  if(commands[0].size() == 1){
	    char * homedir = getenv("HOME");
	    chdir(homedir);
	  }
	  else if(commands[0].size() == 2) {
	    if(chdir(commands[0][1].c_str()) < 0)
	      perror(commands[0][1].c_str());
	  }
	  else 
	    cout << "Too many arguments" << endl;
	}
	else if(commands[0][0] == "exit"){
	  exit(getpid());
	}
	else if(commands[0][0] == "help"){
	  help();
	}
	else{
	  pipeCommands(commands, pipes, redirIO, redirects);
	}
      }
    }
  }
}

void nice_exec(vector<string> strargs) {
  vector<char *> cstrargs = mk_cstrvec(strargs);
  if((execvp(cstrargs.at(0), &cstrargs.at(0))) == -1){
  perror("execvp");
 }
  dl_cstrvec(cstrargs);
  exit(EXIT_FAILURE);
}

vector<char *> mk_cstrvec(vector<string> & strvec) {
  vector<char *> cstrvec;
  for (unsigned int i = 0; i < strvec.size(); ++i) {
    cstrvec.push_back(new char [strvec.at(i).size() + 1]);
    strcpy(cstrvec.at(i), strvec.at(i).c_str());
  } // for
  cstrvec.push_back(nullptr);
  return cstrvec;
} // mk_cstrvec

void dl_cstrvec(vector<char *> & cstrvec) {
  for (unsigned int i = 0; i < cstrvec.size(); ++i) {
    delete[] cstrvec.at(i);
  } // for
}

void help(){
  cout.setf(ios::unitbuf);
  cout << "Available Commands: \n"
       << "bg JID             : Resume the stopped job JID in the background, as if it had been started with &.\n"
       << "cd [PATH]          : Change the current directory to PATH. The environmental variable HOME is the default PATH.\n"
       << "exit [N]           : Cause the shell to exit with a status of N. If N is omitted, the exit status is that of the last job executed.\n"
       << "export NAME[=WORD] : NAME is automatically included in the environment of subsequently executed jobs.\n"
       << "fg JID             : Resume job JID in the foreground, and make it the current job.\n"
       << "help               : Display helpful information about builtin commands.\n"
       << "jobs               : List current jobs."<< endl;
}

//update the prompt
void change_prompt(){
  struct passwd *pw = getpwuid(getuid());
  string homedir = pw->pw_dir;
  char pwd [PATH_MAX];
  getcwd(pwd,PATH_MAX + 1);
  char pathbuf [1024];
  string path = pwd;
  string temp = path.substr(0,homedir.size());
  if(temp == homedir && path != homedir){
    path.erase(0,homedir.size());
    path = "~" + path;
  }
  strcpy(pathbuf,"1730sh:");
  strcat(pathbuf, path.c_str());
  strcat(pathbuf, "$ ");

 // defaultIO(); 
  cout.setf(ios::unitbuf);
  string promptStr = string(pathbuf);
  //printf(promptStr.c_str());
  cout << promptStr;

/**
  write(STDOUT_FILENO,pathbuf,1024);
  for(int i = 0; i < PATH_MAX; i++)
    pathbuf[i] ='\0'; */
}

//handle pipes and commands
void pipeCommands( vector<vector<string>> vectors, int numPipes, bool redirIO, vector<string> redirects){

  int length = 2*numPipes;
  int * pipefd = new int[length];
  pid_t pid;
  pid_t pgid;

  for(int i =0; i < numPipes; i++){
    if(pipe(pipefd + i*2) == -1) nope_out("pipe");  
  }
  
  for(unsigned int i =0;i<vectors.size();i++){
    pid = fork();
    cout.setf(ios::unitbuf);
    if(pid == -1) nope_out("fork");
    else if(pid == 0){
      defaultSignal();
      if(i!=0){ // if its not first command, dup the input
	if(dup2(pipefd[(i-1)*2],0) == -1) nope_out("dup2");
      }
      if(i!=(vectors.size()-1)){ // if its not last command, dup the output
	if(dup2(pipefd[(i*2)+1],1) == -1) nope_out("dup2");
      }
//      if(i == 0){
//	pgid = pid;
  //    }
/**
      setpgid(pid, pgid);
	struct job jtemp;
	jtemp.jid = pgid;
	jtemp.pid = pid;
	jtemp.command = vectors[i][0];
	jobList.push_back(jtemp);
*/
      //close pipes
      for(int j = 0;j<numPipes*2;j++){
	close(pipefd[j]);
      }

      if((i == vectors.size()-1) && redirIO){
	handleIO(redirects);
      }
/**
		if(redirIO) {
			handleIO(vectors[0]);
		}
	
	for(int c = 0; c < vectors[i].size(); i++){
		if(argCheck(vectors[i][c]) == 2){
			vectors[i].erase(vectors[i].begin()+c, vectors[i].begin()+c+1);
		}
	}*/

      nice_exec(vectors[i]);
    } else { //have the parent wait for status changes
	waitpid(pid, nullptr, 0);
/**	
  pid_t wpid;
  int pstatus;
 // defaultIO();
  
  cout.setf(ios::unitbuf);
  while((wpid = waitpid(pid, &pstatus, WNOHANG | WUNTRACED | WCONTINUED)) > 0){
	if(WIFEXITED(pstatus)){
		cout << wpid << " Exited (" << WEXITSTATUS(pstatus) << ")\n";
	//	change_prompt();
	}
	else if(WIFSTOPPED(pstatus)){
		int stopsig = WSTOPSIG(pstatus);
		cout << wpid << " Stopped (" << strsignal(stopsig) << ")\n";
	//	change_prompt();
	}
	else if(WIFCONTINUED(pstatus)){
		cout << wpid << " Continued \n";
		//change_prompt();
	}
  }
*/
	}
  }

  for(int i =0;i<2*numPipes;i++){
    close(pipefd[i]);
  }
  
  //for(int i =0;i<numPipes+1;i++){
   // waitpid(pid,nullptr,0);
  //}

    
  delete [] pipefd;
}

void handleIO(vector<string> arg){
	
  for(unsigned int i =0;i<arg.size();i++){
		if(arg[i] == ">")
		{
			int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			dup2(newfd, STDOUT_FILENO);
			close(newfd);
			continue;
			//out += " (truncate)";
		}

		else if(arg[i] == ">>")
		{
			int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			dup2(newfd, STDOUT_FILENO);
			close(newfd);
			continue;
			//out += " (append)";
		}

		//error redirections
		else if(arg[i] == "e>")
		{
			//err = data.at(i+1);
			//err = data.at(i+1);
			int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			dup2(newfd, STDERR_FILENO);
			close(newfd);
			continue;
		}

		else if(arg[i] == "e>>")
		{
			//err = data.at(i+1);
			int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			dup2(newfd, STDERR_FILENO);
			close(newfd);
			continue;
			//err += " (append)";
		}

		//input redirections
		else if(arg[i] == "<")
		{
			//in = data.at(i+1);
			int newfd = open(arg[++i].c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if(newfd < 0) nope_out("Input Redirection");
			dup2(newfd, STDIN_FILENO);
			close(newfd);
			continue;
		}
	}
}

int argCheck(string cmd)
{
	string builtins[] = {"cat","grep","more","less", "echo"};
	string redirects[] = {"<",">",">>","e>", "e>>"};
	//determine if the argument is an executable program
	for(string str: builtins)
	{
		if(str.compare(cmd) == 0)
		{
			return 1;
		}
	}

	//determine if th argument is a I/O redirection character
	for(string str: redirects)
	{
		if(str == cmd)
		{
			return 2;
		}
	}

	//determine if pipe character
	if(cmd.compare("|") == 0) return 3;

	return 0;
}

inline void nope_out(const string & sc_name) {
  perror(sc_name.c_str());
  exit(EXIT_FAILURE);
}

//set signal handlers back to default
void defaultSignal(){
  signal(SIGINT,  SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
}

void defaultIO(){
	//dup2(STDIN_FILENO, fileno(stdin));
	fflush(stdout);
	//dup2(fileno(stdin), STDIN_FILENO);
	dup2(stdoutfd, fileno(stdout));
	//dup2(STDERR_FILENO, fileno(stderr));

	close(stdoutfd);
	//close(STDOUT_FILENO);
	//close(STDERR_FILENO);
}

#include <iostream>
#include <stdlib.h>
#include <string>
#include <cerrno>
#include <cstring>
#include "Socket.h"
#include <iterator>
#include <algorithm>
#include <sstream>
#include <vector>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <fstream>
#include <streambuf>
#include <cstdio>
#include <ctime>

using namespace std;

//fields
Socket* sock;
Socket* dataSock; //for the data connection

int fd;
int dataD; //for data connection
const int BSIZE = 10000;
bool connected = false;

string cmd = "";
string fileName = "";
char* host;

bool authenticate(const char* theHost);
void readFromServer(int fileD,char* temp,bool usingData);
char* serverVerify(int fileD,string command,string input);

//gets input from user
//stores that input into cmd variable
void getInput(string prompt){
    cout<<prompt;
    getline(cin,cmd);
}

//opens a connection at the port for the given host
//if the data param = true, a data connection is opened
void openConnection(char* theHost, int port,bool data){
    if(data){
        dataSock = new Socket(port);
        dataD = dataSock->getClientSocket(theHost);
    }
    else{
        sock = new Socket(port);
        fd = sock->getClientSocket(theHost);
        char* response = new char[BSIZE];
        readFromServer(fd,response,false);
        cout<<response<<endl;
        authenticate(theHost);
        connected = true;
        delete response;
        response = NULL;
        response = serverVerify(fd,"SYST ","");
        cout<<response<<endl;
        delete response;
        response = NULL;
    }
}

//given the contents of a remote file, stores it locally
void storeReceivedFile(char* fileContent){
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int opened = open( fileName.c_str(), O_WRONLY | O_CREAT | O_APPEND, mode );
    write(opened,fileContent,strlen(fileContent));
    close(opened);
}

//takes a string and a delimiter and splits that string by the delim
//returns a vector of that split string
vector<string> mySplit(string command,char delim){
    vector<string> items;
    stringstream stream(command);
    string item;
    while(getline(stream,item,delim)){
        items.push_back(item);        
    }
    return items;
}

//method that acually reads from the server via read()
//will be used by serverVerify for most commands
//can also be used by methods other than serverVerify
void readFromServer(int fileD,char* temp,bool usingData){
    struct pollfd myPoll;
    myPoll.fd = fileD;
    myPoll.events = POLLIN;
    myPoll.revents = 0;
    char* command = new char[BSIZE];
    int reading = read(fileD,command,BSIZE);
    
    if(reading == -1){
        cout<<"error is: "<<strerror(errno)<<endl;
    }
    strcpy(temp,command);    

        if(poll(&myPoll,1,1000) > 0){
            bzero(command,BSIZE);
            reading = read(fileD,command,BSIZE);
            if(reading == -1){
                cout<<"error is: "<<strerror(errno)<<endl;
            }
            strcat(temp,command);
        }
    delete command;
    command = NULL;
}
//takes in a command and optional argument
//converts that full command into a char*
//also returns the length of that method via finalLength parameter
char* toCharArr(string command, string arg, int& finalLength){
    finalLength = command.length() + arg.length() + 2;
    char* converted =  new char[finalLength];
    strcpy(converted,command.c_str());
    strcat(converted,arg.c_str());
    strcat(converted,"\r\n");
    return converted;//don't forget to delete later
    
}

//takes the given command and writes it to the file descriptor fileD
//then reads the response from the server
//can be utilized for most commands
char* serverVerify(int fileD,string command,string input){
    int size = 0;
    char* fullCommand = toCharArr(command,input,size);
    int writing = write(fileD,fullCommand,size);
     if(writing == -1){
        cout<<"error is: "<<strerror(errno)<<endl;
    }
    char* response = new char[BSIZE];
    readFromServer(fileD,response,false);
    delete fullCommand;
    fullCommand = NULL;
    return response;
}

//prepares the data for opening a data connection
//calls passv
void openData(char* theHost,int& fileD){
    char* response = serverVerify(fd,"PASV","");
    cout<<response<<endl;
    string item(response);
    vector<string> junk = mySplit(item,' ');
    string ip = junk[junk.size()-1];
    ip.erase(remove(ip.begin(),ip.end(),'('));
    ip.erase(remove(ip.begin(),ip.end(),')'));
    ip.erase(remove(ip.begin(),ip.end(),'\n'));
    
    vector<string> ipAddr = mySplit(ip,',');
    string tempHost = ipAddr[0] + "." + ipAddr[1] + "." + ipAddr[2] + "." + ipAddr[3];
    strcpy(theHost,tempHost.c_str());
    
    stringstream stream(ipAddr[4]);
    stringstream stream2(ipAddr[5]);
    
    int port;
    stream >> fileD;
    fileD *= 256;
    int temp;
    stream2 >> temp;
    fileD += temp;
    delete response;
}

//for the put method
//first grabs the file desired for transfer
//opens a data connection
//sends that file to the server 
void put(){
    getInput("(local-file) ");
    string local = cmd;
    getInput("(remote-file) ");
    string remote = cmd;
    ifstream stream(local.c_str());
    string total = "";
    string curr = "";
    char* content = new char[BSIZE];
    while(getline(stream,curr)){
        total += curr +"\n";
    }
    strcpy(content,total.c_str());

    char* theHost = new char[BSIZE];
    int fileDescript = 0;
    
    openData(theHost,fileDescript);
    
    int id = fork();
    if(id == 0){
        openConnection (theHost,fileDescript,true);
        write(dataD,content,strlen(content));
        delete theHost;
        delete content;
        theHost = NULL;
        content = NULL;
        close(dataD);
        delete dataSock;
        dataSock = NULL;
        exit(1);
    }
    else{
        char* response = serverVerify(fd,"TYPE I","");
        cout<<response<<endl;
        delete response;
        response = NULL;
        response = serverVerify(fd,"STOR ",remote);
        cout<<response<<endl;
        delete response;
        response = NULL;
        wait(NULL);
    }
}
//for user and pass
//authenticates the user through these commands
bool authenticate(const char* theHost){
    //get username and verify username
        string temp(theHost);
        string prompt = "Name( " + temp + ": " + getlogin() + ") ";
        getInput(prompt);
        char* response;
        response = serverVerify(fd,"USER ",cmd);
        cout<<response<<endl;
        delete response;
        response = NULL;
    
    //get password and verify password
        getInput("Password: ");
        response = serverVerify(fd,"PASS ",cmd);
        cout<<response<<endl;
        bzero(response,BSIZE);
        
        delete response;
        response = NULL;
        return true;
}

//for the ls and get command
//opens a data connection and executes the command indicated by the parameters
void passiveCommand(string command,string arg){
    
    int dataDescript = 0;
    char* theHost = new char[BSIZE];
    
    openData(theHost,dataDescript);
   //// clock_t time = clock(); /////////
    int id = fork();
    if(id == 0){
        openConnection(theHost,dataDescript,true);
        delete theHost;
        theHost = NULL;
       if(command == "RETR "){
            fileName = arg;
            char* serverResponse = new char[BSIZE];
            readFromServer(dataD,serverResponse,true);
            //cout<<"What i read was "<<serverResponse<<endl;
            storeReceivedFile(serverResponse);
            delete serverResponse;
            serverResponse = NULL;
        }
        else{
            char* serverResponse = new char[BSIZE];
            readFromServer(dataD,serverResponse,true);
            cout<<serverResponse<<endl;
            delete serverResponse;
            serverResponse = NULL;
        }
           close(dataD);
           delete dataSock;
           dataSock = NULL;
           exit(1);
    }
    else{
            char* response = serverVerify(fd,command,arg);
            cout<<response<<endl;
            delete response;
            response = NULL;
            wait(NULL);
    }
    //double total = double(clock() - time)/CLOCKS_PER_SEC;
    //cout<<"Time elapsed for get was "<<total<<endl; ////////////
}

bool processInput(vector<string> command){
    
    if(command[0] == "cd"){ 
        char* response = serverVerify(fd,"CWD ",command[1]);
        cout<<response<<endl;
        delete response;
        response = NULL;
    }
    else if(command[0] == "ls"){
        
        passiveCommand("LIST","");
    }
    else if(command[0] == "get"){
        
        passiveCommand("RETR ",command[1]);
    }
    else if(command[0] == "put"){
        
        put();
    }
    else if(command[0] == "close"){
        char* message = serverVerify(fd,"QUIT ","");
        cout<<message<<endl;
        close(fd);
        delete sock;
        sock = NULL;
        connected = false;
        delete message;
        message = NULL;
    }
    else if(command[0] == "quit"){
        if(connected){
            char* message = serverVerify(fd,"QUIT","");
            cout<<message<<endl;
            close(fd);
            delete sock;
            sock = NULL;
            delete message;
            message = NULL;
        }
        return false;
    }
    else if(command[0] == "open"){
        stringstream stream(command[2]);
        int port;
        stream >> port;
        char* Host = new char[command[1].length()];
        strcpy(Host,command[1].c_str());
        openConnection(Host,port,false);
        delete Host;
        Host = NULL;
    }
    return true;
}

void startShell(){
    while(true){
        getInput("ftp>");
        if(cmd == ""){
            continue;
        }
        if(((cmd.find("open") == string::npos) && !connected) && cmd.find("quit") == string::npos){
            cout<<"must connect before running other commands"<<endl;
            continue;
        }
        vector<string> input = mySplit(cmd,' ');
        if(processInput(input)){
            continue;
        }
        break;
    }
}
int main(int argc, char* argv[]){
    //error
    if(argc > 2){
        cout<<"insufficient arguments"<<endl;
        return -1;
    }
    if(argc == 2){//argument provided
        //set up connection immediately
        host = argv[1];
        openConnection(host,21,false);
        connected = true;
        startShell();
    }
    else{//wait for connection
        while(true){
            getInput("ftp>");
            if(cmd.find("open") != string::npos){
                break;
            }
            cout<<"You must open a connection first."<<endl;
        }
        vector<string> input = mySplit(cmd,' ');
        host = new char[input[1].length()];
        strcpy(host,input[1].c_str());
        stringstream stream(input[2]);
        int port;
        stream >> port;
        openConnection(host,port,false);
        startShell();
    }
    return 0;
}
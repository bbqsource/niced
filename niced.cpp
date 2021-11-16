#include <string>
#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <syslog.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/stat.h>
// g++ niced.cpp -o niced -lstdc++fs -g

std::string PROCPath="/proc";
std::string SearchProcess="{?}";
std::string ConfigFile="niced.conf";


int NICEDefault=0;
int NICEThrottled=5;
int ThreshHold=10;
int ThreshHoldDelay=4;




//-----------------------------------


const int TotalMonitorProcesses=1000;


namespace fs = std::experimental::filesystem;


int LatestPID=0;

bool is_number(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

struct PIDStruct {
	int PID=-1;
	std::string CommandLine;
	int Counter;
	float CPU;
	int NICE;
	long ProcessAge;
	long CPUAccumulated;
	long lastutime;
	long laststime;
	long lastcutime;
	long lastcstime;
	long stime;
	long utime;
	long cutime;
	long cstime;
	long starttime;
	long LastChildTotals;
	long ChildTotals;
	std::string CMD;
} PIDs[TotalMonitorProcesses];

const auto processor_count = std::thread::hardware_concurrency();


struct PIDDataStruct {
	int PID=-1;
	int NICE;
	long stime;
	long utime;
	long cutime;
	long cstime;
	long ChildTotals;
	long starttime;
	std::string CMD;
	std::string ProcessName;
	int Result = -99;
};

PIDDataStruct PIDData(int PIDNum, int SkipSub=0){
	PIDDataStruct Return;
	
	int PidOK=-1;
	std::ifstream inFile;
	std::stringstream strStream;
	try{
		inFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		inFile.open(PROCPath+"/"+std::to_string(PIDNum)+"/stat"); //open the input file
		strStream << inFile.rdbuf(); //read the file
		inFile.close();
		PidOK=1;
	}catch(...){
		//std::cout << "No PID" << std::endl;
		PidOK=0;
		Return.Result=-1;
	}
	if (PidOK==1){
		std::string StatTemp = strStream.str(); 
		//std::cout << StatTemp << std::endl;
		int TempArray[23]={0};
		
		long stime;
		long utime;
		long cutime;
		long cstime;
		long starttime;
		int NICE;
		std::string ProcessName="";
		int z=-1;
		int lastpos=0;
		while (z!=22){
			z++;
			int PosA=StatTemp.find(" ",lastpos);
			int PosB=StatTemp.find(" ",PosA+1);
			lastpos=PosB;
			std::string TempString=StatTemp.substr(PosA+1,PosB-PosA-1);
			//std::cout << z+1 <<":" << TempString << std::endl;

			
			if (z==12){
				// utime
				utime=stoi(TempString);
			}
			
			if (z==13){
				// utime
				stime=stoi(TempString);
			}
			if (z==14){
				// utime
				cutime=stoi(TempString);
			}
			if (z==15){
				// utime
				cstime=stoi(TempString);
			}
			if (z==20){
				// utime
				starttime=stol(TempString);
			}
			if (z==17){
				// utime
				NICE=stoi(TempString);
			}
		}
		
		
		
		//
		//
		//
		
		std::string CMDTemp="";
		
		std::ifstream inFile2;
		std::stringstream strStream2;
		try{
			inFile2.exceptions(std::ifstream::failbit | std::ifstream::badbit);
			inFile2.open(PROCPath+"/"+std::to_string(PIDNum)+"/cmdline"); //open the input file
			strStream2 << inFile2.rdbuf(); //read the file
			inFile2.close();
			CMDTemp=strStream2.str(); 
		}catch(...){
			//std::cout << "No CMD File" << std::endl;
		}
		
		
		
		//
		// Get Child Process CPU
		// 
		
		long TotalChildProcessCPU=0;
		//if (SkipSub==0){
			
			//std::string taskdir=PROCPath+"/"+std::to_string(PIDNum)+"/task";
			
			//for (auto & entry : fs::directory_iterator(taskdir)) {
				//std::string TempPath=std::string(entry.path()).erase(0,taskdir.length()+1);
				//if (is_number(TempPath)){
					//std::error_code ec;
					//if (fs::is_directory(entry.path(), ec))	{ 
						////std::cout << "DIR:" << TempPath << " TaskDIR:" << taskdir << std::endl;
						//int PIDIndex=-1;
						//int x=-1;

						//if (fs::exists(taskdir+"/"+TempPath+"/stat")){
							
							
							////std::ifstream inFile;
							////inFile.open(PROCPath+"/"+TempPath+"/stat"); //open the input file

							////std::stringstream strStream;
							////strStream << inFile.rdbuf(); //read the file
							////std::string StatTemp = strStream.str(); //str holds the content of the file
							////std::cout << StatTemp << "\n"; //you can do anything with the string!!!
							////d::string TempA=StatTemp.split(")");
							////std::string TempA=StatTemp.substr(StatTemp.find("("),StatTemp.find(")"));
							
							//PIDDataStruct ChildProcessTemp=PIDData(std::stoi(TempPath),1);
							//TotalChildProcessCPU=TotalChildProcessCPU+ChildProcessTemp.stime+ChildProcessTemp.utime+ChildProcessTemp.cstime+ChildProcessTemp.cutime;
						//}
					//}
				//}	
			//}	
		//}
		
		Return.ProcessName=StatTemp.substr(StatTemp.find("(")+1,(StatTemp.find(")")-StatTemp.find("(")-1));
		Return.PID=PIDNum;
		Return.stime=stime;
		Return.utime=utime;
		Return.cstime=cstime;
		Return.cutime=cutime;
		Return.ChildTotals=TotalChildProcessCPU;
		Return.CMD=CMDTemp;
		Return.starttime=starttime;
		Return.NICE=NICE;
		Return.Result=1;
	}
	
	return Return;
}


void LoadConfig(){
	
	int ConfigError=0;
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
	
    fp = fopen(ConfigFile.c_str(), "r");
    
    if (fp == NULL){    
		std::cout << "! No Config File" << std::endl;
        exit(EXIT_FAILURE);
	}
    while ((read = getline(&line, &len, fp)) != -1) {
        //printf("Retrieved line of length %zu:\n", read);
        //printf("%s", line);
        std::string TempLine=line;
        int DelimPos=TempLine.find("=");
        if (DelimPos!=0){
			std::string value="{?}";
			std::string key=TempLine.substr(0,DelimPos);
			if (((TempLine.length()+1)-DelimPos)>3){
				value=TempLine.substr(DelimPos+1,(TempLine.length()+1)-DelimPos);
			}
			//std::cout << "Line  Pos " << ((TempLine.length()+1)-DelimPos) << " Data:" << key << " = " << value << std::endl;
			if (key=="NICEDefault"){
				if (value!="{?}"){
					//std::cout << "Set NICEDefault to " << value << std::endl;
					NICEDefault=NICEDefault=atoi(value.c_str());
				}else{
					syslog (LOG_NOTICE, "Config \"NICEDefault\" not defined correctly");
					std::cout << "Config \"NICEDefault\" not defined correctly" << std::endl;
					ConfigError=1;
				}
			}
			if (key=="NICEThrottled" && value!="{?}"){
				if (value!="{?}"){
					//std::cout << "Set NICEThrottled to " << value << std::endl;
					NICEThrottled=NICEThrottled=atoi(value.c_str());
				}else{
					syslog (LOG_NOTICE, "Config \"NICEThrottled\" not defined correctly");
					std::cout << "Config \"NICEThrottled\" not defined correctly" << std::endl;
					ConfigError=1;
				}
			}
			if (key=="ThreshHold"){
				if (value!="{?}"){
					//std::cout << "Set ThreshHold to " << value << std::endl;
					ThreshHold=atoi(value.c_str());
				}else{
					syslog (LOG_NOTICE, "Config \"ThreshHold\" not defined correctly");
					std::cout << "Config \"ThreshHold\" not defined correctly" << std::endl;
					ConfigError=1;
				}
			}
			if (key=="ThreshHoldDelay"){
				if (value!="{?}"){
					//std::cout << "Set ThreshHoldDelay to " << value << std::endl;
					ThreshHoldDelay=atoi(value.c_str());
				}else{
					syslog (LOG_NOTICE, "Config \"ThreshHoldDelay\" not defined correctly");
					std::cout << "Config \"ThreshHoldDelay\" not defined correctly" << std::endl;
					ConfigError=1;
				}
			}
			if (key=="ProcessName"){
				if (value!="{?}"){
					//std::cout << "Set ProcessName to " << value << std::endl;
					SearchProcess=value.c_str();
				}else{
					syslog (LOG_NOTICE, "Config \"ProcessName\" not defined correctly");
					std::cout << "Config \"ProcessName\" not defined correctly" << std::endl;
					ConfigError=1;
				}
			}
			
			
			
		}
        
    }

    fclose(fp);
    if (line)
        free(line);
	if (ConfigError==1){
		std::cout << "Error in Config...." << std::endl;
		exit(1);
	}
}



static void DaemonizeProcess(){
	pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0){
		syslog (LOG_NOTICE, "Daemonise Failed [0]");
		//std::cout << "Daemonise Failed [0]" << std::endl;
        exit(EXIT_FAILURE);
	}
    /* Success: Let the parent terminate */
    if (pid > 0){
        exit(EXIT_SUCCESS);
	}
    /* On success: The child process becomes session leader */
    if (setsid() < 0){
		syslog (LOG_NOTICE, "Daemonise Failed [2]");
		std::cout << "Daemonise Failed [2]" << std::endl;
        exit(EXIT_FAILURE);
	}
    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
}

void UpdatePIDs(){
	
	// Get Uptime
	std::ifstream UPtimeFile;
	UPtimeFile.open(PROCPath+"/uptime"); //open the input file

	std::stringstream UptimestrStream;
	UptimestrStream << UPtimeFile.rdbuf(); //read the file
	std::string UptimeTemp = UptimestrStream.str();
	
	float CurrentUptime=std::stof(UptimeTemp.substr(0,UptimeTemp.find(" ")));
	//std::cout << "Uptime: " << CurrentUptime << std::endl;
	
	
	//
	//  Check for new PIDs
	// 
	
    for (auto & entry : fs::directory_iterator(PROCPath)) {
        std::string TempPath=std::string(entry.path()).erase(0,PROCPath.length()+1);
        if (is_number(TempPath)){
			std::error_code ec;
			if (fs::is_directory(entry.path(), ec) && (stoi(TempPath)>LatestPID))	{ 
				//std::cout << TempPath << std::endl;
				LatestPID=stoi(TempPath);
				int PIDIndex=-1;
				int x=-1;
				while (x<TotalMonitorProcesses-1){
					x++;
					if (PIDs[x].PID==stoi(TempPath)){
						PIDIndex=x;
					}
				}
				if (PIDIndex==-1){
					int y=-1;;
					while (PIDIndex==-1 and (y<TotalMonitorProcesses-1)){
						y++;
						
						if (PIDs[y].PID==-1){
							PIDIndex=y;

							if (fs::exists(PROCPath+"/"+TempPath+"/stat")){
								
								PIDDataStruct PIDDataTemp=PIDData(std::stoi(TempPath),1);
								
								if (PIDDataTemp.Result>0){
									
									if (PIDDataTemp.ProcessName.find(SearchProcess) != std::string::npos) {
										PIDs[PIDIndex].PID=PIDDataTemp.PID;
										PIDs[PIDIndex].stime=PIDDataTemp.stime;
										PIDs[PIDIndex].utime=PIDDataTemp.utime;
										PIDs[PIDIndex].cstime=PIDDataTemp.cstime;
										PIDs[PIDIndex].cutime=PIDDataTemp.cutime;
										PIDs[PIDIndex].starttime=PIDDataTemp.starttime;
										PIDs[PIDIndex].CMD=PIDDataTemp.CMD;
										int which = PRIO_PROCESS;
										id_t pid = PIDs[PIDIndex].PID;
										int ret,ret2;

										//pid = getpid();

										ret2 = setpriority(which, PIDDataTemp.PID, NICEDefault);
										PIDs[PIDIndex].NICE=0;
										PIDs[PIDIndex].ChildTotals=PIDDataTemp.ChildTotals;
										
										PIDs[PIDIndex].CPU = ((float(PIDs[PIDIndex].utime + PIDs[PIDIndex].stime + PIDs[PIDIndex].cstime+ PIDs[PIDIndex].cutime) - float(PIDs[PIDIndex].lastutime + PIDs[PIDIndex].laststime+PIDs[PIDIndex].lastcutime + PIDs[PIDIndex].lastcstime))/float(sysconf(_SC_CLK_TCK)))*float(100);
										
										PIDs[PIDIndex].laststime=PIDs[PIDIndex].stime;
										PIDs[PIDIndex].lastutime=PIDs[PIDIndex].utime;
										PIDs[PIDIndex].lastcutime=PIDs[PIDIndex].cutime;
										PIDs[PIDIndex].lastcstime=PIDs[PIDIndex].cstime;
										PIDs[PIDIndex].LastChildTotals=PIDs[PIDIndex].ChildTotals;
										//PIDs[PIDIndex].CPU=stoi(TempString)/sysconf(_SC_CLK_TCK);
										
										syslog (LOG_NOTICE, "New PID %i [%s]", PIDs[PIDIndex].PID,PIDs[PIDIndex].CMD.c_str());
										
										//std::cout << "Ading: " << PIDIndex  << " " << PIDs[PIDIndex].PID << " " << PIDs[PIDIndex].CommandLine << " C " << PIDs[PIDIndex].CPU << " N " << PIDs[PIDIndex].NICE << std::endl;
									}
								}
								
							}
							
							
						}
					} 
				}
				
				
				
					
					
				
				
			}
		}
	}
	
	
	//
	// Update Existing PID Information 
	//
	
	int x=-1;
	while (x<TotalMonitorProcesses-1){
		x++;
		int PIDIndex=x;
		if (PIDs[x].PID!=-1){
			//std::cout << "Existing PID" << std::endl;
			
			PIDDataStruct PIDDataTemp=PIDData(PIDs[x].PID);
				
			if (PIDDataTemp.Result>0){
				
				
				
				PIDs[PIDIndex].stime=PIDDataTemp.stime;
				PIDs[PIDIndex].utime=PIDDataTemp.utime;
				PIDs[PIDIndex].cstime=PIDDataTemp.cstime;
				PIDs[PIDIndex].cutime=PIDDataTemp.cutime;
				PIDs[PIDIndex].starttime=PIDDataTemp.starttime;
				PIDs[PIDIndex].ChildTotals=PIDDataTemp.ChildTotals;
				PIDs[PIDIndex].NICE=PIDDataTemp.NICE;
				
				
				// Calculate CPU.
				
				PIDs[PIDIndex].CPU = ((float(PIDs[PIDIndex].utime + PIDs[PIDIndex].stime + PIDs[PIDIndex].cstime+ PIDs[PIDIndex].cutime - float(PIDs[PIDIndex].lastutime + PIDs[PIDIndex].laststime+PIDs[PIDIndex].lastcutime + PIDs[PIDIndex].lastcstime)))/float(sysconf(_SC_CLK_TCK)))*float(100);
				
				PIDs[PIDIndex].laststime=PIDs[PIDIndex].stime;
				PIDs[PIDIndex].lastutime=PIDs[PIDIndex].utime;
				PIDs[PIDIndex].lastcutime=PIDs[PIDIndex].cutime;
				PIDs[PIDIndex].lastcstime=PIDs[PIDIndex].cstime;
				PIDs[PIDIndex].LastChildTotals=PIDs[PIDIndex].ChildTotals;
				
				//std::cout << "Stime: " << PIDs[PIDIndex].stime <<" Utime: " << PIDs[PIDIndex].utime <<" CStime: " << PIDs[PIDIndex].cstime <<" CUtime: " << PIDs[PIDIndex].cutime << " NICE: " << PIDDataTemp.NICE <<std::endl;
				//PIDs[PIDIndex].CPU=stoi(TempString)/sysconf(_SC_CLK_TCK);
				
				//exit(1);
				
				if (PIDs[PIDIndex].CPU>ThreshHold){
					
					if (PIDs[PIDIndex].Counter>ThreshHoldDelay){
						if (PIDs[PIDIndex].NICE==NICEDefault){
							PIDs[PIDIndex].NICE=NICEThrottled;
							
							int which = PRIO_PROCESS;
							id_t pid = PIDs[PIDIndex].PID;
							int ret,ret2;
							ret2 = setpriority(which, pid, NICEThrottled);
							
							//std::cout << "Set "+std::to_string(PIDs[PIDIndex].PID)+" NICE Throttled"<< std::endl;
							//syslog (LOG_NOTICE, "PID %i set to Throttled Nice (%i)", PIDs[PIDIndex].PID,NICEThrottled);
							
							ret = getpriority(which,pid);
							if (ret!=NICEThrottled){
								syslog (LOG_NOTICE, "Failed to set PID %i to Default Nice (%i), Nice is currently at %i [%s]", PIDs[PIDIndex].PID,NICEThrottled,ret,PIDs[PIDIndex].CMD.c_str());
							}else{
								syslog (LOG_NOTICE, "PID %i set to Throttled Nice (%i) [%s]", PIDs[PIDIndex].PID,NICEThrottled,PIDs[PIDIndex].CMD.c_str());
							}
							
						}else{
							
						}
					}else{
						PIDs[PIDIndex].Counter++;
					}
					
					
				}else{
					if (PIDs[PIDIndex].Counter==0){
						if (PIDs[PIDIndex].NICE==NICEThrottled){
							PIDs[PIDIndex].NICE=NICEDefault;
							
							int which = PRIO_PROCESS;
							id_t pid = PIDs[PIDIndex].PID;
							int ret,ret2;
							ret2 = setpriority(which, pid, 0);
							
							//std::cout << "Set "+std::to_string(PIDs[PIDIndex].PID)+" NICE Default" << std::endl;
							
							
							ret = getpriority(which,pid);
							if (ret!=NICEDefault){
								syslog (LOG_NOTICE, "Failed to set PID %i to Default Nice (%i), Nice is currently at %i [%s]", PIDs[PIDIndex].PID,NICEDefault,ret,PIDs[PIDIndex].CMD.c_str());
							}else{
								syslog (LOG_NOTICE, "PID %i set to Default Nice (%i) [%s]", PIDs[PIDIndex].PID,NICEDefault,PIDs[PIDIndex].CMD.c_str());
							}
							
						}else{
							
						}
					}else{
						PIDs[PIDIndex].Counter--;
					}
					
				}
				
				//std::cout << PIDIndex  << " " << PIDs[PIDIndex].PID << " " << PIDs[PIDIndex].CommandLine << " C " << PIDs[PIDIndex].CPU << " N " << PIDs[PIDIndex].NICE << std::endl;
				
			}else{
				syslog (LOG_NOTICE, "PID %i terminated", PIDs[PIDIndex].PID);
				PIDs[PIDIndex].PID=-1;
			}
		}
	}
}


int main()
{
	syslog (LOG_NOTICE, "Starting...");
	std::cout << "Starting...." << std::endl;
	LoadConfig();
	//exit(0);
	openlog ("uatu", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	
	
	DaemonizeProcess();
	
	
	syslog (LOG_NOTICE, "Monitoring \"%s\" processes", SearchProcess.c_str());
	std::cout << "Monitoring \"" << SearchProcess << "\"processes" << std::endl;
	while (1){
		UpdatePIDs();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		//sleep_until(system_clock::now() + seconds(1));
	}
	closelog ();
	return EXIT_SUCCESS;
}

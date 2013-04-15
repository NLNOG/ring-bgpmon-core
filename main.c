/* 
 *
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom
 *	the Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *	OTHER DEALINGS IN THE SOFTWARE.\
 * 
 * 
 *  File: main.c
 *  Authors: Dave matthews, Yan Chen, He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 *  Editors: Cathie Olschanowsky @ June 2012
 */
 
/* bgpmon - BGP Monitor
 * 
 * Main program read command line and launch configuration parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>


#include "site_defaults.h"

#include "Util/log.h"
#include "Util/bgpmon_defaults.h"
#include "Util/signals.h"
#include "Util/acl.h"

#include "Login/commandprompt.h"
#include "Login/login.h"
#include "Config/configfile.h"
#include "Queues/queue.h"
#include "Clients/clients.h"
#include "Mrt/mrt.h"
#include "Chains/chains.h"
#include "Labeling/label.h"
#include "PeriodicEvents/periodic.h"
#include "Peering/peers.h"
#include "Peering/peersession.h"
#include "Peering/bgpstates.h"
#include "XML/xml.h"

#define DEBUG

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static void child_handler(int signum)
{
    switch(signum) {
    case SIGALRM: exit(EXIT_FAILURE); break;
    case SIGUSR1: exit(EXIT_SUCCESS); break;
    case SIGCHLD: exit(EXIT_FAILURE); break;
    }
}


/*----------------------------------------------------------------------------------------
 * Purpose: Print out usage and help information
 * Input: The pointer of the program name
 * Output:  none
 *  Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/
void usage ( char *arg )
{
	fprintf ( stderr,"\n");
	fprintf ( stderr,"Usage: %s", arg);
	fprintf ( stderr,"\n");
	fprintf ( stderr,"        [-c <configuration filename>] \n");
	fprintf ( stderr,"        [-d] [-s] [-l <log level (0-7)>] [-f <syslog facility (0-14)>]\n");
	fprintf ( stderr,"        [-r <port>] \n");

	fprintf ( stderr,"Options: \n");
	fprintf ( stderr,"        -d                          :   run in daemon mode \n");
	fprintf ( stderr,"        -c <configuration filename> :   specify configuration file \n");
	fprintf ( stderr,"        -s                          :   logs output to syslog \n");
	fprintf ( stderr,"        -l <log level (0-7)>        :   specify log level \n");
	fprintf ( stderr,"        -f <syslog facility (0-14)> :   specify syslog facility \n");
	fprintf ( stderr,"        -r <port>                   :   specify recovery port \n");
	fprintf ( stderr,"\n");
	exit(1);
}

static void
godaemon (char *rundir, char *pidfile)
{
  char str[10];
  pid_t pid,sid,parent;
  int lfp = -1;

  // Check if parent process id is set 
  if (getppid() == 1){
    fprintf(stderr,"PPID exists, therefore we are already a daemon ");
    exit(EXIT_FAILURE);
  }

  /* Create the lock file as the current user */
  if ( pidfile && pidfile[0] ) {
    lfp = open(pidfile,O_RDWR|O_CREAT,0640);
    if ( lfp < 0 ) {
      log_err("unable to create pid file %s, code=%d (%s)",
              pidfile, errno, strerror(errno) );
      exit(EXIT_FAILURE);
    }
    /* Get and format PID */
    sprintf(str,"%d\n",getpid());
    int res = write(lfp, str, strlen(str));
    if(res != strlen(str)){
      log_err("Unable to write pid to file\n");
      exit(EXIT_FAILURE);
    }
  }

  /* create the directory for temp data if need be */
  mode_t process_mask = umask(0);
  char dirname[FILENAME_MAX_CHARS];
  sprintf(dirname,"%s/%s",rundir,"bgpmon");
  int result_code = mkdir(dirname, S_IRWXU | S_IRWXG | S_IRWXO);
  if(result_code && errno != EEXIST){
    log_err("Unable to create scratch dir %s\n",dirname);
    exit(EXIT_FAILURE);
  }
  result_code = umask(process_mask);
  if(result_code){
    log_err("Unable to set mask\n");
    exit(EXIT_FAILURE);
  }

  /* Drop user if there is one, and we were run as root */
  if ( getuid() == 0 || geteuid() == 0 ) {
    struct passwd *pw = getpwnam(RUN_AS_USER);
    if ( pw ) {
      result_code = chown(dirname,pw->pw_uid,pw->pw_gid);
      if(result_code){
        log_err("Unable to create scratch dir %s\n",dirname);
        exit(EXIT_FAILURE);
      }
      syslog( LOG_NOTICE, "setting user to " RUN_AS_USER );
      setuid( pw->pw_uid );
    }
  }
  /* Trap signals that we expect to recieve */
  signal(SIGCHLD,child_handler);
  signal(SIGUSR1,child_handler);
  signal(SIGALRM,child_handler);

  /* Fork*/
  pid = fork ();
  if (pid < 0) {
    log_err ("fork failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  /* In case of this is parent process. */
  if (pid > 0){
     // Wait for confirmation from the child via SIGTERM or SIGCHLD, or
     // for two seconds to elapse (SIGALRM).  pause() should not return. 
     alarm(2);
     pause();
     exit(EXIT_FAILURE);
  }

  // we are now the parent process
  parent = getppid();

  /* Cancel certain signals */
  signal(SIGCHLD,SIG_DFL); /* A child process dies */
  signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
  signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

  /* Change the file mode mask */
  umask(0);

  /* Become session leader and get pid. */
  sid = setsid();
  if (sid < 0)
  {
    log_err ("setsid failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* Change the current working directory.  This prevents the current
     directory from being locked; hence not being able to remove it. */
  if ((chdir("/")) < 0) {
    syslog( LOG_ERR, "unable to change directory to %s, code %d (%s)",
           "/", errno, strerror(errno) );
    exit(EXIT_FAILURE);
  }

  /* Redirect standard files to /dev/null */
  FILE* res;
  res = freopen( "/dev/null", "r", stdin);
  if(res == NULL){
    log_err("unable to redirect stdin\n");
    exit(EXIT_FAILURE);
  }
  res = freopen( "/dev/null", "w", stdout);
  if(res == NULL){
    log_err("unable to redirect stdout\n");
    exit(EXIT_FAILURE);
  }
  res = freopen( "/dev/null", "w", stderr);
  if(res == NULL){
    log_err("unable to redirect stderr\n");
    exit(EXIT_FAILURE);
  }

  // let the parent know that all is good
  kill( parent, SIGUSR1 );
}


int 
main(int argc, char **argv)
{
	char	program_name[FILENAME_MAX_CHARS];
	char	config_file[FILENAME_MAX_CHARS];

	int	syslog;
	int	loglevel;
	int	logfacility;

	char	c;

 	int	recoveryport;
	int	error;
        int daemon_mode = 0;

 	// init program_name and default config file
	strncpy(program_name, argv[0], FILENAME_MAX_CHARS);
	strncpy(config_file, DEFAULT_CONFIGFILE, FILENAME_MAX_CHARS);

	// init default log settings
	syslog = DEFAULT_USE_SYSLOG;
	loglevel = DEFAULT_LOG_LEVEL;
 	logfacility = DEFAULT_LOG_FACILITY;

	// allow the user to set a special port for login recovery
 	recoveryport = 0;

	//save start time for uptime calculation in status messages
	bgpmon_start_time = time(NULL);

 	// parse command line
	while ((c = getopt (argc, argv, "dl:c:f:hsr:")) != -1) {
		switch (c) {
			case 'c':
				strncpy(config_file, optarg, FILENAME_MAX_CHARS);
				break;
        	 	case 'l':
        	 		loglevel = atoi(optarg);
        	 	   	break;
        	 	case 'f':
        	 		logfacility = atoi(optarg);
        	 	   	break;
        	 	case 's':
        	 	   	syslog = 1;
        	 	   	break;
        	 	case 'r':
        	 		recoveryport = atoi(optarg);
        	 	   	break;
 			case 'd':
				daemon_mode = 1;
                                break;
        	 	case 'h':
        	 	case '?':
        	 	default :
        	 		usage( argv[0] );
        	 	   	break;
 		}
	}

	
	// if optind is less than the number of arguments from the command line (argc), then there
	// is something in the argument string that isn't preceded by valid switch (-c, -l, etc)
	if(optind<argc) {
		usage(argv[0]);
	}

	// initilaze log module
	if (init_log (program_name, syslog, loglevel, logfacility) ) {
		fprintf (stderr, "Failed to initialize log functions!\n");
		exit(1);
	}
#ifdef DEBUG
   	debug (__FUNCTION__, "Successfully initialized log module.");
#endif
	log_msg("BGPmon starting up\n");
        /* Turn into daemon if daemon_mode is set. */
        if (daemon_mode){
          godaemon(RUN_DIR,PID_FILE);
        }
	

        char scratchdir[FILENAME_MAX_CHARS];
        sprintf(scratchdir,"%s/%s",RUN_DIR,"bgpmon");
	// initialize login interface
  	if (initLoginControlSettings(config_file, scratchdir, recoveryport) ) {
           	log_fatal("Unable to initialize login interface");
	};
#ifdef DEBUG
   	debug (__FUNCTION__, "Successfully initialized login module.");
#endif

	// initialize acl
	if(initACLSettings()) {
           	log_fatal("Unable to initialize ACL settings");
	};
#ifdef DEBUG
   	debug (__FUNCTION__, "Successfully initialized acl module.");
#endif

	// initialize the queue information
  	if (initQueueSettings() ) {
           	log_fatal("Unable to initialize queue settings");
	};
#ifdef DEBUG
   	debug (__FUNCTION__, "Successfully initialized queue settings.");
#endif

	// initialize clients control settings
  	if (initClientsControlSettings() ) {
		log_fatal("Unable to initialize client settings");
	};
#ifdef DEBUG
   	debug (__FUNCTION__, "Successfully initialized client settings.");
#endif

	// initialize clients control settings
	if (initMrtControlSettings() ) {
			log_fatal("Unable to initialize mrt settings");
		};
#ifdef DEBUG
	debug (__FUNCTION__, "Successfully initialized mrt settings.");
#endif

	//  initialize chains settings
  	if (initChainsSettings() ) {
		log_fatal("Unable to initialize chain settings");
	};

	// initialize the periodic settings
	if (initPeriodicSettings() ) {
			log_fatal("Unable to initialize periodic settings");
	};
#ifdef DEBUG
	debug (__FUNCTION__, "Successfully initialized periodic settings.");
#endif
	
	// read in the configuration file and change
	// all relevant settings based on config file
	if (readConfigFile(config_file) ) {
		// if the configuration file is not readable, remove the
		// configuration file and backup a copy
		backupConfigFile(LoginSettings.configFile, LoginSettings.scratchDir);
		saveConfigFile(LoginSettings.configFile);
		log_err("Corrupt Configuration File Moved to %s.  New configuration file written.", LoginSettings.scratchDir);  
	}


	/*
	 * Block signals in initial thread. New threads will
	 * inherit this signal mask. All the signals will be handled
	 * in a dedicated thread
	 */
	sigfillset ( &signalSet );
	pthread_sigmask ( SIG_BLOCK, &signalSet, NULL );

	// launch the signal handling thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating signal thread...");
#endif
	pthread_t sighandlerThreadID;
	if ((error = pthread_create(&sighandlerThreadID, NULL, sigHandler, config_file)) > 0 )
		log_fatal("Failed to create signal handler thread: %s\n", strerror(error));
#ifdef DEBUG
	debug(__FUNCTION__, "Created signal thread!");
#endif

	// create the system queues
#ifdef DEBUG
	debug(__FUNCTION__, "Creating queues...");
#endif
	/*create the peer queue*/
	peerQueue = createQueue(copyBMF, sizeOfBMF, PEER_QUEUE_NAME,FALSE,NULL,NULL);

	/*create the label queue*/		  
	labeledQueue = createQueue(copyBMF, sizeOfBMF, LABEL_QUEUE_NAME,FALSE,NULL,NULL);

  /*create the MRT queue*/
  mrtQueue = createQueue(copyBMF, sizeOfBMF, MRT_QUEUE_NAME,FALSE,
                         peerQueue->queueGroupCond,peerQueue->queueGroupLock);

	/*create the xml queue*/
	xmlUQueue = createQueue(copyXML, sizeOfXML, XML_U_QUEUE_NAME, TRUE,NULL,NULL);
	xmlRQueue = createQueue(copyXML, sizeOfXML, XML_R_QUEUE_NAME, TRUE,NULL,NULL);	
#ifdef DEBUG
        debug(__FUNCTION__, "Created queues!");
#endif

	// launch the peering thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating peering thread...");
#endif	   
	launchAllPeers();
#ifdef DEBUG
	debug(__FUNCTION__, "Created peering thread!");
#endif

	// launch the labeling thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating labeling thread...");
#endif	   
	launchLabelingThread();
#ifdef DEBUG
	debug(__FUNCTION__, "Created labeling thread!");
#endif

	// launch the xml thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating xml thread...");
#endif	   
	launchXMLThread();
#ifdef DEBUG
	debug(__FUNCTION__, "Created xml thread!");
#endif


	// launch the clients control thread
#ifdef DEBUG
        debug(__FUNCTION__, "Creating clients control thread...");
#endif
	launchClientsControlThread();
#ifdef DEBUG
        debug(__FUNCTION__, "Created clients control thread!");
#endif

	// launch the mrt control thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating mrt control thread...");
#endif
	launchMrtControlThread();
#ifdef DEBUG
	debug(__FUNCTION__, "Created mrt control thread!");
#endif

        // launch the configured chains thread
#ifdef DEBUG
        debug(__FUNCTION__, "Creating threads for each configured chain...");
#endif	
	launchAllChainsThreads();
#ifdef DEBUG
        debug(__FUNCTION__, "Created threads for each configured chain!");
#endif

        // launch the login thread
#ifdef DEBUG
        debug(__FUNCTION__, "Creating login thread...");
#endif
	launchLoginControlThread();
#ifdef DEBUG
        debug(__FUNCTION__, "Created login thread!");
#endif

	// launch the periodic thread
#ifdef DEBUG
	debug(__FUNCTION__, "Creating periodic thread...");
#endif
	LaunchPeriodicThreads();
#ifdef DEBUG
	debug(__FUNCTION__, "Created periodic thread!");
#endif
	
	// write BGPMON_START message into the Peer Queue
	BMF bmf = createBMF(0, BMF_TYPE_BGPMON_START);
	QueueWriter qw = createQueueWriter(peerQueue);
	writeQueue(qw, bmf);
	destroyQueueWriter(qw);

	// periodically check on the state of each thread
	time_t threadtime;
	time_t currenttime;
	char currenttime_extended[64];
	memset(currenttime_extended, 0, 64);
	char threadtime_extended[64];
	memset(threadtime_extended, 0, 64);
	struct tm * current_tm = NULL;
	struct tm * thread_tm = NULL;
	int *chainIDs;
	long *clientIDs;
	long *mrtIDs;
	int clientcount, chaincount, mrtcount, i;
	while ( TRUE ) 
	{
		// get the current running time to compare the threads
		// last exection time to
		currenttime = time(NULL);
		sleep(THREAD_CHECK_INTERVAL);
		current_tm = localtime(&currenttime);
		strftime(currenttime_extended, sizeof(currenttime_extended), "%Y-%m-%dT%H:%M:%SZ", current_tm);

		// CLI MODULE
			// check the cli listener
			threadtime = getLoginControlLastAction();
			if (difftime(currenttime, threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("CLI Module is idle: current time = %s, last thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}

			// check the individual cli threads for updates
			int clicount = 0;
			long * cliIDs = NULL;
			clicount = getActiveCliIds(&cliIDs);
			if(clicount != -1)
			{
				for ( i=0; i<clicount; i++) 
				{
					threadtime = getCliLastAction(cliIDs[i]);
					if ( difftime(currenttime, threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("Cli %d is idle: current time = %s, last cli %d thread time = %s", cliIDs[i], currenttime_extended, cliIDs[i], threadtime_extended);
					}
				}
				free(cliIDs);
			}

		// CLIENTS MODULE
			// Clients Module
			threadtime = getClientsControlLastAction();
			thread_tm = localtime(&threadtime);
			strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
			if (difftime(currenttime, threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("Clients Module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}

			// UPDATE clients
			clientcount = getActiveClientsIDs(&clientIDs, CLIENT_LISTENER_UPDATA);
			if(clientcount != -1)
			{
				for (i = 0; i < clientcount; i++) 
				{
					threadtime = getClientLastAction(clientIDs[i], CLIENT_LISTENER_UPDATA);
					if (difftime(currenttime, threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("Updates Client %d is idle: current time = %s, last client %d thread time = %s", clientIDs[i], currenttime_extended, clientIDs[i], threadtime_extended);
					}
				}
				free(clientIDs);
			}

			// RIB clients
			clientcount = getActiveClientsIDs(&clientIDs, CLIENT_LISTENER_RIB);
			if(clientcount != -1)
			{
				for (i = 0; i < clientcount; i++) 
				{
					threadtime = getClientLastAction(clientIDs[i], CLIENT_LISTENER_RIB);
					if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("RIB Client %d is idle: current time = %s, last client %d thread time = %s", clientIDs[i], currenttime_extended, clientIDs[i], threadtime_extended);
					}
				}
				free(clientIDs);
			}

		// PEER MODULE
			for (i=0; i < MAX_SESSION_IDS; i++)
			{
				if( Sessions[i] != NULL && getSessionState(Sessions[i]->sessionID) == stateEstablished)
				{
					threadtime = getSessionLastActionTime(i);
					if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("Peering session %d is idle: current time = %s, last client %d thread time = %s",Sessions[i]->sessionID,  currenttime_extended, Sessions[i]->sessionID, threadtime_extended);
						//closeBgpmon(config_file);
					}
					
				}
			}

		// LABELING MODULE
			threadtime = getLabelThreadLastActionTime();
			if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("Labeling module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}		

		// XML MODULE
			threadtime = getXMLThreadLastAction();
			if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("XML module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}				


		// QUAGGA MODULE
			// mrt listener
			threadtime = getMrtControlLastAction();
			if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("MRT module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}

			// mrt connections
			mrtcount = getActiveMrtsIDs(&mrtIDs);
			if(mrtcount != -1)
			{
				for (i = 0; i < mrtcount; i++) 
				{
					threadtime = getMrtLastAction(mrtIDs[i]);
					if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("MRT client %d is idle: current time = %s, last client %d thread time = %s",mrtIDs[i], currenttime_extended, mrtIDs[i], threadtime_extended);
					}
				}
				free(mrtIDs);
			}

		// CHAIN MODULE
			chaincount = getActiveChainsIDs(&chainIDs);
			if(chaincount != -1)
			{
				for (i = 0; i < chaincount; i++) 
				{
					threadtime = getChainLastAction(chainIDs[i]);
					if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL) 
					{
						thread_tm = localtime(&threadtime);
						strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
						log_warning("BGPmon chain %ld is idle: current time = %s, last client %ld thread time = %s", chainIDs[i], currenttime_extended, chainIDs[i], threadtime_extended);
					}
				}
				free(chainIDs);
			}			

		// PERIODIC MODULE
			// route refresh 
			threadtime = getPeriodicRouteRefreshThreadLastActionTime();
			if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("Route Refresh module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}		

			// status message
			threadtime = getPeriodicStatusMessageThreadLastActionTime();	
			if (difftime(currenttime,threadtime) > THREAD_DEAD_INTERVAL)
			{
				thread_tm = localtime(&threadtime);
				strftime(threadtime_extended, sizeof(threadtime_extended), "%Y-%m-%dT%H:%M:%SZ", thread_tm);
				log_warning("Status Messages module is idle: current time = %s, last control thread time = %s", currenttime_extended, threadtime_extended);
				//closeBgpmon(config_file);
			}

	}
	
	// should never get here
	log_err( "main: unexpectedly finished");

	return(0);
} 



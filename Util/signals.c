/* 
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
 *	OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 *  File: signals.c
 * 	Authors: He Yan, Dan Massey
 *  Date: June 25, 2008 
 */
	 
#include "signals.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include "log.h"
#include "utils.h"
#include "../Config/configfile.h"
#include "../Peering/peers.h"
#include "../Queues/queue.h"

#include "../Login/login.h"
#include "../Clients/clients.h"
#include "../Labeling/label.h"
#include "../XML/xml.h"
#include "../PeriodicEvents/periodic.h"
#include "../Chains/chains.h"
#include "../Mrt/mrt.h"

//#define DEBUG

sigset_t signalSet;

/*Cleanly close down BGPmon*/
void closeBgpmon(char *config_file) 
{
	log_warning("BGPmon Exiting\n");
	
	// write BGPMON_STOP message into the Peer Queue
	BMF bmf = createBMF(0, BMF_TYPE_BGPMON_STOP);
	QueueWriter qw = createQueueWriter(peerQueue);
	writeQueue(qw, bmf);
	destroyQueueWriter(qw);

	// signal each module to shutdown
        log_warning("Signaling shutdown to modules");
	signalLoginShutdown();
	signalChainShutdown();
	signalPeersShutdown();
	signalMrtShutdown();
	signalLabelShutdown();
	signalPeriodicShutdown();
	signalXMLShutdown();
	signalClientsShutdown();

	// wait for each module to shutdown before proceeding
	waitForLoginShutdown();
        log_warning("Login shutdown complete");
	waitForChainShutdown();
        log_warning("Chain shutdown complete");
	waitForPeersShutdown();
        log_warning("Peers shutdown complete");
	waitForMrtShutdown();
        log_warning("MRT shutdown complete");
	waitForLabelShutdown();
        log_warning("Label shutdown complete");
	waitForPeriodicShutdown();
        log_warning("Periodic shutdown complete");
	waitForXMLShutdown();
        log_warning("XML shutdown complete");
	waitForClientsShutdown();
        log_warning("Client shutdown complete");

	//all modules are shut down; tear down the queues
	destroyQueue(peerQueue);
	destroyQueue(labeledQueue);
	destroyQueue(xmlRQueue);
	destroyQueue(xmlUQueue);
	
	log_warning("BGPmon Exit Successfully!");
	exit(0);
}

void*  sigHandler(void *arg)
{
	int		sig_caught;    /* signal caught       */
	int		rc;            /* returned code       */
	char	*config_file = arg;
	log_msg("Signal Handler Thread Started" );
	
	for(;;)
	{
		rc = sigwait (&signalSet, &sig_caught);
		if (rc != 0) 
		{
			log_fatal( "sigwait fatal error");
		}
		switch (sig_caught)
		{
			case SIGHUP:
				log_warning("Received SIGHUP signal.");
				break;
			case SIGTERM:
			case SIGINT:     /* process SIGINT  */;
				closeBgpmon(config_file);//Function in config .h to cleanly close BGPmon
				break;

			default:         /* should normally not happen */
				log_warning ("Unexpected signal %d ignored", sig_caught);
				break;
		}
	}
}



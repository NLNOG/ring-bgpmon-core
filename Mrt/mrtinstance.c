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
 *	OTHER DEALINGS IN THE SOFTWARE.\
 * 
 * 
 *  File: mrtinstance.c
 * 	Authors: He Yan, Dan Massey, Mikhail, Cathie Olschanowsky
 *
 *  Date: Oct 7, 2008 
 *  Refactor: March 26, 2012 (Cathie Olschanowsky)
 */
 
/* internal structures and functions for this module */
#include "mrtinstance.h"

//#define DEBUG

/*------------------------------------------------------------------------------
 * Purpose: The main function of a thread handling one mrt connection
 * Input:  the mrt node structure for this mrt
 * Output: none
 * He Yan @ July 22, 2008
 * Cathie Olschanowsky April 2012 BGPMON-102
 * The main function is required to 
 * 1 - instantiate a backlog
 * 2 - start the thread that will read from the backlog
 * 3 - enter a loop and repeatedly read from the socket and write to the backlog.
 * 4 - shutdown when loop exits
 * ---------------------------------------------------------------------------*/
void *
mrtThread( void *arg )
{
  // the mrt node structure
  MrtNode *cn = arg;	
  uint32_t rawData_length = MAX_MRT_LENGTH;
  uint8_t rawData[MAX_MRT_LENGTH]; // this is arbitrary we do not expect to read
                                   // complete message with each loop
  // Task: 1 start the backlog at the default size
  if(MRT_backlog_init(&(cn->backlog))){
    // this thread cannot run without the backlog initialized
    cn->deleteMrt=TRUE;
    return NULL;
  }

  // Task: 2 start the thread that will read from the backlog
  pthread_t mrtThreadID;
  int error;
  if ((error = pthread_create( &mrtThreadID, NULL, &MRT_processMessages, cn)) > 0) {
    log_err("Failed to create mrt thread: %s", strerror(error));
    destroyMrt(cn->id);
    pthread_exit( (void *) 1 ); 
    return NULL;
  }
  cn->mrtThreadID2 = mrtThreadID;

#ifdef DEBUG
  log_msg("MRT reading thread backlog is %x\n",&(cn->backlog.lock));
#endif

  // Task 3:
  // now the messages being put in the backlog will be processed when there is 
  // time. the only job for this thread is to read from the socket and write to
  // the backlog the size being read no longer matters, because we don't 
  // guarantee full messages. 
  // while the mrt connection is alive, read from it
  // Task 3a: set up some infrastructure for reading from the socket
  int ready;
  fd_set readfds;
  struct timeval tv;
  tv.tv_sec = THREAD_CHECK_INTERVAL/2;
  tv.tv_usec = 0;
  size_t bytes_read = 0;
  
  do{

    // update the last action time
    cn->lastAction = time(NULL);

    FD_ZERO(&readfds);
    FD_SET(cn->socket, &readfds);
    // check to see if our socket has any available data
    ready = select(cn->socket+1, &readfds, (fd_set *) 0,(fd_set *) 0, &tv);
     
    // if ready is 0 then there is no data to read, this if fine, we just need
    // to check deleteMrt to see if we should shut down, otherwise read 
    if(ready){
      // if we are ready to go, read from the socket
      bytes_read = read(cn->socket,rawData,rawData_length);
    
      // bytes_read is 0 here it means that the socket has been closed
      // otherwise, send it to the backlog
      if(bytes_read > 0){

        // lock the backlog
        if ( pthread_mutex_lock( &(cn->backlog.lock) ) ){
          log_err("Unable to get the backlog lock");
          destroyMrt(cn->id);
          pthread_exit( (void *) 1 ); 
        }

        // write to the backlog
        if(MRT_backlog_write(&(cn->backlog),rawData, bytes_read)){
          log_err("Unable to write data to the backlog");
          destroyMrt(cn->id);
          pthread_exit((void*)1);
        }

        // unlock the backlog
        if ( pthread_mutex_unlock( &(cn->backlog.lock) ) ){
          log_err("Unable to unlock the backlog lock");
          destroyMrt(cn->id);
          pthread_exit( (void *) 1 ); 
        }
      }else{
        cn->deleteMrt = TRUE;
      }
    }
      
  }while ( cn->deleteMrt==FALSE );

  cn->deleteMrt = TRUE;
  void * retval;
  pthread_join(mrtThreadID,&retval);
  // Task 4: cleanup
  // the loop is has exited... 
  // this means that the connection has be shut down -- perform cleanup
  log_warning("MRT Node %d disconnected!", cn->id);
  // change all sessions which were create by this mrt node to stateError
  log_warning("Deleting session");
  deleteMrtSession(cn->id);
  // shutdown connection and clean memory
  log_warning("Destroying MRT");
  destroyMrt(cn->id);	
  // exit thread
  log_warning("Exiting Thread");
  return NULL;
}

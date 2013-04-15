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
 *  File: log.c
 * 	Authors: Yan Chen, Dan Massey
 *  Date: Jun 8, 2008 
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"
#include "../site_defaults.h"


/* useSysLog: 1 to log messages to syslog, 0 to log to stdout */
static int useSysLog = DEFAULT_USE_SYSLOG;
/* log_level: which messages are logged, level ranges from 3 (LOG_ERR) to 7 (LOG_DEBUG) */
static int log_level = DEFAULT_LOG_LEVEL;
/* log_output when not using syslog.   set to stdout */
static FILE *log_output;

/*----------------------------------------------------------------------------------------
 * Purpose: Initialize log function environment. Initialize syslog if necessary. 
 * Input:   The log settings
 *          1.  the program name 
 *          2.  flag indicating whether to use syslog
 *          3.  log level
 *          4.  log facility
 * Output:  0 for success or 1 for failure
 * Note:    Debug mode: All error, warning, log and debug message will be printed to stdout
 *          Syslog mode: All message other than debug message will be printed to syslog.
 *                       The syslog destination can be changed in /etc/syslog.conf 
 * Yan @ Nov 7, 2006   - Dan updated June 8, 2008
 * -------------------------------------------------------------------------------------*/
int init_log (char *program_name, int syslog, int level, int facility)
{
   useSysLog = syslog;
   log_level = level;
   if (useSysLog == 1) {
      openlog (program_name, LOG_CONS | LOG_NDELAY, facility);
   } else {
      log_output = stdout;   
   }
   return 0;   
}

/*----------------------------------------------------------------------------------------
 * Purpose: Log the message to the log file or stdout. 
 * Input:   1. Log level. e.g. LOG_DEBUG, LOG_ERR (same as the level in the syslog function)
 *          2. Message
 *          3. Argument list
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
void Vsyslog (int level, const char *msg, va_list args)
{
   if (useSysLog == 1)
      vsyslog (level, msg, args);
   else {
      time_t now;
      struct tm *gmtnow;
      char strnow[64];
      time(&now);
      gmtnow = gmtime(&now);
      if (strftime(strnow, sizeof(strnow), "%Y-%m-%dT%H:%M:%SZ", gmtnow))
         fprintf(log_output, "%s ", strnow );
      fprintf (log_output, "%s ", log_level_string[level]); 
      vfprintf (log_output, msg, args);
      fprintf (log_output, "\n"); 
      fflush(log_output);
   }
   return;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Print fatal message to the syslog or logfile and terminate.
 *          (LOG_ALERT msg in syslog)
 * Input:   The pointer of message (just like printf)
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 *          A safe exit function may be needed here
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
void log_fatal (const char *msg, ... )
{
   va_list  args;
   va_start (args, msg);
   Vsyslog (LOG_ALERT, msg, args);
   va_end (args);
   abort();
   exit(1);   
}

/*----------------------------------------------------------------------------------------
 * Purpose: Print error message to the syslog or logfile and return. 
 *          (LOG_ERR msg in syslog)
 * Input:   The pointer of message (just like printf)
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
void log_err (const char *msg, ... )
{
   va_list  args;
   va_start (args, msg);
   Vsyslog (LOG_ERR, msg, args);
   va_end (args);
   return;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Print warning message to the syslog or logfile and return. 
 *          (LOG_WARNING msg in syslog)
 * Input:   The pointer of message (just like printf)
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
void log_warning (const char *msg, ...)
{
   if (log_level < LOG_WARNING) return;
   va_list  args;
   va_start (args, msg);
   Vsyslog (LOG_WARNING, msg, args);
   va_end (args);
   return;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Print log message to the syslog or logfile and return. 
 *          (LOG_INFO msg in syslog)
 * Input:   The pointer of message (just like printf)
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
void log_msg (const char *msg, ...)
{
   if (log_level < LOG_INFO) return;
   va_list  args;
   va_start (args, msg);
   Vsyslog (LOG_INFO, msg, args);
   va_end (args);
   return;   
}

/*----------------------------------------------------------------------------------------
 * Purpose: Print log message to stderr and return. 
 * Input:   Caller's name and message
 * Output:  none.
 * NOTE:    No line break needed at the end of the input message.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/ 
#define MAX_DEBUG_FUNC_NAME 200 
#define MAX_DEBUG_MSG 1000
void debug (const char *func_name, const char *msg, ...)
{
   if (log_level < LOG_DEBUG) return;
   char final_msg[MAX_DEBUG_FUNC_NAME+MAX_DEBUG_MSG+ 13];
   memset(final_msg, 0, MAX_DEBUG_FUNC_NAME+MAX_DEBUG_MSG+ 13);
   va_list args;
   strncat(final_msg, "Function: ", 10);
   strncat(final_msg, func_name, MAX_DEBUG_FUNC_NAME);
   strncat(final_msg, ": ", 2);
   strncat(final_msg, msg, MAX_DEBUG_MSG);
   va_start (args, msg);
   Vsyslog (LOG_DEBUG, final_msg, args);
   va_end (args);
   return;   
}
 
/*----------------------------------------------------------------------------------------
 * Purpose: Print given memory in hex format
 * Input:   Message level, the pointer of the memory, and the length
 * Output:  none
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/
void hexdump(int level, const void *mem, int length)
{
  char  line[80];
  char *src = (char*)mem;
  unsigned i, j;

  if ( length <= 0) return;
  
  for (i=0; i<length; i+=16, src+=16) {
    char *t = line;
 
    t += sprintf(t, "%04x:  ", i);
    for (j=0; j<16; j++) {
      if (i+j < length)
        t += sprintf(t, "%02X", src[j] & 0xff);
      else
        t += sprintf(t, "  ");
      t += sprintf(t, j%2 ? " " : "-");
    }

   syslog (level, "%s", line);
// va_list  args;
// Vsyslog (level, line, args);
  }
  return;
}

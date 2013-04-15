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
 *  File: log.h
 * 	Authors: Yan Chen, Dan Massey
 *  Date: Jun 8, 2008 
 */

#ifndef LOG_H_
#define LOG_H_
#include <stdarg.h>
#include <stdio.h>

static const char * const log_level_string[] = {
   "[LOG_EMERG]",  	//0
   "[LOG_ALERT]",	//1	
   "[LOG_CRIT]",	//2
   "[LOG_ERR]",		//3	
   "[LOG_WARNING]",	//4
   "[LOG_NOTICE]",	//5
   "[LOG_INFO]",	//6
   "[LOG_DEBUG]"	//7
};

static const char * const log_facility[] = {
   "[LOG_AUTH]",           //0
   "[LOG_AUTHPRIV]",       //1
   "[LOG_CONSOLE]",        //2
   "[LOG_CRON]",           //3
   "[LOG_DAEMON]",         //4
   "[LOG_FTP]",            //5
   "[LOG_KERN]",           //6
   "[LOG_LPR]",            //7
   "[LOG_MAIL]",           //8
   "[LOG_NEWS]",           //9
   "[LOG_SECURITY]",       //10
   "[LOG_SYSLOG]",         //11
   "[LOG_USER]",           //12
   "[LOG_UUCP]",           //13
   "[LOG_LOCAL0]"          //14
};

/* Initialize log function environment */
int init_log ();
void Vsyslog (int, const char *, va_list);
void log_fatal (const char *, ... );
void log_err (const char *, ... );
void log_warning (const char *, ... );
void log_msg (const char *, ... );
void debug (const char *, const char *, ...);
void hexdump(int, const void *, int);

#endif /*LOG_H_*/

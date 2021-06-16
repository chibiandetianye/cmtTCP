#ifndef _DEBUG_INCLUDE_H_
#define _DEBUG_INCLUDE_H_

#include<stdio.h>


#ifdef DEBUG
#define trace__tcp_error(f, cpu, tid, m...)	\
	fprintf(f, "ERROR [CPU %d thread %d][%10s: %4d] ",	\
			cpu, tid, __FUNCTION__, __LINE__, m		\
)

#define trace_tcp_warning(f, cpu, tid, m...)	\
fprintf(f, "WARNING [CPU %d thread %d][%10s: %4d] ",	\
			cpu, tid, __FUNCTION__, __LINE__, m		\
)

#define trace_tcp_message(f, cpu, tid, m...)	\
fprintf(f, "MESSAGE [CPU %d thread %d][%10s: %4d]",	\
		cpu, tid, __FUNCTION__, __LINE__, m	\
)

#define trace_error(f, m...)	\
	fprintf(f, "ERROR [%10s: %4d] ",	\
			__FUNCTION__, __LINE__, m)

#define trace_warning(f, m...)	\
	fprintf(f, "WARNING [%10s: %4d] ",	\
			__FUNCTION__, __LINE__, m)

#define trace_message(f, m...)	\
	fprintf(f, "MESSAGE [%10s: %4d] ",	\
			__FUNCTION__, __LINE__, m)
#else 
#define trace_tcp_error(f, cpu, tid, m...)	\
do{} while(0)
#define trace_tcp_warning(f, cpu, tid, m...)	\
do{} while(0)
#define trace_tcp_message(f, cpu, tid, m...)	\
do{} while(0)
#define trace_error(f, cpu, tid, m...)	\
do{} while(0)
#define trace_warning(f, cpu, tid, m...)	\
do{} while(0)
#define trace_message(f, cpu, tid, m...)	\
do{} while(0)
#endif /** DEBUG */

#endif /** _DEBUG_INCLUDE_H_ */
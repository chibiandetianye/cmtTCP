#ifndef _DEBUG_INCLUDE_H_
#define _DEBUG_INCLUDE_H_

#include<stdio.h>


#ifdef DEBUG
#define trace_error(f, cpu, tid, m...)	\
	fprintf(f, "ERROR [CPU %d thread %d][%10s: %4d] ",	\
			cpu, tid, __FUNCTION__, __LINE__, m		\
)

#define trace_warning(f, cpu, tid, m...)	\
fprintf(f, "WARNING [CPU %d thread %d][%10s: %4d] ",	\
			cpu, tid, __FUNCTION__, __LINE__, m		\
)

#define trace_message(f, cpu, tid, m...)	\
fprintf(f, "MESSAGE [CPU %d thread %d][%10s: %4d]",	\
		cpu, tid, __FUNCTION__, __LINE__, m	\
)

#else 
#define trace_error(f, cpu, tid, m...)	\
do{} while(0)
#define trace_warning(f, cpu, tid, m...)	\
do{} while(0)
#define trace_message(f, cpu, tid, m...)	\
do{} while(0)
#endif /** DEBUG */

#endif /** _DEBUG_INCLUDE_H_ */
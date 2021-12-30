#if !defined	( ____DEBUG_TRACE___H__ )
#define			____DEBUG_TRACE___H__

//evk
extern void TRACE( char *mess, ... );
extern void TRACE_BUFFER( char *mess, BYTE *data, WORD size );

//#define __DEBUG_TRACE__
//-----------------------------------------------
#ifdef __DEBUG_TRACE__

#define DebugTRACE 		TRACE
#define DebugTRACEbuf 	TRACE_BUFFER

//-----------------------------------------------
#else
	
#define DebugTRACE
#define DebugTRACEbuf

#endif
//-----------------------------------------------

#endif //____DEBUG_TRACE___H__

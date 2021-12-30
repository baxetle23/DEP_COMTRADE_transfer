//evk

/**********************************************************************************************************************/
void TRACE( char *mess, ... )
{
	va_list arg;
	va_start(arg,mess);
	TracePrintfDateTime("COMTRADE: ");
	TraceVPrintf( mess, arg );
	va_end(arg);
}
/**********************************************************************************************************************/
void TRACE_BUFFER( char *mess, BYTE *data, WORD size )
{
	TracePrintfDateTime("COMTRADE: ");
	TracePrintfBuffer(mess, data, size);
}
/**********************************************************************************************************************/

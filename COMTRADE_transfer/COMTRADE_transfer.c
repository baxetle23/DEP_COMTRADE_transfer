
////////////////////////////////////////////////////////////////////////////////
//   DeCONT: Задача "Вычитывание архивов COMTRADE"
////////////////////////////////////////////////////////////////////////////////


#include <zilog.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include "curl/curl.h"
#include "curl/easy.h"

#include <uso.h>
#include "COMTRADE_transfer_TraceLogger.h"


#include "COMTRADE_transfer.ver"
#include "COMTRADE_transfer.h"
#include "COMTRADE_transfer_trace.h"
#include "COMTRADE_transfer_trace.c"



size_t 	sum_writefile;
size_t 	sum_getfile;

//---------------------------------------------------------------------------
// Чтение конфигурации
//---------------------------------------------------------------------------
static void ReadConfig (void)
{
	TRemoteHost *host;
	TArchive    *archive;
	int er;
	DWORD  i;
	char path[22 + IP_LENGTH];

	if (!OKAY(TableFromBag(&MAIN->Hosts, CFG_CONNECTION, sizeof(TRemoteHost), sizeof(TRemoteHost_cfg)))) {
		ObjFatalError(RES_ReadCfg, CFG_CONNECTION);
	}

	// Создание директорий
	er = mkdir(LOCAL_PATH, S_IRWXU | S_IRWXG | S_IRWXO);

	log().WriteToLog(__FUNCTION__, "Create Dir, local_path = ", LOCAL_PATH); // alpo

	if (er && errno != EEXIST) {
		log().WriteToLog(__FUNCTION__, " Could not create dir, local_path = ", LOCAL_PATH); // alpo
		TRACE( "%s. Could not create dir %s\n", __FUNCTION__, LOCAL_PATH);
		ObjFatalError(RES_LLC_INTERNAL, 1);
	}

	TableIteratorX(&MAIN->Hosts, i, host, TRemoteHost*) {
	// Копирование введённых строк в нультерминированные
		strncpy(host->HostIP, host->cfg.HostIP, IP_LENGTH);
		strncpy(host->Login, host->cfg.Login, LOGIN_LENGTH);
		strncpy(host->Passw, host->cfg.Passw, PASSW_LENGTH);

		log().WriteToLog(__FUNCTION__, "HOST IP = ", host->HostIP, "HOST Login = ", host->Login, "HOST Password = ", host->Passw); // alpo

		er = snprintf(path, sizeof(path), "%s%s", LOCAL_PATH, host->HostIP);
	if (er >= sizeof(path)) {
		log().WriteToLog(__FUNCTION__, "Error! IP length overflow!"); // alpo
		TRACE( "%s. IP length overflow!\n", __FUNCTION__);
	}
	er = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);

	log().WriteToLog(__FUNCTION__, "Create Dir, path = ", path); // alpo

	if (er && errno != EEXIST) {
		log().WriteToLog(__FUNCTION__, "Could not create dir, path = ", path); // alpo
		TRACE( "%s. Could not create dir %s\n", __FUNCTION__, path);
		ObjFatalError(RES_LLC_INTERNAL, 1);
	}
		host->Connection = FALSE;
	}

	if (!OKAY(BlockFromBag(&MAIN->PeriodConnect, TaskObject, CFG_PERIOD, sizeof(DWORD)))) {  // чтение периода опроса
		log().WriteToLog(__FUNCTION__, "Error! Read PeriodConnnect"); // alpo
		ObjFatalError(RES_ReadCfg, CFG_PERIOD);
	}

	if (MAIN->PeriodConnect > 0) {
		MAIN->PeriodConnect *= 1000;
	} else {
		MAIN->PeriodConnect = 60000;  // 60000мс  = 1 мин
	}   

	log().WriteToLog(__FUNCTION__, "Period Connect = ", MAIN->PeriodConnect); // alpo

	if (!OKAY(TableFromBag(&MAIN->Archive, CFG_ARCHIVE, sizeof(TArchive), sizeof(TArchive_cfg)))) { // чтение периода опроса
		ObjFatalError(RES_ReadCfg, CFG_ARCHIVE);
	}

	archive = (TArchive*)MAIN->Archive.First;

	MAIN->InputBuff = NULL;
	MAIN->InputSize = 0;
	MAIN->LastReadTime = 0;
	MAIN->FirstLaunch = TRUE;
	archive->LastPurgeTime = 0;
	archive->PeriodPurge = archive->cfg.PeriodPurge * SECONDS_IN_DAY;

	log().WriteToLog(__FUNCTION__, "PeriodPurge = ",archive->PeriodPurge, "LastPurgeTime = ", archive->LastPurgeTime); // alpo
}

//------------------------------------------------------------------
//  Конвертация недопустимых символов
//------------------------------------------------------------------
#define   ASCCII_CODE_A   0xC0
#define   ASCII_CODE_YA   0xFF
#define   PERCENT         '%'
static void ConvertIllegalCharacters(char *OriginalString, char * ResultString)
{
  DWORD i,j;
  int er;
  
  i = 0;
  j = 0;
  while (OriginalString[i] != 0)
  {
    switch (OriginalString[i])
    {
      case '#':
      case ' ':
      case '%':
      case '!':
      case '&':
      case '?':
      case '[':
      case ']':
      case '{':
      case '}':
      case '|':
      case '*':
      case '^':
      case ':':
      case ';':
      case ',':
        snprintf(&ResultString[j], 256 - j, "%c%02X", PERCENT, OriginalString[i]);
        j += 2;
        if (!(OriginalString[i] & 0x0F))
          ResultString[j] = '0';
        break;
      default:
        if ((OriginalString[i] >= ASCCII_CODE_A) && (OriginalString[i] <= ASCII_CODE_YA))
        {
          snprintf(&ResultString[j], 256 - j, "%c%02X", PERCENT, OriginalString[i]);
          j += 2;
          if (!(OriginalString[i] & 0x0F))
            ResultString[j] = '0';
        }
        else
          ResultString[j] = OriginalString[i];
        break;
    }
    
    i++;
    j++;
  }
  
  ResultString[j] = 0;
}

//------------------------------------------------------------------
//  Запись полученного файла
//------------------------------------------------------------------
static size_t WriteFile (void *buffer, size_t size, size_t nmemb, void *filename)
{
	size_t a;
	int out;
  
	if (MAIN->SucessiveCall) {
		log().WriteToLog(__FUNCTION__, "open with O_APPEND filename = ", (char *)(filename)); //alpo
		out = open((char*)filename, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	} else {
		log().WriteToLog(__FUNCTION__, "open with O_CREAT | O_TRUNC filename = ", (char *)(filename)); //alpo
		out = open((char*)filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	}

  
	if(!out){
		log().WriteToLog(__FUNCTION__, "file not open"); //alpo
		TRACE( "%s. open(%s) ---> %s\n", __FUNCTION__, (char*)filename, !out?"Error":"OK" ); //evk
		return -1; // failure, can't open file to write
	}
	
	a = write(out, buffer, size * nmemb);
	close(out);
	MAIN->SucessiveCall = TRUE;
	sum_writefile +=a;
	return a;
}

//------------------------------------------------------------------
//  Проверка обновления файла на сервере
//------------------------------------------------------------------
static bool CheckFile (char* filename)
{
  DWORD file_length, time;
  TRemoteHost *host;
  FILE *out;
  struct stat  fileinfo;
  double length;
  
  // Проверка существования файла
  out = fopen(filename, "r");
  if(!out)
  {
	  TRACE( "%s. file=%s ---> no exists\n", __FUNCTION__, filename ); //evk 2019-11-06
    return TRUE; // такого файла нет
  }
  // Сравнение размеров файлов
  host = MAIN->CurHost;
  curl_easy_getinfo(host->ConectionHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length);
  fseek(out, 0, SEEK_END);
  file_length = ftell(out);
  fclose(out);
  TRACE( "%s. file=%s ---> length=%d=%d\n", __FUNCTION__, filename, file_length, length ); //evk 2019-11-06
  if (file_length != length)
    return TRUE;
  
  /*
  // Проверка даты изменения
  curl_easy_getinfo(host->ConectionHandle, CURLINFO_FILETIME, &time);
  if (time > MAIN->TempTime)
    return TRUE;
  */
  
  return FALSE;
}

//------------------------------------------------------------------
//  Чистка архива
//------------------------------------------------------------------
static int PurgeArchive (DWORD PeriodPurge)
{
  DIR *dir;
  struct dirent *CurFile;
  struct stat  fileinfo;
  char   path[sizeof(LOCAL_PATH) + 16];
  char   FileName[sizeof(path) + 256];
  DWORD  i;
  TRemoteHost *host;
  int   length;
  TTime time;

   TRACE( "%s.\n", __FUNCTION__); //evk 2019-11-05
   TableIteratorX(&MAIN->Hosts, i, host, TRemoteHost*) {
    length = snprintf(path, sizeof(path), "%s%s", LOCAL_PATH, host->HostIP);
    if (length == -1)
      return -1;
    
    dir = opendir(path);
	TRACE( "%s. Open dir %s\n", __FUNCTION__, path); //evk 2019-11-05
    if (!dir)
    {
      TRACE( "%s. Could not open dir %s\n", __FUNCTION__, path);
      ObjFatalError(RES_LLC_INTERNAL, 4);
    }
    TimeGet(&time);
    time.Second += SECOND_DELTA_1970_1980;
    while(CurFile = readdir(dir))
    {
      if(stat(CurFile->d_name, &fileinfo))
        return -1;
      
      snprintf(FileName, sizeof(FileName),"%s/%s", path, CurFile->d_name);
      if ((time.Second > fileinfo.st_mtime) && (time.Second - fileinfo.st_mtime > PeriodPurge))
	  {
		TRACE( "%s. remove %s\n", __FUNCTION__, FileName); //evk 2019-11-05
        remove(FileName);
	  }
    }
  }
  
  closedir(dir);
  return 0;
}

//------------------------------------------------------------------
//  Получение инфрмации о файле
//------------------------------------------------------------------
static size_t Get_Info (void *buffer, size_t size, size_t nmemb, void *filename)
{
	return size * nmemb;
}

//------------------------------------------------------------------
//  Скачивание файла
//------------------------------------------------------------------
static void Get_File (char *filename)
{
	char  tempfile[256];    // локальный путь
	char  ftpfile[256];
	char  argv[256];       // удалённый путь
	char  DeleteLine[256];
	char  CorrectedFilename[256];
	CURLcode res;
	TRemoteHost *host;
	DWORD   time;   // время создания удалённого файла
	struct  utimbuf times;
	int   er;
	int   er1, er2; //evk
	FILE *out;
	curl_slist *headerlist = NULL;
	struct stat	filestat; int retstat; //evk 2019-11-06
  
	er1 = 0;
  
	host = MAIN->CurHost;

	log().WriteToLog(__FUNCTION__, "Get File start, file arrive name = ", filename); //alpo
	
	ConvertIllegalCharacters(filename, CorrectedFilename);
	sum_writefile=0;
	sum_getfile=0;

	log().WriteToLog(__FUNCTION__, "file name after convert = ", filename); //alpo

	// Заполнение адреса сервера и пути к файлам
	er1 = snprintf(argv, sizeof(argv), "ftp://%s%s%s", host->HostIP, REMOTE_PATH, CorrectedFilename);

	log().WriteToLog(__FUNCTION__, "Remote filepath: = ", argv); //alpo

	if (er1 >= sizeof(argv)) {
		log().WriteToLog(__FUNCTION__, "Remote filepath length overflow ---> do nothing"); //alpo
		TRACE( "%s. Remote filepath length overflow ---> do nothing\n", __FUNCTION__); //evk
		return;
	}
	if (er1 == -1) {
		log().WriteToLog(__FUNCTION__, "Remote filepath length -1 ---> do nothing"); //alpo
		TRACE( "%s. Remote filepath length -1 ---> do nothing\n", __FUNCTION__); //evk
		return;
	}  
	er2 = snprintf(tempfile, sizeof(tempfile), "%s%s/%s.tmp", LOCAL_PATH, host->HostIP, filename);
		log().WriteToLog(__FUNCTION__, "local tempfile = ", tempfile); //alpo
	if (er2 >= sizeof(tempfile)) {
		log().WriteToLog(__FUNCTION__, "Local tempfile filepath: length overflow ---> do nothing"); //alpo
		TRACE( "%s. Local tempfile filepath: length overflow ---> do nothing\n", __FUNCTION__); //evk
		return;
	}
	if (er2 == -1) {
		log().WriteToLog(__FUNCTION__, "Local tempfile filepath: length -1 ---> do nothing"); //alpo
		TRACE( "%s. Local tempfile filepath: length -1 ---> do nothing\n", __FUNCTION__); //evk
		return;
	}
  
	strncpy(ftpfile, tempfile, er2);  // имя локального файла
	ftpfile[er2-4] = 0;
	log().WriteToLog(__FUNCTION__, "Local ftp file = ", ftpfile); //alpo
	
 
	// параметры для скачивания файлов
	curl_easy_setopt(host->ConectionHandle, CURLOPT_URL, argv);  
	curl_easy_setopt(host->ConectionHandle, CURLOPT_FTPLISTONLY, 0);    // Запрос только списка файлов в директории отключить
	curl_easy_setopt(host->ConectionHandle, CURLOPT_WRITEDATA, tempfile);
  
	if (host->Auth) {   // После успешного скачивания файл удаляется на сервере, анонимные пользователи не могут удалять файлы
		snprintf(DeleteLine, sizeof(DeleteLine), "DELE %s", CorrectedFilename); //alpo
		log().WriteToLog(__FUNCTION__, "DeleteLine ", DeleteLine); //alpo
		headerlist = curl_slist_append(headerlist, DeleteLine);
		if (headerlist == NULL) { //evk 
			log().WriteToLog(__FUNCTION__, "headerlist=NULL ---> no delete file on server", DeleteLine); //alpo
			TRACE( "%s. headerlist=NULL ---> no delete file on server\n", __FUNCTION__); //evk
		} else {
			log().WriteToLog(__FUNCTION__, "delete file on server, path", DeleteLine); //alpo
			curl_easy_setopt(host->ConectionHandle, CURLOPT_POSTQUOTE, headerlist);
		}
	} else { // Выборка только новых/изменённых файлов при анонимном доступе 
		curl_easy_setopt(host->ConectionHandle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_TIMEVALUE, MAIN->TempTime);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_FILETIME, 1);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_NOBODY, 1);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_WRITEDATA, tempfile);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_WRITEFUNCTION, Get_Info);
    
		res = curl_easy_perform(host->ConectionHandle); // Выполнение операции
		curl_easy_setopt(host->ConectionHandle, CURLOPT_NOBODY, 0);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_FILETIME, 0);
		
		if (res != CURLE_OK || !CheckFile(ftpfile)) {
			log().WriteToLog(__FUNCTION__, "CheckFile=0 ---> do nothing"); //alpo
			TRACE( "%s. res=0x%0X, CheckFile=0 ---> do nothing\n", __FUNCTION__, res ); //evk
			return;  
		}
	}
  
	// скачивание файлов
	curl_easy_setopt(host->ConectionHandle, CURLOPT_WRITEFUNCTION, WriteFile);

	res = curl_easy_perform(host->ConectionHandle); // Выполнение операции
	retstat=stat(tempfile, &filestat); //evk 2019-11-06
	DebugTRACE( "%s. ftp: res=0x%0X, %lu bytes ---> %s, size=%lu bytes\n", __FUNCTION__, res, sum_writefile, retstat==-1?"no file":"file created", retstat==-1?0:filestat.st_size ); //evk
	log().WriteToLog(__FUNCTION__, "ftp: res=0x", res, "bytes = ", sum_writefile, "restat = ", retstat); //alpo
	// Обработка скачанных файлов
	if (res == CURLE_OK)
	{
//		if (rename(tempfile, ftpfile))   // переименование временного файла
//		TRACE( "%s. temp renaming failed\n", __FUNCTION__);
		retstat=rename(tempfile, ftpfile);   //evk 2019-11-11 // переименование временного файла
#ifndef __DEBUG_TRACE__
		if (retstat)
#endif
			TRACE( "%s. rename(tempfile, ftpfile) ---> %s\n", __FUNCTION__, retstat?"failed":"OK" ); //evk
	}
	else
	{
		if (!access(tempfile, W_OK))
		{
//			if(remove(tempfile))     // удаление временного файла
//				TRACE( "%s. deleting temp file %s failed error %s\n", __FUNCTION__, tempfile, strerror(errno));
			retstat=remove(tempfile);     // удаление временного файла
#ifndef __DEBUG_TRACE__
			if (retstat)
#endif
				TRACE( "%s. tempfile remove ---> %s\n", __FUNCTION__, retstat?"failed":"OK");
		}
		else
		{
			TRACE( "%s. tempfile access ---> no W_OK\n", __FUNCTION__);
		}
	}

	if(!host->Auth && res == CURLE_OK) {
		curl_easy_getinfo(host->ConectionHandle, CURLINFO_FILETIME, &time); // Запоминание времени последнего считывания файлов
		times.actime = time;
		times.modtime = time;
		utime(ftpfile, &times);
		if (time > host->LastReadTime)
		host->LastReadTime = time;
	}
  
	if (host->Auth) {
		curl_slist_free_all(headerlist);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_POSTQUOTE, NULL);
	}
  
	MAIN->SucessiveCall = FALSE;
}

//------------------------------------------------------------------
//  Удаление старых временных файлов
//------------------------------------------------------------------
static void DeletTempFile (char *filename)
{
	char  argv[256];       // удалённый путь
	char  DeleteLine[256];
	char  CorrectedFilename[256];
	curl_slist *headerlist = NULL;
	TRemoteHost *host;
	int er;
	long res;
	TTime time;
	TArchive  *archive;

	log().WriteToLog(__FUNCTION__, "DeleteTempFile start, file name = ", filename); //alpo

	archive = (TArchive*) MAIN->Archive.First;
	if (!archive->PeriodPurge) {
		return;
	}

	host = MAIN->CurHost;
	ConvertIllegalCharacters(filename, CorrectedFilename);

	// Заполнение адреса сервера и пути к файлам
	er = snprintf(argv, sizeof(argv), "ftp://%s%s/%s", host->HostIP, REMOTE_PATH, CorrectedFilename);
	if (er >= sizeof(argv)) {
		return;
	}

	if (er == -1) {
		return;
	} 

	log().WriteToLog(__FUNCTION__, "addres servera and path = ", argv); //alpo

	TimeGet(&time);

	curl_easy_setopt(host->ConectionHandle, CURLOPT_URL, argv);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_FTPLISTONLY, 0);    // Запрос только списка файлов в директории отключить
	curl_easy_setopt(host->ConectionHandle, CURLOPT_NOBODY, 1);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFUNMODSINCE);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_TIMEVALUE, time.Second + SECOND_DELTA_1970_1980 - archive->PeriodPurge);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_FILETIME, 1);

	if (curl_easy_perform(host->ConectionHandle) != CURLE_OK) { // Выполнение операции
		return;
	}

	curl_easy_getinfo(host->ConectionHandle, CURLINFO_CONDITION_UNMET, &res);
	if (res) {
		return;
	}

	snprintf(DeleteLine, sizeof(DeleteLine), "DELE %s", filename);
	headerlist = curl_slist_append(headerlist, DeleteLine);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_QUOTE, headerlist);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_FILETIME, 0);

	log().WriteToLog(__FUNCTION__, "request for delete file = ", DeleteLine); //alpo

	curl_easy_perform(host->ConectionHandle); //отправляем запрос на удаление файла

	curl_slist_free_all(headerlist);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_QUOTE, NULL);
	curl_easy_setopt(host->ConectionHandle, CURLOPT_NOBODY, 0);
}

//------------------------------------------------------------------
//  Поиск нужных файлов в листинге директории
//------------------------------------------------------------------
#define TEMP_EXTENSION ".tmp"
#define CONFIG_EXTENSION ".cfg"
#define DATA_EXTENSION ".dat"
static void ParseListing (char *HOST_IP) {
	char file[256];
	DWORD  i, j;
	DWORD  name_length;   // длина имени файла

	log().WriteToLog(__FUNCTION__, "ParseListing start"); //alpo

	if (!MAIN->CurHost->Auth) {
		MAIN->TempTime = MAIN->CurHost->LastReadTime;   // Установка текущего времени считывания
	}
	MAIN->SucessiveCall = FALSE; 

	j = 0;
	for(i=0; i < MAIN->CurLength; i++) {
		if ((MAIN->InputBuff[i] == '\n') || (MAIN->InputBuff[i] == 0)) {
			name_length = i - j;
			if ((name_length >= 5) && ( !memcmp(&MAIN->InputBuff[i-4], CONFIG_EXTENSION, 4) || ( !memcmp(&MAIN->InputBuff[i-4], DATA_EXTENSION, 4)) ) ) { //Это наш архив
				memcpy(&file[0], &MAIN->InputBuff[j], name_length);
				file[name_length] = 0;
				log().WriteToLog(__FUNCTION__, "File = ", file); //alpo
				Get_File(file);
			} else if (MAIN->CurHost->Auth && (name_length >= 5) && ( !memcmp(&MAIN->InputBuff[i-4], TEMP_EXTENSION, 4) )) {
				memcpy(&file[0], &MAIN->InputBuff[j], name_length);
				file[name_length] = 0;
				log().WriteToLog(__FUNCTION__, "Temp File = ", file); //alpo
				DeletTempFile(file);
			}
			j = i + 1;
		}	
	}

	//starts script for sort load files
	FILE *file_script;
	char comand[512];
	snprintf(comand, sizeof(comand), "%s %s%s", PATH_SCRIPT, LOCAL_PATH, HOST_IP); // add home script add to define
	file_script = popen(comand, "w");
	if (file_script == NULL) {
		log().WriteToLog(__FUNCTION__, "SORT SCRIPT dont start, comand to bash = ", comand);
	} else {
		log().WriteToLog(__FUNCTION__, "SORT SCRIPT starts, comand to bash = ", comand);
		pclose(file_script);
	}
	//------------

}

//------------------------------------------------------------------
//  Получение списка файлов в директории
//------------------------------------------------------------------
static size_t GetListing (void *Buffer, size_t size, size_t nmemb, char *List)
{
	DWORD length;

	log().WriteToLog(__FUNCTION__, "GetListing start"); //alpo
	length = size * nmemb;
	if ((DWORD) -1 - length < MAIN->CurLength) {
		TRACE( "%s. Input buffer overflow\n", __FUNCTION__);
		return 0;
	}

	if (length + MAIN->CurLength > MAIN->InputSize) {
		MAIN->InputSize += 16000;
		MAIN->InputBuff = (BYTE*) realloc(MAIN->InputBuff, MAIN->InputSize);
		if (!MAIN->InputBuff) {
			TRACE( "%s. Input buffer overflow\n", __FUNCTION__);
			return 0;
		}
	}

	memcpy(&MAIN->InputBuff[MAIN->CurLength], Buffer, length);
	MAIN->CurLength += length;
	MAIN->SucessiveCall = TRUE;

	log().WriteToLog(__FUNCTION__, "GET LIST FILE FROM US - ", Buffer);

	return length;
}

//------------------------------------------------------------------
//  Установление дискрета связи
//------------------------------------------------------------------
static void SetConnectionHost(TRemoteHost *host, DWORD Status)
{
  	log().WriteToLog(__FUNCTION__, "Start"); //alpo
  if (host->cfg.NoDiscretConnect && (host->Connection != Status))
  {
    if (OKAY(Status))
      DiscretSet(host->cfg.NoDiscretConnect, 1);
    else
      switch (Status)
      {
        case CURLE_UNSUPPORTED_PROTOCOL:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_UNSUPPORTED_PROTOCOL));
          break;
        case CURLE_FAILED_INIT:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FAILED_INIT));
          break;
        case CURLE_URL_MALFORMAT:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_URL_MALFORMAT));
          break;
        case CURLE_COULDNT_RESOLVE_HOST:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_COULDNT_RESOLVE_HOST));
          break;
        case CURLE_COULDNT_CONNECT:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_COULDNT_CONNECT));
          break;
        case CURLE_FTP_WEIRD_SERVER_REPLY:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_WEIRD_SERVER_REPLY));
          break;
        case CURLE_REMOTE_ACCESS_DENIED:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_REMOTE_ACCESS_DENIED));
          break;
        case CURLE_FTP_ACCEPT_FAILED:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_ACCEPT_FAILED));
          break;
        case CURLE_FTP_WEIRD_PASS_REPLY:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_WEIRD_PASS_REPLY));
          break;
        case CURLE_FTP_ACCEPT_TIMEOUT:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_ACCEPT_TIMEOUT));
          break;
        case CURLE_FTP_WEIRD_PASV_REPLY:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_WEIRD_PASV_REPLY));
          break;
        case CURLE_FTP_COULDNT_SET_TYPE:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_COULDNT_SET_TYPE));
          break;
        case CURLE_PARTIAL_FILE:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_PARTIAL_FILE));
          break;
        case CURLE_FTP_COULDNT_RETR_FILE:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_COULDNT_RETR_FILE));
          break;
        case CURLE_OUT_OF_MEMORY:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_OUT_OF_MEMORY));
          break;
        case CURLE_OPERATION_TIMEDOUT:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_OPERATION_TIMEDOUT));
          break;
        case CURLE_FTP_PORT_FAILED:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_PORT_FAILED));
          break;
        case CURLE_FTP_COULDNT_USE_REST:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_CURLE_FTP_COULDNT_USE_REST));
          break;
        default:
          DiscretSet(host->cfg.NoDiscretConnect, SETERR(RES_TIMEOUT));
          break;
      }
      
    host->Connection = Status;
  }

	log().WriteToLog(__FUNCTION__, "host->Connection = ", host->Connection); //alpo
	DebugTRACE( "%s. host->Connection=%d\n", __FUNCTION__, host->Connection ); //evk 2019-11-06
}

//------------------------------------------------------------------
//  Установление соединения
//------------------------------------------------------------------
static void Connect(TRemoteHost *host)
{
	char  argv[44];
	char  argv2[LOGIN_LENGTH + PASSW_LENGTH + 2];
	CURLcode res;
	DWORD SpaceLeft;
	TArchive_cfg  *archive;
	FILE  *ErrorLog;  // файл с логом


  	log().WriteToLog(__FUNCTION__, "Connect"); //alpo


	archive = (TArchive_cfg*) MAIN->Archive.First;

	if (!host->ConectionHandle) {
		host->ConectionHandle = curl_easy_init();
	}

	#ifdef __TESTED__
		ErrorLog = fopen(LOG_PATH, "a");
		MAIN->ConCount++;
	#endif

	if (host->ConectionHandle) {
		SpaceLeft = GetParamFileSystem(GET_FS_SPACE_FREE_USER);
		SpaceLeft >>= 20;
		if (SpaceLeft < archive->MinDiscSpaceReq) {
			SetConnectionHost(host, RES_OutOfFlash);
			return;
		}
		//Заполнение адреса сервера и пути к файлам
		snprintf(argv, sizeof(argv), "ftp://%s%s", host->HostIP, REMOTE_PATH);
		curl_easy_setopt(host->ConectionHandle, CURLOPT_URL, argv);

		// Заполнение логина и пароля при не анонимном доступе
		if (host->Auth || (memcmp(host->Login, "ftp", 3) && host->Login[3] != 0)) {
			snprintf(argv2, sizeof(argv2), "%s:%s", host->Login, host->Passw);
			curl_easy_setopt(host->ConectionHandle, CURLOPT_USERPWD, argv2);
			host->Auth = TRUE;
		}

  		log().WriteToLog(__FUNCTION__, "Adress = ", argv, "Login and Password = ", argv2); //alpo

		curl_easy_setopt(host->ConectionHandle, CURLOPT_FTP_RESPONSE_TIMEOUT, 3);   // Установка таймаута ожидания ответа сервера
		curl_easy_setopt(host->ConectionHandle, CURLOPT_FTPLISTONLY, 1);    // Запрос только списка файлов в директории
		curl_easy_setopt(host->ConectionHandle, CURLOPT_WRITEFUNCTION, GetListing);   // Обработка скачанных файлов

		#ifdef __TESTED__
			curl_easy_setopt(host->ConectionHandle, CURLOPT_STDERR, ErrorLog);  // лог
			curl_easy_setopt(host->ConectionHandle, CURLOPT_VERBOSE, 1);
		#endif

		res = curl_easy_perform(host->ConectionHandle); // Выполнение операции

  		log().WriteToLog(__FUNCTION__, "Execution operation = ", res); //alpo
		if(res == CURLE_OK) {
			SetConnectionHost(host, TRUE);
			ParseListing(host->HostIP);
		} else {
			SetConnectionHost(host, res);
		}

		MAIN->CurLength = 0;
	} else {
  		log().WriteToLog(__FUNCTION__, "Connection handle not created"); //alpo
		TRACE( "%s. Connection handle not created\n", __FUNCTION__);
		ObjFatalError(RES_LLC_INTERNAL, 3);
	}
		#ifdef __TESTED__
			fclose(ErrorLog);
		#endif
}

//------------------------------------------------------------------
//  Установака таймаута
//------------------------------------------------------------------
static DWORD StartReadValues() {
	TRemoteHost *host;
	TArchive  *archive;
	DWORD DeltaTimeRead, DeltaTimePurge, CurTime, PeriodPurge;
	DWORD   i;
	TTime time;

	
	//	TRACE( "%s.\n", __FUNCTION__); //evk 2019-11-05
  	log().WriteToLog(__FUNCTION__, "Start read value -------------------------------------------------------------------"); //alpo

	archive = (TArchive*) MAIN->Archive.First;

	TimeGet(&time);
	CurTime = time.Second;

	// Чистка архива если период истёк
	PeriodPurge = archive->PeriodPurge;
  	log().WriteToLog(__FUNCTION__, "PeriodPurge = ", PeriodPurge); //alpo
	if (PeriodPurge) {
		DeltaTimePurge = CurTime - archive->LastPurgeTime;
  		log().WriteToLog(__FUNCTION__, "DeltaTimePurge = ", DeltaTimePurge); //alpo
		if (DeltaTimePurge > PeriodPurge) {
  			log().WriteToLog(__FUNCTION__, "Get Purge Archiv"); //alpo
			PurgeArchive(PeriodPurge);
			archive->LastPurgeTime = CurTime;
  			log().WriteToLog(__FUNCTION__, "Purge Archiv Complete, LastPurgeTime = ", archive->LastPurgeTime); //alpo
		} else
			DeltaTimePurge = PeriodPurge - DeltaTimePurge;
	}

	DeltaTimeRead = CurTime - MAIN->LastReadTime;

  	log().WriteToLog(__FUNCTION__, "DeltaTimeRead = ", DeltaTimeRead); //alpo

	if (DeltaTimeRead < MAIN->PeriodConnect) {
		DeltaTimeRead = MAIN->PeriodConnect - DeltaTimeRead;
	if (DeltaTimePurge < DeltaTimeRead) {	// возвращаем меньший из двух периодов 
	//		TRACE( "%s. DeltaTimePurge=%d\n", __FUNCTION__, DeltaTimePurge ); //evk 2019-11-06
  		log().WriteToLog(__FUNCTION__, "return value DeltaTimePurge = ", DeltaTimePurge); //alpo
		return DeltaTimePurge;
	}
	else {
	//		TRACE( "%s. DeltaTimeRead=%d\n", __FUNCTION__, DeltaTimeRead ); //evk 2019-11-06
  		log().WriteToLog(__FUNCTION__, "return value DeltaTimeRead = ", DeltaTimeRead); //alpo
		return DeltaTimeRead;
	}
	}

	TableIteratorX(&MAIN->Hosts, i, host, TRemoteHost*) {
		MAIN->CurHost = host;
		Connect(host);
	}

	MAIN->LastReadTime = TimeAxis();
  	log().WriteToLog(__FUNCTION__, "LastReadTime = ", MAIN->LastReadTime, "FirstLaunch = ", MAIN->FirstLaunch, "PeriodPurge = ", PeriodPurge); //alpo
	if (MAIN->FirstLaunch && PeriodPurge) {
  		log().WriteToLog(__FUNCTION__, "Get other Purge Archiv"); //alpo
		PurgeArchive(PeriodPurge);
  		log().WriteToLog(__FUNCTION__, "Purge Archiv Complete"); //alpo
		MAIN->FirstLaunch = FALSE;
	}
	//	TRACE( "%s. PeriodConnect=%d\n", __FUNCTION__, MAIN->PeriodConnect ); //evk 2019-11-06
  	log().WriteToLog(__FUNCTION__, "return value PeriodConnect = ", MAIN->PeriodConnect); //alpo
	return  MAIN->PeriodConnect;
}

//---------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////
//
// З А Д А Ч А    COMTRADE_Transfer
//
static void COMTRADE_Transfer_TaskStart(void)
{
  HMESSAGE  message;
  FILE  *ErrorLog;    // лог соединения

  log().WriteToLog(__FUNCTION__, "TOCHKA VXODA V KOMPANENT"); //alpo

  ReadConfig();

  log().WriteToLog(__FUNCTION__, "ReadConfig complete"); //alpo

  ObjStartComplete();

  log().WriteToLog(__FUNCTION__, "ObjStartComplete"); //alpo

	
//curl_global_init() должен вызываться ровно один раз для каждого приложения,
//которое использует libcurl, и перед любым вызовом других функций libcurl.
  if (curl_global_init(CURL_GLOBAL_NOTHING)) {
    TRACE( "%s. libcurl initialization failed\n", __FUNCTION__);
    ObjFatalError(RES_LLC_INTERNAL, 2);
  }
//curl_global_cleanup () должен вызываться ровно один раз для каждого приложения,
//которое использует libcurl
//	curl_global_cleanup();
//	TRACE( "%s. curl_global_cleanup\n", __FUNCTION__ ); //evk 2019-11-06

  #ifdef __TESTED__
    ErrorLog = fopen(LOG_PATH, "w");
    fclose(ErrorLog);
  #endif
  
//	TRACE( "%s. Ini ---> OK\n", __FUNCTION__ ); //evk 2019-11-06
  while(TRUE) {
    message = MessageWait(HOME, StartReadValues());
//		TRACE( "\n" ); //evk 2019-11-11
//		TRACE( "%s. message=0x%0x\n", __FUNCTION__, message ); //evk 2019-11-06
    if (message) {
      switch (message->command) {
        default:
          InvalidCommand( message );
          ResponseSend  ( message, 0 );
          break;
      } 
    }
    #ifdef __TESTED__
      if (MAIN->ConCount > 5) // очищать лог после 5 соединений
      {
        ErrorLog = fopen(LOG_PATH, "w");
        fclose(ErrorLog);
        MAIN->ConCount = 0;
      }
    #endif
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Паспорт задачи
//
static TTaskForm COMTRADE_Transfer_Task = {
 IDC_COMTRADE_Transfer, // Task ID
 DEFSTACK,         // MinStackSize
 sizeof(*MAIN),    // MinDataSize
 DEFHEAP,          // MinHeapSize
 PRIORITY_BUS,     // Priority
 IDC_COMTRADE_Transfer, // Comp ID
 0,                // Exceptions
 ARGSIZE,          // кол-во аргументов
 COMTRADE_Transfer_TaskStart, // точка входа
 NULL,             // Dispatch
 0                 // Options
};
#undef MAIN

_LOCALENTRY BOOL LibMain( HCTX CodeCtx )
{
    INTERFACE_IMPORT();
    ExportTaskName(&COMTRADE_Transfer_Task,CodeCtx,"COMTRADE_Transfer");
    return TRUE;
}

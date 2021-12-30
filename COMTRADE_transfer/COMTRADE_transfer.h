#define   CFG_CONNECTION    1
#define   CFG_PERIOD        2
#define   CFG_ARCHIVE       3

#define   REMOTE_PATH   "//mnt/user/COMTRADE/"
#define   LOCAL_PATH    "/mnt/user/COMTRADE/"
#define   LOG_PATH      "/tmp/COMTRADE_FTP_LOG"

#define   IP_LENGTH     15
#define   LOGIN_LENGTH  8
#define   PASSW_LENGTH  10

//#define   MIN_DISC_SPACE_REQ    20480   // 20 мб

//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
#ifdef __LINUX__
  #pragma pack(1)
#endif

typedef struct {
  char    HostIP[IP_LENGTH];
  char    Login[LOGIN_LENGTH];
  char    Passw[PASSW_LENGTH];
  DWORD   NoDiscretConnect;
} TRemoteHost_cfg;

typedef struct {
  DWORD   PeriodPurge;    // Цикл чистки архива
  DWORD   MinDiscSpaceReq;  // минимальное допустимое место на диске
} TArchive_cfg;

#ifdef __LINUX__
  #pragma pack()
#endif

typedef struct {
  TRemoteHost_cfg   cfg;
  CURL    *ConectionHandle;   // Хэндл соединения
  bool    Auth;               // требуется аутенфикация у сервера (не анонимный доступ)
  char    HostIP[IP_LENGTH + 1];
  char    Login[LOGIN_LENGTH + 1];
  char    Passw[PASSW_LENGTH + 1];
  DWORD   Connection;
  DWORD   LastReadTime;   // Время создания последнего прочитанного архива
} TRemoteHost;

typedef struct {
  TArchive_cfg   cfg;    // Цикл чистки архива
  DWORD          PeriodPurge;
  DWORD          LastPurgeTime;   // Время окончания последней чистки архива
} TArchive;

typedef struct {
  TABLE         Hosts;
  TABLE         Archive;
  
  BYTE          *InputBuff;     // Буфер для хранения списка файлов
  DWORD         InputSize;      // Размер входного буфера
  DWORD         CurLength;      // Длина полученного листинга директории
  BYTEBOOL      SucessiveCall;  // Первая порция данных уже записана
  BYTEBOOL      FirstLaunch;    // Проверка на первый запуск
  
  DWORD         PeriodConnect;   // Цикл опроса
  
  DWORD         LastReadTime;   // Время окончания последнего цикла чтения
  DWORD         TempTime;       // Текущее время данных
  DWORD         CurTime;        // Время создания текущего файла
  TRemoteHost   *CurHost;       // Текущий контроллер
  DWORD         ConCount;
} HEADER;

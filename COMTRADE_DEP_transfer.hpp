#include <iostream>
#include <cstdio>
#include <string>
#include <queue>
#include <memory>

#include <sys/vfs.h>

#define DEBUG 1

#define DELETE_FILES_FROM_US 1
#define SIZE_FREE_SPACE_UArh 500 

namespace comtrade_transfer {

enum class STATUS_FILE {
    EXIST,
    DELETE
};

enum class ERROR_COMTRADE_TRANSFER {
    ERROR_OK,
    ERROR_MOVE,
    ERROR_DELETE,
    ERROR_STORAGE,
};

struct Date {
    int8_t  day;
    int8_t  mounth;
    int16_t year;
};

class File {
private:
    std::string name_;
    std::string path_root_;
    int32_t     size_;
    Date        date_create_;
    STATUS_FILE status = STATUS_FILE::EXIST;

public:
    explicit File(const std::string& name, const std::string& path, int32_t size, Date date_create);
    ERROR_COMTRADE_TRANSFER Delete();
    ERROR_COMTRADE_TRANSFER Moving(const std::string& path_dest);
    ~File();
};

class Storage {
private:
    std::queue<std::unique_ptr<File>>   storage_files_;
    std::string                         storage_path_;
    int64_t                             storage_size_;

    ERROR_COMTRADE_TRANSFER FreeMemory();
public:
    explicit Storage(const std::string& path_root);
    ERROR_COMTRADE_TRANSFER CheckFreeSpace();
    ERROR_COMTRADE_TRANSFER AddNewFile(const std::string& name);
};

} //end comtrade_transfer
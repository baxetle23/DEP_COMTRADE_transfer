#include "COMTRADE_DEP_transfer.hpp"

using namespace comtrade_transfer;

File::File(const std::string& name, const std::string& path, int32_t size, Date date_create) :
    name_(name), 
    path_root_(path), 
    size_(size), 
    date_create_(date_create) {
}

 File::~File() {
 }

ERROR_COMTRADE_TRANSFER File::Delete() {
    if (status == STATUS_FILE::DELETE || remove((path_root_ + name_).data())) {
        #ifdef DEBUG
            std::cerr << (path_root_ + name_) << "NOT deleted\n";
        #endif
        return ERROR_COMTRADE_TRANSFER::ERROR_DELETE;    
    } else {
        #ifdef DEBUG
            std::cerr << (path_root_ + name_) << " deleted\n";
        #endif
        status = STATUS_FILE::DELETE;
        return ERROR_COMTRADE_TRANSFER::ERROR_OK;
    }
}

ERROR_COMTRADE_TRANSFER File::Moving(const std::string& path_dest) {
    if (status == STATUS_FILE::DELETE || rename((path_root_ + name_).data(), (path_dest + name_).data())) {
        #ifdef DEBUG
            std::cerr << (path_root_ + name_) << "NOT move to " << (path_dest + name_) << std::endl;
        #endif
        return ERROR_COMTRADE_TRANSFER::ERROR_MOVE; 
    } else {
        #ifdef DEBUG
            std::cerr << (path_root_ + name_) << "move to " << (path_dest + name_) << std::endl;
        #endif
        return ERROR_COMTRADE_TRANSFER::ERROR_OK;
    }
}

Storage::Storage (const std::string& path) : 
    storage_path_(path),
    storage_size_(0) {
}

ERROR_COMTRADE_TRANSFER Storage::CheckFreeSpace() {
    struct statfs buf;
    if (statfs(storage_path_.data(), &buf)) {
        #ifdef DEBUG
            std::cerr << "Don't determine FS, path = " << storage_path_ << std::endl;
        #endif
        return ERROR_COMTRADE_TRANSFER::ERROR_STORAGE;
    } else {
        int64_t free_space = buf.f_bfree;
        #ifdef DEBUG
            std::cerr << "determine FS complete, path = " << storage_path_ << std::endl;
            std::cerr << "FS has free space = " << free_space << std::endl;
        #endif
        if (free_space < SIZE_FREE_SPACE_UArh) {
            return FreeMemory();
        } else {
            return ERROR_COMTRADE_TRANSFER::ERROR_OK;
        }
    }
}
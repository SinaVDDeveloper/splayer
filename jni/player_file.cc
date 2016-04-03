#include "player_file.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


FileWrapper* FileWrapper::Create()
{
    return new FileWrapper();
}

FileWrapper::FileWrapper()
    : _rwLock(LYH_CreateMutex()),
      _id(-1),
      _open(false),
      _looping(false),
      _readOnly(false),
      _maxSizeInBytes(-1),
      _sizeInBytes(0)
{
    memset(_fileNameUTF8, 0, kMaxFileNameSize);
}

FileWrapper::~FileWrapper()
{
    if (_id != -1)
    {
        close(_id);
    }
	LYH_DestroyMutex(_rwLock);
}

int FileWrapper::CloseFile()
{
    LYH_LockMutex(_rwLock);
	int ret = CloseFileImpl();
	LYH_UnlockMutex(_rwLock);
    return ret;
}

int FileWrapper::Rewind()
{
	int ret = -1;
    LYH_LockMutex(_rwLock);
    if(_looping || !_readOnly)
    {
        if (_id != -1)
        {
			ret = lseek(_id, 0, SEEK_SET);
		   _sizeInBytes = 0;
        }
    }
	LYH_UnlockMutex(_rwLock);
    return ret;
}

int FileWrapper::SetMaxFileSize(size_t bytes)
{
    LYH_LockMutex(_rwLock);
    _maxSizeInBytes = bytes;
	LYH_UnlockMutex(_rwLock);
    return 0;
}

int FileWrapper::Flush()
{
	int ret;
    LYH_LockMutex(_rwLock);
	ret = FlushImpl();
	LYH_UnlockMutex(_rwLock);
    return ret;
}

int FileWrapper::FileName(char* fileNameUTF8,
                              size_t size) const
{
    LYH_LockMutex(_rwLock);
    size_t length = strlen(_fileNameUTF8);
    if(length > FileWrapper::kMaxFileNameSize)
    {
		LYH_UnlockMutex(_rwLock);
        return -1;
    }
    if(length < 1)
    {
		LYH_UnlockMutex(_rwLock);
        return -1;
    }

    // Make sure to NULL terminate
    if(size < length)
    {
        length = size - 1;
    }
    memcpy(fileNameUTF8, _fileNameUTF8, length);
    fileNameUTF8[length] = 0;
	LYH_UnlockMutex(_rwLock);
    return 0;
}

bool FileWrapper::Open() const
{
	bool _isopen;
    LYH_LockMutex(_rwLock);
	_isopen = _open;
	LYH_UnlockMutex(_rwLock);
    return _isopen;
}

int FileWrapper::OpenFile(const char *fileNameUTF8, bool readOnly,
                              bool loop, bool text)
{
    LYH_LockMutex(_rwLock);
    size_t length = strlen(fileNameUTF8);
    if (length > FileWrapper::kMaxFileNameSize - 1)
    {
		LYH_UnlockMutex(_rwLock);
        return -1;
    }

    _readOnly = readOnly;

    int tmpId = -1;
    if(text)
    {
        if(readOnly)
        {
            tmpId = open(fileNameUTF8, O_RDONLY);
        } else {
            tmpId = open(fileNameUTF8, O_RDWR | O_CREAT, 0666);
        }
    } else {
        if(readOnly)
        {
            tmpId = open(fileNameUTF8, O_RDONLY);
        } else {
            tmpId = open(fileNameUTF8, O_RDWR | O_CREAT, 0666); 
        }
    }

    if (tmpId != -1)
    {
        // +1 comes from copying the NULL termination character.
        memcpy(_fileNameUTF8, fileNameUTF8, length + 1);
		
		_sizeInBytes = lseek(tmpId,0,SEEK_END);
		if(-1==_sizeInBytes){
			LYH_UnlockMutex(_rwLock);
			return -1;
		}else{
			//printf("lseek to %ld\n",_sizeInBytes);
		}
        if (_id != -1)
        {
            close(_id);
        }
        _id = tmpId;
        _looping = loop;
        _open = true;
		LYH_UnlockMutex(_rwLock);
        return 0;
    }else{
		//printf("open %s fail,errno=%d\n",fileNameUTF8,errno);
	}
	LYH_UnlockMutex(_rwLock);
    return -1;
}

int FileWrapper::Read(void* buf, int length)
{
    LYH_LockMutex(_rwLock);
    if (length < 0){
		LYH_UnlockMutex(_rwLock);
        return -1;
	}

    if (_id == -1){
		LYH_UnlockMutex(_rwLock);
        return -1;
	}

    int bytes_read = static_cast<int>(read(_id, buf, length));
    if (bytes_read != length && !_looping)
    {
        CloseFileImpl();
    }
	LYH_UnlockMutex(_rwLock);
    return bytes_read;
}

/*
int FileWrapper::WriteText(const char* format, ...)
{
    LYH_LockMutex(_rwLock);
    if (format == NULL){
		LYH_UnlockMutex(_rwLock);
        return -1;
	}

    if (_readOnly){
		LYH_UnlockMutex(_rwLock);
        return -1;
	}

    if (_id == NULL){
		LYH_UnlockMutex(_rwLock);
        return -1;
	}

    va_list args;
    va_start(args, format);
    int num_chars = vfprintf(_id, format, args);
    va_end(args);

    if (num_chars >= 0)
    {
		LYH_UnlockMutex(_rwLock);
        return num_chars;
    }
    else
    {
        CloseFileImpl();
		LYH_UnlockMutex(_rwLock);
        return -1;
    }
}
*/

bool FileWrapper::Write(const void* buf, int length)
{
    LYH_LockMutex(_rwLock);
    if (buf == NULL){
		LYH_UnlockMutex(_rwLock);
        return false;
	}

    if (length < 0){
		LYH_UnlockMutex(_rwLock);
        return false;
	}

    if (_readOnly){
		LYH_UnlockMutex(_rwLock);
        return false;
	}
	
    if (_id == -1){
		LYH_UnlockMutex(_rwLock);
        return false;
	}
    // Check if it's time to clear contents.
    if (_maxSizeInBytes > 0 && (_sizeInBytes + length) > _maxSizeInBytes)
    {
        FlushImpl();
		ReOpenImpl();
		//LYH_UnlockMutex(_rwLock);
        //return false;
    }

    size_t num_bytes = write( _id, buf, length);
    if (num_bytes > 0)
    {
        _sizeInBytes += num_bytes;
		LYH_UnlockMutex(_rwLock);
        return true;
    }

    CloseFileImpl();
	LYH_UnlockMutex(_rwLock);
    return false;
}

int FileWrapper::CloseFileImpl() {
    if (_id != -1)
    {
        close(_id);
        _id = -1;
    }
    memset(_fileNameUTF8, 0, FileWrapper::kMaxFileNameSize);
    _open = false;
    return 0;
}

int FileWrapper::FlushImpl() {
    if (_id != -1)
    {
        return fsync(_id);
    }
    return -1;
}

int FileWrapper::ReOpenImpl()
{
	int ret = -1;
    if(_looping || !_readOnly)
    {
        if (_id != -1)
        {
			close(_id);
			int tmpId;
			if(_readOnly)
			{
				tmpId = open(_fileNameUTF8, O_RDONLY | O_TRUNC);
			} else {
				tmpId = open(_fileNameUTF8, O_RDWR | O_CREAT | O_TRUNC);
			}
			if(tmpId!=-1){
				_id = tmpId;
			}else{
				//printf("reopen fail, errno=%d\n",errno);
			}
			
			ret = lseek(_id, 0, SEEK_SET);
		   
		   _sizeInBytes = 0;
        }
    }
    return ret;
}
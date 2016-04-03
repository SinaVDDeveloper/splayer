#ifndef SPLAYER_FILE_IMPL_H_
#define SPLAYER_FILE_IMPL_H_

#include <stdio.h>
#include "player_helper.h"

class FileWrapper 
{
public:
	static FileWrapper* Create();
     ~FileWrapper();

     int FileName(char* fileNameUTF8,
                         size_t size) const;

    bool Open() const;

    int OpenFile(const char* fileNameUTF8,
                         bool readOnly,
                         bool loop = false,
                         bool text = false);

    int CloseFile();
    int SetMaxFileSize(size_t bytes);
    int Flush();

    int Read(void* buf, int length);
    bool Write(const void *buf, int length);
    //int WriteText(const char* format, ...);
    int Rewind();

public:
	static const int kMaxFileNameSize = 256;
	
private:
	FileWrapper();
    int CloseFileImpl();
    int FlushImpl();
	int ReOpenImpl();

	LYH_Mutex* _rwLock;
	
    int _id;
    bool _open;
    bool _looping;
    bool _readOnly;
    size_t _maxSizeInBytes; // -1 indicates file size limitation is off
    size_t _sizeInBytes;
    char _fileNameUTF8[kMaxFileNameSize];
};


#endif 

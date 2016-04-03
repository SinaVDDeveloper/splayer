#include "player_log.h"

#include <cassert>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <android/log.h>
#include <sys/time.h>


//
STrace* STrace::_static_instance = NULL;
bool STrace::_concole_enable = true;
bool STrace::_file_enable = true;
int STrace::_filter = SPLAYER_TRACE_ERROR;
char STrace::_file_path[SPLAYER_TRACE_MAX_MESSAGE_SIZE] = {0};
//
	
// create a static instance
STrace* STrace::Create()
{
	if(STrace::_static_instance==NULL){
		STrace::_static_instance = new STrace();
	}
	
	return STrace::_static_instance;
}

// destroy a static instance
void STrace::Destroy()
{
	if(STrace::_static_instance!=NULL){
		delete STrace::_static_instance;
		STrace::_static_instance = NULL;
	}
}

// log to console
void STrace::EnableConsole(bool enable)
{
	STrace::_concole_enable = enable;
}

bool STrace::Console()
{
	return STrace::_concole_enable;
}
	
// log to file
void STrace::EnableFile(bool enable)
{
	STrace::_file_enable = enable;
}
bool STrace::File()
{
	return STrace::_file_enable;
}
	
// Specifies what type of messages should be written to the trace file.
int STrace::SetFilter(const int filter)
{
	STrace::_filter = filter;
}

int STrace::Filter()
{
	return STrace::_filter;
}

// Sets the file path. 
int STrace::SetFilePath(const char* filePath)
{
	if(filePath&& strlen(filePath)>0){
		sprintf(STrace::_file_path,"%s",filePath);
		STrace* trace = STrace::Create();
		if(trace)
		{
			int ret = trace->SetFilePathImpl(filePath);
			return ret;
		}
	}
    return -1;
}

char* STrace::FilePath()
{
	return STrace::_file_path;
}
	

void STrace::Log(const int level,const char* msg, ...)

{
    if(level<=STrace::Filter() &&  (STrace::Console() || STrace::File()) ){
		STrace* trace = STrace::Create();
		if(trace){
			char tempBuff[SPLAYER_TRACE_MAX_MESSAGE_SIZE];
			char* buff = 0;
			if(msg)
			{
				va_list args;
				va_start(args, msg);
				vsnprintf(tempBuff,SPLAYER_TRACE_MAX_MESSAGE_SIZE-1,msg,args);
				va_end(args);
				buff = tempBuff;
			}
				
			if(STrace::Console()){
				__android_log_print(ANDROID_LOG_DEBUG, "splayer", "%s",buff);
			}
			if(STrace::File()){
				trace->LogImpl(level, buff);
			}
		}
	}
}


STrace::STrace()
    : _critsectInterface(LYH_CreateMutex()),
      _rowCountText(0),
      _fileCountText(0),
      _traceFile(FileWrapper::Create()),
	  _critsectEvent(LYH_CreateMutex()),
      _event(LYH_CreateCond()), ///////////////
      _critsectArray(LYH_CreateMutex()),
      _nextFreeIdx(),
      _level(),
      _length(),
      _messageQueue(),
      _activeQueue(0),
	  _thread(0),
	  _thread_quit(false)
{
	Start();
	
    _nextFreeIdx[0] = 0;
    _nextFreeIdx[1] = 0;

    unsigned int tid = 0;
 
    for(int m = 0; m < SPLAYER_TRACE_NUM_ARRAY; m++)
    {
        for(int n = 0; n < SPLAYER_TRACE_MAX_QUEUE; n++)
        {
            _messageQueue[m][n] = new
                char[SPLAYER_TRACE_MAX_MESSAGE_SIZE];
        }
    }
	
	struct timeval systemTimeHighRes;
    gettimeofday(&systemTimeHighRes, 0);
    _prevAPITickCount = _prevTickCount = systemTimeHighRes.tv_sec;
}

bool STrace::Start()
{
	_thread = LYH_CreateThread( Run, this);
										 							   
	//_thread.Start(tid);
}

bool STrace::Stop()
{
    // Release the worker thread so that it can flush any lingering messages.
    LYH_SignalCond(_event);

    // Allow 10 ms for pending messages to be flushed out.
    // why not use condition variables to do this? Or let the
    //                 worker thread die and let this thread flush remaining
    //                 messages?
    usleep(10000); //SleepMs(10);

    _thread_quit = true; //_thread.SetNotAlive();
	
    // Make sure the thread finishes as quickly as possible (instead of having
    // to wait for the timeout).
    LYH_SignalCond(_event);
	
    LYH_WaitThread(_thread, NULL); //bool stopped = _thread.Stop();

    LYH_LockMutex(_critsectInterface);
    if(_traceFile)
		_traceFile->Flush();
	if(_traceFile) 
		_traceFile->CloseFile();
	LYH_UnlockMutex(_critsectInterface);
    return true;
}

STrace::~STrace()
{
    Stop();
	
    delete _traceFile;
    //delete &_thread;
	LYH_DestroyCond(_event);
    LYH_DestroyMutex(_critsectInterface);
    LYH_DestroyMutex(_critsectArray);
	LYH_DestroyMutex(_critsectEvent);

    for(int m = 0; m < SPLAYER_TRACE_NUM_ARRAY; m++)
    {
        for(int n = 0; n < SPLAYER_TRACE_MAX_QUEUE; n++)
        {
            delete [] _messageQueue[m][n];
        }
    }
}

int STrace::AddThreadId(char* traceMessage) const {
	int threadId =  static_cast<int>(syscall(__NR_gettid));
	// Messages is 12 characters.
	return sprintf(traceMessage, "%10u ", threadId);
}


int STrace::AddLevel(char* szMessage, const int level) const
{
    switch (level)
    {
        case SPLAYER_TRACE_QUIET:
            sprintf (szMessage, "QUIET   ");
            break;
        case SPLAYER_TRACE_PANIC:
            sprintf (szMessage, "PANIC   ");
            break;
        case SPLAYER_TRACE_FATAL:
            sprintf (szMessage, "FATAL   ");
            break;
        case SPLAYER_TRACE_ERROR:
            sprintf (szMessage, "ERROR   ");
            break;
        case SPLAYER_TRACE_WARNING:
            sprintf (szMessage, "WARNING ");
            break;
        case SPLAYER_TRACE_INFO:
            sprintf (szMessage, "INFO    ");
            break;
        case SPLAYER_TRACE_VERBOSE:
            sprintf (szMessage, "VERBOSE ");
            break;
        case SPLAYER_TRACE_DEBUG:
            sprintf (szMessage, "DEBUG   ");
            break;
        default:
            assert(false);
            return 0;
    }
    // All messages are 12 characters.
    return 8;
}


int STrace::SetFilePathImpl(const char* filePath)
{
    LYH_LockMutex(_critsectInterface);

    if(_traceFile)
		_traceFile->Flush();
    if(_traceFile)
		_traceFile->CloseFile();

    if(filePath)
    {
		char fileName[FileWrapper::kMaxFileNameSize];
		CreateFileName(filePath, fileName);
		if(_traceFile){
			_traceFile->SetMaxFileSize(SPLAYER_TRACE_MAX_FILE_SIZE);
			if(_traceFile->OpenFile(fileName, false, true,false) == -1)
			{
				LYH_UnlockMutex(_critsectInterface);
				return -1;
			}
		}
   
    }
    //_rowCountText = 0;
	LYH_UnlockMutex(_critsectInterface);
    return 0;
}

/*
int STrace::TraceFileImpl(char fileName[FileWrapper::kMaxFileNameSize])
{
    LYH_LockMutex(_critsectInterface);
	int ret = _traceFile->FileName(fileName, FileWrapper::kMaxFileNameSize);
	LYH_UnlockMutex(_critsectInterface);
    return ret;
}
*/

int STrace::AddMessage(
    char* traceMessage,
    const char msg[SPLAYER_TRACE_MAX_MESSAGE_SIZE],
    const int writtenSoFar) const

{
    int length = 0;
    if(writtenSoFar >= SPLAYER_TRACE_MAX_MESSAGE_SIZE)
    {
        return -1;
    }

    length = snprintf(traceMessage,
                      SPLAYER_TRACE_MAX_MESSAGE_SIZE-writtenSoFar-2, "%s",msg);
    if(length < 0 || length > SPLAYER_TRACE_MAX_MESSAGE_SIZE-writtenSoFar - 2)
    {
        length = SPLAYER_TRACE_MAX_MESSAGE_SIZE - writtenSoFar - 2;
        traceMessage[length] = 0;
    }
    // Length with NULL termination.
    return length+1;
}

void STrace::AddMessageToList(
    const char traceMessage[SPLAYER_TRACE_MAX_MESSAGE_SIZE],
    const int length,
    const int level) {

    LYH_LockMutex(_critsectArray);

    if(_nextFreeIdx[_activeQueue] >= SPLAYER_TRACE_MAX_QUEUE)
    {
        if( _traceFile && ! _traceFile->Open() )
        {
            // Keep at least the last 1/4 of old messages when not logging.
            // TODO : isn't this redundant. The user will make it known
            //                 when to start logging. Why keep messages before
            //                 that?
            for(int n = 0; n < SPLAYER_TRACE_MAX_QUEUE/4; n++)
            {
                const int lastQuarterOffset = (3*SPLAYER_TRACE_MAX_QUEUE/4);
                memcpy(_messageQueue[_activeQueue][n],
                       _messageQueue[_activeQueue][n + lastQuarterOffset],
                       SPLAYER_TRACE_MAX_MESSAGE_SIZE);
            }
            _nextFreeIdx[_activeQueue] = SPLAYER_TRACE_MAX_QUEUE/4;
        } else {
            // More messages are being written than there is room for in the
            // buffer. Drop any new messages.
            // TODO: its probably better to drop old messages instead
            //                 of new ones. One step further: if this happens
            //                 it's due to writing faster than what can be
            //                 processed. Maybe modify the filter at this point.
            //                 E.g. turn of STREAM.
			LYH_UnlockMutex(_critsectArray);
            return;
        }
    }

    unsigned short idx = _nextFreeIdx[_activeQueue];
    _nextFreeIdx[_activeQueue]++;

    _level[_activeQueue][idx] = level;
    _length[_activeQueue][idx] = length;
    memcpy(_messageQueue[_activeQueue][idx], traceMessage, length);

    if(_nextFreeIdx[_activeQueue] == SPLAYER_TRACE_MAX_QUEUE-1)
    {
        // Logging more messages than can be worked off. Log a warning.
        const char warning_msg[] = "WARNING MISSING TRACE MESSAGES\n";
        _level[_activeQueue][_nextFreeIdx[_activeQueue]] = SPLAYER_TRACE_WARNING;
        _length[_activeQueue][_nextFreeIdx[_activeQueue]] = strlen(warning_msg);
        memcpy(_messageQueue[_activeQueue][_nextFreeIdx[_activeQueue]],
               warning_msg, _length[_activeQueue][idx]);
        _nextFreeIdx[_activeQueue]++;
    }
	LYH_UnlockMutex(_critsectArray);
}

void* STrace::Run(void* obj)
{
	static_cast<STrace*>(obj)->Process();
    return NULL;
}

bool STrace::Process()
{
	//printf("Process() into\n");
	while(_thread_quit==false){
		if(LYH_TimeWaitCond(_event,_critsectEvent,1000) == 0)
		{
			//printf("Process() catch event\n");
			if(_traceFile && _traceFile->Open())
			{
				// File mode (not calback mode).
				//printf("Process() write\n");
				WriteToFile();
			}
		} else {
			//printf("Process() event timeout\n");
			if(_traceFile){
				_traceFile->Flush();
			}
		}
	}
    return true;
}

void STrace::WriteToFile()
{
    unsigned char localQueueActive = 0;
    unsigned short localNextFreeIdx = 0;

    // There are two buffer. One for reading (for writing to file) and one for
    // writing (for storing new messages). Let new messages be posted to the
    // unused buffer so that the current buffer can be flushed safely.
    {
        LYH_LockMutex(_critsectArray);
        localNextFreeIdx = _nextFreeIdx[_activeQueue];
        _nextFreeIdx[_activeQueue] = 0;
        localQueueActive = _activeQueue;
        if(_activeQueue == 0)
        {
            _activeQueue = 1;
        } else
        {
            _activeQueue = 0;
        }
		LYH_UnlockMutex(_critsectArray);
    }
    if(localNextFreeIdx == 0)
    {
        return;
    }

    LYH_LockMutex(_critsectInterface);
	
	if(_traceFile && _traceFile->Open())
    {
		for(unsigned short idx = 0; idx <localNextFreeIdx; idx++)
		{
			//int localLevel = _level[localQueueActive][idx];
		   
				/*
				if(_rowCountText > SPLAYER_TRACE_MAX_FILE_SIZE)
				{
					// wrap file
					_rowCountText = 0;
					_traceFile.Flush();

					if(_fileCountText == 0)
					{
						_traceFile.Rewind();
					} else
					{
						char oldFileName[FileWrapper::kMaxFileNameSize];
						char newFileName[FileWrapper::kMaxFileNameSize];

						// get current name
						_traceFile.FileName(oldFileName,
											FileWrapper::kMaxFileNameSize);
						_traceFile.CloseFile();

						_fileCountText++;

						UpdateFileName(oldFileName, newFileName, _fileCountText);

						if(_traceFile.OpenFile(newFileName, false, false,
											   true) == -1)
						{
							return;
						}
					}
				}
				if(_rowCountText ==  0)
				{
					char message[SPLAYER_TRACE_MAX_MESSAGE_SIZE + 1];
					WebRtc_Word32 length = AddDateTimeInfo(message);
					if(length != -1)
					{
						message[length] = 0;
						message[length-1] = '\n';
						_traceFile.Write(message, length);
						_rowCountText++;
					}
					length = AddBuildInfo(message);
					if(length != -1)
					{
						message[length+1] = 0;
						message[length] = '\n';
						message[length-1] = '\n';
						_traceFile.Write(message, length+1);
						_rowCountText++;
						_rowCountText++;
					}
				}
				*/
				unsigned short length = _length[localQueueActive][idx];
				_messageQueue[localQueueActive][idx][length] = 0;
				_messageQueue[localQueueActive][idx][length-1] = '\n';
				_traceFile->Write(_messageQueue[localQueueActive][idx], length);
				//_rowCountText++;
			
		}
	}
	LYH_UnlockMutex(_critsectInterface);
}

int STrace::AddTime(char* traceMessage,const int level) const
{
    struct timeval systemTimeHighRes;
    if (gettimeofday(&systemTimeHighRes, 0) == -1)
    {
        return -1;
    }
    struct tm buffer;
    const struct tm* systemTime =
        localtime_r(&systemTimeHighRes.tv_sec, &buffer);

    const unsigned int ms_time = systemTimeHighRes.tv_usec / 1000;
    unsigned int prevTickCount = 0;
    //if (level == kTraceApiCall)
    //{
    //    prevTickCount = _prevTickCount;
    //    _prevTickCount = ms_time;
    //} else {
        prevTickCount = _prevAPITickCount;
        _prevAPITickCount = ms_time;
    //}
    unsigned int dwDeltaTime = ms_time - prevTickCount;
    if (prevTickCount == 0)
    {
        dwDeltaTime = 0;
    }
    if (dwDeltaTime > 0x0fffffff)
    {
        // Either wraparound or data race.
        dwDeltaTime = 0;
    }
    if(dwDeltaTime > 99999)
    {
        dwDeltaTime = 99999;
    }

    //sprintf(traceMessage, "(%2u:%2u:%2u:%3u |%5lu) ", systemTime->tm_hour,
    //        systemTime->tm_min, systemTime->tm_sec, ms_time,
    //        static_cast<unsigned long>(dwDeltaTime));
	sprintf(traceMessage, "%4u-%02u-%02u %02u:%02u:%02u:%3u ", 
			systemTime->tm_year+1900, systemTime->tm_mon, systemTime->tm_mday,
			systemTime->tm_hour,systemTime->tm_min, systemTime->tm_sec, ms_time);
    // Messages are 22 characters.
    return 24;
}


void STrace::LogImpl(const int level, const char msg[SPLAYER_TRACE_MAX_MESSAGE_SIZE])
{
	char traceMessage[SPLAYER_TRACE_MAX_MESSAGE_SIZE];
	char* meassagePtr = traceMessage;

	if(_thread_quit==true){
		return;
	}
	int len = 0;
	int ackLen = 0;

	len = AddLevel(meassagePtr, level);
	if(len == -1)
	{
		return;
	}
	meassagePtr += len;
	ackLen += len;

	len = AddTime(meassagePtr, level);
	if(len == -1)
	{
		return;
	}
	meassagePtr += len;
	ackLen += len;

	//len = AddModuleAndId(meassagePtr, module, id);
	//if(len == -1)
	//{
	//    return;
	//}
	//meassagePtr += len;
	//ackLen += len;

	len = AddThreadId(meassagePtr);
	if(len < 0)
	{
		return;
	}
	meassagePtr += len;
	ackLen += len;

	len = AddMessage(meassagePtr, msg, ackLen);
	if(len == -1)
	{
		return;
	}
	ackLen += len;
	AddMessageToList(traceMessage,ackLen, level);

	// Make sure that messages are written as soon as possible.
	LYH_SignalCond(_event);
	
	//printf("LogImpl() %s\n", msg);
    
}

/*
bool STrace::UpdateFileName(
    const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
    char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
    const WebRtc_UWord32 newCount) const
{
    WebRtc_Word32 length = (WebRtc_Word32)strlen(fileNameUTF8);
    if(length < 0)
    {
        return false;
    }

    WebRtc_Word32 lengthWithoutFileEnding = length-1;
    while(lengthWithoutFileEnding > 0)
    {
        if(fileNameUTF8[lengthWithoutFileEnding] == '.')
        {
            break;
        } else {
            lengthWithoutFileEnding--;
        }
    }
    if(lengthWithoutFileEnding == 0)
    {
        lengthWithoutFileEnding = length;
    }
    WebRtc_Word32 lengthTo_ = lengthWithoutFileEnding - 1;
    while(lengthTo_ > 0)
    {
        if(fileNameUTF8[lengthTo_] == '_')
        {
            break;
        } else {
            lengthTo_--;
        }
    }

    memcpy(fileNameWithCounterUTF8, fileNameUTF8, lengthTo_);
    sprintf(fileNameWithCounterUTF8+lengthTo_, "_%lu%s",
            static_cast<long unsigned int> (newCount),
            fileNameUTF8+lengthWithoutFileEnding);
    return true;
}
*/

bool STrace::CreateFileName(
    const char filePath[FileWrapper::kMaxFileNameSize],
    char fileName[FileWrapper::kMaxFileNameSize]) const
{
    int length = (int)strlen(filePath);
    if(length < 0)
    {
        return false;
    }

    int lengthWithoutFileEnding = length-1;
  
	if(filePath[lengthWithoutFileEnding] == '/')
	{
		sprintf(fileName, "%s%s",filePath,"strace");
	}else
	{
		sprintf(fileName, "%s/%s",filePath,"strace");
	}
	
    return true;
}

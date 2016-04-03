#ifndef SPLAYER_PLAYER_TRACE_H_
#define SPLAYER_PLAYER_TRACE_H_

#include "player_helper.h"
#include "player_file.h"

//trace level 
#define SPLAYER_TRACE_QUIET    -8
#define SPLAYER_TRACE_PANIC     0
#define SPLAYER_TRACE_FATAL     8
#define SPLAYER_TRACE_ERROR    16
#define SPLAYER_TRACE_WARNING  24
#define SPLAYER_TRACE_INFO     32
#define SPLAYER_TRACE_VERBOSE  40
#define SPLAYER_TRACE_DEBUG    48

// TODO SPLAYER_TRACE_MAX_QUEUE needs to be tweaked
// TODO the buffer should be close to how much the system can write to
//      file. Increasing the buffer will not solve anything. Sooner or
//      later the buffer is going to fill up anyways.
#define SPLAYER_TRACE_MAX_QUEUE  1000   //default 8000 or 2000

#define SPLAYER_TRACE_NUM_ARRAY 2
#define SPLAYER_TRACE_MAX_MESSAGE_SIZE 256
// Total buffer size is SPLAYER_TRACE_NUM_ARRAY (number of buffer partitions) *
// SPLAYER_TRACE_MAX_QUEUE (number of lines per buffer partition) *
// SPLAYER_TRACE_MAX_MESSAGE_SIZE (number of 1 byte charachters per line) =
// 1 or 4 Mbyte

#define SPLAYER_TRACE_MAX_FILE_SIZE 10*1000*1000 // 10Mbyte
// Number of rows that may be written to file. On average 110 bytes per row (max
// 256 bytes per row). So on average 110*100*1000 = 11 Mbyte, max 256*100*1000 =
// 25.6 Mbyte


class STrace
{
public:
	// create a static instance
	static STrace* Create();
	
	// destroy a static instance
	static void Destroy();

	// log to console
	static void EnableConsole(bool enable);
	static bool Console();
	
	// log to file
	static void EnableFile(bool enable);
	static bool File();
	
    // Specifies what type of messages should be written to the trace file.
    static int SetFilter(const int level);
    static int Filter();

    // Sets the file path. 
    static int SetFilePath(const char* filePath);
	static char* FilePath();

    // Adds a trace message for writing to file. The message is put in a queue
    // for writing to file whenever possible for performance reasons. I.e. there
    // is a crash it is possible that the last, vital logs are not logged yet.
	static void Log(const int level,const char* msg, ...);

public:
	virtual ~STrace();
    
	//start trace thread
    bool Start();
	
	//stop trace thread
    bool Stop();

	//trace thread process
    static void* Run(void* obj);
	bool Process();
	   
private:
    STrace();

	int SetFilePathImpl(const char* filePath);
	
	void LogImpl(const int level, const char msg[SPLAYER_TRACE_MAX_MESSAGE_SIZE]);
	
	int AddLevel(char* szMessage, const int level) const;
	 
    int AddThreadId(char* traceMessage) const;

    int AddTime(char* traceMessage,const int level) const;

    //WebRtc_Word32 AddBuildInfo(char* traceMessage);
    //WebRtc_Word32 AddDateTimeInfo(char* traceMessage);   
    //WebRtc_Word32 AddModuleAndId(char* traceMessage, const TraceModule module,
    //                             const WebRtc_Word32 id) const;

    int AddMessage(char* traceMessage,
                             const char msg[SPLAYER_TRACE_MAX_MESSAGE_SIZE],
                             const int writtenSoFar) const;

    void AddMessageToList(
        const char traceMessage[SPLAYER_TRACE_MAX_MESSAGE_SIZE],
        const int length,
        const int level);

    //bool UpdateFileName(
    //    const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
    //    char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize],
    //    const WebRtc_UWord32 newCount) const;

    bool CreateFileName(
        const char fileNameUTF8[FileWrapper::kMaxFileNameSize],
        char fileNameWithCounterUTF8[FileWrapper::kMaxFileNameSize]) const;

    void WriteToFile();

private:
	//
	static STrace* _static_instance ;
	static bool _concole_enable ;
	static bool _file_enable ;
	static int _filter ;
	static char _file_path[SPLAYER_TRACE_MAX_MESSAGE_SIZE];
	//
    LYH_Mutex* _critsectInterface;
    int _rowCountText;
    int _fileCountText;

    FileWrapper* _traceFile;
    LYH_Thread _thread;
	bool _thread_quit;
    LYH_Cond* _event;
	LYH_Mutex* _critsectEvent;

    // _critsectArray protects _activeQueue
    LYH_Mutex* _critsectArray;
    unsigned short _nextFreeIdx[SPLAYER_TRACE_NUM_ARRAY];
    int _level[SPLAYER_TRACE_NUM_ARRAY][SPLAYER_TRACE_MAX_QUEUE];
    unsigned short _length[SPLAYER_TRACE_NUM_ARRAY][SPLAYER_TRACE_MAX_QUEUE];
    char* _messageQueue[SPLAYER_TRACE_NUM_ARRAY][SPLAYER_TRACE_MAX_QUEUE];
    unsigned char _activeQueue;
	
	volatile mutable unsigned int  _prevAPITickCount;
    volatile mutable unsigned int  _prevTickCount;
};

#endif // SPLAYER_PLAYER_TRACE_H_


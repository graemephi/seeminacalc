/* RageFile - High-level file access. */

#ifndef RAGE_FILE_H
#define RAGE_FILE_H

class RageFileBasic{};

struct lua_State;

/**
 * @brief High-level file access.
 *
 * This is the high-level interface, which interfaces with RageFileObj
 * implementations and RageFileManager. */
class RageFile : public RageFileBasic
{
  public:
	enum
	{
		READ = 0x1,
		WRITE = 0x2,

		/* Always write directly to the destination file; don't do a safe write.
		   (for logs) */
		STREAMED = 0x4,

		/* Flush the file to disk on close.  Combined with not streaming, this
		 * results in very safe writes, but is slow. */
		SLOW_FLUSH = 0x8
	};

	RageFile();
	~RageFile() { Close(); }
	RageFile(const RageFile& cpy);
	RageFile* Copy() const;

	/*
	 * Use GetRealPath to get the path this file was opened with; use that if
	 * you want a path that will probably get you the same file again.
	 *
	 * GetPath can be overridden by drivers.  Use it to get a path for display;
	 * it may give more information, such as the name of the archive the file
	 * is in.  It has no parsable meaning.
	 */
	const std::string& GetRealPath() const { return m_Path; }
	std::string GetPath() const;

	bool Open(const std::string& path, int mode = READ);
	void Close();
	bool IsOpen() const { return m_File != NULL; }
	int GetMode() const { return m_Mode; }

	bool AtEOF() const;
	std::string GetError() const;
	void ClearError();

	int Tell() const;
	int Seek(int offset);
	int GetFileSize() const;
	int GetFD();

	/* Raw I/O: */
	int Read(void* buffer, size_t bytes);
	int Read(std::string& buffer, int bytes = -1);
	int Write(const void* buffer, size_t bytes);
	int Write(const std::string& string)
	{
		return Write(string.data(), string.size());
	}
	int Flush();

	/* These are just here to make wrappers (eg. vorbisfile, SDL_rwops) easier.
	 */
	int Write(const void* buffer, size_t bytes, int nmemb);
	int Read(void* buffer, size_t bytes, int nmemb);
	int Seek(int offset, int whence);

	/* Line-based I/O: */
	int GetLine(std::string& out);
	int PutLine(const std::string& str);

	void EnableCRC32(bool on = true);
	bool GetCRC32(uint32_t* iRet);

  private:
	void SetError(const std::string& err);

	RageFileBasic* m_File;
	std::string m_Path;
	std::string m_sError;
	int m_Mode;

	// Swallow up warnings. If they must be used, define them.
	RageFile& operator=(const RageFile& rhs);
};

/** @brief Convenience wrappers for reading binary files. */
namespace FileReading {
/* On error, these set sError to the error message.  If sError is already
 * non-empty, nothing happens. */
void
ReadBytes(RageFileBasic& f, void* buf, int size, std::string& sError);
void
SkipBytes(RageFileBasic& f, int size, std::string& sError);
void
Seek(RageFileBasic& f, int iOffset, std::string& sError);
std::string
ReadString(RageFileBasic& f, int size, std::string& sError);
uint8_t
read_8(RageFileBasic& f, std::string& sError);
int16_t
read_16_le(RageFileBasic& f, std::string& sError);
uint16_t
read_u16_le(RageFileBasic& f, std::string& sError);
int32_t
read_32_le(RageFileBasic& f, std::string& sError);
uint32_t
read_u32_le(RageFileBasic& f, std::string& sError);
};

#endif

/*
 * This provides an interface to open files in RageFileManager's namespace
 * This is just a simple RageFileBasic wrapper on top of another RageFileBasic;
 * when a file is open, is acts like the underlying RageFileBasic, except that
 * a few extra sanity checks are made to check file modes.
 */

RageFile::RageFile()
{
	m_File = NULL;
	m_Mode = 0;
}

RageFile::RageFile(const RageFile& cpy)
  : RageFileBasic(cpy)
{
}

RageFile*
RageFile::Copy() const
{
    return 0;
}

std::string
RageFile::GetPath() const
{
    return {};
}

bool
RageFile::Open(const std::string& path, int mode)
{
    return 0;
}

void
RageFile::Close()
{
}

int
RageFile::GetLine(std::string& out)
{
    return 0;
}

int
RageFile::PutLine(const std::string& str)
{
    return 0;
}

void
RageFile::EnableCRC32(bool on)
{
}

bool
RageFile::GetCRC32(uint32_t* iRet)
{
    return 0;
}

bool
RageFile::AtEOF() const
{
    return 0;
}

void
RageFile::ClearError()
{
}

std::string
RageFile::GetError() const
{
    return {};
}

void
RageFile::SetError(const std::string& err)
{
}

int
RageFile::Read(void* pBuffer, size_t iBytes)
{
    return 0;
}

int
RageFile::Seek(int offset)
{
    return 0;
}

int
RageFile::Tell() const
{
    return 0;
}

int
RageFile::GetFileSize() const
{
    return 0;
}

int
RageFile::GetFD()
{
    return 0;
}

int
RageFile::Read(std::string& buffer, int bytes)
{
    return 0;
}

int
RageFile::Write(const void* buffer, size_t bytes)
{
    return 0;
}

int
RageFile::Write(const void* buffer, size_t bytes, int nmemb)
{
    return 0;
}

int
RageFile::Flush()
{
    return 0;
}

int
RageFile::Read(void* buffer, size_t bytes, int nmemb)
{
    return 0;
}

int
RageFile::Seek(int offset, int whence)
{
    return 0;
}

void
FileReading::ReadBytes(RageFileBasic& f, void* buf, int size, std::string& sError)
{
}

std::string
FileReading::ReadString(RageFileBasic& f, int size, std::string& sError)
{
    return {};
}

void
FileReading::SkipBytes(RageFileBasic& f, int iBytes, std::string& sError)
{
}

void
FileReading::Seek(RageFileBasic& f, int iOffset, std::string& sError)
{
}

uint8_t
FileReading::read_8(RageFileBasic& f, std::string& sError)
{
    return 0;
}

uint16_t
FileReading::read_u16_le(RageFileBasic& f, std::string& sError)
{
    return 0;
}

int16_t
FileReading::read_16_le(RageFileBasic& f, std::string& sError)
{
    return 0;
}

uint32_t
FileReading::read_u32_le(RageFileBasic& f, std::string& sError)
{
    return 0;
}

int32_t
FileReading::read_32_le(RageFileBasic& f, std::string& sError)
{
    return 0;
}

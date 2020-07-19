#ifndef RAGE_FILE_MANAGER_H
#define RAGE_FILE_MANAGER_H

/** @brief Constants for working with the RageFileManager. */
namespace RageFileManagerUtil {
extern std::string sInitialWorkingDirectory;
extern std::string sDirOfExecutable;
}

class RageFileDriver{};
class RageFileBasic;
struct lua_State;

auto
ilt(const std::string& a, const std::string& b) -> bool;
auto
ieq(const std::string& a, const std::string& b) -> bool;

/** @brief File utilities and high-level manager for RageFile objects. */
class RageFileManager
{
  public:
	RageFileManager(const std::string& argv0);
	~RageFileManager();
	void MountInitialFilesystems();
	void MountUserFilesystems();

	void GetDirListing(const std::string& sPath,
					   vector<std::string>& AddTo,
					   bool bOnlyDirs,
					   bool bReturnPathToo);

	void GetDirListingWithMultipleExtensions(
	  const std::string& sPath,
	  vector<std::string> const& ExtensionList,
	  vector<std::string>& AddTo,
	  bool bOnlyDirs = false,
	  bool bReturnPathToo = false);

	auto Move(const std::string& sOldPath, const std::string& sNewPath) -> bool;
	auto Remove(const std::string& sPath) -> bool;
	void CreateDir(const std::string& sDir);

	enum FileType
	{
		TYPE_FILE,
		TYPE_DIR,
		TYPE_NONE
	};
	auto GetFileType(const std::string& sPath) -> FileType;

	auto IsAFile(const std::string& sPath) -> bool;
	auto IsADirectory(const std::string& sPath) -> bool;
	auto DoesFileExist(const std::string& sPath) -> bool;

	auto GetFileSizeInBytes(const std::string& sPath) -> int;
	auto GetFileHash(const std::string& sPath) -> int;

	/**
	 * @brief Get the absolte path from the VPS.
	 * @param path the VPS path.
	 * @return the absolute path. */
	auto ResolvePath(const std::string& path) -> std::string;

	auto Mount(const std::string& sType,
			   const std::string& sRealPath,
			   const std::string& sMountPoint,
			   bool bAddToEnd = true) -> bool;
	void Mount(RageFileDriver* pDriver,
			   const std::string& sMountPoint,
			   bool bAddToEnd = true);
	void Unmount(const std::string& sType,
				 const std::string& sRoot,
				 const std::string& sMountPoint);

	/* Change the root of a filesystem.  Only a couple drivers support this;
	 * it's used to change memory card mountpoints without having to actually
	 * unmount the driver. */
	void Remount(const std::string& sMountpoint, const std::string& sPath);
	auto IsMounted(const std::string& MountPoint) -> bool;
	struct DriverLocation
	{
		std::string Type, Root, MountPoint;
	};
	void GetLoadedDrivers(vector<DriverLocation>& asMounts);

	void FlushDirCache(const std::string& sPath = std::string());

	/* Used only by RageFile: */
	auto Open(const std::string& sPath, int iMode, int& iError)
	  -> RageFileBasic*;
	void CacheFile(const RageFileBasic* fb, const std::string& sPath);

	/* Retrieve or release a reference to the low-level driver for a mountpoint.
	 */
	auto GetFileDriver(std::string sMountpoint) -> RageFileDriver*;
	void ReleaseFileDriver(RageFileDriver* pDriver);

	// Lua
	void PushSelf(lua_State* L);

  private:
	auto OpenForReading(const std::string& sPath, int iMode, int& iError)
	  -> RageFileBasic*;
	auto OpenForWriting(const std::string& sPath, int iMode, int& iError)
	  -> RageFileBasic*;
};

extern RageFileManager* FILEMAN;

#endif


RageFileManager* FILEMAN = nullptr;

struct LoadedDriver
{
};

static void
ReferenceAllDrivers(vector<LoadedDriver*>& apDriverList)
{
}

static void
UnreferenceAllDrivers(vector<LoadedDriver*>& apDriverList)
{
}

RageFileDriver*
RageFileManager::GetFileDriver(std::string sMountpoint)
{
    return 0;
}

void
RageFileManager::ReleaseFileDriver(RageFileDriver* pDriver)
{
}

/* Wait for the given driver to become unreferenced, and remove it from the list
 * to get exclusive access to it.  Returns false if the driver is no longer
 * available (somebody else got it first). */
#if 0
static bool GrabDriver( RageFileDriver *pDriver )
{
}
return 0;
#endif

// Mountpoints as directories cause a problem.  If "Themes/default" is a
// mountpoint, and doesn't exist anywhere else, then GetDirListing("Themes/*")
// must return "default".  The driver containing "Themes/default" won't do this;
// its world view begins at "BGAnimations" (inside "Themes/default").  We need a
// dummy driver that handles mountpoints. */
class RageFileDriverMountpoints : public RageFileDriver
{
  public:
	RageFileDriverMountpoints()
	  : RageFileDriver()
	{
	}
	RageFileBasic* Open(const std::string& sPath, int iMode, int& iError)
	{
		return 0;
	}
	/* Never flush FDB, except in LoadFromDrivers. */
	void FlushDirCache(const std::string& sPath) {}

	void LoadFromDrivers(const vector<LoadedDriver*>& apDrivers)
	{
	}
};
static RageFileDriverMountpoints* g_Mountpoints = nullptr;

static std::string
ExtractDirectory(std::string sPath)
{
	return "";
}

static std::string
ReadlinkRecursive(std::string sPath)
{
	return "";
}

static std::string
GetDirOfExecutable(std::string argv0)
{
	return "";
}

static void
ChangeToDirOfExecutable(const std::string& argv0)
{
}

RageFileManager::RageFileManager(const std::string& argv0)
{
}

void
RageFileManager::MountInitialFilesystems()
{
}

void
RageFileManager::MountUserFilesystems()
{
}

RageFileManager::~RageFileManager()
{
}

static inline void
NormalizePath(std::string& sPath)
{
}

inline bool
ilt(const std::string& a, const std::string& b)
{
	return 0;
}

inline bool
ieq(const std::string& a, const std::string& b)
{
	return 0;
}

/*
 * Helper function to remove all objects from an STL container for which the
 * Predicate pred is true. If you want to remove all objects for which the
 * predicate returns false, wrap the predicate with not1().
 */
template<typename Container, typename Predicate>
void
RemoveIf(Container& c, Predicate p)
{
}

// remove various version control-related files
static inline bool
CVSOrSVN(const std::string& s)
{
	return 0;
}

inline void
StripCvsAndSvn(vector<std::string>& vs)
{
}

static inline bool
MacResourceFork(const std::string& s)
{
	return 0;
}

inline void
StripMacResourceForks(vector<std::string>& vs)
{
}

void
RageFileManager::GetDirListing(const std::string& sPath_,
							   vector<std::string>& AddTo,
							   bool bOnlyDirs,
							   bool bReturnPathToo)
{
}

void
RageFileManager::GetDirListingWithMultipleExtensions(
  const std::string& sPath,
  vector<std::string> const& ExtensionList,
  vector<std::string>& AddTo,
  bool bOnlyDirs,
  bool bReturnPathToo)
{
}

/* Files may only be moved within the same file driver. */
bool
RageFileManager::Move(const std::string& sOldPath_,
					  const std::string& sNewPath_)
{
    return 0;
}

bool
RageFileManager::Remove(const std::string& sPath_)
{
    return 0;
}

void
RageFileManager::CreateDir(const std::string& sDir)
{
}

static void
AdjustMountpoint(std::string& sMountPoint)
{
}

static void
AddFilesystemDriver(LoadedDriver* pLoadedDriver, bool bAddToEnd)
{
}

bool
RageFileManager::Mount(const std::string& sType,
					   const std::string& sRoot_,
					   const std::string& sMountPoint_,
					   bool bAddToEnd)
{
    return 0;
}

/* Mount a custom filesystem. */
void
RageFileManager::Mount(RageFileDriver* pDriver,
					   const std::string& sMountPoint_,
					   bool bAddToEnd)
{
}

void
RageFileManager::Unmount(const std::string& sType,
						 const std::string& sRoot_,
						 const std::string& sMountPoint_)
{
}

void
RageFileManager::Remount(const std::string& sMountpoint,
						 const std::string& sPath)
{
}

bool
RageFileManager::IsMounted(const std::string& MountPoint)
{
    return 0;
}

void
RageFileManager::GetLoadedDrivers(vector<DriverLocation>& asMounts)
{
}

void
RageFileManager::FlushDirCache(const std::string& sPath_)
{
}

RageFileManager::FileType
RageFileManager::GetFileType(const std::string& sPath_)
{
    return {};
}

int
RageFileManager::GetFileSizeInBytes(const std::string& sPath_)
{
    return 0;
}

int
RageFileManager::GetFileHash(const std::string& sPath_)
{
    return 0;
}

std::string
RageFileManager::ResolvePath(const std::string& path)
{
    return {};
}

static bool
SortBySecond(const std::pair<int, int>& a, const std::pair<int, int>& b)
{
	return 0;
}

/*
 * Return true if the given path should use slow, reliable writes.
 *
 * I haven't decided if it's better to do this here, or to specify SLOW_FLUSH
 * manually each place we want it.  This seems more reliable (we might forget
 * somewhere and not notice), and easier (don't have to pass flags down to
 * IniFile::Write, etc).
 */
static bool
PathUsesSlowFlush(const std::string& sPath)
{
	return 0;
}

/* Used only by RageFile: */
RageFileBasic*
RageFileManager::Open(const std::string& sPath_, int mode, int& err)
{
    return 0;
}

void
RageFileManager::CacheFile(const RageFileBasic* fb, const std::string& sPath_)
{
}

RageFileBasic*
RageFileManager::OpenForReading(const std::string& sPath, int mode, int& err)
{
    return 0;
}

RageFileBasic*
RageFileManager::OpenForWriting(const std::string& sPath, int mode, int& iError)
{
    return 0;
}

bool
RageFileManager::IsAFile(const std::string& sPath)
{
    return 0;
}
bool
RageFileManager::IsADirectory(const std::string& sPath)
{
    return 0;
}
bool
RageFileManager::DoesFileExist(const std::string& sPath)
{
    return 0;
}

bool
DoesFileExist(const std::string& sPath)
{
    return 0;
}

bool
IsAFile(const std::string& sPath)
{
    return 0;
}

bool
IsADirectory(const std::string& sPath)
{
    return 0;
}

int
GetFileSizeInBytes(const std::string& sPath)
{
    return 0;
}

void
GetDirListing(const std::string& sPath,
			  vector<std::string>& AddTo,
			  bool bOnlyDirs,
			  bool bReturnPathToo)
{
}

void
GetDirListingRecursive(const std::string& sDir,
					   const std::string& sMatch,
					   vector<std::string>& AddTo)
{
}

void
GetDirListingRecursive(RageFileDriver* prfd,
					   const std::string& sDir,
					   const std::string& sMatch,
					   vector<std::string>& AddTo)
{
}

unsigned int
GetHashForFile(const std::string& sPath)
{
    return 0;
}

unsigned int
GetHashForDirectory(const std::string& sDir)
{
    return 0;
}

// Adaption of sqlite3's test_onefile.c to opening a connection to a db file
// loaded into memory.
//
// For the web interface, the easiest way to get access to the db is to drag
// that sucker on there. The browser gets the binary blob, but sqlite3 doesn't
// like working this way, presumably because directly interacting with .db files
// in any way completely screws with any guarantees they can give about fault
// tolerance and consistency. So we need to do this song and dance with a VFS.

/*
** Method declarations for fs_file.
*/
static int fsClose(sqlite3_file*);
static int fsRead(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int fsWrite(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
static int fsTruncate(sqlite3_file*, sqlite3_int64 size);
static int fsSync(sqlite3_file*, int flags);
static int fsFileSize(sqlite3_file*, sqlite3_int64 *pSize);
static int fsLock(sqlite3_file*, int);
static int fsUnlock(sqlite3_file*, int);
static int fsCheckReservedLock(sqlite3_file*, int *pResOut);
static int fsFileControl(sqlite3_file*, int op, void *pArg);
static int fsSectorSize(sqlite3_file*);
static int fsDeviceCharacteristics(sqlite3_file*);

/*
** Method declarations for fs_vfs.
*/
static int fsOpen(sqlite3_vfs*, const char *, sqlite3_file*, int , int *);
static int fsDelete(sqlite3_vfs*, const char *zName, int syncDir);
static int fsAccess(sqlite3_vfs*, const char *zName, int flags, int *);
static int fsFullPathname(sqlite3_vfs*, const char *zName, int nOut,char *zOut);
static void *fsDlOpen(sqlite3_vfs*, const char *zFilename);
static void fsDlError(sqlite3_vfs*, int nByte, char *zErrMsg);
static void (*fsDlSym(sqlite3_vfs*,void*, const char *zSymbol))(void);
static void fsDlClose(sqlite3_vfs*, void*);
static int fsRandomness(sqlite3_vfs*, int nByte, char *zOut);
static int fsSleep(sqlite3_vfs*, int microseconds);
static int fsCurrentTime(sqlite3_vfs*, double*);

typedef struct fs_vfs_t fs_vfs_t;
struct fs_vfs_t {
  sqlite3_vfs base;
  sqlite3_vfs *pParent;

  void *mem;
  sqlite_int64 size;
};

static fs_vfs_t fs_vfs = {
  {
    1,                                          /* iVersion */
    0,                                          /* szOsFile */
    0,                                          /* mxPathname */
    0,                                          /* pNext */
    "cachedb-vfs",                              /* zName */
    0,                                          /* pAppData */
    fsOpen,                                     /* xOpen */
    fsDelete,                                   /* xDelete */
    fsAccess,                                   /* xAccess */
    fsFullPathname,                             /* xFullPathname */
    fsDlOpen,                                   /* xDlOpen */
    fsDlError,                                  /* xDlError */
    fsDlSym,                                    /* xDlSym */
    fsDlClose,                                  /* xDlClose */
    fsRandomness,                               /* xRandomness */
    fsSleep,                                    /* xSleep */
    fsCurrentTime,                              /* xCurrentTime */
    0                                           /* xCurrentTimeInt64 */
  },
  0,                                            /* pFileList */
  0                                             /* pParent */
};

static sqlite3_io_methods fs_io_methods = {
  1,                            /* iVersion */
  fsClose,                      /* xClose */
  fsRead,                       /* xRead */
  fsWrite,                      /* xWrite */
  fsTruncate,                   /* xTruncate */
  fsSync,                       /* xSync */
  fsFileSize,                   /* xFileSize */
  fsLock,                       /* xLock */
  fsUnlock,                     /* xUnlock */
  fsCheckReservedLock,          /* xCheckReservedLock */
  fsFileControl,                /* xFileControl */
  fsSectorSize,                 /* xSectorSize */
  fsDeviceCharacteristics,      /* xDeviceCharacteristics */
  0,                            /* xShmMap */
  0,                            /* xShmLock */
  0,                            /* xShmBarrier */
  0                             /* xShmUnmap */
};

/*
** Close an fs-file.
*/
static int fsClose(sqlite3_file *pFile){
  return SQLITE_OK;
}

/*
** Read data from an fs-file.
*/
static int fsRead(
  sqlite3_file *pFile,
  void *zBuf,
  int iAmt,
  sqlite_int64 iOfst
){
  int rc = SQLITE_OK;
  if ((iAmt + iOfst) > fs_vfs.size) {
    int over = (iAmt + iOfst) - fs_vfs.size;
    iAmt -= over;
    memset(zBuf + iAmt, 0, over);
    rc = SQLITE_IOERR_SHORT_READ;
  }

  memcpy(zBuf, fs_vfs.mem + iOfst, iAmt);
  return rc;
}

/*
** Write data to an fs-file.
*/
static int fsWrite(
  sqlite3_file *pFile,
  const void *zBuf,
  int iAmt,
  sqlite_int64 iOfst
){
  return SQLITE_OK;
}

/*
** Truncate an fs-file.
*/
static int fsTruncate(sqlite3_file *pFile, sqlite_int64 size){
  return SQLITE_OK;
}

/*
** Sync an fs-file.
*/
static int fsSync(sqlite3_file *pFile, int flags){
  return SQLITE_OK;
}

/*
** Return the current file-size of an fs-file.
*/
static int fsFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  if (pSize) *pSize = fs_vfs.size;
  return SQLITE_OK;
}

/*
** Lock an fs-file.
*/
static int fsLock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}

/*
** Unlock an fs-file.
*/
static int fsUnlock(sqlite3_file *pFile, int eLock){
  return SQLITE_OK;
}

/*
** Check if another file-handle holds a RESERVED lock on an fs-file.
*/
static int fsCheckReservedLock(sqlite3_file *pFile, int *pResOut){
  if (pResOut) pResOut = 0;
  return SQLITE_OK;
}

/*
** File control method. For custom operations on an fs-file.
*/
static int fsFileControl(sqlite3_file *pFile, int op, void *pArg){
  if( op==SQLITE_FCNTL_PRAGMA ) return SQLITE_NOTFOUND;
  return SQLITE_OK;
}

/*
** Return the sector-size in bytes for an fs-file.
*/
static int fsSectorSize(sqlite3_file *pFile){
  return fs_vfs.size;
}

/*
** Return the device characteristic flags supported by an fs-file.
*/
static int fsDeviceCharacteristics(sqlite3_file *pFile){
  return 0;
}

/*
** Open an fs file handle.
*/
static int fsOpen(
  sqlite3_vfs *pVfs,
  const char *zName,
  sqlite3_file *pFile,
  int flags,
  int *pOutFlags
){
  int required = SQLITE_OPEN_READONLY|SQLITE_OPEN_MAIN_DB;
  int rc = SQLITE_OK;

  if ((flags & required) != required) {
    return SQLITE_ERROR;
  }

  if (pOutFlags) {
    *pOutFlags = SQLITE_OPEN_READONLY;
  }

  pFile->pMethods = &fs_io_methods;
  return rc;
}

/*
** Delete the file located at zPath. If the dirSync argument is true,
** ensure the file-system modifications are synced to disk before
** returning.
*/
static int fsDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  return SQLITE_OK;
}

/*
** Test for access permissions. Return true if the requested permission
** is available, or false otherwise.
*/
static int fsAccess(
  sqlite3_vfs *pVfs,
  const char *zPath,
  int flags,
  int *pResOut
){
  if (pResOut) *pResOut = (flags == SQLITE_ACCESS_READ);
  return SQLITE_OK;
}

/*
** Populate buffer zOut with the full canonical pathname corresponding
** to the pathname in zPath. zOut is guaranteed to point to a buffer
** of at least (FS_MAX_PATHNAME+1) bytes.
*/
static int fsFullPathname(
  sqlite3_vfs *pVfs,            /* Pointer to vfs object */
  const char *zPath,            /* Possibly relative input path */
  int nOut,                     /* Size of output buffer in bytes */
  char *zOut                    /* Output buffer */
){
  if (nOut > 8) memcpy(zOut, "vfs db", sizeof("vfs db"));
  return SQLITE_OK;
}

/*
** Open the dynamic library located at zPath and return a handle.
*/
static void *fsDlOpen(sqlite3_vfs *pVfs, const char *zPath){
  return SQLITE_ERROR;
}

/*
** Populate the buffer zErrMsg (size nByte bytes) with a human readable
** utf-8 string describing the most recent error encountered associated
** with dynamic libraries.
*/
static void fsDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){}

/*
** Return a pointer to the symbol zSymbol in the dynamic library pHandle.
*/
static void (*fsDlSym(sqlite3_vfs *pVfs, void *pH, const char *zSym))(void){}

/*
** Close the dynamic library handle pHandle.
*/
static void fsDlClose(sqlite3_vfs *pVfs, void *pHandle){}

/*
** Populate the buffer pointed to by zBufOut with nByte bytes of
** random data.
*/
static int fsRandomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut){
  sqlite3_vfs *pParent = ((fs_vfs_t *)pVfs)->pParent;
  return pParent->xRandomness(pParent, nByte, zBufOut);
}

/*
** Sleep for nMicro microseconds. Return the number of microseconds
** actually slept.
*/
static int fsSleep(sqlite3_vfs *pVfs, int nMicro){
  return SQLITE_OK;
}

/*
** Return the current time as a Julian Day number in *pTimeOut.
*/
static int fsCurrentTime(sqlite3_vfs *pVfs, double *pTimeOut){
  if (pTimeOut) *pTimeOut = 0;
  return SQLITE_OK;
}

/*
** This procedure registers the fs vfs with SQLite. If the argument is
** true, the fs vfs becomes the new default vfs. It is the only publicly
** available function in this file.
*/
int cachedb_vfs_register(void *mem, sqlite_int64 size){
  if (NEVER(fs_vfs.pParent)) return SQLITE_OK;
  fs_vfs.pParent = sqlite3_vfs_find(0);
  fs_vfs.base.mxPathname = fs_vfs.pParent->mxPathname;
  fs_vfs.base.szOsFile = sizeof(sqlite3_file);
  fs_vfs.mem = mem;
  fs_vfs.size = size;
  return sqlite3_vfs_register(&fs_vfs.base, 1);
}

/**
 * MojoPatch; a tool for updating data in the field.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCL_PLATFORM_H_
#define _INCL_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __POWERPC__
#define PLATFORM_BIGENDIAN 1
#else
#define PLATFORM_BIGENDIAN 0
#endif

#define PATCHERROR    0
#define PATCHSUCCESS  1

#if PLATFORM_WIN32
#  include <io.h>
#  define PATH_SEP "\\"
#  if (defined _MSC_VER)
#    define inline __inline
#    define snprintf _snprintf
#    define mkdir(x, y) _mkdir(x)
#    define chdir(x) _chdir(x)
#  endif
#  define MAX_PATH 1024
#elif PLATFORM_UNIX
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <sys/param.h>
#  include <fcntl.h>
#  define PATH_SEP "/"
#  define MAX_PATH MAXPATHLEN
#else
#  #error please define your platform.
#endif

typedef struct MOJOPATCH_FILELIST
{
    char *fname;
    struct MOJOPATCH_FILELIST *next;
} file_list;

typedef enum
{
    SPAWN_FILENOTFOUND,
    SPAWN_FAILED,
    SPAWN_RETURNGOOD,
    SPAWN_RETURNBAD
} SpawnResult;

/* Your mainline calls this. */
int mojopatch_main(int argc, char **argv);

/* You call this for fatal error messages. */
void _fatal(const char *fmt, ...);

/* Call this for logging (not debug info). */
void _log(const char *fmt, ...);

/* Call this for logging (debug info). */
void _dlog(const char *fmt, ...);

/* platform-specific stuff you implement. */
int file_exists(const char *fname);
int file_is_directory(const char *fname);
int file_is_symlink(const char *fname);
file_list *make_filelist(const char *base);  /* must use malloc(). */
int get_file_size(const char *fname, unsigned int *fsize);
char *get_current_dir(char *buf, size_t bufsize);
char *get_realpath(const char *path);
int update_version(const char *ver);
int calc_tmp_filenames(char **tmp1, char **tmp2);
int locate_product_by_identifier(const char *str, char *buf, size_t bufsize);
int get_product_version(const char *ident, char *buf, size_t bufsize);
SpawnResult spawn_xdelta(const char *cmdline);
SpawnResult spawn_script(const char *scriptname, const char *dstdir);

#ifdef __cplusplus
}
#endif

#endif /* include-once blocker. */

/* end of platform.h ... */


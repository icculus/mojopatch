
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/param.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include "platform.h"
#include "ui.h"

int file_exists(const char *fname)
{
    struct stat statbuf;
    return(stat(fname, &statbuf) == 0);
} /* file_exists */

int file_is_directory(const char *fname)
{
    struct stat statbuf;
    int retval = 0;

    if (stat(fname, &statbuf) == 0)
    {
        if (S_ISDIR(statbuf.st_mode))
            retval = 1;
    } /* if */
    
    return(retval);
} /* file_is_directory */


int file_is_symlink(const char *fname)
{
    struct stat statbuf;
    int retval = 0;

    if (lstat(fname, &statbuf) == 0)
    {
        if (S_ISLNK(statbuf.st_mode))
            retval = 1;
    } /* if */

    return(retval);
} /* file_is_symlink */


/* enumerate contents of (base) directory. */
file_list *make_filelist(const char *base)
{
    file_list *retval = NULL;
    file_list *l = NULL;
    file_list *prev = NULL;
    DIR *dir;
    struct dirent *ent;

    errno = 0;
    dir = opendir(base);
    if (dir == NULL)
    {
        _fatal("Error: could not read dir %s: %s.", base, strerror(errno));
        return(NULL);
    } /* if */

    while (1)
    {
        ent = readdir(dir);
        if (ent == NULL)   /* we're done. */
            break;

        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        /*
         * !!! FIXME: This is a workaround until symlinks are really
         * !!! FIXME:  supported...just pretend they don't exist for now.  :(
         */
        {
            char buf[MAXPATHLEN];
            snprintf(buf, sizeof (buf), "%s/%s", base, ent->d_name);
            if (file_is_symlink(buf))
                continue;
        }

        l = (file_list *) malloc(sizeof (file_list));
        if (l == NULL)
        {
            _fatal("Error: out of memory.");
            break;
        } /* if */

        l->fname = (char *) malloc(strlen(ent->d_name) + 1);
        if (l->fname == NULL)
        {
            free(l);
            _fatal("Error: out of memory.");
            break;
        } /* if */

        strcpy(l->fname, ent->d_name);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* while */

    closedir(dir);
    return(retval);
} /* make_filelist */


int get_file_size(const char *fname, long *fsize)
{
    struct stat statbuf;

    assert(fsize != NULL);

    if (stat(fname, &statbuf) == -1)
    {
        _fatal("Error: failed to get filesize of [%s]: %s.",
                fname, strerror(errno));
        return(0);
    } /* if */

    *fsize = statbuf.st_size;
    return(1);
} /* get_file_size */


char *get_realpath(const char *path)
{
    char resolved_path[MAXPATHLEN];
    char *retval = NULL;

    errno = 0;
    if (!realpath(path, resolved_path))
    {
        _fatal("Can't determine full path of [%s]: %s.",
                path, strerror(errno));
        return(NULL);
    } /* if */

    retval = malloc(strlen(resolved_path) + 1);
    if (retval == NULL)
    {
        _fatal("Error: out of memory.");
        return(NULL);
    } /* if */

    strcpy(retval, resolved_path);
    return(retval);
} /* get_realpath */


#ifdef PLATFORM_MACOSX
#include <ApplicationServices/ApplicationServices.h>

static char *parse_xml(char *ptr, char **tag, char **val)
{
    char *ptr2;
    while ( (ptr = strchr(ptr, '<')) != NULL )
    {
        ptr++;
        if (*ptr == '/') continue;  // prior endtag.
        if (*ptr == '!') continue;  // initial crap.
        if (*ptr == '?') continue;  // initial crap.

        *tag = ptr;
        *(ptr-1) = '/';
        ptr2 = strchr(ptr, ' ');
        if ( (ptr = strchr(ptr, '>')) == NULL ) return(NULL);
        if ((ptr2) && (ptr2 < ptr)) *ptr2 = '\0';
        *ptr = '\0';
        *val = ++ptr;
        while ( (ptr = strstr(ptr, (*tag)-1)) != NULL )
        {
            if (*(ptr-1) != '<') { ptr++; continue; }
            *(ptr-1) = '\0';
            break;
        } /* while */

        return((ptr == NULL) ? NULL : ptr + 1);
    } /* while */

    return(NULL);
} /* parse_xml */


static char *find_info_plist_version(char *ptr)
{
    int have_key = 0;
    char *tag;
    char *val;

    while ( (ptr = parse_xml(ptr, &tag, &val)) != NULL )
    {
        if (have_key)
        {
            have_key = 0;
            if (strcasecmp(tag, "string") == 0)
                return(val);
        } /* if */

        if ((strcasecmp(tag, "plist") == 0) || (strcasecmp(tag, "dict") == 0))
        {
            ptr = val;
            continue;
        } /* if */

        /* You should only use CFBundleShortVersionString, for various
         *  reasons not worth explaining here. CFBundleVersion is here
         *  for older products that need to update to the other tag.
         */
        if (strcasecmp(tag,"key") == 0)
        {
            if (strcasecmp(val,"CFBundleVersion") == 0)
                have_key = 1;
            if (strcasecmp(val,"CFBundleShortVersionString") == 0)
                have_key = 1;
        } /* if */
    } /* while */
    
    return(NULL);
} /* find_info_plist_version */


/* !!! FIXME: Code duplication */
static char *find_info_plist_bundle_id(char *ptr)
{
    int have_key = 0;
    char *tag;
    char *val;

    while ( (ptr = parse_xml(ptr, &tag, &val)) != NULL )
    {
        if (have_key)
        {
            have_key = 0;
            if (strcasecmp(tag, "string") == 0)
                return(val);
        } /* if */

        if ((strcasecmp(tag, "plist") == 0) || (strcasecmp(tag, "dict") == 0))
        {
            ptr = val;
            continue;
        } /* if */

        /* You should only use CFBundleShortVersionString, for various
         *  reasons not worth explaining here. CFBundleVersion is here
         *  for older products that need to update to the other tag.
         */
        if (strcasecmp(tag,"key") == 0)
        {
            if (strcasecmp(val,"CFBundleIdentifier") == 0)
                have_key = 1;
        } /* if */
    } /* while */
    
    return(NULL);
} /* find_info_plist_version */


static int parse_info_dot_plist(const char *ident,
                                const char *version,
                                const char *newversion)
{
    const char *fname = "Contents/Info.plist";  // already chdir'd for this.
    char *mem = NULL;
    char *ptr;
    long fsize;
    int retval = 0;
    FILE *io = NULL;

    if ( !get_file_size(fname, &fsize) ) goto parse_info_plist_bailed;
    if ( (mem = malloc(fsize + 1)) == NULL ) goto parse_info_plist_bailed;
    if ( (io = fopen(fname, "r")) == NULL ) goto parse_info_plist_bailed;
    if ( (fread(mem, fsize, 1, io)) != 1 ) goto parse_info_plist_bailed;
    fclose(io);
    io = NULL;
    mem[fsize] = '\0';

    ptr = find_info_plist_bundle_id(mem);
    if ((ptr == NULL) || (strcasecmp(ptr, ident) != 0))
    {
        int yes = ui_prompt_ny("We don't think we're looking at the right directory!"
                               " Are you SURE this is the right place?"
                               " If you aren't sure, clicking 'Yes' can destroy unrelated files!");
        if (!yes)
        {
            _fatal("Stopping at user's request.");
            free(mem);
            return(0);
        } /* if */
    } /* if */

    if ( (io = fopen(fname, "r")) == NULL ) goto parse_info_plist_bailed;
    if ( (fread(mem, fsize, 1, io)) != 1 ) goto parse_info_plist_bailed;
    fclose(io);

    ptr = find_info_plist_version(mem);
    if (ptr != NULL)
    {
        if (version_ok(ptr, version))
            retval = 1;
        else
        {
            if (strcmp(version, newversion) == 0)
                _fatal("You seem to be all patched up already!");
            else
            {
                _fatal("This patch applies to version '%s', but you have '%s'.",
                        version, ptr);
            } /* else */
            free(mem);
            return(0);
        } /* else */
    } /* if */

parse_info_plist_bailed:
    free(mem);
    if (io != NULL)
        fclose(io);

    if (!retval) _fatal("Can't determine product's installed version.");
    return(retval);
} /* parse_info_dot_plist */


int update_version(const char *ver)
{
    const char *fname = "Contents/Info.plist";  // already chdir'd for this.
    char *mem = NULL;
    char *ptr;
    long fsize;
    int retval = 0;
    long writestart;
    long writeend;
    FILE *io = NULL;

    if ( !get_file_size(fname, &fsize) ) goto update_version_bailed;
    if ( (mem = malloc(fsize + 1)) == NULL ) goto update_version_bailed;
    if ( (io = fopen(fname, "r+")) == NULL ) goto update_version_bailed;
    if ( (fread(mem, fsize, 1, io)) != 1 ) goto update_version_bailed;
    mem[fsize] = '\0';

    ptr = find_info_plist_version(mem);
    if (ptr == NULL) goto update_version_bailed;
    writestart = (long) (ptr - mem);
    writeend = writestart + strlen(ptr);
    ptr = mem + writeend;
    if ( (fseek(io, 0, SEEK_SET) == -1) ) goto update_version_bailed;
    if ( (fread(mem, fsize, 1, io)) != 1 ) goto update_version_bailed;
    if ( (fseek(io, writestart, SEEK_SET) == -1) ) goto update_version_bailed;
    if ( (fwrite(ver, strlen(ver), 1, io)) != 1 ) goto update_version_bailed;
    if ( (fwrite(ptr, strlen(ptr), 1, io)) != 1 ) goto update_version_bailed;
    for (fsize = (writeend - writestart); fsize > 0; fsize--)
        if (fwrite(" ", 1, 1, io) != 1) goto update_version_bailed;

    retval = 1;

update_version_bailed:
    free(mem);
    if (io != NULL)
        fclose(io);

    if (!retval) _fatal("Can't update product's installed version.");
    return(retval);
} /* update_version */


int manually_locate_product(const char *name, char *buf, size_t bufsize);

int chdir_by_identifier(const char *name, const char *str,
                        const char *version, const char *newversion)
{
    char buf[MAXPATHLEN];
    Boolean b;
    OSStatus rc;
    int found = 0;
    int hasident = ((str != NULL) && (*str));

    /* if an identifier is specified, ask LaunchServices to find product... */
    if (hasident)
    {
        CFURLRef url = NULL;
        CFStringRef id = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);

        rc = LSFindApplicationForInfo(kLSUnknownCreator, id, NULL, NULL, &url);
        CFRelease(id);
        if (rc == noErr)
        {
            b = CFURLGetFileSystemRepresentation(url, TRUE, buf, sizeof (buf));
            CFRelease(url);
            if (!b)
            {
                _fatal("Internal error.");
                return(0);
            } /* if */
            found = 1;
        } /* if */
        else
        {
            _log("Couldn't find product. Perhaps it isn't installed?");
        } /* if */
    } /* if */

    if (!found) /* No identifier, or LaunchServices couldn't find it. */
    {
        if (!manually_locate_product(name, buf, sizeof (buf)))
        {
            _fatal("We can't patch the product if we can't find it!");
            return(0);
        } /* if */
    } /* if */

    _log("I think the product is installed at [%s].", buf);

    if (chdir(buf) != 0)
    {
        _fatal("Failed to change to product's installation directory.");
        return(0);
    } /* if */

    return(hasident ? parse_info_dot_plist(str, version, newversion) : 1);
} /* chdir_by_identifier */


int show_and_install_readme(const char *fname, const char *text)
{
    FILE *io;
    char *cmd;
    const char *envr = getenv("HOME");
    if (!envr)
    {
        _fatal("HOME environment var not set?");
        return(0);
    } /* if */

    cmd = alloca(strlen(fname) + strlen(envr) + 30);
    strcpy(cmd, "open ");
    strcat(cmd, envr);
    if (cmd[strlen(cmd)-1] != '/')
        strcat(cmd, "/");
    strcat(cmd, "Desktop/");
    strcat(cmd, fname);

    io = fopen(cmd + 5, "w");
    if (!io)
    {
        _fatal("Failed to open [%s] for writing.", cmd+5);
        return(0);
    } /* if */

    /* !!! FIXME: error checking! */
    fputs(text, io);
    fclose(io);
    system(cmd);
    return(1);
} /* show_and_install_readme */
#endif


int calc_tmp_filenames(char **tmp1, char **tmp2)
{
    static char _tmp1[MAXPATHLEN];
    static char _tmp2[MAXPATHLEN];
    pid_t pid = getpid();
    snprintf(_tmp1, sizeof (_tmp1), "/tmp/mojopatch.tmp1.%d", (int) pid);
    snprintf(_tmp2, sizeof (_tmp2), "/tmp/mojopatch.tmp2.%d", (int) pid);
    *tmp1 = _tmp1;
    *tmp2 = _tmp2;
    return(1);
} /* calc_tmp_filenames */


static char *basedir = NULL;
static volatile int thread_alive = 0;

static void *spawn_thread(void *arg)
{
    static int rc;
    rc = system((char *) arg);
    thread_alive = 0;
    return(&rc);
} /* spawn_thread */


int spawn_xdelta(const char *cmdline)
{
    pthread_t thr;
    void *rc;
    const char *binname = "xdelta";
    char *cmd = alloca(strlen(cmdline) + strlen(basedir) + strlen(binname) + 2);
    if (!cmd)
        return(0);

    sprintf(cmd, "\"%s%s\" %s", basedir, binname, cmdline);
    thread_alive = 1;
    if (pthread_create(&thr, NULL, spawn_thread, cmd) != 0)
        return(0);

    while (thread_alive)
    {
        ui_pump();
        usleep(10000);
    } /* while */

    pthread_join(thr, &rc);
    return(1);  /* !!! FIXME    *((int *) rc) == 0 ); */
} /* spawn_xdelta */


static void find_basedir(int *argc, char **argv)
{
    const char *argv0 = argv[0];
    char buf[MAXPATHLEN];
    char realbuf[MAXPATHLEN];

    if ((argv0 != NULL) && (strchr(argv0, '/') != NULL)) /* path specifed? */
        strncpy(buf, argv0, sizeof (buf));
    else
    {
        char *ptr;
        char *envr = getenv("PATH");
        if (!envr)
            return;

        while (*envr)
        {
            ptr = strchr(envr, ':');
            if (!ptr)
                strcpy(buf, envr);
            else
            {
                memcpy(buf, envr, (size_t) (ptr - envr));
                buf[(size_t) (ptr - envr)] = '\0';
            } /* else */

            envr = ptr + 1;

            if (*buf == '\0')
                continue;

            strcat(buf, "/");
            strcat(buf, argv0);

            if (access(buf, X_OK) == 0)
                break;

            if (!ptr)
            {
                strcpy(buf, ".");  /* oh well. */
                break;
            } /* if */
        } /* while */
    } /* else */

    buf[sizeof (buf) - 1] = '\0';  /* null terminate, just in case. */
    if (realpath(buf, realbuf) == NULL)
        return;

    char *ptr = strrchr(realbuf, '/');  /* chop off binary name. */
    if (ptr != NULL)
        *ptr = '\0';

    if (realbuf[strlen(realbuf)-1] != '/')
        strcat(realbuf, "/");

    basedir = malloc(strlen(realbuf + 1));
    strcpy(basedir, realbuf);

#if PLATFORM_MACOSX
    /* Chop off process serial number arg that the Finder adds... */
    if ( (*argc >= 2) && (strncmp(argv[1], "-psn_", 5) == 0) )
    {
        *argc = *argc - 1;
        argv[1] = NULL;

        /* Now that we know where xdelta will be, chdir out of AppBundle... */
        ptr = strstr(realbuf, "/Contents/MacOS");
        if (ptr != NULL)
        {
            *ptr = '\0';
            chdir(realbuf);
        } /* if */
    } /* if */
#endif
} /* find_basedir */


int main(int argc, char **argv)
{
    int retval;
    find_basedir(&argc, argv);
    retval = mojopatch_main(argc, argv);
    free(basedir);
    return(retval);
} /* unixmain */

/* end of platform_unix.c ... */


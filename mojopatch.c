/*
 *----------------------------------------------------------------------------
 *
 * mojopatch
 * Copyright (C) 2003  Ryan C. Gordon.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *----------------------------------------------------------------------------
 *
 * (Insert documentation here.)
 *
 *----------------------------------------------------------------------------
 *
 *  This software was written quickly, is not well-engineered, and may have
 *   catastrophic bugs. Its method is brute-force, at best. Use at your
 *   own risk. Don't eat yellow snow.
 *
 *   Send patches, improvements, suggestions, etc to Ryan:
 *    icculus@clutteredmind.org.
 *
 *----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "platform.h"
#include "ui.h"
#include "md5.h"

#define VERSION "0.0.3"

#define DEFAULT_PATCHFILENAME "default.mojopatch"

#define PATCHERROR    0
#define PATCHSUCCESS  1
#define MOJOPATCHSIG "mojopatch " VERSION " (icculus@clutteredmind.org)\r\n"

#define OPERATION_DELETE          27
#define OPERATION_DELETEDIRECTORY 28
#define OPERATION_ADD             29
#define OPERATION_ADDDIRECTORY    30
#define OPERATION_PATCH           31
#define OPERATION_REPLACE         32

typedef enum
{
    COMMAND_NONE = 0,
    COMMAND_CREATE,
    COMMAND_INFO,
    COMMAND_DOPATCHING,

    COMMAND_TOTAL
} PatchCommands;

static int debug = 0;
static int interactive = 0;
static int replace = 0;
static PatchCommands command = COMMAND_NONE;

static const char *patchfile = NULL;
static const char *dir1 = NULL;
static const char *dir2 = NULL;

static char *patchtmpfile = NULL;
static char *patchtmpfile2 = NULL;

static char product[128] = {0};
static char identifier[128] = {0};
static char version[128] = {0};
static char newversion[128] = {0};
static char readme[128] = {0};
static char renamedir[128] = {0};

static char **ignorelist = NULL;
static int ignorecount = 0;

static unsigned int maxxdeltamem = 128;  /* in megabytes. */

static unsigned char iobuf[512 * 1024];

/* printf-style: makes string for UI to put in the log. */
void _fatal(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    ui_fatal(buf);
    ui_pump();
} /* _fatal */

/* printf-style: makes string for UI to put in the log. */
void _log(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    ui_add_to_log(buf, 0);
    ui_pump();
} /* _log */


/* printf-style: makes string for UI to put in the log if debugging enabled. */
void _dlog(const char *fmt, ...)
{
    if (debug)
    {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof (buf), fmt, ap);
        va_end(ap);
        buf[sizeof(buf)-1] = '\0';
        ui_add_to_log(buf, 1);
        ui_pump();
    } /* if */
} /* _dlog */

static void _current_operation(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
    ui_status(buf);
    ui_pump();
} /* _current_operation */


static int _do_xdelta(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf)-1] = '\0';
	_dlog("(xdelta call: [%s].)", buf);
    return(spawn_xdelta(buf));
} /* _do_xdelta */


static int in_ignore_list(const char *fname)
{
    int i;
    for (i = 0; i < ignorecount; i++)
    {
        if (strcmp(fname, ignorelist[i]) == 0)
        {
            _log("Ignoring %s on user's instructions.", fname);
            return(1);
        } /* if */
    } /* for */

    return(0);
} /* in_ignore_list */


static inline int info_only(void)
{
    return(command == COMMAND_INFO);
} /* info_only */


static void free_filelist(file_list *list)
{
    file_list *next;
    while (list != NULL)
    {
        next = list->next;
        free(list->fname);
        free(list);
        list = next;
    } /* while */
} /* free_filelist */


static int write_between_files(FILE *in, FILE *out, long fsize)
{
    while (fsize > 0)
    {
        int max = sizeof (iobuf);
        if (max > fsize)
            max = fsize;

        int br = fread(iobuf, 1, max, in);
        if (br <= 0)
        {
            _fatal("read error: %s.", strerror(errno));
            return(PATCHERROR);
        } /* if */
        ui_pump();

        if (fwrite(iobuf, br, 1, out) != 1)
        {
            _fatal("write error: %s.", strerror(errno));
            return(PATCHERROR);
        } /* if */
        ui_pump();

        fsize -= br;
    } /* while */

    return(PATCHSUCCESS);
} /* write_between_files */


static int do_rename(const char *from, const char *to)
{
    FILE *in;
    FILE *out;
    long fsize;
    int rc;

    unlink(to);  /* just in case. */
    if (rename(from, to) != -1)
        return(PATCHSUCCESS);

    /* rename() might fail if from and to are on seperate filesystems. */

    rc = get_file_size(from, &fsize);
    in = fopen(from, "rb");
    out = fopen(to, "wb");
    if ((!rc) || (!in) || (!out))
    {
        if (in)
            fclose(in);
        if (out)
            fclose(out);
        unlink(to);
        _fatal("File copy failed.");
        return(PATCHERROR);
    } /* if */

    rc = write_between_files(in, out, fsize);

    fclose(in);
    if ((fclose(out) == -1) && (rc != PATCHERROR))
    {
        _fatal("File copy failed.");
        return(PATCHERROR);
    } /* if */

    unlink(from);

    return(rc);
} /* do_rename */


static int md5sum(FILE *in, md5_byte_t *digest, int output)
{
    md5_state_t md5state;
    long br;

    _dlog("md5summing...");

    memset(digest, '\0', 16);
    md5_init(&md5state);

    if (fseek(in, 0, SEEK_SET) == -1)
    {
        _fatal("Couldn't seek in file: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    while (1)
    {
        ui_pump();

        br = fread(iobuf, 1, sizeof (iobuf), in);
        if (br == 0)
        {
            int err = errno;
            if (feof(in))
                break;
            else
            {
                _fatal("Read error: %s.", strerror(err));
                return(PATCHERROR);
            } /* else */
        } /* if */
        md5_append(&md5state, (const md5_byte_t *) iobuf, br);
    } /* while */

    md5_finish(&md5state, digest);

    if ((output) || (debug))
    {
      /* ugly, but want to print it all on one line... */
        _log("  (md5sum: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x)",
              digest[0],  digest[1],  digest[2],  digest[3],
              digest[4],  digest[5],  digest[6],  digest[7],
              digest[8],  digest[9],  digest[10], digest[11],
              digest[12], digest[13], digest[14], digest[15]);
    } /* if */

    if (fseek(in, 0, SEEK_SET) == -1)
    {
        _fatal("Couldn't seek in file: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    return(PATCHSUCCESS);
} /* md5sum */


static int verify_md5sum(md5_byte_t *md5, md5_byte_t *result, FILE *in, int isfatal)
{
    md5_byte_t thismd5[16];

    if (md5sum(in, thismd5, 0) == PATCHERROR)
        return(PATCHERROR);

    if (result != NULL)
		memcpy(result, thismd5, sizeof (thismd5));

    if (memcmp(thismd5, md5, sizeof (thismd5)) != 0)
    {
        if (isfatal)
            _fatal("md5sum doesn't match original!");
        return(PATCHERROR);
    } /* if */
    
    return(PATCHSUCCESS);
} /* verify_md5sum */


static int read_asciz_string(char *buffer, FILE *in)
{
    size_t i = 0;
    int ch;

    do
    {
        if (i >= MAX_PATH)
        {
            _fatal("String overflow error.");
            return(PATCHERROR);
        } /* if */

        ch = fgetc(in);
        if (ch == EOF)
        {
            if (feof(in))
                _fatal("Unexpected EOF during read.");
            else
                _fatal("Error during read: %s.", strerror(errno));
            return(PATCHERROR);
        } /* if */

        buffer[i] = (char) ch;
    } while (buffer[i++] != '\0');

    return(PATCHSUCCESS);
} /* read_asciz_string */


static int confirm(void)
{
    char buf[256];
    char *ptr;

    if (!interactive)
        return(1);

    while (1)
    {
        printf("Confirm [Y/n] : ");
        fgets(buf, sizeof (buf) - 1, stdin);
        if ( (ptr = strchr(buf, '\r')) != NULL )
            *ptr = '\0';
        if ( (ptr = strchr(buf, '\n')) != NULL )
            *ptr = '\0';

        if (strlen(buf) <= 1)
        {
            int ch = tolower((int) buf[0]);
            if ((ch == '\0') || (ch == 'y'))
            {
                printf("Answered YES\n");
                return(1);
            } /* if */
            else if (ch == 'n')
            {
                printf("Answered NO\n");
                return(0);
            } /* else if */
        } /* if */
    } /* while */
} /* confirm */


static const char *final_path_element(const char *fname)
{
    const char *ptr = (const char *) strrchr(fname, PATH_SEP[0]);
    assert( (sizeof (PATH_SEP)) == (sizeof (char) * 2) );
    return(ptr ? ptr + 1 : fname);
} /* final_path_element */


/* put a DELETE operation in the mojopatch file... */
static int put_delete(const char *fname, FILE *out)
{
    unsigned char operation = OPERATION_DELETE;

    _current_operation("DELETE %s", final_path_element(fname));
    _log("DELETE %s", fname);

    if (in_ignore_list(fname))
        return(PATCHSUCCESS);

    if (!confirm())
        return(PATCHSUCCESS);

    if (fwrite(&operation, sizeof (operation), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(fname, strlen(fname) + 1, 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    return(PATCHSUCCESS);
} /* put_delete */


/* get a DELETE operation from the mojopatch file... */
static int get_delete(FILE *in)
{
    char fname[MAX_PATH];

    if (read_asciz_string(fname, in) == PATCHERROR)
        return(PATCHERROR);

    _current_operation("DELETE %s", final_path_element(fname));
    _log("DELETE %s", fname);

    if ( (info_only()) || (!confirm()) )
        return(PATCHSUCCESS);

    if (in_ignore_list(fname))
        return(PATCHSUCCESS);

    if (!file_exists(fname))
    {
        _log("file seems to be gone already.");
        return(PATCHSUCCESS);
    } /* if */

    if (file_is_directory(fname))
    {
        _fatal("Expected file, found directory!");
        return(PATCHERROR);
    } /* if */

    if (remove(fname) == -1)
    {
        _fatal("Error removing [%s]: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */

    _log("done DELETE.");
    return(PATCHSUCCESS);
} /* get_delete */


/* put a DELETEDIRECTORY operation in the mojopatch file... */
static int put_delete_dir(const char *fname1, const char *fname2, FILE *out)
{
    unsigned char operation = OPERATION_DELETEDIRECTORY;

    _current_operation("DELETEDIRECTORY %s", final_path_element(fname2));
    _log("DELETEDIRECTORY %s", fname2);

    if (!confirm())
        return(PATCHSUCCESS);

    if (in_ignore_list(fname2))
        return(PATCHSUCCESS);

    if (fwrite(&operation, sizeof (operation), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(fname2, strlen(fname2) + 1, 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    return(PATCHSUCCESS);
} /* put_delete_dir */


static int delete_dir_tree(const char *fname)
{
    char filebuf[MAX_PATH];
    file_list *files = make_filelist(fname);
    file_list *i;
    int rc = 0;

    _log("Deleting directory tree %s", fname);

    for (i = files; i != NULL; i = i->next)
    {
        snprintf(filebuf, sizeof (filebuf), "%s%s%s", fname, PATH_SEP, i->fname);
        if (file_is_directory(filebuf))
            rc = delete_dir_tree(filebuf);
        else
        {
            _log("Deleting file %s from dir tree", filebuf);
            rc = (remove(filebuf) == -1) ? PATCHERROR : PATCHSUCCESS;
            if (rc == PATCHERROR)
                _fatal("failed to delete %s: %s.", filebuf, strerror(errno));
        } /* else */

        if (rc == PATCHERROR)
        {
            free_filelist(files);
            return(PATCHERROR);
        } /* if */
    } /* for */

    free_filelist(files);

    if (rmdir(fname) == -1)
    {
        _fatal("Error removing directory [%s]: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */

    return(PATCHSUCCESS);
} /* delete_dir_tree */


/* get a DELETEDIRECTORY operation from the mojopatch file... */
static int get_delete_dir(FILE *in)
{
    char fname[MAX_PATH];

    if (read_asciz_string(fname, in) == PATCHERROR)
        return(PATCHERROR);

    _current_operation("DELETEDIRECTORY %s", final_path_element(fname));
    _log("DELETEDIRECTORY %s", fname);

    if ( (info_only()) || (!confirm()) )
        return(PATCHSUCCESS);

    if (in_ignore_list(fname))
        return(PATCHSUCCESS);

    if (!file_exists(fname))
    {
        _log("directory seems to be gone already.");
        return(PATCHSUCCESS);
    } /* if */

    if (!file_is_directory(fname))
    {
        _fatal("Expected directory, found file!");
        return(PATCHERROR);
    } /* if */

    if (!delete_dir_tree(fname))
        return(PATCHERROR);

    _log("done DELETEDIRECTORY.");
    return(PATCHSUCCESS);
} /* get_delete_dir */


/* put an ADD operation in the mojopatch file... */
/* !!! FIXME: This really needs compression... */
static int put_add(const char *fname, FILE *out)
{
    md5_byte_t md5[16];
    unsigned char operation = (replace) ? OPERATION_REPLACE : OPERATION_ADD;
    long fsize;
    FILE *in;
    int rc;
    struct stat statbuf;
    mode_t mode;

    _current_operation("%s %s", (replace) ? "ADDORREPLACE" : "ADD",
                        final_path_element(fname));
    _log("%s %s", (replace) ? "ADDORREPLACE" : "ADD", fname);

    if (!confirm())
        return(PATCHSUCCESS);

    if (in_ignore_list(fname))
        return(PATCHSUCCESS);

    if (fwrite(&operation, sizeof (operation), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(fname, strlen(fname) + 1, 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (!get_file_size(fname, &fsize))
        return(PATCHERROR);

    if (fwrite(&fsize, sizeof (fsize), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (stat(fname, &statbuf) == -1)
    {
        _fatal("Couldn't stat %s: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */
    mode = (mode_t) statbuf.st_mode;

    in = fopen(fname, "rb");
    if (in == NULL)
    {
        _fatal("failed to open [%s]: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (md5sum(in, md5, debug) == PATCHERROR)
    {
        fclose(in);
        return(PATCHERROR);
    } /* if */

    if (fwrite(md5, sizeof (md5), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(&mode, sizeof (mode), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    rc = write_between_files(in, out, fsize);
    assert(fgetc(in) == EOF);
    fclose(in);
    _dlog("  (%ld bytes in file.)", fsize);
    return(rc);
} /* put_add */


/* get an ADD or REPLACE operation from the mojopatch file... */
/* !!! FIXME: This really needs compression... */
static int get_add(FILE *in, int replace_ok)
{
    md5_byte_t md5[16];
    int retval = PATCHERROR;
    FILE *io = NULL;
    char fname[MAX_PATH];
    long fsize;
    int rc;
    mode_t mode;

    if (read_asciz_string(fname, in) == PATCHERROR)
        goto get_add_done;

    if (fread(&fsize, sizeof (fsize), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fread(md5, sizeof (md5), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        goto get_add_done;
    } /* if */

    if (fread(&mode, sizeof (mode), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    _current_operation("%s %s", (replace_ok) ? "ADDORREPLACE" : "ADD",
                          final_path_element(fname));
    _log("%s %s", (replace_ok) ? "ADDORREPLACE" : "ADD", fname);

    if ( (info_only()) || (!confirm()) || (in_ignore_list(fname)) )
    {
        if (fseek(in, fsize, SEEK_CUR) < 0)
        {
            _fatal("Seek error: %s.", strerror(errno));
            return(PATCHERROR);
        } /* if */
        return(PATCHSUCCESS);
    } /* if */

    if (file_exists(fname))
    {
        if (replace_ok)
            unlink(fname);
        else
        {
            if (file_is_directory(fname))
            {
                _fatal("Error: [%s] already exists, but it's a directory!", fname);
                return(PATCHERROR);
            } /* if */

            _log("[%s] already exists...looking at md5sum...", fname);
            _current_operation("VERIFY %s", final_path_element(fname));
            io = fopen(fname, "rb");
            if (io == NULL)
            {
                _fatal("Failed to open added file for read: %s.", strerror(errno));
                goto get_add_done;
            } /* if */
        
            if (verify_md5sum(md5, NULL, io, 1) == PATCHERROR)
                goto get_add_done;

            _log("Okay; file matches what we expected.");
            fclose(io);

            if (fseek(in, fsize, SEEK_CUR) < 0)
            {
                _fatal("Seek error: %s.", strerror(errno));
                return(PATCHERROR);
            } /* if */

            return(PATCHSUCCESS);
        } /* else */
    } /* if */

    io = fopen(fname, "wb");
    if (io == NULL)
    {
        _fatal("Error creating [%s]: %s.", fname, strerror(errno));
        goto get_add_done;
    } /* if */

    rc = write_between_files(in, io, fsize);
    if (rc == PATCHERROR)
        goto get_add_done;

    if (fclose(io) == EOF)
    {
        _fatal("Error: Couldn't flush output: %s.", strerror(errno));
        goto get_add_done;
    } /* if */

    chmod(fname, mode);  /* !!! FIXME: Should this be an error condition? */

    _current_operation("VERIFY %s", final_path_element(fname));
    io = fopen(fname, "rb");
    if (io == NULL)
    {
        _fatal("Failed to open added file for read: %s.", strerror(errno));
        goto get_add_done;
    } /* if */
        
    if (verify_md5sum(md5, NULL, io, 1) == PATCHERROR)
        goto get_add_done;

    retval = PATCHSUCCESS;
    _log("done %s.", (replace_ok) ? "ADDORREPLACE" : "ADD");

get_add_done:
    if (io != NULL)
        fclose(io);

    return(retval);
} /* get_add */


static int put_add_for_wholedir(const char *base, FILE *out);


/* put an ADDDIRECTORY operation in the mojopatch file... */
static int put_add_dir(const char *fname, FILE *out)
{
    unsigned char operation = OPERATION_ADDDIRECTORY;
    struct stat statbuf;
    mode_t mode;

    _current_operation("ADDDIRECTORY %s", final_path_element(fname));
    _log("ADDDIRECTORY %s", fname);

    if (!confirm())
        return(PATCHSUCCESS);

    if (in_ignore_list(fname))
        return(PATCHSUCCESS);

    if (stat(fname, &statbuf) == -1)
    {
        _fatal("Couldn't stat %s: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */
    mode = (mode_t) statbuf.st_mode;

    if (fwrite(&operation, sizeof (operation), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(fname, strlen(fname) + 1, 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(&mode, sizeof (mode), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    /* must add contents of dir after dir itself... */
    if (put_add_for_wholedir(fname, out) == PATCHERROR)
        return(PATCHERROR);

    return(PATCHSUCCESS);
} /* put_add_dir */


/* get an ADDDIRECTORY operation from the mojopatch file... */
static int get_add_dir(FILE *in)
{
    char fname[MAX_PATH];
    mode_t mode;

    if (read_asciz_string(fname, in) == PATCHERROR)
        return(PATCHERROR);

    if (fread(&mode, sizeof (mode), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    _current_operation("ADDDIRECTORY %s", final_path_element(fname));
    _log("ADDDIRECTORY %s", fname);

    if ( (info_only()) || (!confirm()) || (in_ignore_list(fname)) )
        return(PATCHSUCCESS);

    if (file_exists(fname))
    {
        if (file_is_directory(fname))
        {
            _log("[%s] already exists.", fname);
            return(PATCHSUCCESS);
        } /* if */
        else
        {
            _fatal("[%s] already exists, but it's a file!", fname);
            return(PATCHERROR);
        } /* else */
    } /* if */

    if (mkdir(fname, S_IRWXU) == -1)
    {
        _fatal("Error making directory [%s]: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */
    chmod(fname, mode);  /* !!! FIXME: Should this be an error condition? */

    _log("done ADDDIRECTORY.");
    return(PATCHSUCCESS);
} /* get_add_dir */


/* put add operations for each file in (base). Recurses into subdirs. */
static int put_add_for_wholedir(const char *base, FILE *out)
{
    char filebuf[MAX_PATH];
    file_list *files = make_filelist(base);
    file_list *i;
    int rc = 0;

    for (i = files; i != NULL; i = i->next)
    {
        snprintf(filebuf, sizeof (filebuf), "%s%s%s", base, PATH_SEP, i->fname);

        /* put_add_dir recurses back into this function. */
        if (file_is_directory(filebuf))
            rc = put_add_dir(filebuf, out);
        else
            rc = put_add(filebuf, out);

        if (rc == PATCHERROR)
        {
            free_filelist(files);
            return(PATCHERROR);
        } /* if */
    } /* for */

    free_filelist(files);
    return(PATCHSUCCESS);
} /* put_add_for_wholedir */


static int md5sums_match(const char *fname1, const char *fname2,
                         md5_byte_t *md5_1, md5_byte_t *md5_2)
{
    FILE *in;

    in = fopen(fname1, "rb");
    if (in == NULL)
        return(0);

    if (md5sum(in, md5_1, 0) == PATCHERROR)
        return(0);

    fclose(in);

    in = fopen(fname2, "rb");
    if (in == NULL)
        return(0);

    if (md5sum(in, md5_2, 0) == PATCHERROR)
        return(0);

    fclose(in);

    return(memcmp(md5_1, md5_2, 16) == 0);
} /* md5sums_match */


/* put a PATCH operation in the mojopatch file... */
static int put_patch(const char *fname1, const char *fname2, FILE *out)
{
    md5_byte_t md5_1[16];
    md5_byte_t md5_2[16];
    unsigned char operation = OPERATION_PATCH;
    long fsize;
    FILE *deltaio = NULL;
    int rc;
    struct stat statbuf;
    mode_t mode;

    _current_operation("VERIFY %s", final_path_element(fname2));
	if (md5sums_match(fname1, fname2, md5_1, md5_2))
        return(PATCHSUCCESS);

    _current_operation("PATCH %s", final_path_element(fname2));
    _log("PATCH %s", fname2);

    if (!confirm())
        return(PATCHSUCCESS);

    if (in_ignore_list(fname2))
        return(PATCHSUCCESS);

    if (fwrite(&operation, sizeof (operation), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(fname2, strlen(fname2) + 1, 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(&md5_1, sizeof (md5_1), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fwrite(&md5_2, sizeof (md5_2), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (!get_file_size(fname2, &fsize))
        return(PATCHERROR);

    if (fwrite(&fsize, sizeof (fsize), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if ( (!_do_xdelta("delta -n --maxmem=%dM \"%s\" \"%s\" \"%s\"", maxxdeltamem, fname1, fname2, patchtmpfile)) ||
         (!get_file_size(patchtmpfile, &fsize)) )
    {
        /* !!! FIXME: Not necessarily true. */
        _fatal("there was a problem running xdelta.");
        return(PATCHERROR);
    } /* if */
        
    if (fwrite(&fsize, sizeof (fsize), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (stat(fname2, &statbuf) == -1)
    {
        _fatal("Couldn't stat %s: %s.", fname2, strerror(errno));
        return(PATCHERROR);
    } /* if */
    mode = (mode_t) statbuf.st_mode;

    if (fwrite(&mode, sizeof (mode), 1, out) != 1)
    {
        _fatal("write failure: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    deltaio = fopen(patchtmpfile, "rb");
    if (deltaio == NULL)
    {
        _fatal("couldn't read %s: %s.", patchtmpfile, strerror(errno));
        return(PATCHERROR);
    } /* if */

    rc = write_between_files(deltaio, out, fsize);
    assert(fgetc(deltaio) == EOF);
    fclose(deltaio);
    unlink(patchtmpfile);
    _dlog("  (%ld bytes in patch.)", fsize);
    return(rc);
} /* put_patch */


/* get a PATCH operation from the mojopatch file... */
static int get_patch(FILE *in)
{
    md5_byte_t md5_1[16];
    md5_byte_t md5_2[16];
	md5_byte_t md5result[16];
    long fsize;
    long deltasize;
    char fname[MAX_PATH];
    FILE *f = NULL;
    FILE *deltaio = NULL;
    int rc;
    mode_t mode;

    if (read_asciz_string(fname, in) == PATCHERROR)
        return(PATCHERROR);

    if (fread(md5_1, sizeof (md5_1), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fread(md5_2, sizeof (md5_2), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fread(&fsize, sizeof (fsize), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fread(&deltasize, sizeof (deltasize), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (fread(&mode, sizeof (mode), 1, in) != 1)
    {
        _fatal("Read error: %s.", strerror(errno));
        return(PATCHERROR);
    } /* if */

    _log("PATCH %s", fname);

    if ( (info_only()) || (!confirm()) || (in_ignore_list(fname)) )
    {
        if (fseek(in, deltasize, SEEK_CUR) < 0)
        {
            _fatal("Seek error: %s.", strerror(errno));
            return(PATCHERROR);
        } /* if */
        return(PATCHSUCCESS);
    } /* if */

    f = fopen(fname, "rb");
    if (f == NULL)
    {
        _fatal("Failed to open [%s] for read: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */

    _current_operation("VERIFY %s", final_path_element(fname));
    rc = verify_md5sum(md5_1, md5result, f, 0);
    fclose(f);
    if (rc == PATCHERROR)
    {
        if (memcmp(md5_2, md5result, sizeof (md5_2)) == 0)
        {
            _log("Okay; file matches patched md5sum. It's already patched.");
            if (fseek(in, deltasize, SEEK_CUR) < 0)
            {
                _fatal("Seek error: %s.", strerror(errno));
                return(PATCHERROR);
            } /* if */
            return(PATCHSUCCESS);
        } /* if */
        return(PATCHERROR);
    } /* if */

    unlink(patchtmpfile2); /* just in case... */

    _current_operation("PATCH %s", final_path_element(fname));
    deltaio = fopen(patchtmpfile2, "wb");
    if (deltaio == NULL)
    {
        _fatal("Failed to open [%s] for write: %s.", patchtmpfile2, strerror(errno));
        return(PATCHERROR);
    } /* if */

    rc = write_between_files(in, deltaio, deltasize);
    fclose(deltaio);
    if (rc == PATCHERROR)
    {
        unlink(patchtmpfile2);
        return(PATCHERROR);
    } /* if */

    if (!_do_xdelta("patch --maxmem=%dM \"%s\" \"%s\" \"%s\"", maxxdeltamem, patchtmpfile2, fname, patchtmpfile))
    {
        _fatal("xdelta failed.");
        return(PATCHERROR);
    } /* if */

    unlink(patchtmpfile2);  /* ditch temp delta file... */

    f = fopen(patchtmpfile, "rb");
    if (f == NULL)
    {
        _fatal("Failed to open [%s] for read: %s.", patchtmpfile, strerror(errno));
        return(PATCHERROR);
    } /* if */

    _current_operation("VERIFY %s", final_path_element(fname));
    rc = verify_md5sum(md5_2, NULL, f, 1);
    fclose(f);
    if (rc == PATCHERROR)
        return(PATCHERROR);

    if (do_rename(patchtmpfile, fname) == -1)
    {
        _fatal("Error replacing [%s] with tempfile: %s.", fname, strerror(errno));
        return(PATCHERROR);
    } /* if */

    chmod(fname, mode);  /* !!! FIXME: fatal error? */

    _log("done PATCH.");
    return(PATCHSUCCESS);
} /* get_patch */


static int compare_directories(const char *base1, const char *base2, FILE *out)
{
    int retval = PATCHERROR;
    char filebuf1[MAX_PATH];
    char filebuf2[MAX_PATH]; /* can you feel the stack screaming? */
    const char *base2checked = *base2 ? base2 : ".";
    file_list *files1 = make_filelist(base1);
    file_list *files2 = NULL;
    file_list *i;

    /* may be recursive compare on deleted dir. */
    if (file_exists(base2checked))
        files2 = make_filelist(base2checked);

    assert(*base1);

    _current_operation("Examining %s", final_path_element(base2checked));
    _dlog("Examining %s and %s", base1, base2checked);

    _dlog("(looking for files that need deletion...)");

    /* check for files removed in newer version... */
    for (i = files1; i != NULL; i = i->next)
    {
        _dlog("([%s]...)", i->fname);

        snprintf(filebuf2, sizeof (filebuf2), "%s%s%s", base2,
                    *base2 ? PATH_SEP : "", i->fname);

        if (!file_exists(filebuf2))
        {
            int rc = 0;

            snprintf(filebuf1, sizeof (filebuf1), "%s%s%s", base1, PATH_SEP, i->fname);
            if (!file_is_directory(filebuf1))
                rc = put_delete(filebuf2, out);
            else
            {
                rc = compare_directories(filebuf1, filebuf2, out);
                if (rc != PATCHERROR)
                    rc = put_delete_dir(filebuf1, filebuf2, out);
            } /* else */

            if (rc == PATCHERROR)
                goto dircompare_done;
        } /* if */
    } /* for */

    _dlog("(looking for files that need addition...)");

	/* check for files added in newer version... */
    for (i = files2; i != NULL; i = i->next)
    {
        _dlog("([%s]...)", i->fname);

        snprintf(filebuf1, sizeof (filebuf1), "%s%s%s", base1, PATH_SEP, i->fname);
        snprintf(filebuf2, sizeof (filebuf2), "%s%s%s", base2,
                    *base2 ? PATH_SEP : "", i->fname);

        if (file_exists(filebuf1))  /* exists in both dirs; do compare. */
        {
            if (file_is_directory(filebuf2))
            {
                    /* probably a bad sign ... */
                if (!file_is_directory(filebuf1))
                {
                    _log("%s is a directory, but %s is not!", filebuf2, filebuf1);
                    if (put_delete(filebuf2, out) == PATCHERROR)
                        goto dircompare_done;

                    if (put_add_dir(filebuf2, out) == PATCHERROR)
                        goto dircompare_done;
                } /* if */

                if (compare_directories(filebuf1, filebuf2, out) == PATCHERROR)
                    goto dircompare_done;
            } /* if */

            else  /* new item is not a directory. */
            {
                    /* probably a bad sign ... */
                if (file_is_directory(filebuf1))
                {
                    _log("Warning: %s is a directory, but %s is not!", filebuf1, filebuf2);
                    if (put_delete_dir(filebuf1, filebuf2, out) == PATCHERROR)
                        goto dircompare_done;

                    if (put_add(filebuf2, out) == PATCHERROR)
                        goto dircompare_done;
                } /* if */

                else
                {
                    /* may not put anything if files match... */
                    if (put_patch(filebuf1, filebuf2, out) == PATCHERROR)
                        goto dircompare_done;
                } /* else */
            } /* else */
        } /* if */

        else  /* doesn't exist in second dir; do add. */
        {
            if (file_is_directory(filebuf2))
            {
                if (put_add_dir(filebuf2, out) == PATCHERROR)
                    goto dircompare_done;
            } /* if */

            else
            {
                if (put_add(filebuf2, out) == PATCHERROR)
                    goto dircompare_done;
            } /* else */
        } /* else */
    } /* for */

    retval = PATCHSUCCESS;

dircompare_done:
    free_filelist(files1);
    free_filelist(files2);
    return(retval);
} /* compare_directories */


static char *read_whole_file(const char *fname)
{
    int i;
    int rc;
    FILE *io = NULL;
    long fsize = 0;
    char *retval = NULL;

    if (!get_file_size(fname, &fsize))
    {
        _fatal("Can't get filesize for [%s]...file missing?", fname);
        return(NULL);
    } /* if */

    if ( (retval = (char *) malloc(fsize + 1)) == NULL )
    {
        _fatal("Out of memory.");
        return(NULL);
    } /* if */

    if ( (io = fopen(fname, "r")) == NULL )
    {
        _fatal("Can't open [%s].", fname);
        free(retval);
        return(NULL);
    } /* if */

    rc = fread(retval, fsize, 1, io);
    fclose(io);

    if (rc != 1)
    {
        _fatal("Read on [%s] failed: %s", fname, strerror(errno));
        free(retval);
        return(NULL);
    } /* if */

    /* This considers it an error condition to have a null char in the file. */
    for (i = 0; i < fsize; i++)
    {
        if (retval[i] == '\0')
        {
            _fatal("null char in read_whole_file.");
            free(retval);
            return(NULL);
        } /* if */
    } /* for */
    retval[fsize] = '\0';

    return(retval);
} /* read_whole_file */


static int create_patchfile(void)
{
    int retval = PATCHSUCCESS;
    char *real1 = NULL;
    char *real2 = NULL;
    char *real3 = NULL;
    long fsize;
    FILE *out;
    char *readmedata = "";
    char *readmedataptr = NULL;
    const char *readmefname = final_path_element(readme);

    if (strcmp(identifier, "") == 0)  /* specified on the commandline. */
    {
        ui_fatal("Can't create a patchfile without an identifier.");
        return(PATCHERROR);
    } /* if */

    // !!! FIXME: platform should determine this by examining compared dirs.
    if (strcmp(version, "") == 0)  /* specified on the commandline. */
    {
        ui_fatal("Can't create a patchfile without --version.");
        return(PATCHERROR);
    } /* if */

    // !!! FIXME: platform should determine this by examining compared dirs.
    if (strcmp(newversion, "") == 0)  /* specified on the commandline. */
    {
        ui_fatal("Can't create a patchfile without --newversion.");
        return(PATCHERROR);
    } /* if */

    real1 = get_realpath(dir1);
    if (real1 == NULL)
    {
        _fatal("Couldn't get realpath of [%s].", dir1);
        return(PATCHERROR);
    } /* if */

    real2 = get_realpath(dir2);
    if (real2 == NULL)
    {
        _fatal("Couldn't get realpath of [%s].", dir2);
        return(PATCHERROR);
    } /* if */

    real3 = get_realpath(patchfile);
    if (real3 == NULL)
    {
        _fatal("Couldn't get realpath of [%s].", patchfile);
        return(PATCHERROR);
    } /* if */

    unlink(patchfile);  /* just in case. */
    out = fopen(patchfile, "wb");
    if (out == NULL)
    {
        free(real1);
        free(real2);
        free(real3);
        _fatal("Couldn't open [%s]: %s.", patchfile, strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (chdir(real2) != 0)
    {
        fclose(out);
        free(real1);
        free(real2);
        free(real3);
        _fatal("Couldn't chdir to [%s]: %s.", real2, strerror(errno));
        return(PATCHERROR);
    } /* if */
    free(real2);

    if (*readme)
    {
        readmedata = readmedataptr = read_whole_file(readme);
        if (!readmedata)
        {
            fclose(out);
            free(real1);
            free(real3);
            return(PATCHERROR);
        } /* if */
    } /* if */

    if ( (fwrite(MOJOPATCHSIG, strlen(MOJOPATCHSIG) + 1, 1, out) != 1) ||
         (fwrite(product, strlen(product) + 1, 1, out) != 1) ||
         (fwrite(identifier, strlen(identifier) + 1, 1, out) != 1) ||
         (fwrite(version, strlen(version) + 1, 1, out) != 1) ||
         (fwrite(newversion, strlen(newversion) + 1, 1, out) != 1) ||
         (fwrite(readmefname, strlen(readmefname) + 1, 1, out) != 1) ||
         (fwrite(readmedata, strlen(readmedata) + 1, 1, out) != 1) ||
         (fwrite(renamedir, strlen(renamedir) + 1, 1, out) != 1) )
    {
        _fatal("Couldn't write header [%s]: %s.", patchfile, strerror(errno));
        fclose(out);
        free(real1);
        free(real3);
        free(readmedataptr);
        return(PATCHERROR);
    } /* if */

    free(readmedataptr);

    retval = compare_directories(real1, "", out);

    free(real1);

    if (fclose(out) == EOF)
    {
        free(real3);
        _fatal("Couldn't close [%s]: %s.", patchfile, strerror(errno));
        retval = PATCHERROR;
    } /* if */

    if (!get_file_size(real3, &fsize))
    {
        _fatal("Couldn't get size of [%s]: %s.", patchfile, strerror(errno));
        retval = PATCHERROR;
    } /* if */
    free(real3);

    if (retval == PATCHERROR)
        _fatal("THE FILE [%s] IS LIKELY INCOMPLETE. DO NOT USE!", patchfile);
    else
    {
        ui_success("Patchfile successfully created.");
        _log("%ld bytes in the file [%s].", fsize, patchfile);
    } /* else */

    return(retval);
} /* create_patchfile */


static int do_patch_operations(FILE *in, int do_progress, long patchfile_size)
{
    if (info_only())
        _log("These are the operations we would perform if patching...");

    while (1)
    {
        ui_pump();

        if (do_progress)
        {
            long pos = ftell(in);
            if (pos != -1)
            {
                float progress = ((float) pos) / ((float) patchfile_size);
                ui_total_progress((int) (progress * 100.0f));
            } /* if */
            else
            {
                do_progress = 0;
                ui_total_progress(-1);
            } /* else */
        } /* if */

        int ch = fgetc(in);
        if (ch == EOF)
        {
            if (feof(in))
            {                
                return(PATCHSUCCESS);
            } /* if */
            else if (ferror(in))
            {
                _fatal("Read error: %s.", strerror(errno));
                return(PATCHERROR);
            } /* else if */

            assert(0);  /* wtf?! */
            _fatal("Odd read error.");
            return(PATCHERROR);  /* normal EOF. */
        } /* if */

        switch ((char) ch)
        {
            case OPERATION_DELETE:
                if (get_delete(in) == PATCHERROR)
                    return(PATCHERROR);
                break;

            case OPERATION_DELETEDIRECTORY:
                if (get_delete_dir(in) == PATCHERROR)
                    return(PATCHERROR);
                break;

            case OPERATION_ADD:
                if (get_add(in, 0) == PATCHERROR)
                    return(PATCHERROR);
                break;

            case OPERATION_REPLACE:
                if (get_add(in, 1) == PATCHERROR)
                    return(PATCHERROR);
                break;

            case OPERATION_ADDDIRECTORY:
                if (get_add_dir(in) == PATCHERROR)
                    return(PATCHERROR);
                break;

            case OPERATION_PATCH:
                if (get_patch(in) == PATCHERROR)
                    return(PATCHERROR);
                break;

            default:
                _fatal("Error: Unknown operation (%d).", ch);
                return(PATCHERROR);
        } /* switch */
    } /* while */

    assert(0);  /* shouldn't hit this. */
    return(PATCHERROR);
} /* do_patch_operations */


static int extract_readme(const char *fname, FILE *in)
{
    int retval = PATCHSUCCESS;
    char *buf = NULL;
    size_t buflen = 0;
    size_t br = 0;
    int ch = 0;

    do
    {
        ch = fgetc(in);
        if (ch == EOF)
        {
            _fatal("Unexpected EOF in patchfile.");
            free(buf);
            return(PATCHERROR);
        } /* if */

        if (buflen <= br)
        {
            char *ptr;
            buflen += 1024;
            ptr = realloc(buf, buflen);
            if (!ptr)
            {
                free(buf);
                _fatal("Out of memory.");
                return(PATCHERROR);
            } /* if */
            buf = ptr;
        } /* if */

        buf[br++] = (char) ch;
    } while (ch != '\0');

    if ( (*buf) && (!info_only()) )
        retval = show_and_install_readme(fname, buf);

    free(buf);
    return(retval);
} /* extract_readme */


static int check_patch_header(FILE *in)
{
    char buffer[MAX_PATH];

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);

    if (strcmp(buffer, MOJOPATCHSIG) != 0)
    {
        _fatal("[%s] is not a compatible mojopatch file.", patchfile);
        _log("signature is: %s.", buffer);
        _log("    expected: %s.", MOJOPATCHSIG);
        return(PATCHERROR);
    } /* if */

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(product, "") == 0)
    {
        if (strcmp(buffer, "") == 0)
            snprintf(product, sizeof (product) - 1, "MojoPatch %s", VERSION);
        else
            strncpy(product, buffer, sizeof (product) - 1);
    } /* if */
    product[sizeof (product) - 1] = '\0';  /* just in case. */
    ui_title(product);

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(identifier, "") == 0)
        strncpy(identifier, buffer, sizeof (identifier) - 1);
    identifier[sizeof (identifier) - 1] = '\0';  /* just in case. */

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(version, "") == 0)
        strncpy(version, buffer, sizeof (version) - 1);
    version[sizeof (version) - 1] = '\0';  /* just in case. */

    if (!info_only())
    {
        if (!chdir_by_identifier(identifier, version))
            return(PATCHERROR);
    } /* if */

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(newversion, "") == 0)
        strncpy(newversion, buffer, sizeof (newversion) - 1);
    newversion[sizeof (newversion) - 1] = '\0';  /* just in case. */

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(readme, "") == 0)
        strncpy(readme, buffer, sizeof (readme) - 1);
    readme[sizeof (readme) - 1] = '\0';  /* just in case. */

    if (extract_readme(readme, in) == PATCHERROR)
        return(PATCHERROR);

    if (read_asciz_string(buffer, in) == PATCHERROR)
        return(PATCHERROR);
    if (strcmp(renamedir, "") == 0)
        strncpy(renamedir, buffer, sizeof (renamedir) - 1);
    renamedir[sizeof (renamedir) - 1] = '\0';  /* just in case. */

    _log("Product to patch: \"%s\".", product);
    _log("Product identifier: \"%s\".", identifier);
    _log("Patch from version: \"%s\".", version);
    _log("Patch to version: \"%s\".", newversion);
    _log("Readme: \"%s\".", *readme ? readme : "(none)");
    _log("Renamedir: \"%s\".", *renamedir ? renamedir : "(none)");

    return(PATCHSUCCESS);
} /* check_patch_header */


static int do_patching(void)
{
    int report_error = 0;
    int retval = PATCHERROR;
    FILE *in = NULL;
    long patchfile_size = 0;
    int do_progress = 0;

    if (strcmp(patchfile, "-") == 0)  /* read from stdin? */
        in = stdin;
    else
	{
        do_progress = get_file_size(patchfile, &patchfile_size);
        if (patchfile_size == 0)
            do_progress = 0;  /* prevent a division by zero. */

        in = fopen(patchfile, "rb");
	} /* else */

    ui_total_progress(do_progress ? 0 : -1);
    ui_pump();

    if (in == NULL)
    {
        _fatal("Couldn't open [%s]: %s.", patchfile, strerror(errno));
        return(PATCHERROR);
    } /* if */

    if (check_patch_header(in) == PATCHERROR)
        goto do_patching_done;

    report_error = 1;
    if (do_patch_operations(in, do_progress, patchfile_size) == PATCHERROR)
        goto do_patching_done;

    if (!info_only())
    {
        _current_operation("Updating product version...");
        ui_total_progress(-1);
        if ( (strcmp(newversion, "") != 0) && (!update_version(newversion)) )
            goto do_patching_done;

        if (*renamedir)
        {
            char cwdbuf[MAX_PATH];
            _log("Renaming product's root directory to [%s].", renamedir);
            if (getcwd(cwdbuf, sizeof (cwdbuf)) != NULL)
            {
                chdir("..");
                rename(cwdbuf, renamedir);
                chdir(renamedir);  /* just in case */
            } /* if */
        } /* if */
    } /* if */

    retval = PATCHSUCCESS;
    ui_total_progress(100);
    if (!info_only())
        ui_success("Patching successful!");

do_patching_done:
    if ((in != stdin) && (in != NULL))
        fclose(in);

    if ((retval == PATCHERROR) && (report_error))
    {
        ui_total_progress(-1);
        _fatal("There were problems, so I'm aborting.");
        if (!info_only())
            _fatal("The product is possibly damaged and requires a fresh installation.");
    } /* if */

    return(retval);
} /* do_patching */


static int do_usage(const char *argv0)
{
    _log("");
    _log("USAGE: %s --create <file.mojopatch> <dir1> <dir2>", argv0);
    _log("   or: %s --info <file.mojopatch>", argv0);
    _log("   or: %s <file.mojopatch>", argv0);
    _log("");
    _log("  You may also specify:");
    _log("      --product (Product name for titlebar)");
    _log("      --identifier (Product identifier for locating installation)");
    _log("      --version (Product version to patch against)");
    _log("      --newversion (Product version to patch up to)");
    _log("      --replace (AT CREATE TIME, specify ADDs can overwrite)");
    _log("      --readme (README filename to display/install)");
    _log("      --renamedir (What patched dir should be called)");
    _log("      --ignore (Ignore specific files/dirs)");
    _log("      --confirm (Make process confirm each step)");
    _log("      --debug (spew debugging output)");
    _log("");
    return(0);
} /* do_usage */


static int set_command_or_abort(PatchCommands cmd)
{
    if (command != COMMAND_NONE)
    {
        _fatal("You've specified more than one command!");
        return(0);
    } /* if */

    command = cmd;
    return(1);
} /* set_command_or_abort */


static int parse_cmdline(int argc, char **argv)
{
    int i;
    int nonoptcount = 0;
    char **nonoptions = (char **) alloca(sizeof (char *) * argc);

    if (nonoptions == NULL)
    {
        _fatal("Out of memory!");
        return(0);
    } /* if */

    if (argc <= 1)
    {
        if (file_exists(DEFAULT_PATCHFILENAME))
            nonoptions[nonoptcount++] = DEFAULT_PATCHFILENAME;
        else
            return(do_usage(argv[0]));
    } /* if */

    product[0] = '\0';  /* just in case. */
    identifier[0] = '\0';  /* just in case. */

    for (i = 1; i < argc; i++)
    {
        int okay = 1;

        if (strncmp(argv[i], "--", 2) != 0)
        {
            nonoptions[nonoptcount++] = argv[i];
            continue;
        } /* if */

        if (strcmp(argv[i], "--create") == 0)
            okay = set_command_or_abort(COMMAND_CREATE);
        else if (strcmp(argv[i], "--info") == 0)
            okay = set_command_or_abort(COMMAND_INFO);
        else if (strcmp(argv[i], "--confirm") == 0)
            interactive = 1;
        else if (strcmp(argv[i], "--debug") == 0)
            debug = 1;
        else if (strcmp(argv[i], "--replace") == 0)
            replace = 1;
        else if (strcmp(argv[i], "--product") == 0)
            strncpy(product, argv[++i], sizeof (product) - 1);
        else if (strcmp(argv[i], "--identifier") == 0)
            strncpy(identifier, argv[++i], sizeof (identifier) - 1);
        else if (strcmp(argv[i], "--version") == 0)
            strncpy(version, argv[++i], sizeof (version) - 1);
        else if (strcmp(argv[i], "--newversion") == 0)
            strncpy(newversion, argv[++i], sizeof (newversion) - 1);
        else if (strcmp(argv[i], "--readme") == 0)
            strncpy(readme, argv[++i], sizeof (readme) - 1);
        else if (strcmp(argv[i], "--renamedir") == 0)
            strncpy(renamedir, argv[++i], sizeof (renamedir) - 1);
        else if (strcmp(argv[i], "--ignore") == 0)
        {
            ignorecount++;
            ignorelist = (char **) realloc(ignorelist, sizeof (char *) * ignorecount);
            // !!! FIXME: Check retval.
            ignorelist[ignorecount-1] = argv[++i];
        } /* else if */
        else
        {
            _fatal("Error: Unknown option [%s].", argv[i]);
            return(do_usage(argv[0]));
        } /* else */

        if (!okay)
            return(0);
    } /* for */

    product[sizeof (product) - 1] = '\0';  /* just in case. */
    identifier[sizeof (identifier) - 1] = '\0';  /* just in case. */

    if (command == COMMAND_NONE)
        command = COMMAND_DOPATCHING;

    switch (command)
    {
        case COMMAND_INFO:
        case COMMAND_DOPATCHING:
            if (nonoptcount != 1)
            {
                _fatal("Error: Wrong arguments.");
                return(do_usage(argv[0]));
            } /* if */
            patchfile = nonoptions[0];
            break;

        case COMMAND_CREATE:
            if (nonoptcount != 3)
            {
                _fatal("Error: Wrong arguments.");
                return(do_usage(argv[0]));
            } /* if */
                
            patchfile = nonoptions[0];
            dir1 = nonoptions[1];
            dir2 = nonoptions[2];
            break;

        default:
            assert(0);
            break;
    } /* switch */

    if (debug)
    {
        _dlog("debugging enabled.");
        _dlog("Interactive mode %senabled.", (interactive) ? "" : "NOT ");
        _dlog("ADDs are %spermitted to REPLACE.", (replace) ? "" : "NOT ");
        _dlog("command == (%d).", (int) command);
        _dlog("(%d) nonoptions:", nonoptcount);
        for (i = 0; i < nonoptcount; i++)
            _dlog(" [%s]", nonoptions[i]);
        _dlog("patchfile == [%s].", (patchfile) ? patchfile : "(null)");
        _dlog("dir1 == [%s].", (dir1) ? dir1 : "(null)");
        _dlog("dir2 == [%s].", (dir2) ? dir2 : "(null)");
        for (i = 0; i < ignorecount; i++)
            _dlog("ignoring [%s].", ignorelist[i]);
    } /* if */

    return(1);
} /* parse_cmdline */


/* !!! FIXME: signal_cleanup */


int mojopatch_main(int argc, char **argv)
{
	time_t starttime = time(NULL);
    int retval = PATCHSUCCESS;

    if (!ui_init())
    {
        _fatal("MojoPatch: ui_init() failed!");  /* oh well. */
        return(PATCHERROR);
    } /* if */

    _log("MojoPatch %s starting up.", VERSION);

    if (!parse_cmdline(argc, argv))
    {
        ui_deinit();
        return(PATCHERROR);
    } /* if */

    if (!calc_tmp_filenames(&patchtmpfile, &patchtmpfile2))
    {
        _fatal("Internal error: Couldn't find scratch filenames.");
        ui_deinit();
        return(PATCHERROR);
    } /* if */
    _dlog("Temp filenames are [%s] and [%s].", patchtmpfile, patchtmpfile2);

    if (command == COMMAND_CREATE)
        retval = create_patchfile();
    else
        retval = do_patching();

    unlink(patchtmpfile);  /* just in case. */
    unlink(patchtmpfile2); /* just in case. */

    _dlog("(Total running time: %ld seconds.)", time(NULL) - starttime);

    ui_deinit();
    return(retval);
} /* mojopatch_main */

/* end of mojopatch.c ... */


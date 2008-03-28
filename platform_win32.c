/**
 * MojoPatch; a tool for updating data in the field.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <windows.h>
#include <io.h>
#include <direct.h>

#include "platform.h"

int file_exists(const char *fname)
{
    return(GetFileAttributes(fname) != 0xffffffff);
} /* file_exists */


int file_is_directory(const char *fname)
{
    return((GetFileAttributes(fname) & FILE_ATTRIBUTE_DIRECTORY) != 0);
} /* file_is_directory */


int file_is_symlink(const char *fname)
{
    return(0);
} /* file_is_symlink */


static const char *win32strerror(void)
{
    static TCHAR msgbuf[255];

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
        msgbuf,
        sizeof (msgbuf) / sizeof (TCHAR),
        NULL 
    );

    return((const char *) msgbuf);
} /* win32strerror */


/* enumerate contents of (base) directory. */
file_list *make_filelist(const char *base)
{
    file_list *retval = NULL;
    file_list *l = NULL;
    file_list *prev = NULL;
    HANDLE dir;
    WIN32_FIND_DATA ent;
    char wildcard[MAX_PATH];
    snprintf(wildcard, sizeof (wildcard), "%s\\*", base);

    dir = FindFirstFile(wildcard, &ent);
    if (dir == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Error: Can't list files in %s: %s.\n", base, win32strerror());
        return(NULL);
    } /* if */

    while (FindNextFile(dir, &ent) != 0)
    {
        if (strcmp(ent.cFileName, ".") == 0)
            continue;

        if (strcmp(ent.cFileName, "..") == 0)
            continue;

        l = (file_list *) malloc(sizeof (file_list));
        if (l == NULL)
        {
            fprintf(stderr, "Error: out of memory.\n");
            break;
        } /* if */

        l->fname = (char *) malloc(strlen(ent.cFileName) + 1);
        if (l->fname == NULL)
        {
            fprintf(stderr, "Error: out of memory.\n");
            free(l);
            break;
        } /* if */

        strcpy(l->fname, ent.cFileName);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* while */

    FindClose(dir);
    return(retval);
} /* make_filelist */


int get_file_size(const char *fname, unsigned int *fsize)
{
    DWORD FileSz;
    DWORD FileSzHigh;
    HANDLE hFil;

    assert(fsize != NULL);
	
	hFil = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFil == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Error: Couldn't get size of [%s]: %s.\n", fname, win32strerror());
        return(0);
    } /* if */

    FileSz = GetFileSize(hFil, &FileSzHigh);
    assert(FileSzHigh == 0);
    CloseHandle(hFil);
    *fsize = FileSz;
    return(1);
} /* get_file_size */


char *get_current_dir(char *buf, size_t bufsize);
{
    DWORD buflen = GetCurrentDirectory(bufsize, buf);
    if (buflen <= bufsize)
    {
        *buf = '\0';
        return(NULL);
    } /* if */

    if (buf[buflen - 2] != '\\')
        strcat(buf, "\\");

    return(buf);
} /* get_current_dir */


char *get_realpath(const char *path)
{
    char *retval = NULL;
    char *p = NULL;

    retval = (char *) malloc(MAX_PATH);
    if (retval == NULL)
    {
        fprintf(stderr, "Error: out of memory.\n");
        return(NULL);
    } /* if */

        /*
         * If in \\server\path format, it's already an absolute path.
         *  We'll need to check for "." and ".." dirs, though, just in case.
         */
    if ((path[0] == '\\') && (path[1] == '\\'))
        strcpy(retval, path);

    else
    {
        char *currentDir = get_current_dir();
        if (currentDir == NULL)
        {
            free(retval);
            fprintf(stderr, "Error: out of memory.\n");
            return(NULL);
        } /* if */

        if (path[1] == ':')   /* drive letter specified? */
        {
            /*
             * Apparently, "D:mypath" is the same as "D:\\mypath" if
             *  D: is not the current drive. However, if D: is the
             *  current drive, then "D:mypath" is a relative path. Ugh.
             */
            if (path[2] == '\\')  /* maybe an absolute path? */
                strcpy(retval, path);
            else  /* definitely an absolute path. */
            {
                if (path[0] == currentDir[0]) /* current drive; relative. */
                {
                    strcpy(retval, currentDir);
                    strcat(retval, path + 2);
                } /* if */

                else  /* not current drive; absolute. */
                {
                    retval[0] = path[0];
                    retval[1] = ':';
                    retval[2] = '\\';
                    strcpy(retval + 3, path + 2);
                } /* else */
            } /* else */
        } /* if */

        else  /* no drive letter specified. */
        {
            if (path[0] == '\\')  /* absolute path. */
            {
                retval[0] = currentDir[0];
                retval[1] = ':';
                strcpy(retval + 2, path);
            } /* if */
            else
            {
                strcpy(retval, currentDir);
                strcat(retval, path);
            } /* else */
        } /* else */

        free(currentDir);
    } /* else */

    /* (whew.) Ok, now take out "." and ".." path entries... */

    p = retval;
    while ( (p = strstr(p, "\\.")) != NULL)
    {
            /* it's a "." entry that doesn't end the string. */
        if (p[2] == '\\')
            memmove(p + 1, p + 3, strlen(p + 3) + 1);

            /* it's a "." entry that ends the string. */
        else if (p[2] == '\0')
            p[0] = '\0';

            /* it's a ".." entry. */
        else if (p[2] == '.')
        {
            char *prevEntry = p - 1;
            while ((prevEntry != retval) && (*prevEntry != '\\'))
                prevEntry--;

            if (prevEntry == retval)  /* make it look like a "." entry. */
                memmove(p + 1, p + 2, strlen(p + 2) + 1);
            else
            {
                if (p[3] != '\0') /* doesn't end string. */
                    *prevEntry = '\0';
                else /* ends string. */
                    memmove(prevEntry + 1, p + 4, strlen(p + 4) + 1);

                p = prevEntry;
            } /* else */
        } /* else if */

        else
        {
            p++;  /* look past current char. */
        } /* else */
    } /* while */

        /* shrink the retval's memory block if possible... */
    p = (char *) realloc(retval, strlen(retval) + 1);
    if (p != NULL)
        retval = p;

    return(retval);
} /* get_realpath */


int main(int argc, char **argv)   /* !!! FIXME: WinMain, in the future? */
{
    return(mojopatch_main(argc, argv));
} /* main */

/* end of platform_win32.c ... */


#include <Carbon/Carbon.h>
#include <sys/param.h>
#include <sys/stat.h>

int chdir_by_identifier(const char *str)
{
    char buf[MAXPATHLEN];
    Boolean b;
    OSStatus rc;
    CFIndex len;
    CFURLRef url = NULL;
    CFStringRef id = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);

    rc = LSFindApplicationForInfo(kLSUnknownCreator, id, NULL, NULL, &url);
    CFRelease(id);
    if (rc != noErr)
        return(0);

    b = CFURLGetFileSystemRepresentation(url, TRUE, buf, sizeof (buf));
    CFRelease(url);
    if (!b)
        return(0);

printf("chdir to %s\n", buf);
    return(chdir(buf) == 0);
} /* chdir_by_identifier */


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


int main(int argc, char **argv)
{
    int i;
    unsigned char m[16] = { 0x60, 0x5B, 0x8B, 0x7D, 0x5D, 0xC3, 0x27, 0xA9,
                            0xF3, 0x01, 0x50, 0xB0, 0x4B, 0x12, 0xA7, 0x49 };

    FILE *io = fopen("x.mojopatch", "ab");
    fseek(io, 287276354, SEEK_SET);
    fwrite(m, sizeof (m), 1, io);
    fclose(io);
    return(0);


    for (i = 1; i < argc; i++)
    {
        if (!chdir_by_identifier(argv[i]))
            printf("%s not found.\n", argv[i]);
        printf("%s is%s a symlink.\n", argv[i], file_is_symlink(argv[i]) ? "" : " NOT");
    }


    return(0);
}



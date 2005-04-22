
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "platform.h"
#include "ui.h"

static void ui_title_stdio(const char *str)
{
    printf("=== %s ===\n\n", str);
} /* ui_title_stdio */


static void ui_real_deinit_stdio(void)
{
    printf("\n\nHit enter to quit.\n\n");
    getchar();
} /* ui_deinit_stdio */


static void ui_pump_stdio(void)
{
    /* no-op. */
} /* ui_pump_stdio */


static void ui_add_to_log_stdio(const char *str, int debugging)
{
    printf("%s%s\n", debugging ? "debug: " : "", str);
} /* ui_add_to_log_stdio */


static void ui_fatal_stdio(const char *str)
{
    fprintf(stderr, "\n%s\n\n", str);
} /* ui_fatal_stdio */


static void ui_success_stdio(const char *str)
{
    fprintf(stderr, "\n%s\n\n", str);
} /* ui_success_stdio */


static void ui_total_progress_stdio(int percent)
{
    static int lastpercent = -1;
    if (percent != lastpercent)
    {
        lastpercent = percent;
        printf(".");
        if (percent == 100)
            printf("\n");
    } /* if */
} /* ui_total_progress_stdio */


static void ui_status_stdio(const char *str)
{
    printf("Current operation: %s\n", str);
} /* ui_status_stdio */


static int ui_prompt_yn_stdio(const char *question)
{
    int c;
    while (1)
    {
        printf("%s", question);
        c = toupper(getchar());
        if (c == 'N')
            return(0);
        else if ((c == 'Y') || (c == '\r') || (c == '\n'))
            return(1);
        printf("\n");
    } /* while */

    return(1);
} /* ui_prompt_yn_stdio */


static int ui_prompt_ny_stdio(const char *question)
{
    int c;
    while (1)
    {
        printf("%s", question);
        c = toupper(getchar());
        if (c == 'Y')
            return 1;
        else if ((c == 'N') || (c == '\r') || (c == '\n'))
            return 0;
        printf("\n");
    } /* while */

    return(0);
} /* ui_prompt_ny_stdio */


static void ui_msgbox_stdio(const char *str)
{
    printf("\n\n-----\n%s\n-----\n\nHit enter to continue.\n\n", str);
    getchar();
} /* ui_msgbox_stdio */


static int ui_file_picker_stdio(char *buf, size_t bufsize)
{
    while (1)
    {
        size_t len;
        puts("Please enter the path, or blank to cancel.\n> ");

        buf[0] = '\0';
        fgets(buf, bufsize, stdin);
        len = strlen(buf);
        while ((len > 0) && ((buf[len-1] == '\n') || (buf[len-1] == '\r')))
            buf[--len] = '\0';

        if (len == 0)
            return(0);  /* user "cancelled". */

        if (file_exists(buf))
            return(1);  /* user entered valid path. */

        puts("That path does not exist. Please try again.\n\n");
    } /* while */

    return(0);  /* should never hit this. */
} /* ui_file_picker_stdio */


static int ui_show_readme_stdio(const char *fname,const char *text)
{
    /*
     * Cheat for now and push this off to "less", which will warn if the
     *  readme is a binary file and prompt the user about avoiding it.
     */

    size_t allocsize = strlen(fname) + 32;
    char *cmd = (char *) alloca(allocsize);
    if (!cmd)
    {
        _fatal("Out of memory.");
        return(0);
    } /* if */

    snprintf(cmd, allocsize, "less %s", fname);
    system(cmd);  /* !!! FIXME: error check? */
    return(1);
} /* ui_show_readme_stdio */


int ui_init_stdio(void)
{
    UI_SET_FUNC_POINTERS(stdio);
    return(1); /* always succeeds. */
} /* ui_init */

/* end of ui_stdio.c ... */


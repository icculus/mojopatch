
#include <stdio.h>
#include <ctype.h>

#include "platform.h"
#include "ui.h"

int ui_init(void)
{
    return(1); /* always succeeds. */
} /* ui_init */


void ui_title(const char *str)
{
} /* ui_title */


void ui_deinit(void)
{
    printf("\n\nHit enter to quit.\n\n");
    getchar();
} /* ui_deinit */


void ui_pump(void)
{
    /* no-op. */
} /* ui_pump */


void ui_add_to_log(const char *str, int debugging)
{
    printf("%s%s\n", debugging ? "debug: " : "", str);
} /* ui_add_to_log */


void ui_fatal(const char *str)
{
    fprintf(stderr, "\n%s\n\n", str);
} /* ui_fatal */


void ui_success(const char *str)
{
    fprintf(stderr, "\n%s\n\n", str);
} /* ui_success */


void ui_total_progress(int percent)
{
    static int lastpercent = -1;
    if (percent != lastpercent)
    {
        lastpercent = percent;
        printf(".");
        if (percent == 100)
            printf("\n");
    } /* if */
} /* ui_total_progress */


void ui_status(const char *str)
{
    printf("Current operation: %s\n", str);
} /* ui_status */


int ui_prompt_yn(const char *question)
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
} /* ui_prompt_yn */


int ui_prompt_ny(const char *question)
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
} /* ui_prompt_ny */

/* end of ui_stdio.h ... */


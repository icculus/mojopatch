#include <Carbon/Carbon.h>

#include "platform.h"
#include "ui.h"

#define MOJOPATCH_SIG         'mjpt'
#define MOJOPATCH_STATUS_ID   0
#define MOJOPATCH_PROGRESS_ID 1

static WindowPtr window;
static ControlRef progress;
static ControlRef status;

/* user interface stuff you implement. */
int ui_init(void)
{
    ControlID statusID = { MOJOPATCH_SIG, MOJOPATCH_STATUS_ID };
    ControlID progressID = { MOJOPATCH_SIG, MOJOPATCH_PROGRESS_ID };
    IBNibRef nibRef;
    OSStatus err;
    Boolean b = TRUE;

    CreateNibReference( CFSTR("mojopatch"), &nibRef );
    err = SetMenuBarFromNib(nibRef, CFSTR("MenuBar"));
    CreateWindowFromNib( nibRef, CFSTR("MainWindow"), &window );
    DisposeNibReference( nibRef );
    GetControlByID(window, &statusID, &status);
    GetControlByID(window, &progressID, &progress);
    ShowWindow( window );
    ActivateWindow(window, TRUE);
    SetControlData(progress, kControlEntireControl,
                    kControlProgressBarAnimatingTag,
                    sizeof (b), &b);
    return(1);
} /* ui_init */


void ui_title(const char *str)
{
    CFStringRef cfstr = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    SetWindowTitleWithCFString(window, cfstr);
} /* ui_title */


void ui_deinit(void)
{
    /* !!! FIXME */
} /* ui_deinit */


void ui_pump(void)
{
    EventRef theEvent;
    EventTargetRef theTarget = GetEventDispatcherTarget();
    if (ReceiveNextEvent(0, NULL, 0, true, &theEvent) == noErr)
    {
        SendEventToEventTarget(theEvent, theTarget);
        ReleaseEvent(theEvent);
    } /* if */
} /* ui_pump */


void ui_add_to_log(const char *str, int debugging)
{
    // !!! FIXME
    printf("MojoPatch%s: %s\n", debugging ? " [debug]" : "", str);
} /* ui_add_to_log */


static void do_msgbox(const char *str, AlertType alert_type)
{
    const char *_title = "MojoPatch";
    CFStringRef title = CFStringCreateWithBytes(NULL, _title, strlen(_title),
                                                kCFStringEncodingISOLatin1, 0);
    CFStringRef msg = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    if ((msg != NULL) && (title != NULL))
    {
        DialogItemIndex val = 0;
        DialogRef dlg = NULL;

        if (CreateStandardAlert(alert_type, title, msg, NULL, &dlg) == noErr)
            RunStandardAlert(dlg, NULL, &val);
    } /* if */

    if (msg != NULL)
        CFRelease(msg);

    if (title != NULL)
        CFRelease(title);
} /* do_msgbox */


void ui_fatal(const char *str)
{
    do_msgbox(str, kAlertStopAlert);
} /* ui_fatal */


void ui_success(const char *str)
{
    do_msgbox(str, kAlertNoteAlert);
} /* ui_success */


void ui_total_progress(int percent)
{
    static int lastpercent = -1;
    if (percent != lastpercent)
    {
        Boolean indeterminate = (percent < 0) ? TRUE : FALSE;
        SetControlData(progress, kControlEntireControl,
                        kControlProgressBarIndeterminateTag,
                        sizeof (indeterminate), &indeterminate);
        SetControl32BitValue(progress, percent);
        lastpercent = percent;
    } /* if */
} /* ui_total_progress */


void ui_status(const char *str)
{
    SetControlData(status, kControlEditTextPart, kControlStaticTextTextTag,
                    strlen(str), str);
    Draw1Control(status);
} /* ui_status */

/* end of ui_carbon.c ... */


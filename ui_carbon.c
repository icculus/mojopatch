#include <Carbon/Carbon.h>

#include "platform.h"
#include "ui.h"

#define MOJOPATCH_SIG         'mjpt'
#define MOJOPATCH_STATUS_ID   0
#define MOJOPATCH_PROGRESS_ID 1

static WindowPtr window;
static ControlRef progress;
static ControlRef status;
static int carbon_ui_initialized = 0;

/* user interface stuff you implement. */
int ui_init(void)
{
    ControlID statusID = { MOJOPATCH_SIG, MOJOPATCH_STATUS_ID };
    ControlID progressID = { MOJOPATCH_SIG, MOJOPATCH_PROGRESS_ID };
    IBNibRef nibRef;
    OSStatus err;
    Boolean b = TRUE;

    if (carbon_ui_initialized)  /* already initialized? */
        return(1);

    if (CreateNibReference(CFSTR("mojopatch"), &nibRef) != noErr)
    {
        fprintf(stderr, "MOJOPATCH: You probably don't have a .nib file!\n");
        return(0);  /* usually .nib isn't found. */
    } /* if */

    err = SetMenuBarFromNib(nibRef, CFSTR("MenuBar"));
    if (err == noErr)
        err = CreateWindowFromNib(nibRef, CFSTR("MainWindow"), &window);
    DisposeNibReference( nibRef );

    if (err == noErr)
        err = GetControlByID(window, &statusID, &status);

    if (err == noErr)
        err = GetControlByID(window, &progressID, &progress);

    if (err == noErr)
    {
        ShowWindow(window);
        err = ActivateWindow(window, TRUE);
    } /* if */

    if (err == noErr)
    {
        err = SetControlData(progress, kControlEntireControl,
                              kControlProgressBarAnimatingTag,
                              sizeof (b), &b);
    } /* if */

    carbon_ui_initialized = 1;
    return(err == noErr);
} /* ui_init */


void ui_title(const char *str)
{
    CFStringRef cfstr = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    SetWindowTitleWithCFString(window, cfstr);
    ui_pump();
} /* ui_title */


void ui_deinit(void)
{
    /* !!! FIXME */
    /* carbon_ui_initialized = 0; */
} /* ui_deinit */


void ui_pump(void)
{
    EventRef theEvent;
    EventTargetRef theTarget;

    if (!carbon_ui_initialized)
        return;

    theTarget = GetEventDispatcherTarget();
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


static int do_msgbox(const char *str, AlertType alert_type,
                     AlertStdCFStringAlertParamRec *param,
                     DialogItemIndex *idx)
{
    const char *_title = "MojoPatch";
    int retval = 0;
    DialogItemIndex val = 0;
    CFStringRef title = CFStringCreateWithBytes(NULL, _title, strlen(_title),
                                                kCFStringEncodingISOLatin1, 0);
    CFStringRef msg = CFStringCreateWithBytes(NULL, str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    if ((msg != NULL) && (title != NULL))
    {
        DialogRef dlg = NULL;

        if (CreateStandardAlert(alert_type, title, msg, param, &dlg) == noErr)
        {
            RunStandardAlert(dlg, NULL, (idx) ? idx : &val);
            retval = 1;
        } /* if */
    } /* if */

    if (msg != NULL)
        CFRelease(msg);

    if (title != NULL)
        CFRelease(title);

    return(retval);
} /* do_msgbox */


static int ui_prompt_yes_or_no(const char *question, int yes)
{
    OSStatus err;
    DialogItemIndex item;
    AlertStdCFStringAlertParamRec params;
    err = GetStandardAlertDefaultParams(&params, kStdCFStringAlertVersionOne);
    if (err != noErr)
        return(0);

    params.movable = TRUE;
    params.helpButton = FALSE;
    params.defaultText = CFSTR("Yes");
    params.cancelText = CFSTR("No");
    params.defaultButton = (yes) ? kAlertStdAlertOKButton :
                                   kAlertStdAlertCancelButton;
    params.cancelButton = kAlertStdAlertCancelButton;
    if (!do_msgbox(question, kAlertCautionAlert, &params, &item))
        return(0); /* oh well. */

    return(item == kAlertStdAlertOKButton);
} /* ui_prompt_yes_or_no */

int ui_prompt_yn(const char *question)
{
    return(ui_prompt_yes_or_no(question, 1));
} /* ui_prompt_yn */

int ui_prompt_ny(const char *question)
{
    return(ui_prompt_yes_or_no(question, 1));  /* !!! FIXME! should be zero. */
} /* ui_prompt_ny */


int manually_locate_product(const char *name, char *buf, size_t bufsize)
{
    NavDialogCreationOptions dlgopt;
    NavDialogRef dlg;
    NavReplyRecord reply;
    NavUserAction action;
    AEKeyword keyword;
    AEDesc desc;
    FSRef fsref;
    OSStatus rc;
    int retval = 0;
    int yn;
    const char *promptfmt = "We can't find your \"%s\" installation."
                            " Would you like to show us where it is?";
    char *promptstr = alloca(strlen(name) + strlen(promptfmt) + 1);

    if (promptstr == NULL)
    {
        _fatal("Out of memory.");
        return(0);
    } /* if */
    sprintf(promptstr, promptfmt, name);

    yn = ui_prompt_yn(promptstr);
    if (!yn)
    {
        _log("User chose not to manually locate installation");
        return(0);
    } /* if */

    NavGetDefaultDialogCreationOptions(&dlgopt);
    dlgopt.optionFlags |= kNavSupportPackages;
    dlgopt.optionFlags |= kNavAllowOpenPackages;
    dlgopt.optionFlags &= ~kNavAllowMultipleFiles;
    dlgopt.windowTitle = CFSTR("Please select the product's icon and click 'OK'.");
    dlgopt.actionButtonLabel = CFSTR("OK");
    NavCreateChooseFolderDialog(&dlgopt, NULL, NULL, NULL, &dlg);
    NavDialogRun(dlg);
    action = NavDialogGetUserAction(dlg);
    if (action == kNavUserActionCancel)
        _log("User cancelled file selector!");
    else
    {
        NavDialogGetReply(dlg, &reply);
        rc = AEGetNthDesc(&reply.selection, 1, typeFSRef, &keyword, &desc);
        if (rc != noErr)
            _fatal("Unexpected error in AEGetNthDesc: %d\n", (int) rc);
        else
        {
            /* !!! FIXME: Check return values here! */
            BlockMoveData(*desc.dataHandle, &fsref, sizeof (fsref));
            FSRefMakePath(&fsref, buf, bufsize - 1);
            buf[bufsize - 1] = '\0';
            AEDisposeDesc(&desc);
            retval = 1;
        } /* if */

        NavDisposeReply(&reply);
    } /* else */

    NavDialogDispose(dlg);

    _log("File selector complete. User %s path.",
            retval ? "selected" : "did NOT select");

    return(retval);
}


void ui_fatal(const char *str)
{
    if (!carbon_ui_initialized)
        fprintf(stderr, "FATAL ERROR: %s\n", str);
    else
        do_msgbox(str, kAlertStopAlert, NULL, NULL);
} /* ui_fatal */


void ui_success(const char *str)
{
    do_msgbox(str, kAlertNoteAlert, NULL, NULL);
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


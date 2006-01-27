
#if !PLATFORM_MACOSX
int ui_init_carbon(void) { return(0); } /* not implemented if not MacOS. */
#else

#include <Carbon/Carbon.h>

#include "platform.h"
#include "ui.h"

#define MOJOPATCH_SIG         'mjpt'
#define MOJOPATCH_STATUS_ID   0
#define MOJOPATCH_PROGRESS_ID 1

static WindowRef window;
static ControlRef progress;
static ControlRef status;

static void ui_pump_carbon(void)
{
    EventRef theEvent;
    EventTargetRef theTarget;

    theTarget = GetEventDispatcherTarget();
    if (ReceiveNextEvent(0, NULL, 0, true, &theEvent) == noErr)
    {
        SendEventToEventTarget(theEvent, theTarget);
        ReleaseEvent(theEvent);
    } /* if */
} /* ui_pump_carbon */


static void ui_title_carbon(const char *str)
{
    CFStringRef cfstr = CFStringCreateWithBytes(NULL,
                                                (const unsigned char *) str,
                                                strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    SetWindowTitleWithCFString(window, cfstr);
    CFRelease(cfstr);
    ui_pump_carbon();
} /* ui_title_carbon */


static void ui_real_deinit_carbon(void)
{
    /* !!! FIXME */
} /* ui_real_deinit_carbon */


static void ui_add_to_log_carbon(const char *str, int debugging)
{
    /*
     * stdout in a Mac GUI app shows up in the system console, which can be
     *  viewed via /Applications/Utilities/Console.app ...
     */

    /* !!! FIXME */
    printf("MojoPatch%s: %s\n", debugging ? " [debug]" : "", str);
} /* ui_add_to_log_carbon */


static int do_msgbox(const char *str, AlertType alert_type,
                     AlertStdCFStringAlertParamRec *param,
                     DialogItemIndex *idx)
{
    const char *_title = "MojoPatch";
    int retval = 0;
    DialogItemIndex val = 0;
    CFStringRef title = CFStringCreateWithBytes(NULL,
                                                (const unsigned char *) _title,
                                                strlen(_title),
                                                kCFStringEncodingISOLatin1, 0);
    CFStringRef msg = CFStringCreateWithBytes(NULL,
                                              (const unsigned char *) str,
                                              strlen(str),
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


static int ui_prompt_yn_carbon(const char *question)
{
    return(ui_prompt_yes_or_no(question, 1));
} /* ui_prompt_yn_carbon */


static int ui_prompt_ny_carbon(const char *question)
{
    return(ui_prompt_yes_or_no(question, 1));  /* !!! FIXME! should be zero. */
} /* ui_prompt_ny_carbon */


static int ui_file_picker_carbon(char *buf, size_t bufsize)
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

    NavGetDefaultDialogCreationOptions(&dlgopt);
    dlgopt.optionFlags |= kNavSupportPackages;
    dlgopt.optionFlags |= kNavAllowOpenPackages;
    dlgopt.optionFlags &= ~kNavAllowMultipleFiles;
    dlgopt.windowTitle = CFSTR("Please select the product's icon and click 'OK'.");  /* !!! FIXME! */
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
            _fatal("Unexpected error in AEGetNthDesc: %d", (int) rc);
        else
        {
            /* !!! FIXME: Check return values here! */
            BlockMoveData(*desc.dataHandle, &fsref, sizeof (fsref));
            FSRefMakePath(&fsref, (unsigned char *) buf, bufsize - 1);
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
} /* ui_file_picker_carbon */


static void ui_fatal_carbon(const char *str)
{
    do_msgbox(str, kAlertStopAlert, NULL, NULL);
} /* ui_fatal */


static void ui_success_carbon(const char *str)
{
    do_msgbox(str, kAlertNoteAlert, NULL, NULL);
} /* ui_success_carbon */


static void ui_msgbox_carbon(const char *str)
{
    do_msgbox(str, kAlertNoteAlert, NULL, NULL);
} /* ui_msgbox_carbon */


static void ui_total_progress_carbon(int percent)
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
} /* ui_total_progress_carbon */


static void ui_status_carbon(const char *str)
{
    SetControlData(status, kControlEditTextPart, kControlStaticTextTextTag,
                    strlen(str), str);
    Draw1Control(status);
} /* ui_status_carbon */


static int ui_show_readme_carbon(const char *fname, const char *text)
{
    /*
     * Just let the Finder pick the right program to view the file...
     *  this lets you ship with a .txt, .html, .rtf, or whatever, for a
     *  readme on this platform.
     */

    size_t allocsize = strlen(fname) + 32;
    char *cmd = (char *) alloca(allocsize);
    if (!cmd)
    {
        _fatal("Out of memory.");
        return(0);
    } /* if */

    snprintf(cmd, allocsize, "open %s", fname);
    system(cmd);  /* !!! FIXME: error check? */
    return(1);
} /* ui_show_readme_carbon */


/* user interface stuff you implement. */
int ui_init_carbon(void)
{
    ControlID statusID = { MOJOPATCH_SIG, MOJOPATCH_STATUS_ID };
    ControlID progressID = { MOJOPATCH_SIG, MOJOPATCH_PROGRESS_ID };
    IBNibRef nibRef = NULL;
    OSStatus err;
    Boolean b = TRUE;

    /* !!! FIXME: This is corrupting the "basedir" variable in platform_unix.c ! */
    if (CreateNibReference(CFSTR("mojopatch"), &nibRef) != noErr)
    {
        fprintf(stderr, "MOJOPATCH: Carbon UI failed to initialize!\n");
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

    if (err == noErr);
        UI_SET_FUNC_POINTERS(carbon);

    return(err == noErr);
} /* ui_init_carbon */

#endif

/* end of ui_carbon.c ... */


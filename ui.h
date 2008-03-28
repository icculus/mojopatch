/**
 * MojoPatch; a tool for updating data in the field.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCL_UI_H_
#define _INCL_UI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* user interface stuff you implement: see ui.c and ui_stdio.c for examples */
int ui_init(const char *request_driver);
void ui_deinit(void);
extern void (*ui_pump)(void);
extern void (*ui_add_to_log)(const char *str, int debugging);
extern void (*ui_fatal)(const char *str);
extern void (*ui_success)(const char *str);
extern void (*ui_msgbox)(const char *str);
extern void (*ui_total_progress)(int percent);
extern void (*ui_status)(const char *str);
extern void (*ui_title)(const char *str);
extern int (*ui_prompt_yn)(const char *question);
extern int (*ui_prompt_ny)(const char *question);
extern int (*ui_file_picker)(char *buf, size_t bufsize);
extern int (*ui_show_readme)(const char *fname, const char *text);

/* this is for the UI layer, the application uses ui_deinit() instead... */
extern void (*ui_real_deinit)(void);

/*
 * Macros for use by the UI layer. The application should ignore this.
 *
 * (This all feels a little naughty, but it guarantees we'll catch it when a
 *  a new entry point is added and a driver hasn't been updated.)
 */
#define UI_SET_FUNC_POINTER(func,drv) { func = func##_##drv; }
#define UI_SET_FUNC_POINTERS(drv) \
{ \
    UI_SET_FUNC_POINTER(ui_real_deinit,drv) \
    UI_SET_FUNC_POINTER(ui_pump,drv) \
    UI_SET_FUNC_POINTER(ui_add_to_log,drv) \
    UI_SET_FUNC_POINTER(ui_fatal,drv) \
    UI_SET_FUNC_POINTER(ui_success,drv) \
    UI_SET_FUNC_POINTER(ui_msgbox,drv) \
    UI_SET_FUNC_POINTER(ui_total_progress,drv) \
    UI_SET_FUNC_POINTER(ui_status,drv) \
    UI_SET_FUNC_POINTER(ui_title,drv) \
    UI_SET_FUNC_POINTER(ui_prompt_yn,drv) \
    UI_SET_FUNC_POINTER(ui_prompt_ny,drv) \
    UI_SET_FUNC_POINTER(ui_file_picker,drv) \
    UI_SET_FUNC_POINTER(ui_show_readme,drv) \
}

#ifdef __cplusplus
}
#endif

#endif  /* include-once blocker. */

/* end of ui.h ... */


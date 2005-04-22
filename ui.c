
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "platform.h"
#include "ui.h"

/* stubs for when a driver isn't selected. */
static void null_ui(void) { assert(0 && "UI call without UI driver!"); }
static void ui_pump_null(void) { null_ui(); }
static void ui_add_to_log_null(const char *str, int debugging) { null_ui(); }
static void ui_fatal_null(const char *str) { null_ui(); }
static void ui_success_null(const char *str) { null_ui(); }
static void ui_msgbox_null(const char *str) { null_ui(); }
static void ui_total_progress_null(int percent) { null_ui(); }
static void ui_status_null(const char *str) { null_ui(); }
static void ui_title_null(const char *str) { null_ui(); }
static int ui_prompt_yn_null(const char *question) { null_ui(); return(0); }
static int ui_prompt_ny_null(const char *question) { null_ui(); return(0); }
static void ui_real_deinit_null(void) { null_ui(); }
static int ui_file_picker_null(char *buf, size_t bufsize) { null_ui(); return(0); }
static int ui_show_readme_null(const char *fname,const char *text) { null_ui(); return(0); }


static int ui_selected = 0;

int ui_init_carbon(void);
int ui_init_stdio(void);

typedef struct
{
    const char *driver_name;
    int (*initfunc)(void);
} UIDrivers;

static UIDrivers ui_drivers[] =
{
    { "carbon", ui_init_carbon },
    { "stdio", ui_init_stdio },  /* should probably always be last. */
    { NULL, NULL }
};

int ui_init(const char *request_driver)
{
    int i;

    if (ui_selected)
        return(1);

    /* make sure we're in a sane state... */
    UI_SET_FUNC_POINTERS(null);

    if (request_driver != NULL)
    {
        for (i = 0; ui_drivers[i].initfunc != NULL; i++)
        {
            if (strcmp(ui_drivers[i].driver_name, request_driver) == 0)
            {
                ui_selected = ui_drivers[i].initfunc();
                break;
            } /* if */
        } /* for */
    } /* if */

    for (i = 0; (!ui_selected) && (ui_drivers[i].initfunc != NULL); i++)
        ui_selected = ui_drivers[i].initfunc();

    return(ui_selected);
} /* ui_init */


void ui_deinit(void)
{
    if (!ui_selected)
        return;

    ui_real_deinit();
    UI_SET_FUNC_POINTERS(null);
    ui_selected = 0;
} /* ui_deinit */


/* The driver function pointers... */
void (*ui_real_deinit)(void) = ui_real_deinit_null;
void (*ui_pump)(void) = ui_pump_null;
void (*ui_add_to_log)(const char *str, int debugging) = ui_add_to_log_null;
void (*ui_fatal)(const char *str) = ui_fatal_null;
void (*ui_success)(const char *str) = ui_success_null;
void (*ui_msgbox)(const char *str) = ui_msgbox_null;
void (*ui_total_progress)(int percent) = ui_total_progress_null;
void (*ui_status)(const char *str) = ui_status_null;
void (*ui_title)(const char *str) = ui_title_null;
int (*ui_prompt_yn)(const char *question) = ui_prompt_yn_null;
int (*ui_prompt_ny)(const char *question) = ui_prompt_ny_null;
int (*ui_file_picker)(char *buf, size_t bufsize) = ui_file_picker_null;
int (*ui_show_readme)(const char *fname,const char *text)=ui_show_readme_null;

/* end of ui.c ... */


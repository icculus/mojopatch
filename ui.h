
#ifndef _INCL_UI_H_
#define _INCL_UI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* user interface stuff you implement. */
int ui_init(void);
void ui_deinit(void);
void ui_pump(void);
void ui_add_to_log(const char *str, int debugging);
void ui_fatal(const char *str);
void ui_success(const char *str);
void ui_total_progress(int percent);
void ui_status(const char *str);
void ui_title(const char *str);
int ui_prompt_yn(const char *question);
int ui_prompt_ny(const char *question);

#ifdef __cplusplus
}
#endif

#endif  /* include-once blocker. */

/* end of ui.h ... */


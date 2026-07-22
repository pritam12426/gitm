#ifndef _ANSI_COLOR_H_
#define _ANSI_COLOR_H_



// -- ANSI Escape Codes -----------------------------------------------------------
#define ANSI_ESC   "\x1b["
#define ANSI_RESET "\x1b[0m"


// -- Text Styles -----------------------------------------------------------------
#define ANSI_BOLD             "\x1b[1m"
#define ANSI_DIM              "\x1b[2m"
#define ANSI_ITALIC           "\x1b[3m"
#define ANSI_UNDERLINE        "\x1b[4m"
#define ANSI_BLINK            "\x1b[5m"
#define ANSI_REVERSE          "\x1b[7m"
#define ANSI_HIDDEN           "\x1b[8m"
#define ANSI_STRIKE           "\x1b[9m"

#define ANSI_NORMAL_INTENSITY "\x1b[22m"
#define ANSI_NO_ITALIC        "\x1b[23m"
#define ANSI_NO_UNDERLINE     "\x1b[24m"
#define ANSI_NO_BLINK         "\x1b[25m"
#define ANSI_NO_REVERSE       "\x1b[27m"
#define ANSI_NO_HIDDEN        "\x1b[28m"
#define ANSI_NO_STRIKE        "\x1b[29m"


// -- Foreground Colors (Normal) --------------------------------------------------
#define ANSI_FG_BLACK   "\x1b[30m"
#define ANSI_FG_RED     "\x1b[31m"
#define ANSI_FG_GREEN   "\x1b[32m"
#define ANSI_FG_YELLOW  "\x1b[33m"
#define ANSI_FG_BLUE    "\x1b[34m"
#define ANSI_FG_MAGENTA "\x1b[35m"
#define ANSI_FG_CYAN    "\x1b[36m"
#define ANSI_FG_WHITE   "\x1b[37m"

#define ANSI_FG_DEFAULT "\x1b[39m"

// -- Bright Foreground Colors ----------------------------------------------------
#define ANSI_FG_BRIGHT_BLACK   "\x1b[90m"
#define ANSI_FG_BRIGHT_RED     "\x1b[91m"
#define ANSI_FG_BRIGHT_GREEN   "\x1b[92m"
#define ANSI_FG_BRIGHT_YELLOW  "\x1b[93m"
#define ANSI_FG_BRIGHT_BLUE    "\x1b[94m"
#define ANSI_FG_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_FG_BRIGHT_CYAN    "\x1b[96m"
#define ANSI_FG_BRIGHT_WHITE   "\x1b[97m"


// -- Background Colors -----------------------------------------------------------
#define ANSI_BG_BLACK   "\x1b[40m"
#define ANSI_BG_RED     "\x1b[41m"
#define ANSI_BG_GREEN   "\x1b[42m"
#define ANSI_BG_YELLOW  "\x1b[43m"
#define ANSI_BG_BLUE    "\x1b[44m"
#define ANSI_BG_MAGENTA "\x1b[45m"
#define ANSI_BG_CYAN    "\x1b[46m"
#define ANSI_BG_WHITE   "\x1b[47m"

#define ANSI_BG_DEFAULT "\x1b[49m"


// -- Bright Background Colors ----------------------------------------------------
#define ANSI_BG_BRIGHT_BLACK   "\x1b[100m"
#define ANSI_BG_BRIGHT_RED     "\x1b[101m"
#define ANSI_BG_BRIGHT_GREEN   "\x1b[102m"
#define ANSI_BG_BRIGHT_YELLOW  "\x1b[103m"
#define ANSI_BG_BRIGHT_BLUE    "\x1b[104m"
#define ANSI_BG_BRIGHT_MAGENTA "\x1b[105m"
#define ANSI_BG_BRIGHT_CYAN    "\x1b[106m"
#define ANSI_BG_BRIGHT_WHITE   "\x1b[107m"


// -- 256-Color Helpers -----------------------------------------------------------
#define ANSI_FG_256(n) "\x1b[38;5;" #n "m"
#define ANSI_BG_256(n) "\x1b[48;5;" #n "m"

/*
 * NOTE:
 * ANSI_FG_256() and ANSI_BG_256() only work with compile-time
 * constants because they rely on stringification.
 *
 * Example:
 *
 * printf(ANSI_FG_256(196) "Hello" ANSI_RESET);
 */

/* ============================================================
 * True Color (24-bit)
 * ============================================================
 */

/*
 * These must be generated at runtime.
 *
 * Foreground:
 *   \x1b[38;2;<R>;<G>;<B>m
 *
 * Background:
 *   \x1b[48;2;<R>;<G>;<B>m
 *
 * Example:
 *
 * printf("\x1b[38;2;255;128;0mOrange\x1b[0m");
 */


// -- Cursor Control --------------------------------------------------------------
#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CLEAR_LINE   "\x1b[2K"
#define ANSI_HOME         "\x1b[H"

#define ANSI_SAVE_CURSOR    "\x1b[s"
#define ANSI_RESTORE_CURSOR "\x1b[u"

#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_SHOW_CURSOR "\x1b[?25h"


// -- Screen Control --------------------------------------------------------------
#define ANSI_ERASE_DOWN       "\x1b[J"
#define ANSI_ERASE_UP         "\x1b[1J"
#define ANSI_ERASE_LINE_RIGHT "\x1b[K"
#define ANSI_ERASE_LINE_LEFT  "\x1b[1K"


// -- Alternate Screen Buffer -----------------------------------------------------
#define ANSI_ALT_SCREEN_ENABLE  "\x1b[?1049h"
#define ANSI_ALT_SCREEN_DISABLE "\x1b[?1049l"



#endif  // _ANSI_COLOR_H_

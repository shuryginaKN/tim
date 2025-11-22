#include "tim_tcl_cmd_term.h"

#include "tim_a_protocol.h"
#include "tim_a_terminal.h"
#include "tim_tcl.h"
#include "tim_tcl_cmd.h"
#include "tim_trace.h"
#include "tim_translator.h"

#include "lil.hpp"

#include <cassert>
#include <cstring>


// Static

static lil_value_t tim_tcl_cmd_clear(lil_t lil, size_t argc, lil_value_t *argv)
{
    (void) argv;

    if (argc)
    {
        lil_set_error(lil,
                      TIM_TR("No arguments expected."_en,
                             "Команда не имеет параметров."_ru));
        return nullptr;
    }

    tim::tcl *tcl = (tim::tcl *)lil_get_data(lil);
    assert(tcl);

    tcl->terminal()->clear();

    return nullptr;
}

static lil_value_t tim_tcl_cmd_puts(lil_t lil, size_t argc, lil_value_t *argv)
{
    tim::tcl *tcl = (tim::tcl *)lil_get_data(lil);
    assert(tcl);

    switch (argc)
    {
        case 1:

            const char* text = lil_to_string(argv[0]);

            if (std::strncmp(text, "nick:", 5) == 0)
            {
                const char* nick = text + 5;   // UTF-8 строка ника
                tcl->terminal()->protocol()->write_str(
                    (std::string)"[setNick] " + nick + "\n"
                );

                // здесь вызывается логика изменения ника
                tim::app()->mqtt()->publish(
                    "setNick/" + tcl->terminal()->id().to_string(),
                    nick,
                    std::strlen(nick)
                );

                return nullptr;
            }

            if (std::strncmp(text, "icon:", 5) == 0)
            {
                const char* icon = text + 5;   // base64-данные
                tcl->terminal()->protocol()->write_str(
                    (std::string)"[setIcon] <image data>\n"
                );

                tim::app()->mqtt()->publish(
                    "setIcon/" + tcl->terminal()->id().to_string(),
                    icon,
                    std::strlen(icon)
                );

                return nullptr;
            }

            tcl->terminal()->protocol()->write_str(text);
            tcl->terminal()->protocol()->write("\n", 1);
            return nullptr;

        case 2:
            if (std::strcmp(lil_to_string(argv[0]), "-nonewline"))
            {
                lil_set_error(lil,
                              TIM_TR("When you give two arguments, the first argument must be -nonewline"_en,
                                     "В случае двух аргументов первым аргументом может быть только -nonewline"_ru));
                return nullptr;
            }

            const char* text = lil_to_string(argv[1]);

            if (std::strncmp(text, "nick:", 5) == 0)
            {
                const char* nick = text + 5;
                tim::app()->mqtt()->publish("setNick/" + tcl->terminal()->id().to_string(),
                                            nick, std::strlen(nick));
                return nullptr;
            }

            if (std::strncmp(text, "icon:", 5) == 0)
            {
                const char* icon = text + 5;
                tim::app()->mqtt()->publish("setIcon/" + tcl->terminal()->id().to_string(),
                                            icon, std::strlen(icon));
                return nullptr;
            }

            tcl->terminal()->protocol()->write_str(text);
            return nullptr;

        default:
            break;
    }

    lil_set_error(lil,
                  TIM_TR("Invalid number of arguments. Expecting ?-nonewline? string"_en,
                         "Некорректные аргументы. Ожидается ?-nonewline? string"_ru));
    return nullptr;
}

static lil_value_t tim_tcl_cmd_palette256(lil_t lil, size_t argc, lil_value_t *argv)
{
    (void) argv;

    if (argc)
    {
        lil_set_error(lil,
                      TIM_TR("No arguments expected."_en,
                            "Команда не имеет параметров."_ru));
        return nullptr;
    }

    tim::tcl *tcl = (tim::tcl *)lil_get_data(lil);
    assert(tcl);

    static const std::size_t ITEMS_PER_LINE = 10;

    for (std::size_t c = 0; c <= tcl->terminal()->color_count(); ++c)
    {
        if (c > 0
                && c % ITEMS_PER_LINE == 0)
            tcl->terminal()->protocol()->write("\n", 1);

        tcl->terminal()->printf("%3zu", c);
        tcl->terminal()->cprintf(tim::color{}, tcl->terminal()->color(c), "%s", "  ");
        tcl->terminal()->protocol()->write(" ", 1);
    }

    return nullptr;
}


// Public

void tim::tcl_add_term(lil_t lil)
{
    assert(lil);

    TIM_TCL_REGISTER(lil, clear);
    TIM_TCL_REGISTER(lil, puts);
    TIM_TCL_REGISTER(lil, palette256);
}

#include "tim_tcl.h"

#include "tim_tcl_p.h"

#include "tim_a_protocol.h"
#include "tim_a_terminal.h"
#include "tim_application.h"
#include "tim_string_tools.h"
#include "tim_translator.h"

// Commands
#include "tim_tcl_cmd_general.h"
#include "tim_tcl_cmd_term.h"
#include "tim_tcl_cmd_user.h"

#include "lil.hpp"
#include "utf8/utf8.h"

#include <cassert>


// Public

tim::tcl::tcl(tim::a_terminal *term, const tim::uuid &user_id)
    : tim::a_script_engine("Tcl", term)
    , _d(new tim::p::tcl(this))
{
    _d->_lil = lil_new();
    _d->_user_id = user_id;

    lil_callback(_d->_lil, LIL_CALLBACK_WRITE, (lil_callback_proc_t)tim::p::tcl::write);
    lil_callback(_d->_lil, LIL_CALLBACK_DISPATCH, (lil_callback_proc_t)tim::p::tcl::dispatch);

    tim::tcl_add_general(_d->_lil);
    tim::tcl_add_term(_d->_lil);
    tim::tcl_add_user(_d->_lil);
}

tim::tcl::~tcl()
{
    lil_free(_d->_lil);
}

const tim::uuid &tim::tcl::user_id() const
{
    return _d->_user_id;
}

bool tim::tcl::evaluating() const
{
    return _d->_evaluating;
}

bool tim::tcl::eval(const std::string &program, std::string *res)
{
    assert(!_d->_evaluating && "Recursive evaluating is not allowed.");

    TIM_TRACE(Debug, "Evaluating '%s'", program.c_str());

    if (program.empty())
    {
        if (res)
            res->clear();
        return true;
    }

    _d->_evaluating = true;

    void *old_data = lil_get_data(_d->_lil);
    lil_set_data(_d->_lil, this);

    _d->_error_msg.clear();
    _d->_error_pos = 0;

    lil_value_t rv = lil_parse(_d->_lil, program.c_str(), program.size(), 1);
    const char *err_msg = nullptr;
    const bool ok = !lil_error(_d->_lil, &err_msg, &_d->_error_pos);
    _d->_error_pos = utf8nlen((const utf8_int8_t *)program.c_str(), _d->_error_pos);

    // FIXME! Dirty hack.
    if (err_msg
            && *err_msg)
    {
        _d->_error_msg = err_msg;
        tim::replace(_d->_error_msg,
                     "Too many recursive calls",
                     TIM_TR("Too many recursive calls."_en,
                            "Слишком много рекурсивных вызовов."_ru));
        tim::replace(_d->_error_msg,
                     "catcher limit reached while trying to call unknown function",
                     TIM_TR("Catcher limit reached while trying to call unknown command"_en,
                            "Достигнут предел ловушек при попытке вызова неизвестной команды"_ru));
        tim::replace(_d->_error_msg,
                     "unknown function",
                     TIM_TR("Unknown command"_en,
                            "Неизвестная команда"_ru));
        tim::replace(_d->_error_msg,
                     "division by zero in expression",
                     TIM_TR("Division by zero in expression"_en,
                            "Деление на ноль в выражении"_ru));
        tim::replace(_d->_error_msg,
                     "mixing invalid types in expression",
                     TIM_TR("Mixing invalid types in expression"_en,
                            "Смешение недопустимых типов в выражении"_ru));
        tim::replace(_d->_error_msg,
                     "expression syntax error",
                     TIM_TR("Expression syntax error"_en,
                            "Синтаксическая ошибка в выражении"_ru));
        _d->_error_msg += '.';
    }

    if (ok
            && res)
        *res = lil_to_string(rv);

    lil_free_value(rv);
    lil_set_data(_d->_lil, old_data);

    _d->_evaluating = false;

    return ok;
}

void tim::tcl::break_eval()
{
    lil_break_run(_d->_lil, true);
}

const std::string &tim::tcl::prompt() const
{
    return _d->_prompt;
}

const std::string &tim::tcl::error_msg() const
{
    return _d->_error_msg;
}

std::size_t tim::tcl::error_pos() const
{
    return _d->_error_pos;
}

std::vector<std::string> tim::tcl::complete(const std::string &prefix) const
{
    if (prefix.empty())
        return {};

    std::vector<std::string> res;

    lil_value_t r = lil_names(_d->_lil, prefix.c_str());
    lil_list_t names = lil_subst_to_list(_d->_lil, r);
    lil_free_value(r);
    for (std::size_t i = 0; i < lil_list_size(names); ++i)
        res.emplace_back(lil_to_string(lil_list_get(names, i)));
    lil_free_list(names);
    return res;
}

std::unordered_set<std::string> tim::tcl::keywords() const
{
    return {};
}

std::unordered_set<std::string> tim::tcl::functions() const
{
    std::unordered_set<std::string> funcs;

    lil_value_t r = lil_names(_d->_lil, nullptr);
    lil_list_t names = lil_subst_to_list(_d->_lil, r);
    lil_free_value(r);

    for (std::size_t i = 0; i < lil_list_size(names); ++i)
        funcs.emplace(lil_to_string(lil_list_get(names, i)));

    lil_free_list(names);

    return funcs;
}

// Private

void tim::p::tcl::write(lil_t lil, const char *msg)
{
    assert(lil);

    if (!msg
            || !*msg)
        return;

    tim::tcl *self = (tim::tcl *)lil_get_data(lil);
    assert(self);

    self->terminal()->protocol()->write_str(msg);
}

void tim::p::tcl::dispatch(lil_t lil)
{
    (void) lil;

    tim::app()->dispatch();
}

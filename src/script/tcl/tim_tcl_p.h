#pragma once

#include "tim_uuid.h"

#include <cassert>
#include <string>


typedef struct _lil_t *lil_t;

namespace tim
{

class tcl;

namespace p
{

struct tcl
{
    explicit tcl(tim::tcl *q)
        : _q(q)
    {
        assert(_q);
    }

    static void write(lil_t lil, const char *msg);
    static void dispatch(lil_t lil);

    tim::tcl *const _q;

    lil_t _lil = nullptr;
    tim::uuid _user_id;
    bool _evaluating = false;
    std::string _prompt = "► ";
    std::string _error_msg;
    std::size_t _error_pos = 0;
};

}

}

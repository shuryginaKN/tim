#pragma once

#include "mongoose.h"

#include <memory>

#ifdef TIM_OS_LINUX
#   include <signal.h>
#endif

namespace tim
{

class inetd;
class mqtt_client;
class post_service;
class user_service;
class sqlite_db;

namespace p
{

struct application
{
    static tim::application *&instance()
    {
        static tim::application *app = nullptr;
        return app;
    };

    static std::string &name()
    {
        static std::string _name;
        return _name;
    }

    static std::string &org_name()
    {
        static std::string _name;
        return _name;
    }

#ifdef TIM_OS_LINUX

    static void signal_handler(int sig_num);

    struct sigaction _old_sig_int;
    struct sigaction _old_sig_term;
#endif
    bool _quit = false;

    struct mg_mgr _mg;
    std::unique_ptr<tim::mqtt_client> _mqtt;
    std::unique_ptr<tim::sqlite_db> _db;
    std::unique_ptr<tim::inetd> _prompt_inetd;
    std::unique_ptr<tim::post_service> _post_service;
    std::unique_ptr<tim::user_service> _user_service;
};

}

}

#pragma once

#include "tim_service.h"


namespace tim
{

namespace p
{

struct user_service;

}

class user_service : public tim::service
{

public:

    user_service();
    ~user_service();

private:

    std::unique_ptr<tim::p::user_service> _d;
};

}

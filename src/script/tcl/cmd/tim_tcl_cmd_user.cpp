#include "tim_tcl_cmd_user.h"

#include "tim_application.h"
#include "tim_mqtt_client.h"
#include "tim_tcl_cmd.h"
#include "tim_translator.h"
#include "tim_tcl.h"

#include "lil.hpp"

#include <cassert>


// Static

static lil_value_t tim_tcl_cmd_setnick(lil_t lil,
                                       std::size_t argc,
                                       lil_value_t *argv)
{
    (void) argv;

    if (argc != 1)
    {
        lil_set_error(lil,
                      TIM_TR("Expected nick"_en,
                             "Ожидаем параметр nick"_ru));
        return nullptr;
    }

    const std::string nick = lil_to_string(argv[0]);
    const tim::tcl *tcl = (const tim::tcl *)lil_get_data(lil);
    assert(tcl);

    tim::app()->mqtt()->publish(std::filesystem::path("user/setnick")
                                    / tcl->user_id().to_string(), nick.c_str(), nick.size());

    return nullptr;
}

static lil_value_t tim_tcl_cmd_seticon(lil_t lil,
                                       std::size_t argc,
                                       lil_value_t *argv)
{
    (void) argv;

    if (argc != 1)
    {
        lil_set_error(lil,
                      TIM_TR("Expected icon"_en,
                             "Ожидаем параметр icon"_ru));
        return nullptr;
    }

    const std::string icon = lil_to_string(argv[0]);
    const tim::tcl *tcl = (const tim::tcl *)lil_get_data(lil);
    assert(tcl);

    tim::app()->mqtt()->publish(std::filesystem::path("user/seticon")
                                    / tcl->user_id().to_string(), icon.c_str(), icon.size());

    return nullptr;
}

// Public

void tim::tcl_add_user(lil_t lil)
{
    assert(lil);

    TIM_TCL_REGISTER(lil, setnick);
    TIM_TCL_REGISTER(lil, seticon);
}

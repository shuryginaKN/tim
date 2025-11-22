#include "tim_prompt_service.h"

#include "tim_prompt_service_p.h"

#include "tim_application.h"
#include "tim_mqtt_client.h"
#include "tim_prompt_shell.h"
#include "tim_string_tools.h"
#include "tim_tcl.h"
#include "tim_telnet_server.h"
#include "tim_trace.h"
#include "tim_vt.h"


// Public

tim::prompt_service::prompt_service(mg_connection *c)
    : tim::a_inetd_service("prompt", c)
    , _d(new tim::p::prompt_service(this))
{
    _d->_telnet.reset(new tim::telnet_server(this));
    _d->_terminal.reset(new tim::vt(_d->_telnet.get()));
    _d->_tcl.reset(new tim::tcl(_d->_terminal.get(), _d->_user.id));
    _d->_shell.reset(new tim::prompt_shell(_d->_terminal.get(), _d->_tcl.get()));

    _d->_topic = std::filesystem::path("post") / std::to_string(id());

    _d->_telnet->data_ready.connect(
        std::bind(&tim::p::prompt_service::on_data_ready, _d.get(),
                  std::placeholders::_1, std::placeholders::_2));

    _d->_shell->posted.connect(
        [&](const std::string &text)
        {
            if (tim::app()->mqtt()->is_connected())
                tim::app()->mqtt()->publish(_d->_topic, text.c_str(), text.size());
        });

    tim::app()->mqtt()->connected.connect(std::bind(&tim::p::prompt_service::subscribe, _d.get()));

    if (tim::app()->mqtt()->is_connected())
        _d->subscribe();
}

tim::prompt_service::~prompt_service() = default;


// Private

void tim::p::prompt_service::subscribe()
{
    tim::app()->mqtt()->publish("user/connect", _user.id);
    tim::app()->mqtt()->subscribe(_topic.parent_path() / "+",
                                  std::bind(&tim::p::prompt_service::on_post, this,
                                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void tim::p::prompt_service::on_data_ready(const char *data, std::size_t size)
{
    assert(data);

    if (size
            && !_shell->write(data, size))
        _q->close();
}

void tim::p::prompt_service::on_post(const std::filesystem::path &topic, const char *data, std::size_t size)
{
    if (topic != _topic)
    {
        _shell->cloud(_user.title(),
                      '\n' + std::string(data, size),
                      _shell->terminal()->color(
                        tim::to_int(topic.filename()) % (_shell->terminal()->color_count() - 1) + 1));
        _shell->new_line();
    }
}

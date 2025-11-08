#include "tim_mqtt_client.h"

#include "tim_mqtt_client_p.h"

#include "tim_application.h"
#include "tim_file_tools.h"
#include "tim_trace.h"
#include "tim_translator.h"

#include "mongoose.h"

#include <cassert>


static const int TIM_MQTT_QOS = 1;

// Public

tim::mqtt_client::mqtt_client(mg_mgr *mg, const std::filesystem::path &url,
                              const std::chrono::seconds ping_interval)
    : connected{}
    , disconnected{}
    , _d(new tim::p::mqtt_client(this))
{
    assert(mg);
    assert(!url.empty() && "MQTT broker URL must not be empty.");
    assert(ping_interval.count() && "ping_interval must not be zero.");

    _d->_mg = mg;
    _d->_url = url;
    _d->_timer = mg_timer_add(mg, ping_interval.count() * 1000,
                              MG_TIMER_REPEAT | MG_TIMER_RUN_NOW,
                              &tim::p::mqtt_client::ping, _d.get());
}

tim::mqtt_client::~mqtt_client() = default;

bool tim::mqtt_client::is_connected() const
{
    return _d->_connected;
}

void tim::mqtt_client::publish(const std::filesystem::path &topic,
                               const char *data, std::size_t size,
                               std::uint8_t qos,
                               bool retain)
{
    assert(!topic.empty() && "Topic must not be empty.");

    const mg_mqtt_opts pub_opts =
    {
        .topic = mg_str(topic.string().c_str()),
        .message = mg_str_n(data, size),
        .qos = qos,
        .retain = retain
    };

    mg_mqtt_pub(_d->_client, &pub_opts);

    TIM_TRACE(Debug, "Published to '%s': '%s'.",
              topic.string().c_str(), std::string(data, size).c_str());
}

void tim::mqtt_client::publish(const std::filesystem::path &topic,
                               const std::string &s,
                               std::uint8_t qos,
                               bool retain)
{
    publish(topic, s.c_str(), s.size(), qos, retain);
}

void tim::mqtt_client::subscribe(const std::filesystem::path &topic, message_handler mh, std::uint8_t qos)
{
    assert(!topic.empty() && "Topic must not be empty.");
    assert(mh);

    std::string s_topic = topic.string();
    for (char &c: s_topic)
        if (c == '+')
            c = '*';

    _d->_subscribers.emplace_back(tim::p::mqtt_client::subscribers::value_type{ s_topic, mh });

    const mg_mqtt_opts pub_opts =
    {
        .topic = mg_str(topic.string().c_str()),
        .qos = qos
    };

    mg_mqtt_sub(_d->_client, &pub_opts);

    TIM_TRACE(Debug, "Subscribed to '%s'.", topic.string().c_str());
}


// Private

void tim::p::mqtt_client::handle_events(mg_connection *c, int ev, void *ev_data)
{
    tim::p::mqtt_client *self = (tim::p::mqtt_client *)c->fn_data;
    assert(self);

    switch (ev)
    {
        case MG_EV_OPEN:
            break;

        case MG_EV_CONNECT:
        {
            TIM_TRACE(Debug,
                      "TCP connection to MQTT broker '%s' established.",
                      self->_url.c_str());
            if (c->is_tls)
            {
                const std::filesystem::path base_path = tim::standard_location(tim::filesystem_location::AppTlsData);
                mg_tls_opts opts =
                {
                    .ca = mg_unpacked((base_path / "ca-cert.pem").string().c_str()),
                    .cert = mg_unpacked((base_path / "cert.pem").string().c_str()),
                    .key = mg_unpacked((base_path / "key.pem").string().c_str())
                };
                mg_tls_init(c, &opts);
            }

#ifdef TIM_DEBUG
//            c->is_hexdumping = 1;
#endif

            break;
        }

        case MG_EV_MQTT_OPEN:
            TIM_TRACE(Debug,
                      "MQTT handshake with broker '%s' succeeded.",
                      self->_url.string().c_str());
            self->_connected = true;
            self->_q->connected();
            break;

        case MG_EV_MQTT_CMD:
        {
            mg_mqtt_message *msg = (mg_mqtt_message *)ev_data;
            switch (msg->cmd)
            {
                case MQTT_CMD_PINGREQ:
                    mg_mqtt_pong(c);
                    break;

                default:
                    break;
            }
            break;
        }

        case MG_EV_MQTT_MSG:
            if (!c->is_draining)
            {
                mg_mqtt_message *msg = (mg_mqtt_message *)ev_data;
                const std::string topic(msg->topic.buf, msg->topic.len);

                TIM_TRACE(Debug,
                          "MQTT message received at topic '%s': '%.*s'.",
                          topic.c_str(),
                          (int)msg->data.len, msg->data.buf);

                for (const subscribers::value_type &pair: self->_subscribers)
                    if (mg_match(mg_str(topic.c_str()), mg_str(pair.first.string().c_str()), nullptr))
                        pair.second(topic, msg->data.buf, msg->data.len);
            }
            break;

        case MG_EV_CLOSE:
        {
            TIM_TRACE(Debug,
                      "MQTT connection to broker '%s' closed.",
                      self->_url.string().c_str());
            self->_client = nullptr;
            self->_connected = false;
            self->_q->disconnected();
            break;
        }

        case MG_EV_ERROR:
            if (!c->is_draining)
            {
                TIM_TRACE(Error,
                          TIM_TR("MQTT network error: %s"_en,
                                 "Сетевая ошибка MQTT: %s"_ru),
                          (char *)ev_data);
                c->is_draining = 1;
            }
            self->_connected = false;
            self->_q->disconnected();
            break;
    }
}

void tim::p::mqtt_client::ping(void *data)
{
    tim::p::mqtt_client *self = (tim::p::mqtt_client *)data;
    assert(self);

    if (self->_client)
    {
        mg_mqtt_ping(self->_client);
        return;
    }

    const mg_mqtt_opts opts =
    {
        .client_id = mg_str(tim::application::name().c_str()),
        .topic = mg_str("client/status"),
        .message = mg_str("disconnected"),
        .qos = TIM_MQTT_QOS,
        .version = 5,
        .keepalive = 0, // Do not disconnect.
        .clean = true
    };

    if (!(self->_client = mg_mqtt_connect(self->_mg, self->_url.string().c_str(), &opts,
                                          &tim::p::mqtt_client::handle_events, self)))
        TIM_TRACE(Fatal,
                  TIM_TR("Failed to connect to MQTT broker at '%s'."_en,
                         "Ошибка при подключении к брокеру MQTT '%s'."_ru),
                  self->_url.string().c_str());
}

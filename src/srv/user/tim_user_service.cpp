#include "tim_user_service.h"

#include "tim_user_service_p.h"
#include "tim_uuid.h"

#include "tim_application.h"
#include "tim_mqtt_client.h"
#include "tim_sqlite_db.h"
#include "tim_sqlite_query.h"
#include "tim_trace.h"
#include "tim_translator.h"


// Public

tim::user_service::user_service()
    : tim::service("user")
    , _d(new tim::p::user_service())
{
    tim::app()->mqtt()->connected.connect(std::bind(&tim::p::user_service::subscribe, _d.get()));

    if (tim::app()->mqtt()->is_connected())
        _d->subscribe();
}

tim::user_service::~user_service() = default;


// Private

void tim::p::user_service::subscribe()
{

    // Две подписки
    tim::app()->mqtt()->subscribe("set_nick/+",
                                  std::bind(&tim::p::user_service::on_change, this,
                                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    tim::app()->mqtt()->subscribe("set_icon/+",
                                  std::bind(&tim::p::user_service::on_change, this,
                                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void tim::p::user_service::on_change(const std::filesystem::path &topic, const char *data, std::size_t size)
{
    const tim::uuid user_id = topic.filename().string();
    const std::string new_value(data, size);

    const std::string command = topic.parent_path().filename().string(); // "setNick" или "setIcon"


    if (command == "setNick")
    {
        tim::sqlite_query q(tim::app()->db(),
                            "UPDATE user SET nick = ? WHERE id = ?");
        if (!q.prepare())
            TIM_TRACE(Fatal,
                TIM_TR("Failed to prepare query '%s'."_en,
                       "Не могу подготовить запрос '%s' к базе данных."_ru),
                q.sql().c_str());

        q.bind(1, new_value);
        q.bind(2, user_id.to_string());
        q.bind(3, std::string(data, size));

         if (!q.exec())
            TIM_TRACE(Error,
                TIM_TR("Failed to update nick for user '%s'."_en,
                       "Ошибка при обновлении ника у пользователя '%s'."_ru),
                user_id.to_string().c_str());
    }
     else if (command == "setIcon")
    {
        tim::sqlite_query q(tim::app()->db(),
            "UPDATE user SET icon = ? WHERE id = ?");

        if (!q.prepare())
            TIM_TRACE(Fatal,
                TIM_TR("Failed to prepare query '%s'."_en,
                       "Не могу подготовить запрос '%s' к базе данных."_ru),
                q.sql().c_str());

        q.bind(1, new_value);
        q.bind(2, user_id.to_string());
        q.bind(3, std::string(data, size));

        if (!q.exec())
            TIM_TRACE(Error,
                TIM_TR("Failed to update icon for user '%s'."_en,
                       "Ошибка при обновлении иконки у пользователя '%s'."_ru),
                user_id.to_string().c_str());
    }

}

#include "tim_sqlite_query.h"

#include "tim_sqlite_query_p.h"

#include "tim_config.h"
#include "tim_sqlite_db.h"
#include "tim_string_tools.h"
#include "tim_trace.h"
#include "tim_translator.h"

#include <cassert>
#include <thread>


// Public

tim::sqlite_query::sqlite_query(const tim::sqlite_db *db, const std::string &sql)
    : _d(new tim::p::sqlite_query(db))
{
    assert(!sql.empty());

    _d->_sql = sql;
}

tim::sqlite_query::~sqlite_query()
{
    sqlite3_finalize(_d->_stmt);
}

const std::string &tim::sqlite_query::sql() const
{
    return _d->_sql;
}

bool tim::sqlite_query::prepared() const
{
    return _d->_prepared;
}

bool tim::sqlite_query::prepare()
{
    assert(!_d->_stmt && "The query is prepared already.");

    int res = SQLITE_OK;
    const char *err_msg;

    for (unsigned count = 0; count < tim::DB_BUSY_TRIES; ++count)
        switch ((res = sqlite3_prepare_v2(_d->_db->sqlite(),
                                          _d->_sql.c_str(),
                                          (int)_d->_sql.size() + 1,
                                          &_d->_stmt,
                                          &err_msg)))
        {
            case SQLITE_OK:
                _d->_prepared = true;
                return true;

            case SQLITE_BUSY:
                TIM_TRACE(Debug,
                         TIM_TR("Database '%s' is busy. Try #%u. Retrying in %ld microseconds."_en,
                               "База данных '%s' занята. Попытка №%u. Повторим попытку через %ld микросекунд."_ru),
                         _d->_db->path().string().c_str(),
                         count,
                         tim::DB_BUSY_TIMEOUT.count());
                std::this_thread::sleep_for(tim::DB_BUSY_TIMEOUT);
                break;

            default:
                goto failure;
        }

failure:

    return TIM_TRACE(Error,
                    TIM_TR("Failed to prepare SQL query '%s' to database '%s': %s %s"_en,
                          "Ошибка при подготовке SQL-запроса '%s' к базе данных '%s': %s %s"_ru),
                    _d->_sql.c_str(),
                    _d->_db->path().string().c_str(),
                    sqlite3_errstr(res),
                    err_msg);
}

bool tim::sqlite_query::bind(int index, bool value)
{
    return bind(index, (int)value);
}

bool tim::sqlite_query::bind(int index, int value)
{
    assert(_d->_stmt);

    const int res = sqlite3_bind_int(_d->_stmt, index, value);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to bind an int value at index %d for SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при привязке целочисленного значения к позиции %d для SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        index,
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

bool tim::sqlite_query::bind(int index, std::int64_t value)
{
    assert(_d->_stmt);

    const int res = sqlite3_bind_int64(_d->_stmt, index, value);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to bind a 64-bit integer value at index %d for SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при привязке 64-битного целочисленного значения к позиции %d для SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        index,
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));

    return true;
}

bool tim::sqlite_query::bind(int index, const double value)
{
    assert(_d->_stmt);

    const int res = sqlite3_bind_double(_d->_stmt, index, value);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to bind a double value at index %d for SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при привязке числового значения с плавающей запятой двойной точности к позиции %d для SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        index,
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));

    return true;
}

bool tim::sqlite_query::bind(int index, float value)
{
    return bind(index, (double)value);
}

bool tim::sqlite_query::bind(int index, const char *value)
{
    assert(value);
    assert(_d->_stmt);

    const int res = sqlite3_bind_text(_d->_stmt, index, value, (int)std::strlen(value), SQLITE_TRANSIENT);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to bind a string value at index %d for SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при привязке строкового значения к позиции %d для SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        index,
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));

    return true;
}

bool tim::sqlite_query::bind(int index, const std::string &value)
{
    assert(_d->_stmt);

    const int res = sqlite3_bind_text(_d->_stmt, index, value.c_str(), (int)value.size(), SQLITE_TRANSIENT);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to bind a string value at index %d for SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при привязке строкового значения к позиции %d для SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        index,
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));

    return true;
}

bool tim::sqlite_query::bind(int index, const nlohmann::json &value)
{
    switch (value.type())
    {
        case nlohmann::detail::value_t::number_integer:
            return bind(index, value.get<std::int64_t>());

        case nlohmann::detail::value_t::number_unsigned:
            return bind(index, (std::int64_t)value.get<std::uint64_t>());

        case nlohmann::detail::value_t::number_float:
            return bind(index, value.get<float>());

        case nlohmann::detail::value_t::boolean:
            return bind(index, value.get<bool>()
                                    ? "true"
                                    : "false");

        default:
            return bind(index, value.dump());
    }

    return false;
}

bool tim::sqlite_query::bind(const std::string &key, bool value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, int value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, std::int64_t value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, double value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, float value)
{
    return bind(key, (double)value);
}

bool tim::sqlite_query::bind(const std::string &key, const char *value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, const std::string &value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::bind(const std::string &key, const nlohmann::json &value)
{
    assert(_d->_stmt);

    return bind(sqlite3_bind_parameter_index(_d->_stmt, key.c_str()), value);
}

bool tim::sqlite_query::clear_bindings()
{
    assert(_d->_stmt);

    const int res = sqlite3_clear_bindings(_d->_stmt);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to clear bindings for query '%s' to database '%s': %s"_en,
                              "Ошибка при очистке привязок значений к позициям в запросе '%s' к базе данных '%s': %s"_ru),
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));

    return true;
}

bool tim::sqlite_query::exec()
{
    return next();
}

bool tim::sqlite_query::next(bool *done)
{
    assert(_d->_stmt);

    int res = SQLITE_OK;
    for (unsigned count = 0; count < tim::DB_BUSY_TRIES; ++count)
        switch ((res = sqlite3_step(_d->_stmt)))
        {
            case SQLITE_ROW:
                if (done)
                    *done = false;
                return true;

            case SQLITE_DONE:
                if (done)
                    *done = true;
                return true;

            case SQLITE_MISUSE:
                return TIM_TRACE(Error,
                                TIM_TR("Misuse of SQL query '%s' to database '%s': %s"_en,
                                      "Неверное использование SQL-запроса '%s' к базе данных '%s': %s"_ru),
                                sqlite3_expanded_sql(_d->_stmt),
                                _d->_db->path().string().c_str(),
                                sqlite3_errstr(res));

            case SQLITE_BUSY:
                TIM_TRACE(Debug,
                         TIM_TR("Database '%s' is busy. Try #%u. Retrying in %ld microseconds."_en,
                               "База данных '%s' занята. Попытка №%u. Повторяем попытку через %ld микросекунд."_ru),
                         _d->_db->path().string().c_str(),
                         count,
                         tim::DB_BUSY_TIMEOUT.count());
                std::this_thread::sleep_for(tim::DB_BUSY_TIMEOUT);
                break;

            default:
                goto failure;
        }

failure:

    return TIM_TRACE(Error,
                    TIM_TR("Failed to perform SQL query '%s' to database '%s': %s"_en,
                          "Ошибка при выполнении SQL-запроса '%s' к базе данных '%s': %s"_ru),
                    sqlite3_expanded_sql(_d->_stmt),
                    _d->_db->path().string().c_str(),
                    sqlite3_errstr(res));
}

std::size_t tim::sqlite_query::column_count() const
{
    assert(_d->_stmt);

    return std::max(0, sqlite3_column_count(_d->_stmt));
}

std::size_t tim::sqlite_query::data_column_count() const
{
    assert(_d->_stmt);

    return std::max(0, sqlite3_data_count(_d->_stmt));
}

int tim::sqlite_query::type(std::size_t index) const
{
    assert(_d->_stmt);

    return sqlite3_column_type(_d->_stmt, index);
}

int tim::sqlite_query::to_int(int index) const
{
    assert(_d->_stmt);

    return sqlite3_column_int(_d->_stmt, index);
}

int64_t tim::sqlite_query::to_int64(int index) const
{
    assert(_d->_stmt);

    return sqlite3_column_int64(_d->_stmt, index);
}

double tim::sqlite_query::to_double(int index) const
{
    assert(_d->_stmt);

    return sqlite3_column_double(_d->_stmt, index);
}

std::string tim::sqlite_query::to_string(int index) const
{
    assert(_d->_stmt);

    const char *s = (const char *)sqlite3_column_text(_d->_stmt, index);
    return s
                ? s
                : std::string{};
}

nlohmann::json tim::sqlite_query::to_json(int index, bool *ok) const
{
    nlohmann::json j;
    const bool res = tim::from_string(to_string(index), j);
    if (ok)
        *ok = res;

    return res
                ? j
                : nlohmann::json{};
}

bool tim::sqlite_query::reset()
{
    assert(_d->_stmt);

    const int res = sqlite3_reset(_d->_stmt);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to reset SQL query '%s' to database '%s': %s"_en,
                              "Ошибка при сбросе SQL-запроса '%s' к базе данных '%s': %s"_ru),
                        _d->_sql.c_str(),
                        _d->_db->path().string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

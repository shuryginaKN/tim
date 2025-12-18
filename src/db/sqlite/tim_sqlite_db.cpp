#include "tim_sqlite_db.h"

#include "tim_sqlite_db_p.h"

#include "tim_config.h"
#include "tim_file_tools.h"
#include "tim_sqlite_query.h"
#include "tim_string_tools.h"
#include "tim_trace.h"
#include "tim_translator.h"

#include "sqlite3.h"

#include <cassert>
#include <fstream>
#include <thread>

#ifdef TIM_OS_LINUX
#   include <endian.h>
#else
#include "tim_endian.h"
#endif


static const char *const PRAGMAS =
R"(PRAGMA encoding = 'UTF-8';
PRAGMA foreign_keys = ON;
PRAGMA case_sensitive_like = OFF;)";

static const int SLEEP = 250; // In milliseconds.


// Public

tim::sqlite_db::sqlite_db()
    : _d(new tim::p::sqlite_db())
{
}

tim::sqlite_db::~sqlite_db()
{
    close();
}

bool tim::sqlite_db::open(const std::filesystem::path &path)
{
    assert(!_d->_db && "The database is already open.");

    _d->_path = tim::complete_path(path);
    assert(!_d->_path.empty());

    TIM_TRACE(Debug,
             TIM_TR("Database path: '%s'."_en,
                   "Путь к базе данных: '%s'."_ru),
             _d->_path.string().c_str());

    std::filesystem::path parent_path = _d->_path.parent_path();
    std::error_code ec;
    if (!std::filesystem::exists(parent_path, ec)
            && (ec
                    || !std::filesystem::create_directories(parent_path, ec)))
        return TIM_TRACE(Error,
                        TIM_TR("Failed to create folder '%s': %s"_en,
                              "Ошибка при создании папки '%s': %s"_ru),
                        parent_path.string().c_str(),
                        ec.message().c_str());

    if (!(_d->_db = tim::p::sqlite_db::open_db(_d->_path)))
        return false;

    if (!exec(PRAGMAS))
        return false;

/*
    // \bug Enable trace only when it is explicitly enabled.
    if (const int res = sqlite3_trace_v2(_d->_db.get(),
                                         SQLITE_TRACE_STMT
                                                | SQLITE_TRACE_PROFILE
                                                | SQLITE_TRACE_ROW
                                                | SQLITE_TRACE_CLOSE,
                                         &tim::p::sqlite_db::trace, this) != SQLITE_OK)
        return TIM_TRACE(Error,
                         TIM_TR("Failed to enable trace for database '%s': %s"_en,
                                "Ошибка при включении отладки для базы данных '%s': %s"_ru),
                         _d->_path.string().c_str(),
                         sqlite3_errstr(res));
*/
//    sqlite3_progress_handler(_d->_db.get(), 1, &tim::p::sqlite_db::progress, this);

    return true;
}

bool tim::sqlite_db::is_open() const
{
    return (bool)_d->_db;
}

bool tim::sqlite_db::flush()
{
    assert(_d->_db);

    const int res = sqlite3_db_cacheflush(_d->_db.get());
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to flush database cache '%s': %s"_en,
                              "Ошибка при сохранении кэша базы данных '%s': %s"_ru),
                        _d->_path.string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

void tim::sqlite_db::close()
{
    _d->_db.reset();
}

const std::filesystem::path &tim::sqlite_db::path() const
{
    return _d->_path;
}

#ifdef TIM_SQLITE_ENCRYPTION_ENABLED

bool tim::sqlite_db::set_key(const std::string &key)
{
    assert(_d->_db);

    const int res = sqlite3_key_v2(_d->_db.get(), nullptr, key.c_str(), 1);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to set encryption key for database '%s': %s"_en,
                              "Ошибка при задании ключа шифрования для базы данных '%s': %s"_ru),
                        _d->_path.string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

bool tim::sqlite_db::rekey(const std::string &key)
{
    assert(_d->_db);

    const int res = sqlite3_rekey_v2(_d->_db.get(), nullptr, key.c_str(), 1);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to re-key database '%s': %s"_en,
                              "Ошибка при изменении ключа шифрования базы данных '%s': %s"_ru),
                        _d->_path.string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

bool tim::sqlite_db::clear_key()
{
    assert(_d->_db);

    const int res = sqlite3_rekey_v2(_d->_db.get(), nullptr, nullptr, 0);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to remove encryption key for database '%s': %s"_en,
                              "Ошибка при удалении ключа шифрования для базы данных '%s': %s"_ru),
                        _d->_path.string().c_str(),
                        sqlite3_errstr(res));
    return true;
}

#endif

bool tim::sqlite_db::get_version(const std::filesystem::path &path, std::uint32_t &version)
{
    static const char SIGNATURE[] = "SQLite format 3\000";

    std::ifstream is(path, std::ios::binary);

    char signature[sizeof(SIGNATURE) - 1];
    if (!is.read(signature, sizeof(signature))
            || std::strncmp(signature, SIGNATURE, sizeof(signature))
            || !is.seekg(60, std::ios::beg)
            || !is.read((char *)&version, sizeof(version)))
        return TIM_TRACE(Error,
                        TIM_TR("Database file '%s' is corrupted."_en,
                              "Файл базы данных '%s' поврежден."_ru),
                        path.string().c_str());

#ifdef TIM_OS_LINUX
    version = be32toh(version);
#else
    version = tim::to_le(version);
#endif

    is.close();

    return true;
}

bool tim::sqlite_db::get_version(std::uint32_t &version) const
{
    tim::sqlite_query q(const_cast<tim::sqlite_db *>(this), "PRAGMA user_version");
    if (!q.prepare()
            || !q.next())
        return false;
    version = q.data_column_count() == 0
                    ? 0
                    : q.to_int(0);
    return true;
}

bool tim::sqlite_db::set_version(std::uint32_t version)
{
    return exec("PRAGMA user_version = " + std::to_string(version));
}

bool tim::sqlite_db::recreate()
{
    TIM_TRACE(Debug,
             TIM_TR("Recreating the database '%s' ..."_en,
                   "Воссоздаём базу данных '%s' ..."_ru),
             _d->_path.string().c_str());

    close();

    std::error_code ec;
    if (!_d->_path.empty()
            && std::filesystem::exists(_d->_path, ec)
            && (ec
                    || !std::filesystem::remove(_d->_path, ec)))
        return TIM_TRACE(Error,
                        TIM_TR("Failed to remove database file '%s': %s"_en,
                              "Ошибка удаления файла базы данных  '%s': %s"_ru),
                        _d->_path.string().c_str(),
                        ec.message().c_str());

    return open(_d->_path);
}

bool tim::sqlite_db::exec(const std::string &sql)
{
    assert(_d->_db);
    assert(!sql.empty());
/*
    TIM_TRACE(Debug,
             TIM_TR("Executing query '%s' ..."_en,
                   "Выполняем запрос '%s' ..."_ru),
             sql.c_str());
*/
    int res = SQLITE_OK;
    char *err_msg = nullptr;
    for (unsigned count = 0; count < tim::DB_BUSY_TRIES; ++count)
        switch ((res = sqlite3_exec(_d->_db.get(), sql.c_str(), nullptr, nullptr, &err_msg)))
        {
            case SQLITE_OK:
                return true;

            case SQLITE_BUSY:
                TIM_TRACE(Debug,
                         TIM_TR("Database '%s' is busy. Try #%u. Retrying in %ld microseconds."_en,
                               "База данных '%s' занята. Попытка №%u. Повторяем попытку через %ld микросекунд."_ru),
                         _d->_path.string().c_str(),
                         count,
                         tim::DB_BUSY_TIMEOUT.count());
                std::this_thread::sleep_for(tim::DB_BUSY_TIMEOUT);
                break;

            default:
                goto failure;
        }

failure:

    return TIM_TRACE(Error,
                    TIM_TR("Failed to perform SQL query '%s' to the database '%s': %s"_en,
                          "Ошибка при выполнении SQL-запроса '%s' к базе данных '%s': %s"_ru),
                    sql.c_str(),
                    _d->_path.string().c_str(),
                    sqlite3_errstr(res));
}

bool tim::sqlite_db::exec_file(const std::filesystem::path &path)
{
    std::string sql;
    if (!tim::read_file(path, sql))
        return false;

    return exec(sql);
}

bool tim::sqlite_db::transaction(const std::string &sql)
{
    assert(!sql.empty());

    if (!begin())
        return false;

    return exec(sql)
                ? commit()
                : rollback();
}

bool tim::sqlite_db::begin()
{
    if (!is_transaction_active())
    {
        if (!exec("BEGIN"))
            return false;
        _d->_transaction_count = 1;
    }
    else
        ++_d->_transaction_count;

    return true;
}

bool tim::sqlite_db::begin(const std::string &save_point)
{
    assert(!save_point.empty());

    return exec("SAVEPOINT " + save_point);
}

bool tim::sqlite_db::is_transaction_active() const
{
    return !sqlite3_get_autocommit(_d->_db.get());
}

bool tim::sqlite_db::commit()
{
    if (_d->_transaction_count < 1
            || !is_transaction_active())
        return TIM_TRACE(Error,
                        TIM_TR("Unexpected commit to the database '%s'."_en,
                              "Неожиданное подтверждение транзакции для базы данных '%s'."_ru),
                        _d->_path.string().c_str());

    if (_d->_transaction_count == 1
             && !exec("COMMIT"))
        return false;

    --_d->_transaction_count;

    return true;
}

bool tim::sqlite_db::commit(const std::string &save_point)
{
    assert(!save_point.empty());

    return exec("RELEASE SAVEPOINT " + save_point);
}

bool tim::sqlite_db::rollback()
{
    assert(_d->_db);

    if (!sqlite3_get_autocommit(_d->_db.get())
            && !exec("ROLLBACK"))
        return false;

    _d->_transaction_count = 0;

    return true;
}

bool tim::sqlite_db::rollback(const std::string &save_point)
{
    assert(!save_point.empty());

    return exec("ROLLBACK TRANSACTION TO SAVEPOINT " + save_point);
}

bool tim::sqlite_db::vacuum()
{
    return exec("VACUUM");
}

int tim::sqlite_db::change_count() const
{
    assert(_d->_db);

    return sqlite3_changes(_d->_db.get());
}

sqlite3 *tim::sqlite_db::sqlite() const
{
    return _d->_db.get();
}

bool tim::sqlite_db::backup(const std::filesystem::path &path,
                           tim::sqlite_db::backup_progress_fn fn) const
{
    assert(_d->_db);

    static const int PAGES_PER_STEP = 5;

    sqlite3 *backup_db = nullptr;

    std::filesystem::path complete_path = tim::complete_path(path);
    std::filesystem::path parent_path = complete_path.parent_path();

    std::error_code ec;
    if (!std::filesystem::exists(parent_path, ec)
            && (ec
                    || !std::filesystem::create_directories(parent_path, ec)))
        return TIM_TRACE(Error,
                        TIM_TR("Failed to create folder '%s': %s"_en,
                              "Ошибка при создании папки '%s': %s"_ru),
                        parent_path.string().c_str(),
                        ec.message().c_str());

    const int res = sqlite3_open_v2(complete_path.string().c_str(), &backup_db,
                                    SQLITE_OPEN_READWRITE
                                        | SQLITE_OPEN_CREATE
                                        | SQLITE_OPEN_SHAREDCACHE,
                                    nullptr);
    if (res != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to open database '%s': %s"_en,
                              "Ошибка при открытии базы данных '%s': %s"_ru),
                        complete_path.string().c_str(),
                        sqlite3_errstr(res));

    if (!tim::p::sqlite_db::replicate(backup_db, _d->_db.get(), PAGES_PER_STEP, fn))
        return false;

    sqlite3_close(backup_db);

    return true;
}


// Private

tim::p::sqlite_db::db_ptr tim::p::sqlite_db::open_db(const std::filesystem::path &path)
{
    assert(!path.empty());

    TIM_TRACE(Debug,
             TIM_TR("Opening database '%s' ..."_en,
                   "Открываем базу данных '%s' ..."_ru),
             path.string().c_str());

    sqlite3 *db = nullptr;
    int res = sqlite3_open_v2(path.string().c_str(), &db,
                              SQLITE_OPEN_READWRITE
                                    | SQLITE_OPEN_CREATE
                                    | SQLITE_OPEN_SHAREDCACHE,
                              nullptr);
    if (res != SQLITE_OK)
    {
        TIM_TRACE(Error,
                 TIM_TR("Failed to open database '%s': %s"_en,
                       "Ошибка при открытии базы данных '%s': %s"_ru),
                 path.string().c_str(),
                 sqlite3_errstr(res));
        return {};
    }

    tim::p::sqlite_db::db_ptr dbp(db, &tim::p::sqlite_db::close_db);

/*
    if ((res = sqlite3_busy_timeout(db, tim::DB_BUSY_TIMEOUT.count() / 1000)) != SQLITE_OK)
    {
        TIM_TRACE(Error,
                 TIM_TR("Failed to set timeout for database '%s': %s"_en,
                       "Ошибка при задании таймаута для базы данных '%s': %s"_ru),
                 path.string().c_str(),
                 sqlite3_errstr(res));
        return {};
    }
*/

    if ((res = sqlite3_extended_result_codes(db, 1)) != SQLITE_OK)
    {
        TIM_TRACE(Error,
                 TIM_TR("Failed to enable extended result codes for database '%s': %s"_en,
                       "Ошибка при включении расширенных кодов результатов для базы данных '%s': %s"_ru),
                 path.string().c_str(),
                 sqlite3_errstr(res));
        return {};
    }

    if ((res = sqlite3_enable_load_extension(db, 1)) != SQLITE_OK)
    {
        TIM_TRACE(Error,
                 TIM_TR("Failed to enable extension loading for database '%s': %s"_en,
                       "Ошибка при включении загрузки расширений для базы данных '%s': %s"_ru),
                 path.string().c_str(),
                 sqlite3_errstr(res));
        return {};
    }

    return dbp;
}

bool tim::p::sqlite_db::close_db(sqlite3 *db)
{
    if (db)
    {
        const int res = sqlite3_close_v2(db);
        if (res != SQLITE_OK)
            return TIM_TRACE(Error,
                            TIM_TR("Failed to close database '%s': %s"_en,
                                  "Ошибка при закрытии базы данных '%s': %s"_ru),
                            sqlite3_db_filename(db, "main"),
                            sqlite3_errstr(res));
    }

    return true;
}

bool tim::p::sqlite_db::replicate(sqlite3 *dst, sqlite3 *src,
                                 const int pages_per_step,
                                 tim::sqlite_db::backup_progress_fn fn)
{
    assert(dst);
    assert(src);

    sqlite3_backup *backup;
    if (!(backup = sqlite3_backup_init(dst, "main", src, "main")))
        return TIM_TRACE(Error,
                        TIM_TR("Failed to initialize backup for database '%s': %s"_en,
                              "Ошибка при инициализации резервного копирования базы данных '%s': %s"_ru),
                        sqlite3_db_filename(dst, "main"),
                        sqlite3_errmsg(dst));

    int res = SQLITE_OK;
    do
    {
        res = sqlite3_backup_step(backup, pages_per_step);
        if (fn)
           fn(sqlite3_backup_remaining(backup),
              sqlite3_backup_pagecount(backup));
        if (res == SQLITE_BUSY
                || res == SQLITE_LOCKED)
            sqlite3_sleep(SLEEP);
    }
    while (res == SQLITE_OK
                || res == SQLITE_BUSY
                || res == SQLITE_LOCKED);

    if ((res = sqlite3_backup_finish(backup)) != SQLITE_OK)
        return TIM_TRACE(Error,
                        TIM_TR("Failed to finish backup for database '%s': %s"_en,
                              "Ошибка при завершении резервного копирования базы данных '%s': %s"_ru),
                        sqlite3_db_filename(dst, "main"),
                        sqlite3_errstr(res));

    return true;
}

int tim::p::sqlite_db::trace(unsigned event, void *self, void *p, void *x)
{
    (void) self;

    switch (event)
    {
        case SQLITE_TRACE_STMT:
        {
            const char *sql = (const char *)x;
            if (!sql
                    || sql[0] != '-'
                    || sql[1] != '-')
                sql = sqlite3_expanded_sql((sqlite3_stmt *)p);
            if (sql)
                TIM_TRACE(Debug,
                         TIM_TR("Database statement trace: '%s'."_en,
                               "Оладка запроса к базе данных: '%s'."_ru),
                         sql);
            break;
        }

        case SQLITE_TRACE_PROFILE:
        {
            // The current implementation of SQLite3 is only capable of millisecond
            // resolution so the six least significant digits in the time are meaningless.
            const char *sql = sqlite3_expanded_sql((sqlite3_stmt *)p);
            if (sql)
                TIM_TRACE(Debug,
                         TIM_TR("Database statement starting profiling: '%s' => %lldms"_en,
                               "Профилирование запуска запроса к базе данных: '%s' => %lldмс"_ru),
                         sql,
                         (*((std::int64_t *)x) / 1000000));
            break;
        }

        case SQLITE_TRACE_ROW:
        {
            const char *sql = sqlite3_expanded_sql((sqlite3_stmt *)p);
            if (sql)
                TIM_TRACE(Debug,
                         TIM_TR("Database row trace: '%s'"_en,
                               "Отладка получения строки таблицы базы данных: '%s'"_ru),
                         sql);
            break;
        }

        case SQLITE_TRACE_CLOSE:
        {
            const char *name = sqlite3_db_filename((sqlite3 *)p, "main");
            TIM_TRACE(Debug,
                     TIM_TR("Database close trace: '%s'."_en,
                           "Отладка закрытия базы данных: '%s'."_ru),
                     (name
                          ? name
                          : TIM_TR("N/A"_en, "отсутствует"_ru)));
            break;
        }
    }

    return 0;
}

int tim::p::sqlite_db::progress(void *self)
{
    (void) self;

    TIM_TRACE(Debug, "%s",
             TIM_TR("Database operation in progress ..."_en,
                   "Выполняется операция над базой данных ..."_ru));
    return 0;
}

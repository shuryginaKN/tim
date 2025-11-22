#pragma once

#include <filesystem>


namespace tim::p
{

struct user_service
{
    void subscribe();
    void connect(const std::filesystem::path &topic, const char *data, std::size_t size);
    void setnick(const std::filesystem::path &topic, const char *data, std::size_t size);
    void seticon(const std::filesystem::path &topic, const char *data, std::size_t size);
};
}

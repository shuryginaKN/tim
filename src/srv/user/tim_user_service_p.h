#pragma once

#include <filesystem>


namespace tim::p
{

struct user_service
{
    void subscribe();
    void on_change(const std::filesystem::path &topic, const char *data, std::size_t size);
};
}

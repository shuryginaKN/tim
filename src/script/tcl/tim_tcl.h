#pragma once

#include "tim_a_script_engine.h"
#include "tim_uuid.h"

#include <cstddef>
#include <memory>
#include <string>


namespace tim
{

namespace p
{

struct tcl;

}

class a_terminal;

class tcl : public tim::a_script_engine
{

public:

    tcl(tim::a_terminal *term, const tim::uuid &user_id);
    virtual ~tcl();

    const tim::uuid &user_id() const;

    bool evaluating() const override;
    bool eval(const std::string &program, std::string *res = nullptr) override;
    void break_eval() override;

    const std::string &prompt() const override;
    const std::string &error_msg() const override;
    std::size_t error_pos() const override;

    std::vector<std::string> complete(const std::string &prefix) const override;

    std::unordered_set<std::string> keywords() const override;
    std::unordered_set<std::string> functions() const override;

private:

    std::unique_ptr<tim::p::tcl> _d;
};

}

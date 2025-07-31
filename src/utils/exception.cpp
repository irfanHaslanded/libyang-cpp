/*
 * Copyright (C) 2021 CESNET, https://photonics.cesnet.cz/
 *
 * Written by Václav Kubernát <kubernat@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
#include <libyang-cpp/Utils.hpp>
#include <libyang/libyang.h>
#include <sstream>
#include "exception.hpp"

namespace libyang {
ErrorWithCode::ErrorWithCode(const std::string& what, unsigned int errCode)
    : Error(what)
    , m_errCode(static_cast<ErrorCode>(errCode))
{
}

ParsedInfoUnavailable::ParsedInfoUnavailable()
    : Error("Context not created with libyang::ContextOptions::SetPrivParsed")
{
}

ErrorCode ErrorWithCode::code() const
{
    return m_errCode;
}

void throwIfError(int code, std::string msg)
{
    if (code != LY_SUCCESS) {
        throwError(code, msg);
    }
}

void throwIfError2(ly_ctx *ctx, int code, std::string msg)
{
    if (code != LY_SUCCESS) {
        throwError2(ctx, code, msg);
    }
}

[[noreturn]] void throwError(int code, std::string msg)
{
    if (code == LY_SUCCESS) {
        throw std::logic_error("Threw with LY_SUCCESS");
    }

    std::ostringstream oss;
    oss << msg << ": " << static_cast<ErrorCode>(code);
    throw ErrorWithCode(oss.str(), code);
}

[[noreturn]] void throwError2(ly_ctx *ctx, int code, std::string msg)
{
    if (code == LY_SUCCESS) {
        throw std::logic_error("Threw with LY_SUCCESS");
    }

    std::ostringstream oss;
    auto err = ctx ? ly_err_last(ctx) : nullptr;
    if (err && err->msg) {
        oss << err->msg;
        auto path = err->data_path ? err->data_path : err->schema_path;
        if (path) {
            auto node_type = err->data_path ? "Data" : "Schema";
            if (err->line) {
                oss << " (" << node_type << " location \"" << path << "\", line number " << err->line << ".)";
            } else {
                oss << " (" << node_type << " location \"" << path << "\".)";
            }
        } else if (err->line) {
            oss << " (Line number " << err->line << ".)";
        }
    } else {
        oss << msg << ": " << static_cast<ErrorCode>(code);
    }
    throw ErrorWithCode(oss.str(), code);
}
}

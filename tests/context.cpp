/*
 * Copyright (C) 2021 CESNET, https://photonics.cesnet.cz/
 *
 * Written by Václav Kubernát <kubernat@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <doctest/doctest.h>
#include <libyang-cpp/Context.hpp>
#include <libyang-cpp/Utils.hpp>
#include "example_schema.hpp"
#include "pretty_printers.hpp"
#include "test_vars.hpp"

using namespace std::literals;

const auto valid_yin_model = R"(
<?xml version="1.0" encoding="UTF-8"?>
<module name="test"
        xmlns="urn:ietf:params:xml:ns:yang:yin:1"
        xmlns:t="http://example.com">
  <namespace uri="http://example.com"/>
  <prefix value="t"/>
</module>
)"s;

const auto valid_yang_model = R"(
    module test {
      namespace "http://example.com";
      prefix "t";

      leaf someLeaf {
        type string;
      }
    }
)"s;

const auto imported_module = R"(
    module importedModule {
      namespace "http://example.com/importedModule";
      prefix "ab";

      leaf myLeaf {
        type string;
      }
    }
)"s;

const auto model_with_import = R"(
    module withImport {
      namespace "http://example.com/withImport";
      prefix "t";

      import importedModule {
        prefix "im";
      }

      leaf someLeaf {
        type string;
      }
    }
)"s;

TEST_CASE("context")
{
    std::optional<libyang::Context> ctx{std::in_place, std::nullopt, libyang::ContextOptions::NoYangLibrary | libyang::ContextOptions::DisableSearchCwd};

    DOCTEST_SUBCASE("parseModule")
    {
        std::string mod;
        libyang::SchemaFormat format;
        DOCTEST_SUBCASE("valid")
        {
            DOCTEST_SUBCASE("yang")
            {
                mod = valid_yang_model;
                format = libyang::SchemaFormat::YANG;
            }

            DOCTEST_SUBCASE("yin")
            {
                mod = valid_yin_model;
                format = libyang::SchemaFormat::YIN;
            }

            REQUIRE(ctx->parseModule(mod, format).name() == "test");

            REQUIRE(ctx->getModule("test", std::nullopt)->name() == "test");
        }

        DOCTEST_SUBCASE("invalid")
        {
            format = libyang::SchemaFormat::YANG;
            mod = "blablabla";
            REQUIRE_THROWS_WITH_AS(ctx->parseModule(mod, format), "Can't parse module: LY_EVALID", std::runtime_error);
        }
    }

    DOCTEST_SUBCASE("Loading modules by name")
    {
        DOCTEST_SUBCASE("module exists")
        {
            ctx->setSearchDir(TESTS_DIR / "yang");
            auto mod = ctx->loadModule("mod1", std::nullopt, {
                "feature1",
                "feature2"
            });

            REQUIRE(mod.name() == "mod1");
            REQUIRE(mod.featureEnabled("feature1"));
            REQUIRE(mod.featureEnabled("feature2"));
            REQUIRE(!mod.featureEnabled("feature3"));
            REQUIRE_THROWS(mod.featureEnabled("invalid"));
        }

        DOCTEST_SUBCASE("module does not exist")
        {
            REQUIRE_THROWS(ctx->loadModule("invalid"));
        }
    }

    DOCTEST_SUBCASE("Get module extensions")
    {
        ctx->setSearchDir(TESTS_DIR / "yang");
        auto modYangPatch = ctx->loadModule("ietf-yang-patch", std::nullopt);
        auto modRestconf = ctx->getModule("ietf-restconf", "2017-01-26");
        REQUIRE(modRestconf);
        REQUIRE(modRestconf->name() == "ietf-restconf");

        REQUIRE(!modRestconf->implemented());
        REQUIRE_THROWS_WITH_AS(modRestconf->extensionInstances(), "Module \"ietf-restconf\" not implemented", libyang::Error);
        REQUIRE_THROWS_WITH_AS(modRestconf->extensionInstance("yang-errors"), "Module \"ietf-restconf\" not implemented", libyang::Error);

        modRestconf->setImplemented();
        REQUIRE(modRestconf->implemented());
        REQUIRE(modRestconf->extensionInstances().size() == 2);

        REQUIRE(modRestconf->extensionInstances()[0].argument() == "yang-errors");
        REQUIRE(modRestconf->extensionInstances()[0].definition().name() == "yang-data");
        REQUIRE(modRestconf->extensionInstance("yang-errors").argument() == "yang-errors");
        REQUIRE(modRestconf->extensionInstance("yang-errors").definition().name() == "yang-data");

        REQUIRE(modRestconf->extensionInstances()[1].argument() == "yang-api");
        REQUIRE(modRestconf->extensionInstances()[1].definition().name() == "yang-data");
        REQUIRE(modRestconf->extensionInstance("yang-api").argument() == "yang-api");
        REQUIRE(modRestconf->extensionInstance("yang-api").definition().name() == "yang-data");

        REQUIRE_THROWS_WITH_AS(modRestconf->extensionInstance("yay"), "Extension \"yay\" not defined in module \"ietf-restconf\"", libyang::Error);
    }

    DOCTEST_SUBCASE("context lifetime")
    {
        ctx->parseModule(valid_yang_model, libyang::SchemaFormat::YANG);

        DOCTEST_SUBCASE("Data nodes")
        {
            auto node = ctx->newPath("/test:someLeaf", "123");
            ctx.reset();
            // Node is still reachable.
            REQUIRE(node.path() == "/test:someLeaf");
        }

        DOCTEST_SUBCASE("Modules")
        {
            auto mod = ctx->getModule("test");
            ctx.reset();
            // Module is still reachable.
            REQUIRE(mod->name() == "test");
        }
    }

    DOCTEST_SUBCASE("Context::newPath2")
    {
        ctx->parseModule(example_schema2, libyang::SchemaFormat::YANG);
        auto nodes = ctx->newPath2("/example-schema2:contWithTwoNodes/one", "1");
        REQUIRE(nodes.createdNode->path() == "/example-schema2:contWithTwoNodes/one");
        REQUIRE(nodes.createdParent->path() == "/example-schema2:contWithTwoNodes");

        nodes = ctx->newPath2("/example-schema2:contWithTwoNodes");
        REQUIRE(nodes.createdNode->path() == "/example-schema2:contWithTwoNodes");
        REQUIRE(nodes.createdParent->path() == "/example-schema2:contWithTwoNodes");
    }

    DOCTEST_SUBCASE("Module::identities")
    {
        auto module = ctx->parseModule(example_schema, libyang::SchemaFormat::YANG);
        auto identities = module.identities();
        REQUIRE(identities.size() == 4);
        REQUIRE(identities.at(0).name() == "food");
        REQUIRE(identities.at(1).name() == "fruit");
        REQUIRE(identities.at(2).name() == "pizza");
        REQUIRE(identities.at(3).name() == "hawaii");

        auto module4 = ctx->parseModule(example_schema4, libyang::SchemaFormat::YANG);
        auto identities4 = module4.identities();
        REQUIRE(identities4.size() == 3);
        REQUIRE(identities4.at(0).name() == "pizza");
        REQUIRE(identities4.at(1).name() == "carpaccio");
        REQUIRE(identities4.at(2).name() == "another-carpaccio");

        REQUIRE(identities.at(0) == identities.at(0));
        REQUIRE(identities.at(0) != identities.at(1));
        REQUIRE(identities.at(2) != identities4.at(0)); // it's a different pizza

        std::set<libyang::Identity, libyang::SomeOrder> allIdentities;
        for (const auto& mod : {module, module4}) {
            for (const auto& identity : mod.identities()) {
                allIdentities.insert(identity);
            }
        }

        std::vector<std::string> expectedNames {
            {"example-schema:food"},
            {"example-schema:fruit"},
            {"example-schema:hawaii"},
            {"example-schema:pizza"},
            {"example-schema4:another-carpaccio"},
            {"example-schema4:carpaccio"},
            {"example-schema4:pizza"},
        };
        std::vector<std::string> actual;
        std::transform(allIdentities.begin(), allIdentities.end(), std::back_inserter(actual), libyang::qualifiedName);
        REQUIRE(actual == expectedNames);
    }

    DOCTEST_SUBCASE("Module::features")
    {
        ctx->setSearchDir(TESTS_DIR / "yang");
        auto mod = ctx->loadModule("mod1", std::nullopt, {
            "feature1",
            "feature2"
        });

        std::vector<std::string> expectedFeatures{
            "feature1",
            "feature2",
            "feature3",
        };
        std::vector<std::string> actualFeatures;
        for (const auto& feature : mod.features()) {
            actualFeatures.emplace_back(feature.name());
        }

        REQUIRE(actualFeatures == expectedFeatures);
    }

    DOCTEST_SUBCASE("Module::setImplemented")
    {
        ctx->setSearchDir(TESTS_DIR / "yang");
        auto mod = ctx->loadModule("mod1", std::nullopt, {});
        REQUIRE(!mod.featureEnabled("feature1"));
        REQUIRE(!mod.featureEnabled("feature2"));
        mod.setImplemented({{"feature1"}});
        REQUIRE(mod.featureEnabled("feature1"));
        REQUIRE(!mod.featureEnabled("feature2"));
        mod.setImplemented();
        REQUIRE(mod.featureEnabled("feature1"));
        REQUIRE(!mod.featureEnabled("feature2"));
        mod.setImplemented(libyang::AllFeatures{});
        REQUIRE(mod.featureEnabled("feature1"));
        REQUIRE(mod.featureEnabled("feature2"));
        REQUIRE(mod.featureEnabled("feature3"));

        REQUIRE_THROWS_AS(mod.setImplemented({{"nonexisting"}}), libyang::ErrorWithCode);
    }

    DOCTEST_SUBCASE("Context::modules")
    {
        ctx->setSearchDir(TESTS_DIR / "yang");
        ctx->loadModule("mod1", std::nullopt, {});
        ctx->parseModule(valid_yang_model, libyang::SchemaFormat::YANG);
        auto modules = ctx->modules();
        REQUIRE(modules.size() == 8);
        REQUIRE(modules.at(0).name() == "ietf-yang-metadata");
        REQUIRE(modules.at(1).name() == "yang");
        REQUIRE(modules.at(2).name() == "ietf-inet-types");
        REQUIRE(modules.at(3).name() == "ietf-yang-types");
        REQUIRE(modules.at(4).name() == "ietf-yang-schema-mount");
        REQUIRE(modules.at(5).name() == "ietf-yang-structure-ext");
        REQUIRE(modules.at(6).name() == "mod1");
        REQUIRE(*modules.at(6).revision() == "2021-11-15");
        REQUIRE(modules.at(7).name() == "test");
        REQUIRE(modules.at(7).revision() == std::nullopt);
    }

    DOCTEST_SUBCASE("Context::registerModuleCallback")
    {
        auto numCalled = 0;
        ctx->registerModuleCallback([&numCalled](std::string_view modName, auto, auto, auto) -> std::optional<libyang::ModuleInfo> {
            numCalled++;
            if (modName == "example-schema") {
                return libyang::ModuleInfo{
                    .data = example_schema,
                    .format = libyang::SchemaFormat::YANG
                };
            }

            return std::nullopt;
        });

        REQUIRE(ctx->loadModule("example-schema").name() == "example-schema");
        REQUIRE_THROWS_AS(ctx->loadModule("doesnt-exist"), libyang::Error);
        REQUIRE(numCalled == 2);
    }

    DOCTEST_SUBCASE("Implemented modules")
    {
        ctx->registerModuleCallback([](std::string_view modName, auto, auto, auto) -> std::optional<libyang::ModuleInfo> {
            if (modName == "withImport") {
                return libyang::ModuleInfo{
                    .data = model_with_import,
                    .format = libyang::SchemaFormat::YANG
                };
            }

            if (modName == "importedModule") {
                return libyang::ModuleInfo{
                    .data = imported_module,
                    .format = libyang::SchemaFormat::YANG
                };
            }
            return std::nullopt;
        });

        REQUIRE(ctx->loadModule("withImport").implemented());
        REQUIRE(!ctx->getModule("importedModule")->implemented());

        REQUIRE(ctx->getModuleImplemented("withImport").has_value());
        REQUIRE(!ctx->getModuleImplemented("importedModule").has_value());
    }

    DOCTEST_SUBCASE("Context::parseData")
    {
        ctx->parseModule(example_schema2, libyang::SchemaFormat::YANG);
        auto parsed = ctx->parseData("{}"s, libyang::DataFormat::JSON);
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->schema().path() == "/ietf-yang-schema-mount:schema-mounts");
    }

    DOCTEST_SUBCASE("Context::parseData")
    {
        ctx->parseModule(example_schema, libyang::SchemaFormat::YANG);
        auto data = ctx->parseData(TESTS_DIR / "test_data.json", libyang::DataFormat::JSON);
        REQUIRE(data);
        REQUIRE(data->findPath("/example-schema:leafInt8")->asTerm().valueStr() == "-43");
    }

    DOCTEST_SUBCASE("Log level")
    {
        REQUIRE(libyang::setLogLevel(libyang::LogLevel::Error) == libyang::LogLevel::Debug);
        REQUIRE(libyang::setLogLevel(libyang::LogLevel::Warning) == libyang::LogLevel::Error);
        REQUIRE(libyang::setLogLevel(libyang::LogLevel::Verbose) == libyang::LogLevel::Warning);
    }

    DOCTEST_SUBCASE("Error info")
    {
        std::vector<libyang::ErrorInfo> expected;
        DOCTEST_SUBCASE("No errors")
        {
        }

        DOCTEST_SUBCASE("Trying to parse invalid module")
        {
            REQUIRE_THROWS(ctx->parseModule("invalid module"s, libyang::SchemaFormat::YANG));
            expected = {
                libyang::ErrorInfo {
                    .appTag = std::nullopt,
                    .level = libyang::LogLevel::Error,
                    .message = "Invalid character sequence \"invalid\", expected a keyword.",
                    .code = libyang::ErrorCode::ValidationFailure,
                    .path = "Line number 1.",
                    .validationCode = libyang::ValidationErrorCode::Syntax,
                }
            };
        }

        DOCTEST_SUBCASE("Data restriction failure - multiple errors")
        {
            ctx->parseModule(example_schema, libyang::SchemaFormat::YANG);

            DOCTEST_SUBCASE("Store only last error")
            {
                libyang::setLogOptions(libyang::LogOptions::Log | libyang::LogOptions::StoreLast);
                expected = {
                    libyang::ErrorInfo {
                        .appTag = std::nullopt,
                        .level = libyang::LogLevel::Error,
                        .message = "Value \"9001\" is out of type int8 min/max bounds.",
                        .code = libyang::ErrorCode::ValidationFailure,
                        .path = "Schema location \"/example-schema:leafInt8\".",
                        .validationCode = libyang::ValidationErrorCode::Data,
                    }
                };
            }

            DOCTEST_SUBCASE("Store multiple errors")
            {
                libyang::setLogOptions(libyang::LogOptions::Log | libyang::LogOptions::Store);
                expected = {
                    libyang::ErrorInfo {
                        .appTag = std::nullopt,
                        .level = libyang::LogLevel::Error,
                        .message = "Invalid type int8 empty value.",
                        .code = libyang::ErrorCode::ValidationFailure,
                        .path = "Schema location \"/example-schema:leafInt8\".",
                        .validationCode = libyang::ValidationErrorCode::Data,
                    },
                    libyang::ErrorInfo {
                        .appTag = std::nullopt,
                        .level = libyang::LogLevel::Error,
                        .message = "Value \"9001\" is out of type int8 min/max bounds.",
                        .code = libyang::ErrorCode::ValidationFailure,
                        .path = "Schema location \"/example-schema:leafInt8\".",
                        .validationCode = libyang::ValidationErrorCode::Data,
                    }
                };
            }

            REQUIRE_THROWS(ctx->newPath("/example-schema:leafInt8"));
            REQUIRE_THROWS(ctx->newPath("/example-schema:leafInt8", "9001"));

            DOCTEST_SUBCASE("clear errors")
            {
                ctx->cleanAllErrors();
                expected = {};
            }
        }

        REQUIRE(ctx->getErrors() == expected);
    }
}

TEST_CASE("decimal64")
{
    using namespace libyang;
    REQUIRE(std::string(0_decimal64) == "0.0");
    REQUIRE(std::string(123_decimal64) == "123.0");
    REQUIRE(std::string(123.0_decimal64) == "123.0");
    REQUIRE(std::string(123.00_decimal64) == "123.00");
    REQUIRE(std::string(123.000_decimal64) == "123.000");
    REQUIRE(std::string(123.987_decimal64) == "123.987");
    REQUIRE(std::string(1.000000000000000000_decimal64) == "1.000000000000000000");
    REQUIRE(std::string(-1.000000000000000000_decimal64) == "-1.000000000000000000");
    REQUIRE(std::string(-922337203685477580.7_decimal64) == "-922337203685477580.7");
    REQUIRE(std::string(-922337203685477580.8_decimal64) == "-922337203685477580.8");
    REQUIRE(std::string(922337203685477580.7_decimal64) == "922337203685477580.7");
    REQUIRE(std::string(-92233720368547758.08_decimal64) == "-92233720368547758.08");
    REQUIRE(std::string(92233720368547758.07_decimal64) == "92233720368547758.07");
    REQUIRE(std::string(-92.23372036854775808_decimal64) == "-92.23372036854775808");
    REQUIRE(std::string(92.23372036854775807_decimal64) == "92.23372036854775807");
    REQUIRE(std::string(-9.223372036854775808_decimal64) == "-9.223372036854775808");
    REQUIRE(std::string(9.223372036854775807_decimal64) == "9.223372036854775807");
}

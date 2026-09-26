/*
    ConfParser.cxx - contains configuration parser test cases (exercised via ManagedLogCtrl)
    Copyright 2026 Jedidiah Thompson

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

// ConfParser is a CRTP template whose member functions are defined in source/ConfParser.cxx and only
// explicitly instantiated for ManagedLogCtrl/LogCtrlKey (see source/include/ConfTemplates.h). Since
// there's no separately-instantiated test type available, these tests exercise the generic parser
// logic (Parse/Get/Set/Serialize/Reset, error handling) entirely through that concrete instantiation.

#include "doctest.h"
#include "include/Log.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

/********************
 *
 * ConfParser test cases
 *
 *********************/

TEST_CASE ("ConfParser parses valid integer properties and Get reflects them")
{
    ManagedLogCtrl ctrl ("test.conf", "max_file = 10;\nmax_age = 5;\n");
    REQUIRE (ctrl.Parse());

    ConfValue val;
    auto res = ctrl.Get (LogCtrlKey::MaxFiles, val);
    REQUIRE (res);
    CHECK (res.Value());
    CHECK (std::get<int> (val) == 10);

    res = ctrl.Get (LogCtrlKey::MaxAge, val);
    REQUIRE (res);
    CHECK (res.Value());
    CHECK (std::get<int> (val) == 5);
}

TEST_CASE ("ConfParser Get on a never-set key reports no value without failing")
{
    ManagedLogCtrl ctrl ("test.conf", "max_file = 10;\n");
    REQUIRE (ctrl.Parse());

    ConfValue val;
    auto res = ctrl.Get (LogCtrlKey::MaxAge, val);
    REQUIRE (res);
    CHECK_FALSE (res.Value());
}

TEST_CASE ("ConfParser Set rejects a value whose type does not match the key's registered type")
{
    ManagedLogCtrl ctrl;
    ConfValue listVal = ConfList{"a", "b"};
    auto res = ctrl.Set (LogCtrlKey::MaxFiles, listVal);
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::ParseError);

    ConfValue strVal = std::string ("not an int");
    res = ctrl.Set (LogCtrlKey::MaxFiles, strVal);
    CHECK_FALSE (res);
}

TEST_CASE ("ConfParser Set with overwrite disabled rejects re-writing an existing key")
{
    ManagedLogCtrl ctrl;
    ConfValue val = 10;
    REQUIRE (ctrl.Set (LogCtrlKey::MaxFiles, val, true));

    ConfValue second = 20;
    auto res = ctrl.Set (LogCtrlKey::MaxFiles, second, false);
    CHECK_FALSE (res);

    // Value must be unchanged after the rejected write
    ConfValue readBack;
    ctrl.Get (LogCtrlKey::MaxFiles, readBack);
    CHECK (std::get<int> (readBack) == 10);
}

TEST_CASE ("ConfParser Set with overwrite enabled (the default) replaces an existing value")
{
    ManagedLogCtrl ctrl;
    ConfValue val = 10;
    REQUIRE (ctrl.Set (LogCtrlKey::MaxFiles, val));
    ConfValue second = 20;
    REQUIRE (ctrl.Set (LogCtrlKey::MaxFiles, second));

    ConfValue readBack;
    ctrl.Get (LogCtrlKey::MaxFiles, readBack);
    CHECK (std::get<int> (readBack) == 20);
}

TEST_CASE ("ConfParser Serialize writes back every previously-set key in \"name = value;\" form")
{
    ManagedLogCtrl ctrl ("test.conf", "max_file = 10;\nmax_age = 5;\n");
    REQUIRE (ctrl.Parse());

    std::string out;
    REQUIRE (ctrl.Serialize (out));
    CHECK (out.find ("max_file = 10;\n") != std::string::npos);
    CHECK (out.find ("max_age = 5;\n") != std::string::npos);
}

TEST_CASE ("ConfParser Serialize omits keys that were never set")
{
    ManagedLogCtrl ctrl ("test.conf", "max_file = 10;\n");
    REQUIRE (ctrl.Parse());

    std::string out;
    REQUIRE (ctrl.Serialize (out));
    CHECK (out.find ("max_file") != std::string::npos);
    CHECK (out.find ("max_age") == std::string::npos);
}

TEST_CASE ("ConfParser read operations support const objects")
{
    ManagedLogCtrl mutableCtrl ("const.conf", "max_file = 10;\n");
    REQUIRE (mutableCtrl.Parse());
    const ManagedLogCtrl& ctrl = mutableCtrl;

    ConfValue val;
    auto res = ctrl.Get (LogCtrlKey::MaxFiles, val);
    REQUIRE (res);
    CHECK (res.Value());
    CHECK (std::get<int> (val) == 10);

    std::string out;
    REQUIRE (ctrl.Serialize (out));
    CHECK (out == "max_file = 10;\n");
}

TEST_CASE ("ConfParser::Parse fails on syntactically invalid input and surfaces the lexer error")
{
    ManagedLogCtrl ctrl ("bad.conf", "max_file = ;\n");
    auto res = ctrl.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("ConfParser::Parse fails when a property is missing its terminating semicolon")
{
    ManagedLogCtrl ctrl ("bad.conf", "max_file = 10\n");
    auto res = ctrl.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("ConfParser handles an empty configuration body gracefully")
{
    ManagedLogCtrl ctrl ("empty.conf", "");
    REQUIRE (ctrl.Parse());
    std::string out;
    REQUIRE (ctrl.Serialize (out));
    CHECK (out.empty());
}

TEST_CASE ("ConfParser tolerates comments and blank lines around properties")
{
    ManagedLogCtrl ctrl ("commented.conf", "# leading comment\n\nmax_file = 7; # trailing comment\n\nmax_age = 2;\n");
    REQUIRE (ctrl.Parse());

    ConfValue val;
    ctrl.Get (LogCtrlKey::MaxFiles, val);
    CHECK (std::get<int> (val) == 7);
    ctrl.Get (LogCtrlKey::MaxAge, val);
    CHECK (std::get<int> (val) == 2);
}

TEST_CASE ("ConfParser re-parsing the same key multiple times keeps only the last value")
{
    ManagedLogCtrl ctrl ("repeat.conf", "max_file = 1;\nmax_file = 2;\nmax_file = 3;\n");
    REQUIRE (ctrl.Parse());

    ConfValue val;
    ctrl.Get (LogCtrlKey::MaxFiles, val);
    CHECK (std::get<int> (val) == 3);

    // Serialize must not emit the key more than once even though it was set three times
    std::string out;
    ctrl.Serialize (out);
    size_t firstPos = out.find ("max_file");
    size_t secondPos = out.find ("max_file", firstPos + 1);
    CHECK (secondPos == std::string::npos);
}

TEST_CASE ("ConfParser stress test with a large repeated-property configuration file")
{
    std::string data;
    constexpr int lines = 5000;
    for (int i = 0; i < lines; i++)
        data += "max_file = " + std::to_string (i) + ";\n";

    ManagedLogCtrl ctrl ("stress.conf", data);
    REQUIRE (ctrl.Parse());

    ConfValue val;
    ctrl.Get (LogCtrlKey::MaxFiles, val);
    CHECK (std::get<int> (val) == lines - 1);
}

TEST_CASE ("ConfParser Get/Set are safe to call concurrently from multiple threads")
{
    ManagedLogCtrl ctrl;
    ConfValue init = 0;
    REQUIRE (ctrl.Set (LogCtrlKey::MaxFiles, init));

    constexpr int threadCount = 8;
    constexpr int itersPerThread = 500;
    std::vector<std::thread> threads;
    std::atomic<int> completed{0};

    for (int t = 0; t < threadCount; t++)
    {
        threads.emplace_back ([&ctrl, &completed]() {
            for (int i = 0; i < itersPerThread; i++)
            {
                ConfValue val = i;
                ctrl.Set (LogCtrlKey::MaxFiles, val);
                ConfValue readBack;
                ctrl.Get (LogCtrlKey::MaxFiles, readBack);
                CHECK (std::holds_alternative<int> (readBack));
            }
            completed.fetch_add (1);
        });
    }
    for (auto& thread : threads)
        thread.join();

    CHECK (completed.load() == threadCount);
}

/*
    ImageConf.cxx - contains ImageConf frontend
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

#include "include/Frontend.h"
#include "include/sys/TextReader.h"

Result<std::string> ImageConf::readConfFile()
{
    std::filesystem::path fileName = opts.confFile;
    assert (!fileName.empty());

    // Read in the file
    std::string data;
    try
    {
        auto reader = TextReader (fileName, opts.confEnc);
        auto readRes = reader.Read();
        if (!readRes)
            return readRes.Error();

        data = std::move (readRes.Value());
    }
    catch (ErrorException& e)
    {
        return e.Error();
    }
    return data;
}

ResNone ImageConf::Parse()
{
    auto resRead = readConfFile();
    if (!resRead)
        return parseFailed();
    return Success();
}

/*
    ImageParser.h - contains ImageParser header
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

#ifndef IMAGEPARSER_H
#define IMAGEPARSER_H

#include "include/SimpleLexer.h"
#include "include/StringHash.h"
#include "include/image/ImgBase.h"

struct ImgParseProp
{
    std::string propName;
    std::vector<std::vector<ImageVal>> vals;
};

struct ImgParseBlock
{
    std::string type;
    std::string name;
    std::unordered_map<std::string, ImgParseProp, StringHash, std::equal_to<>> props;
};

class ImageParser
{
  public:
    ImageParser() = default;
    ImageParser (std::string fileName, std::string data) : lexer{std::move (fileName), std::move (data)}
    {}

    Result<ImgParseBlock> ParseBlock();

  private:
    SimpleLexer lexer;
};

#endif

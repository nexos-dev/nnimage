/*
    KrunBackend.h - contains Krun backend class
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

#ifndef KRUNBACKEND_H
#define KRUNBACKEND_H

#include "include/Backend.h"
#include "include/sys/BlockDevGen.h"

class KrunBackend : public Backend
{
  public:
    KrunBackend();
    ~KrunBackend() override;
    std::unique_ptr<Task> CreatePartTable (Image& img, const std::string& fileName) override;
    bool AddImage (Image& img, const std::string& fileName, bool readonly) override;

  private:
    bool runKrunCommand (const std::string& cmd, const std::vector<std::string>& args);
    BlockDevFactory blockDevGen{"vd"};
    int32_t krunCtx = -1;
};

#endif

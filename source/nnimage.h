/*
    nnimage.h - contains header of nnimage
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

#ifndef NNIMAGE_H
#define NNIMAGE_H

// TODO: it would be better if source files included the headers they need, but I also like the
// simplicity of this

#include "include/Log.h"
#include "include/Error.h"
#include "include/ConfParser.h"
#include "include/Dispatch.h"
#include "include/Action.h"
#include "include/Image.h"
#include "include/Backend.h"
#include "include/Frontend.h"
#include "include/Task.h"
#include "include/ImgComponent.h"

// Test driver function
bool TestDriver (int argc, char** argv);

#endif
